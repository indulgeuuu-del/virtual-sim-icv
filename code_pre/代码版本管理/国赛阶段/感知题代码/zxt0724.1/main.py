# -*- coding: utf-8 -*-

import cv2  # 用于图像处理和视频操作
import pandas as pd  # 用于数据处理和保存统计结果
import numpy as np
from ultralytics import YOLO  # 用于加载和使用 YOLOv8 模型
from vehicle import Vehicle  # 车辆类
from boundary import FrameBoundary  # 边界类

# 定义类别名称
CN_NAMES = ['Police', 'Ambulance', 'Tricycle', 'Pedestrian', 'Motorcycle', 'Car', 'Truck', 'Bus', 'Van']

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
            if vehicle.cls != cls:
                continue
            if vehicle.last_bbox is not None:
                iou = bbox_iou((x1, y1, x2, y2), vehicle.last_bbox)
                if iou > max_iou:
                    max_iou = iou
                    matched_id = vid

        # 如果 IOU 匹配失败，使用欧氏距离判断轨迹相似性
        if max_iou <= iou_thresh:
            for vid, vehicle in vehicle_tracks.items():
                if vehicle.cls != cls:
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


def process_vehicle_tracks(frame):
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
            cv2.putText(frame, f'{direction}', (int(cx) + 20, int(cy) + 10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

        # 绘制轨迹线
        for i in range(1, len(pts)):
            if show_marks:
                cv2.line(frame, (int(pts[i - 1][0]), int(pts[i - 1][1])), (int(pts[i][0]), int(pts[i][1])), (255, 0, 0), 2)

        if len(pts) < min_track_len or vehicle.finished or vehicle.counted:
            continue

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
    drawFrameBoundaryLines(frame, frameBoundaryMargin, (0, 0, 255), 2)

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

    return frame

def main():
    global next_vehicle_id, show_marks

    # 模型路径和视频路径
    MODEL_PATH = r'best.pt'
    VIDEO_PATH = r'C:\Users\KrisB\Desktop\testVideo\测试场景9-五车道路口-雨天下午.mp4'

    # 初始化模型和视频
    model = YOLO(MODEL_PATH)
    cap = cv2.VideoCapture(VIDEO_PATH)

    # 清空数据
    vehicle_tracks.clear()
    next_vehicle_id = 0
    for cn in CN_NAMES:
        traffic_participant_counts[cn] = 0
        traffic_direction_counts[cn] = {'Straight': 0, 'Left': 0, 'Right': 0}

    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break

        # 目标检测
        frameYolo = frame.copy()
        results = model(frameYolo, conf=0.4, verbose=False)
        dets = results[0].boxes.cpu().numpy()
        xyxy = np.array([b.xyxy[0] for b in dets])  # shape: (N, 4)
        confidences = np.array([b.conf[0] for b in dets])  # shape: (N,)
        class_ids = np.array([int(b.cls[0]) for b in dets])  # shape: (N,)

        # 非极大抑制处理
        indices = cv2.dnn.NMSBoxes(
            bboxes=xyxy.tolist(),
            scores=confidences.tolist(),
            score_threshold=0.4,#最小置信度门限
            nms_threshold=0.8#NMS的IOU阈值，例如 0.5，表示两个框重叠超过 50% 时去掉较小置信度的
        )
        detections = []
        if len(indices) > 0:
            for i in indices.flatten():
                x1, y1, x2, y2 = xyxy[i]
                cls_id = class_ids[i]
                if 0 <= cls_id < len(CN_NAMES):
                    detections.append((x1, y1, x2, y2, cls_id))
                    if show_marks:
                        # 可视化检测框
                        cv2.rectangle(frame, (int(x1), int(y1)), (int(x2), int(y2)), (0, 255, 255), 2)
                        # 可选：标注类别
                        # cv2.putText(frame, CN_NAMES[cls_id], (int(x1), int(y1) - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 2)

        # 更新车辆轨迹
        update_tracks_with_iou(detections)

        # 处理车辆轨迹、绘制轨迹与方向
        frame = process_vehicle_tracks(frame)

        cv2.namedWindow("Traffic Statistics", cv2.WINDOW_NORMAL)
        cv2.imshow('Traffic Statistics', frame)

        global lastKey
        lastKey = cv2.waitKey(0) & 0xFF
        if lastKey == 27:  # ESC键退出
            break
        elif lastKey == 9:
            show_marks = not show_marks

    cap.release()
    cv2.destroyAllWindows()

    # 保存统计结果到Excel文件
    df = pd.DataFrame({
        'Type': CN_NAMES,
        'Total': [traffic_participant_counts[cn] for cn in CN_NAMES],
        'Straight': [traffic_direction_counts[cn]['Straight'] for cn in CN_NAMES],
        'Left': [traffic_direction_counts[cn]['Left'] for cn in CN_NAMES],
        'Right': [traffic_direction_counts[cn]['Right'] for cn in CN_NAMES]
    })
    df.to_excel('traffic_statistics.xlsx', index=False)
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
    pd.DataFrame(track_infos).to_excel('track_details.xlsx', index=False)
    print('Track details saved to track_details.xlsx')

main()
# if __name__=="__main__":
#     v = (0,1)
#     n=(-1,0)
#     vx, vy = v
#     nx, ny = n
#     # 计算点积和“2D 叉积”（标量形式）
#     dot = vx * nx + vy * ny
#     cross_z = vx * ny - vy * nx
#     # 计算有符号角度：atan2(cross, dot)
#     angle_rad = np.arctan2(cross_z, dot)
#     angle_deg = np.degrees(angle_rad)
#     # 将角度映射到 [0, 360)
#     if angle_deg < 0:
#         angle_deg += 360
#     print(angle_deg)