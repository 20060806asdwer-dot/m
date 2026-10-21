"""【20 电脑端测试：序号回卷、日志来源和功耗积分】
流程与文件对应关系见 docs/代码阅读地图.md。
"""
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'desktop'))
from analyze import power,telemetry
from protocol import Record

def row(seq,boot=1):
    return {'source':'COM5','received_at':'2026-10-21T12:00:00',
      **Record(1,boot,seq,seq,2500,4000,10,2,7).__dict__}

class DesktopTests(unittest.TestCase):
    def test_sequence_wrap_and_boot(self):
        result=telemetry([row(65534),row(0),row(0),row(65535),row(7,2)])
        self.assertEqual((result['sequence_gaps'],result['duplicates'],result['old_or_out_of_order']),(1,1,1))
        self.assertEqual(result['observed_node_boot_sessions'],2)
    def test_simulation_rejected(self):
        item=row(0);item['source']='SIMULATED'
        with self.assertRaises(ValueError):telemetry([item])
        self.assertTrue(telemetry([item],True)['simulation'])
    def test_uneven_power_integration(self):
        result=power([{'t_s':0,'v_v':4,'i_ma':1},{'t_s':1,'v_v':4,'i_ma':3},{'t_s':3,'v_v':4,'i_ma':1}])
        self.assertAlmostEqual(result['energy_j'],.024)
        self.assertAlmostEqual(result['average_mw'],8)
    def test_bad_measurements(self):
        for points in ([('0','nan','1'),('1','4','1')],[('0','4','1'),('0','4','1')],[('0','4','-1'),('1','4','1')]):
            with self.assertRaises(ValueError):power([dict(zip(('t_s','v_v','i_ma'),p)) for p in points])

if __name__=='__main__':unittest.main()
