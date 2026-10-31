// Учебный компилятор на C++17: выражения, переменные, вывод, условия, циклы, списки и функции.
//
// Конвейер:
//   текст → [Лексер] → токены → [Парсер] → AST → [Семантика] → AST
//        → [Оптимизатор] → AST → [Кодогенератор] → байт-код
//        → [Виртуальная машина] → результат
//
// Язык:
//   var y = 5            создать переменную (число или строка)
//   y = y + 1            изменить уже существующую переменную (присваивание без var)
//   var z = x - y        в значении можно использовать выражения и другие переменные
//   show (z)             вывести значение выражения
//   show ("a = ", a, "!")  можно несколько значений через запятую: печатаются подряд
//   var s = input        переменная с ОТЛОЖЕННЫМ вводом: сейчас ничего не спрашиваем, пользователь
//                        вводит значение при первом использовании s (число — как число, остальное — строка)
//   length (x)           длина строки, списка или записи числа
//   show ("Привет")      строки пишутся в двойных кавычках
//   2 + 3 * (4 - 1)      просто выражение — результат вернётся / напечатается
//   # комментарий        комментарий: от '#' или '//' до конца строки
//
//   Условия:
//     if (x > y) { show (x) }
//     else { show (y) }
//
//     if (a == 1) { show ("один") }
//     else if (a == 2) { show ("два") }
//     else { show ("много") }
//
//   Цикл while: тело повторяется, пока условие истинно (не равно 0):
//     var x = 1
//     while x < 4 {
//         var x = x + 1
//         show (x)            → 2, 3, 4
//     }
//   Условие можно писать со скобками и без: while (x < 4) { ... }. То же верно
//   для if. Цикл вложенный и в if, и в другие циклы. Чтобы вечный цикл не
//   повесил программу, выполнение останавливается после 50 млн инструкций.
//
//   Ввод (input) и отложенное чтение:
//     var x = input                ничего не спрашивает: x просто «ждёт» ввода
//     show ("Введите число: ", x)  подсказка печатается сейчас, и только потом программа
//                                  ждёт ввод; введённое значение становится значением x
//     show ("Ещё раз: ", input)    input прямо в выражении читает строку сразу
//   После первого использования значение запоминается и больше не спрашивается.
//   Если переменную с input ни разу не использовать, запроса не будет совсем.
//   Порядок запросов — порядок ПЕРВЫХ использований переменных, а не порядок var:
//     var x = input
//     var y = input
//     show ("y: ", y)              спросит сначала y, потом (ниже) x
//   show печатает значения по мере вычисления, поэтому текст перед переменной
//   с input всегда появляется на экране раньше, чем начинается ожидание ввода.
//
//   Сравнения: ==  !=  <  >  <=  >=   (дают 1 — истина, 0 — ложь)
//   Сравнивать можно числа с числами и строки со строками. Приоритет
//   сравнений ниже, чем у + и -, цепочки (a < b < c) не поддерживаются:
//   пишите a < b and b < c.
//   Команды разделяются переводом строки или ';'. 'else' можно писать
//   на следующей строке после '}'.
//
//   Присваивание (без var):
//     var x = 1
//     x = x + 1            теперь x = 2
//     x = input            отложенный ввод, как и в var x = input
//   Присваивание НЕ создаёт переменную: если x не объявлена через var — ошибка.
//   Списку присваивать нельзя (x = 5, где x — список, — ошибка; чтобы заменить
//   список, пишите list x = ...), а положить список в переменную тоже нельзя.
//   Внутри функции x = ... меняет локальную переменную с таким именем, а если
//   её нет — ГЛОБАЛЬНУЮ (так функция может менять глобальное состояние).
//   Команда var по-прежнему всегда создаёт локальную переменную.
//     var total = 0
//     func add (n) { total = total + n }
//     add(5)  add(7)       total = 12
//
//   Логика: and, or, not (результат — 1 или 0):
//     if (x > 1 and x < 5) { show ("от 2 до 4") }
//     if (not (x == 3) or ready) { show ("ок") }
//   Приоритет от низкого к высокому: or, and, not, сравнения, + -, * / %, **.
//   Значит, not x == 1 — это not (x == 1), а a < b and c < d не требует скобок.
//   and и or ленивые: правая часть вычисляется, только если результат ещё не
//   известен (0 and f() и 1 or f() функцию f не вызовут). Операнды — числа или
//   результаты сравнений; со строкой или списком будет ошибка, как и в условии if.
//
//   break и continue (только внутри while):
//     var i = 0
//     while i < 10 {
//         i = i + 1
//         if (i % 2 == 0) { continue }   // чётные пропускаем: сразу к проверке условия
//         if (i > 7) { break }           // выйти из цикла совсем
//         show (i)                       // → 1, 3, 5, 7
//     }
//   Действуют на ближайший (самый внутренний) цикл. continue переходит к проверке
//   условия, поэтому счётчик нужно менять ДО continue (иначе цикл станет вечным).
//   Вне цикла — ошибка компиляции; break внутри функции не может прервать цикл
//   вызывающего кода: у функции свои циклы.
//
//   Списки (массивы):
//     list x = 5, 6, 4, 2
//     list y = "Дима", "Толя"
//     show (x)             → 5642   (элементы выводятся подряд, без разделителей)
//     show (y $0)          → Дима   (индекс пишется после '$', счёт с нуля)
//     var i = 1
//     show (y $i)          → Толя   (индекс — число, переменная или (выражение))
//     show (x $(i + 1) * 2)→ 8      ('$' привязан к списку сильнее, чем '*')
//   В списке могут лежать числа, строки и выражения: list z = 1 + 2, y $0.
//   Список целиком нельзя использовать в арифметике, сравнениях и условиях.
//   Имя не может быть одновременно переменной (var) и списком (list):
//     list x = 1, 2
//     var x = 5            → ошибка: имя 'x' уже занято списком (и наоборот)
//   Повторный list с тем же именем перезаписывает список, повторный var —
//   переменную. Переменная хранит только число или строку: var b = x, где
//   x — список, — ошибка (используйте элемент: var b = x $0).
//   Индекс вне границ списка — ошибка выполнения.
//
//   Запуск программы из файла:
//     runF "имя файла.txt"
//   Файл ищется рядом с компилятором (потом — рядом с запускающим файлом и
//   в текущей папке). Его код выполняется с общей памятью: переменные файла
//   доступны после runF, и наоборот. runF можно писать внутри if/else и
//   внутри самих файлов; файл, запускающий сам себя, — ошибка. Имя файла
//   может быть и переменной: var f = "a.txt"  и  runF f.
//   Ошибки внутри файла показывают его имя и номер строки.
//
//   Функции (func):
//     func sum (x, y) {
//         var z = x + y
//         show (z)
//     }
//     sum(5, 7)              → 12
//   После слова func идут имя функции и параметры в скобках через запятую
//   (параметров может не быть: func hello () { show ("Привет") }).
//   Вызов — имя и аргументы в скобках: sum(5, 7), sum(a, b * 2).
//   Функцию можно объявить в любом месте программы, но вызвать — только после
//   того, как выполнилось её объявление. Повторный func с тем же именем
//   заменяет функцию. Объявление можно писать и в одну строку:
//     func sum (x, y) { var z = x + y; show (z) }
//
//   Результат функции — слово return (после return выполнение функции
//   сразу заканчивается). Вызов с результатом можно использовать в выражениях:
//     func square (n) { return n * n }
//     show (square(4) + 1)   → 17
//     var r = square(3)
//   Функция без return ничего не возвращает, её можно вызывать только как
//   отдельную команду (sum(5, 7)), а не внутри выражения.
//   Вызов как отдельная команда ведёт себя как обычное выражение: если функция
//   вернула значение, в интерактивном режиме оно печатается.
//
//   Рекурсия работает (глубина — до 10 000 вложенных вызовов):
//     func fact (n) {
//         if (n <= 1) { return 1 }
//         return n * fact(n - 1)
//     }
//     show (fact(5))         → 120
//
//   Переменные функции локальны: параметры и всё, что создано внутри через
//   var / list, живёт только во время вызова. Читать глобальные переменные
//   функция может, но var внутри функции всегда создаёт ЛОКАЛЬНУЮ переменную
//   и не меняет глобальную с тем же именем; а присваивание x = ... без var меняет
//   локальную переменную, если она есть, иначе глобальную. Функции можно передать список:
//   f(x) — внутри будет доступно x $0. Имена функций и переменных живут
//   раздельно (func sum и var sum не конфликтуют).
//
//   Числа: 42, 3.14. Операции: + - * / % ** , унарный минус, скобки.
//   Имена переменных: буквы (в т. ч. русские), цифры и '_', не с цифры.
//   Зарезервированные слова: var, show, list, if, else, while, runF, func, return, input,
//   and, or, not, break, continue.
//   Встроенная функция: length (имя length занято, свою func length создать нельзя).
//   Область видимости одна на всю программу: переменная, созданная внутри
//   { } , остаётся доступной и после блока (если ветка выполнилась).
//
// По умолчанию компилятор работает «молча»: calc::run(...) только исполняет
// код. Пошаговый разбор (токены, дерево, байт-код, стек) включается ОТДЕЛЬНОЙ
// функцией calc::explain(...).
//
//   calc::Environment env;                  // память: переменные + поток вывода
//   calc::run("var x = 5", env);            // тихо
//   calc::explain("if (x > 3) { show (x) }", env);   // с разбором всех этапов
//
// ЭТО ЯДРО ДЛЯ ПРИЛОЖЕНИЙ: компилятор собирается как динамическая библиотека (libcalccore.so)
// и вызывается из Python/Kivy через C-интерфейс (см. раздел «C-ИНТЕРФЕЙС» в конце кода).
//
// Сборка для проверки на компьютере:
//   g++ -std=c++17 -O2 -fPIC -shared -o libcalccore.so calc_core.cpp
// Для Android библиотеку собирает рецепт python-for-android (recipes/calccore).
//
// Все числа хранятся как double (в отличие от Python, где есть big int).
// В конце файла — дорожная карта дальнейшего расширения.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace calc {

// ════════════════════════════════════════════════════════════════════
//  Ошибки
// ════════════════════════════════════════════════════════════════════

// Ошибка компиляции с позицией в исходном тексте.
class CompileError : public std::runtime_error {
public:
    CompileError(const std::string& msg, std::size_t pos, const std::string& src)
        : std::runtime_error(msg), pos_(pos), src_(src) {}

    // Сообщение + строка с ошибкой + указатель '^' на проблемное место.
    // Если программа многострочная, добавляется номер строки.
    std::string pretty() const {
        std::size_t p = std::min(pos_, src_.size());
        std::size_t lineNo = 1 + static_cast<std::size_t>(std::count(src_.begin(), src_.begin() + p, '\n'));
        std::size_t start = 0;
        if (p > 0) {
            std::size_t nl = src_.rfind('\n', p - 1);
            start = (nl == std::string::npos) ? 0 : nl + 1;
        }
        std::size_t end = src_.find('\n', p);
        if (end == std::string::npos) end = src_.size();
        std::string line = src_.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();

        std::size_t col = 0;                       // считаем символы, а не байты UTF-8
        for (std::size_t i = start; i < p; ++i) {
            if ((static_cast<unsigned char>(src_[i]) & 0xC0) != 0x80) ++col;
        }

        std::size_t firstNl = src_.find('\n');
        bool multiline = firstNl != std::string::npos &&
                         src_.find_first_not_of(" \t\r\n", firstNl) != std::string::npos;
        std::string where;                         // «файл a.txt, строка 3» / «строка 3»
        if (!file_.empty()) where = "файл " + file_ + ", строка " + std::to_string(lineNo);
        else if (multiline) where = "строка " + std::to_string(lineNo);
        return std::string(what()) + (where.empty() ? "" : " (" + where + ")") +
               "\n  " + line + "\n  " + std::string(col, ' ') + "^";
    }

    // Номер строки ошибки (с 1) в исходном тексте.
    std::size_t line() const {
        std::size_t p = std::min(pos_, src_.size());
        return 1 + static_cast<std::size_t>(std::count(src_.begin(), src_.begin() + p, '\n'));
    }

    // Имя файла, в котором найдена ошибка (если код пришёл из runF).
    bool hasFile() const { return !file_.empty(); }
    void setFile(std::string name) { file_ = std::move(name); }

private:
    std::size_t pos_;
    std::string src_;
    std::string file_;
};

// Ошибка времени выполнения (деление на ноль и т. п.).
class RuntimeError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// ════════════════════════════════════════════════════════════════════
//  Значения и память
//  Значение — число, строка или список. Environment хранит переменные и поток,
//  в который команда show печатает (по умолчанию std::cout).
// ════════════════════════════════════════════════════════════════════

