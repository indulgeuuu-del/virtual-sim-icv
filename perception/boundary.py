import cv2

class FrameBoundary:
    def __init__(self, frame_shape, margin, bias=None, color=(0, 0, 255), thickness=1):
        """
        :param frame_shape: 图像尺寸 (高, 宽)
        :param margin: 所有边界的基础边距
        :param bias: 每条边单独的偏移（dict），如 {'left': -2, 'right': 5, 'top': 0, 'bottom': -3}
        :param color: 边界线颜色
        :param thickness: 线宽
        """
        self.h, self.w = frame_shape[:2]
        self.margin = margin
        self.bias = bias if bias else {}
        self.color = color
        self.thickness = thickness

        # 初始化每条边的最终边距
        self.left_margin = margin + self.bias.get("left", 0)
        self.right_margin = margin + self.bias.get("right", 0)
        self.top_margin = margin + self.bias.get("top", 0)
        self.bottom_margin = margin + self.bias.get("bottom", 0)

    def draw(self, frame):
        """绘制边界线到图像"""
        cv2.line(frame, (self.left_margin, 0), (self.left_margin, self.h - 1), self.color, self.thickness)      # 左
        cv2.line(frame, (self.w - self.right_margin, 0), (self.w - self.right_margin, self.h - 1), self.color, self.thickness)  # 右
        cv2.line(frame, (0, self.top_margin), (self.w - 1, self.top_margin), self.color, self.thickness)         # 上
        cv2.line(frame, (0, self.h - self.bottom_margin), (self.w - 1, self.h - self.bottom_margin), self.color, self.thickness)  # 下
        return frame

    def check_exit(self, cx, cy, vehicle):
        """
        检查车辆是否越界（含bias校正）
        :return: 若越界则返回方向向量，否则 None
        """
        if cx < self.left_margin and vehicle.angleWithLine((0, 1)):
            return (0, 1)  # 从左边界出去
        elif cx > self.w - self.right_margin and vehicle.angleWithLine((0, -1)):
            return (0, -1)  # 从右边界出去
        elif cy < self.top_margin and vehicle.angleWithLine((-1, 0)):
            return (-1, 0)  # 从上边界出去
        elif cy > self.h - self.bottom_margin and vehicle.angleWithLine((1, 0)):
            return (1, 0)  # 从下边界出去
        return None