# 历史视频感知模块使用说明

本模块将视频中的检测结果关联成轨迹，并按方向和离场规则计数，独立于驾驶/泊车主链。是否继续开发及已验证结果统一见[团队共享开发进度](../docs/team/开发进度.md)。只有正式赛题或明确验证任务需要时，再使用本模块。

来源为 `code_pre/2025 智能网联汽车-曾熙桐/代码工程/感知题代码/`；[provenance.json](provenance.json)记录原始文件SHA-256。原件保留，开发副本提供显式命令行入口，不包含历史Gitee上传行为。继承类别与计数规则需结合实际数据验证，程序跑完不代表准确率达标。

## 这条程序做什么

视频解码器逐帧读图；YOLO 模型根据权重找出目标位置和类别；程序把相邻帧的目标关联成轨迹；最后根据轨迹方向和是否离开画面边界计数。`vehicle.py` 中的 SciPy 平滑会减小轨迹抖动，pandas/openpyxl 把统计写进 Excel。

“检测框数量”不是“车辆数量”：同一辆车会出现在很多帧中。程序必须保持它的编号，并只计数一次。流程可以跑完，也可能存在漏检、换编号或重复计数，需要人工核对视频才能判断准确率。

## 独立环境

完整推理使用 `.venv-inference`，离线测试的 `.venv` 可以继续保留。两者的 NumPy 版本不同：恢复的 Ultralytics 8.3.107 要求 NumPy 不高于 2.1.1。不要在离线测试环境中混装两个依赖清单。