// Кратчайшая запись double, которая читается обратно без потерь — как у std::to_chars.
// Своя реализация, потому что to_chars для double есть не во всех библиотеках C++
// (в частности, нет в libc++ старых версий Android NDK): подбираем число цифр через snprintf/strtod,
// затем выбираем более короткую из записей «обычная» и «с порядком» (при равенстве — обычная).
std::string shortestDouble(double v) {
    char buf[48];
    for (int prec = 0; prec <= 16; ++prec) {                  // %.{prec}e даёт prec+1 значащих цифр
        std::snprintf(buf, sizeof buf, "%.*e", prec, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    std::string s(buf);
    bool neg = !s.empty() && s[0] == '-';
    if (neg) s.erase(0, 1);
    std::size_t epos = s.find('e');
    int exp10 = std::atoi(s.c_str() + epos + 1);
    std::string digits;
    for (std::size_t i = 0; i < epos; ++i) if (s[i] != '.') digits += s[i];
    while (digits.size() > 1 && digits.back() == '0') digits.pop_back();
    const int n = static_cast<int>(digits.size());

    std::string sci = digits.substr(0, 1);
    if (n > 1) sci += "." + digits.substr(1);
    int ae = exp10 < 0 ? -exp10 : exp10;
    sci += std::string("e") + (exp10 < 0 ? "-" : "+") + (ae < 10 ? "0" : "") + std::to_string(ae);

    std::string fixed;
    if (exp10 >= 0) {
        if (n <= exp10 + 1) {                                  // целое число: как printf("%.0f"), т. е. точное значение
            char big[400];
            std::snprintf(big, sizeof big, "%.0f", std::fabs(v));
            fixed = big;
        } else fixed = digits.substr(0, static_cast<std::size_t>(exp10 + 1)) + "." + digits.substr(static_cast<std::size_t>(exp10 + 1));
    } else {
        fixed = "0." + std::string(static_cast<std::size_t>(-exp10 - 1), '0') + digits;
    }
    return (neg ? "-" : "") + (fixed.size() <= sci.size() ? fixed : sci);
}

// Форматирование числа: 5.0 → "5", 2.5 → "2.5", 1/3 → "0.3333333333333333".
std::string fmt(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
    if (v == std::floor(v) && std::fabs(v) < 1e15) {
        return std::to_string(static_cast<long long>(v));
    }
    return shortestDouble(v);                              // кратчайшая точная запись
}

struct Value {
    enum class Type { Number, String, List, Pending };   // Pending — input, который ещё не прочитан
    Type type = Type::Number;
    double num = 0;
    std::string str;
    std::vector<Value> items;      // для List

    static Value number(double v) { Value x; x.type = Type::Number; x.num = v; return x; }
    static Value text(std::string s) { Value x; x.type = Type::String; x.str = std::move(s); return x; }
    static Value list(std::vector<Value> v) { Value x; x.type = Type::List; x.items = std::move(v); return x; }
    // «Ожидание ввода»: лежит в переменной после var x = input. Когда переменную впервые читают
    // (LOAD), VM спрашивает пользователя и заменяет Pending настоящим значением.
    // На стек вычислений Pending никогда не попадает.
    static Value pending() { Value x; x.type = Type::Pending; return x; }
    bool isNumber() const { return type == Type::Number; }
    bool isList() const { return type == Type::List; }
};

// Как значение выглядит при выводе (show): строка без кавычек,
// список — элементы подряд без разделителей (5, 6, 4, 2 → 5642).
std::string display(const Value& v) {
    switch (v.type) {
        case Value::Type::Number: return fmt(v.num);
        case Value::Type::String: return v.str;
        case Value::Type::List: {
            std::string s;
            for (const Value& item : v.items) s += display(item);
            return s;
        }
        case Value::Type::Pending: return "";
    }
    return "";
}

// Как значение выглядит в отладочном выводе: строка в кавычках, список в [ ].
std::string repr(const Value& v) {
    switch (v.type) {
        case Value::Type::Number: return fmt(v.num);
        case Value::Type::String: return "\"" + v.str + "\"";
        case Value::Type::List: {
            std::string s = "[";
            for (std::size_t i = 0; i < v.items.size(); ++i) s += (i ? ", " : "") + repr(v.items[i]);
            return s + "]";
        }
        case Value::Type::Pending: return "<input: ждёт первого использования>";
    }
    return "";
}

struct Function;                             // функция, созданная через func (определена ниже, рядом с байт-кодом)

struct Environment {
    std::map<std::string, Value> vars;       // глобальные переменные и списки
    std::map<std::string, std::shared_ptr<const Function>> funcs;   // функции, созданные через func
    std::ostream* out = &std::cout;
    std::istream* in = &std::cin;            // откуда input читает ввод пользователя
    std::string exeDir;                      // папка компилятора: там runF ищет файлы
    std::vector<std::string> fileStack;      // файлы, выполняемые через runF прямо сейчас
};

struct Options {
    bool optimize = true;                       // включить оптимизатор
    std::size_t maxSteps = 50'000'000;          // лимит инструкций на одну программу (защита от вечных циклов)
    const std::atomic<bool>* cancel = nullptr;  // если станет true — программа прервётся (RuntimeError)
    std::function<void()> onTick;               // вызывается примерно раз в 4096 инструкций (досылка вывода и т. п.)
};

// Выполняет файл для команды runF (определена ниже, после конвейера).
void runFileCommand(const std::string& name, Environment& env, const Options& opt, std::ostream* log);

// ════════════════════════════════════════════════════════════════════
//  ЭТАП 1. ЛЕКСЕР — текст → токены
// ════════════════════════════════════════════════════════════════════

enum class TokenType {
    Number, String, Ident,
    KwVar, KwShow, KwIf, KwElse, KwWhile, KwList, KwRunF, KwFunc, KwReturn, KwInput,   // зарезервированные слова
    KwAnd, KwOr, KwNot, KwBreak, KwContinue,         // логика и управление циклом
    Plus, Minus, Star, Slash, Percent, Power,
    Eq, Ne, Lt, Gt, Le, Ge,                          // == != < > <= >=
    LParen, RParen, LBrace, RBrace, Assign,
    Comma, Dollar,                                   // ,  и  $ (индекс списка)
    Semicolon, Newline, End
};

struct Token {
    TokenType type;
    double value;        // только для Number
    std::size_t pos;     // позиция в исходном тексте (для сообщений об ошибках)
    std::string text{};  // для String (содержимое) и Ident (имя)
};

std::string tokenName(const Token& t) {
    switch (t.type) {
        case TokenType::Number:    return "NUMBER(" + fmt(t.value) + ")";
        case TokenType::String:    return "STRING(\"" + t.text + "\")";
        case TokenType::Ident:     return "IDENT(" + t.text + ")";
        case TokenType::KwVar:     return "VAR";
        case TokenType::KwShow:    return "SHOW";
        case TokenType::KwIf:      return "IF";
        case TokenType::KwElse:    return "ELSE";
        case TokenType::KwWhile:   return "WHILE";
        case TokenType::KwList:    return "LIST";
        case TokenType::KwRunF:    return "RUNF";
        case TokenType::KwFunc:    return "FUNC";
        case TokenType::KwReturn:  return "RETURN";
        case TokenType::KwInput:   return "INPUT";
        case TokenType::KwAnd:     return "AND";
        case TokenType::KwOr:      return "OR";
        case TokenType::KwNot:     return "NOT";
        case TokenType::KwBreak:   return "BREAK";
        case TokenType::KwContinue: return "CONTINUE";
        case TokenType::Comma:     return "COMMA";
        case TokenType::Dollar:    return "DOLLAR";
        case TokenType::Plus:      return "PLUS";
        case TokenType::Minus:     return "MINUS";
        case TokenType::Star:      return "STAR";
        case TokenType::Slash:     return "SLASH";
        case TokenType::Percent:   return "PERCENT";
        case TokenType::Power:     return "POWER";
        case TokenType::Eq:        return "EQ";
        case TokenType::Ne:        return "NE";
        case TokenType::Lt:        return "LT";
        case TokenType::Gt:        return "GT";
        case TokenType::Le:        return "LE";
        case TokenType::Ge:        return "GE";
        case TokenType::LParen:    return "LPAREN";
        case TokenType::RParen:    return "RPAREN";
        case TokenType::LBrace:    return "LBRACE";
        case TokenType::RBrace:    return "RBRACE";
        case TokenType::Assign:    return "ASSIGN";
        case TokenType::Semicolon: return "SEMI";
        case TokenType::Newline:   return "NEWLINE";
        case TokenType::End:       return "EOF";
    }
    return "?";
}

// Байты >= 0x80 — части UTF-8, поэтому русские буквы допустимы в именах.
bool isIdentStart(char c) {
    auto u = static_cast<unsigned char>(c);
    return std::isalpha(u) || c == '_' || u >= 0x80;
}
bool isIdentChar(char c) {
    return isIdentStart(c) || std::isdigit(static_cast<unsigned char>(c));
}

class Lexer {
public:
    explicit Lexer(const std::string& source) : src_(source) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (pos_ < src_.size()) {
            char ch = src_[pos_];
            char next = pos_ + 1 < src_.size() ? src_[pos_ + 1] : '\0';

            if (ch == '\n') {
                // Перевод строки разделяет команды, но внутри скобок ( ) он не значит ничего.
                if (parenDepth_ == 0) tokens.push_back({TokenType::Newline, 0, pos_});
                ++pos_;
            } else if (std::isspace(static_cast<unsigned char>(ch))) {
                ++pos_;
            } else if (ch == '#' || (ch == '/' && next == '/')) {   // комментарий: # или //
                while (pos_ < src_.size() && src_[pos_] != '\n') ++pos_;
            } else if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '.') {
                tokens.push_back(readNumber());
            } else if (ch == '"') {
                tokens.push_back(readString());
            } else if (isIdentStart(ch)) {
                tokens.push_back(readIdent());
            } else if (ch == '*') {                        // '*' или '**' — смотрим на символ вперёд
                if (next == '*') {
                    tokens.push_back({TokenType::Power, 0, pos_});
                    pos_ += 2;
                } else {
                    tokens.push_back({TokenType::Star, 0, pos_++});
                }
            } else if (ch == '=') {                        // '=' или '=='
                two(tokens, next == '=', TokenType::Eq, TokenType::Assign);
            } else if (ch == '<') {                        // '<' или '<='
                two(tokens, next == '=', TokenType::Le, TokenType::Lt);
            } else if (ch == '>') {                        // '>' или '>='
                two(tokens, next == '=', TokenType::Ge, TokenType::Gt);
            } else if (ch == '!') {                        // только '!='
                if (next != '=') throw CompileError("Неизвестный символ '!' (имелось в виду '!=')", pos_, src_);
                tokens.push_back({TokenType::Ne, 0, pos_});
                pos_ += 2;
            } else {
                TokenType t;
                switch (ch) {
                    case '+': t = TokenType::Plus; break;
                    case '-': t = TokenType::Minus; break;
                    case '/': t = TokenType::Slash; break;
                    case '%': t = TokenType::Percent; break;
                    case '(': t = TokenType::LParen; ++parenDepth_; break;
                    case ')': t = TokenType::RParen; if (parenDepth_ > 0) --parenDepth_; break;
                    case '{': t = TokenType::LBrace; break;
                    case '}': t = TokenType::RBrace; break;
                    case ';': t = TokenType::Semicolon; break;
                    case ',': t = TokenType::Comma; break;
                    case '$': t = TokenType::Dollar; break;
                    default:
                        throw CompileError(std::string("Неизвестный символ '") + ch + "'", pos_, src_);
                }
                tokens.push_back({t, 0, pos_++});
            }
        }
        // Конец ввода указываем сразу за последним значимым символом.
        std::size_t last = src_.find_last_not_of(" \t\r\n");
        tokens.push_back({TokenType::End, 0, last == std::string::npos ? 0 : last + 1});
        return tokens;
    }

private:
    // Двухсимвольный токен (если second == true) или односимвольный.
    void two(std::vector<Token>& tokens, bool second, TokenType twoChar, TokenType oneChar) {
        if (second) {
            tokens.push_back({twoChar, 0, pos_});
            pos_ += 2;
        } else {
            tokens.push_back({oneChar, 0, pos_++});
        }
    }

    Token readNumber() {
        std::size_t start = pos_;
        int dots = 0;
        while (pos_ < src_.size() &&
               (std::isdigit(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '.')) {
            if (src_[pos_] == '.') ++dots;
            ++pos_;
        }
        std::string text = src_.substr(start, pos_ - start);
        if (dots > 1 || text == ".") {
            throw CompileError("Некорректное число '" + text + "'", start, src_);
        }
        return {TokenType::Number, std::stod(text), start};
    }

    // Строка в двойных кавычках; escape-последовательности: \n \t \" \\.
    Token readString() {
        std::size_t start = pos_++;                        // пропускаем открывающую кавычку
        std::string text;
        while (pos_ < src_.size() && src_[pos_] != '"') {
            char c = src_[pos_++];
            if (c == '\\' && pos_ < src_.size()) {
                char e = src_[pos_++];
                switch (e) {
                    case 'n':  text += '\n'; break;
                    case 't':  text += '\t'; break;
                    case '"':  text += '"'; break;
                    case '\\': text += '\\'; break;
                    default:   text += '\\'; text += e;
                }
            } else {
                text += c;
            }
        }
        if (pos_ >= src_.size()) {
            throw CompileError("Незакрытая строка: не хватает закрывающей кавычки", start, src_);
        }
        ++pos_;                                            // закрывающая кавычка
        return {TokenType::String, 0, start, text};
    }

    // Слово: либо зарезервированное (var, show, if, else), либо имя переменной.
    Token readIdent() {
        std::size_t start = pos_;
        while (pos_ < src_.size() && isIdentChar(src_[pos_])) ++pos_;
        std::string word = src_.substr(start, pos_ - start);
        if (word == "var")  return {TokenType::KwVar, 0, start, word};
        if (word == "show") return {TokenType::KwShow, 0, start, word};
        if (word == "while") return {TokenType::KwWhile, 0, start, word};
        if (word == "list") return {TokenType::KwList, 0, start, word};
        if (word == "runF") return {TokenType::KwRunF, 0, start, word};
        if (word == "func") return {TokenType::KwFunc, 0, start, word};
        if (word == "return") return {TokenType::KwReturn, 0, start, word};
        if (word == "input") return {TokenType::KwInput, 0, start, word};
        if (word == "if")   return {TokenType::KwIf, 0, start, word};
        if (word == "else") return {TokenType::KwElse, 0, start, word};
        if (word == "and")  return {TokenType::KwAnd, 0, start, word};
        if (word == "or")   return {TokenType::KwOr, 0, start, word};
        if (word == "not")  return {TokenType::KwNot, 0, start, word};
        if (word == "break") return {TokenType::KwBreak, 0, start, word};
        if (word == "continue") return {TokenType::KwContinue, 0, start, word};
        return {TokenType::Ident, 0, start, word};
    }

    const std::string& src_;
    std::size_t pos_ = 0;
    int parenDepth_ = 0;
};

// ════════════════════════════════════════════════════════════════════
//  Таблицы операций — единое место, где описано «что значит оператор».
//  Оптимизатор и виртуальная машина берут функции отсюда.
// ════════════════════════════════════════════════════════════════════

enum class OpCode {
    Push, Load, Store, StoreList, Show, Newline, Pop, MakeList, Index, RunFile, Input, DeferInput, Length,
    Assign, DeferAssign,
    Add, Sub, Mul, Div, Mod, Pow, Neg, Not,
    Eq, Ne, Lt, Gt, Le, Ge,
    Jump, JumpIfFalse,
    DefineFunc, Call, Return,
    Halt
};

std::string opName(OpCode op) {
    switch (op) {
        case OpCode::Push:        return "PUSH";
        case OpCode::Load:        return "LOAD";
        case OpCode::Store:       return "STORE";
        case OpCode::StoreList:   return "STORE_LIST";
        case OpCode::Show:        return "SHOW";
        case OpCode::Newline:     return "NEWLINE";
        case OpCode::Input:       return "INPUT";
        case OpCode::DeferInput:  return "DEFER_INPUT";
        case OpCode::Assign:      return "ASSIGN";
        case OpCode::DeferAssign: return "DEFER_ASSIGN";
        case OpCode::Length:      return "LENGTH";
        case OpCode::Pop:         return "POP";
        case OpCode::MakeList:    return "MAKE_LIST";
        case OpCode::Index:       return "INDEX";
        case OpCode::RunFile:     return "RUN_FILE";
        case OpCode::Add:         return "ADD";
        case OpCode::Sub:         return "SUB";
        case OpCode::Mul:         return "MUL";
        case OpCode::Div:         return "DIV";
        case OpCode::Mod:         return "MOD";
        case OpCode::Pow:         return "POW";
        case OpCode::Neg:         return "NEG";
        case OpCode::Not:         return "NOT";
        case OpCode::Eq:          return "EQ";
        case OpCode::Ne:          return "NE";
        case OpCode::Lt:          return "LT";
        case OpCode::Gt:          return "GT";
        case OpCode::Le:          return "LE";
        case OpCode::Ge:          return "GE";
        case OpCode::Jump:        return "JUMP";
        case OpCode::JumpIfFalse: return "JUMP_IF_FALSE";
        case OpCode::DefineFunc:  return "DEFINE_FUNC";
        case OpCode::Call:        return "CALL";
        case OpCode::Return:      return "RETURN";
        case OpCode::Halt:        return "HALT";
    }
    return "?";
}

using BinFn = double (*)(double, double);
using UnFn = double (*)(double);

struct BinaryOpInfo { OpCode code; BinFn fn; };
struct UnaryOpInfo  { OpCode code; UnFn fn; };

double doAdd(double a, double b) { return a + b; }
double doSub(double a, double b) { return a - b; }
double doMul(double a, double b) { return a * b; }
double doDiv(double a, double b) {
    if (b == 0) throw RuntimeError("Деление на ноль");
    return a / b;
}
double doMod(double a, double b) {          // знак результата как у делителя (как в Python)
    if (b == 0) throw RuntimeError("Деление на ноль");
    double r = std::fmod(a, b);
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}
double doPow(double a, double b) {
    if (a == 0 && b < 0) throw RuntimeError("Деление на ноль");
    double r = std::pow(a, b);
    if (std::isnan(r)) throw RuntimeError("Результат не определён");
    return r;
}
double doNeg(double a) { return -a; }
double doNot(double a) { return a == 0 ? 1 : 0; }     // логическое «не»: 0 → 1, всё остальное → 0

//           символ → (опкод,     функция)
const std::map<std::string, BinaryOpInfo>& binaryOps() {
    static const std::map<std::string, BinaryOpInfo> t = {
        {"+",  {OpCode::Add, doAdd}}, {"-", {OpCode::Sub, doSub}},
        {"*",  {OpCode::Mul, doMul}}, {"/", {OpCode::Div, doDiv}},
        {"%",  {OpCode::Mod, doMod}}, {"**", {OpCode::Pow, doPow}},
    };
    return t;
}
const std::map<std::string, UnaryOpInfo>& unaryOps() {
    static const std::map<std::string, UnaryOpInfo> t = {
        {"-", {OpCode::Neg, doNeg}}, {"not", {OpCode::Not, doNot}},
    };
    return t;
}

// Сравнения: символ → опкод. Результат сравнения — число 1 (истина) или 0 (ложь).
const std::map<std::string, OpCode>& comparisonOps() {
    static const std::map<std::string, OpCode> t = {
        {"==", OpCode::Eq}, {"!=", OpCode::Ne}, {"<", OpCode::Lt},
        {">", OpCode::Gt},  {"<=", OpCode::Le}, {">=", OpCode::Ge},
    };
    return t;
}

bool isComparison(OpCode c) {
    return c == OpCode::Eq || c == OpCode::Ne || c == OpCode::Lt ||
           c == OpCode::Gt || c == OpCode::Le || c == OpCode::Ge;
}

bool compareNumbers(OpCode op, double a, double b) {
    switch (op) {
        case OpCode::Eq: return a == b;
        case OpCode::Ne: return a != b;
        case OpCode::Lt: return a < b;
        case OpCode::Gt: return a > b;
        case OpCode::Le: return a <= b;
        case OpCode::Ge: return a >= b;
        default:         return false;
    }
}

// Сравнение значений: числа с числами, строки со строками (по алфавиту, побайтово).
// Разные типы: == даёт ложь, != — истину, остальные сравнения — ошибка.
bool compareValues(OpCode op, const Value& a, const Value& b) {
    if (a.isList() || b.isList()) {
        throw RuntimeError("Списки нельзя сравнивать целиком: сравнивайте элементы (например, x $0 == y $0)");
    }
    if (a.type != b.type) {
        if (op == OpCode::Eq) return false;
        if (op == OpCode::Ne) return true;
        throw RuntimeError("Нельзя сравнивать число со строкой");
    }
    if (a.isNumber()) return compareNumbers(op, a.num, b.num);
    int c = a.str.compare(b.str);
    return compareNumbers(op, c < 0 ? -1 : (c > 0 ? 1 : 0), 0);
}

// ════════════════════════════════════════════════════════════════════
//  ЭТАП 2. ПАРСЕР — токены → дерево (AST), рекурсивный спуск
//
//  Грамматика (чем ниже правило — тем выше приоритет):
//     program    → (statement (NEWLINE | ';')+)*
//     statement  → 'var' IDENT '=' expr
//                | 'list' IDENT '=' expr (',' expr)*
//                | 'show' '(' expr ')'
//                | 'runF' expr                    ← выражение даёт имя файла
//                | 'func' IDENT '(' (IDENT (',' IDENT)*)? ')' block
//                | 'return' expr?                  ← только внутри функции
//                | 'if' expr block ('else' (block | if-statement))?
//                | 'while' expr block              ← скобки вокруг условия не обязательны
//                | IDENT '=' expr                 ← присваивание уже созданной переменной
//                | 'break' | 'continue'           ← только внутри while
//                | block
//                | expr
//     block      → '{' (statement (NEWLINE | ';')*)* '}'
//     expr       → and_expr ('or' and_expr)*
//     and_expr   → not_expr ('and' not_expr)*
//     not_expr   → 'not' not_expr | comparison
//     comparison → sum (('==' | '!=' | '<' | '>' | '<=' | '>=') sum)?
//     sum        → term   (('+' | '-') term)*
//     term       → unary  (('*' | '/' | '%') unary)*
//     unary      → '-' unary | power
//     power      → postfix ('**' unary)?          ← правоассоциативная
//     postfix    → primary ('$' primary)*         ← индекс списка: y $0
//     primary    → NUMBER | STRING | IDENT | '(' expr ')'
//                | IDENT '(' (expr (',' expr)*)? ')'        ← вызов функции
// ════════════════════════════════════════════════════════════════════

struct Node;
using NodePtr = std::unique_ptr<Node>;

struct Node {
    enum class Kind { Number, String, Variable, Unary, Binary, VarDecl, ListDecl, Index,
                      Show, RunFile, If, While, Block, FuncDecl, Return, Call, Input,
                      Assign, Logical, Break, Continue } kind;
    double value = 0;             // Number
    std::string text;             // String: содержимое; Variable, VarDecl, ListDecl, Assign: имя переменной
    std::string op;               // Unary / Binary: "+", "-", "*", "/", "%", "**", "<", ...;
                                  // Unary ещё и "not"; Logical: "and" / "or" (left, right)
    NodePtr left, right;          // Unary, VarDecl, Show, RunFile — только left;
                                  // If: left = условие, right = ветка then;
                                  // While: left = условие, right = тело (Block);
                                  // Index: left = список, right = индекс
    NodePtr elseBranch;           // If: ветка else (Block или другой If), может быть пустой
    std::vector<std::string> params;   // FuncDecl: имена параметров (text — имя функции, left — тело-Block)
                                       // Return: left — возвращаемое значение (может быть пустым)
                                       // Call: text — имя функции, body — аргументы
    bool asStatement = false;     // Call: вызов стоит как отдельная команда (результат не обязателен)
    std::vector<NodePtr> body;    // Block: список команд; ListDecl: элементы списка;
                                  // Show: выводимые выражения (одно или несколько через запятую)
    std::size_t pos = 0;          // позиция в тексте: Variable, VarDecl, ListDecl (для ошибок имён)

    static NodePtr make(Kind k) {
        auto n = std::make_unique<Node>();
        n->kind = k;
        return n;
    }
    static NodePtr number(double v) {
        auto n = make(Kind::Number);
        n->value = v;
        return n;
    }
    static NodePtr string(std::string s) {
        auto n = make(Kind::String);
        n->text = std::move(s);
        return n;
    }
    static NodePtr variable(std::string name, std::size_t pos) {
        auto n = make(Kind::Variable);
        n->text = std::move(name);
        n->pos = pos;
        return n;
    }
    static NodePtr unary(std::string op, NodePtr operand) {
        auto n = make(Kind::Unary);
        n->op = std::move(op);
        n->left = std::move(operand);
        return n;
    }
    static NodePtr binary(std::string op, NodePtr l, NodePtr r) {
        auto n = make(Kind::Binary);
        n->op = std::move(op);
        n->left = std::move(l);
        n->right = std::move(r);
        return n;
    }
    static NodePtr varDecl(std::string name, NodePtr value, std::size_t pos) {
        auto n = make(Kind::VarDecl);
        n->text = std::move(name);
        n->left = std::move(value);
        n->pos = pos;
        return n;
    }
    static NodePtr show(std::vector<NodePtr> values) {
        auto n = make(Kind::Show);
        n->body = std::move(values);
        return n;
    }
    static NodePtr input(std::size_t pos) {
        auto n = make(Kind::Input);
        n->pos = pos;
        return n;
    }
    static NodePtr runFile(NodePtr fileName) {
        auto n = make(Kind::RunFile);
        n->left = std::move(fileName);
        return n;
    }
    static NodePtr listDecl(std::string name, std::size_t pos) {
        auto n = make(Kind::ListDecl);
        n->text = std::move(name);
        n->pos = pos;
        return n;
    }
    static NodePtr whileNode(NodePtr cond, NodePtr body) {
        auto n = make(Kind::While);
        n->left = std::move(cond);
        n->right = std::move(body);
        return n;
    }
    static NodePtr index(NodePtr list, NodePtr idx) {
        auto n = make(Kind::Index);
        n->left = std::move(list);
        n->right = std::move(idx);
        return n;
    }
    static NodePtr ifNode(NodePtr cond, NodePtr thenBranch, NodePtr elseBranch) {
        auto n = make(Kind::If);
        n->left = std::move(cond);
        n->right = std::move(thenBranch);
        n->elseBranch = std::move(elseBranch);
        return n;
    }
    static NodePtr block() { return make(Kind::Block); }
    static NodePtr assign(std::string name, NodePtr value, std::size_t pos) {   // x = значение (left)
        auto n = make(Kind::Assign);
        n->text = std::move(name);
        n->left = std::move(value);
        n->pos = pos;
        return n;
    }
    static NodePtr logical(std::string op, NodePtr l, NodePtr r) {              // "and" / "or"
        auto n = make(Kind::Logical);
        n->op = std::move(op);
        n->left = std::move(l);
        n->right = std::move(r);
        return n;
    }
    static NodePtr loopJump(Kind k, std::size_t pos) {                          // Break или Continue
        auto n = make(k);
        n->pos = pos;
        return n;
    }
    static NodePtr funcDecl(std::string name, std::vector<std::string> params, NodePtr body, std::size_t pos) {
        auto n = make(Kind::FuncDecl);
        n->text = std::move(name);
        n->params = std::move(params);
        n->left = std::move(body);
        n->pos = pos;
        return n;
    }
    static NodePtr returnNode(NodePtr value, std::size_t pos) {
        auto n = make(Kind::Return);
        n->left = std::move(value);
        n->pos = pos;
        return n;
    }
    static NodePtr call(std::string name, std::vector<NodePtr> args, std::size_t pos) {
        auto n = make(Kind::Call);
        n->text = std::move(name);
        n->body = std::move(args);
        n->pos = pos;
        return n;
    }
};

// Встроенные функции: вызываются как обычные (имя(арг)), но выполняются самой машиной.
bool isBuiltinFunc(const std::string& name) { return name == "length"; }

// Команда (не выражение): после неё на стеке ничего не остаётся.
bool isStatementKind(const Node& n) {
    return n.kind == Node::Kind::VarDecl || n.kind == Node::Kind::ListDecl ||
           n.kind == Node::Kind::Show || n.kind == Node::Kind::RunFile ||
           n.kind == Node::Kind::If || n.kind == Node::Kind::While || n.kind == Node::Kind::Block ||
           n.kind == Node::Kind::FuncDecl || n.kind == Node::Kind::Return ||
           n.kind == Node::Kind::Assign || n.kind == Node::Kind::Break || n.kind == Node::Kind::Continue ||
           (n.kind == Node::Kind::Call && n.asStatement);      // вызов-команда не оставляет значения на стеке
}

class Parser {
public:
    Parser(std::vector<Token> tokens, const std::string& source)
        : toks_(std::move(tokens)), src_(source) {}

    // Вся программа — это блок команд верхнего уровня.
    NodePtr parse() {
        NodePtr program = Node::block();
        parseStatements(*program, /*inBlock=*/false);
        return program;
    }

private:
    const Token& cur() const { return toks_[i_]; }

    // Если текущий токен — один из перечисленных, съесть его и вернуть true.
    bool match(std::initializer_list<TokenType> types, TokenType* matched = nullptr) {
        for (TokenType t : types) {
            if (cur().type == t) {
                if (matched) *matched = t;
                ++i_;
                return true;
            }
        }
        return false;
    }

    [[noreturn]] void fail(const std::string& msg) const {
        throw CompileError(msg, cur().pos, src_);
    }

    void skipNewlines() { while (cur().type == TokenType::Newline) ++i_; }
    void skipSeparators() {
        while (cur().type == TokenType::Newline || cur().type == TokenType::Semicolon) ++i_;
    }

    static const char* symbol(TokenType t) {
        switch (t) {
            case TokenType::Plus: return "+";
            case TokenType::Minus: return "-";
            case TokenType::Star: return "*";
            case TokenType::Slash: return "/";
            case TokenType::Percent: return "%";
            case TokenType::Power: return "**";
            case TokenType::Eq: return "==";
            case TokenType::Ne: return "!=";
            case TokenType::Lt: return "<";
            case TokenType::Gt: return ">";
            case TokenType::Le: return "<=";
            case TokenType::Ge: return ">=";
            default: return "?";
        }
    }

    // Команды подряд — до конца текста (inBlock = false) или до '}' (inBlock = true).
    void parseStatements(Node& block, bool inBlock) {
        while (true) {
            skipSeparators();
            TokenType t = cur().type;
            if (t == TokenType::End) {
                if (inBlock) fail("Не хватает закрывающей скобки '}'");
                return;
            }
            if (t == TokenType::RBrace) {
                if (inBlock) return;
                fail("Лишняя закрывающая скобка '}'");
            }
            block.body.push_back(statement());

            // После команды должен идти разделитель, конец текста или '}'.
            t = cur().type;
            bool ok = t == TokenType::Newline || t == TokenType::Semicolon ||
                      t == TokenType::End || (inBlock && t == TokenType::RBrace);
            if (!ok) fail("Лишние символы после конца команды");
        }
    }

    // '{' команды '}'
    NodePtr block(const std::string& errorIfMissing) {
        if (!match({TokenType::LBrace})) fail(errorIfMissing);
        NodePtr b = Node::block();
        parseStatements(*b, /*inBlock=*/true);
        match({TokenType::RBrace});                  // parseStatements остановился на '}'
        return b;
    }

    NodePtr statement() {
        // Присваивание: IDENT '=' выражение (var не нужен). '==' лексер даёт отдельным токеном Eq.
        if (cur().type == TokenType::Ident && toks_[i_ + 1].type == TokenType::Assign) {
            std::string name = cur().text;
            std::size_t namePos = cur().pos;
            i_ += 2;                                       // имя и '='
            return Node::assign(std::move(name), expr(), namePos);
        }
        if (cur().type == TokenType::KwBreak || cur().type == TokenType::KwContinue) {
            Node::Kind k = cur().type == TokenType::KwBreak ? Node::Kind::Break : Node::Kind::Continue;
            std::size_t pos = cur().pos;
            ++i_;
            return Node::loopJump(k, pos);
        }
        if (match({TokenType::KwVar})) {
            if (cur().type != TokenType::Ident) fail("Ожидалось имя переменной после 'var'");
            std::string name = cur().text;
            std::size_t namePos = cur().pos;
            ++i_;
            if (!match({TokenType::Assign})) fail("Ожидался знак '=' после имени переменной");
            return Node::varDecl(std::move(name), expr(), namePos);
        }
        if (match({TokenType::KwList})) {                  // list имя = a, b, c
            if (cur().type != TokenType::Ident) fail("Ожидалось имя списка после 'list'");
            NodePtr decl = Node::listDecl(cur().text, cur().pos);
            ++i_;
            if (!match({TokenType::Assign})) fail("Ожидался знак '=' после имени списка");
            do {
                skipNewlines();                            // элементы можно переносить на новую строку после запятой
                decl->body.push_back(expr());
            } while (match({TokenType::Comma}));
            return decl;
        }
        if (match({TokenType::KwShow})) {
            if (!match({TokenType::LParen})) fail("Ожидалась '(' после 'show'");
            std::vector<NodePtr> values;                   // show (a, b, c) — выводятся подряд
            do {
                values.push_back(expr());
            } while (match({TokenType::Comma}));
            if (!match({TokenType::RParen})) fail("Ожидалась закрывающая скобка ')'");
            return Node::show(std::move(values));
        }
        if (match({TokenType::KwRunF})) {                  // runF "файл"  или  runF имя_переменной
            TokenType t = cur().type;
            if (t == TokenType::End || t == TokenType::Newline ||
                t == TokenType::Semicolon || t == TokenType::RBrace) {
                fail("После 'runF' нужно указать имя файла в кавычках: runF \"имя.txt\"");
            }
            return Node::runFile(expr());
        }
        if (match({TokenType::KwFunc})) return funcStatement();
        if (cur().type == TokenType::KwReturn) return returnStatement();
        if (match({TokenType::KwIf})) return ifStatement();
        if (match({TokenType::KwWhile})) return whileStatement();
        if (cur().type == TokenType::KwElse) fail("'else' без соответствующего 'if'");
        if (cur().type == TokenType::LBrace) return block("Ожидалась '{'");
        NodePtr e = expr();
        // sum(5, 7) как отдельная команда; встроенная length всегда даёт значение — это обычное выражение
        if (e->kind == Node::Kind::Call && !isBuiltinFunc(e->text)) e->asStatement = true;
        return e;
    }

    // 'func' уже прочитан.  func имя (параметры) { тело }
    NodePtr funcStatement() {
        if (cur().type != TokenType::Ident) fail("Ожидалось имя функции после 'func'");
        std::string name = cur().text;
        std::size_t namePos = cur().pos;
        if (isBuiltinFunc(name)) fail("Имя '" + name + "' занято встроенной функцией, выберите другое");
        ++i_;
        if (!match({TokenType::LParen})) {
            fail("Ожидалась '(' после имени функции, например: func " + name + " (x, y) { ... }");
        }
        std::vector<std::string> params;
        if (cur().type != TokenType::RParen) {
            do {
                if (cur().type != TokenType::Ident) fail("Ожидалось имя параметра");
                if (std::find(params.begin(), params.end(), cur().text) != params.end()) {
                    fail("Параметр '" + cur().text + "' указан дважды");
                }
                params.push_back(cur().text);
                ++i_;
            } while (match({TokenType::Comma}));
        }
        if (!match({TokenType::RParen})) fail("Ожидалась ')' после списка параметров");
        skipNewlines();
        NodePtr body = block("Ожидалась '{' перед телом функции");
        return Node::funcDecl(std::move(name), std::move(params), std::move(body), namePos);
    }

    // return  или  return выражение  (значение не обязательно)
    NodePtr returnStatement() {
        std::size_t pos = cur().pos;
        ++i_;                                              // слово 'return'
        TokenType t = cur().type;
        NodePtr value;
        if (!(t == TokenType::End || t == TokenType::Newline ||
              t == TokenType::Semicolon || t == TokenType::RBrace)) {
            value = expr();
        }
        return Node::returnNode(std::move(value), pos);
    }

    // Условие для if / while: обычное выражение, скобки вокруг него не обязательны:
    //   if (x > y) { ... }     while (x < 4) { ... }     while x < 4 { ... }
    NodePtr condition(const char* word) {
        TokenType t = cur().type;
        if (t == TokenType::LBrace || t == TokenType::End ||
            t == TokenType::Newline || t == TokenType::Semicolon) {
            fail(std::string("После '") + word + "' нужно условие, например: " + word + " (x < 10) { ... }");
        }
        return expr();
    }

    // 'while' уже прочитан.  while условие { ... }
    // Тело выполняется, пока условие истинно (не равно 0).
    NodePtr whileStatement() {
        NodePtr cond = condition("while");
        skipNewlines();
        NodePtr body = block("Ожидалась '{' после условия while");
        return Node::whileNode(std::move(cond), std::move(body));
    }

    // 'if' уже прочитан.  if (условие) { ... } [else { ... } | else if ...]
    NodePtr ifStatement() {
        NodePtr cond = condition("if");
        skipNewlines();
        NodePtr thenBranch = block("Ожидалась '{' после условия if");

        // 'else' может стоять и на следующей строке после '}'. Если его нет —
        // возвращаемся назад, чтобы перевод строки остался разделителем команд.
        NodePtr elseBranch;
        std::size_t save = i_;
        skipNewlines();
        if (match({TokenType::KwElse})) {
            skipNewlines();
            if (match({TokenType::KwIf})) elseBranch = ifStatement();      // else if
            else elseBranch = block("Ожидалась '{' или 'if' после 'else'");
        } else {
            i_ = save;
        }
        return Node::ifNode(std::move(cond), std::move(thenBranch), std::move(elseBranch));
    }

    // Выражение — самый низкий приоритет у 'or', выше 'and', выше 'not', затем сравнения.
    NodePtr expr() {
        NodePtr node = andExpr();
        while (match({TokenType::KwOr})) {
            node = Node::logical("or", std::move(node), andExpr());     // левая ассоциативность
        }
        return node;
    }

    NodePtr andExpr() {
        NodePtr node = notExpr();
        while (match({TokenType::KwAnd})) {
            node = Node::logical("and", std::move(node), notExpr());
        }
        return node;
    }

    NodePtr notExpr() {
        if (match({TokenType::KwNot})) return Node::unary("not", notExpr());
        return comparison();
    }

    // Сравнение; одно сравнение на выражение.
    NodePtr comparison() {
        NodePtr node = sum();
        TokenType t;
        if (match({TokenType::Eq, TokenType::Ne, TokenType::Lt, TokenType::Gt,
                   TokenType::Le, TokenType::Ge}, &t)) {
            node = Node::binary(symbol(t), std::move(node), sum());
            TokenType n = cur().type;
            if (n == TokenType::Eq || n == TokenType::Ne || n == TokenType::Lt ||
                n == TokenType::Gt || n == TokenType::Le || n == TokenType::Ge) {
                fail("Цепочки сравнений (a < b < c) не поддерживаются: напишите a < b and b < c");
            }
        }
        return node;
    }

    NodePtr sum() {
        NodePtr node = term();
        TokenType t;
        while (match({TokenType::Plus, TokenType::Minus}, &t)) {
            node = Node::binary(symbol(t), std::move(node), term());   // левая ассоциативность
        }
        return node;
    }

    NodePtr term() {
        NodePtr node = unary();
        TokenType t;
        while (match({TokenType::Star, TokenType::Slash, TokenType::Percent}, &t)) {
            node = Node::binary(symbol(t), std::move(node), unary());
        }
        return node;
    }

    NodePtr unary() {
        if (match({TokenType::Minus})) return Node::unary("-", unary());
        return power();
    }

    // Индексация списка: список $ индекс  (y $0, y $i, y $(i + 1)); можно цепочкой.
    NodePtr postfix() {
        NodePtr node = primary();
        while (match({TokenType::Dollar})) {
            TokenType t = cur().type;
            if (t != TokenType::Number && t != TokenType::Ident && t != TokenType::LParen) {
                fail("После '$' ожидался индекс: число, переменная или (выражение)");
            }
            node = Node::index(std::move(node), primary());
        }
        return node;
    }

    NodePtr power() {
        NodePtr base = postfix();
        if (match({TokenType::Power})) {
            // справа unary(), а не power(): 2**3**2 = 2**(3**2) и работает 2**-1
            return Node::binary("**", std::move(base), unary());
        }
        return base;
    }

    // Вызов функции: имя уже прочитано, текущий токен — '('.  имя(арг1, арг2, ...)
    NodePtr callArgs(std::string name, std::size_t pos) {
        ++i_;                                              // '('
        std::vector<NodePtr> args;
        if (cur().type != TokenType::RParen) {
            do {
                args.push_back(expr());
            } while (match({TokenType::Comma}));
        }
        if (!match({TokenType::RParen})) fail("Ожидалась ')' после аргументов вызова функции");
        return Node::call(std::move(name), std::move(args), pos);
    }

    NodePtr primary() {
        const Token& t = cur();
        if (t.type == TokenType::Number) {
            ++i_;
            return Node::number(t.value);
        }
        if (t.type == TokenType::String) {
            ++i_;
            return Node::string(t.text);
        }
        if (t.type == TokenType::KwInput) {                // input — прочитать строку от пользователя
            ++i_;
            return Node::input(t.pos);
        }
        if (t.type == TokenType::Ident) {
            ++i_;
            if (cur().type == TokenType::LParen) return callArgs(t.text, t.pos);   // имя(...) — вызов
            return Node::variable(t.text, t.pos);
        }
        if (match({TokenType::LParen})) {
            NodePtr node = expr();
            if (!match({TokenType::RParen})) fail("Ожидалась закрывающая скобка ')'");
            return node;
        }
        if (t.type == TokenType::End) fail("Неожиданный конец команды");
        fail("Ожидалось число, строка, переменная или '('");
    }

    std::vector<Token> toks_;
    const std::string& src_;
    std::size_t i_ = 0;
};

// ════════════════════════════════════════════════════════════════════
//  ЭТАП 3. СЕМАНТИЧЕСКАЯ ПРОВЕРКА
//  Грамматически «show(q)» верно, но смысла нет, если q не создана.
//  Эту ошибку ловим ДО выполнения, с указанием места в тексте.
//  Идём по программе сверху вниз и запоминаем объявленные имена.
//  (Область видимости программы одна, поэтому переменная из { } считается
//  объявленной и дальше; остаток проверит виртуальная машина. Тело функции
//  проверяется отдельно: у неё свои локальные имена и доступ к глобальным.)
//  Заодно проверяется, что вызываемая функция существует, число аргументов
//  совпадает с числом параметров, а return стоит только внутри функции.
// ════════════════════════════════════════════════════════════════════

// Служебная «запись» в множестве объявленных имён: после runF мы не знаем,
// какие переменные создаст файл, поэтому проверку имён дальше по тексту
// переносим на время выполнения (её делает LOAD в виртуальной машине).
// Имя '*' не может быть именем переменной, конфликта нет.
const std::string kDynamicNames = "*";

// Объявленные имена: имя → true, если это список (list), и false, если обычная
// переменная (var). Одно имя не может быть и тем и другим одновременно.
using Names = std::map<std::string, bool>;

// Известные функции: имя → число параметров. kAnyArity — функция объявлена
// несколько раз с разным числом параметров; тогда число аргументов проверит VM.
using Funcs = std::map<std::string, std::size_t>;
constexpr std::size_t kAnyArity = static_cast<std::size_t>(-1);

// Что известно про имя: nullopt — не объявлено; true — список; false — обычная переменная.
// Сначала смотрим локальную область, потом глобальные имена (если мы внутри функции).
std::optional<bool> findNameKind(const std::string& name, const Names& declared, const Names* outer) {
    auto local = declared.find(name);
    if (local != declared.end()) return local->second;
    if (outer) {
        auto global = outer->find(name);
        if (global != outer->end()) return global->second;
    }
    return std::nullopt;
}

struct CheckContext {
    const std::string& src;
    Funcs funcs;         // функции из памяти + все func из программы (даже объявленные ниже вызова)
    Names globals;       // все глобальные имена программы: функции читают их при вызове
};

// Вызывает f для каждого дочернего узла.
template <class F>
void forEachChild(const Node& n, F&& f) {
    if (n.left) f(*n.left);
    if (n.right) f(*n.right);
    if (n.elseBranch) f(*n.elseBranch);
    for (const NodePtr& c : n.body) f(*c);
}

// Собирает все func программы (в том числе вложенные): имя → число параметров.
void collectFuncs(const Node& n, Funcs& funcs) {
    if (n.kind == Node::Kind::FuncDecl) {
        auto it = funcs.find(n.text);
        if (it == funcs.end()) funcs[n.text] = n.params.size();
        else if (it->second != n.params.size()) it->second = kAnyArity;
    }
    forEachChild(n, [&](const Node& c) { collectFuncs(c, funcs); });
}

// Собирает глобальные имена (var / list вне функций) и признак runF.
void collectGlobals(const Node& n, Names& globals, bool inFunc) {
    switch (n.kind) {
        case Node::Kind::VarDecl:
            if (!inFunc && !globals.count(n.text)) globals[n.text] = false;
            break;
        case Node::Kind::ListDecl:
            if (!inFunc && !globals.count(n.text)) globals[n.text] = true;
            break;
        case Node::Kind::RunFile:
            globals[kDynamicNames] = false;
            break;
        default:
            break;
    }
    bool childInFunc = inFunc || n.kind == Node::Kind::FuncDecl;
    forEachChild(n, [&](const Node& c) { collectGlobals(c, globals, childInFunc); });
}

// declared — имена текущей области (программы или тела функции);
// outer — глобальные имена, видимые из функции (nullptr, если мы не внутри функции).
void checkNames(const Node& n, Names& declared, const Names* outer, const CheckContext& ctx) {
    const std::string& src = ctx.src;
    auto dynamicNames = [&]() {
        return declared.count(kDynamicNames) > 0 || (outer && outer->count(kDynamicNames) > 0);
    };

    switch (n.kind) {
        case Node::Kind::Number:
        case Node::Kind::String:
        case Node::Kind::Input:
            return;
        case Node::Kind::Show:
            for (const NodePtr& a : n.body) checkNames(*a, declared, outer, ctx);
            return;
        case Node::Kind::Variable:
            if (!declared.count(n.text) && !(outer && outer->count(n.text)) && !dynamicNames()) {
                throw CompileError("Переменная '" + n.text + "' не объявлена (создайте её: var "
                                   + n.text + " = ...)", n.pos, src);
            }
            return;
        case Node::Kind::Unary:
            checkNames(*n.left, declared, outer, ctx);
            return;
        case Node::Kind::VarDecl: {
            checkNames(*n.left, declared, outer, ctx);   // сначала значение: var x = x + 1 требует старого x
            // var хранит число или строку; список в неё положить нельзя (var y = x, где x — список).
            if (n.left->kind == Node::Kind::Variable) {
                bool sourceIsList = false;
                auto local = declared.find(n.left->text);
                if (local != declared.end()) {
                    sourceIsList = local->second;
                } else if (outer) {
                    auto global = outer->find(n.left->text);
                    if (global != outer->end()) sourceIsList = global->second;
                }
                if (sourceIsList) {
                    throw CompileError("Значение — список, а переменная хранит только число или строку. "
                                       "Для списка используйте: list " + n.text + " = ...", n.left->pos, src);
                }
            }
            auto it = declared.find(n.text);
            if (it != declared.end() && it->second) {
                throw CompileError("Имя '" + n.text + "' уже занято списком: переменную и список "
                                   "нельзя называть одинаково", n.pos, src);
            }
            declared[n.text] = false;
            return;
        }
        case Node::Kind::ListDecl: {
            for (const NodePtr& e : n.body) checkNames(*e, declared, outer, ctx);
            auto it = declared.find(n.text);
            if (it != declared.end() && !it->second) {
                throw CompileError("Имя '" + n.text + "' уже занято переменной: список и переменную "
                                   "нельзя называть одинаково", n.pos, src);
            }
            declared[n.text] = true;
            return;
        }
        case Node::Kind::RunFile:
            checkNames(*n.left, declared, outer, ctx);
            declared[kDynamicNames] = false;          // что создаст файл — узнаем при выполнении
            return;
        case Node::Kind::Index:
        case Node::Kind::Binary:
        case Node::Kind::Logical:
            checkNames(*n.left, declared, outer, ctx);
            checkNames(*n.right, declared, outer, ctx);
            return;
        case Node::Kind::Break:                       // допустимость внутри цикла проверяет checkLoops
        case Node::Kind::Continue:
            return;
        case Node::Kind::Assign: {
            checkNames(*n.left, declared, outer, ctx);   // сначала значение: x = x + 1 читает старый x
            std::optional<bool> kind = findNameKind(n.text, declared, outer);
            if (!kind && !dynamicNames()) {
                throw CompileError("Переменная '" + n.text + "' не объявлена: присваивание меняет уже созданную "
                                   "переменную (создайте её: var " + n.text + " = ...)", n.pos, src);
            }
            if (kind && *kind) {
                throw CompileError("'" + n.text + "' — список, ему нельзя присвоить число или строку. "
                                   "Чтобы заменить список, используйте: list " + n.text + " = ...", n.pos, src);
            }
            if (n.left->kind == Node::Kind::Variable) {   // x = y, где y — список
                std::optional<bool> src2 = findNameKind(n.left->text, declared, outer);
                if (src2 && *src2) {
                    throw CompileError("Значение — список, а переменная хранит только число или строку. "
                                       "Возьмите элемент списка: " + n.text + " = " + n.left->text + " $0",
                                       n.left->pos, src);
                }
            }
            return;
        }
        case Node::Kind::While:                       // условие проверяется раньше тела
            checkNames(*n.left, declared, outer, ctx);
            checkNames(*n.right, declared, outer, ctx);
            return;
        case Node::Kind::If:
            checkNames(*n.left, declared, outer, ctx);
            checkNames(*n.right, declared, outer, ctx);
            if (n.elseBranch) checkNames(*n.elseBranch, declared, outer, ctx);
            return;
        case Node::Kind::Block:
            for (const NodePtr& s : n.body) checkNames(*s, declared, outer, ctx);
            return;
        case Node::Kind::FuncDecl: {
            // Внутри функции своя область: параметры + локальные переменные.
            // Глобальные переменные видны только для чтения (ctx.globals).
            Names local;
            for (const std::string& p : n.params) local[p] = false;
            checkNames(*n.left, local, &ctx.globals, ctx);
            return;
        }
        case Node::Kind::Return:
            if (!outer) {
                throw CompileError("'return' можно писать только внутри функции", n.pos, src);
            }
            if (n.left) checkNames(*n.left, declared, outer, ctx);
            return;
        case Node::Kind::Call: {
            for (const NodePtr& a : n.body) checkNames(*a, declared, outer, ctx);
            if (isBuiltinFunc(n.text)) {                // length(x): ровно один аргумент
                if (n.body.size() != 1) {
                    throw CompileError("Функция '" + n.text + "' принимает ровно 1 аргумент, а передано: " +
                                       std::to_string(n.body.size()), n.pos, src);
                }
                return;
            }
            auto it = ctx.funcs.find(n.text);
            if (it == ctx.funcs.end()) {
                if (!dynamicNames()) {                // после runF функцию мог создать файл
                    throw CompileError("Функция '" + n.text + "' не объявлена (создайте её: func "
                                       + n.text + " (...) { ... })", n.pos, src);
                }
            } else if (it->second != kAnyArity && it->second != n.body.size()) {
                throw CompileError("Функция '" + n.text + "' принимает параметров: " +
                                   std::to_string(it->second) + ", а передано аргументов: " +
                                   std::to_string(n.body.size()), n.pos, src);
            }
            return;
        }
    }
}

// break и continue допустимы только внутри while. Тело функции — новая область:
// цикл вызывающего кода из функции «не виден», поэтому глубина там начинается с нуля.
void checkLoops(const Node& n, int loopDepth, const std::string& src) {
    switch (n.kind) {
        case Node::Kind::Break:
            if (loopDepth == 0) throw CompileError("'break' можно писать только внутри цикла while", n.pos, src);
            return;
        case Node::Kind::Continue:
            if (loopDepth == 0) throw CompileError("'continue' можно писать только внутри цикла while", n.pos, src);
            return;
        case Node::Kind::FuncDecl:
            checkLoops(*n.left, 0, src);
            return;
        case Node::Kind::While:
            checkLoops(*n.left, loopDepth, src);
            checkLoops(*n.right, loopDepth + 1, src);
            return;
        default:
            forEachChild(n, [&](const Node& c) { checkLoops(c, loopDepth, src); });
    }
}

// Проверка всей программы. declared на входе — имена, уже лежащие в памяти;
// known — функции, уже лежащие в памяти (имя → число параметров).
void checkProgram(const Node& root, Names& declared, const Funcs& known, const std::string& src) {
    CheckContext ctx{src, known, declared};
    collectFuncs(root, ctx.funcs);
    collectGlobals(root, ctx.globals, false);
    checkNames(root, declared, nullptr, ctx);
    checkLoops(root, 0, src);
}

// ════════════════════════════════════════════════════════════════════
//  ЭТАП 4. ОПТИМИЗАТОР
//   • свёртка констант:      2 + 3 * 4  →  14,   1 < 2  →  1
//   • удаление мёртвых веток: if (1 > 2) {A} else {B}  →  B
//   • логика с известной левой частью:  0 and X → 0,  1 or X → 1  (X не вычисляется
//     и при выполнении, так что выкинуть его безопасно)
//  Выражения с переменными не сворачиваются: их значения известны
//  только при выполнении.
// ════════════════════════════════════════════════════════════════════

NodePtr optimize(NodePtr node) {
    switch (node->kind) {
        case Node::Kind::Number:
        case Node::Kind::String:
        case Node::Kind::Variable:
        case Node::Kind::Input:
        case Node::Kind::Break:
        case Node::Kind::Continue:
            return node;

        case Node::Kind::VarDecl:
        case Node::Kind::Assign:
        case Node::Kind::RunFile:
        case Node::Kind::FuncDecl:                          // left — тело функции
            node->left = optimize(std::move(node->left));
            return node;

        case Node::Kind::Return:                            // значение может отсутствовать
            if (node->left) node->left = optimize(std::move(node->left));
            return node;

        case Node::Kind::Block:
        case Node::Kind::Show:                              // выводимые выражения
        case Node::Kind::ListDecl:                          // элементы списка оптимизируем как выражения
        case Node::Kind::Call:                              // аргументы вызова — тоже выражения
            for (NodePtr& s : node->body) s = optimize(std::move(s));
            return node;

        case Node::Kind::Index:
            node->left = optimize(std::move(node->left));
            node->right = optimize(std::move(node->right));
            return node;

        case Node::Kind::While:
            node->left = optimize(std::move(node->left));
            if (node->left->kind == Node::Kind::Number && node->left->value == 0) {
                return Node::block();                              // условие всегда ложно — цикл не выполнится
            }
            node->right = optimize(std::move(node->right));
            return node;

        case Node::Kind::If: {
            node->left = optimize(std::move(node->left));
            if (node->left->kind == Node::Kind::Number) {          // условие известно заранее
                if (node->left->value != 0) return optimize(std::move(node->right));
                if (node->elseBranch) return optimize(std::move(node->elseBranch));
                return Node::block();                              // ветки нет — команды нет
            }
            node->right = optimize(std::move(node->right));
            if (node->elseBranch) node->elseBranch = optimize(std::move(node->elseBranch));
            return node;
        }

        case Node::Kind::Logical: {
            node->left = optimize(std::move(node->left));
            node->right = optimize(std::move(node->right));
            if (node->left->kind == Node::Kind::Number) {
                bool l = node->left->value != 0;
                bool isAnd = node->op == "and";
                if (isAnd && !l) return Node::number(0);           // 0 and X → 0
                if (!isAnd && l) return Node::number(1);           // 1 or X → 1
                if (node->right->kind == Node::Kind::Number) {     // обе части известны
                    return Node::number(node->right->value != 0 ? 1 : 0);
                }
            }
            return node;
        }

        case Node::Kind::Unary: {
            node->left = optimize(std::move(node->left));
            if (node->left->kind == Node::Kind::Number) {
                return Node::number(unaryOps().at(node->op).fn(node->left->value));
            }
            return node;
        }

        case Node::Kind::Binary: {
            node->left = optimize(std::move(node->left));
            node->right = optimize(std::move(node->right));
            if (node->left->kind == Node::Kind::Number && node->right->kind == Node::Kind::Number) {
                double a = node->left->value, b = node->right->value;
                auto cmp = comparisonOps().find(node->op);
                if (cmp != comparisonOps().end()) {
                    return Node::number(compareNumbers(cmp->second, a, b) ? 1 : 0);
                }
                try {
                    return Node::number(binaryOps().at(node->op).fn(a, b));
                } catch (const RuntimeError&) {
                    // не сворачиваем: ошибку покажет виртуальная машина при запуске
                }
            }
            return node;
        }
    }
    return node;
}

// ════════════════════════════════════════════════════════════════════
//  ЭТАП 5. КОДОГЕНЕРАТОР — дерево → список инструкций стековой машины
//  Обход «снизу вверх»: сначала операнды, потом операция.
//     var z = x - y   →   LOAD x, LOAD y, SUB, STORE z
//     show (z)        →   LOAD z, SHOW
//     list x = 5, 6   →   PUSH 5, PUSH 6, MAKE_LIST 2, STORE x
//     show (x $1)     →   LOAD x, PUSH 1, INDEX, SHOW
//
//  Условие превращается в переходы (индексы инструкций):
//     if (c) {A} else {B}           if (c) {A}
//         <c>                           <c>
//         JUMP_IF_FALSE L1              JUMP_IF_FALSE L1
//         <A>                           <A>
//         JUMP L2                   L1:
//     L1: <B>
//     L2:
//
//  Присваивание:  x = x + 1   →   LOAD x, PUSH 1, ADD, ASSIGN x   (x обязана уже существовать)
//
//  Логика (ленивая — правая часть пропускается, если результат уже известен):
//     a and b                       a or b
//         <a>                           <a>
//         JUMP_IF_FALSE Lfalse          JUMP_IF_FALSE Lb
//         <b>                           PUSH 1
//         JUMP_IF_FALSE Lfalse          JUMP Lend
//         PUSH 1                    Lb: <b>
//         JUMP Lend                     JUMP_IF_FALSE Lfalse
//     Lfalse: PUSH 0                    PUSH 1
//     Lend:                             JUMP Lend
//                                   Lfalse: PUSH 0
//                                   Lend:
//     not a   →   <a>, NOT
//
//  break и continue — обычные JUMP: continue ведёт на проверку условия цикла,
//  break — на первую инструкцию после цикла (адрес дописывается, когда цикл закончен).
//
//  Ввод:
//     var x = input        →   DEFER_INPUT x          (ничего не читаем: в x лежит «ждёт ввода»)
//     show ("a", x)        →   PUSH "a", SHOW, LOAD x, SHOW, NEWLINE
//                              (LOAD x впервые читает x: тут и спросим пользователя)
//     show (input)         →   INPUT, SHOW, NEWLINE   (input в выражении читаем сразу)
//
//  Функции (у каждой функции свой байт-код, нумерация инструкций с нуля):
//     func f (a) { return a }   →   DEFINE_FUNC f(a)      (регистрирует функцию в памяти)
//     f(5)                      →   PUSH 5, CALL f 1      (аргументы → на стек, затем вызов)
//     return a                  →   LOAD a, RETURN        (в конце тела — RETURN без значения)
// ════════════════════════════════════════════════════════════════════

struct Instruction {
    OpCode op;
    Value value{};              // для PUSH
    std::string name{};         // для LOAD / STORE / CALL
    std::size_t arg = 0;        // JUMP / JUMP_IF_FALSE: номер инструкции; MAKE_LIST: число элементов;
                                // CALL: число аргументов; RETURN: 1 — со значением, 0 — без значения
    bool discard = false;       // CALL: вызов-команда (результат на стек не кладём)
    std::shared_ptr<const Function> func{};   // DEFINE_FUNC: какую функцию зарегистрировать

    static Instruction simple(OpCode op) { return Instruction{op, {}, {}, 0}; }
    static Instruction push(Value v) { return Instruction{OpCode::Push, std::move(v), {}, 0}; }
    static Instruction named(OpCode op, std::string n) { return Instruction{op, {}, std::move(n), 0}; }
    static Instruction withArg(OpCode op, std::size_t arg) { return Instruction{op, {}, {}, arg}; }

    std::string str() const;    // определена ниже, после Function
};

// Функция, созданная командой func: имя, параметры и собственный байт-код.
struct Function {
    std::string name;
    std::vector<std::string> params;
    std::vector<Instruction> code;      // последняя инструкция — всегда RETURN
};

inline std::string Instruction::str() const {
    switch (op) {
        case OpCode::Push:  return "PUSH " + repr(value);
        case OpCode::Load:
        case OpCode::Store:
        case OpCode::StoreList:
        case OpCode::Assign: return opName(op) + " " + name;
        case OpCode::Jump:
        case OpCode::JumpIfFalse:
        case OpCode::MakeList: return opName(op) + " " + std::to_string(arg);
        case OpCode::DefineFunc: {
            std::string s = "DEFINE_FUNC " + (func ? func->name : name) + "(";
            if (func) {
                for (std::size_t k = 0; k < func->params.size(); ++k) s += (k ? ", " : "") + func->params[k];
            }
            return s + ")";
        }
        case OpCode::Call:
            return "CALL " + name + " " + std::to_string(arg) + (discard ? "  (результат не нужен)" : "");
        case OpCode::Return:
            return arg ? "RETURN  (значение со стека)" : "RETURN  (без значения)";
        case OpCode::DeferAssign:
            return "DEFER_ASSIGN " + name + "  (в x положить «ждёт ввода», спросим при первом использовании)";
        case OpCode::DeferInput:
            return "DEFER_INPUT " + name + "  (запомнить «ждёт ввода», спросим при первом использовании)";
        default:            return opName(op);
    }
}

class CodeGenerator {
public:
    std::vector<Instruction> generate(const Node& root) {
        code_.clear();
        visitStatement(root);
        code_.push_back(Instruction::simple(OpCode::Halt));
        return code_;
    }

    // Байт-код тела функции. Всегда заканчивается RETURN без значения: туда мы
    // попадаем, если выполнение дошло до конца тела, не встретив 'return'.
    std::vector<Instruction> generateBody(const Node& body) {
        code_.clear();
        visitStatement(body);
        code_.push_back(Instruction::withArg(OpCode::Return, 0));
        return code_;
    }

private:
    // Команда: если это «голое» выражение, его значение снимаем со стека (POP).
    void visitStatement(const Node& n) {
        visit(n);
        if (!isStatementKind(n)) code_.push_back(Instruction::simple(OpCode::Pop));
    }

    void visit(const Node& n) {
        switch (n.kind) {
            case Node::Kind::Number:
                code_.push_back(Instruction::push(Value::number(n.value)));
                break;
            case Node::Kind::String:
                code_.push_back(Instruction::push(Value::text(n.text)));
                break;
            case Node::Kind::Variable:
                code_.push_back(Instruction::named(OpCode::Load, n.text));
                break;
            case Node::Kind::VarDecl:
                if (n.left->kind == Node::Kind::Input) {          // var x = input — читать будем позже,
                    code_.push_back(Instruction::named(OpCode::DeferInput, n.text));   // при первом использовании x
                    break;
                }
                visit(*n.left);                                   // значение → на стек
                code_.push_back(Instruction::named(OpCode::Store, n.text));
                break;
            case Node::Kind::Assign:
                if (n.left->kind == Node::Kind::Input) {          // x = input — тоже отложенный ввод
                    code_.push_back(Instruction::named(OpCode::DeferAssign, n.text));
                    break;
                }
                visit(*n.left);
                code_.push_back(Instruction::named(OpCode::Assign, n.text));
                break;
            case Node::Kind::Logical: {
                std::vector<std::size_t> toFalse, toEnd;           // адреса переходов, которые допишем позже
                auto jumpIfFalse = [&]() {
                    toFalse.push_back(code_.size());
                    code_.push_back(Instruction::withArg(OpCode::JumpIfFalse, 0));
                };
                auto jumpToEnd = [&]() {
                    toEnd.push_back(code_.size());
                    code_.push_back(Instruction::withArg(OpCode::Jump, 0));
                };
                visit(*n.left);
                if (n.op == "and") {
                    jumpIfFalse();                                // левая ложна — правую не считаем
                    visit(*n.right);
                    jumpIfFalse();
                    code_.push_back(Instruction::push(Value::number(1)));
                    jumpToEnd();
                } else {
                    std::size_t firstJump = code_.size();
                    code_.push_back(Instruction::withArg(OpCode::JumpIfFalse, 0));
                    code_.push_back(Instruction::push(Value::number(1)));   // левая истинна — правую не считаем
                    jumpToEnd();
                    code_[firstJump].arg = code_.size();          // левая ложна — смотрим правую
                    visit(*n.right);
                    jumpIfFalse();
                    code_.push_back(Instruction::push(Value::number(1)));
                    jumpToEnd();
                }
                for (std::size_t j : toFalse) code_[j].arg = code_.size();
                code_.push_back(Instruction::push(Value::number(0)));
                for (std::size_t j : toEnd) code_[j].arg = code_.size();
                break;
            }
            case Node::Kind::Break:
                if (loops_.empty()) throw RuntimeError("'break' вне цикла");
                loops_.back().breakJumps.push_back(code_.size());
                code_.push_back(Instruction::withArg(OpCode::Jump, 0));   // адрес выхода допишет цикл
                break;
            case Node::Kind::Continue:
                if (loops_.empty()) throw RuntimeError("'continue' вне цикла");
                code_.push_back(Instruction::withArg(OpCode::Jump, loops_.back().start));
                break;
            case Node::Kind::While: {
                std::size_t loopStart = code_.size();             // сюда возвращаемся после каждого прохода
                visit(*n.left);                                   // условие
                std::size_t exitJump = code_.size();
                code_.push_back(Instruction::withArg(OpCode::JumpIfFalse, 0));   // ложь → выйти из цикла
                loops_.push_back({loopStart, {}});                // для break / continue внутри тела
                visitStatement(*n.right);                         // тело
                code_.push_back(Instruction::withArg(OpCode::Jump, loopStart));  // назад: проверить условие
                std::size_t loopEnd = code_.size();               // адрес выхода — сразу за циклом
                code_[exitJump].arg = loopEnd;
                for (std::size_t j : loops_.back().breakJumps) code_[j].arg = loopEnd;
                loops_.pop_back();
                break;
            }
            case Node::Kind::Show:
                // Каждое значение печатается сразу после вычисления (SHOW без перевода строки),
                // а в конце — NEWLINE. Так текст стоит на экране раньше, чем переменная с
                // input потребует ввод: show ("Введите число: ", x).
                for (const NodePtr& a : n.body) {
                    visit(*a);
                    code_.push_back(Instruction::simple(OpCode::Show));
                }
                code_.push_back(Instruction::simple(OpCode::Newline));
                break;
            case Node::Kind::Input:
                code_.push_back(Instruction::simple(OpCode::Input));   // строка от пользователя → на стек
                break;
            case Node::Kind::RunFile:
                visit(*n.left);                                   // имя файла → на стек
                code_.push_back(Instruction::simple(OpCode::RunFile));
                break;
            case Node::Kind::Unary:
                visit(*n.left);
                code_.push_back(Instruction::simple(unaryOps().at(n.op).code));
                break;
            case Node::Kind::Binary: {
                visit(*n.left);
                visit(*n.right);
                auto cmp = comparisonOps().find(n.op);
                code_.push_back(Instruction::simple(
                    cmp != comparisonOps().end() ? cmp->second : binaryOps().at(n.op).code));
                break;
            }
            case Node::Kind::ListDecl:
                for (const NodePtr& e : n.body) visit(*e);        // все элементы → на стек
                code_.push_back(Instruction::withArg(OpCode::MakeList, n.body.size()));
                code_.push_back(Instruction::named(OpCode::StoreList, n.text));
                break;
            case Node::Kind::Index:
                visit(*n.left);                                   // список
                visit(*n.right);                                  // индекс
                code_.push_back(Instruction::simple(OpCode::Index));
                break;
            case Node::Kind::Block:
                for (const NodePtr& s : n.body) visitStatement(*s);
                break;
            case Node::Kind::FuncDecl: {
                auto fn = std::make_shared<Function>();
                fn->name = n.text;
                fn->params = n.params;
                fn->code = CodeGenerator().generateBody(*n.left);   // тело — отдельный байт-код
                Instruction ins = Instruction::simple(OpCode::DefineFunc);
                ins.name = n.text;
                ins.func = std::move(fn);
                code_.push_back(std::move(ins));
                break;
            }
            case Node::Kind::Return:
                if (n.left) visit(*n.left);                       // значение → на стек
                code_.push_back(Instruction::withArg(OpCode::Return, n.left ? 1 : 0));
                break;
            case Node::Kind::Call: {
                for (const NodePtr& a : n.body) visit(*a);        // аргументы → на стек по порядку
                if (isBuiltinFunc(n.text)) {                      // встроенная length: своя инструкция, не CALL
                    code_.push_back(Instruction::simple(OpCode::Length));
                    break;
                }
                Instruction ins = Instruction::withArg(OpCode::Call, n.body.size());
                ins.name = n.text;
                ins.discard = n.asStatement;
                code_.push_back(std::move(ins));
                break;
            }
            case Node::Kind::If: {
                visit(*n.left);                                   // условие
                std::size_t jumpIfFalse = code_.size();
                code_.push_back(Instruction::withArg(OpCode::JumpIfFalse, 0));   // адрес допишем позже
                visitStatement(*n.right);                         // ветка then
                if (n.elseBranch) {
                    std::size_t jumpOver = code_.size();
                    code_.push_back(Instruction::withArg(OpCode::Jump, 0));      // обойти else
                    code_[jumpIfFalse].arg = code_.size();     // ложь → сюда (начало else)
                    visitStatement(*n.elseBranch);
                    code_[jumpOver].arg = code_.size();        // конец всего if
                } else {
                    code_[jumpIfFalse].arg = code_.size();     // конец всего if
                }
                break;
            }
        }
    }

    // Циклы, внутри которых мы сейчас находимся (внутренний — последний).
    struct LoopInfo {
        std::size_t start;                      // адрес проверки условия: туда ведёт continue
        std::vector<std::size_t> breakJumps;    // JUMP-ы от break: их адрес известен только в конце цикла
    };

    std::vector<Instruction> code_;
    std::vector<LoopInfo> loops_;
};

// ════════════════════════════════════════════════════════════════════
//  ЭТАП 6. ВИРТУАЛЬНАЯ МАШИНА — выполняет байт-код на стеке
//  LOAD берёт значение переменной (сначала из локальных переменных текущей
//  функции, потом из глобальных — Environment), STORE записывает (внутри
//  функции — всегда в локальные), SHOW снимает значение со стека и печатает
//  его (без перевода строки; строку заканчивает NEWLINE). Если в переменной
//  лежит Pending (var x = input), то LOAD сначала спрашивает пользователя и
//  заменяет Pending введённым значением. Номер текущей инструкции (pc) обычно растёт на 1, а JUMP /
//  JUMP_IF_FALSE меняют его.
//
//  Вызов функции: CALL снимает аргументы со стека, создаёт кадр (Frame) с
//  локальными переменными и запоминает, куда вернуться (код и pc вызывающего),
//  после чего машина выполняет байт-код функции. RETURN убирает кадр, кладёт
//  результат на стек (если он нужен) и возвращается к вызывающему.
// ════════════════════════════════════════════════════════════════════

class VM {
public:
    using Trace = std::function<void(const Instruction&, const std::vector<Value>&)>;

    // opt и log нужны для runF: вложенный файл выполняется с теми же настройками
    // (и с тем же пошаговым выводом, если он включён).
    VM(std::vector<Instruction> code, Environment& env, const Options& opt, std::ostream* log)
        : code_(std::move(code)), env_(env), opt_(opt), log_(log) {}

    // Возвращает значение последнего «голого» выражения программы
    // (для var, show и if без выражений значения нет — nullopt).
    std::optional<Value> run(const Trace& trace = nullptr) {
        try {
            return execute(trace);
        } catch (const RuntimeError& e) {
            if (frames_.empty()) throw;                // ошибка в основной программе
            throw RuntimeError(std::string(e.what()) + " (в функции '" + frames_.back().fn->name + "')");
        }
    }

private:
    static constexpr std::size_t kMaxDepth = 10000;    // максимум вложенных вызовов (защита от бесконечной рекурсии)

    // Кадр вызова функции.
    struct Frame {
        std::shared_ptr<const Function> fn;            // кто выполняется (держит код живым)
        std::map<std::string, Value> locals;           // параметры и локальные переменные
        const std::vector<Instruction>* retCode = nullptr;   // куда вернуться: код вызывающего ...
        std::size_t retPc = 0;                         // ... и номер следующей инструкции
        bool discard = false;                          // вызов-команда: результат на стек не нужен
    };

    std::optional<Value> execute(const Trace& trace) {
        const std::vector<Instruction>* code = &code_;   // выполняемый сейчас байт-код: основной или функции
        std::size_t pc = 0;
        std::size_t steps = 0;
        while (pc < code->size()) {
            const Instruction& ins = (*code)[pc++];
            std::shared_ptr<const Function> keepAlive;    // код функции должен пережить RETURN до trace
            if (ins.op == OpCode::Halt) break;
            if (++steps > opt_.maxSteps) {                // защита от бесконечного цикла
                throw RuntimeError("Превышен лимит в " + std::to_string(opt_.maxSteps) +
                                   " инструкций: возможно, цикл while или рекурсия никогда не заканчивается "
                                   "(условие всегда истинно?)");
            }
            if ((steps & 0xFFF) == 0) {                   // раз в 4096 инструкций
                if (opt_.cancel && opt_.cancel->load(std::memory_order_relaxed)) {
                    throw RuntimeError("Выполнение прервано");
                }
                if (opt_.onTick) opt_.onTick();
            }

            if (ins.op == OpCode::Push) {
                stack_.push_back(ins.value);
            } else if (ins.op == OpCode::Load) {
                Value* v = lookup(ins.name);
                if (!v) throw RuntimeError("Переменная '" + ins.name + "' не объявлена");
                if (v->type == Value::Type::Pending) {
                    // Первое чтение переменной с input: спрашиваем пользователя сейчас
                    // и запоминаем ответ, чтобы больше не спрашивать.
                    *v = readInput();
                }
                stack_.push_back(*v);
            } else if (ins.op == OpCode::Store || ins.op == OpCode::StoreList || ins.op == OpCode::DeferInput) {
                // STORE — для var (число или строка), STORE_LIST — для list.
                // Имя не может быть одновременно и переменной, и списком.
                // Внутри функции пишем в локальные переменные, иначе — в глобальные.
                bool forList = ins.op == OpCode::StoreList;
                Value v = ins.op == OpCode::DeferInput ? Value::pending() : pop();   // DEFER_INPUT: значения на стеке нет
                if (!forList && v.isList()) {
                    throw RuntimeError("Переменная '" + ins.name + "' хранит только число или строку, "
                                       "а не список. Для списка используйте: list " + ins.name + " = ...");
                }
                auto& scope = frames_.empty() ? env_.vars : frames_.back().locals;
                auto it = scope.find(ins.name);
                if (it != scope.end() && it->second.isList() != forList) {
                    throw RuntimeError("Имя '" + ins.name + "' уже занято " +
                                       (forList ? "переменной: список и переменную"
                                                : "списком: переменную и список") +
                                       " нельзя называть одинаково");
                }
                scope[ins.name] = std::move(v);
            } else if (ins.op == OpCode::Assign || ins.op == OpCode::DeferAssign) {
                // Присваивание существующей переменной: сначала среди локальных переменных функции,
                // потом среди глобальных — поэтому функция может менять глобальную переменную.
                Value v = ins.op == OpCode::DeferAssign ? Value::pending() : pop();
                if (v.isList()) {
                    throw RuntimeError("Переменная '" + ins.name + "' хранит только число или строку, "
                                       "а не список. Для списка используйте: list " + ins.name + " = ...");
                }
                Value* target = lookup(ins.name);
                if (!target) {
                    throw RuntimeError("Переменная '" + ins.name + "' не объявлена: присваивание меняет "
                                       "уже созданную переменную (создайте её: var " + ins.name + " = ...)");
                }
                if (target->isList()) {
                    throw RuntimeError("'" + ins.name + "' — список, ему нельзя присвоить число или строку. "
                                       "Чтобы заменить список, используйте: list " + ins.name + " = ...");
                }
                *target = std::move(v);
            } else if (ins.op == OpCode::Show) {
                // SHOW печатает одно значение (без перевода строки), NEWLINE заканчивает строку:
                // show (a, b, c) — значения идут подряд без разделителей
                *env_.out << display(pop());
            } else if (ins.op == OpCode::Newline) {
                *env_.out << "\n";
            } else if (ins.op == OpCode::Input) {
                stack_.push_back(readInput());
            } else if (ins.op == OpCode::Length) {
                stack_.push_back(Value::number(static_cast<double>(lengthOf(pop()))));
            } else if (ins.op == OpCode::Pop) {
                Value v = pop();
                if (frames_.empty()) lastValue_ = std::move(v);   // результатом программы считаем только верхний уровень
            } else if (ins.op == OpCode::RunFile) {
                Value fileName = pop();
                if (fileName.type != Value::Type::String) {
                    throw RuntimeError("runF ожидает имя файла в кавычках: runF \"имя.txt\"");
                }
                runFileCommand(fileName.str, env_, opt_, log_);
            } else if (ins.op == OpCode::DefineFunc) {
                env_.funcs[ins.func->name] = ins.func;            // функция становится доступной для вызова
            } else if (ins.op == OpCode::Call) {
                auto fit = env_.funcs.find(ins.name);
                if (fit == env_.funcs.end()) {
                    throw RuntimeError("Функция '" + ins.name + "' ещё не определена (объявите её через func "
                                       "до вызова)");
                }
                std::shared_ptr<const Function> fn = fit->second;
                if (fn->params.size() != ins.arg) {
                    throw RuntimeError("Функция '" + ins.name + "' принимает параметров: " +
                                       std::to_string(fn->params.size()) + ", а передано аргументов: " +
                                       std::to_string(ins.arg));
                }
                if (frames_.size() >= kMaxDepth) {
                    throw RuntimeError("Слишком глубокая рекурсия: больше " + std::to_string(kMaxDepth) +
                                       " вложенных вызовов функций");
                }
                std::vector<Value> args(ins.arg);
                for (std::size_t k = ins.arg; k-- > 0;) args[k] = pop();    // со стека — в обратном порядке
                Frame frame;
                frame.fn = fn;
                frame.retCode = code;
                frame.retPc = pc;
                frame.discard = ins.discard;
                for (std::size_t k = 0; k < fn->params.size(); ++k) {
                    frame.locals[fn->params[k]] = std::move(args[k]);
                }
                frames_.push_back(std::move(frame));
                code = &frames_.back().fn->code;                    // переходим в байт-код функции
                pc = 0;
            } else if (ins.op == OpCode::Return) {
                if (frames_.empty()) throw RuntimeError("'return' вне функции");
                bool hasValue = ins.arg != 0;
                if (!hasValue && !frames_.back().discard) {
                    throw RuntimeError("Результат функции используется в выражении, но она ничего не вернула "
                                       "(добавьте return со значением)");
                }
                std::optional<Value> result;
                if (hasValue) result = pop();
                Frame frame = std::move(frames_.back());
                frames_.pop_back();
                code = frame.retCode;                               // возвращаемся к вызывающему
                pc = frame.retPc;
                if (frame.discard) {
                    // вызов-команда: значение на стек не кладём; на верхнем уровне оно
                    // становится результатом программы (как у «голого» выражения)
                    if (result && frames_.empty()) lastValue_ = std::move(result);
                } else {
                    stack_.push_back(std::move(*result));
                }
                keepAlive = std::move(frame.fn);
            } else if (ins.op == OpCode::MakeList) {
                std::vector<Value> items(ins.arg);
                for (std::size_t k = ins.arg; k-- > 0;) items[k] = pop();   // со стека — в обратном порядке
                stack_.push_back(Value::list(std::move(items)));
            } else if (ins.op == OpCode::Index) {
                Value idx = pop();                    // индекс лежит сверху, список под ним
                Value base = pop();
                if (!base.isList()) throw RuntimeError("Оператор '$' применим только к спискам");
                if (!idx.isNumber() || idx.num < 0 || idx.num != std::floor(idx.num)) {
                    throw RuntimeError("Индекс должен быть целым неотрицательным числом (получено "
                                       + display(idx) + ")");
                }
                if (idx.num >= static_cast<double>(base.items.size())) {
                    throw RuntimeError("Индекс " + fmt(idx.num) + " вне границ списка (размер "
                                       + std::to_string(base.items.size()) + ", индексы с 0 до "
                                       + std::to_string(base.items.size() - 1) + ")");
                }
                stack_.push_back(base.items[static_cast<std::size_t>(idx.num)]);
            } else if (ins.op == OpCode::Jump) {
                pc = ins.arg;
            } else if (ins.op == OpCode::JumpIfFalse) {
                Value cond = pop();
                if (!cond.isNumber()) {
                    throw RuntimeError("Условие должно быть числом или результатом сравнения, а не строкой или списком");
                }
                if (cond.num == 0) pc = ins.arg;
            } else if (isComparison(ins.op)) {
                Value right = pop();                  // правый операнд лежит сверху!
                Value left = pop();
                stack_.push_back(Value::number(compareValues(ins.op, left, right) ? 1 : 0));
            } else if (const BinaryOpInfo* b = findBinary(ins.op)) {
                double right = popNumber();
                double left = popNumber();
                stack_.push_back(Value::number(b->fn(left, right)));
            } else if (const UnaryOpInfo* u = findUnary(ins.op)) {
                stack_.push_back(Value::number(u->fn(popNumber())));
            } else {
                throw RuntimeError("Неизвестная инструкция " + opName(ins.op));
            }

            if (trace) trace(ins, stack_);
        }
        return lastValue_;
    }

    // input: читает одну строку из env_.in. Если строка целиком — число (например -5 или 3.14),
    // возвращается число, иначе — строка как есть (без символа конца строки).
    Value readInput() {
        env_.out->flush();
        std::string line;
        if (!std::getline(*env_.in, line)) {
            throw RuntimeError("input: ввод закончился, читать больше нечего");
        }
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::size_t b = line.find_first_not_of(" \t");
        if (b != std::string::npos) {
            std::string t = line.substr(b, line.find_last_not_of(" \t") - b + 1);
            std::size_t k = (t[0] == '-') ? 1 : 0;
            int digits = 0, dots = 0;
            bool ok = k < t.size();
            for (; ok && k < t.size(); ++k) {
                if (std::isdigit(static_cast<unsigned char>(t[k]))) ++digits;
                else if (t[k] == '.') ++dots;
                else ok = false;
            }
            if (ok && digits > 0 && dots <= 1) return Value::number(std::stod(t));
        }
        return Value::text(std::move(line));
    }

    // length: символов в строке (русские буквы считаются по одной), элементов в списке,
    // для числа — символов в его записи (length(100) = 3).
    static std::size_t lengthOf(const Value& v) {
        if (v.isList()) return v.items.size();
        std::string s = display(v);
        std::size_t n = 0;
        for (char c : s) if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
        return n;
    }

    // Ищет переменную: сначала среди локальных переменных текущей функции, потом среди глобальных.
    Value* lookup(const std::string& name) {
        if (!frames_.empty()) {
            auto it = frames_.back().locals.find(name);
            if (it != frames_.back().locals.end()) return &it->second;
        }
        auto it = env_.vars.find(name);
        return it == env_.vars.end() ? nullptr : &it->second;
    }

    Value pop() {
        if (stack_.empty()) throw RuntimeError("Стек пуст");
        Value v = std::move(stack_.back());
        stack_.pop_back();
        return v;
    }
    double popNumber() {
        Value v = pop();
        if (!v.isNumber()) throw RuntimeError("Арифметика и логика применимы только к числам, а не к строкам или спискам");
        return v.num;
    }
    static const BinaryOpInfo* findBinary(OpCode c) {
        for (const auto& kv : binaryOps()) if (kv.second.code == c) return &kv.second;
        return nullptr;
    }
    static const UnaryOpInfo* findUnary(OpCode c) {
        for (const auto& kv : unaryOps()) if (kv.second.code == c) return &kv.second;
        return nullptr;
    }

    std::vector<Instruction> code_;
    Environment& env_;
    const Options& opt_;
    std::ostream* log_;
    std::vector<Value> stack_;
    std::vector<Frame> frames_;                // стек вызовов: пуст, пока выполняется основная программа
    std::optional<Value> lastValue_;
};

// ════════════════════════════════════════════════════════════════════
//  Вывод для обучения
// ════════════════════════════════════════════════════════════════════

// Рисует дерево псевдографикой.
void dumpAst(const Node& n, std::ostream& out, const std::string& tag = "",
             const std::string& prefix = "", bool isLast = true, bool isRoot = true) {
    std::string label;
    std::vector<std::pair<std::string, const Node*>> children;   // (подпись, узел)
    switch (n.kind) {
        case Node::Kind::Number:   label = "Number(" + fmt(n.value) + ")"; break;
        case Node::Kind::String:   label = "String(\"" + n.text + "\")"; break;
        case Node::Kind::Variable: label = "Variable(" + n.text + ")"; break;
        case Node::Kind::VarDecl:  label = "VarDecl(" + n.text + ")"; children = {{"", n.left.get()}}; break;
        case Node::Kind::Show:
            label = "Show";
            for (const NodePtr& a : n.body) children.push_back({"", a.get()});
            break;
        case Node::Kind::Input:    label = "Input"; break;
        case Node::Kind::Assign:   label = "Assign(" + n.text + ")"; children = {{"", n.left.get()}}; break;
        case Node::Kind::Logical:  label = "Logical(" + n.op + ")";
                                   children = {{"", n.left.get()}, {"", n.right.get()}}; break;
        case Node::Kind::Break:    label = "Break"; break;
        case Node::Kind::Continue: label = "Continue"; break;
        case Node::Kind::RunFile:  label = "RunFile"; children = {{"файл: ", n.left.get()}}; break;
        case Node::Kind::FuncDecl: {
            label = "FuncDecl(" + n.text + " (";
            for (std::size_t k = 0; k < n.params.size(); ++k) label += (k ? ", " : "") + n.params[k];
            label += "))";
            children = {{"тело: ", n.left.get()}};
            break;
        }
        case Node::Kind::Return:
            label = "Return";
            if (n.left) children = {{"", n.left.get()}};
            break;
        case Node::Kind::Call:
            label = "Call(" + n.text + ")";
            for (const NodePtr& a : n.body) children.push_back({"арг: ", a.get()});
            break;
        case Node::Kind::While:
            label = "While";
            children = {{"условие: ", n.left.get()}, {"тело: ", n.right.get()}};
            break;
        case Node::Kind::ListDecl:
            label = "ListDecl(" + n.text + ")";
            for (const NodePtr& e : n.body) children.push_back({"", e.get()});
            break;
        case Node::Kind::Index:
            label = "Index";
            children = {{"список: ", n.left.get()}, {"индекс: ", n.right.get()}};
            break;
        case Node::Kind::Unary:    label = "UnaryOp(" + n.op + ")"; children = {{"", n.left.get()}}; break;
        case Node::Kind::Binary:   label = "BinaryOp(" + n.op + ")";
                                   children = {{"", n.left.get()}, {"", n.right.get()}}; break;
        case Node::Kind::If:
            label = "If";
            children = {{"условие: ", n.left.get()}, {"then: ", n.right.get()}};
            if (n.elseBranch) children.push_back({"else: ", n.elseBranch.get()});
            break;
        case Node::Kind::Block:
            label = isRoot ? "Program" : "Block";
            for (const NodePtr& s : n.body) children.push_back({"", s.get()});
            break;
    }
    label = tag + label;

    std::string childPrefix;
    if (isRoot) {
        out << "    " << label << "\n";
    } else {
        out << "    " << prefix << (isLast ? "└── " : "├── ") << label << "\n";
        childPrefix = prefix + (isLast ? "    " : "│   ");
    }
    for (std::size_t k = 0; k < children.size(); ++k) {
        dumpAst(*children[k].second, out, children[k].first, childPrefix,
                k + 1 == children.size(), false);
    }
}

// Печатает байт-код с номерами инструкций; тела функций — вложенными блоками.
void dumpCode(const std::vector<Instruction>& code, std::ostream& out, const std::string& indent) {
    for (std::size_t n = 0; n < code.size(); ++n) {
        out << indent << (n < 10 ? "0" : "") << n << "  " << code[n].str() << "\n";
        if (code[n].op == OpCode::DefineFunc && code[n].func) {
            out << indent << "    ┌─ тело функции " << code[n].func->name
                << " (свой байт-код, нумерация с 0):\n";
            dumpCode(code[n].func->code, out, indent + "    │ ");
            out << indent << "    └─\n";
        }
    }
}

// Главный конвейер. Если log != nullptr — печатает каждый этап в этот поток.
// `source` — программа целиком (одна или несколько команд).
std::optional<Value> pipeline(const std::string& source, Environment& env,
                              const Options& opt, std::ostream* log) {
    if (log) *log << "\n▶ Исходный код:\n" << source << "\n";

    // 1. Лексер
    std::vector<Token> tokens = Lexer(source).tokenize();
    if (log) {
        *log << "\n[1] ЛЕКСЕР — разбиваем текст на токены:\n    ";
        for (const Token& t : tokens) *log << tokenName(t) << " ";
        *log << "\n";
    }

    // 2. Парсер
    NodePtr ast = Parser(tokens, source).parse();
    if (log) {
        *log << "\n[2] ПАРСЕР — строим дерево (приоритеты, скобки и блоки учтены):\n";
        dumpAst(*ast, *log);
    }

    // 3. Семантика
    Names declared;
    for (const auto& kv : env.vars) declared[kv.first] = kv.second.isList();
    Funcs knownFuncs;
    for (const auto& kv : env.funcs) knownFuncs[kv.first] = kv.second->params.size();
    checkProgram(*ast, declared, knownFuncs, source);
    if (log) {
        *log << "\n[3] СЕМАНТИКА — имена объявлены, переменные и списки не пересекаются. Известны после проверки: ";
        bool first = true;
        for (const auto& kv : declared) {
            if (kv.first == kDynamicNames) continue;
            *log << (first ? "" : ", ") << kv.first << (kv.second ? " (список)" : "");
            first = false;
        }
        if (first) *log << "(нет)";
        if (declared.count(kDynamicNames)) {
            *log << "\n    (в программе есть runF: имена, которые создадут файлы, проверит виртуальная машина)";
        }
        *log << "\n";
    }

    // 4. Оптимизатор
    if (opt.optimize) ast = optimize(std::move(ast));
    if (log) {
        *log << (opt.optimize ? "\n[4] ОПТИМИЗАТОР — свёртка констант и удаление мёртвых веток:\n"
                              : "\n[4] ОПТИМИЗАТОР — отключён, дерево не меняется:\n");
        dumpAst(*ast, *log);
    }

    // 5. Кодогенерация
    std::vector<Instruction> code = CodeGenerator().generate(*ast);
    if (log) {
        *log << "\n[5] КОДОГЕНЕРАТОР — байт-код для стековой машины (число слева — номер инструкции):\n";
        dumpCode(code, *log, "    ");
    }

    // 6. Выполнение
    VM::Trace trace;
    if (log) {
        *log << "\n[6] ВИРТУАЛЬНАЯ МАШИНА — выполнение (состояние стека после шага):\n";
        // Цикл может выполнить тысячи инструкций — показываем только первые 300 шагов.
        auto shown = std::make_shared<std::size_t>(0);
        trace = [log, shown](const Instruction& ins, const std::vector<Value>& stack) {
            constexpr std::size_t kMaxShownSteps = 300;
            if (++*shown > kMaxShownSteps) {
                if (*shown == kMaxShownSteps + 1) {
                    *log << "    ... (дальше шаги не показываются, программа продолжает выполняться)\n";
                }
                return;
            }
            std::string s = ins.str();
            *log << "    " << s << std::string(s.size() < 18 ? 18 - s.size() : 1, ' ') << "→  стек: [";
            for (std::size_t k = 0; k < stack.size(); ++k) *log << (k ? ", " : "") << repr(stack[k]);
            *log << "]\n";
        };
    }
    std::optional<Value> result = VM(std::move(code), env, opt, log).run(trace);
    if (log) {
        if (result) *log << "\n✔ Результат: " << display(*result) << "\n";
        else        *log << "\n✔ Выполнено\n";
    }
    return result;
}

// ════════════════════════════════════════════════════════════════════
//  runF — выполнение программы из файла
//
//  Файл ищется по порядку:
//    1. рядом с файлом, который сейчас выполняется (для вложенных runF);
//    2. в папке самого компилятора;
//    3. в текущей папке.
//  Абсолютный путь используется как есть. Содержимое файла проходит весь
//  конвейер (лексер → … → виртуальная машина) с ОБЩЕЙ памятью: переменные
//  файла видны основной программе, и наоборот.
// ════════════════════════════════════════════════════════════════════

void runFileCommand(const std::string& name, Environment& env, const Options& opt, std::ostream* log) {
    namespace fs = std::filesystem;
    std::error_code ec;

    // Где искать.
    std::vector<fs::path> dirs;
    auto addDir = [&dirs](const fs::path& d) {
        if (!d.empty() && std::find(dirs.begin(), dirs.end(), d) == dirs.end()) dirs.push_back(d);
    };
    if (!env.fileStack.empty()) addDir(fs::path(env.fileStack.back()).parent_path());
    if (!env.exeDir.empty()) addDir(env.exeDir);
    addDir(fs::current_path(ec));

    fs::path given(name);
    fs::path found;
    if (given.is_absolute()) {
        if (fs::is_regular_file(given, ec)) found = given;
    } else {
        for (const fs::path& d : dirs) {
            if (fs::is_regular_file(d / given, ec)) { found = d / given; break; }
        }
    }
    if (found.empty()) {
        std::string where;
        if (given.is_absolute()) where = given.parent_path().string();
        for (const fs::path& d : dirs) {
            if (!given.is_absolute()) where += (where.empty() ? "" : ", ") + d.string();
        }
        throw RuntimeError("Файл '" + name + "' не найден (искал в: " + where + ")");
    }

    // Защита от бесконечной рекурсии: файл не может запускать сам себя (даже через другие файлы).
    fs::path canon = fs::weakly_canonical(found, ec);
    std::string key = (ec ? found : canon).string();
    if (std::find(env.fileStack.begin(), env.fileStack.end(), key) != env.fileStack.end()) {
        throw RuntimeError("Файл '" + name + "' запускает сам себя (цикл runF)");
    }

    std::ifstream in(found, std::ios::binary);
    if (!in) throw RuntimeError("Не удалось открыть файл '" + name + "'");
    std::stringstream buf;
    buf << in.rdbuf();
    std::string text = buf.str();
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);      // BOM из «Блокнота»

    env.fileStack.push_back(key);
    struct PopGuard {                                  // убрать файл из стека при любом выходе
        std::vector<std::string>& stack;
        ~PopGuard() { stack.pop_back(); }
    } guard{env.fileStack};

    if (log) *log << "\n▶ runF: выполняется файл " << name << "\n";
    try {
        pipeline(text, env, opt, log);
    } catch (CompileError& e) {
        if (!e.hasFile()) e.setFile(name);             // ошибка покажет имя файла и строку
        throw;
    } catch (const RuntimeError& e) {
        throw RuntimeError("[файл " + name + "] " + e.what());
    }
}

