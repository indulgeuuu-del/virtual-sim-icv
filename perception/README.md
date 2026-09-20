# 视频感知开发副本

来源为 `code_pre/2025 智能网联汽车-曾熙桐/代码工程/感知题代码/` 的三份 Python 文件。`provenance.json` 记录原始文件 SHA-256；原版保留不改。

开发副本保留原有九类模型接口、方向判断和计数规则，修复两个已复现问题，并增加独立运行入口：

- NMS（检测框去重）：先将 `[左,上,右,下]` 转为 OpenCV 要求的 `[左,上,宽,高]`。
- 目标关联：同一帧已经分配过的编号不能再分配，IoU 匹配和距离后备匹配均遵守此规则。
- 导入不执行推理；模型、视频、输出目录通过参数传入。模型/视频路径须存在，视频无法打开或无可读帧会报错。

## 离线验证

在仓库根运行（只需 NumPy 与 OpenCV）：

```powershell
python -m unittest discover -s tests -p "test_perception.py" -v
python -m perception.main --help
```

这些测试调用真实开发模块，无需模型、视频、SciPy 或仿真平台。它们验证框去重、关联和输入边界，不验证检测精度或整段视频计数准确率。

## 视频运行入口

环境安装、类别映射、完整命令及结果解释见 [视频推理运行说明](../docs/team/视频推理运行.md)。准备好独立环境、本地九类权重和视频后，可从仓库根执行：

```powershell
python -m perception.main --model "路径/best.pt" --video "路径/sample.mp4" --output-dir "runs/sample" --device cpu --headless --save-video
```

`--headless` 无窗口运行，`--save-video` 保存标注视频；`--max-frames 30` 可只处理前30帧试跑。省略 `--headless` 会打开原有 GUI，Esc 提前结束且只统计已处理片段。每次实验必须使用空目录或新目录。入口不包含历史 Gitee 上传逻辑。

输出包括 `traffic_statistics.xlsx`、`track_details.xlsx`、`run_metadata.json` 及可选的 `annotated.mp4`。轨迹表沿用原逻辑，只包含结束时尚保留且已有方向的轨迹，不是完整历史明细。九类英文标签仍沿用原版，ID6/8 分别对应权重中的工程用车/货车；加载时检查模型的类别顺序。

目前仍是按输入次序的贪心关联，不保证遮挡、交叉或高密度车流不会换 ID；二次 NMS 也仍沿用原版跨类别处理。需要真实视频评估后再决定是否升级这些策略。
