"""Run only Dr.Hack UI/passive diagnostics; no attack implementation is compiled."""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2] + '\n'


def main():
    tests = Path(__file__).resolve().parent
    firmware = tests.parent / 'arduino/CompanionOS_Main'
    page = (firmware / 'page_dr_hack.h').read_text(encoding='utf-8')
    globals_source = (firmware / 'globals.h').read_text(encoding='utf-8')
    ir = (firmware / 'dh_ir_tools.h').read_text(encoding='utf-8')
    cc = (firmware / 'dh_cc1101_tools.h').read_text(encoding='utf-8')
    radio = (firmware / 'dh_radio_tools.h').read_text(encoding='utf-8')
    ble = (firmware / 'dh_ble_tools.h').read_text(encoding='utf-8')
    wifi = (firmware / 'dh_wifi_tools.h').read_text(encoding='utf-8')
    enum_start = globals_source.index('enum DrHackSubState {')
    enum_end = globals_source.index('};', enum_start) + 2
    # Exercise legacy passive BLE logic in isolation; production config disables it.
    pieces = ['#define ENABLE_BLE_TOOLS 1\n#define DH_TOOLS_PER_PAGE 8\n#define DH_MENU_PAGES 6\n#define DH_TOTAL_TOOLS 48\n',
              globals_source[enum_start:enum_end], function(globals_source, 'inline bool dhUsesPageInput('),
              page[page.index('DrHackSubState dhCurrentState ='):page.index('static void dhExitScanline()')],
              '#define DH_DISCOVERY_HOST\n#include "' + (firmware / 'dh_discovery.h').as_posix() + '"\n',
              ble[ble.index('#define DH_BII_MAX'):ble.index('enum DH_SpamMode')],
              wifi[wifi.index('static void dhWifiPrepare()'):wifi.index('#define DH_PROBE_MAX')],
              function(wifi, 'static void dhRunWifiConfig()'),
              function(page, 'static void dhShowError(const char* msg)'),
              function(page, 'void drawDrHackTile()'), function(page, 'static void dhDrawMenu()'),
              page[page.index('#define DH_MAX_NETWORKS'):page.index('#define DH_PORT_COUNT')],
              page[page.index('#define DH_BLE_MAX'):page.index('static void dhDrawSysInfo()')],
              function(page, 'static void dhDrawSysInfo()'), function(page, 'static void dhDrawAbout()'),
              function(page, 'void dhNavigate(int delta)'), function(cc, 'static bool dhProbeCc1101()'),
              function(ir, 'static void dhRunHwDiag()'), function(ir, 'static void dhRunInputMonitor()'),
              radio[radio.index('static const uint8_t ch_wifi_spec[14]'):radio.index('// RC/Drone:')],
              radio[radio.index('#define DH_SCAN_CHANNELS'):radio.index('\n#endif // ESP32')]]
    dispatch = function(page, 'void dhSelect()')
    compiled = {'dhRunWifiScan', 'dhRunWifiDiscovery', 'dhRunBleScan', 'dhDrawSysInfo', 'dhRunHwDiag', 'dhDrawAbout',
                'dhDrawMenu', 'dhSelect', 'dhRunInputMonitor', 'dhRunRadioScanner', 'dhRunBleInspector', 'dhRunWifiConfig', 'dhBiiSelect',
                'dhRunChannelScan', 'dhRunWifiRadar', 'dhRunWifiDirection'}
    # Every excluded tool fails the test if accidentally dispatched.
    for name in sorted(set(re.findall(r'\b(dh\w+)\(\)', dispatch)) - compiled):
        pieces.append(f'static void {name}() {{ ++forbiddenCalls; throw std::runtime_error("Excluded tool called"); }}\n')
    pieces.append(page[page.index('#define DH_RUN_TOOL'):page.index('void dhSelect()')])
    pieces.append(dispatch)
    pieces.append(function(page, 'void initDrHack()'))
    compiler = shutil.which('g++') or 'C:/mingw/bin/g++.exe'
    fonts = Path(os.environ['USERPROFILE']) / 'OneDrive/Documents/Arduino/libraries/TFT_eSPI'
    env = dict(os.environ)
    env['PATH'] = str(Path(compiler).parent) + os.pathsep + env.get('PATH', '')
    output = tests / 'host-output'
    output.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='companion-diagnostics-') as directory:
        directory = Path(directory)
        (directory / 'dr_hack_passive.inc').write_text('\n'.join(pieces), encoding='utf-8')
        for w, h in ((160, 128), (320, 240)):
            executable = output / f'dr-hack-diagnostics-{w}.exe'
            frames = output / f'dr-hack-{w}x{h}'
            frames.mkdir(exist_ok=True)
            subprocess.run([compiler, '-std=c++14', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                            '-Wno-unused-function', '-Wno-unused-variable', '-Wno-sign-compare',
                            f'-DSCREEN_W={w}', f'-DSCREEN_H={h}', '-I', str(fonts), '-I', str(directory),
                            str(tests / 'dr_hack_diagnostics_host.cpp'), '-o', str(executable)], env=env, check=True)
            subprocess.run([str(executable), str(frames)], env=env, check=True)


if __name__ == '__main__':
    main()
