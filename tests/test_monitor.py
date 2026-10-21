"""【电脑接收闭环测试】用假的串口验证真实模式的分片、CRC、ACK和CSV。"""
import csv
from pathlib import Path
import sys
import tempfile
import tkinter as tk
import types
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'desktop'))
import monitor
from protocol import Record,encode,ack_for

class MonitorTest(unittest.TestCase):
    def test_serial_ack_and_csv(self):
        record=Record(1,7,65535,123456,-1234,3900,27,2,7,3)
        good=encode(record);bad=bytearray(good);bad[12]^=1
        chunks=[b'noise'+good[:5],good[5:]+bytes(bad),good]
        writes=[];closed=[]
        class Port:
            def __init__(self,*args,**kwargs):pass
            def read(self,n):return chunks.pop(0) if chunks else b''
            def write(self,data):writes.append(bytes(data));return len(data)
            def close(self):closed.append(True)
        real_tk=tk.Tk
        def hidden_tk():
            root=real_tk();root.withdraw();real_protocol=root.protocol
            def protocol(name,callback=None):
                result=real_protocol(name,callback)
                if name=='WM_DELETE_WINDOW' and callback:root.after(450,callback)
                return result
            root.protocol=protocol;return root
        base=(Path(__file__).resolve().parents[1]/'build/tests').resolve()
        assert base.is_relative_to(Path(__file__).resolve().parents[1])
        base.mkdir(parents=True,exist_ok=True)
        with tempfile.TemporaryDirectory(dir=base) as out:
            with patch.dict(sys.modules,{'serial':types.SimpleNamespace(Serial=Port)}),patch.object(tk,'Tk',hidden_tk),patch.object(sys,'argv',['monitor.py','--port','FAKE_TEST_PORT','--ack','--out-dir',out]):
                monitor.main()
            files=list(Path(out).glob('LIVE_*.csv'));self.assertEqual(len(files),1)
            with files[0].open(encoding='utf-8-sig',newline='') as f:rows=list(csv.DictReader(f))
        self.assertEqual(len(rows),2)
        self.assertTrue(all(r['source']=='FAKE_TEST_PORT' for r in rows))
        self.assertEqual(writes,[ack_for(record),ack_for(record)])
        self.assertTrue(closed)

if __name__=='__main__':unittest.main()
