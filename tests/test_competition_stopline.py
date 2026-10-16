import math
import unittest
from tools.competition_stopline import StopLineController
from tools.carla_stopline import assess


class StopLineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.controller=StopLineController()

    def test_real_braking_and_recovery(self):
        c=self.controller
        self.assertEqual(c.step(60,8,8),8)
        self.assertEqual(c.step(5,3,8),0)
        self.assertEqual(c.step(-1,0,8),0)
        self.assertEqual(c.step(60,8,8),8)

    def test_speed_dependent_braking(self):
        c=self.controller
        self.assertEqual(c.step(7,0,8),8)
        self.assertEqual(c.step(7,10,8),0)

    def test_invalid_state(self):
        for args in [(math.nan,0,8),(20,math.inf,8),(20,-1,8),(20,1,-1)]:
            with self.subTest(args=args),self.assertRaises(ValueError):
                self.controller.step(*args)
        self.assertEqual(self.controller.dll.stopline_step(math.nan,0,8,0.2),0)

    @staticmethod
    def rows(seconds=6,gap=0.25):
        return [dict(time=i/10,speed_mps=0,front_gap_m=gap,target_mps=0) for i in range(int(seconds*10)+1)]

    def test_distance_and_time_boundaries(self):
        self.assertTrue(assess(self.rows(gap=0.5),[])['passed'])
        self.assertFalse(assess(self.rows(gap=0.501),[])['passed'])
        self.assertFalse(assess(self.rows(seconds=5),[])['passed'])
        self.assertTrue(assess(self.rows(seconds=5.1),[])['passed'])

    def test_crossing_collision_and_gap(self):
        rows=self.rows();rows[0]['front_gap_m']=-0.01
        self.assertFalse(assess(rows,[])['passed'])
        self.assertFalse(assess(self.rows(),[{'frame':1}])['passed'])
        rows=self.rows();rows.pop(10);rows.pop(10)
        self.assertFalse(assess(rows,[])['passed'])
        rows=self.rows();rows[0]['time']=math.nan
        self.assertFalse(assess(rows,[])['passed'])


if __name__=='__main__':
    unittest.main()
