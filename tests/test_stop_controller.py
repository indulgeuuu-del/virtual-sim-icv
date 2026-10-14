"""Exercise the actual C++ library through its Python/C ABI; no simulator required."""
import math
import unittest

from tools.competition_stop import StopController


class StopControllerTests(unittest.TestCase):
    def controller(self, duration=0.5, **kwargs):
        obj = StopController(duration, **kwargs)
        self.addCleanup(obj.close)
        return obj

    def test_approach_then_move_then_standstill(self):
        c = self.controller()
        self.assertEqual(c.step(0, 3, False, 2).target_speed, 2)
        self.assertEqual(c.step(0.1, 3, True, 2).target_speed, 0)
        for t in (0.2, 0.3, 0.4, 0.5, 0.6):
            self.assertFalse(c.step(t, 0, True, 2).ready)
        result = c.step(0.7, 0, True, 2)
        # 0.7 - 0.2 can round down: conservative extra sample is permitted.
        if not result.ready:
            result = c.step(0.8, 0, True, 2)
        self.assertTrue(result.ready)
        self.assertEqual(result.target_speed, 0)  # obstacle still blocks departure
        self.assertEqual(c.step(0.9, 0, True, 2, release_allowed=True).target_speed, 2)

    def test_rule_duration_boundaries(self):
        # Scoring PDF p13: dwell <10s fails; p19 ID20: dwell <=5s fails.
        # No claim that this timer alone implements either complete scenario.
        for duration, exclusive in ((10, False), (5, True)):
            with self.subTest(duration=duration, exclusive=exclusive):
                c = self.controller(duration, exclusive=exclusive)
                for tick in range(int(duration * 10)):
                    self.assertFalse(c.step(tick / 10, 0, True, 3).ready)
                self.assertEqual(c.step(duration, 0, True, 3).ready, not exclusive)
                self.assertTrue(c.step(duration + 0.1, 0, True, 3).ready)

    def test_paused_time_does_not_count(self):
        c = self.controller()
        for _ in range(100):
            self.assertEqual(c.step(0, 0, True, 3).held_seconds, 0)
        self.assertFalse(c.step(0.1, 0, True, 3).ready)

    def test_motion_and_leaving_zone_reset_interval(self):
        for speed, zone in ((0.2, True), (0, False), (-0.2, True)):
            with self.subTest(speed=speed, zone=zone):
                c = self.controller()
                c.step(0, 0, True, 3)
                c.step(0.2, 0, True, 3)
                self.assertEqual(c.step(0.3, speed, zone, 3).held_seconds, 0)
                c.step(0.4, 0, True, 3)
                self.assertFalse(c.step(0.6, 0, True, 3).ready)

    def test_missing_invalid_and_discontinuous_samples(self):
        for seconds, speed, valid in ((0.2, 0, False), (0.2, math.nan, True),
                                      (math.inf, 0, True), (-1, 0, True), (2, 0, True), (0.05, 0, True)):
            with self.subTest(seconds=seconds, speed=speed, valid=valid):
                c = self.controller()
                c.step(0, 0, True, 3); c.step(0.1, 0, True, 3)
                value = c.step(seconds, speed, True, 3, valid=valid)
                self.assertEqual((value.phase, value.target_speed, value.ready), ('invalid', 0, False))
                self.assertEqual(c.step(2.1, 0, True, 3).held_seconds, 0)

    def test_independent_instances_and_reset(self):
        a, b = self.controller(), self.controller()
        for i in range(6):
            last = a.step(i / 10, 0, True, 3)
        self.assertTrue(last.ready)
        self.assertFalse(b.step(0.5, 0, True, 3).ready)
        a.reset(0.5)
        self.assertEqual(a.step(0.6, 0, True, 3).held_seconds, 0)

    def test_bad_configuration_and_closed_handle(self):
        for duration in (-1, math.nan, math.inf):
            with self.subTest(duration=duration), self.assertRaises(ValueError):
                StopController(duration)
        c = self.controller(); c.close(); c.close()
        with self.assertRaises(RuntimeError):
            c.step(0, 0, True, 1)


if __name__ == '__main__':
    unittest.main()
