"""【无硬件总验收】核对依赖、电脑协议、假串口闭环和ARM算法，可选编译全部配置。"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys
from project import ROOT,PROFILES,build

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',action='store_true',help='再编译全部六配置；不连接硬件')
    args=parser.parse_args()
    manifest=json.loads((ROOT/'DEPENDENCIES.json').read_text(encoding='utf-8'))
    for name,expected in manifest['files'].items():
        file=ROOT/'firmware/Drivers'/name
        if hashlib.sha256(file.read_bytes()).hexdigest()!=expected:
            raise SystemExit('第三方文件校验失败：'+name)
    print('PASS: 固定版本第三方依赖校验',flush=True)
    commands=[['desktop/selftest.py'],['-m','unittest','discover','-s','tests','-p','test_*.py'],['tests/run_core.py']]
    for command in commands:subprocess.run([sys.executable,*command],cwd=ROOT,check=True)
    if args.build:
        for profile in PROFILES:build(profile)
    print('PASS: 无硬件软件检查完成；未执行烧录或DeepSeek调用。')

if __name__=='__main__':main()
