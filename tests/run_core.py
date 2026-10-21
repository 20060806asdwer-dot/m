"""Runs real cross-compiled pure C logic under Unicorn; does not emulate peripherals."""
from pathlib import Path
import json
import struct
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from project import find_tool

def main():
    try:
        import unicorn as uc
        from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR
    except ImportError:
        raise SystemExit('测试专用依赖未安装：python -m pip install unicorn==2.1.4；正常编译烧录不需要它。')
    gcc = Path(find_tool('arm-none-eabi-gcc.exe', 'gnu-tools-for-stm32'))
    out = ROOT/'build/tests';out.mkdir(parents=True,exist_ok=True)
    elf = out/'test_core.elf';binary=out/'test_core.bin'
    src = ROOT/'firmware/Core/Src'
    args=[gcc,'-mcpu=cortex-m3','-mthumb','-Os','-g','-ffreestanding','-fno-builtin','-nostdlib','-Wall','-Wextra','-Werror',
          '-I'+str(ROOT/'firmware/Core/Inc'), ROOT/'tests/test_core.c',
          *[src/n for n in ('energy_policy.c','packet.c','features.c','boot_journal.c')],
          '-T'+str(ROOT/'tests/test_flash.ld'),'-lgcc','-o',elf]
    subprocess.run([str(a) for a in args],check=True)
    subprocess.run([str(gcc.parent/'arm-none-eabi-objcopy.exe'),'-O','binary',str(elf),str(binary)],check=True)
    lines=subprocess.check_output([str(gcc.parent/'arm-none-eabi-nm.exe'),str(elf)],text=True).splitlines()
    symbols={p[2]:int(p[0],16) for line in lines if len(p:=line.split())==3}
    engine=uc.Uc(uc.UC_ARCH_ARM,uc.UC_MODE_THUMB|uc.UC_MODE_MCLASS)
    engine.mem_map(0x08000000,0x10000);engine.mem_map(0x20000000,0x5000)
    engine.mem_write(0x08000000,binary.read_bytes())
    engine.reg_write(UC_ARM_REG_SP,0x20004ff0);engine.reg_write(UC_ARM_REG_LR,0x08000001)
    reached=[]
    def stop(e,address,size,user):
        if address==symbols['tests_finished']:
            reached.append(True);e.emu_stop()
    engine.hook_add(uc.UC_HOOK_CODE,stop)
    engine.emu_start(symbols['test_main']|1,0,count=30000000)
    def word(name):return struct.unpack('<I',engine.mem_read(symbols[name],4))[0]
    assert reached,'测试未运行到结束（指令上限或异常）'
    assert word('test_failed_line')==0,f'C测试失败: test_core.c:{word("test_failed_line")}'
    sys.path.insert(0,str(ROOT/'desktop'))
    from protocol import Record,encode
    expected=encode(Record(1,7,65535,123456,-1234,3900,27,2,7,3))
    actual=bytes(engine.mem_read(symbols['test_frame'],24))
    assert actual==expected,'ARM C 与 Python 数据帧不一致'
    report={'status':'PASS','checks':word('test_checks'),'python_frame_matches_arm_c':True,
            'scope':'Pure C logic only; no GPIO, RTC, ADC, I2C, RF or flash hardware emulation',
            'unicorn_version':uc.__version__,'frame_hex':actual.hex()}
    (out/'result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
