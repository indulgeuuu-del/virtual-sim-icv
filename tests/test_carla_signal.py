"""CARLA boundary fixtures only; task decisions use the real C++ bridge."""
import math
from types import SimpleNamespace as NS
import unittest

from tools.carla_signal import select_binding, observe, assess_signal
from tools.competition_stopline import SignalStopTask


def waypoint(x, y=0, yaw=0, road=1, lane=1, section=0, z=0):
    return NS(road_id=road,lane_id=lane,section_id=section,transform=NS(
        location=NS(x=x,y=y,z=z),rotation=NS(yaw=yaw),
        get_forward_vector=lambda:NS(x=math.cos(math.radians(yaw)),y=math.sin(math.radians(yaw)))))


class BindingTests(unittest.TestCase):
    def test_nearest_forward_lane_stop_not_nearest_pole(self):
        start=waypoint(0)
        valid=(NS(id=10),waypoint(30))
        bindings=[(NS(id=11),waypoint(1,lane=-1)),(NS(id=12),waypoint(-5)),
                  (NS(id=13),waypoint(60)),valid]
        self.assertIs(select_binding(start,bindings)[0],valid[0])

    def test_wrong_lane_direction_section_or_bad_geometry_rejected(self):
        for kw in (dict(road=2),dict(lane=-1),dict(section=2),dict(yaw=180),
                   dict(y=4),dict(z=1),dict(x=math.nan),dict(yaw=math.nan)):
            with self.subTest(kw=kw):
                args=dict(x=30);args.update(kw)
                self.assertIsNone(select_binding(waypoint(0),[(NS(id=1),waypoint(**args))]))

    def test_absence_or_ambiguous_same_stop_rejected(self):
        self.assertIsNone(select_binding(waypoint(0),[]))
        self.assertIsNone(select_binding(waypoint(0),[(NS(id=1),waypoint(30)),(NS(id=2),waypoint(30.1))]))

    def test_actual_enum_names_not_ordinals(self):
        for name,expected in (('Red',1),('Green',2),('Yellow',3),('Off',0),('Unknown',0),('2',0)):
            self.assertEqual(observe(NS(is_alive=True,get_state=lambda:name)),(expected,expected!=0,name))

    def test_failed_or_missing_lamp_stops_active_cpp_task_then_recovers(self):
        def fail(): raise RuntimeError('actor gone')
        faults=[NS(is_alive=False),NS(is_alive=True,get_state=fail),
                NS(is_alive=True,get_state=lambda:'Off')]
        for lamp in faults:
            c=SignalStopTask();self.addCleanup(c.close)
            def step(t,code,ok=True):return c.step(1,t,2.65,2.43,0,8,.2,code,ok=ok)['target']
            self.assertEqual(step(0,1),0)
            code,ok,_=observe(lamp)
            self.assertEqual(step(.1,code,ok),0)
            self.assertEqual(step(.2,2),8)


