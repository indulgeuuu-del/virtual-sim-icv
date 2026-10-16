import math
import unittest
from types import SimpleNamespace
from unittest.mock import patch
import contextlib
import io

from tools.carla_stationary_stop import (projected_gap, stop_command, observed_dwell, speed_actuators,
                                       reference_to_body_offset, prepare_ego_transform, parse_args, PISpeedActuators)


class StationaryStopTests(unittest.TestCase):
    def test_settling_options_are_bounded_and_do_not_change_defaults(self):
        with patch('sys.argv',['experiment']):
            args=parse_args()
        self.assertEqual((args.speed_actuator,args.settle_seconds,args.gear_mode),('legacy',0,'automatic'))
        for opts in (['--settle-seconds','nan'],['--settle-seconds','-1'],['--settle-seconds','4'],
                     ['--controller','experimental','--speed-actuator','pi']):
            with patch('sys.argv',['experiment',*opts]),contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit): parse_args()

    def test_pi_compensates_persistent_load_and_stop_clears_history(self):
        controller = PISpeedActuators()
        values = [controller.step(8,7.5,0.05)[0] for _ in range(10)]
        self.assertGreater(values[-1], values[0])
        self.assertGreater(controller.step(8,8,0.05)[0],0)
        self.assertEqual(controller.step(0,8,0.05),(0,1))
        self.assertEqual(controller.step(8,8,0.05),(0,0))

    def test_pi_saturation_does_not_store_unbounded_error(self):
        controller = PISpeedActuators()
        for _ in range(1000):
            self.assertEqual(controller.step(30,0,0.05),(0.55,0))
        self.assertEqual(controller.step(3,3,0.05),(0,0))
        self.assertGreater(controller.step(1,3,0.05)[1],0)

    def test_pi_invalid_inputs_clear_history_and_time_step_is_physical(self):
        for bad in ((math.nan,1,0.05),(1,-1,0.05),(1,1,0),(1,1,0.3)):
            c=PISpeedActuators()
            c.step(2,1,0.05)
            with self.assertRaises(ValueError): c.step(*bad)
            self.assertEqual(c.step(1,1,0.05),(0,0))
        a,b=PISpeedActuators(),PISpeedActuators()
        for _ in range(4): a.step(2,1,0.05)
        for _ in range(2): b.step(2,1,0.1)
        self.assertAlmostEqual(a.integral,b.integral)

    def test_lane_center_spawn_removes_offset_and_preserves_height_without_mutation(self):
        # SDK value-object boundary only; construction uses real helper logic.
        carla = SimpleNamespace(Location=lambda **kw: SimpleNamespace(**kw),
                               Rotation=lambda **kw: SimpleNamespace(**kw),
                               Transform=lambda location, rotation: SimpleNamespace(location=location, rotation=rotation))
        for yaw in (-90, 90, 180):
            spawn = carla.Transform(carla.Location(x=-2.42, y=188, z=3.3),
                                    carla.Rotation(pitch=0, yaw=yaw+1, roll=0))
            lane = carla.Transform(carla.Location(x=-2.008, y=187.97, z=3),
                                   carla.Rotation(pitch=2, yaw=yaw, roll=1))
            actual = prepare_ego_transform(carla, spawn, lane, 'lane-center')
            self.assertEqual((actual.location.x, actual.location.y, actual.location.z), (-2.008,187.97,3.3))
            self.assertEqual(vars(actual.rotation), vars(lane.rotation))
            actual.rotation.yaw = 0
            actual.location.x = 99
            self.assertEqual(lane.rotation.yaw, yaw)
            self.assertEqual(spawn.location.x, -2.42)
            self.assertEqual(lane.location.x, -2.008)

    def test_original_map_spawn_remains_available_for_failure_comparison(self):
        original = object()
        self.assertIs(prepare_ego_transform(None, original, None, 'map-spawn'), original)
        with self.assertRaises(ValueError):
            prepare_ego_transform(None, original, None, 'unknown')

    def test_route_selector_requires_complete_pair_and_nonnegative_spawn(self):
        for options in (['--road-id','15'], ['--lane-id','-1'], ['--spawn-index','-1']):
            with patch('sys.argv', ['experiment', *options]), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    parse_args()
                self.assertEqual(error.exception.code, 2)
        with patch('sys.argv', ['experiment','--road-id','15','--lane-id','-1','--spawn-index','224']):
            args = parse_args()
        self.assertEqual((args.road_id,args.lane_id,args.spawn_index,args.spawn_alignment), (15,-1,224,'lane-center'))

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
