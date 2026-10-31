"""Тесты ядра компилятора (C++) через ctypes и логики консоли.

Запуск из корня проекта:   python tests/test_core.py
Если библиотека ещё не собрана, тест сам соберёт её компилятором g++ (или $CXX).
Свою библиотеку можно указать через переменную окружения CALC_LIB.
"""
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

_TMP = None


def build_library():
    global _TMP
    if os.environ.get("CALC_LIB"):
        return
    _TMP = tempfile.mkdtemp(prefix="calccore-")
    out = os.path.join(_TMP, "libcalccore.so")
    cxx = os.environ.get("CXX") or shutil.which("g++") or shutil.which("c++") or shutil.which("clang++")
    if not cxx:
        raise SystemExit("Не найден компилятор C++ (g++/clang++), а библиотека не собрана")
    src = os.path.join(ROOT, "recipes", "calccore", "calc_core.cpp")
    cmd = [cxx, "-std=c++17", "-O2", "-fPIC", "-shared", "-fvisibility=hidden", "-o", out, src]
    print("сборка:", " ".join(cmd))
    subprocess.run(cmd, check=True)
    os.environ["CALC_LIB"] = out


build_library()

from calc_bridge import (Engine, FLAG_EXPLAIN, FLAG_NO_OPT, FLAG_PRINT, KIND_ERR, KIND_OUT,  # noqa: E402
                         RC_CANCELLED, RC_COMPILE, RC_OK, RC_RUNTIME)
from session import (K_CLEAR, K_ECHO, K_INFO, K_OUT, SAMPLE_PROGRAM, ConsoleModel, Repl,  # noqa: E402
                      is_blank, starts_with_else)


class Collector:
    """Собирает вывод и результаты из рабочего потока."""

    def __init__(self):
        self.chunks = []
        self.results = []
        self.lock = threading.Lock()

    def output(self, text, kind):
        with self.lock:
            self.chunks.append((text, kind))

    def done(self, result):
        with self.lock:
            self.results.append(result)

    @property
    def text(self):
        with self.lock:
            return "".join(t for t, _ in self.chunks)

    def text_of(self, kind):
        with self.lock:
            return "".join(t for t, k in self.chunks if k == kind)

    def clear(self):
        with self.lock:
            self.chunks.clear()
            self.results.clear()


class CoreTestCase(unittest.TestCase):
    def setUp(self):
        self.c = Collector()
        self.e = Engine(self.c.output, self.c.done)
        self.tmp = tempfile.mkdtemp(prefix="calc-test-")

    def tearDown(self):
        self.e.close()
        shutil.rmtree(self.tmp, ignore_errors=True)

    def run_code(self, src, flags=0, timeout=20):
        self.c.clear()
        self.e.submit_run(src, flags, tag="t")
        self.assertTrue(self.e.wait_idle(timeout), "программа не завершилась за %s с" % timeout)
        # on_done вызывается после уменьшения счётчика — дадим потоку закончить
        for _ in range(200):
            if self.c.results:
                break
            time.sleep(0.005)
        return self.c.results[-1].code

    def wait_for(self, cond, timeout=10):
        end = time.time() + timeout
        while time.time() < end:
            if cond():
                return True
            time.sleep(0.005)
        return False


