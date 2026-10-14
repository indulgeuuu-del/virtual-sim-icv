"""Behavior of actual production following functions, with SDK state replaced."""
import math
import unittest
from tools.competition_following import FollowingController, extract


class FollowingTests(unittest.TestCase):
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

    def test_known_moving_target_minimum_speed_is_exposed(self):
        # Diagnostic, not a safety-pass claim: current production has this floor.
        c = self.controller()
        self.assertEqual(c.step(0, 3, [(1, 1, 2, 1, 1)])['target_speed'], c.parameters['followingLimitSpeed'])

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
        a.close(); a.close()
        with self.assertRaises(RuntimeError):
            a.step(0, 0, [])

    def test_changed_extraction_boundary_fails_loudly(self):
        with self.assertRaises(ValueError):
            extract('start start end', 'start', 'end')


if __name__ == '__main__':
    unittest.main()
