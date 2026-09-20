import contextlib
import io
import subprocess
import sys
import unittest
from pathlib import Path

from perception import main as pipeline


class PerceptionRegressionTests(unittest.TestCase):
    def setUp(self):
        pipeline.vehicle_tracks.clear()
        pipeline.next_vehicle_id = 0

    def test_disjoint_boxes_survive_nms(self):
        boxes = [[1000, 1000, 1010, 1010], [1015, 1000, 1025, 1010]]
        self.assertEqual(pipeline.suppress_boxes(boxes, [.95, .9]).tolist(), [0, 1])

    def test_duplicate_boxes_keep_higher_confidence(self):
        boxes = [[10, 10, 30, 30], [10, 10, 30, 30]]
        self.assertEqual(pipeline.suppress_boxes(boxes, [.6, .9]).tolist(), [1])

    def test_empty_detections(self):
        self.assertEqual(pipeline.suppress_boxes([], []).tolist(), [])
        pipeline.update_tracks_with_iou([])
        self.assertEqual(pipeline.vehicle_tracks, {})

    def test_iou_cannot_assign_one_track_twice(self):
        pipeline.update_tracks_with_iou([(100, 100, 120, 120, 5)])
        pipeline.update_tracks_with_iou([
            (101, 100, 121, 120, 5), (102, 100, 122, 120, 5)])
        self.assertEqual(len(pipeline.vehicle_tracks), 2)
        self.assertEqual(pipeline.vehicle_tracks[0].last_bbox, (101, 100, 121, 120))

    def test_distance_fallback_cannot_reuse_track(self):
        pipeline.update_tracks_with_iou([(100, 100, 120, 120, 5)])
        pipeline.update_tracks_with_iou([
            (101, 100, 121, 120, 5), (125, 100, 145, 120, 5)])
        self.assertEqual(len(pipeline.vehicle_tracks), 2)
        self.assertEqual(len(pipeline.vehicle_tracks[0].pts), 2)

    def test_new_tracks_are_not_reused_in_same_frame(self):
        pipeline.update_tracks_with_iou([
            (100, 100, 120, 120, 5), (125, 100, 145, 120, 5)])
        self.assertEqual(len(pipeline.vehicle_tracks), 2)

    def test_continuity_and_class_separation(self):
        pipeline.update_tracks_with_iou([(100, 100, 120, 120, 5)])
        pipeline.update_tracks_with_iou([(102, 100, 122, 120, 5)])
        self.assertEqual(list(pipeline.vehicle_tracks), [0])
        pipeline.update_tracks_with_iou([(102, 100, 122, 120, 3)])
        self.assertEqual(len(pipeline.vehicle_tracks), 2)
        self.assertEqual(pipeline.vehicle_tracks[0].lost, 1)

    def test_lost_track_expires_at_original_threshold(self):
        pipeline.update_tracks_with_iou([(100, 100, 120, 120, 5)])
        for _ in range(pipeline.max_lost):
            pipeline.update_tracks_with_iou([])
        self.assertIn(0, pipeline.vehicle_tracks)
        pipeline.update_tracks_with_iou([])
        self.assertNotIn(0, pipeline.vehicle_tracks)

    def test_missing_input_is_rejected(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                pipeline.parse_args(['--model', 'nonexistent-test-model.pt',
                                     '--video', 'nonexistent-test-video.mp4',
                                     '--output-dir', 'runs/test'])
        self.assertEqual(raised.exception.code, 2)

    def test_import_does_not_load_inference_stack(self):
        code = ('import sys; import perception.main; '
                'assert "ultralytics" not in sys.modules; '
                'assert "pandas" not in sys.modules; '
                'assert "scipy" not in sys.modules')
        result = subprocess.run([sys.executable, '-c', code],
                                cwd=Path(__file__).resolve().parents[1],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
