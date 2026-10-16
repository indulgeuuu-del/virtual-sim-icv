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

    def test_measured_front_changes_legacy_braking(self):
        c=self.controller
        # Same reference point: 4.38m legacy front stops, measured 2.43m does not.
        self.assertEqual(c.step(5,3,8),0)
        self.assertEqual(c.step(5,3,8,front_offset=2.43),8)

    def test_approach_slows_before_full_brake(self):
        c=self.controller
        speeds=[c.step(2.43+gap,3,8,front_offset=2.43,policy='approach')
                for gap in (20,5,1,0.3)]
        self.assertTrue(all(0<b<a<=8 for a,b in zip(speeds,speeds[1:])))
        self.assertLess(speeds[-1],0.1)
        self.assertEqual(c.step(60,0,0,front_offset=2.43,policy='approach'),0)

    def test_approach_capture_and_recovery(self):
        c=self.controller
        for gap in (0.24,0.2,0,-1):
            self.assertEqual(c.step(2.43+gap,0.1,8,front_offset=2.43,policy='approach'),0)
        self.assertGreater(c.step(2.43+0.26,0,8,front_offset=2.43,policy='approach'),0)
        self.assertEqual(c.step(60,0,8,front_offset=2.43,policy='approach'),8)

    def test_explicit_input_failure(self):
        for front in (math.nan,math.inf,-1):
            with self.subTest(front=front),self.assertRaises(ValueError):
                self.controller.step(10,0,8,front_offset=front,policy='approach')
        with self.assertRaises(ValueError):
            self.controller.step(10,0,8,policy='approach')
        for front in (math.nan,math.inf,-1):
            self.assertEqual(self.controller.dll.stopline_explicit(10,0,8,0.2,front,1),0)
        self.assertEqual(self.controller.dll.stopline_explicit(10,0,8,0.2,2.43,2),0)

    def test_geometry_reference_invariance_and_ceiling(self):
        c=self.controller
        for gap in (0.2,0.3,1,5,50):
            for ceiling in (0,0.01,1,8):
                a=c.step(2.43+gap,1,ceiling,front_offset=2.43,policy='approach')
                b=c.step(4.38+gap,1,ceiling,front_offset=4.38,policy='approach')
                self.assertAlmostEqual(a,b)
                self.assertLessEqual(a,ceiling)

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
