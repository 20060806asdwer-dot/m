"""Run without sensors, serial ports, keys or internet: python selftest.py"""
from protocol import Record, Parser, encode, decode, crc16, policy
from deepseek_review import prepare

assert crc16(b'123456789') == 0x29b1
r = Record(1, 7, 65535, 123456, -1234, 3900, 27, 2, 7, 3)
good = encode(r)
assert len(good) == 24 and decode(good) == r
parser = Parser()
assert parser.feed(b'noise\xa5') == []
assert parser.feed(good[1:10]) == []
assert parser.feed(good[10:] + good) == [r, r]
corrupt = bytearray(good)
corrupt[12] ^= 1
assert parser.feed(bytes(corrupt) + good) == [r]
assert parser.bad_frames >= 1
assert parser.feed(b'\x00' * 200000) == [] and len(parser.buffer) <= 1
for invalid in [Record(1, 1, 1, 1, 13000, 3900, 0, 2, 7),
                Record(1, 1, 1, 1, 0, 3900, 0, 3, 7)]:
    try:
        decode(encode(invalid))
    except ValueError:
        pass
    else:
        raise AssertionError('invalid telemetry accepted')
assert decode(encode(Record(1, 1, 1, 1, -32768, 0, 0, 0, 0))).flags == 0
assert policy(2, 3649) == 0
assert policy(0, 3799) == 0 and policy(0, 3800) == 1
assert policy(1, 3949) == 1 and policy(1, 3950) == 2
assert policy(2, 3849) == 1 and policy(2, 3850) == 2
assert policy(2, 4000, False) == 0
assert policy(99, 4000) == 0
assert '问题' in prepare('问题', [])
print('PASS: CRC, negative temperature, framing, fragmentation, corruption recovery, bounds, energy reference boundaries, review dry-run preparation')
print('此脚本检查上位机协议；ARM C测试另见 tests/run_core.py，真实硬件仍需烧录实测。')
