import math
import unittest
from tools.competition_stopline import StopLineController, StopLineSequence, SignalStopTask
from tools.carla_stopline import assess, assess_release, assess_signal_tasks


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


class SignalTaskTests(unittest.TestCase):
    def make(self,dwell=0):
        c=SignalStopTask(dwell);self.addCleanup(c.close);return c

    @staticmethod
    def step(c,t=0,token=10,status=1,**kw):
        args=dict(distance=2.65,front=2.43,speed=0,ceiling=8,margin=.2)
        args.update(kw)
        return c.step(token,t,status=status,**args)

    def test_green_does_not_require_artificial_stop(self):
        c=self.make()
        green=self.step(c,status=2,distance=30)
        self.assertEqual(green['target'],8);self.assertEqual(green['signal'],2)
        self.assertEqual(self.step(c,.1,status=1)['target'],0)
        self.assertEqual(self.step(c,.2,status=2)['target'],8)
        self.assertEqual(self.step(c,.3,status=3)['target'],0)

    def test_unknown_and_disappearance_cannot_release_active_stop(self):
        for fault in (dict(status=0),dict(status=2,ok=False),dict(status=-1),dict(status=2,relevant=False)):
            c=self.make()
            self.step(c)
            result=self.step(c,.1,**fault)
            self.assertEqual(result['target'],0)
            self.assertEqual(result['signal'],3 if fault.get('status')==-1 or fault.get('relevant')==False else 0)
            self.assertEqual(self.step(c,.2,status=2)['target'],8)
        c=self.make()
        self.assertEqual(self.step(c,status=-1)['target'],8)
        self.step(c,.1)
        c.clear() # adapter explicitly confirms it has exited the task
        self.assertEqual(self.step(c,.2,status=-1)['target'],8)

    def test_two_tasks_do_not_share_completed_dwell(self):
        c=self.make(5)
        for i in range(23): self.step(c,i*.25)
        self.assertEqual(self.step(c,5.75,status=2)['target'],8)
        self.assertEqual(self.step(c,6,status=2,token=11)['target'],0)
        for i in range(1,21): self.assertEqual(self.step(c,6+i*.25,status=2,token=11)['target'],0)
        self.assertEqual(self.step(c,11.25,status=2,token=11)['target'],8)

    def test_query_failure_invalidates_completed_hold(self):
        c=self.make(5)
        for i in range(23): self.step(c,i*.25)
        self.assertEqual(self.step(c,5.75,status=2,ok=False)['target'],0)
        r=self.step(c,6,status=2)
        self.assertEqual(r['target'],0);self.assertEqual(r['held'],0)

    def test_both_physical_stops_required(self):
        rows=[]
        for task in (1,2):
            base=(task-1)*8
            rows.extend(dict(time=base+i*.25,speed_mps=0,front_gap_m=.25,target_mps=0,
                release_allowed=False,task_id=task) for i in range(25))
            rows.extend(dict(time=base+i*.25,speed_mps=2,front_gap_m=-5,target_mps=8,
                release_allowed=True,task_id=task) for i in range(25,32))
        self.assertTrue(assess_signal_tasks(rows,[])['passed'])
        self.assertFalse(assess_signal_tasks(rows[:32],[])['passed'])
        bad=[dict(r) for r in rows];bad[34]['front_gap_m']=-.1
        self.assertFalse(assess_signal_tasks(bad,[])['passed'])
        bad=[dict(r) for r in rows];bad[40]['speed_mps']=2
        self.assertFalse(assess_signal_tasks(bad,[])['passed'])
        self.assertFalse(assess_signal_tasks(rows[32:]+rows[:32],[])['passed'])

    def cross_on_green(self,c):
        self.step(c,0,status=2,distance=2.63,speed=2)
        return self.step(c,.2,status=2,distance=2.23,speed=2)

    def test_passed_green_signal_does_not_restop_for_old_lamp(self):
        for fault in (dict(status=1),dict(status=3),dict(status=0),dict(status=-1),dict(status=2,ok=False)):
            c=self.make();self.cross_on_green(c)
            with self.subTest(fault=fault):
                self.assertEqual(self.step(c,.3,distance=2.03,speed=2,**fault)['target'],8)
                self.assertEqual(self.step(c,.4,distance=1.83,speed=2,ceiling=0,**fault)['target'],0)

    def test_negative_distance_alone_does_not_prove_green_passage(self):
        for first in (1,2,0,-1):
            c=self.make()
            self.step(c,0,status=first,distance=2.23,speed=2)
            self.assertEqual(self.step(c,.1,status=1,distance=2.03,speed=2)['target'],0)

    def test_crossing_without_continuous_green_cannot_release(self):
        for fault in (dict(status=1),dict(status=0),dict(status=2,ok=False)):
            c=self.make();self.step(c,0,status=2,distance=2.63,speed=2)
            self.assertEqual(self.step(c,.2,distance=2.23,speed=2,**fault)['target'],0)
            self.assertEqual(self.step(c,.3,status=1,distance=2.03,speed=2)['target'],0)

    def test_passage_does_not_survive_token_clear_or_return_before_line(self):
        for kw in (dict(token=11,distance=2.03),dict(distance=2.63)):
            c=self.make();self.cross_on_green(c)
            self.assertEqual(self.step(c,.3,status=1,speed=2,**kw)['target'],0)
        c=self.make();self.cross_on_green(c);c.clear()
        self.assertEqual(self.step(c,.3,status=1,distance=2.03,speed=2)['target'],0)

    def test_teleport_or_time_gap_cannot_create_passage(self):
        for t,d in ((.3,2.23),(.2,-100),(-.1,2.23)):
            c=self.make();self.step(c,0,status=2,distance=2.63,speed=2)
            self.step(c,t,status=2,distance=d,speed=2)
            self.assertEqual(self.step(c,t+.1,status=1,distance=d-.2,speed=2)['target'],0)

    def test_localization_failure_still_stops_after_passage(self):
        c=self.make();self.cross_on_green(c)
        self.assertEqual(self.step(c,.3,status=2,distance=math.nan,speed=2)['target'],0)

    def test_unknown_first_observation_cannot_turn_absence_into_permission(self):
        c=self.make();self.step(c,0,status=0)
        self.assertEqual(self.step(c,.1,status=-1)['target'],0)

    def test_standstill_geometry_noise_is_not_passage(self):
        c=self.make();self.step(c,0,status=2,distance=2.48,speed=0)
        self.assertFalse(self.step(c,.1,status=2,distance=2.39,speed=0)['passed'])
        self.assertEqual(self.step(c,.2,status=1,distance=2.39,speed=0)['target'],0)

    def test_mandatory_stop_must_finish_before_passage(self):
        c=self.make(5);self.cross_on_green(c)
        self.assertEqual(self.step(c,.3,status=1,distance=2.03,speed=2)['target'],0)
        c=self.make(5)
        for i in range(23): self.step(c,i*.25)
        self.assertEqual(self.step(c,5.75,status=2)['target'],8)
        self.assertTrue(self.step(c,6,status=2,distance=2.23,speed=2)['passed'])
        self.assertEqual(self.step(c,6.1,status=1,distance=2.03,speed=2)['target'],8)

    def test_time_or_geometry_discontinuity_revokes_passage(self):
        for t,d in ((.6,2.03),(.3,-100),(.1,2.03)):
            c=self.make();self.cross_on_green(c)
            r=self.step(c,t,status=2,distance=d,speed=2)
            self.assertEqual(r['target'],0);self.assertFalse(r['passed'])

    def test_line_equality_is_not_already_passed(self):
        c=self.make();self.step(c,0,status=2,distance=2.63,speed=2)
        self.assertFalse(self.step(c,.1,status=2,distance=2.43,speed=2)['passed'])
        self.assertTrue(self.step(c,.2,status=2,distance=2.23,speed=2)['passed'])


if __name__=='__main__':
    unittest.main()
