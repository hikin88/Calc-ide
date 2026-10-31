"""Calc IDE — редактор и консоль для языка из calc_core.cpp (Kivy, Android).

Интерфейс (редактор, кнопки, консоль) — на Python/Kivy. Программы выполняет C++ ядро (libcalccore.so)
в рабочем потоке; вывод и ввод идут через calc_bridge.Engine.
"""
import json
import os
import time
from collections import deque

from kivy.app import App
from kivy.clock import Clock
from kivy.core.clipboard import Clipboard
from kivy.core.window import Window
from kivy.metrics import dp, sp
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.button import Button
from kivy.uix.filechooser import FileChooserListView
from kivy.uix.label import Label
from kivy.uix.popup import Popup
from kivy.uix.scrollview import ScrollView
from kivy.uix.textinput import TextInput
from kivy.utils import escape_markup

from calc_bridge import RC_CANCELLED, RC_COMPILE, RC_OK, Engine
from session import (K_CLEAR, K_ECHO, K_ERR, K_INFO, K_OUT, SAMPLE_PROGRAM, ConsoleModel, Repl)

HERE = os.path.dirname(os.path.abspath(__file__))
_FONT = os.path.join(HERE, "fonts", "JetBrainsMono-Regular.ttf")
MONO = _FONT if os.path.exists(_FONT) else "Roboto"

BG = (0.12, 0.12, 0.12, 1)
PANEL = (0.09, 0.09, 0.09, 1)
TEXT = (0.83, 0.83, 0.83, 1)

# цвета строк консоли (hex для markup)
LINE_COLORS = {K_OUT: "cccccc", K_ERR: "f48771", K_ECHO: "56b6c2", K_INFO: "8fbc8f"}

QUICK_KEYS = ["Tab", "{", "}", "(", ")", '"', "$", "=", "==", "!=", "<", ">", "<=", ">=",
              "+", "-", "*", "/", "%", "#", ",", ";"]


def fix_glyphs(text):
    """В шрифте нет галочки ✔ — заменяем на близкий по смыслу символ."""
    return text.replace("\u2714", "\u221a").replace("\u2713", "\u221a").replace("\t", "    ")


