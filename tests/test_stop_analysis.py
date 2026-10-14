"""Trace alignment and attribution checks, independent of CARLA execution."""
import copy
import math
import unittest

from tools.analyze_stationary_stop import analyze_trace, static_model


class StopAnalysisTests(unittest.TestCase):
    def fixture(self, targets=(3, 0, 0)):
        # At zero request: body gap 2.2, reference distance 7.2, speed 2.
        # Model offset 4, braking 2: geometry +1, threshold +0.8,
        # actual subsequent gap reduction 0.5: model residual -1.5.
        gaps, speeds = (2.4, 2.2, 1.8, 1.7), (2, 2, 0.5, 0)
        rows = [dict(time=i*0.1, gap_m=gaps[i], speed_mps=speeds[i],
                     longitudinal_origin_distance_m=gaps[i]+5,
                     target_speed_mps=targets[i], result_time=(i+1)*0.1,
                     result_gap_m=gaps[i+1], result_speed_mps=speeds[i+1]) for i in range(3)]
        final = dict(time=0.3, gap_m=1.7, speed_mps=0)
        model = dict(quadratic=0.5, linear=0, constant=0, reference_offset=4)
        return rows, final, model

    def test_uses_observation_before_command_not_after_it(self):
        result = analyze_trace(*self.fixture(), desired=2)
        self.assertAlmostEqual(result['first_zero_request']['body_gap_m'], 2.2)
        self.assertAlmostEqual(result['post_request_gap_reduction_m'], 0.5)
        self.assertAlmostEqual(result['geometry_mismatch_m'], 1)
        self.assertAlmostEqual(result['threshold_crossing_m'], 0.8)
        self.assertAlmostEqual(result['braking_model_residual_m'], -1.5)
        self.assertAlmostEqual(result['desired_gap_shortfall_m'], 0.3)
        self.assertAlmostEqual(result['decomposition_residual_m'], 0)

    def test_no_stop_request_is_not_a_successful_braking_measurement(self):
        result = analyze_trace(*self.fixture((3, 3, 3)), desired=2)
        self.assertFalse(result['decomposition_available'])
        self.assertEqual(result['stop_request_episodes'], 0)

    def test_restart_prevents_single_stop_attribution(self):
        result = analyze_trace(*self.fixture((0, 3, 0)), desired=2)
        self.assertFalse(result['decomposition_available'])
        self.assertEqual(result['target_restart_count'], 1)
        self.assertEqual(result['stop_request_episodes'], 2)

    def test_missing_frames_and_misaligned_results_are_rejected(self):
        original, final, model = self.fixture()
        for field, value in (('time', 0.15), ('gap_m', 99), ('speed_mps', 99),
                             ('target_speed_mps', math.nan), ('speed_mps', -1)):
            rows = copy.deepcopy(original)
            rows[1][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                analyze_trace(rows, final, model, 2)

    def test_missing_or_mismatched_final_snapshot_is_rejected(self):
        rows, final, model = self.fixture()
        for field in ('time', 'gap_m', 'speed_mps'):
            invalid = dict(final, **{field: math.inf})
            with self.subTest(field=field), self.assertRaises(ValueError):
                analyze_trace(rows, invalid, model, 2)

    def test_extractor_rejects_a_changed_expression(self):
        with self.assertRaises(ValueError):
            static_model('float caculateFollowingSpeed() { /* changed */ } // 计算超车速度')


if __name__ == '__main__':
    unittest.main()
