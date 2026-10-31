app]

# Название и идентификатор приложения (package.domain.package.name должен быть уникальным)
title = Calc IDE
package.name = calcide
package.domain = org.calcide

# Какие файлы кладём в APK (папки recipes, tests, .github в приложение не попадают)
source.dir = .
source.include_exts = py,ttf,txt,png
source.exclude_dirs = tests, bin, recipes, .github, .buildozer, __pycache__

version = 1.0.0

# Иконка приложения: квадратный PNG в корне репозитория
icon.filename = %(source.dir)s/icon.png

# calccore — наш рецепт из папки recipes: собирает C++ ядро компилятора
requirements = python3,kivy==2.3.0,pyjnius,calccore

orientation = portrait, landscape
fullscreen = 0

# Разрешения не нужны: файлы программ хранятся во внутренней папке приложения
android.permissions =

android.archs = arm64-v8a, armeabi-v7a
android.api = 33
android.minapi = 21
android.ndk = 25b
android.accept_sdk_license = True
android.allow_backup = True
android.logcat_filters = *:S python:D

# Локальные рецепты python-for-android
p4a.local_recipes = ./recipes
# Версия python-for-android закреплена: новая master использует Python новее 3.11,
# с которым Kivy 2.3.0 не компилируется. Эта версия собирает Python 3.11.
p4a.branch = v2024.01.21


[buildozer]

# 0 = только ошибки, 1 = информация, 2 = подробно
log_level = 2
warn_on_root = 1
