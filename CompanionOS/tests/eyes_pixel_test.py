"""Render actual eye engines on the host, including intermediate-frame checks."""

import os
from pathlib import Path
import shutil
import subprocess


def main():
    tests = Path(__file__).resolve().parent
    output = tests / 'host-output'
    output.mkdir(exist_ok=True)
    compiler = shutil.which('g++') or 'C:/mingw/bin/g++.exe'
    fonts = Path(os.environ['USERPROFILE']) / 'OneDrive/Documents/Arduino/libraries/TFT_eSPI'
    env = dict(os.environ)
    env['PATH'] = str(Path(compiler).parent) + os.pathsep + env.get('PATH', '')
    for w, h in ((160, 128), (320, 240)):
        frames = output / f'{w}x{h}'
        frames.mkdir(exist_ok=True)
        executable = output / f'eyes-pixels-{w}.exe'
        subprocess.run([compiler, '-std=c++14', '-O1', '-Wall', '-Wextra', '-Wno-unused-function',
                        '-Wno-unused-variable', '-Wno-sign-compare', '-I', str(fonts),
                        f'-DSCREEN_W={w}', f'-DSCREEN_H={h}', '-DESP32',
                        str(tests / 'eyes_pixel_regression.cpp'), '-o', str(executable)], env=env, check=True)
        subprocess.run([str(executable), str(frames)], env=env, check=True)


if __name__ == '__main__':
    main()