// ════════════════════════════════════════════════════════════════════
//  ПУБЛИЧНЫЙ ИНТЕРФЕЙС
// ════════════════════════════════════════════════════════════════════

// Обычный запуск программы: ничего не печатает сам (кроме show).
// Возвращает значение последнего «голого» выражения; для var, show и if — nullopt.
std::optional<Value> run(const std::string& source, Environment& env, const Options& opt = {}) {
    return pipeline(source, env, opt, nullptr);
}

// Отдельная функция, включающая пошаговый разбор: лексер, парсер, семантика,
// оптимизатор, байт-код и работа стека. Печатает в `out` (по умолчанию std::cout).
std::optional<Value> explain(const std::string& source, Environment& env,
                             const Options& opt = {}, std::ostream& out = std::cout) {
    return pipeline(source, env, opt, &out);
}

}  // namespace calc

// ════════════════════════════════════════════════════════════════════
//  C-ИНТЕРФЕЙС
//
//  Плоский C API: его можно вызывать из Python (ctypes), Java/Kotlin (JNI), Swift и т. д.
//  Сам интерпретатор синхронный: calc_run() возвращает управление, когда программа закончилась.
//  Поэтому приложение запускает его в рабочем потоке, а остановка и ввод делаются так:
//
//    calc_cancel(h)   — можно вызывать из ДРУГОГО потока: программа прервётся в течение
//                       нескольких тысяч инструкций;
//    ReadFn           — функция, которую ядро вызывает, когда программе нужен input.
//                       Она может блокироваться, пока пользователь не введёт строку.
//
//  Все остальные функции нужно вызывать из одного потока за раз (на один обработчик h).
//  Исключения C++ через границу C API не проходят: ошибки возвращаются кодом и текстом.
//
//    void* h = calc_create();
//    calc_set_callbacks(h, write_fn, read_fn, user);
//    int rc = calc_run(h, "var x = 5\nshow (x)", 0);      // write_fn получит "5\n"
//    calc_destroy(h);
// ════════════════════════════════════════════════════════════════════