class ConsoleView(ScrollView):
    """Прокручиваемый вывод. Строки разбиты на блоки по CHUNK штук — по одному Label на блок, чтобы текстуры
    оставались небольшими, а при новом выводе перерисовывался только последний блок."""

    CHUNK = 25

    def __init__(self, model, **kwargs):
        super().__init__(do_scroll_x=False, bar_width=dp(5), **kwargs)
        self.model = model
        self.box = BoxLayout(orientation="vertical", size_hint_y=None)
        self.box.bind(minimum_height=self.box.setter("height"))
        self.add_widget(self.box)
        self._labels = []
        self._texts = []
        self._shown_version = -1

    def _new_label(self):
        lbl = Label(text="", markup=True, font_name=MONO, font_size=sp(13), color=TEXT,
                    size_hint_y=None, height=dp(18), halign="left", valign="top", padding=(dp(6), 0))
        lbl.bind(width=lambda inst, w: setattr(inst, "text_size", (w, None)))
        lbl.bind(texture_size=lambda inst, ts: setattr(inst, "height", max(ts[1], 0)))
        return lbl

    @staticmethod
    def _markup(text, kind):
        esc = escape_markup(fix_glyphs(text))
        color = LINE_COLORS.get(kind)
        return "[color=%s]%s[/color]" % (color, esc) if color and esc else esc

    def refresh(self):
        if self.model.version == self._shown_version:
            return
        self._shown_version = self.model.version
        at_bottom = self.scroll_y <= 0.02 or self.box.height <= self.height
        lines = self.model.lines
        n_chunks = max(1, -(-len(lines) // self.CHUNK))
        while len(self._labels) < n_chunks:
            lbl = self._new_label()
            self._labels.append(lbl)
            self._texts.append(None)
            self.box.add_widget(lbl)
        while len(self._labels) > n_chunks:
            self.box.remove_widget(self._labels.pop())
            self._texts.pop()
        for ci in range(n_chunks):
            chunk = lines[ci * self.CHUNK:(ci + 1) * self.CHUNK]
            text = "\n".join(self._markup(t, k) for t, k in chunk)
            if self._texts[ci] != text:
                self._texts[ci] = text
                self._labels[ci].text = text
        if at_bottom:
            Clock.schedule_once(lambda dt: setattr(self, "scroll_y", 0), 0)


class CalcIdeApp(App):
    title = "Calc IDE"

    # ------------------------------------------------------------------ построение интерфейса
    def build(self):
        Window.clearcolor = BG
        try:
            Window.softinput_mode = "resize"        # клавиатура сжимает окно, а не закрывает поле ввода
        except Exception:
            pass
        self.programs_dir = os.path.join(self.user_data_dir, "programs")
        os.makedirs(self.programs_dir, exist_ok=True)
        self.state_file = os.path.join(self.programs_dir, ".state.json")
        self.draft_file = os.path.join(self.programs_dir, ".draft.txt")

        self._outq = deque()                        # (текст, вид, line) из любых потоков → в консоль
        self._doneq = deque()                       # результаты запусков из рабочего потока
        self.console = ConsoleModel()
        self.current_path = None
        self.dirty = False
        self._loading = False
        self._popup = None
        self._prev_waiting = False
        self._status_until = 0.0

        try:
            self.engine = Engine(on_output=self._on_output, on_done=self._doneq.append)
        except Exception as exc:                    # не удалось загрузить libcalccore.so
            return Label(text="Не удалось загрузить ядро компилятора:\n\n%s" % exc, halign="left",
                         valign="top", padding=(dp(12), dp(12)), text_size=(Window.width - dp(24), None))
        self.repl = Repl(self.engine, self.emit, run_editor=self.run_program, quit_app=self.stop)

        root = BoxLayout(orientation="vertical")
        self.root_layout = root
        root.add_widget(self._build_toolbar())

        self.status = Label(text="", size_hint_y=None, height=dp(22), font_size=sp(12), halign="left",
                            valign="middle", color=(0.7, 0.7, 0.7, 1), padding=(dp(8), 0))
        self.status.bind(size=lambda inst, s: setattr(inst, "text_size", s))
        root.add_widget(self.status)

        self.editor = TextInput(
            multiline=True, font_name=MONO, font_size=sp(15), write_tab=False, unfocus_on_touch=False,
            background_color=BG, foreground_color=TEXT, cursor_color=(1, 1, 1, 1),
            selection_color=(0.2, 0.45, 0.8, 0.45), padding=(dp(8), dp(6)), size_hint_y=0.55)
        self.editor.bind(text=self._on_editor_text)
        root.add_widget(self.editor)

        root.add_widget(self._build_quick_keys())

        self.console_panel = self._build_console()
        root.add_widget(self.console_panel)

        Window.bind(on_keyboard=self._on_keyboard)
        Clock.schedule_interval(self._pump, 1 / 15.0)
        self._startup_document()
        self._emit_welcome()
        return root

    def _btn(self, text, handler, width=dp(92), color=None):
        b = Button(text=text, size_hint_x=None, width=width, font_size=sp(13), on_release=handler)
        if color:
            b.background_color = color
        return b

    def _build_toolbar(self):
        scroll = ScrollView(size_hint_y=None, height=dp(46), do_scroll_y=False, bar_width=0)
        bar = BoxLayout(size_hint_x=None, spacing=dp(4), padding=(dp(4), dp(3)))
        bar.bind(minimum_width=bar.setter("width"))
        self.btn_run = self._btn("Запуск", self.run_program, color=(0.3, 0.75, 0.35, 1))
        self.btn_stop = self._btn("Стоп", self.stop_program, width=dp(72), color=(0.85, 0.35, 0.3, 1))
        self.btn_console = self._btn("Консоль", self.toggle_console, width=dp(88))
        for b in (self.btn_run, self.btn_stop,
                  self._btn("Сохранить", self.save_current, dp(100)),
                  self._btn("Открыть", self.open_dialog, dp(88)),
                  self._btn("Новый", self.new_document, dp(76)),
                  self._btn("Сохр. как", self.save_as_dialog, dp(92)),
                  self.btn_console,
                  self._btn("Копировать вывод", self.copy_output, dp(150))):
            bar.add_widget(b)
        scroll.add_widget(bar)
        return scroll

    def _build_quick_keys(self):
        scroll = ScrollView(size_hint_y=None, height=dp(40), do_scroll_y=False, bar_width=0)
        bar = BoxLayout(size_hint_x=None, spacing=dp(3), padding=(dp(3), dp(2)))
        bar.bind(minimum_width=bar.setter("width"))
        for sym in QUICK_KEYS:
            w = dp(52) if len(sym) > 1 else dp(40)
            b = Button(text=sym, size_hint_x=None, width=w, font_name=MONO, font_size=sp(15))
            b.bind(on_release=lambda inst, s=sym: self._insert_symbol(s))
            bar.add_widget(b)
        scroll.add_widget(bar)
        return scroll

    def _build_console(self):
        panel = BoxLayout(orientation="vertical", size_hint_y=0.45)
        self.console_view = ConsoleView(self.console, size_hint_y=1)
        panel.add_widget(self.console_view)
        row = BoxLayout(size_hint_y=None, height=dp(46), spacing=dp(3), padding=(dp(3), dp(2)))
        self.prompt_label = Label(text=">>> ", size_hint_x=None, width=dp(62), font_name=MONO, font_size=sp(13),
                                  color=(0.55, 0.85, 0.55, 1))
        self.cin = TextInput(multiline=False, font_name=MONO, font_size=sp(14), write_tab=False,
                             background_color=BG, foreground_color=TEXT, cursor_color=(1, 1, 1, 1),
                             hint_text="команда или ответ для input", padding=(dp(8), dp(10)),
                             on_text_validate=self._on_console_submit)
        row.add_widget(self.prompt_label)
        row.add_widget(self.cin)
        row.add_widget(self._btn("OK", self._on_console_submit, dp(48)))
        row.add_widget(self._btn("^", self._hist_prev, dp(38)))
        row.add_widget(self._btn("v", self._hist_next, dp(38)))
        panel.add_widget(row)
        return panel

    # ------------------------------------------------------------------ вывод в консоль
    def _on_output(self, text, kind):               # вызывается из рабочего потока
        self._outq.append((text, kind, False))

    def emit(self, text, kind, line=False):         # из любого потока
        self._outq.append((text, kind, line))

    def _emit_welcome(self):
        self.emit("Консоль компилятора (%s). Введите команду (help — справка)\n"
                  "или нажмите «Запуск», чтобы выполнить программу из редактора.\n" % self.engine.version, K_INFO)

    def _pump(self, dt):
        """Раз в 1/15 с (в потоке интерфейса): забрать вывод, обработать завершения, обновить кнопки."""
        while True:
            try:
                text, kind, line = self._outq.popleft()
            except IndexError:
                break
            self.console.apply(text, kind, line)
        while True:
            try:
                res = self._doneq.popleft()
            except IndexError:
                break
            self._on_done(res)
        self.console_view.refresh()

        busy = self.engine.busy
        waiting = self.engine.waiting_input
        self.btn_run.disabled = busy
        self.btn_stop.disabled = not busy
        self.prompt_label.text = self.repl.prompt
        if waiting and not self._prev_waiting:       # программа ждёт ввод: показать консоль и дать ей фокус
            self._show_console(True)
            Clock.schedule_once(lambda d: setattr(self.cin, "focus", True), 0.05)
        self._prev_waiting = waiting
        if self._status_until and time.time() > self._status_until:
            self._status_until = 0.0
            self._update_status()

    def _on_done(self, res):
        if res.code == RC_CANCELLED:
            self.emit("■ Остановлено", K_INFO, True)
        elif res.tag == "editor" and res.code == RC_OK:
            ms = "менее 1 мс" if res.ms < 1 else "%.0f мс" % res.ms
            self.emit("✔ Завершено за " + ms, K_INFO, True)
        elif res.tag == "editor" and res.code == RC_COMPILE and res.error_line > 0:
            self._goto_line(res.error_line)
            self.set_status("Ошибка компиляции в строке %d" % res.error_line)

    # ------------------------------------------------------------------ запуск / остановка
    def run_program(self, *_):
        if self.engine.busy:
            self.set_status("Программа уже выполняется")
            return
        self.editor.focus = False
        self._show_console(True)
        self.repl.block = ""
        self.engine.submit_reset(self.programs_dir)  # runF ищет файлы в папке программ
        self.emit("── Запуск: %s ──" % self._display_name(), K_INFO, True)
        self.engine.submit_run(self.editor.text, self.repl.flags(False), tag="editor")

    def stop_program(self, *_):
        if self.engine.busy:
            self.engine.cancel()
            self.repl.block = ""
        else:
            self.set_status("Сейчас ничего не выполняется")

    def _goto_line(self, line):
        try:
            self.editor.cursor = (0, max(0, line - 1))
        except Exception:
            pass

    def copy_output(self, *_):
        Clipboard.copy(self.console.plain_text())
        self.set_status("Вывод консоли скопирован")

    def toggle_console(self, *_):
        self._show_console(self.console_panel.parent is None)

    def _show_console(self, show):
        attached = self.console_panel.parent is not None
        if show and not attached:
            self.root_layout.add_widget(self.console_panel)
            self.editor.size_hint_y = 0.55
        elif not show and attached:
            self.root_layout.remove_widget(self.console_panel)
            self.editor.size_hint_y = 1

    # ------------------------------------------------------------------ строка ввода консоли
    def _on_console_submit(self, *_):
        line = self.cin.text
        self.cin.text = ""
        try:
            self.repl.submit(line)
        except Exception as exc:
            self.emit("Внутренняя ошибка консоли: %s\n" % exc, K_ERR, True)
        Clock.schedule_once(lambda d: setattr(self.cin, "focus", True), 0.05)

    def _hist_prev(self, *_):
        text = self.repl.history_prev()
        if text is not None:
            self.cin.text = text

    def _hist_next(self, *_):
        text = self.repl.history_next()
        if text is not None:
            self.cin.text = text

    def _insert_symbol(self, sym):
        self.editor.insert_text("    " if sym == "Tab" else sym)

    # ------------------------------------------------------------------ статус
    def _display_name(self):
        return os.path.basename(self.current_path) if self.current_path else "без имени"

    def _update_status(self):
        self.status.text = "%s%s" % (self._display_name(), " *" if self.dirty else "")

    def set_status(self, text, seconds=4):
        self.status.text = text
        self._status_until = time.time() + seconds

    def _on_editor_text(self, *_):
        if not self._loading and not self.dirty:
            self.dirty = True
            if not self._status_until:
                self._update_status()

    # ------------------------------------------------------------------ документы и файлы
    def _set_editor_text(self, text, path, dirty=False):
        self._loading = True
        self.editor.text = text
        self._loading = False
        self.current_path = path
        self.dirty = dirty
        self.editor.cursor = (0, 0)
        self._update_status()

    def _startup_document(self):
        state = {}
        try:
            with open(self.state_file, encoding="utf-8") as f:
                state = json.load(f)
        except (OSError, ValueError):
            pass
        path = state.get("path")
        if path and os.path.isfile(path) and self.load_path(path):
            return
        if os.path.isfile(self.draft_file):
            try:
                with open(self.draft_file, encoding="utf-8", errors="replace") as f:
                    self._set_editor_text(f.read(), None, dirty=True)
                return
            except OSError:
                pass
        example = os.path.join(self.programs_dir, "primer.txt")
        if not os.path.exists(example):
            self._write_file(example, SAMPLE_PROGRAM)
        self.load_path(example)

    @staticmethod
    def _write_file(path, text):
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)

    def load_path(self, path):
        try:
            with open(path, encoding="utf-8-sig", errors="replace") as f:
                text = f.read()
        except OSError as exc:
            self.set_status("Не удалось открыть: %s" % exc)
            return False
        self._set_editor_text(text.replace("\r\n", "\n").replace("\r", "\n"), path)
        self.set_status("Открыт: " + os.path.basename(path))
        self._save_state()
        return True

    def _save_state(self):
        try:
            with open(self.state_file, "w", encoding="utf-8") as f:
                json.dump({"path": self.current_path}, f)
        except OSError:
            pass

    def save_current(self, *_, then=None):
        if not self.current_path:
            self.save_as_dialog(then=then)
            return
        if self._write_to(self.current_path) and then:
            then()

    def _write_to(self, path):
        try:
            self._write_file(path, self.editor.text)
        except OSError as exc:
            self.set_status("Ошибка сохранения: %s" % exc)
            return False
        self.current_path = path
        self.dirty = False
        self._save_state()
        self.set_status("Сохранено: " + os.path.basename(path))
        return True

    @staticmethod
    def _clean_name(name):
        name = os.path.basename(name.strip().replace("\\", "/"))
        if not name or name.startswith("."):
            return ""
        return name if "." in name else name + ".txt"

    def save_as_dialog(self, *_, then=None):
        initial = os.path.basename(self.current_path) if self.current_path else "программа.txt"
        self._ask_text("Сохранить как", initial, lambda name: self._save_as(name, then))

    def _save_as(self, name, then):
        name = self._clean_name(name)
        if not name:
            self.set_status("Укажите имя файла")
            return
        path = os.path.join(self.programs_dir, name)
        if self._write_to(path) and then:
            then()

    def _guard(self, action):
        """Выполнить action, предварительно спросив про несохранённые изменения."""
        if not self.dirty:
            action()
            return
        self._choose("Несохранённые изменения",
                     "Сохранить изменения в «%s»?" % self._display_name(),
                     [("Сохранить", lambda: self.save_current(then=action)),
                      ("Не сохранять", action),
                      ("Отмена", None)])

    def new_document(self, *_):
        self._guard(lambda: (self._set_editor_text("", None), self._save_state()))

    def open_dialog(self, *_):
        self._guard(self._open_chooser)

    def _open_chooser(self):
        chooser = FileChooserListView(path=self.programs_dir, rootpath=self.programs_dir, dirselect=False)
        content = BoxLayout(orientation="vertical", spacing=dp(4))
        content.add_widget(chooser)
        row = BoxLayout(size_hint_y=None, height=dp(46), spacing=dp(4))

        def do_open(*_a):
            if chooser.selection:
                path = chooser.selection[0]
                self.close_popup()
                self.load_path(path)

        def do_delete(*_a):
            if not chooser.selection:
                return
            path = chooser.selection[0]
            self.close_popup()
            self._choose("Удалить файл", "Удалить «%s»?" % os.path.basename(path),
                         [("Удалить", lambda: self._delete_file(path)), ("Отмена", None)])

        for text, handler in (("Открыть", do_open), ("Удалить", do_delete), ("Отмена", lambda *_a: self.close_popup())):
            row.add_widget(Button(text=text, on_release=handler))
        content.add_widget(row)
        chooser.bind(on_submit=lambda *_a: do_open())
        self._popup_open("Открыть файл", content, (0.95, 0.9))

    def _delete_file(self, path):
        try:
            os.remove(path)
        except OSError as exc:
            self.set_status("Не удалось удалить: %s" % exc)
            return
        if path == self.current_path:
            self.current_path = None
            self.dirty = True
            self._save_state()
        self.set_status("Удалён: " + os.path.basename(path))
        self._update_status()

    # ------------------------------------------------------------------ окна
    def _popup_open(self, title, content, size_hint=(0.9, 0.5)):
        self.close_popup()
        popup = Popup(title=title, content=content, size_hint=size_hint)
        popup.bind(on_dismiss=lambda *_a: setattr(self, "_popup", None))
        self._popup = popup
        popup.open()
        return popup

    def close_popup(self):
        if self._popup is not None:
            popup, self._popup = self._popup, None
            popup.dismiss()

    def _choose(self, title, message, options):
        """Окно с сообщением и кнопками: options — список (подпись, действие или None)."""
        content = BoxLayout(orientation="vertical", spacing=dp(8), padding=dp(8))
        lbl = Label(text=message, halign="center", valign="middle")
        lbl.bind(size=lambda inst, s: setattr(inst, "text_size", s))
        content.add_widget(lbl)
        row = BoxLayout(size_hint_y=None, height=dp(46), spacing=dp(6))
        for text, action in options:
            def handler(*_a, act=action):
                self.close_popup()
                if act:
                    act()
            row.add_widget(Button(text=text, on_release=handler))
        content.add_widget(row)
        self._popup_open(title, content, (0.9, 0.35))

    def _ask_text(self, title, initial, on_ok):
        content = BoxLayout(orientation="vertical", spacing=dp(8), padding=dp(8))
        field = TextInput(text=initial, multiline=False, size_hint_y=None, height=dp(44), font_size=sp(15))
        content.add_widget(field)
        content.add_widget(Label())
        row = BoxLayout(size_hint_y=None, height=dp(46), spacing=dp(6))

        def ok(*_a):
            value = field.text
            self.close_popup()
            on_ok(value)

        field.bind(on_text_validate=ok)
        row.add_widget(Button(text="OK", on_release=ok))
        row.add_widget(Button(text="Отмена", on_release=lambda *_a: self.close_popup()))
        content.add_widget(row)
        self._popup_open(title, content, (0.9, 0.38))
        Clock.schedule_once(lambda d: setattr(field, "focus", True), 0.2)

    def _on_keyboard(self, window, key, *args):
        if key == 27 and self._popup is not None:    # «Назад» закрывает окно, а не приложение
            self.close_popup()
            return True
        return False

    # ------------------------------------------------------------------ жизненный цикл
    def _autosave(self):
        try:
            if self.current_path and self.dirty:
                self._write_file(self.current_path, self.editor.text)
                self.dirty = False
            elif not self.current_path and self.editor.text.strip():
                self._write_file(self.draft_file, self.editor.text)
            self._save_state()
        except Exception:
            pass

    def on_pause(self):
        self._autosave()
        return True

    def on_stop(self):
        self._autosave()
        try:
            self.engine.close()
        except Exception:
            pass


if __name__ == "__main__":
    CalcIdeApp().run()
