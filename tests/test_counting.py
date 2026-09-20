"""Counting integration with real SciPy, synthetic detections, no model inference."""
import importlib.util
import unittest

import numpy as np
from perception import main as pipeline


@unittest.skipUnless(importlib.util.find_spec('scipy'), 'requires inference environment (SciPy)')
class CountingIntegrationTests(unittest.TestCase):
    def setUp(self):
        pipeline.vehicle_tracks.clear()
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


if __name__ == '__main__':
    unittest.main()