#if defined(_WIN32)
#  define CALC_API extern "C" __declspec(dllexport)
#else
#  define CALC_API extern "C" __attribute__((visibility("default")))
#endif

namespace calc_capi {

enum Kind { kOut = 0, kErr = 1 };                       // вид текста в WriteFn: обычный вывод / сообщение об ошибке
enum Flags { kExplain = 1, kNoOptimize = 2, kPrintResult = 4 };
enum Result { kOk = 0, kCompileError = 1, kRuntimeError = 2, kCancelled = 3, kInternal = 4 };

// Получает кусок текста в UTF-8 (не обязательно целую строку: кусок может обрываться и посреди символа).
using WriteFn = void (*)(const char* data, int len, int kind, void* user);
// Должна записать в buf (не более cap байт) одну введённую строку БЕЗ перевода строки и вернуть её длину.
// Возврат -1 означает «ввода больше не будет» (отмена, закрытие).
using ReadFn = int (*)(char* buf, int cap, void* user);

struct Session;

// Вывод программы копится в буфере и отправляется пачками: по размеру, по времени (раз в ~15 мс)
// и перед ожиданием ввода — так цикл с миллионом show не превращается в миллион вызовов Python.
class OutBuf : public std::streambuf {
public:
    explicit OutBuf(Session* s) : s_(s), last_(std::chrono::steady_clock::now()) {}
    void flush();
    void tick() { if (!buf_.empty() && elapsedMs() >= kFlushMs) flush(); }

protected:
    int_type overflow(int_type ch) override {
        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            buf_ += traits_type::to_char_type(ch);
            afterWrite();
        }
        return traits_type::not_eof(ch);
    }
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        buf_.append(s, static_cast<std::size_t>(n));
        afterWrite();
        return n;
    }

