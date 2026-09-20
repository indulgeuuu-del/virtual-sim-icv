# 视频感知开发副本

来源为 `code_pre/2025 智能网联汽车-曾熙桐/代码工程/感知题代码/` 的三份 Python 文件。`provenance.json` 记录原始文件 SHA-256；原版保留不改。

本轮保留原有九类模型接口、方向判断和计数规则，仅修复两个已复现问题，并增加独立运行入口：

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

## 后续视频运行入口

`requirements.txt` 是依赖清单，尚不是验证完成的版本锁。准备好独立环境、本地九类权重和视频后，可从仓库根执行：

```powershell
python -m perception.main --model "路径/best.pt" --video "路径/sample.mp4" --output-dir "runs/sample"
```

当前会打开原有 GUI 窗口，按 Esc 提前结束；提前结束只统计已处理片段。请为每次实验使用不同输出目录，同名 Excel 会被覆盖。程序无网络上传步骤；本轮未加载模型或执行完整视频推理。

输出是 `traffic_statistics.xlsx` 和 `track_details.xlsx`。后者沿用原逻辑，只包含结束时尚保留的轨迹，不是完整历史明细。九类英文标签仍沿用原版，ID6/8 分别对应权重中的工程用车/货车。

目前仍是按输入次序的贪心关联，不保证遮挡、交叉或高密度车流不会换 ID；二次 NMS 也仍沿用原版跨类别处理。需要真实视频评估后再决定是否升级这些策略。