class AssessmentTests(unittest.TestCase):
    @staticmethod
    def red_green():
        # Only 1 second stationary before green. Ordinary signal response must
        # not inherit the distinct mandatory stop task's >5s requirement.
        rows=[dict(time=i*.25,speed_mps=0,front_gap_m=.25,rear_gap_m=5,
                   target_mps=0,lamp_state='Red',task_cleared=False) for i in range(5)]
        rows.extend(dict(time=i*.25,speed_mps=0 if i==5 else 2,front_gap_m=.2 if i<8 else -5,
                         rear_gap_m=4 if i<8 else -.2,target_mps=8,lamp_state='Green',
                         task_cleared=i==8) for i in range(5,9))
        return rows

    def test_short_red_stop_and_green_departure_pass(self):
        result=assess_signal(self.red_green(),[])
        self.assertTrue(result['passed']);self.assertEqual(result['departure_delay_s'],.25)

    def test_false_successes_rejected(self):
        for index,change in ((0,dict(front_gap_m=-.01)),(4,dict(front_gap_m=.5001)),
                             (4,dict(speed_mps=.2)),(4,dict(target_mps=1)),
                             (2,dict(lamp_state='Unknown')),(8,dict(task_cleared=False)),
                             (8,dict(rear_gap_m=.01)),(6,dict(time=9)),
                             (2,dict(front_gap_m=math.nan))):
            with self.subTest(change=change):
                rows=self.red_green();rows[index].update(change)
                self.assertFalse(assess_signal(rows,[])['passed'])
        self.assertFalse(assess_signal(self.red_green(),[{'frame':1}])['passed'])
        rows=self.red_green()
        for r in rows:r['speed_mps']=0
        self.assertFalse(assess_signal(rows,[])['passed'])

    def test_distance_and_response_boundaries(self):
        for gap,delay,expected in ((.5,5,True),(.5001,5,False),(.5,5.25,False)):
            rows=self.red_green()[:5];rows[-1]['front_gap_m']=gap
            for i in range(int(delay*4)+1):
                moved=i*.25>=delay
                rows.append(dict(time=1.25+i*.25,speed_mps=2 if moved else 0,
                    front_gap_m=-5 if moved else gap,rear_gap_m=-.2 if moved else 5,
                    target_mps=8,lamp_state='Green',task_cleared=moved))
            self.assertEqual(assess_signal(rows,[])['passed'],expected)

    def test_green_from_start_requires_no_stop(self):
        rows=[dict(time=i*.25,speed_mps=0 if i==0 else 2,front_gap_m=20 if i==0 else -5,
                   rear_gap_m=25 if i==0 else -.2,target_mps=8,lamp_state='Green',
                   task_cleared=i==1) for i in range(2)]
        self.assertTrue(assess_signal(rows,[],True)['passed'])
        rows[1]['target_mps']=0
        self.assertFalse(assess_signal(rows,[],True)['passed'])


class DynamicSignalTests(unittest.TestCase):
    def assess(self,rows,profile='approach-red'):
        from tools.carla_signal import assess_transitions
        return assess_transitions(rows,[],profile)

    @staticmethod
    def rows(stop='Red',gap=.25):
        rows=[dict(time=i*.25,speed_mps=2,front_gap_m=30-i,rear_gap_m=35-i,
                   target_mps=8,lamp_state='Green',task_cleared=False) for i in range(4)]
        rows.extend(dict(time=i*.25,speed_mps=0,front_gap_m=gap,rear_gap_m=gap+5,
                         target_mps=0,lamp_state=stop,task_cleared=False) for i in range(4,9))
        rows.extend(dict(time=i*.25,speed_mps=0 if i==9 else 2,front_gap_m=gap if i<12 else -5,
                         rear_gap_m=gap+5 if i<12 else -.1,target_mps=8,lamp_state='Green',
                         task_cleared=i==12) for i in range(9,13))
        return rows

    def test_moving_green_red_green_and_yellow(self):
        for color,profile in [('Red','approach-red'),('Yellow','approach-yellow')]:
            r=self.assess(self.rows(color),profile)
            self.assertTrue(r['passed']);self.assertEqual(r['departure_delay_s'],.25)

    def test_no_event_or_initial_motion_is_not_success(self):
        rows=self.rows()
        self.assertFalse(self.assess(rows[4:])['passed'])
        for r in rows:r['lamp_state']='Green'
        self.assertFalse(self.assess(rows)['passed'])
        rows=self.rows()
        for r in rows[:4]:r['speed_mps']=0
        self.assertFalse(self.assess(rows)['passed'])

    def test_fault_requires_zero_request_physical_stop_and_recovery(self):
        rows=self.rows('injected_read_failure',gap=20)
        self.assertTrue(self.assess(rows,'read-outage')['passed'])
        for i,change in ((5,dict(target_mps=1)),(8,dict(speed_mps=1)),
                         (6,dict(front_gap_m=-.01)),(12,dict(task_cleared=False)),
                         (10,dict(lamp_state='injected_read_failure'))):
            bad=[dict(r) for r in rows];bad[i].update(change)
            with self.subTest(change=change):
                self.assertFalse(self.assess(bad,'read-outage')['passed'])

    def test_rejected_crossing_distance_or_sample_gap(self):
        for i,change in ((4,dict(front_gap_m=-.01)),(8,dict(front_gap_m=.51)),
                         (3,dict(time=3)),(4,dict(lamp_state='Yellow'))):
            rows=self.rows();rows[i].update(change)
            with self.subTest(change=change):self.assertFalse(self.assess(rows)['passed'])


if __name__=='__main__':
    unittest.main()