class TestBasics(CoreTestCase):
    def test_version(self):
        self.assertTrue(self.e.version.startswith("calc-core"))

    def test_show(self):
        self.assertEqual(self.run_code('var x = 5\nshow ("x = ", x)'), RC_OK)
        self.assertEqual(self.c.text, "x = 5\n")

    def test_print_result_flag(self):
        self.run_code("2 + 3 * 4", FLAG_PRINT)
        self.assertEqual(self.c.text, "14\n")
        self.run_code("2 + 3 * 4", 0)
        self.assertEqual(self.c.text, "")

    def test_number_formatting(self):
        self.run_code('show (0.1 + 0.2)\nshow (1 / 3)\nshow (2 ** 70)\nshow (1 / 100000)\nshow (10 / 4)')
        self.assertEqual(self.c.text.split("\n")[:5],
                         ["0.30000000000000004", "0.3333333333333333", "1180591620717411303424", "1e-05", "2.5"])

    def test_new_language_features(self):
        code = ('var i = 0\nwhile i < 10 {\n  i = i + 1\n  if (i % 2 == 0) { continue }\n'
                '  if (i > 7) { break }\n  show (i)\n}\n'
                'show (1 and 0, 1 or 0, not 0)\n'
                'var total = 0\nfunc add (n) { total = total + n }\nadd(5)\nadd(7)\nshow (total)')
        self.assertEqual(self.run_code(code), RC_OK)
        self.assertEqual(self.c.text, "1\n3\n5\n7\n011\n12\n")

    def test_unicode_roundtrip(self):
        self.run_code('show ("Привет, мир! Ёжик ✔ 日本")')
        self.assertEqual(self.c.text, "Привет, мир! Ёжик ✔ 日本\n")
        self.run_code('var имя = "Анна"\nshow (имя, " ", length(имя))')
        self.assertEqual(self.c.text, "Анна 4\n")

    def test_memory_persists_and_reset(self):
        self.run_code("var x = 1")
        self.run_code("x = x + 1\nshow (x)")
        self.assertEqual(self.c.text, "2\n")
        self.e.submit_reset(self.tmp)
        self.e.wait_idle(5)
        self.assertEqual(self.run_code("show (x)"), RC_COMPILE)

    def test_has_var_and_vars(self):
        self.run_code('var a = 5\nlist b = 1, 2\nfunc f (n, m) { return n + m }')
        self.assertTrue(self.e.has_var("a"))
        self.assertFalse(self.e.has_var("zzz"))
        self.c.clear()
        self.e.submit_vars()
        self.e.wait_idle(5)
        text = self.c.text
        self.assertIn("a = 5", text)
        self.assertIn("func f (n, m)", text)

    def test_explain_and_no_opt(self):
        self.run_code("var x = 2 + 3", FLAG_EXPLAIN)
        self.assertGreater(len(self.c.text), 200)            # подробный разбор по этапам
        self.assertIn("x", self.c.text)
        self.run_code("show (2 + 3)", FLAG_EXPLAIN | FLAG_NO_OPT)
        self.assertGreater(len(self.c.text), 200)

    def test_runF_uses_exe_dir(self):
        with open(os.path.join(self.tmp, "lib.txt"), "w", encoding="utf-8") as f:
            f.write("func twice (n) { return n * 2 }\n")
        self.e.submit_reset(self.tmp)
        self.e.wait_idle(5)
        self.assertEqual(self.run_code('runF "lib.txt"\nshow (twice(21))'), RC_OK)
        self.assertEqual(self.c.text, "42\n")


class TestSample(CoreTestCase):
    def test_sample_program_runs(self):
        def feed():
            if self.wait_for(lambda: self.e.waiting_input, 10):
                self.e.send_input("5")
        th = threading.Thread(target=feed, daemon=True)
        th.start()
        self.assertEqual(self.run_code(SAMPLE_PROGRAM), RC_OK)
        th.join(5)
        self.assertEqual(self.c.text, "Введите число: 5\n1! = 1\n2! = 2\n3! = 6\n4! = 24\n5! = 120\n")


