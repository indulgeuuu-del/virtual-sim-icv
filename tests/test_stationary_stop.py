import math
import unittest

from tools.carla_stationary_stop import projected_gap, stop_command, observed_dwell, speed_actuators, reference_to_body_offset


class StationaryStopTests(unittest.TestCase):
    def test_reference_geometry_supports_asymmetric_origins_and_reverse_axis(self):
        for direction in (1, -1):
            ego = [(direction*x, 0, 0) for x in (-1, 3)]
            lead = [(direction*x, 0, 0) for x in (8, 14)]
            axis = (direction, 0, 0)
            offset = reference_to_body_offset(ego, lead, (0, 0, 0), (10*direction, 0, 0), axis)
            self.assertEqual(offset, 5)
            self.assertEqual(10-offset, projected_gap(ego, lead, axis))

    def test_reference_geometry_uses_orientation_not_constant_vehicle_length(self):
        # Lead has rotated 90 degrees: its projected half-width is now 1m.
        ego = [(-2, 0, 0), (2, 0, 0)]
        lead = [(9, -2, 0), (11, 2, 0)]
        self.assertEqual(reference_to_body_offset(ego, lead, (0, 0, 0), (10, 0, 0), (1, 0, 0)), 3)

    def test_invalid_reference_geometry_is_rejected(self):
        for points, axis in (([], (1, 0, 0)), ([(math.nan, 0, 0)], (1, 0, 0)), ([(0, 0, 0)], (2, 0, 0))):
            with self.subTest(points=points, axis=axis), self.assertRaises(ValueError):
                reference_to_body_offset(points, [(10, 0, 0)], (0, 0, 0), (10, 0, 0), axis)

    def test_speed_mode_adapter_only_maps_the_requested_speed(self):
        self.assertEqual(speed_actuators(0, 0), (0, 1))
        self.assertEqual(speed_actuators(0, 5), (0, 1))
        self.assertGreater(speed_actuators(3, 1)[0], 0)
        self.assertGreater(speed_actuators(3, 5)[1], 0)
        for value in (math.nan, math.inf, -1):
            with self.assertRaises(ValueError):
                speed_actuators(value, 1)

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

    def test_dwell_is_measured_from_vehicle_states(self):
        states = [dict(time=i/10, speed_mps=0, gap_m=2.1) for i in range(11)]
        self.assertEqual(observed_dwell(states, 2), 1)
        states[6]['speed_mps'] = 0.2
        self.assertAlmostEqual(observed_dwell(states, 2), 0.3)
        states[-1]['gap_m'] = 3
        self.assertEqual(observed_dwell(states, 2), 0)
        self.assertAlmostEqual(observed_dwell(states, None), 0.3)

    def test_dwell_does_not_count_pauses_or_missing_intervals(self):
        self.assertEqual(observed_dwell([dict(time=1, speed_mps=0, gap_m=2)]*30, 2), 0)
        self.assertEqual(observed_dwell([dict(time=t, speed_mps=0, gap_m=2) for t in (0, 1)], 2), 0)

    def test_dwell_rejects_invalid_vehicle_observations(self):
        for field in ('time', 'speed_mps', 'gap_m'):
            for value in (math.nan, math.inf, -math.inf):
                with self.subTest(field=field, value=value):
                    states = [dict(time=t, speed_mps=0, gap_m=2) for t in (0, 0.1, 0.2)]
                    states[-1][field] = value
                    self.assertEqual(observed_dwell(states, 2), 0)
        self.assertEqual(observed_dwell([dict(time=t, speed_mps=-1, gap_m=2) for t in (0, 0.1)], 2), 0)


if __name__ == '__main__':
    unittest.main()
