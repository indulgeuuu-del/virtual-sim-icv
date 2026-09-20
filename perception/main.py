# -*- coding: utf-8 -*-

import cv2  # 用于图像处理和视频操作
import argparse
import hashlib
import json
import platform
import time
from importlib.metadata import version
from pathlib import Path
import numpy as np
if __package__:
    from .vehicle import Vehicle
else:
    from vehicle import Vehicle

# 定义类别名称
CN_NAMES = ['Police', 'Ambulance', 'Tricycle', 'Pedestrian', 'Motorcycle', 'Car', 'Truck', 'Bus', 'Van']
MODEL_NAMES = ['警车', '救护车', '三轮车', '行人', '两轮摩托车', '轿车', '工程用车', '大巴车', '货车']

# 跟踪参数
max_lost = 10                   # 最大丢失帧数
min_track_len = 10              # 最少帧数参与统计
iou_thresh = 0.7                # IOU 判定阈值，防止重复 ID
angle_straight = 15             # 直行判定阈值（角度差度数）
angle_turn = 35                 # 左右转判定阈值（角度差度数）
position_similarity_thresh = 50 # 位置相似性阈值
frameBoundaryMargin = 80

# 初始化统计变量
traffic_participant_counts = {cn: 0 for cn in CN_NAMES}  # 每类目标的计数
traffic_direction_counts = {cn: {'Straight': 0, 'Left': 0, 'Right': 0} for cn in CN_NAMES}  # 每类目标的方向计数

vehicle_tracks = {}
next_vehicle_id = 0

show_marks = True

def drawFrameBoundaryLines(frame, border_margin, color=(0, 0, 255), thickness=1):
    h, w = frame.shape[:2]

    # 左边界线
    cv2.line(frame, (border_margin, 0), (border_margin, h - 1), color, thickness)
    # 右边界线
    cv2.line(frame, (w - border_margin, 0), (w - border_margin, h - 1), color, thickness)
    # 上边界线
    cv2.line(frame, (0, border_margin), (w - 1, border_margin), color, thickness)
    # 下边界线
    cv2.line(frame, (0, h - border_margin), (w - 1, h - border_margin), color, thickness)

    return frame

# 计算两个边界框的 IOU
def bbox_iou(boxA, boxB):
    xA = max(boxA[0], boxB[0])
    yA = max(boxA[1], boxB[1])
    xB = min(boxA[2], boxB[2])
    yB = min(boxA[3], boxB[3])
    interArea = max(0, xB - xA) * max(0, yB - yA)
    boxAArea = (boxA[2] - boxA[0]) * (boxA[3] - boxA[1])
    boxBArea = (boxB[2] - boxB[0]) * (boxB[3] - boxB[1])
    iou = interArea / float(boxAArea + boxBArea - interArea + 1e-6)
    return iou

# 使用 IOU 更新跟踪轨迹，使用 Vehicle 类管理车辆信息
def suppress_boxes(xyxy, confidences):
    """Convert corner coordinates to OpenCV (x, y, width, height) boxes."""
    if len(xyxy) == 0:
        return np.empty(0, dtype=int)
    boxes = [[float(x1), float(y1), float(x2 - x1), float(y2 - y1)]
             for x1, y1, x2, y2 in xyxy]
    indices = cv2.dnn.NMSBoxes(boxes, list(map(float, confidences)), 0.4, 0.8)
    return np.asarray(indices, dtype=int).reshape(-1)


