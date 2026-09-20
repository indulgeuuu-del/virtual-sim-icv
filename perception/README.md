# 视频感知开发副本

本目录保留历史感知题复现成果，当前暂停继续优化，开发聚焦本届驾驶与泊车。当前2027公开规则未发现独立视频计数提交要求，见 [团队共享开发进度](../docs/team/开发路线.md)。

来源为 `code_pre/2025 智能网联汽车-曾熙桐/代码工程/感知题代码/` 的三份 Python 文件。`provenance.json` 记录原始文件 SHA-256；原版保留不改。

开发副本保留原有九类模型接口、方向判断和计数规则，修复已复现问题，并增加独立运行入口：

- NMS（检测框去重）：先将 `[左,上,右,下]` 转为 OpenCV 要求的 `[左,上,宽,高]`。
- 目标关联：同一帧已经分配过的编号不能再分配，IoU 匹配和距离后备匹配均遵守此规则。
- 二次NMS按类别分别去重，避免不同类别互相抑制；距离后备匹配选择阈值内最近的可用同类轨迹。
- 导入不执行推理；模型、视频、输出目录通过参数传入。模型/视频路径须存在，视频无法打开或无可读帧会报错。

## 离线验证

在仓库根运行（只需 NumPy 与 OpenCV）：

```powershell
python -m unittest discover -s tests -p "test_perception.py" -v
python -m perception.main --help
```

这些测试调用真实开发模块，无需模型、视频、SciPy 或仿真平台。它们验证框去重、关联和输入边界，不验证检测精度或整段视频计数准确率。

## 视频运行入口

