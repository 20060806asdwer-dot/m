"""【串口协议压力测试】随机分片、杂音、坏帧、未知版本与确认帧。"""
from pathlib import Path
import random
import struct
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'desktop'))
from protocol import Record,Parser,encode,decode,ack_for,crc16

class ProtocolTest(unittest.TestCase):
    def test_fragmented_corrupt_stream(self):
        rng=random.Random(20261021);expected=[];stream=bytearray()
        for i in range(500):
            record=Record(1,7,i,i*15,rng.randrange(-5500,12501),rng.randrange(0,5501),rng.randrange(65536),2,7)
            frame=encode(record)
            stream.extend(bytes(rng.randrange(0,160) for _ in range(rng.randrange(12))))
            corrupt=bytearray(frame);corrupt[12]^=1;stream.extend(corrupt)
            stream.extend(frame);expected.append(record)
        parser=Parser();actual=[];offset=0
        while offset<len(stream):
            size=rng.randrange(1,74);actual.extend(parser.feed(stream[offset:offset+size]));offset+=size
            self.assertLess(len(parser.buffer),24)
        self.assertEqual(actual,expected)
        self.assertGreaterEqual(parser.bad_frames,500)
    def test_unknown_version(self):
        frame=bytearray(encode(Record(1,1,1,1,2500,4000,1,2,7)));frame[2]=99
        frame[-2:]=struct.pack('<H',crc16(frame[:-2]))
        with self.assertRaises(ValueError):decode(frame)
        parser=Parser();self.assertEqual(parser.feed(frame),[])
    def test_ack_fields_and_crc(self):
        frame=ack_for(Record(1,7,65535,0,0,0,0,0,0))
        self.assertEqual(frame[:8],bytes.fromhex('b55b01010700ffff'))
        self.assertEqual(len(frame),10)
        self.assertEqual(struct.unpack('<H',frame[8:])[0],crc16(frame[:8]))

if __name__=='__main__':unittest.main()