class TestErrors(CoreTestCase):
    def test_compile_error(self):
        rc = self.run_code("show (1)\nvar = 5")
        self.assertEqual(rc, RC_COMPILE)
        self.assertIn("Ошибка компиляции", self.c.text_of(KIND_ERR))
        self.assertEqual(self.c.results[-1].error_line, 2)
        self.assertEqual(self.c.text_of(KIND_OUT), "")        # ничего не выполнилось

    def test_error_line_in_runF_is_not_reported(self):
        with open(os.path.join(self.tmp, "bad.txt"), "w", encoding="utf-8") as f:
            f.write("var = 1\n")
        self.e.submit_reset(self.tmp)
        self.e.wait_idle(5)
        self.assertEqual(self.run_code('runF "bad.txt"'), RC_COMPILE)
        self.assertEqual(self.c.results[-1].error_line, -1)   # ошибка в другом файле: строки основного текста нет

    def test_runtime_error_and_order(self):
        rc = self.run_code("show (1)\nshow (1 / 0)\nshow (2)")
        self.assertEqual(rc, RC_RUNTIME)
        kinds = [k for _, k in self.c.chunks]
        self.assertEqual(self.c.chunks[0][0], "1\n")
        self.assertEqual(kinds[0], KIND_OUT)
        self.assertEqual(kinds[-1], KIND_ERR)                 # обычный вывод дошёл раньше ошибки
        self.assertIn("Ошибка выполнения", self.c.text_of(KIND_ERR))
        self.assertNotIn("2", self.c.text_of(KIND_OUT).replace("1\n", ""))

    def test_step_limit(self):
        e2 = Engine(self.c.output, self.c.done)               # лимит 50 млн инструкций — убедимся, что вечный цикл не вечен
        try:
            t0 = time.time()
            e2.submit_run("while 1 { }", 0)
            self.assertTrue(e2.wait_idle(60))
            self.assertLess(time.time() - t0, 60)
            self.assertIn("лимит", self.c.text_of(KIND_ERR))
        finally:
            e2.close()


class TestInputAndCancel(CoreTestCase):
    def feed_when_waiting(self, answers):
        def worker():
            for a in answers:
                if not self.wait_for(lambda: self.e.waiting_input, 10):
                    return
                self.e.send_input(a)
                self.wait_for(lambda: not self.e.waiting_input, 5)
        t = threading.Thread(target=worker, daemon=True)
        t.start()
        return t

    def test_input_two_values(self):
        t = self.feed_when_waiting(["40", "2"])
        rc = self.run_code('var a = input\nvar b = input\nshow ("Введите: ", a, " ", b)\nshow ("sum=", a + b)')
        t.join(5)
        self.assertEqual(rc, RC_OK)
        self.assertEqual(self.c.text, "Введите: 40 2\nsum=42\n")

    def test_prompt_is_visible_before_waiting(self):
        self.c.clear()
        self.e.submit_run('var n = input\nshow ("Введите число: ", n)', 0, "t")
        self.assertTrue(self.wait_for(lambda: self.e.waiting_input))
        self.assertEqual(self.c.text, "Введите число: ")      # приглашение уже показано, а программа ждёт
        self.e.send_input("7")
        self.assertTrue(self.e.wait_idle(10))
        self.assertEqual(self.c.text, "Введите число: 7\n")

    def test_input_unicode_string(self):
        t = self.feed_when_waiting(["Привет мир"])
        self.run_code('var s = input\nshow (s, "!")')
        t.join(5)
        self.assertEqual(self.c.text, "Привет мир!\n")

    def test_cancel_running_loop(self):
        self.c.clear()
        self.e.submit_run("var i = 0\nwhile 1 { i = i + 1 }", 0, "t")
        time.sleep(0.3)
        self.assertTrue(self.e.busy)
        t0 = time.time()
        self.e.cancel()
        self.assertTrue(self.e.wait_idle(5))
        self.assertLess(time.time() - t0, 2.0)
        self.wait_for(lambda: self.c.results)
        self.assertEqual(self.c.results[-1].code, RC_CANCELLED)
        self.assertEqual(self.c.text, "")                     # сообщение об ошибке при остановке не печатается

    def test_cancel_while_waiting_for_input(self):
        self.c.clear()
        self.e.submit_run('var a = input\nshow (a)', 0, "t")
        self.assertTrue(self.wait_for(lambda: self.e.waiting_input))
        self.e.cancel()
        self.assertTrue(self.e.wait_idle(5))
        self.wait_for(lambda: self.c.results)
        self.assertEqual(self.c.results[-1].code, RC_CANCELLED)

    def test_engine_works_after_cancel(self):
        self.e.submit_run("while 1 { }", 0, "t")
        time.sleep(0.2)
        self.e.cancel()
        self.assertTrue(self.e.wait_idle(5))
        self.assertEqual(self.run_code("show (1 + 1)"), RC_OK)
        self.assertEqual(self.c.text, "2\n")

    def test_cancel_drops_queue(self):
        self.e.submit_run("while 1 { }", 0, "a")
        self.e.submit_run('show ("не должно выполниться")', 0, "b")
        time.sleep(0.2)
        self.e.cancel()
        self.assertTrue(self.e.wait_idle(5))
        self.assertNotIn("не должно", self.c.text)


