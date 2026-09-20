import numpy as np

# 定义类别名称
CN_NAMES = ['Police', 'Ambulance', 'Tricycle', 'Pedestrian', 'Motorcycle', 'Car', 'Truck', 'Bus', 'Van']

class Vehicle:
    def __init__(self, vid, cls_id, bbox):
        self.id = vid
        self.cls = cls_id
        self.last_bbox = bbox
        self.pts = [self.get_center(bbox)]
        self.lost = 0
        self.counted = False
        self.finished = False
        self.direction = None
        self.updated = True

    def get_center(self, bbox):
        x1, y1, x2, y2 = bbox
        return (int((x1 + x2) // 2), int((y1 + y2) // 2))

    def update(self, bbox):
        v=np.array([self.get_center(bbox)[0]-self.get_center(self.last_bbox)[0],self.get_center(bbox)[1]-self.get_center(self.last_bbox)[1]])
        if np.linalg.norm(v)>1e-3:
            self.pts.append(self.get_center(bbox))
        self.last_bbox = bbox
        self.lost = 0
        self.updated = True

    def mark_lost(self):
        self.lost+=1
        self.updated=False

    def mark_finished(self):
        self.finished = True

    def mark_counted(self):
        self.counted = True

    def get_smoothed_pts(self, sigma=2):
        from scipy.ndimage import gaussian_filter1d
        pts_array = np.array(self.pts, dtype=np.float32)
        if len(pts_array) < 3:
            return self.pts
        smooth_x = gaussian_filter1d(pts_array[:, 0], sigma, mode='nearest')
        smooth_y = gaussian_filter1d(pts_array[:, 1], sigma, mode='nearest')
        return np.stack((smooth_x, smooth_y), axis=1).tolist()

    def update_direction(self):
        pts = self.get_smoothed_pts()
        if len(pts) < 3:
            self.direction = "Straight"
            return "Straight", 0.0
        p0 = np.array(pts[len(pts) // 2])
        p1 = np.array(pts[0])
        p2 = np.array(pts[-1])
        v1 = p1 - p0
        v2 = p2 - p0
        norm1 = np.dot(v1, v1)
        norm2 = np.dot(v2, v2)
        if norm1 < 1e-6 or norm2 < 1e-6:
            self.direction = "Straight"
            return "Straight", 0.0
        angle = np.degrees(np.arctan2(v2[1], v2[0]) - np.arctan2(v1[1], v1[0]))
        angle = (angle + 360) % 360
        angle = round(angle, 2)
        if 170 <= angle <= 190:
            self.direction = "Straight"
        elif angle < 180:
            self.direction = "Left"
        else:
            self.direction = "Right"
        return self.direction, angle

    def angleWithLine(self, n):
        # 轨迹点不足时，不进行计算
        if len(self.pts) < 10 or np.linalg.norm(np.array(self.pts[-1], dtype=float) - np.array(self.pts[1], dtype=float)) < 80:
            return False

        # 取第 -1 帧和第 -10 帧之间的位移向量 v
        v = np.array(self.pts[-1], dtype=float) - np.array(self.pts[-10], dtype=float)

        # 计算平均速度（像素/帧）
        average_speed = np.linalg.norm(v) / 9
        # print(f"cls:{CN_NAMES[self.cls]},speed:{average_speed}")

        if average_speed < 1 or CN_NAMES[self.cls] == 'Pedestrian':
            # 轨迹点不足时，不进行计算
            if len(self.pts) < 25: return False

            intervals = [(-1, -10), (-6, -15), (-11, -20), (-16, -25)]
            for end, start in intervals:
                v_temp = np.array(self.pts[end], dtype=float) - np.array(self.pts[start], dtype=float)
                vx, vy = v_temp
                nx, ny = n
                dot = vx * nx + vy * ny
                cross_z = vx * ny - vy * nx
                angle_rad = np.arctan2(cross_z, dot)
                angle_deg = np.degrees(angle_rad)
                if angle_deg < 0:
                    angle_deg += 360

                if not (200 < angle_deg < 340):
                    return False
            return True  # 所有段都满足

        # 法向量 n 假定已归一化或不需归一化都可
        vx, vy = v
        nx, ny = n
        # 计算点积和“2D 叉积”（标量形式）
        dot = vx * nx + vy * ny
        cross_z = vx * ny - vy * nx
        # 计算有符号角度：atan2(cross, dot)
        angle_rad = np.arctan2(cross_z, dot)
        angle_deg = np.degrees(angle_rad)
        # 将角度映射到 [0, 360)
        if angle_deg < 0:
            angle_deg += 360

        if 200 < angle_deg < 340:
            return True
        else:
            return  False