private:
    static constexpr long long kFlushMs = 15;
    static constexpr std::size_t kFlushBytes = 4096;
    long long elapsedMs() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - last_).count();
    }
    void afterWrite() { if (buf_.size() >= kFlushBytes || elapsedMs() >= kFlushMs) flush(); }

    Session* s_;
    std::string buf_;
    std::chrono::steady_clock::time_point last_;
};

class InBuf : public std::streambuf {
public:
    explicit InBuf(Session* s) : s_(s) {}

protected:
    int_type underflow() override;

private:
    Session* s_;
    std::string buf_;
};

struct Session {
    calc::Environment env;
    std::atomic<bool> cancel{false};
    WriteFn writeFn = nullptr;
    ReadFn readFn = nullptr;
    void* user = nullptr;
    int errorLine = -1;                 // строка последней ошибки компиляции в основном тексте (с 1), -1 — нет
    OutBuf outBuf;
    InBuf inBuf;
    std::ostream out;
    std::istream in;

    Session() : outBuf(this), inBuf(this), out(&outBuf), in(&inBuf) { bind(); }
    void bind() {
        env.out = &out;
        env.in = &in;
    }
    // Сообщение об ошибке уходит отдельным куском с видом kErr — сначала досылаем обычный вывод, чтобы не нарушить порядок.
    void emit(const std::string& text, int kind) {
        outBuf.flush();
        if (writeFn && !text.empty()) writeFn(text.data(), static_cast<int>(text.size()), kind, user);
    }
};

