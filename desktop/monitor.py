"""【15 电脑上位机：接收、可选确认、显示、记录】
流程与文件对应关系见 docs/代码阅读地图.md。
"""
"""Local monitor: explicit simulation OR physical serial, never silently substitutes data."""
import argparse
import csv
from datetime import datetime
import math
from pathlib import Path
import time
import tkinter as tk
from tkinter import ttk, messagebox
from protocol import Parser, Record, encode, ack_for

def main():
    # A 选择真实串口或明确的演示模式
    ap = argparse.ArgumentParser(description=__doc__)
    source = ap.add_mutually_exclusive_group(required=True)
    source.add_argument('--simulate', action='store_true', help='明确使用演示数据，不连接硬件')
    source.add_argument('--port', help='例如COM5，实际LoRa接收端串口')
    ap.add_argument('--baud', type=int, default=9600)
    ap.add_argument('--ack',action='store_true',help='为wireless_ack固件回传确认；需要接收端USB-TTL TX接E22 RXD')
    ap.add_argument('--timeout',type=float,default=900,help='通信超时秒数；默认900覆盖自适应最长300秒周期，固定15秒可设45')
    ap.add_argument('--out-dir', type=Path, default=Path(__file__).resolve().parent / 'records')
    args = ap.parse_args()
    if not math.isfinite(args.timeout) or args.timeout<=0:ap.error('--timeout必须是正的有限秒数')
    if args.ack and args.simulate:ap.error('--ack需要真实串口，不能与演示模式一起使用')
    port = None
    if args.port:
        try:
            import serial
            port = serial.Serial(args.port, args.baud, timeout=0,write_timeout=.2)
        except (ImportError, OSError, ValueError) as exc:
            ap.exit(2, f'串口无法打开：{exc}\n需要硬件模式时先安装pyserial；不会改用模拟数据。\n')

    # B 创建带来源标识的CSV记录
    args.out_dir.mkdir(parents=True, exist_ok=True)
    dest = args.out_dir / f'{"SIMULATED" if args.simulate else "LIVE"}_{datetime.now():%Y%m%d_%H%M%S_%f}.csv'
    output = dest.open('x', newline='', encoding='utf-8-sig')
    writer = csv.writer(output)
    writer.writerow(['received_at', 'source', *Record.__dataclass_fields__.keys()])
    # C 创建界面
    root = tk.Tk()
    root.title('磁场取电监测 | ' + ('演示数据 非实测' if args.simulate else f'串口实测 {args.port}'))
    root.geometry('1050x780')
    root.minsize(760, 600)
    frame = ttk.Frame(root, padding=18)
    frame.pack(fill='both', expand=True)
    ttk.Label(frame, text='磁场取电无线监测', font=('Microsoft YaHei UI', 19, 'bold')).pack(anchor='w')
    ttk.Label(frame, text='演示数据，不能用于性能报告' if args.simulate else '仅显示接收数据；数值正确性仍取决于传感器校准', foreground='#ad4b13').pack(anchor='w', pady=5)
    status = tk.StringVar(value='等待数据')
    values = tk.StringVar(value='温度 —    储能 —    振动 —    状态 —')
    ttk.Label(frame, textvariable=values, font=('Microsoft YaHei UI', 14)).pack(anchor='w', pady=10)
    ttk.Label(frame, textvariable=status, wraplength=970).pack(anchor='w')
    threshold_row = ttk.Frame(frame)
    threshold_row.pack(anchor='w', pady=8)
    ttk.Label(threshold_row, text='温度告警阈值 ℃').pack(side='left')
    threshold = tk.StringVar(value='50')
    ttk.Entry(threshold_row, textvariable=threshold, width=8).pack(side='left', padx=8)
    chart = tk.Canvas(frame, background='#f4f7fb', highlightthickness=0)
    chart.pack(fill='both', expand=True, pady=8)
    ttk.Label(frame, text=f'CSV记录：{dest}', wraplength=970).pack(anchor='w')
    ttk.Label(frame, text='包统计仅覆盖本次接收会话；断电重启标识必须由固件正确维护。', foreground='#596675').pack(anchor='w', pady=5)
    parser = Parser()
    records = []
    sessions = {}
    stats = {'received': 0, 'gap': 0, 'duplicate': 0}
    last = [None]
    started = time.monotonic()
    next_sim = [started]
    error = ['']

    # D 绘制最近接收记录的三条曲线
    def render():
        chart.delete('all')
        width, height = max(chart.winfo_width(), 600), max(chart.winfo_height(), 300)
        series = [('温度 ℃', lambda r: r.temp_centi / 100 if r.flags & 1 else None, '#ce5e35'),
                  ('储能 V', lambda r: r.store_mv / 1000 if r.flags & 4 else None, '#157b67'),
                  ('振动 RMS mg', lambda r: r.vibration_mg if r.flags & 2 else None, '#416ab5')]
        for idx, (label, value, color) in enumerate(series):
            top = idx * height / 3 + 26
            bottom = (idx + 1) * height / 3 - 16
            chart.create_text(10, top - 12, text=label, anchor='w', fill=color, font=('Microsoft YaHei UI', 10))
            chart.create_line(65, bottom, width - 15, bottom, fill='#cbd4df')
            numbers = [value(r) for r in records]
            valid = [v for v in numbers if v is not None]
            if not valid:
                continue
            lo, hi = min(valid), max(valid)
            pad = max((hi - lo) * .15, .05 if idx == 1 else 1)
            lo, hi = lo - pad, hi + pad
            chart.create_text(60, top, text=f'{hi:.2f}', anchor='e', fill='#647080')
            chart.create_text(60, bottom, text=f'{lo:.2f}', anchor='e', fill='#647080')
            points = []
            for i, val in enumerate(numbers):
                if val is None:
                    if len(points) >= 4:
                        chart.create_line(*points, fill=color, width=2)
                    points = []
                    continue
                points += [70 + i * (width - 90) / max(len(numbers) - 1, 1), bottom - (val - lo) / (hi - lo) * (bottom - top)]
            if len(points) >= 4:
                chart.create_line(*points, fill=color, width=2)
        chart.create_text(width - 18, height - 3, text='横轴：最近180条接收记录；不是均匀时间轴', anchor='se', fill='#667385')

    # E 接收统计与CSV写入
    def accept(r):
        stats['received'] += 1
        key = (r.node, r.boot)
        previous = sessions.get(key)
        if previous is not None:
            diff = (r.seq - previous) & 0xffff
            if diff == 0:
                stats['duplicate'] += 1
            elif 1 < diff < 0x8000:
                stats['gap'] += diff - 1
            if 0 < diff < 0x8000:
                sessions[key] = r.seq
        else:
            sessions[key] = r.seq
        records.append(r)
        del records[:-180]
        last[0] = time.monotonic()
        writer.writerow([datetime.now().isoformat(timespec='milliseconds'), 'SIMULATED' if args.simulate else args.port, *r.__dict__.values()])
        output.flush()
        temp = f'{r.temp_centi/100:.2f}℃' if r.flags & 1 else '故障/无效'
        store = f'{r.store_mv/1000:.3f}V' if r.flags & 4 else '故障/无效'
        vib = f'{r.vibration_mg}mg' if r.flags & 2 else '未采样/无效'
        values.set(f'温度 {temp}    储能 {store}    振动 {vib}    {("保护", "低能量", "正常")[r.state]}')
        render()

    # F 超时、阈值和有效位显示
    def update_status():
        if error[0]:
            status.set(error[0])
            return
        if last[0] is None:
            status.set('等待有效24字节数据帧；本软件不会用模拟值填充串口数据。')
            return
        r = records[-1]
        age = time.monotonic() - last[0]
        stale = age > args.timeout
        try:
            limit = float(threshold.get())
            if not math.isfinite(limit):
                raise ValueError
        except ValueError:
            note = '阈值输入无效'
        else:
            note = '温度告警' if r.flags & 1 and r.temp_centi / 100 >= limit else '温度未越限'
        if not r.flags & 1:
            note = '温度传感器无有效数据'
        if r.flags & 8:
            note += '；节点告警标志'
        status.set(f'{"通信超时，请检查节点" if stale else note} | 收到{stats["received"]}，序号缺口{stats["gap"]}，重复{stats["duplicate"]}，坏帧{parser.bad_frames}，距上次{age:.1f}s')

    # G 定时读取串口：校验通过后可回传ACK
    def poll():
        now = time.monotonic()
        try:
            if args.simulate and now >= next_sim[0]:
                i = int(now - started)
                r = Record(1, 1, i & 65535, i, int(2700 + 350 * math.sin(i / 12)),
                           int(3870 + 100 * math.sin(i / 18)), int(30 + 20 * abs(math.sin(i / 6))), 2, 7)
                for decoded in parser.feed(encode(r)):
                    accept(decoded)
                next_sim[0] = now + 1
            elif port and not error[0]:
                for decoded in parser.feed(port.read(4096)):
                    if args.ack:
                        reply=ack_for(decoded)
                        if port.write(reply)!=len(reply):raise OSError('确认帧未完整写入串口')
                    accept(decoded)
        except OSError as exc:
            error[0] = f'串口断开：{exc}。请重新连接并重启本程序；没有转为模拟。'
            if port:
                port.close()
        update_status()
        root.after(100, poll)

    # H 关闭串口、日志和窗口
    def close():
        if port:
            port.close()
        output.close()
        root.destroy()
    root.protocol('WM_DELETE_WINDOW', close)
    chart.bind('<Configure>', lambda event: render())
    poll()
    root.mainloop()

if __name__ == '__main__':
    main()
