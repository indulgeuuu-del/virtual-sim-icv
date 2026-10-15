import math
import unittest
from tools.lead_profile import StopGoProfile, BrakingProfile, assess_stop_go, assess_braking


class BrakingProfileTests(unittest.TestCase):
    def samples(self):
        p = BrakingProfile()
        # Synthetic acceptance fixtures, not a physical controller trajectory.
        return p, [dict(elapsed=i/10, speed_mps=p.sample(i/10)[1],
                        gap_m=50 if i/10 <= 3 else 2, formal_target_speed_mps=p.sample(i/10)[1])
                   for i in range(math.ceil(p.end_time*10)+1)]

    def test_rule_stimulus_and_no_restart(self):
        p = BrakingProfile()
        self.assertEqual(p.sample(3), (25, 30/3.6, 'braking'))
        self.assertAlmostEqual(p.sample(4)[1], 30/3.6-2)
        self.assertAlmostEqual(p.sample(100)[0], 25+(30/3.6)**2/4)
        self.assertEqual(p.sample(100)[1:], (0, 'holding'))
        for t in (3, p.stop_time):
            for field in (0, 1):
                self.assertAlmostEqual(p.sample(t-1e-7)[field], p.sample(t+1e-7)[field], places=5)

    def test_invalid_profile_and_time(self):
        for v in (0, -1, math.nan, math.inf):
            with self.assertRaises(ValueError):
                BrakingProfile(deceleration=v)
        for t in (-1, math.nan, math.inf):
            with self.assertRaises(ValueError):
                BrakingProfile().sample(t)

    def test_valid_stop_and_cruise(self):
        p, rows = self.samples()
        self.assertTrue(assess_braking(rows, p)['passed'])
        for gap in (1.5, 2.5):
            changed = [dict(r, gap_m=gap) if r['elapsed'] > 3 else r for r in rows]
            self.assertTrue(assess_braking(changed, p)['passed'])

    def test_stop_does_not_hide_invalid_cruise(self):
        p, rows = self.samples()
        for field, value in (('speed_mps', p.speed+0.51), ('gap_m', 49.49)):
            bad = [dict(r, **{field: value}) if r['elapsed'] == 2 else r for r in rows]
            result = assess_braking(bad, p)
            self.assertTrue(result['stop_passed'])
            self.assertFalse(result['passed'])
            self.assertFalse(result['cruise_matched'])

    def test_missing_invalid_or_overlapping_frames_fail(self):
        p, rows = self.samples()
        for bad in ([], rows[:-1], rows[10:], rows[:30]+rows[40:], rows[:40]+[rows[39]]+rows[40:],
                    [dict(r, gap_m=-1) if r['elapsed'] == 4 else r for r in rows],
                    [dict(r, speed_mps=math.nan) if r['elapsed'] == 4 else r for r in rows]):
            self.assertFalse(assess_braking(bad, p)['passed'])

    def test_final_stop_requires_valid_gap_speed_and_zero_request(self):
        p, rows = self.samples()
        for field, value in (('speed_mps', 0.11), ('gap_m', 1.49), ('gap_m', 2.51), ('formal_target_speed_mps', 0.01)):
            bad = [dict(r, **{field: value}) if r['elapsed'] >= p.stop_time else r for r in rows]
            self.assertFalse(assess_braking(bad, p)['stop_passed'])
        # A past stop cannot conceal a final restart.
        self.assertFalse(assess_braking(rows[:-1]+[dict(rows[-1], speed_mps=1)], p)['stop_passed'])

    def test_dwell_equality_and_exclusive_boundary(self):
        p, rows = self.samples()
        duration = assess_braking(rows, p)['final_standstill_seconds']
        self.assertTrue(assess_braking(rows, p, dwell_seconds=duration)['passed'])
        self.assertFalse(assess_braking(rows, p, dwell_seconds=duration, exclusive=True)['passed'])


class LeadProfileTests(unittest.TestCase):
    def test_continuous_distance_speed_and_hold(self):
        p = StopGoProfile()
        for t in (10, p.stop_time, p.restart_time, p.restart_time+3):
            left, right = p.sample(t-1e-7), p.sample(t+1e-7)
            self.assertAlmostEqual(left[0], right[0], places=5)
            self.assertAlmostEqual(left[1], right[1], places=5)
        self.assertEqual(p.sample(15), (32.25, 0, 'holding'))
        self.assertEqual(p.sample(0), (0, 3, 'cruise'))

    def samples(self):
        p = StopGoProfile()
        return p, [dict(elapsed=i/10, speed_mps=p.sample(i/10)[1], gap_m=30)
                   for i in range(round(p.end_time*10)+1)]

    def test_stop_and_restart_within_gap_bounds(self):
        p, rows = self.samples()
        self.assertTrue(assess_stop_go(rows, p)['passed'])
        for gap in (10, 50):
            self.assertTrue(assess_stop_go([dict(r, gap_m=gap) for r in rows], p)['passed'])

    def test_incomplete_bad_gap_and_missing_frames_fail(self):
        p, rows = self.samples()
        for bad in (rows[:-1], rows[10:], rows[:150]+rows[160:],
                    [dict(r, gap_m=9.99) for r in rows],
                    [dict(r, gap_m=50.01) for r in rows],
                    [dict(r, speed_mps=3) for r in rows],
                    [dict(r, speed_mps=0) for r in rows]):
            self.assertFalse(assess_stop_go(bad, p)['passed'])

    def test_invalid_input_fails(self):
        for v in (0, -1, math.nan, math.inf):
            with self.assertRaises(ValueError):
                StopGoProfile(speed=v)
        p, rows = self.samples()
        rows[150]['speed_mps'] = math.nan
        self.assertFalse(assess_stop_go(rows, p)['passed'])


if __name__ == '__main__':
    unittest.main()