inline void OutBuf::flush() {
    if (!buf_.empty()) {
        std::string chunk;
        chunk.swap(buf_);
        if (s_->writeFn) s_->writeFn(chunk.data(), static_cast<int>(chunk.size()), kOut, s_->user);
    }
    last_ = std::chrono::steady_clock::now();
}

inline InBuf::int_type InBuf::underflow() {
    if (gptr() < egptr()) return traits_type::to_int_type(*gptr());
    if (!s_->readFn || s_->cancel.load()) return traits_type::eof();
    s_->outBuf.flush();                                  // приглашение («Введите число: ») должно быть видно до ожидания
    constexpr int kCap = 65536;
    buf_.assign(kCap, '\0');
    int n = s_->readFn(&buf_[0], kCap - 1, s_->user);
    if (n < 0 || s_->cancel.load()) return traits_type::eof();
    buf_.resize(static_cast<std::size_t>(std::min(n, kCap - 1)));
    buf_ += '\n';
    setg(&buf_[0], &buf_[0], &buf_[0] + buf_.size());
    return traits_type::to_int_type(buf_[0]);
}

inline Session* S(void* h) { return static_cast<Session*>(h); }

// Нужно ли дочитывать ввод в консоли: 0 — команда завершена; 1 — не закрыта '{';
// 2 — завершён if без else (дальше может прийти 'else').
inline int continuationState(const std::string& text) {
    try {
        std::vector<calc::Token> toks = calc::Lexer(text).tokenize();
        int depth = 0;
        for (const calc::Token& t : toks) {
            if (t.type == calc::TokenType::LBrace) ++depth;
            if (t.type == calc::TokenType::RBrace) --depth;
        }
        if (depth > 0) return 1;
        calc::NodePtr program = calc::Parser(toks, text).parse();
        if (!program->body.empty()) {
            const calc::Node* last = program->body.back().get();
            if (last->kind == calc::Node::Kind::If) {
                while (last->elseBranch && last->elseBranch->kind == calc::Node::Kind::If) last = last->elseBranch.get();
                if (!last->elseBranch) return 2;
            }
        }
    } catch (...) {
        // ошибку покажет само выполнение
    }
    return 0;
}

}  // namespace calc_capi

