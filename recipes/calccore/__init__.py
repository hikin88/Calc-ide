"""Рецепт python-for-android: собирает C++ ядро компилятора (calc_core.cpp) в libcalccore.so.

Библиотека собирается NDK-компилятором отдельно для каждой архитектуры из android.archs и попадает
в папку нативных библиотек APK. Python-часть загружает её через ctypes (см. calc_bridge.py).

Исходник лежит рядом с этим файлом, поэтому скачивать нечего (url = None).
"""
import shutil
from os.path import join

import sh
from pythonforandroid.logger import info, shprint
from pythonforandroid.recipe import Recipe
from pythonforandroid.util import ensure_dir


class CalcCoreRecipe(Recipe):
    name = 'calccore'
    version = '1.1'
    url = None
    depends = []

    def _cxx_command(self, arch, env):
        """clang++ для нужной архитектуры (aarch64-linux-android21-clang++ и т. п.)."""
        try:
            exe = arch.get_clang_exe(with_target=True, plus_plus=True)
        except Exception:
            exe = None
        if exe:
            # sh.Command ищет программу по PATH текущего процесса, а нужный каталог NDK — только в env
            return [shutil.which(exe, path=env.get('PATH')) or exe]
        return env['CXX'].split()          # запасной вариант для старых версий python-for-android

    def build_arch(self, arch):
        env = self.get_recipe_env(arch)
        src = join(self.get_recipe_dir(), 'calc_core.cpp')
        build_dir = self.get_build_dir(arch.arch)
        ensure_dir(build_dir)
        out = join(build_dir, 'libcalccore.so')

        cxx = self._cxx_command(arch, env)
        info('calccore: компиляция {} для {}'.format(src, arch.arch))
        shprint(
            sh.Command(cxx[0]), *cxx[1:],
            '-std=c++17', '-O2', '-fPIC', '-shared', '-fvisibility=hidden',
            '-static-libstdc++',                      # библиотека C++ внутрь .so: отдельный libc++_shared.so не нужен
            '-Wl,-z,max-page-size=16384',             # требование Android 15+ к выравниванию (страницы 16 КБ)
            '-Wl,--strip-all',
            src, '-o', out,
            _env=env,
        )

        libs_dir = self.ctx.get_libs_dir(arch.arch)
        ensure_dir(libs_dir)
        shutil.copyfile(out, join(libs_dir, 'libcalccore.so'))


recipe = CalcCoreRecipe()
