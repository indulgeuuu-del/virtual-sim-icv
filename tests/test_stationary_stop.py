import math
import unittest

from tools.carla_stationary_stop import projected_gap, stop_command


class StationaryStopTests(unittest.TestCase):
    def test_body_gap_not_center_distance(self):
        self.assertEqual(projected_gap([(-2, 0, 0), (2, 0, 0)], [(8, 0, 0), (12, 0, 0)], (1, 0, 0)), 6)

    def test_negative_direction_and_overlap(self):
        self.assertEqual(projected_gap([(0, -2, 0), (0, 2, 0)], [(0, -12, 0), (0, -8, 0)], (0, -1, 0)), 6)
        self.assertEqual(projected_gap([(-2, 0, 0), (2, 0, 0)], [(1, 0, 0), (5, 0, 0)], (1, 0, 0)), -1)

    def test_targets_decrease_with_distance(self):
        targets = [stop_command(gap, 2, 8.33, 2)[0] for gap in (60, 20, 10, 3, 2)]
        self.assertEqual(targets, sorted(targets, reverse=True))
        self.assertEqual(targets[-1], 0)

    def test_brakes_when_too_fast_or_inside_stop_distance(self):
        for gap in (3, 2, 1, -1):
            _, throttle, brake, _ = stop_command(gap, 8, 8.33, 2)
            self.assertEqual(throttle, 0)
            self.assertGreater(brake, 0)

    def test_hold_and_exclusive_actuators(self):
        self.assertEqual(stop_command(2.1, 0.1, 3, 2), (0, 0, 1, True))
        for gap in (1, 2, 3, 20, 60):
            for speed in (0, 2, 10):
                _, throttle, brake, _ = stop_command(gap, speed, 3, 2)
                self.assertTrue(0 <= throttle <= 1 and 0 <= brake <= 1)
                self.assertEqual(throttle * brake, 0)

    def test_invalid_data_rejected(self):
        for value in (math.nan, math.inf, -math.inf):
            with self.assertRaises(ValueError):
                stop_command(value, 2, 3, 2)


if __name__ == '__main__':
    unittest.main()
