"""Summarize receiver CSV or integrate measured power CSV. Never invent readings."""
import argparse
import csv
from datetime import datetime
import json
import math
from pathlib import Path
from protocol import Record,encode,decode

def telemetry(rows, allow_simulation=False):
    if not rows:
        raise ValueError('CSV没有记录')
    sessions={};gaps=duplicates=backwards=0;valid=[0,0,0];sources=set();times=[]
    for row in rows:
        source=row['source'];sources.add(source)
        if source=='SIMULATED' and not allow_simulation:
            raise ValueError('这是演示CSV；若只检查脚本，明确添加 --allow-simulation')
        record=decode(encode(Record(**{f:int(row[f]) for f in Record.__dataclass_fields__})))
        times.append(datetime.fromisoformat(row['received_at']))
        key=(record.node,record.boot);previous=sessions.get(key)
        if previous is None:
            sessions[key]=record.seq
        else:
            diff=(record.seq-previous)&65535
            if diff==0:duplicates+=1
            elif diff<32768:
                gaps+=diff-1;sessions[key]=record.seq
            else:backwards+=1
        for bit in range(3):valid[bit]+=bool(record.flags&(1<<bit))
    return dict(records=len(rows),sources=sorted(sources),simulation='SIMULATED' in sources,
        observed_node_boot_sessions=len(sessions),sequence_gaps=gaps,duplicates=duplicates,
        old_or_out_of_order=backwards,valid_temperature=valid[0],valid_vibration=valid[1],valid_voltage=valid[2],
        receiver_span_s=(max(times)-min(times)).total_seconds(),
        limitation='序号缺口只覆盖已观察会话的内部；无法计算开始前、结尾后或完全未收到会话的丢包。时间为电脑接收时间。')

def power(rows):
    if len(rows)<2:raise ValueError('功耗积分至少需要两行测量数据')
    samples=[]
    for row in rows:
        t,v,i=(float(row[k]) for k in ('t_s','v_v','i_ma'))
        if not all(math.isfinite(x) and x>=0 for x in (t,v,i)):
            raise ValueError('时间、电压、电流必须是非负有限数；本脚本用于单向供电输入功耗')
        if samples and t<=samples[-1][0]:raise ValueError('t_s必须严格递增')
        samples.append((t,v*i))
    energy_mj=sum((b[0]-a[0])*(a[1]+b[1])/2 for a,b in zip(samples,samples[1:]))
    duration=samples[-1][0]-samples[0][0]
    return dict(samples=len(samples),duration_s=duration,energy_j=energy_mj/1000,
        average_mw=energy_mj/duration,peak_sample_mw=max(p for t,p in samples),
        limitation='按离散功率点做梯形积分；采样过慢会漏掉射频尖峰。输入必须为仪器实测值并记录测量位置。')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('kind',choices=('telemetry','power'));p.add_argument('csv',type=Path)
    p.add_argument('--allow-simulation',action='store_true')
    args=p.parse_args()
    try:
        with args.csv.open(encoding='utf-8-sig',newline='') as f:rows=list(csv.DictReader(f))
        result=telemetry(rows,args.allow_simulation) if args.kind=='telemetry' else power(rows)
    except (OSError,ValueError,KeyError,TypeError) as exc:p.exit(2,f'无法分析：{exc}\n')
    print(json.dumps(result,ensure_ascii=False,indent=2))

if __name__=='__main__':main()