CALC_API const char* calc_version(void) { return "calc-core 1.1"; }

CALC_API void* calc_create(void) {
    try { return new calc_capi::Session(); } catch (...) { return nullptr; }
}

CALC_API void calc_destroy(void* h) { delete calc_capi::S(h); }

CALC_API void calc_set_callbacks(void* h, calc_capi::WriteFn w, calc_capi::ReadFn r, void* user) {
    if (!h) return;
    calc_capi::Session* s = calc_capi::S(h);
    s->writeFn = w;
    s->readFn = r;
    s->user = user;
}

// Начать с чистой памяти. exeDir — папка, где runF ищет файлы программ.
CALC_API void calc_reset(void* h, const char* exeDir) {
    if (!h) return;
    calc_capi::Session* s = calc_capi::S(h);
    try {
        s->env = calc::Environment();
        s->env.exeDir = exeDir ? exeDir : "";
        s->bind();
        s->errorLine = -1;
        s->cancel.store(false);
    } catch (...) {
    }
}

// Выполняет программу в памяти h. Возвращает calc_capi::Result (0 — успех).
CALC_API int calc_run(void* h, const char* source, int flags) {
    using namespace calc_capi;
    if (!h) return kInternal;
    Session* s = S(h);
    s->cancel.store(false);
    s->errorLine = -1;
    calc::Options opt;
    opt.optimize = !(flags & kNoOptimize);
    opt.cancel = &s->cancel;
    opt.onTick = [s]() { s->outBuf.tick(); };
    int rc = kOk;
    try {
        std::string src = source ? source : "";
        if (flags & kExplain) {
            calc::explain(src, s->env, opt, s->out);
        } else {
            auto r = calc::run(src, s->env, opt);
            if (r && (flags & kPrintResult)) s->out << calc::display(*r) << "\n";
        }
    } catch (const calc::CompileError& e) {
        rc = kCompileError;
        if (!e.hasFile()) s->errorLine = static_cast<int>(e.line());
        s->emit("Ошибка компиляции: " + e.pretty() + "\n", kErr);
    } catch (const calc::RuntimeError& e) {
        if (s->cancel.load()) rc = kCancelled;
        else { rc = kRuntimeError; s->emit(std::string("Ошибка выполнения: ") + e.what() + "\n", kErr); }
    } catch (const std::exception& e) {
        rc = kInternal;
        s->emit(std::string("Внутренняя ошибка: ") + e.what() + "\n", kErr);
    } catch (...) {
        rc = kInternal;
        s->emit("Внутренняя ошибка\n", kErr);
    }
    if (s->cancel.load() && rc != kOk) rc = kCancelled;
    s->outBuf.flush();
    return rc;
}

