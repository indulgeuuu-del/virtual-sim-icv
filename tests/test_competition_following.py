"""Behavior of actual production following functions, with SDK state replaced."""
import math
import unittest
from tools.competition_following import FollowingController, extract


class FollowingTests(unittest.TestCase):
    def test_policy_standstill_at_requested_gap_does_not_restart(self):
        c = self.controller()
        for gap in (1.99, 2.0):
            for speed in (0, 0.01, 0.1):
                result = c.step(0, speed, [(gap+5,0,0,1,1)], static_reference_offset=5,
                                policy=dict(moving_gap_m=50, stopped_gap_m=2, max_speed_mps=8))
                self.assertEqual(result['target_speed'], 0)

    def test_policy_close_lead_classification_toggle_keeps_stop(self):
        c = self.controller()
        policy = dict(moving_gap_m=50, stopped_gap_m=2, max_speed_mps=8)
        for velocity in (0.02, 0.01, 0.009, 0, 0.011, 0.009, 0):
            kind = 0 if velocity < 0.01 else 2  # Same threshold as production Obstacle.
            result = c.step(0, 0, [(7,velocity,kind,1,1)], static_reference_offset=5, policy=policy)
            self.assertEqual(result['target_speed'], 0)
        self.assertGreater(c.step(0, 0, [(55,3,2,1,1)], static_reference_offset=5, policy=policy)['target_speed'], 0)

    def stateful_controller(self):
        c = self.controller()
        # Test nonzero integral gain without modifying repository configuration.
        replacement = c._dll.following_create(-0.2, 0.1, 0, c.parameters['followingLimitSpeed'], c.parameters['caseStop'])
        self.assertTrue(replacement)
        c._dll.following_destroy(c._handle)
        c._handle = replacement
        return c

    def test_policy_resets_history_after_static_or_absent_target(self):
        policy = dict(moving_gap_m=50, stopped_gap_m=2, max_speed_mps=8)
        for interrupted in ([], [(6.9,0,0,1,1)], [(55,3,2,0,1)]):
            c = self.stateful_controller()
            for _ in range(4):
                c.step(0, 3, [(45,3,2,1,1)], static_reference_offset=5, policy=policy)
            c.step(0, 0, interrupted, static_reference_offset=5, policy=policy)
            resumed = c.step(0, 0, [(55,3,2,1,1)], static_reference_offset=5, policy=policy)
            self.assertAlmostEqual(resumed['target_speed'], 3, places=5)

    def test_policy_invalid_settings_reset_native_history(self):
        import ctypes
        from tools.competition_following import _Input, _Output
        c = self.stateful_controller()
        policy = dict(moving_gap_m=50, stopped_gap_m=2, max_speed_mps=8)
        for _ in range(4):
            c.step(0, 3, [(45,3,2,1,1)], static_reference_offset=5, policy=policy)
        data = (_Input*1)(_Input(55,3,2,1,1))
        out = _Output()
        self.assertEqual(c._dll.following_step(c._handle,0,0,data,1,2,1,5,1,50,2,-1,ctypes.byref(out)),1)
        self.assertEqual(out.target,0)
        self.assertAlmostEqual(c.step(0,0,[(55,3,2,1,1)],static_reference_offset=5,policy=policy)['target_speed'],3,places=5)

    def test_policy_zero_ceiling_does_not_accumulate_hidden_history(self):
        c = self.stateful_controller()
        for _ in range(4):
            self.assertEqual(c.step(0,0,[(45,3,2,1,1)], static_reference_offset=5,
                policy=dict(moving_gap_m=50,stopped_gap_m=2,max_speed_mps=0))['target_speed'],0)
        resumed = c.step(0,0,[(55,3,2,1,1)],static_reference_offset=5,
                         policy=dict(moving_gap_m=50,stopped_gap_m=2,max_speed_mps=8))
        self.assertAlmostEqual(resumed['target_speed'],3,places=5)

    def test_policy_valid_empty_list_and_slow_moving_lead_remain_usable(self):
        c = self.controller()
        policy = dict(moving_gap_m=50,stopped_gap_m=2,max_speed_mps=8)
        self.assertEqual(c.step(0,0,[],static_reference_offset=5,policy=policy)['target_speed'],3)
        for speed in (0.01,0.02,0.1,1):
            self.assertAlmostEqual(c.step(0,0,[(55,speed,2,1,1)],static_reference_offset=5,
                                         policy=policy)['target_speed'],speed,places=5)

    def test_bridge_rejected_input_clears_history_before_recovery(self):
        policy = dict(moving_gap_m=50,stopped_gap_m=2,max_speed_mps=8)
        for bad_policy, bad_target in ((dict(policy,max_speed_mps=-1),[(55,3,2,1,1)]),
                                       (policy,[(math.nan,3,2,1,1)])):
            c = self.stateful_controller()
            for _ in range(4):
                c.step(0,3,[(45,3,2,1,1)],static_reference_offset=5,policy=policy)
            with self.assertRaises(ValueError):
                c.step(0,0,bad_target,static_reference_offset=5,policy=bad_policy)
            self.assertAlmostEqual(c.step(0,0,[(55,3,2,1,1)],static_reference_offset=5,policy=policy)['target_speed'],3,places=5)

    def test_reset_is_context_local_and_preserves_gains(self):
        a, b = self.stateful_controller(), self.stateful_controller()
        policy = dict(moving_gap_m=50,stopped_gap_m=2,max_speed_mps=8)
        def prime(c):
            for _ in range(4):
                c.step(0,3,[(45,3,2,1,1)],static_reference_offset=5,policy=policy)
        def observe(c):
            return c.step(0,0,[(55,3,2,1,1)],static_reference_offset=5,policy=policy)['target_speed']
        prime(a); prime(b)
        a.reset()
        self.assertEqual(observe(a),3)
        self.assertEqual(observe(b),7)
        prime(a)
        self.assertEqual(observe(a),7)  # Integral gain was retained, not reset to config defaults.
        a.close()
        with self.assertRaises(RuntimeError):
            a.reset()

    def test_legacy_static_threshold_preserves_float_boundary(self):
        import ctypes
        c = self.controller()
        for speed in (1, 2, 3):
            d0 = ctypes.c_float(0.05272*speed*speed + 0.06714*speed - 0.02188).value
            threshold = ctypes.c_float(d0 + 2).value
            self.assertEqual(c.step(0, speed, [(threshold,0,0,1,1)], static_reference_offset=0)['target_speed'], 0)
            # Selection stores distance as float32; a float64 nextafter rounds back.
            self.assertEqual(c.step(0, speed, [(threshold+1e-6,0,0,1,1)],
                                    static_reference_offset=0)['target_speed'], 3)

    def test_policy_keeps_body_gap_at_lead_speed_across_vehicle_geometries(self):
        policy = dict(moving_gap_m=50, stopped_gap_m=2, max_speed_mps=30/3.6)
        for offset in (4, 4.792, 7):
            c = self.controller()
            result = c.step(100, 30/3.6, [(150+offset, 30/3.6, 2, 1, 1)],
                            static_reference_offset=offset, policy=policy)
            self.assertAlmostEqual(result['target_speed'], 30/3.6, places=5)
            self.assertAlmostEqual(result['selected_s'], 150+offset)

    def test_policy_speed_ceiling_applies_to_moving_static_and_missing_target(self):
        c = self.controller()
        for ceiling in (0, 1, 30/3.6):
            for obstacles in ([], [(100, 0, 0, 1, 1)], [(100, 20, 2, 1, 1)]):
                result = c.step(0, 3, obstacles, static_reference_offset=5,
                    policy=dict(moving_gap_m=50, stopped_gap_m=2, max_speed_mps=ceiling))
                self.assertLessEqual(result['target_speed'], ceiling+1e-6)
                self.assertGreaterEqual(result['target_speed'], 0)

    def test_policy_static_gap_is_explicit_and_independent_of_case_index(self):
        c = self.controller()
        for index in (2, c.parameters['caseStop']):
            for desired, expected in ((2, 3), (5, 0)):
                result = c.step(0, 3, [(9.9, 0, 0, 1, 1)], index,
                    static_reference_offset=5, policy=dict(moving_gap_m=50, stopped_gap_m=desired, max_speed_mps=10))
                self.assertEqual(result['target_speed'], expected)

    def test_policy_deceleration_stop_and_restart(self):
        c = self.controller()
        policy = dict(moving_gap_m=50, stopped_gap_m=2, max_speed_mps=30/3.6)
        for v in (8, 5, 1, 0.1):
            result = c.step(0, 3, [(55, v, 2, 1, 1)], static_reference_offset=5, policy=policy)
            self.assertAlmostEqual(result['target_speed'], v, places=5)
        self.assertEqual(c.step(0, 0, [(6.9, 0, 0, 1, 1)], static_reference_offset=5, policy=policy)['target_speed'], 0)
        self.assertGreater(c.step(0, 0, [(55, 3, 2, 1, 1)], static_reference_offset=5, policy=policy)['target_speed'], 0)

    def test_policy_invalid_parameters_are_rejected(self):
        c = self.controller()
        policy = dict(moving_gap_m=50, stopped_gap_m=2, max_speed_mps=8)
        with self.assertRaises(ValueError):
            c.step(0, 3, [], policy=policy)
        for key in policy:
            for value in (-1, math.nan, math.inf):
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    c.step(0, 3, [], static_reference_offset=5, policy=dict(policy, **{key:value}))
        for key in ('moving_gap_m', 'stopped_gap_m'):
            with self.assertRaises(ValueError):
                c.step(0, 3, [], static_reference_offset=5, policy=dict(policy, **{key:0}))

    def test_native_policy_rejects_invalid_parameters_and_recovers(self):
        # Bypass Python validation: exercise the production C++ protection itself.
        import ctypes
        from tools.competition_following import _Input, _Output
        c = self.controller()
        data = (_Input*1)(_Input(55, 8, 2, 1, 1))
        for offset, moving, stopped, limit in ((-1,50,2,8), (5,0,2,8), (5,50,0,8),
                (5,50,2,-1), (math.nan,50,2,8), (5,math.inf,2,8), (5,50,math.nan,8), (5,50,2,math.inf)):
            out = _Output()
            self.assertEqual(c._dll.following_step(c._handle, 0, 3, data, 1, 2, 1, offset,
                1, moving, stopped, limit, ctypes.byref(out)), 1)
            self.assertEqual(out.target, 0)
        result = c.step(0, 3, [(55,8,2,1,1)], static_reference_offset=5,
                        policy=dict(moving_gap_m=50, stopped_gap_m=2, max_speed_mps=8))
        self.assertEqual(result['target_speed'], 8)

    def test_static_geometry_uses_body_clearance_without_changing_reference_coordinates(self):
        c = self.controller()
        # Same 2.4m body clearance and speed, two different reference geometries.
        for offset in (4.0, 4.792, 7.0):
            with self.subTest(offset=offset):
                lead_s = 100 + 2.4 + offset
                result = c.step(100, 3, [(lead_s, 0, 0, 1, 1)], static_reference_offset=offset)
                self.assertEqual(result['target_speed'], 0)
                self.assertAlmostEqual(result['selected_s'], lead_s)
                self.assertEqual(c.step(100, 3, [(lead_s+10, 0, 0, 1, 1)],
                    static_reference_offset=offset)['target_speed'], 3)
        # The old 4.38046m offset is intentionally preserved by the legacy entry.
        self.assertEqual(c.step(100, 3, [(107.192, 0, 0, 1, 1)])['target_speed'], 3)

    def test_geometry_does_not_change_moving_following_or_legacy_default(self):
        c = self.controller()
        for distance in (1, 6, 8, 30, 60):
            for speed in (0, 2, 8):
                data = [(distance, 0, 0, 1, 1)]
                self.assertEqual(c.step(0, speed, data),
                    c.step(0, speed, data, static_reference_offset=4.38046))
        for offset in (0, 4, 8):
            data = [(45, 2, 2, 1, 1)]
            self.assertEqual(c.step(0, 3, data)['target_speed'],
                             c.step(0, 3, data, static_reference_offset=offset)['target_speed'])

    def test_invalid_explicit_geometry_is_rejected(self):
        c = self.controller()
        for offset in (-1, math.nan, math.inf):
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                c.step(0, 3, [(10, 0, 0, 1, 1)], static_reference_offset=offset)

    def controller(self):
        obj = FollowingController()
        self.addCleanup(obj.close)
        return obj

    def test_static_speed_and_braking_are_production_formula(self):
        c = self.controller()
        self.assertEqual(c.step(100, 3, [])['target_speed'], 3)
        self.assertEqual(c.step(100, 3, [(130, 0, 0, 1, 1)])['target_speed'], 3)
        self.assertEqual(c.step(100, 3, [(106, 0, 0, 1, 1)])['target_speed'], 0)
        # Production index special case, not a 2027 scoring ID.
        self.assertEqual(c.step(100, 3, [(130, 0, 0, 1, 1)], c.parameters['caseStop'])['target_speed'], 0)

    def test_nearest_selection_ignores_rear_and_other_lanes(self):
        c = self.controller()
        for distractor in ((99, 0, 0, 1, 1), (102, 0, 0, 0, 1), (103, 0, 0, 1, 0)):
            with self.subTest(distractor=distractor):
                result = c.step(100, 3, [distractor, (130, 0, 0, 1, 1)])
                self.assertEqual(result['selected_s'], 130)
                self.assertEqual(result['target_speed'], 3)
                self.assertIsNone(c.step(100, 3, [distractor])['selected_s'])

    def test_nearest_front_and_zero_distance_overlap(self):
        c = self.controller()
        self.assertEqual(c.step(100, 3, [(140, 0, 0, 1, 1), (110, 0, 0, 1, 1)])['selected_s'], 110)
        self.assertEqual(c.step(100, 3, [(100, 0, 0, 1, 1)])['target_speed'], 0)

    def test_close_moving_lead_can_request_stop(self):
        # Scoring p17 tables16/17 and p22 table27: collision/short gap means zero.
        # This is a command invariant, not proof of the physical 10m boundary.
        c = self.controller()
        for distance in (1, 10, 20, 30):
            for lead_speed in (0.01, 0.1, 1, 2):
                with self.subTest(distance=distance, lead_speed=lead_speed):
                    self.assertEqual(c.step(0, 3, [(distance, lead_speed, 2, 1, 1)])['target_speed'], 0)

    def test_slow_lead_at_desired_spacing_is_not_sped_up(self):
        c = self.controller()
        for lead_speed in (0.01, 0.1, 1, 2, 3.49, 3.5, 5):
            with self.subTest(lead_speed=lead_speed):
                self.assertAlmostEqual(c.step(0, 3, [(45, lead_speed, 2, 1, 1)])['target_speed'], lead_speed, places=5)

    def test_lead_deceleration_then_restart_is_not_latched(self):
        c = self.controller()
        values = [c.step(0, 3, [(45, v, 2, 1, 1)])['target_speed'] for v in (5, 3, 1, 0.1)]
        self.assertEqual(values, sorted(values, reverse=True))
        self.assertLess(values[-1], 0.11)
        self.assertEqual(c.step(0, 3, [(20, 0.01, 2, 1, 1)])['target_speed'], 0)
        self.assertEqual(c.step(0, 0, [(20, 0, 0, 1, 1)], c.parameters['caseStop'])['target_speed'], 0)
        self.assertGreater(c.step(0, 0, [(50, 3, 2, 1, 1)])['target_speed'], 3.5)

    def test_open_gap_and_regular_following_still_allow_motion(self):
        c = self.controller()
        self.assertGreater(c.step(0, 3, [(60, 5, 2, 1, 1)])['target_speed'], 5)
        self.assertAlmostEqual(c.step(0, 3, [(45, 5, 2, 1, 1)])['target_speed'], 5)

    def test_invalid_observation_is_rejected_at_boundary(self):
        c = self.controller()
        for value in (math.nan, math.inf, -1):
            with self.assertRaises(ValueError):
                c.step(0, value, [])
        with self.assertRaises(ValueError):
            c.step(0, 1, [(math.nan, 0, 0, 1, 1)])
        with self.assertRaises(ValueError):
            c.step(0, 1, [(5, 0, 4, 1, 1)])

    def test_close_is_idempotent(self):
        a = self.controller()
        self.assertEqual(a.build['abi_version'], a._dll.following_abi_version())
        a.close(); a.close()
        with self.assertRaises(RuntimeError):
            a.step(0, 0, [])

    def test_changed_extraction_boundary_fails_loudly(self):
        with self.assertRaises(ValueError):
            extract('start start end', 'start', 'end')


if __name__ == '__main__':
    unittest.main()