环境安装、类别映射、完整命令及结果解释见 [下方完整视频推理说明](#完整视频推理说明)。准备好独立环境、本地九类权重和视频后，可从仓库根执行：

```powershell
python -m perception.main --model "路径/best.pt" --video "路径/sample.mp4" --output-dir "runs/sample" --device cpu --headless --save-video
```

`--headless` 无窗口运行，`--save-video` 保存标注视频；`--max-frames 30` 可只处理前30帧试跑。省略 `--headless` 会打开原有 GUI，Esc 提前结束且只统计已处理片段。每次实验必须使用空目录或新目录。入口不包含历史 Gitee 上传逻辑。

输出包括 `traffic_statistics.xlsx`、`track_details.xlsx`、`tracking_diagnostics.json`、`run_metadata.json` 及可选的 `annotated.mp4`。轨迹表包含全部创建过的轨迹，包括丢失和未计数的轨迹；诊断文件记录计数条件检查次数。轨迹数量不等于真实车辆数量。九类英文标签沿用原版，ID6/8 分别对应工程用车/货车；加载时检查模型类别顺序。

目前仍是贪心关联，不保证遮挡、交叉或高密度车流不会换 ID；局部最近匹配也不等于整帧全局最优匹配。计数边界沿用历史规则，真实准确率未验证。程序默认将Ultralytics配置放在项目的 `work/inference/ultralytics`，并启用其离线模式；显式环境变量可覆盖默认值，离线模式不构成网络隔离保证。

## 完整视频推理说明

### 最新复现验收：2026-09-20

以下是历史验收使用的命令和本地资料位置，不是首次安装步骤。新队员先完成下文“独立环境”“模型与视频”“运行”；相同输入文件尚未建立仓库内可访问的共享入口，无法仅靠克隆仓库复现这组709帧结果。

```powershell
.\.venv-inference\Scripts\python.exe -m perception.main --model "code_pre/2025 智能网联汽车-曾熙桐/代码工程/感知题代码/best.pt" --video work/inference/trafic_camera.mp4 --output-dir runs/perception-diagnostic-001 --device cpu --headless --save-video
```

完整709帧再次通过推理，输出视频逐帧解码709帧。轨迹表现在保留269条完整历史，原来只保留结束时的部分轨迹；Counted总和与九类统计表一致。原始三份源码哈希与来源记录一致，运行记录中的开发源码哈希与实测代码一致。

完整环境25项测试通过，新增验证包含四个出口、停留误检、向内运动、行人较长观察窗口及九类乘三方向的27组轨迹。测试把已知位置的检测框送进真实跟踪和计数代码，确认理想输入能只计一次；不经过YOLO，不能证明它能正确识别视频中的目标。

现实公路视频仍是零计数。269条轨迹中226条最终不足10个有效移动点；这里的点数不是帧数，中心位置不变时不会增加点。计数检查有3362次短轨迹、845次位移不足、662次未进入出口区域、7次出口运动不符，没有一次满足全部条件。检查次数按“轨迹×处理帧”累加，包含暂时丢失但仍保留的轨迹，不能相加当车辆数量。唯一进入出口运动检查的轨迹从左下方向画面内部移动，被旧规则拒绝。

看懂诊断顺序：先累计足够移动点，再检查从第二个点到末点至少80像素，随后判断是否靠近画面出口、运动方向是否向外，最后才计数。JSON中的displacement则是首点到末点的距离，不能直接替代第二个点起算的门槛。各类出口边距沿用历史规则。当前没有为了让数字非零而放宽门槛。

结论：完整运行和导出已恢复，历史计数逻辑在上述受控轨迹上通过；比赛识别与计数准确率未复现。画面抽查仍有路牌误判为Bus。交接里的短演示GIF已经绘制了检测框和文字，不能当原始评测视频。现有素材不足以核验九类真实识别、遮挡后保持编号和各方向漏计/重计。

若今后继续感知验证，可用CARLA构造固定俯视相机、单车直行/转弯的可控视频，保存车辆ID和位置作为参考。CARLA车辆资源与九类模型未必一一对应，生成视频通过不能代替比赛原始数据。当前优先推进本届驾驶与泊车，尚未开展这项视频实验。下文初次恢复及同日回归保留为历史记录，以本节为最新状态。

团队当前聚焦本届驾驶与泊车，暂停历史感知题优化。当前保存的2027规则未发现独立视频计数提交要求，见[团队共享开发进度](../docs/team/开发路线.md)。

### 这条程序做什么

视频解码器逐帧读图；YOLO 模型根据权重找出目标位置和类别；程序把相邻帧的目标关联成轨迹；最后根据轨迹方向和是否离开画面边界计数。`vehicle.py` 中的 SciPy 平滑会减小轨迹抖动，pandas/openpyxl 把统计写进 Excel。

“检测框数量”不是“车辆数量”：同一辆车会出现在很多帧中。程序必须保持它的编号，并只计数一次。流程可以跑完，也可能存在漏检、换编号或重复计数，需要人工核对视频才能判断准确率。

### 独立环境

完整推理使用 `.venv-inference`，离线测试的 `.venv` 可以继续保留。两者的 NumPy 版本不同：恢复的 Ultralytics 8.3.107 要求 NumPy 不高于 2.1.1。不要在离线测试环境中混装两个依赖清单。

先按[仿真环境准备](../docs/team/仿真环境准备.md#1-安装carla)安装Python 3.10（运行感知不需要安装CARLA）。以下均在仓库根目录的PowerShell执行，虚拟环境仅首次创建：

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

### 模型与视频

交接权重的 SHA-256：

```text
a8d408a5a33823c3c593d90c55c9e2c1c1da970ee5b67f5440107b9f308ca6af
```

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

### 运行

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

核对模型哈希与上文一致；若复现历史709帧实验，视频哈希也应与下文历史记录一致。自备其他视频只能复现运行流程，不能要求计数结果相同。30帧试跑成功后，在同一个终端完整处理：

```powershell
$fullOutput = 'runs/full-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
.\.venv-inference\Scripts\python.exe -m perception.main --model $modelPath --video $videoPath --output-dir $fullOutput --device cpu --headless --save-video
```

`--headless` 表示不打开窗口，后台逐帧处理；`--save-video` 保存检测框和轨迹，方便回看。省略 `--headless` 会显示窗口，Esc 提前结束，Tab切换标记显示。GUI模式仍需在自己的桌面上验证。输出目录必须为空或不存在，避免覆盖旧实验。

### 看懂结果

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

### 2026-09-20 初次恢复记录

Windows、Python 3.10.5、CPU，新建推理环境通过 `pip check` 和全部 16 项测试。其中一项使用真实 SciPy 平滑与人工构造的直行轨迹，验证一辆车驶出下边界只计数一次；它没有调用模型，不能证明模型准确。

输入来自交接 DeepSort 参考 ZIP 中的 `assets/Videos/trafic_camera.mp4`，为现实公路画面，1280×720、709帧、约30.06FPS，时长约23.6秒。视频SHA-256为 `648317e01eae8a21e6ec5701145d277e5274386de8c379032f8aa1cf3ce01d81`。它不是比赛评测数据，未重新发布视频。

| 项目 | 结果 |
| --- | --- |
| 前30帧试跑 | 模型加载、30帧处理、两张表、标注视频和运行记录成功 |
| 完整视频 | 709/709帧，读取结束，CPU全流程47.199秒 |
| 检测与轨迹 | 707帧有检测，累计1903个框，创建266条轨迹；这些都不是车辆真实数量 |
| 最终计数 | 九类全为0，不能作为有效交通统计结果 |
| 输出核验 | 标注视频逐帧解码709帧；统计表9行，方向分项与总数一致；轨迹表9行 |
| 画面抽查 | 第15帧把道路指示牌识别为大巴，存在明显误检 |

结论：视频到输出文件的技术流程已恢复，真实识别与计数质量尚未恢复。当前样本与比赛画面不同，模型泛化不足是待验证因素；零计数也可能涉及短轨迹、目标关联、边界和方向条件，尚未逐项归因。若后续确有视频计数需求，再记录每条轨迹为何计入/未计入，结合适合的场景视频和人工真值定位问题，不直接凭这个视频调参或重训。

### 同日审查后的回归

按类别抑制重复框、选择最近轨迹及配置目录隔离新增4项测试，完整环境20项通过；轻量环境19项通过、1项因缺SciPy跳过。完整视频再次处理709帧，1925个检测框、269条轨迹、计数仍为0，全流程40.424秒。单次耗时不能证明性能提升，框和轨迹增加也不能证明准确率提升。

该完整回归发生在配置隔离补丁之前，Ultralytics因旧配置不兼容而重建了用户目录中的设置文件；未保存旧文件，不能声称已恢复。入口随后补上项目内默认配置目录。最终代码真实推理5帧通过，未预设YOLO环境变量，用户设置文件运行前后哈希一致，项目内配置存在，运行记录中的源码哈希与当前代码一致。两次回归的标注视频分别逐帧解码709/5帧，两张Excel均可读取，九类统计的方向分项与总数一致。
