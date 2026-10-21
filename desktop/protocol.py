"""【14 电脑端协议：遥测解码、确认帧和串口分片恢复】
流程与文件对应关系见 docs/代码阅读地图.md。
"""
"""Bench protocol V1, 24 bytes; no dependency on serial hardware."""
from dataclasses import dataclass
import struct

MAGIC = b'\xa5\x5a'
BODY = struct.Struct('<2sBBHHIhHHBBH')
FRAME_SIZE = 24
assert BODY.size == 22

# A CRC16校验
def crc16(data):
    """CRC-16/CCITT-FALSE: poly 0x1021, init FFFF, no reflection/xorout."""
    crc = 0xffff
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xffff if crc & 0x8000 else (crc << 1) & 0xffff
    return crc

# B 一条遥测记录的数据字段
@dataclass(frozen=True)
class Record:
    node: int
    boot: int
    seq: int
    uptime_s: int
    temp_centi: int
    store_mv: int
    vibration_mg: int
    state: int
    flags: int
    reset_count: int = 0

# C 生成24字节遥测帧
def encode(r):
    data = BODY.pack(MAGIC, 1, r.node, r.boot, r.seq, r.uptime_s,
                     r.temp_centi, r.store_mv, r.vibration_mg, r.state, r.flags, r.reset_count)
    return data + struct.pack('<H', crc16(data))

# D 校验并解析收到的遥测帧
def decode(frame):
    if len(frame) != FRAME_SIZE or frame[:2] != MAGIC:
        raise ValueError('帧长度或帧头错误')
    if crc16(frame[:-2]) != struct.unpack('<H', frame[-2:])[0]:
        raise ValueError('CRC错误')
    magic, version, *fields = BODY.unpack(frame[:-2])
    if version != 1:
        raise ValueError('不支持的协议版本')
    r = Record(*fields)
    if r.state not in (0, 1, 2) or r.flags & ~15:
        raise ValueError('状态或标志无效')
    if r.flags & 1 and not -5500 <= r.temp_centi <= 12500:
        raise ValueError('有效温度超出DS18B20范围')
    if r.flags & 4 and not 0 <= r.store_mv <= 5500:
        raise ValueError('有效储能电压超出本方案范围')
    return r

# E 为已校验记录生成10字节确认帧
def ack_for(record):
    """接收端确认：B5 5B、版本、节点、boot、seq、CRC16，共10字节。"""
    body=struct.pack('<2sBBHH',b'\xb5\x5b',1,record.node,record.boot,record.seq)
    return body+struct.pack('<H',crc16(body))

# F 串口流解析：拼分片、找帧头、坏帧后恢复
class Parser:
    def __init__(self):
        self.buffer = bytearray()
        self.bad_frames = 0

    def feed(self, data):
        self.buffer.extend(data)
        result = []
        while True:
            start = self.buffer.find(MAGIC)
            if start < 0:
                # Keep a possible first byte across fragmented serial reads.
                self.buffer[:] = self.buffer[-1:] if self.buffer.endswith(MAGIC[:1]) else b''
                break
            del self.buffer[:start]
            if len(self.buffer) < FRAME_SIZE:
                break
            try:
                record = decode(bytes(self.buffer[:FRAME_SIZE]))
            except ValueError:
                self.bad_frames += 1
                del self.buffer[0]
            else:
                result.append(record)
                del self.buffer[:FRAME_SIZE]
        return result

# G 旧电压状态策略的Python参考
def policy(previous, mv, valid=True):
    """Reference for the C energy policy, not a physical power model."""
    if not valid or mv < 3650 or previous not in (0, 1, 2):
        return 0
    if previous == 0 and mv < 3800:
        return 0
    return 2 if mv >= 3950 or (previous == 2 and mv >= 3850) else 1
