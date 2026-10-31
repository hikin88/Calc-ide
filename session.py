"""Логика консоли, не зависящая от Kivy: модель вывода и REPL-режим (как консоль самого компилятора)."""
from calc_bridge import FLAG_EXPLAIN, FLAG_NO_OPT, FLAG_PRINT

# Виды строк в консоли (0 и 1 совпадают с KIND_OUT / KIND_ERR из ядра)
K_OUT, K_ERR, K_ECHO, K_INFO, K_CLEAR = 0, 1, 2, 3, 9


class ConsoleModel:
    """Список строк консоли. Последняя строка может быть «недописанной» — так выглядит приглашение
    программы перед input («Введите число: »)."""

    MAX_LINES = 2000
    TRIM_STEP = 50            # лишнее отбрасывается кратно этому числу, чтобы блоки по 25 строк в интерфейсе не сдвигались

    def __init__(self):
        self.lines = [["", K_OUT]]
        self.version = 0                         # растёт при каждом изменении (чтобы интерфейс знал, что перерисовать)

    def apply(self, text, kind, line=False):
        """Добавить текст. line=True — сначала закончить текущую строку, а после текста перейти на новую."""
        if kind == K_CLEAR:
            self.lines = [["", K_OUT]]
            self.version += 1
            return
        if line:
            self.ensure_newline()
            text += "\n"
        parts = text.replace("\r", "").split("\n")
        for i, part in enumerate(parts):
            if i > 0:
                self.lines.append(["", K_OUT])
            if part:
                last = self.lines[-1]
                if not last[0]:
                    last[1] = kind               # вид строки определяет её первый кусок
                last[0] += part
        if len(self.lines) > self.MAX_LINES:
            excess = len(self.lines) - self.MAX_LINES
            del self.lines[: -(-excess // self.TRIM_STEP) * self.TRIM_STEP]
        self.version += 1

    def ensure_newline(self):
        if self.lines[-1][0]:
            self.lines.append(["", K_OUT])

    def plain_text(self):
        return "\n".join(t for t, _ in self.lines)


SAMPLE_PROGRAM = """# Пример программы. Нажмите «Запуск».
func fact (n) {
    if (n <= 1) { return 1 }
    return n * fact(n - 1)
}

var n = input
show ("Введите число: ", n)

var i = 1
while i <= n {
    show (i, "! = ", fact(i))
    i = i + 1
    if (i > 12) { break }
}
"""


def is_blank(line):
    s = line.strip()
    return not s or s.startswith("#")


def starts_with_else(line):
    t = line.strip()
    if not t.startswith("else"):
        return False
    return len(t) == 4 or not (t[4].isalnum() or t[4] == "_")


HELP_LINES = [
    "Команды консоли:",
    "  <код>           выполнить команду языка (блоки { } можно вводить по строкам)",
    "  explain <код>   пошаговый разбор одной команды;  explain / quiet — для всех команд",
    "  noopt / opt     выключить / включить оптимизатор",
    "  vars            показать переменные и функции",
    "  run             запустить программу из редактора",
    "  clear           очистить консоль",
    "Если программа ждёт ввод (input), просто напишите ответ в строке ввода.",
]


class Repl:
    """Обработчик строк консоли. Повторяет REPL компилятора: многострочные блоки, else на следующей строке,
    команды explain / quiet / opt / noopt / vars. Все результаты выводятся через emit(text, kind, line=False).
    emit можно вызывать из любого потока: порядок вывода программы и этих сообщений сохраняется."""

    def __init__(self, engine, emit, run_editor=None, quit_app=None):
        self.engine = engine
        self.emit = emit
        self.run_editor = run_editor
        self.quit_app = quit_app
        self.block = ""                          # накопленный незавершённый ввод
        self.explain_mode = False
        self.optimize = True
        self.one_shot = False
        self.history = []
        self.hist_pos = 0

    @property
    def prompt(self):
        if self.engine.busy:
            return "ввод> " if self.engine.waiting_input else ""
        return ">>> " if not self.block else "... "

    def flags(self, print_result):
        f = 0
        if self.explain_mode or self.one_shot:
            f |= FLAG_EXPLAIN
        if not self.optimize:
            f |= FLAG_NO_OPT
        if print_result:
            f |= FLAG_PRINT
        return f

    # ---- история ----
    def history_prev(self):
        if self.hist_pos > 0:
            self.hist_pos -= 1
            return self.history[self.hist_pos]
        return None

    def history_next(self):
        if self.hist_pos < len(self.history):
            self.hist_pos += 1
            return self.history[self.hist_pos] if self.hist_pos < len(self.history) else ""
        return None

    # ---- ввод строки ----
    def submit(self, line):
        e = self.engine
        if e.busy:                               # идёт программа: строка — это её ввод (input)
            e.send_input(line)
            self.emit(line + "\n", K_ECHO)
            return
        self.emit(self.prompt + line, K_ECHO, True)
        if line.strip() and (not self.history or self.history[-1] != line):
            self.history.append(line)
        self.hist_pos = len(self.history)
        self._line(line)

    def _line(self, line):
        e = self.engine
        if self.block:
            if e.continuation(self.block) == 2:
                # Ждали возможный 'else'. Любая другая строка завершает if без else.
                if not starts_with_else(line):
                    self._execute()
                    if is_blank(line):
                        return
            elif is_blank(line):
                return                           # пустая строка внутри незакрытого блока
        if not self.block:
            if is_blank(line):
                return
            line = line.strip()
            # слова-команды работают, пока не заняты именем переменной
            is_var = (not e.busy) and e.has_var(line)
            if not is_var and self._command(line):
                return
            if line.startswith("explain "):
                self.one_shot = True
                line = line[len("explain "):]
        self.block += line + "\n"
        if e.continuation(self.block) == 0:
            self._execute()

    def _execute(self):
        src = self.block.strip()
        self.block = ""
        if src:
            self.engine.submit_run(src, self.flags(True), tag="repl")
        self.one_shot = False

    def _command(self, cmd):
        info = lambda text: self.emit(text, K_INFO, True)
        if cmd == "exit":
            if self.quit_app:
                self.quit_app()
            return True
        if cmd in ("clear", "cls"):
            self.emit("", K_CLEAR)
            return True
        if cmd == "help":
            for text in HELP_LINES:
                info(text)
            return True
        if cmd == "run":
            if self.run_editor:
                self.run_editor()
            return True
        if cmd == "explain":
            self.explain_mode = True
            info("Пошаговый разбор включён (quiet — выключить)")
            return True
        if cmd == "quiet":
            self.explain_mode = False
            info("Пошаговый разбор выключен")
            return True
        if cmd in ("opt", "noopt"):
            self.optimize = cmd == "opt"
            info("Оптимизатор: " + ("включён" if self.optimize else "выключен"))
            return True
        if cmd == "vars":
            self.engine.submit_vars(tag="repl")
            return True
        return False
