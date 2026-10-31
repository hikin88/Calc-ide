"""Мост между Python и ядром компилятора на C++ (libcalccore.so).

Ядро синхронное: calc_run() возвращается, когда программа закончилась. Поэтому Engine запускает
программы в отдельном рабочем потоке — интерфейс не «зависает» ни на долгих циклах, ни на input.

    engine = Engine(on_output=lambda text, kind: print(text, end=""))
    engine.submit_run("show (2 + 3)")
    engine.wait_idle()

Вывод приходит в on_output(text, kind) из РАБОЧЕГО потока (kind: KIND_OUT или KIND_ERR).
Если программе нужен input, она ждёт, пока кто-нибудь вызовет engine.send_input("строка").
engine.cancel() прерывает выполнение (в том числе ожидание ввода).
"""
import codecs
import ctypes
import os
import queue
import threading
import time
from collections import namedtuple

KIND_OUT, KIND_ERR = 0, 1                       # виды текста от ядра
FLAG_EXPLAIN, FLAG_NO_OPT, FLAG_PRINT = 1, 2, 4  # флаги calc_run
RC_OK, RC_COMPILE, RC_RUNTIME, RC_CANCELLED, RC_INTERNAL = range(5)

LIB_NAME = "libcalccore.so"

WRITE_CB = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_void_p)
READ_CB = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p)

Result = namedtuple("Result", "tag kind code error_line ms")
_Job = namedtuple("_Job", "kind source flags tag exe_dir")


def _library_candidates():
    env = os.environ.get("CALC_LIB")
    if env:
        yield env
    yield LIB_NAME                       # Android: библиотека лежит в папке нативных библиотек приложения
    here = os.path.dirname(os.path.abspath(__file__))
    for name in (LIB_NAME, "calccore.dll", "libcalccore.dylib"):
        yield os.path.join(here, name)
    try:                                 # запасной путь на Android: спросить у системы папку нативных библиотек
        from jnius import autoclass      # type: ignore
        activity = autoclass("org.kivy.android.PythonActivity").mActivity
        yield os.path.join(str(activity.getApplicationInfo().nativeLibraryDir), LIB_NAME)
    except Exception:
        pass


def load_library(path=None):
    errors = []
    for cand in ([path] if path else _library_candidates()):
        try:
            return ctypes.CDLL(cand)
        except OSError as exc:
            errors.append("%s: %s" % (cand, exc))
    raise OSError("Не удалось загрузить %s:\n%s" % (LIB_NAME, "\n".join(errors)))


def _declare(lib):
    c_void_p, c_char_p, c_int = ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int
    lib.calc_version.restype = c_char_p
    lib.calc_version.argtypes = []
    lib.calc_create.restype = c_void_p
    lib.calc_create.argtypes = []
    lib.calc_destroy.restype = None
    lib.calc_destroy.argtypes = [c_void_p]
    lib.calc_set_callbacks.restype = None
    lib.calc_set_callbacks.argtypes = [c_void_p, WRITE_CB, READ_CB, c_void_p]
    lib.calc_reset.restype = None
    lib.calc_reset.argtypes = [c_void_p, c_char_p]
    lib.calc_run.restype = c_int
    lib.calc_run.argtypes = [c_void_p, c_char_p, c_int]
    lib.calc_cancel.restype = None
    lib.calc_cancel.argtypes = [c_void_p]
    lib.calc_error_line.restype = c_int
    lib.calc_error_line.argtypes = [c_void_p]
    lib.calc_continuation.restype = c_int
    lib.calc_continuation.argtypes = [c_char_p]
    lib.calc_has_var.restype = c_int
    lib.calc_has_var.argtypes = [c_void_p, c_char_p]
    lib.calc_vars.restype = None
    lib.calc_vars.argtypes = [c_void_p]


