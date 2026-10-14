import math
import unittest
from tools.lead_profile import StopGoProfile, assess_stop_go


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
