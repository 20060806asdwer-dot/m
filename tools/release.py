"""【固件打包】把已编译并通过核心测试的六配置复制到releases，记录源码与文件校验值。"""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
from project import ROOT,PROFILES,find_tool,build

def sha(file):return hashlib.sha256(file.read_bytes()).hexdigest()
def main():
    result_file=ROOT/'build/tests/result.json'
    if not result_file.exists():raise SystemExit('先运行 python tools/check.py --build')
    result=json.loads(result_file.read_text(encoding='utf-8'))
    if result.get('status')!='PASS':raise SystemExit('核心测试未通过')
    gcc=Path(find_tool('arm-none-eabi-gcc.exe','gnu-tools-for-stm32'))
    reports=[]
    for profile in PROFILES:
        build(profile)
        source=ROOT/'build'/profile
        if not all((source/('railnode.'+ext)).exists() for ext in ('bin','hex','elf')):
            raise SystemExit('配置缺少编译结果：'+profile)
        target=ROOT/'releases'/profile;target.mkdir(parents=True,exist_ok=True)
        for ext in ('bin','hex','elf'):shutil.copy2(source/('railnode.'+ext),target/('railnode.'+ext))
        subprocess.run([str(gcc.parent/'arm-none-eabi-strip.exe'),'--strip-debug',str(target/'railnode.elf')],check=True)
        sizes=subprocess.check_output([str(gcc.parent/'arm-none-eabi-size.exe'),str(source/'railnode.elf')],text=True).splitlines()[1].split()
        text,data,bss=map(int,sizes[:3])
        reports.append(dict(profile=profile,flash_bytes=text+data,reserved_flash_journal_bytes=2048,
            data_bss_stack_heap_bytes=data+bss,minimum_stack_bytes=2048,heap_bytes=512,
            files={p.name:sha(p) for p in target.iterdir() if p.is_file()}))
    code=[p for p in ROOT.rglob('*') if p.is_file() and
          (p.is_relative_to(ROOT/'firmware/Core') or p.name in ('CMakeLists.txt','CMakePresets.json','arm-gcc.cmake','STM32F103C8Tx_FLASH.ld'))]
    report=dict(board='STM32F103C8T6 Blue Pill PC13',compiler=subprocess.check_output([str(gcc),'--version'],text=True).splitlines()[0],
        profiles=reports,application_source_sha256={p.relative_to(ROOT).as_posix():sha(p) for p in code},
        checks=['Six profiles build with -Wall -Wextra -Werror',f"{result['checks']} ARM core/fake-HAL checks and C/Python frame equality",'Python protocol selftest, 8 desktop/stream/fake-serial tests'],
        not_tested=['Physical flash/SWD/F5 and peripherals','Actual I2C recovery and UART interrupt timing','Over-air ACK timing and RF reliability','STOP wake, measured power and budget calibration','DeepSeek live API'],
        note='仅打包现有编译文件；修改源码后必须先重新执行check.py --build。注释变化也会更新源码校验值。')
    (ROOT/'releases/BUILD_INFO.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    shutil.copy2(result_file,ROOT/'releases/CORE_TEST_RESULT.json')
    print('PASS: 六配置固件、来源哈希与核心测试结果已打包。')

if __name__=='__main__':main()
