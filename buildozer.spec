[app]

# Название и идентификатор приложения (package.domain.package.name должен быть уникальным)
title = Calc IDE
package.name = calcide
package.domain = org.calcide

# Какие файлы кладём в APK (папки recipes, tests, .github в приложение не попадают)
source.dir = .
source.include_exts = py,ttf,txt
source.exclude_dirs = tests, bin, recipes, .github, .buildozer, __pycache__

version = 1.0.0

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
p4a.branch = v2024.01.21


[buildozer]

# 0 = только ошибки, 1 = информация, 2 = подробно
log_level = 2
warn_on_root = 1