// Прервать выполнение. Безопасно вызывать из другого потока.
CALC_API void calc_cancel(void* h) {
    if (h) calc_capi::S(h)->cancel.store(true);
}

CALC_API int calc_error_line(void* h) { return h ? calc_capi::S(h)->errorLine : -1; }

CALC_API int calc_continuation(const char* text) { return calc_capi::continuationState(text ? text : ""); }

CALC_API int calc_has_var(void* h, const char* name) {
    if (!h || !name) return 0;
    return calc_capi::S(h)->env.vars.count(name) ? 1 : 0;
}

// Печатает переменные и функции из памяти (команда vars в консоли).
CALC_API void calc_vars(void* h) {
    if (!h) return;
    calc_capi::Session* s = calc_capi::S(h);
    try {
        calc::Environment& env = s->env;
        if (env.vars.empty() && env.funcs.empty()) s->out << "(переменных и функций пока нет)\n";
        for (const auto& kv : env.vars) s->out << kv.first << " = " << calc::repr(kv.second) << "\n";
        for (const auto& kv : env.funcs) {
            s->out << "func " << kv.first << " (";
            const auto& ps = kv.second->params;
            for (std::size_t k = 0; k < ps.size(); ++k) s->out << (k ? ", " : "") << ps[k];
            s->out << ")\n";
        }
        s->outBuf.flush();
    } catch (...) {
    }
}

// ════════════════════════════════════════════════════════════════════
//  КАК РАСШИРЯТЬ (дорожная карта)
// ════════════════════════════════════════════════════════════════════
//
// ── Ещё циклы:  for i in список ──
//   break и continue уже есть (стек циклов в кодогенераторе: LoopInfo).
//   for поверх списков строится из while, скрытого счётчика, length и INDEX.
//
// ── Составное присваивание:  x += 1,  x -= 2,  x *= 3 ──
//   Присваивание x = ... уже есть. В лексере добавить токены '+=', '-=' и т. д.,
//   в statement() превратить  x += e  в  Assign(x, Binary('+', Variable(x), e)).
//
// ── Логика: тонкости ──
//   and / or / not уже есть и возвращают 1 или 0. Если захочется, как в Python,
//   возвращать сам операнд (0 or "текст" → "текст"), изменится кодогенерация Logical.
//
// ── Конкатенация строк:  "a" + "b" ──
//   В VM для Add проверить, что оба значения — строки, и склеить их
//   (сейчас popNumber() запрещает строки в арифметике).
//
// ── Изменение списков:  x $1 = 10,   append (x, 7),   len (x) ──
//   Присваивание элементу: в statement() после postfix-выражения вида Index
//   увидеть Assign и создать узел IndexAssign; опкод StoreIndex снимает со
//   стека значение, индекс и список. Для len и append нужны «встроенные
//   функции» (разбор вызова и опкод Call уже есть — осталось проверять имя
//   встроенной функции раньше, чем ищем её в env.funcs).
//
// ── Новый бинарный оператор (например, целочисленное деление ~) ──
//   ('//' занят под комментарии, поэтому берём другой символ.)
//   1. Lexer::tokenize(): ветка для '~' в односимвольных токенах.
//   2. TokenType: добавить IntDiv; в Parser::symbol() вернуть "~".
//   3. Parser::term(): добавить токен в match({...}) рядом со Star/Slash.
//   4. OpCode::IntDiv + opName + функция doIntDiv + запись в binaryOps():
//        {"~", {OpCode::IntDiv, doIntDiv}}
//   Оптимизатор, кодогенератор и VM менять не нужно — они читают таблицу.
//
// ── Подключение файлов с определениями ──
//   runF выполняет файл целиком в общей памяти, а функции (func) уже есть,
//   поэтому «библиотека» работает сразу: файл с func-определениями
//   подключается командой runF "lib.txt", дальше функции вызываются по имени.
//
// ── Что можно добавить к функциям ──
//   func / return, инструкции DEFINE_FUNC / CALL / RETURN, кадры вызова и
//   локальные переменные уже реализованы. Дальше можно добавить:
//   значения параметров по умолчанию, вложенные функции с доступом к
//   локальным переменным внешней (замыкания).
//
// ── Настоящая компиляция вместо VM ──
//   Вместо байт-кода можно генерировать ассемблер или LLVM IR. Конвейер до
//   кодогенератора остаётся тем же — меняется только этап 5.