class Engine:
    """Один интерпретатор с общей памятью (переменные и функции живут между запусками)."""

    def __init__(self, on_output=None, on_done=None, lib=None):
        self._lib = lib or load_library()
        _declare(self._lib)
        self._on_output = on_output
        self._on_done = on_done
        self._decoders = {
            KIND_OUT: codecs.getincrementaldecoder("utf-8")("replace"),
            KIND_ERR: codecs.getincrementaldecoder("utf-8")("replace"),
        }
        # ссылки на callback-объекты нужно держать, иначе их соберёт сборщик мусора
        self._wcb = WRITE_CB(self._write)
        self._rcb = READ_CB(self._read)
        self._h = self._lib.calc_create()
        if not self._h:
            raise MemoryError("calc_create вернул NULL")
        self._lib.calc_set_callbacks(self._h, self._wcb, self._rcb, None)

        self._jobs = queue.Queue()
        self._inputs = queue.Queue()
        self._lock = threading.Lock()
        self._pending = 0               # задач в очереди + выполняемая
        self._waiting = False
        self._closed = False
        self._thread = threading.Thread(target=self._loop, name="calc-worker", daemon=True)
        old = None
        try:                            # глубокие выражения разбираются рекурсивно: даём потоку большой стек
            old = threading.stack_size(16 * 1024 * 1024)
        except (ValueError, RuntimeError):
            pass
        self._thread.start()
        if old is not None:
            try:
                threading.stack_size(old)
            except (ValueError, RuntimeError):
                pass

    # ---------------------------------------------------------------- состояние
    @property
    def version(self):
        return self._lib.calc_version().decode("ascii", "replace")

    @property
    def busy(self):
        """True, пока выполняется программа или в очереди есть задачи."""
        with self._lock:
            return self._pending > 0

    @property
    def waiting_input(self):
        """True, когда программа остановилась на input и ждёт строку."""
        return self._waiting

    # ---------------------------------------------------------------- задачи
    def _submit(self, job):
        with self._lock:
            self._pending += 1
        self._jobs.put(job)

    def submit_run(self, source, flags=0, tag=None):
        """Поставить программу в очередь. Память сохраняется между запусками (см. submit_reset)."""
        self._submit(_Job("run", source.replace("\0", ""), flags, tag, None))

    def submit_reset(self, exe_dir="", tag=None):
        """Очистить память. exe_dir — папка, в которой runF ищет файлы программ."""
        self._submit(_Job("reset", "", 0, tag, exe_dir))

    def submit_vars(self, tag=None):
        """Напечатать переменные и функции (через on_output)."""
        self._submit(_Job("vars", "", 0, tag, None))

    def send_input(self, line):
        """Передать программе строку для input."""
        self._inputs.put(line)

    def cancel(self):
        """Остановить выполняемую программу и выбросить всё, что ещё стоит в очереди."""
        self._lib.calc_cancel(self._h)
        self._inputs.put(None)          # разбудить ожидание ввода
        dropped = 0
        while True:
            try:
                job = self._jobs.get_nowait()
            except queue.Empty:
                break
            if job is None:             # метка закрытия — вернуть на место
                self._jobs.put(None)
                break
            dropped += 1
        if dropped:
            with self._lock:
                self._pending -= dropped

    def wait_idle(self, timeout=None):
        """Ждать, пока всё выполнится. Возвращает True, если дождались."""
        end = None if timeout is None else time.monotonic() + timeout
        while self.busy:
            if end is not None and time.monotonic() > end:
                return False
            time.sleep(0.005)
        return True

    # ---------------------------------------------------------------- запросы (только когда не busy)
    def has_var(self, name):
        return bool(self._lib.calc_has_var(self._h, name.encode("utf-8")))

    def continuation(self, text):
        """0 — команда завершена; 1 — не закрыта '{'; 2 — if без else (может прийти else)."""
        return int(self._lib.calc_continuation(text.replace("\0", "").encode("utf-8")))

    def close(self):
        if self._closed:
            return
        self._closed = True
        self.cancel()
        self._jobs.put(None)
        self._thread.join(timeout=3)
        if not self._thread.is_alive():
            self._lib.calc_destroy(self._h)
        self._h = None

    # ---------------------------------------------------------------- рабочий поток
    def _loop(self):
        lib, h = self._lib, self._h
        while True:
            job = self._jobs.get()
            if job is None:
                break
            code, err, ms = RC_OK, -1, 0.0
            try:
                if job.kind == "run":
                    self._clear_inputs()
                    t0 = time.perf_counter()
                    code = lib.calc_run(h, job.source.encode("utf-8"), job.flags)
                    ms = (time.perf_counter() - t0) * 1000.0
                    err = lib.calc_error_line(h)
                elif job.kind == "vars":
                    lib.calc_vars(h)
                elif job.kind == "reset":
                    lib.calc_reset(h, (job.exe_dir or "").encode("utf-8"))
            except Exception as exc:    # на всякий случай: рабочий поток не должен умирать
                code = RC_INTERNAL
                self._emit("Внутренняя ошибка: %s\n" % exc, KIND_ERR)
            finally:
                with self._lock:
                    self._pending -= 1
            if self._on_done:
                try:
                    self._on_done(Result(job.tag, job.kind, code, err, ms))
                except Exception:
                    pass

    def _clear_inputs(self):
        while True:
            try:
                self._inputs.get_nowait()
            except queue.Empty:
                return

    def _emit(self, text, kind):
        if self._on_output and text:
            try:
                self._on_output(text, kind)
            except Exception:
                pass

    # ---------------------------------------------------------------- вызовы из C++ (в рабочем потоке)
    def _write(self, data, length, kind, user):
        try:
            raw = ctypes.string_at(data, length)
            decoder = self._decoders.get(kind) or self._decoders[KIND_OUT]
            self._emit(decoder.decode(raw), kind)    # incremental: кусок может обрываться посреди символа UTF-8
        except Exception:
            pass

    def _read(self, buf, cap, user):
        try:
            self._waiting = True
            try:
                line = self._inputs.get()           # блокируемся, пока не придёт строка или None (отмена)
            finally:
                self._waiting = False
            if line is None:
                return -1
            data = line.encode("utf-8")[:cap]
            ctypes.memmove(buf, data, len(data))
            return len(data)
        except Exception:
            return -1
