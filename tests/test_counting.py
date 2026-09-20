"""Counting integration with real SciPy, synthetic detections, no model inference."""
import importlib.util
import unittest

import numpy as np
from perception import main as pipeline


@unittest.skipUnless(importlib.util.find_spec('scipy'), 'requires inference environment (SciPy)')
class CountingIntegrationTests(unittest.TestCase):
    def setUp(self):
        pipeline.vehicle_tracks.clear()
        pipeline.retired_tracks.clear()
        pipeline.next_vehicle_id = 0
        for name in pipeline.CN_NAMES:
            pipeline.traffic_participant_counts[name] = 0
            pipeline.traffic_direction_counts[name] = dict(Straight=0, Left=0, Right=0)

    def test_downward_car_is_counted_once_at_bottom_boundary(self):
        for center_y in range(200, 696, 15):
            pipeline.update_tracks_with_iou([(610, center_y - 20, 670, center_y + 20, 5)])
            pipeline.process_vehicle_tracks(np.zeros((720, 1280, 3), dtype=np.uint8),
                                            show_window=False)
        self.assertEqual(pipeline.next_vehicle_id, 1)
        self.assertEqual(pipeline.traffic_participant_counts['Car'], 1)
        self.assertEqual(pipeline.traffic_direction_counts['Car'],
                         dict(Straight=1, Left=0, Right=0))

    def test_all_four_exits_count_once_and_preserve_expired_history(self):
        for axis, values, fixed in [
                ('x', range(640, 40, -15), 360),
                ('x', range(640, 1240, 15), 360),
                ('y', range(360, 15, -15), 640),
                ('y', range(360, 710, 15), 640)]:
            with self.subTest(axis=axis, first=values[0], last=values[-1]):
                self.setUp()
                for value in values:
                    x, y = (value, fixed) if axis == 'x' else (fixed, value)
                    pipeline.update_tracks_with_iou([(x-20, y-20, x+20, y+20, 5)])
                    pipeline.process_vehicle_tracks(np.zeros((720, 1280, 3), dtype=np.uint8), False)
                self.assertEqual(pipeline.traffic_participant_counts['Car'], 1)
                for _ in range(pipeline.max_lost + 1):
                    pipeline.update_tracks_with_iou([])
                self.assertFalse(pipeline.vehicle_tracks)
                self.assertEqual(len(pipeline.retired_tracks), 1)
                self.assertTrue(pipeline.retired_tracks[0]['counted'])
                self.assertEqual(pipeline.retired_tracks[0]['end_reason'], 'lost')

    def test_stationary_false_positive_is_not_counted(self):
        for _ in range(40):
            pipeline.update_tracks_with_iou([(20, 20, 60, 60, 7)])
            pipeline.process_vehicle_tracks(np.zeros((720, 1280, 3), dtype=np.uint8), False)
        self.assertEqual(pipeline.traffic_participant_counts['Bus'], 0)
        self.assertEqual(pipeline.counting_status(pipeline.vehicle_tracks[0], (720, 1280)),
                         'short_track')

    def test_inward_car_near_edge_is_not_counted(self):
        vehicle = pipeline.Vehicle(0, 5, (0, 0, 10, 10))
        vehicle.pts = [(x, 360) for x in range(-50, 100, 5)]
        self.assertEqual(pipeline.counting_status(vehicle, (720, 1280)), 'exit_motion_rejected')

    def test_pedestrian_uses_longer_motion_window(self):
        for y in range(250, 710, 10):
            pipeline.update_tracks_with_iou([(630, y-10, 650, y+10, 3)])
            pipeline.process_vehicle_tracks(np.zeros((720, 1280, 3), dtype=np.uint8), False)
        self.assertEqual(pipeline.traffic_participant_counts['Pedestrian'], 1)

    def test_nine_classes_and_three_directions(self):
        paths = {
            'Straight': [(640, y) for y in range(200, 716, 10)],
            'Left': ([(640, y) for y in range(200, 361, 10)] +
                     [(x, 360) for x in range(650, 1261, 10)]),
            'Right': ([(640, y) for y in range(200, 361, 10)] +
                      [(x, 360) for x in range(630, 19, -10)]),
        }
        for cls, label in enumerate(pipeline.CN_NAMES):
            for direction, points in paths.items():
                with self.subTest(label=label, direction=direction):
                    self.setUp()
                    for x, y in points:
                        pipeline.update_tracks_with_iou([(x-20, y-20, x+20, y+20, cls)])
                        pipeline.process_vehicle_tracks(np.zeros((720, 1280, 3), dtype=np.uint8), False)
                    self.assertEqual(pipeline.next_vehicle_id, 1)
                    self.assertEqual(pipeline.traffic_participant_counts[label], 1)
                    self.assertEqual(pipeline.traffic_direction_counts[label][direction], 1)


if __name__ == '__main__':
    unittest.main()