先按[正式开发指南](../docs/team/README.md#首次准备)安装Python 3.10（运行感知不需要安装CARLA）。以下均在仓库根目录的PowerShell执行，虚拟环境仅首次创建：

```powershell
py -3.10 -m venv .venv-inference
.\.venv-inference\Scripts\python.exe -m pip install -r perception/requirements.txt
.\.venv-inference\Scripts\python.exe -m pip check
.\.venv-inference\Scripts\python.exe -m unittest discover -s tests -p "test_*.py" -v
```

下载慢时可在安装命令添加 `--index-url https://pypi.tuna.tsinghua.edu.cn/simple`。环境文件以 Windows/Python 3.10/CPU 为验证目标；其他系统、GPU加速需单独验证。

`requirements.txt` 引用 `requirements-inference.lock.txt`，固定本次环境的全部运行依赖。主要版本是 Ultralytics 8.3.107、PyTorch 2.7.1、torchvision 0.22.1、NumPy 2.1.1、OpenCV 4.12.0.88、SciPy 1.15.3、pandas 2.2.3、openpyxl 3.1.5。

入口默认将第三方配置放到项目的 `work/inference/ultralytics`，并设置 `YOLO_OFFLINE=true`。已有环境变量会保留；以下是可选的显式设置（不更改系统设置）。离线选项不等于网络隔离：

```powershell
$env:YOLO_CONFIG_DIR = Join-Path (Get-Location) 'work/inference/ultralytics'
$env:YOLO_OFFLINE = 'true'
```

## 模型与视频

交接权重的 SHA-256：

```text
a8d408a5a33823c3c593d90c55c9e2c1c1da970ee5b67f5440107b9f308ca6af
```

参考视频来自交接DeepSort参考ZIP中的 `assets/Videos/trafic_camera.mp4`，1280×720、709帧。SHA-256为 `648317e01eae8a21e6ec5701145d277e5274386de8c379032f8aa1cf3ce01d81`。它是现实公路参考素材，不是官方评测数据。相同模型和视频需要团队私下提供，不能仅靠克隆仓库获得；共享入口的落实状态见开发进度。

权重元数据记录 Ultralytics 8.3.107 和日期 2025-07-23；其他依赖是本次重新搭配并验证的版本，不代表恢复了学长的完整原环境。模型/视频不包含在公开仓库中，团队通过私有共享位置取得文件后核对哈希。只加载来源可信的权重：这类旧 `.pt` 会反序列化 Python 对象。

| ID | 权重类别 | 原程序输出标签 |
| --- | --- | --- |
| 0 | 警车 | Police |
| 1 | 救护车 | Ambulance |
| 2 | 三轮车 | Tricycle |
| 3 | 行人 | Pedestrian |
| 4 | 两轮摩托车 | Motorcycle |
| 5 | 轿车 | Car |
| 6 | 工程用车 | Truck |
| 7 | 大巴车 | Bus |
| 8 | 货车 | Van |

程序会检查完整类别顺序，拒绝直接套用通用 80 类模型或不同类别的权重。输出仍保留历史英文标签，特别注意 ID6/8 的实际中文含义。

## 运行

先取得上述可信模型和待测视频。在仓库根目录的PowerShell运行以下整段，按提示输入各自电脑上的完整文件路径，不加外层引号。变量仅在当前终端有效；换终端后重新输入。

```powershell
$modelPath = Read-Host '输入best.pt的完整路径（不加引号）'
$videoPath = Read-Host '输入待测视频的完整路径（不加引号）'
if (-not (Test-Path -LiteralPath $modelPath -PathType Leaf)) { throw '模型文件不存在' }
if (-not (Test-Path -LiteralPath $videoPath -PathType Leaf)) { throw '视频文件不存在' }
Get-FileHash -LiteralPath $modelPath -Algorithm SHA256
Get-FileHash -LiteralPath $videoPath -Algorithm SHA256
$smokeOutput = 'runs/smoke-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
.\.venv-inference\Scripts\python.exe -m perception.main --model $modelPath --video $videoPath --output-dir $smokeOutput --device cpu --headless --max-frames 30 --save-video
```

核对模型哈希与上文一致；若复现历史709帧实验，视频哈希也应与上文参考视频一致。自备其他视频只能复现运行流程，不能要求计数结果相同。30帧试跑成功后，在同一个终端完整处理：

```powershell
$fullOutput = 'runs/full-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
.\.venv-inference\Scripts\python.exe -m perception.main --model $modelPath --video $videoPath --output-dir $fullOutput --device cpu --headless --save-video
```

`--headless` 表示不打开窗口，后台逐帧处理；`--save-video` 保存检测框和轨迹，方便回看。省略 `--headless` 会显示窗口，Esc 提前结束，Tab切换标记显示。GUI模式仍需在自己的桌面上验证。输出目录必须为空或不存在，避免覆盖旧实验。

## 看懂结果

| 文件 | 用途与限制 |
| --- | --- |
| `traffic_statistics.xlsx` | 九类总数及直行/左转/右转计数，不是准确率报告 |
| `track_details.xlsx` | 全部创建过的轨迹，含未计数和已丢失轨迹；Counted表示是否计入，End Reason表示结束原因 |
| `tracking_diagnostics.json` | 各轨迹首末点、移动点数量、计数状态与条件检查次数 |
| `annotated.mp4` | 可选，带框和轨迹的视频，按输入帧率保存 |
| `run_metadata.json` | 模型/视频/源码哈希、依赖版本、处理帧数、停止原因、检测与计数总量和耗时 |

先看 `processed_frames` 与 `source_frames`。`stop_reason=frame_limit` 或 `escape` 表示主动提前结束；`read_end` 表示读取结束，当输入容器有有效总帧数时程序还会检查是否提前解码失败。总帧数未知时不能仅凭 `read_end` 断言完整视频没有损坏。异常退出产生的部分标注视频不能当作成功结果；成功运行会写出两张表和运行记录。

再看 `frames_with_detections`、`tracks_created`、`counted_total`，分别表示有检测结果的帧数、创建过的轨迹数、满足计数条件的目标数。三者含义不同，不应相等。耗时包含模型加载、视频处理和表格导出，不能直接当作纯模型推理速度。

最后回看标注视频，人工记录每类/每方向的真实数量再比较。二次NMS已按类别分别处理，距离回退选择同类别未分配轨迹中最近的一条；关联整体仍是依赖检测顺序的贪心方法，边界与方向规则也尚未按本届场景校准。

## 仅运行离线检查

不需要模型、视频或CARLA时，可在仓库根目录的PowerShell建立轻量环境。它与完整推理环境依赖不同，不能混装：

```powershell
py -3.10 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r tests/requirements.txt
.\.venv\Scripts\python.exe -m pip check
.\.venv\Scripts\python.exe -m unittest discover -s tests -p test_perception.py -v
.\.venv\Scripts\python.exe -m perception.main --help
```

这些测试验证框去重、目标关联和输入边界，不运行YOLO或衡量视频准确率；需要真实SciPy的计数集成测试请使用上文完整推理环境。日常仅修改对应模块、验证受影响行为，协作流程见正式开发指南。

## 常见问题

| 现象 | 处理 |
| --- | --- |
| 提示缺少模块 | 使用相应虚拟环境中的python，重新核对对应requirements和pip check，不使用系统python混装 |
| 模型或视频不存在 | 检查输入路径和资料共享状态；输入不包含外层引号 |
| 类别校验失败 | 核对九类权重及SHA-256，不替换为任意通用模型 |
| 输出目录已存在或非空 | 重新运行输出目录赋值命令，保留旧结果 |
| 视频有框但计数为零 | 回看轨迹和tracking_diagnostics.json，核对运动点、位移、出口和方向条件；不要为非零结果随意放宽门槛 |