class TestVolume(CoreTestCase):
    def test_many_lines_in_order(self):
        n = 60000
        rc = self.run_code('var i = 0\nwhile i < %d {\n  i = i + 1\n  show ("строка ", i)\n}' % n, 0, timeout=60)
        self.assertEqual(rc, RC_OK)
        lines = self.c.text.split("\n")
        self.assertEqual(len(lines), n + 1)
        self.assertEqual(lines[0], "строка 1")
        self.assertEqual(lines[n - 1], "строка %d" % n)
        for i in (0, 1, 999, 4095, 4096, 30000, n - 1):       # порядок и целостность UTF-8 на границах кусков
            self.assertEqual(lines[i], "строка %d" % (i + 1))

    def test_output_arrives_while_running(self):
        self.c.clear()
        self.e.submit_run('show ("начало")\nvar i = 0\nwhile 1 { i = i + 1 }', 0, "t")
        self.assertTrue(self.wait_for(lambda: "начало" in self.c.text, 3), "вывод должен появиться, не дожидаясь конца")
        self.e.cancel()
        self.e.wait_idle(5)


class TestContinuation(CoreTestCase):
    def test_states(self):
        self.assertEqual(self.e.continuation("show (1)"), 0)
        self.assertEqual(self.e.continuation("if (1) {"), 1)
        self.assertEqual(self.e.continuation("while 1 {\n show (1)"), 1)
        self.assertEqual(self.e.continuation("if (1) { show (1) }"), 2)
        self.assertEqual(self.e.continuation("if (1) { show (1) } else { show (2) }"), 0)
        self.assertEqual(self.e.continuation("if (1) { show (1) } else if (2) { show (2) }"), 2)
        self.assertEqual(self.e.continuation("var = "), 0)    # синтаксическая ошибка: пусть выполнение её покажет


class TestHelpers(unittest.TestCase):
    def test_blank_and_else(self):
        self.assertTrue(is_blank("   "))
        self.assertTrue(is_blank("# комментарий"))
        self.assertFalse(is_blank("show (1)"))
        self.assertTrue(starts_with_else("  else { }"))
        self.assertFalse(starts_with_else("elsewhere = 1"))

    def test_console_model(self):
        m = ConsoleModel()
        m.apply("Введите: ", K_OUT)
        self.assertEqual(m.lines[-1][0], "Введите: ")                 # недописанная строка
        m.apply("42\n", K_ECHO)
        self.assertEqual(m.lines[-2][0], "Введите: 42")
        self.assertEqual(m.lines[-1][0], "")
        m.apply("ошибка", 1, True)
        self.assertEqual(m.lines[-2], ["ошибка", 1])
        m.apply("a\r\nb\n", K_OUT)
        self.assertEqual([l[0] for l in m.lines[-3:]], ["a", "b", ""])
        m.apply("", K_CLEAR)
        self.assertEqual(m.plain_text(), "")
        for i in range(3000):
            m.apply("x%d\n" % i, K_OUT)
        self.assertLessEqual(len(m.lines), ConsoleModel.MAX_LINES)
        self.assertEqual(m.lines[-2][0], "x2999")
        self.assertEqual((3001 - len(m.lines)) % ConsoleModel.TRIM_STEP, 0)   # отброшено кратно шагу