def update_tracks_with_iou(detections):
    global next_vehicle_id
    assigned = set()

    # 标记所有车辆未更新
    for vehicle in vehicle_tracks.values():
        vehicle.updated = False

    for det in detections:
        x1, y1, x2, y2, cls = det
        matched_id = None
        max_iou = 0

        # 尝试用 IOU 匹配已有车辆
        for vid, vehicle in vehicle_tracks.items():
            if vid in assigned or vehicle.cls != cls:
                continue
            if vehicle.last_bbox is not None:
                iou = bbox_iou((x1, y1, x2, y2), vehicle.last_bbox)
                if iou > max_iou:
                    max_iou = iou
                    matched_id = vid

        # 如果 IOU 匹配失败，使用欧氏距离判断轨迹相似性
        if max_iou <= iou_thresh:
            for vid, vehicle in vehicle_tracks.items():
                if vid in assigned or vehicle.cls != cls:
                    continue
                last_pt = vehicle.pts[-1]
                new_pt = (int((x1 + x2) // 2), int((y1 + y2) // 2))
                if np.linalg.norm(np.array(last_pt) - np.array(new_pt)) < position_similarity_thresh:
                    max_iou = 0.8  # 视为强匹配
                    matched_id = vid
                    break

        if max_iou > iou_thresh and matched_id is not None:
            vehicle_tracks[matched_id].update((x1, y1, x2, y2))
            assigned.add(matched_id)
        else:
            # 创建新车辆
            vehicle_tracks[next_vehicle_id] = Vehicle(next_vehicle_id, cls, (x1, y1, x2, y2))
            assigned.add(next_vehicle_id)
            next_vehicle_id += 1

    # 对未更新的车辆标记丢失
    to_delete = []
    for vid, vehicle in vehicle_tracks.items():
        if not vehicle.updated:
            vehicle.mark_lost()
            if vehicle.lost > max_lost:
                to_delete.append(vid)

    for vid in to_delete:
        del vehicle_tracks[vid]


def process_vehicle_tracks(frame, show_window=True):
    for vid, vehicle in vehicle_tracks.items():
        pts_before = vehicle.pts
        pts = vehicle.get_smoothed_pts()

        cls_nm = CN_NAMES[vehicle.cls]
        cx, cy = pts[-1]

        # 判定方向（首尾点角度法）
        direction, dirAngle = vehicle.update_direction()

        # 可视化轨迹中心点和方向
        if show_marks:
            cv2.circle(frame, (int(cx), int(cy)), 4, (0, 255, 0), -1)
            cv2.putText(frame, f'{cls_nm} ID:{vid} ', (int(cx) - 30, int(cy) - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)

        if direction and show_marks:
            cv2.putText(frame, f'{direction} {str(dirAngle)}', (int(cx) + 20, int(cy) + 10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

        # 绘制轨迹线
        for i in range(1, len(pts)):
            if show_marks:
                if not vehicle.finished:
                    cv2.line(frame, (int(pts[i - 1][0]), int(pts[i - 1][1])), (int(pts[i][0]), int(pts[i][1])), (255, 0, 0), 2)
                else:
                    cv2.line(frame, (int(pts[i - 1][0]), int(pts[i - 1][1])), (int(pts[i][0]), int(pts[i][1])), (0, 0, 255), 2)


        if len(pts) < min_track_len or vehicle.finished or vehicle.counted:
            continue

        global frameBoundaryMargin
        if vehicle.cls==3:
            # 对于行人单独划线
            frameBoundaryMargin=40
            drawFrameBoundaryLines(frame, frameBoundaryMargin, (255, 0, 0), 2)
        elif CN_NAMES[vehicle.cls] == 'Bus' or CN_NAMES[vehicle.cls] == 'Van':
            # 对于大巴车单独划线
            frameBoundaryMargin = 160
            drawFrameBoundaryLines(frame, frameBoundaryMargin, (0, 255, 0), 2)
        else:
            frameBoundaryMargin = 100
            drawFrameBoundaryLines(frame, frameBoundaryMargin, (0, 0, 255), 2)

        # 判定轨迹是否结束
        if cx < frameBoundaryMargin and vehicle.angleWithLine((0,1)):
            vehicle.mark_finished()
            traffic_participant_counts[cls_nm] += 1
            traffic_direction_counts[cls_nm][direction] += 1
            vehicle.mark_counted()
            cv2.circle(frame, (int(cx), int(cy)), 24, (0, 0, 255), -1)

        elif cx > frame.shape[1] - frameBoundaryMargin and  vehicle.angleWithLine((0,-1)):
            vehicle.mark_finished()
            traffic_participant_counts[cls_nm] += 1
            traffic_direction_counts[cls_nm][direction] += 1
            vehicle.mark_counted()
            cv2.circle(frame, (int(cx), int(cy)), 24, (0, 0, 255), -1)

        elif cy < frameBoundaryMargin and  vehicle.angleWithLine((-1,0)):
            vehicle.mark_finished()
            traffic_participant_counts[cls_nm] += 1
            traffic_direction_counts[cls_nm][direction] += 1
            vehicle.mark_counted()
            cv2.circle(frame, (int(cx), int(cy)), 24, (0, 0, 255), -1)

        elif cy > frame.shape[0] - frameBoundaryMargin and  vehicle.angleWithLine((1,0)):
            vehicle.mark_finished()
            traffic_participant_counts[cls_nm] += 1
            traffic_direction_counts[cls_nm][direction] += 1
            vehicle.mark_counted()
            cv2.circle(frame, (int(cx), int(cy)), 24, (0, 0, 255), -1)

    # 绘制边界

    # 显示总统计数量
    cv2.putText(frame, f'Total Counted: {sum(traffic_participant_counts.values())}', (30, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
    # 初始化汇总字典
    total_counts = {'Straight': 0, 'Left': 0, 'Right': 0}

    # 累加每个类别的方向计数
    for dirs in traffic_direction_counts.values():
        total_counts['Straight'] += dirs.get('Straight', 0)
        total_counts['Left'] += dirs.get('Left', 0)
        total_counts['Right'] += dirs.get('Right', 0)

    # 打印汇总结果
    cv2.putText(frame, f"Total Straight: {total_counts['Straight']}", (30, 70), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
    cv2.putText(frame, f"Total Left:     {total_counts['Left']}", (30, 110), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
    cv2.putText(frame, f"Total Right:    {total_counts['Right']}", (30, 150), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)

    if show_marks and show_window:
        # 创建纯黑色图像（高度800，宽度1200）
        stateFrame = np.zeros((400, 800, 3), dtype=np.uint8)

        place = 100
        for cn in CN_NAMES:
            cv2.putText(stateFrame,
                        f"label:{cn}, left:{traffic_direction_counts[cn]['Left']}, right:{traffic_direction_counts[cn]['Right']}, Straight:{traffic_direction_counts[cn]['Straight']}",
                        (10, place), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
            place += 30

        # 显示图像
        cv2.namedWindow("Traffic Direction Stats", cv2.WINDOW_NORMAL)
        cv2.imshow("Traffic Direction Stats", stateFrame)

    return frame

def parse_args(argv=None):
    parser = argparse.ArgumentParser(description='视频交通参与者计数（开发版）')
    parser.add_argument('--model', type=Path, required=True, help='本地模型权重')
    parser.add_argument('--video', type=Path, required=True, help='本地输入视频')
    parser.add_argument('--output-dir', type=Path, required=True, help='统计结果目录')
    parser.add_argument('--headless', action='store_true', help='不打开窗口，处理完成后自动退出')
    parser.add_argument('--device', default='cpu', help='推理设备，默认 cpu；加速设备需另行验证')
    parser.add_argument('--max-frames', type=int, help='只处理前N帧用于试跑；省略则处理到视频结束')
    parser.add_argument('--save-video', action='store_true', help='保存带检测框/轨迹的视频 annotated.mp4')
    args = parser.parse_args(argv)
    for name in ('model', 'video'):
        if not getattr(args, name).is_file():
            parser.error(f'{name} 文件不存在: {getattr(args, name)}')
    if args.output_dir.exists() and not args.output_dir.is_dir():
        parser.error('output-dir 必须是目录')
    if args.max_frames is not None and args.max_frames <= 0:
        parser.error('max-frames 必须为正整数')
    if args.output_dir.is_dir() and any(args.output_dir.iterdir()):
        parser.error('output-dir 必须为空目录或尚不存在，避免覆盖既有实验')
    return args


def file_sha256(path):
    digest = hashlib.sha256()
    with open(path, 'rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def validate_model_names(names):
    actual = [names.get(i) for i in range(len(names))] if isinstance(names, dict) else list(names)
    if actual != MODEL_NAMES:
        raise ValueError(f'模型类别与九类交接基线不一致: {actual}')


def main(argv=None):
    global next_vehicle_id, show_marks

    args = parse_args(argv)
    # 仅在真实视频运行时加载推理和导出依赖，导入模块不会启动模型。
    import pandas as pd
    from ultralytics import YOLO
    started = time.perf_counter()
    model_hash = file_sha256(args.model)
    video_hash = file_sha256(args.video)
    cap = cv2.VideoCapture(str(args.video))
    if not cap.isOpened():
        cap.release()
        raise ValueError(f'无法打开视频: {args.video}')
    writer = None
    frames_read = 0
    detections_total = 0
    frames_with_detections = 0
    stop_reason = 'read_end'
    source_fps = cap.get(cv2.CAP_PROP_FPS)
    source_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))

    # 清空数据
    vehicle_tracks.clear()
    next_vehicle_id = 0
    show_marks = True
    for cn in CN_NAMES:
        traffic_participant_counts[cn] = 0
        traffic_direction_counts[cn] = {'Straight': 0, 'Left': 0, 'Right': 0}

    try:
        model = YOLO(str(args.model))
        validate_model_names(model.names)
        while cap.isOpened():
            if args.max_frames is not None and frames_read >= args.max_frames:
                stop_reason = 'frame_limit'
                break
            ret, frame = cap.read()
            if not ret:
                break
            frames_read += 1

            # 模型对这一帧找框；关联和计数仍使用交接算法。
            results = model(frame.copy(), conf=0.4, verbose=False, device=args.device)
            dets = results[0].boxes.cpu().numpy()
            xyxy = dets.xyxy
            confidences = dets.conf
            class_ids = dets.cls.astype(int)
            indices = suppress_boxes(xyxy, confidences)
            detections = []
            for i in indices:
                x1, y1, x2, y2 = xyxy[i]
                cls_id = int(class_ids[i])
                if 0 <= cls_id < len(CN_NAMES):
                    detections.append((x1, y1, x2, y2, cls_id))
                    if show_marks:
                        cv2.rectangle(frame, (int(x1), int(y1)), (int(x2), int(y2)), (0, 255, 255), 2)
            detections_total += len(detections)
            frames_with_detections += bool(detections)
            update_tracks_with_iou(detections)
            frame = process_vehicle_tracks(frame, show_window=not args.headless)

            if args.save_video:
                if writer is None:
                    if not np.isfinite(source_fps) or source_fps <= 0:
                        raise ValueError('视频帧率无效，无法按原帧率保存标注视频')
                    args.output_dir.mkdir(parents=True, exist_ok=True)
                    writer = cv2.VideoWriter(str(args.output_dir / 'annotated.mp4'),
                                             cv2.VideoWriter_fourcc(*'mp4v'), source_fps,
                                             (frame.shape[1], frame.shape[0]))
                    if not writer.isOpened():
                        raise ValueError('无法创建标注视频')
                writer.write(frame)
            if frames_read % 100 == 0:
                print(f'Processed {frames_read}/{source_frames} frames', flush=True)
            if not args.headless:
                cv2.namedWindow('Traffic Statistics', cv2.WINDOW_NORMAL)
                cv2.imshow('Traffic Statistics', frame)
                last_key = cv2.waitKey(1) & 0xFF
                if last_key == 27:
                    stop_reason = 'escape'
                    break
                if last_key == 9:
                    show_marks = not show_marks
    finally:
        cap.release()
        if writer is not None:
            writer.release()
        if not args.headless:
            cv2.destroyAllWindows()
    if frames_read == 0:
        raise ValueError('视频没有可读取帧，未生成计数表')
    if stop_reason == 'read_end' and source_frames > 0 and frames_read < source_frames:
        raise ValueError(f'视频提前停止解码: {frames_read}/{source_frames}，未生成计数表')
    args.output_dir.mkdir(parents=True, exist_ok=True)

    # 保存统计结果到Excel文件
    df = pd.DataFrame({
        'Type': CN_NAMES,
        'Total': [traffic_participant_counts[cn] for cn in CN_NAMES],
        'Straight': [traffic_direction_counts[cn]['Straight'] for cn in CN_NAMES],
        'Left': [traffic_direction_counts[cn]['Left'] for cn in CN_NAMES],
        'Right': [traffic_direction_counts[cn]['Right'] for cn in CN_NAMES]
    })
    df.to_excel(args.output_dir / 'traffic_statistics.xlsx', index=False)
    print('Statistics saved to traffic_statistics.xlsx')

    # 保存详细轨迹信息到Excel文件
    track_infos = []
    for vid, vehicle in vehicle_tracks.items():
        if vehicle.direction:
            track_infos.append({
                'ID': vid,
                'Type': CN_NAMES[vehicle.cls],
                'Direction': vehicle.direction,
                'Track Length': len(vehicle.pts)
            })
    pd.DataFrame(track_infos, columns=['ID', 'Type', 'Direction', 'Track Length']).to_excel(
        args.output_dir / 'track_details.xlsx', index=False)
    print('Track details saved to track_details.xlsx')
    metadata = {
        'model_sha256': model_hash, 'video_sha256': video_hash,
        'source_sha256': {name: file_sha256(Path(__file__).with_name(name))
                          for name in ('main.py', 'vehicle.py')},
        'model_names': model.names, 'output_labels': CN_NAMES,
        'python': platform.python_version(),
        'packages': {name: version(name) for name in
                     ('ultralytics', 'torch', 'torchvision', 'numpy', 'opencv-python',
                      'scipy', 'pandas', 'openpyxl')},
        'device': args.device, 'headless': args.headless, 'max_frames': args.max_frames,
        'source_fps': source_fps, 'source_frames': source_frames,
        'processed_frames': frames_read, 'stop_reason': stop_reason,
        'detections_total': detections_total, 'frames_with_detections': frames_with_detections,
        'tracks_created': next_vehicle_id, 'counted_total': sum(traffic_participant_counts.values()),
        'elapsed_seconds': round(time.perf_counter() - started, 3),
        'parameters': {'confidence': 0.4, 'nms_iou': 0.8, 'max_lost': max_lost,
                       'min_track_len': min_track_len, 'iou_thresh': iou_thresh,
                       'position_similarity_thresh': position_similarity_thresh},
        'track_details_scope': 'active tracks at end, not complete history',
    }
    (args.output_dir / 'run_metadata.json').write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2), encoding='utf-8')
    print(f'Processed {frames_read} frames; counted {metadata["counted_total"]}; '
          f'results: {args.output_dir}', flush=True)

if __name__ == '__main__':
    main()
