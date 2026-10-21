"""【13 工程工具：找工具链、编译、烧录与F5设置】
流程与文件对应关系见 docs/代码阅读地图.md。
"""
"""Build and optionally flash F103 firmware. Only `flash` accesses hardware."""
from pathlib import Path
import argparse
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PROFILES = ('bringup', 'sensors', 'wireless', 'wireless_ack', 'adaptive', 'fixed')

# A 查找本机ST工具
def find_tool(exe, bundle):
    override = os.environ.get('RAILNODE_' + bundle.upper().replace('-', '_'))
    if override:
        path = Path(override)
        if not path.is_file():
            raise RuntimeError(f'工具路径无效: {path}')
        return str(path)
    found = shutil.which(exe)
    if found:
        return found
    base = Path(os.environ.get('LOCALAPPDATA', '')) / 'stm32cube' / 'bundles' / bundle
    files = list(base.glob('*/bin/' + exe))
    if not files:
        files = list(base.glob('**/' + exe))
    if not files:
        raise RuntimeError(f'找不到 {exe}；在 VS Code 的 ST Bundle Manager 安装 {bundle}。')
    def version_key(p):
        return tuple(int(x) for x in re.findall(r'\d+', str(p.relative_to(base))))
    return str(max(files, key=version_key))

# B 只为本次构建准备PATH
def environment():
    gcc = find_tool('arm-none-eabi-gcc.exe', 'gnu-tools-for-stm32')
    cmake = find_tool('cmake.exe', 'cmake')
    ninja = find_tool('ninja.exe', 'ninja')
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([str(Path(p).parent) for p in (gcc, cmake, ninja)] + [env.get('PATH', '')])
    return cmake, ninja, env

# C 执行工具命令并检查退出状态
def run(args, env=None):
    print('>', subprocess.list2cmdline([str(x) for x in args]), flush=True)
    subprocess.run([str(x) for x in args], cwd=ROOT, env=env, check=True)

# D 生成与编译指定固件
def build(profile):
    cmake, ninja, env = environment()
    dest = ROOT / 'build' / profile
    configure=[cmake, '-S', ROOT, '-B', dest, '-G', 'Ninja', '-DCMAKE_MAKE_PROGRAM=' + ninja, '-DPROFILE=' + profile]
    if not (dest/'CMakeCache.txt').exists():
        configure.append('-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'cmake/arm-gcc.cmake'))
    run(configure, env)
    run([cmake, '--build', dest, '--parallel'], env)

# E 生成当前电脑的F5配置
def setup():
    cmake, ninja, env = environment()
    print('工具检查通过:', cmake, ninja, sep='\n')
    gdb = find_tool('arm-none-eabi-gdb.exe', 'gnu-gdb-for-stm32')
    server = find_tool('ST-LINK_gdbserver.exe', 'stlink-gdbserver')
    programmer = find_tool('STM32_Programmer_CLI.exe', 'programmer')
    configs = []
    for profile in PROFILES:
        configs.append(dict(name='F103 ' + profile, type='stlinkgdbtarget', request='launch', origin='snippet',
            cwd='${workspaceFolder}', preLaunchTask='Build ' + profile, runEntry='main',
            gdb=gdb, serverExe=server, serverCubeProgPath=str(Path(programmer).parent),
            serverInterface='SWD', serverInterfaceFrequency='1000', deviceName='STM32F103C8',
            deviceDebugInLowPower=False,
            imagesAndSymbols=[dict(imageFileName='${workspaceFolder}/build/' + profile + '/railnode.elf')]))
    path = ROOT / '.vscode/launch.json'
    path.parent.mkdir(exist_ok=True)
    path.write_text(json.dumps(dict(version='0.2.0', configurations=configs), indent=2), encoding='utf-8')
    print('已生成当前电脑的 F5 调试配置。先 Build bringup，再连接 ST-Link。')

# F 命令行入口：setup/build/flash
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('setup', 'build', 'flash'))
    parser.add_argument('profile', choices=PROFILES, nargs='?', default='bringup')
    args = parser.parse_args()
    try:
        if args.action == 'setup':
            setup()
        elif args.action == 'build':
            build(args.profile)
        else:
            image = ROOT / 'build' / args.profile / 'railnode.hex'
            if not image.exists():
                raise RuntimeError('先执行 build ' + args.profile)
            programmer = find_tool('STM32_Programmer_CLI.exe', 'programmer')
            # Erase only downloaded sectors; no mass erase, preserving the boot journal.
            run([programmer, '-c', 'port=SWD', 'mode=UR', 'freq=1000', '-d', image, '-v', '-rst'])
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print('失败:', error, file=sys.stderr)
        return 1
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