class TestRepl(CoreTestCase):
    def setUp(self):
        super().setUp()
        self.model = ConsoleModel()
        self.mlock = threading.Lock()
        self.ran_editor = []
        self.quit = []

        def emit(text, kind, line=False):
            with self.mlock:
                self.model.apply(text, kind, line)

        # вывод ядра идёт в ту же модель, как это делает приложение
        self.e.close()
        self.e = Engine(lambda t, k: emit(t, k), self.c.done)
        self.repl = Repl(self.e, emit, run_editor=lambda: self.ran_editor.append(1), quit_app=lambda: self.quit.append(1))

    def say(self, *lines):
        for ln in lines:
            self.repl.submit(ln)
            self.e.wait_idle(10)
        time.sleep(0.05)

    def text(self):
        with self.mlock:
            return self.model.plain_text()

    def test_expression_and_echo(self):
        self.say("2 + 3")
        self.assertIn(">>> 2 + 3", self.text())
        self.assertIn("\n5\n", "\n" + self.text())

    def test_multiline_block(self):
        self.say("var i = 0", "while i < 3 {", "    show (i)", "    i = i + 1", "}")
        out = self.text()
        self.assertIn("... ", out)                              # приглашение продолжения
        self.assertTrue(out.rstrip().endswith("0\n1\n2".rstrip()) or "0\n1\n2" in out)

    def test_if_else_on_next_line(self):
        self.say("var x = 5", "if (x > 3) { show (\"big\") }", "else { show (\"small\") }")
        self.assertIn("big", self.text())
        self.assertNotIn("small\n", self.text().replace("else { show (\"small\") }", ""))

    def test_if_without_else_executes_on_next_command(self):
        self.say("var x = 1", "if (x == 1) { show (\"one\") }", "show (\"next\")")
        out = self.text()
        self.assertIn("one", out)
        self.assertIn("next", out)

    def test_commands(self):
        self.say("var q = 7", "vars")
        self.assertIn("q = 7", self.text())
        self.say("noopt")
        self.assertIn("Оптимизатор: выключен", self.text())
        self.assertFalse(self.repl.optimize)
        self.say("opt")
        self.assertTrue(self.repl.optimize)
        self.say("explain")
        self.assertTrue(self.repl.explain_mode)
        self.say("quiet")
        self.assertFalse(self.repl.explain_mode)
        self.say("explain show (1)")
        self.assertGreater(len(self.text()), 500)
        self.say("run")
        self.assertEqual(self.ran_editor, [1])
        self.say("help")
        self.assertIn("Команды консоли", self.text())
        self.say("exit")
        self.assertEqual(self.quit, [1])
        self.say("clear")
        self.assertEqual(self.text(), "")

    def test_variable_shadows_command(self):
        self.say("var run = 3", "run")
        self.assertEqual(self.ran_editor, [])                  # «run» занято переменной: это выражение, а не команда
        self.assertIn("3", self.text())

    def test_input_inside_repl(self):
        self.repl.submit("var n = input")
        self.e.wait_idle(5)
        self.repl.submit('show ("Число: ", n)')
        self.assertTrue(self.wait_for(lambda: self.e.waiting_input))
        self.assertEqual(self.repl.prompt, "ввод> ")
        self.repl.submit("41")
        self.assertTrue(self.e.wait_idle(10))
        time.sleep(0.05)
        self.assertIn("Число: 41", self.text())

    def test_history(self):
        self.say("show (1)", "show (2)")
        self.assertEqual(self.repl.history_prev(), "show (2)")
        self.assertEqual(self.repl.history_prev(), "show (1)")
        self.assertEqual(self.repl.history_next(), "show (2)")
        self.assertEqual(self.repl.history_next(), "")


if __name__ == "__main__":
    try:
        unittest.main(verbosity=2)
    finally:
        if _TMP:
            shutil.rmtree(_TMP, ignore_errors=True)
