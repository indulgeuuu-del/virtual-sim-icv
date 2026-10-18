import math
import unittest
from tools.competition_stopline import StopLineController, StopLineSequence
from tools.carla_stopline import assess, assess_release


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


class SequenceTests(unittest.TestCase):
    def setUp(self):
        self.c=StopLineSequence(5)
        self.addCleanup(self.c.close)

    def step(self,t,permission=False,**kw):
        args=dict(distance=2.65,front=2.43,speed=0,ceiling=8,margin=.2,
                  permission=permission,observed=True)
        args.update(kw)
        return self.c.step(t,**args)

    def test_hold_then_independent_permission(self):
        self.assertGreater(self.step(0,distance=60)['target'],0)
        for i in range(1,24):
            r=self.step(i*.25)
            self.assertEqual(r['target'],0)
        self.assertTrue(r['ready'])
        self.assertEqual(self.step(6,True)['target'],8)
        self.assertEqual(self.step(6.25,True,distance=-2,speed=2)['target'],8)
        self.assertEqual(self.step(6.5,False,distance=-3,speed=2)['target'],0)

    def test_early_permission_does_not_skip_dwell(self):
        for i in range(21):
            self.assertEqual(self.step(i*.25,True)['target'],0)
        self.assertEqual(self.step(5.25,True)['target'],8)

    def test_missing_observation_and_clock_reset_hold(self):
        for failure in ('observed','time'):
            c=StopLineSequence(5)
            self.addCleanup(c.close);self.c=c
            for i in range(21): self.step(i*.25)
            r=self.step(5.25,True,observed=False) if failure=='observed' else self.step(6,True)
            self.assertEqual(r['target'],0)
            t=5.5 if failure=='observed' else 6.25
            self.assertEqual(self.step(t,True)['held'],0)
            self.assertEqual(self.step(t+.25,True)['target'],0)

    def test_crossed_or_invalid_cannot_release(self):
        for kw in (dict(distance=2),dict(front=math.nan),dict(speed=-1)):
            self.assertEqual(self.step(0,True,**kw)['target'],0)

    def test_drift_after_ready_before_permission_resets_hold(self):
        for i in range(23): self.step(i*.25)
        r=self.step(5.75,False,speed=.2)
        self.assertFalse(r['ready'])
        self.assertEqual(self.step(6,True)['target'],0)

    def test_release_assessment_uses_physical_motion(self):
        rows=[dict(time=i*.1,speed_mps=0,front_gap_m=.25,target_mps=0,release_allowed=False)
              for i in range(62)]
        rows.extend(dict(time=i*.1,speed_mps=1,front_gap_m=-5 if i==70 else .1,
                         target_mps=8,release_allowed=True) for i in range(62,71))
        self.assertTrue(assess_release(rows,[])['passed'])
        still=[dict(r,speed_mps=0,front_gap_m=.25) for r in rows]
        self.assertFalse(assess_release(still,[])['passed'])
        bad=[dict(r) for r in rows];bad[5]['front_gap_m']=-.01
        self.assertFalse(assess_release(bad,[])['passed'])
        bad=[dict(r) for r in rows];bad[-1]['time']+=.5
        self.assertFalse(assess_release(bad,[])['passed'])
        self.assertFalse(assess_release(rows,[{'frame':1}])['passed'])

    def test_departure_five_second_boundary(self):
        # Binary-exact quarter-second samples keep this equality test exact.
        for delay,passed in ((5,True),(5.25,False)):
            rows=[dict(time=i*.25,speed_mps=0,front_gap_m=.25,target_mps=0,
                       release_allowed=False) for i in range(25)]
            for i in range(1,int(delay*4)+2):
                moved=i*.25>=delay+.25
                rows.append(dict(time=6+i*.25,speed_mps=1 if moved else 0,
                    front_gap_m=-5 if moved else .25,target_mps=8,release_allowed=True))
            self.assertEqual(assess_release(rows,[])['passed'],passed)


if __name__=='__main__':
    unittest.main()
