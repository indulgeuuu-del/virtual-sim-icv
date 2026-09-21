# 正式开发指南

本文说明如何使用仓库进行规则驱动的继承开发。日常状态统一查看[团队共享开发进度](开发进度.md)；本文只在开发流程、接口或运行方法变化时更新。

## 首次准备

以下命令使用Windows PowerShell。安装[Git for Windows](https://git-scm.com/download/win)和[Python 3.10.11 Windows 64位版本](https://www.python.org/downloads/release/python-31011/)，保留Python Launcher安装选项。其他系统尚未验证。

在自己选定的父目录打开PowerShell，首次下载：

```powershell
git clone https://github.com/indulgeuuu-del/virtual-sim-icv.git
Set-Location virtual-sim-icv
git status
py -3.10 --version
```

最后一行应显示Python 3.10.x。后续命令均在包含README.md、tools和tests的仓库根目录运行；新开终端时先进入该目录。无需使用相同盘符，也无需激活虚拟环境，按命令直接调用对应解释器即可。

公开仓库可以直接克隆。推送需仓库所有者在GitHub的Settings → Collaborators添加协作者，对方接受邀请。安装包、SDK、模型和视频不包含在克隆结果中。

## 正式开发流程

### 先确认规则和输入输出

从[赛事官网](http://gcxl.edu.cn/new/index.html)的2027届通知取得《命题与运行》《评分与规则》及后续修订。开始某项任务前记录规则版本、评分条款、实际场景名、输入、控制输出和验收条件。命题序号、评分编号与旧代码caseIdx不能直接视作同一编号。

正式开发顺序如下；阶段是否完成只在开发进度中记录：

| 阶段 | 工作与验收 |
| --- | --- |
| 建立基线 | 选定交接版本，在独立开发目录复制必要源码，记录源路径和逐文件哈希；全队使用同一来源 |
| 修复基础问题 | 构造能暴露问题的输入，修复初始化、索引、配置和几何等问题，保留对应回归测试 |
| 接通比赛接口 | 先运行配套SDK官方例程，再验证连续读状态、发控制、正常结束；版本与单位逐项核对 |
| 覆盖赛题能力 | 停车/跟车、车道控制、信号灯/停止线、变道避障、路口让行；泊车独立验证入库、驻停和出库 |
| 按评分优化 | 用实际车型、地图和场景核对距离、灯光、限速、时限；依据真实扣分调参 |
| 综合验证与定版 | 回归已有场景和连续赛道，检查不同初始条件，固定代码、依赖、配置与提交方法 |

在选定基线上复现已有行为，再结合本届要求逐步适配和改进。平台尚不可用时，可先做开发副本、纯算法测试和已确认缺陷的修复；SDK编译和官方评分须等待真实环境验证，不阻碍前述工作。

### 选定继承基线

交付主基线为：

`code_pre/2025 智能网联汽车-曾熙桐/代码工程/【20250725.8 Traj】last version/`

| 模块 | 作用 |
| --- | --- |
| `TrajectoryControl/` | 通常驾驶、跟车、预设策略点 |
| `AVP/` | 自动泊车 |
| `util/` | 地图、路径、控制与第三方公共工具 |

历史目录中的同名last version不能替代上述基线。首次正式开发在独立目录建立副本并附来源记录；保留原件字节及版权。避免一次复制多个历史版本或带入.vs、编译产物、SDK二进制与私人材料。源码混合UTF-8和GB18030，修改前确认目标编码。

检查历史程序先读入口及调用关系，不通过导入或直接运行来“看看会发生什么”；较新感知历史版本含旧Gitee自动上传逻辑。已有感知副本独立使用，见[其使用说明](../../perception/README.md)。

### 每项功能怎样落地

先用一个明确场景说明“输入什么、应该输出什么”，沿现有工程调用链找到对应逻辑，再做最小修改和对应验证。记录坐标系、角度单位、距离定义和帧号，避免同一变量在不同平台含义不同。

主干数据流为：平台车辆/道路/障碍物状态 → 决策与路径 → 目标速度和转向 → 平台执行 → 下一帧反馈。可以共用的算法尽量共用，平台读写单独适配；不为尚未出现的需求提前建设复杂通用框架。

学长主链Speed模式的throttle是m/s目标速度，CARLA的throttle是0～1油门比例，不能直接替换。车型和地图变化后应重新校准制动距离、转向和固定坐标。工程调试阈值不能直接当作官方满分阈值。

验证顺序为离线反例/测试 → 真实SDK编译 → 平台场景 → 官方评分。新修改还要回归受影响的旧场景。保存代码提交号、平台/SDK版本、车型、地图、输入参数、结果及失败原因；不得把假SDK、仿真真值或合成输入的通过写成真实比赛能力。

## 比赛平台与C++开发

[SimOne产品入口](https://www.51sim.com/products/simone)可用于申请试用或高校授权；[国创中心竞赛平台](https://dc.nevc.com.cn:4430/competition/icvsim/srs/)需要对应训练权限。大赛报名账号不代表已获得仿真许可。

接入前需取得匹配的仿真客户端、有效授权、完整SDK（头文件、库、官方示例）及可运行场景。按[官方开发快速开始](https://simone-docs.51sim.com/20_Developer_Manual/01_Developer_Quick_Start/index.html)先运行配套示例，再接入项目代码。

C++开发需安装Visual Studio的“使用C++的桌面开发”工作负载（含MSVC和Windows SDK），并安装配套CMake工具。在仓库根目录检查：

```powershell
py -3.10 tools/check_driving_environment.py
# 取得SDK后再执行以下两行
$simoneSdk = Read-Host '输入本机SimOne SDK根目录（不加引号）'
py -3.10 tools/check_driving_environment.py --sdk-root $simoneSdk
```

报告在work/driving_environment/report.json。编译器小程序通过只证明工具链可用，SDK文件检查也不能证明版本兼容。学长CMakeLists依赖SDK上层工程，不能直接当独立项目构建；构建输出放源码树之外。

安装包、SDK、许可证与账号信息不提交公开仓库。

具体完整构建命令必须来自取得的SDK上层工程，验证后随开发副本提供。不要直接对交接子目录运行通用cmake命令并宣称兼容；SDK文件检查成功也不等于许可可用或比赛工程编译成功。

## CARLA辅助验证

CARLA提供提前验证控制逻辑的独立环境，实验应服务于正式开发。其运行结果不等于官方评分通过。

### 安装客户端和仿真器

本实验使用Python 3.10（64位）和CARLA 0.9.15。先完成本文“首次准备”。仿真器模拟地图与车辆，Python客户端负责读取状态和发送控制，两者都需要安装。机器需具备可运行CARLA的显卡和驱动；首次以低画质运行，是否适用以连接检查和实际实验为准。

从[官方0.9.15发布页](https://github.com/carla-simulator/carla/releases/tag/0.9.15)获取[Windows安装包](https://downloads.carlasim.com/Windows/CARLA_0.9.15.zip)。压缩包约7.8GB，解压后约19.2GB；同时保留两者时建议预留至少35GB空间。完整解压到无空格英文路径，找到CarlaUE4.exe。安装位置由各队员自行选择，不必与其他人相同。

在仓库根目录打开PowerShell，首次创建独立客户端环境（后续运行无需重建）：

```powershell
py -3.10 -m venv work/carla/venv
work/carla/venv/Scripts/python.exe -m pip install carla==0.9.15
work/carla/venv/Scripts/python.exe -m pip check
work/carla/venv/Scripts/python.exe tools/check_carla_environment.py
```

若没有py命令，使用本机Python 3.10解释器的完整路径替代py -3.10。不要混用感知模块的虚拟环境。最后一个命令只检查客户端，passed为true不代表仿真器已启动。

### 启动与连接

启动仿真器时运行下面整段命令，按提示输入自己解压出的CarlaUE4.exe完整路径，不加外层引号。该变量仅在当前PowerShell窗口有效；重新打开终端时再次输入即可。如果CARLA已经运行，跳过启动步骤。

```powershell
$carlaExe = Read-Host '输入CarlaUE4.exe的完整路径（不加引号）'
if (-not (Test-Path -LiteralPath $carlaExe -PathType Leaf)) { throw '找不到文件，请检查解压位置' }
& $carlaExe -quality-level=Low -windowed -ResX=800 -ResY=600
```

等待地图加载完成后，在仓库根目录另一个PowerShell窗口执行：

```powershell
work/carla/venv/Scripts/python.exe tools/check_carla_environment.py --connect
```

默认连接127.0.0.1:2000。报告中客户端和服务器版本应一致，continuous_frames_verified与passed应为true。结果写入work/carla/client_check.json。该检查只观察连续帧，不生成车辆或改变地图。

### 单车接口测试

确认没有其他程序使用当前仿真服务器，然后执行：

```powershell
$smokeOutput = 'work/carla/control-smoke-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
work/carla/venv/Scripts/python.exe tools/carla_control_smoke.py --output $smokeOutput
```

命令每次生成新的输出目录，保留实验结果。脚本会重新加载Town01并清空原世界，只能独占服务器运行。

测试生成一辆Model 3及摄像头、碰撞传感器：刹车稳定1秒，以0.25油门加速3秒，再全刹车5秒，转向始终为0。它测试控制接口，不包含车道跟踪或障碍停车策略。

程序以同步模式运行，每次tick推进0.05秒，共180帧。正常结束后销毁本次车辆与传感器并恢复原推进设置，不恢复加载前地图。

| 输出 | 内容 |
| --- | --- |
| report.json | 是否通过、最高/最终车速、位移、碰撞与清理错误 |
| frames.json | 每帧时间、位置、速度、油门和刹车 |
| driving.png | 行驶时的摄像头画面 |

通过条件：最高速度大于1m/s、位移大于1m、末速小于0.1m/s、无碰撞事件、保存画面且清理无报错。退出码0表示这些接口测试条件通过。

停止使用时，先结束控制脚本，再关闭CARLA窗口，释放显存和内存。

### 静止前车自动停车

先按上文启动CARLA并检查连接，然后在仓库根目录运行：

```powershell
work/carla/venv/Scripts/python.exe tools/carla_stationary_stop.py
```

默认初速10km/h、初始车身间距25m、目标停车间距2m。CARLA窗口会显示跟随主车的视角，可看到接近前车、减速和刹停。终端每秒显示实际速度、目标速度与车身间距。脚本结束时移除两车和传感器，因此最终画面请查看保存的stopped.png。

测试30km/h、60m间距：

```powershell
work/carla/venv/Scripts/python.exe tools/carla_stationary_stop.py --speed-kmh 30 --gap 60 --stop-gap 2
```

参数含义：speed-kmh同时指定初速和巡航速度上限；gap是本车车头到前车车尾的净距，stop-gap是目标停车净距，单位均为米。默认timeout为60秒仿真时间。支持1～30km/h、1～5m停车间距、初始间距至少比停车间距多5m且不超过65m；能接受参数不代表该组合一定能安全停车。

输出默认进入work/carla/stationary-stop-时间戳/，每次自动新建目录。可用--output指定空目录；--no-realtime取消实时节奏限制，用于快速回归。实际运行可能因机器性能比实时更慢。

| 输出 | 查看方式 |
| --- | --- |
| report.json | passed为true且进程退出码为0表示实验通过；final.gap_m为实际停车净距，collisions和cleanup_errors应为空 |
| frames.csv | 每行记录当前帧两车距离、车速、目标速度、控制值，以及执行控制后下一帧的距离与车速 |
| start.png、braking.png、stopped.png | 起始、首次明显制动和停车后的摄像头画面；失败可能只留下部分图片 |

验收要求：实际初速与设置相差不超过0.5m/s、初始间距误差不超过0.5m；最终净距在目标±0.5m内且为正，速度低于0.1m/s持续2秒，无碰撞，前车保持静止，横向偏移不超过0.4m，清理无错误。超时、数据异常或驶离直道范围都会失败，并保留已有记录。Ctrl+C会尝试清理并保存失败报告；引擎崩溃或进程被强制结束时可能无法完整保存。

#### 程序如何决定刹车

1. 用同一帧两车的包围盒（包住车身的三维盒子），沿道路方向投影，求车头到车尾的间距，避免把车辆中心距离误当成可用距离。
2. 距离较远时保持巡航速度；接近停车位置时降低目标速度。代码取巡航上限、按1.5m/s²规划减速的速度上限、剩余距离乘0.8三者的最小值，越接近停车位置越慢。
3. 实际速度低于目标时加油，高于目标时刹车。进入最后0.15m且速度低于0.25m/s时锁定刹车，避免停车后再次起步。

对应代码为tools/carla_stationary_stop.py中的projected_gap、stop_command和run。前两者可离线验证：

```powershell
work/carla/venv/Scripts/python.exe -m unittest discover -s tests -p test_stationary_stop.py -v
```

实验准备阶段会设置主车速度以建立初始条件，并冻结前车；正式控制阶段只下发油门/刹车，不强制主车速度或位置。当前限定Town01直道、同向Model 3，转向为0；输入来自CARLA真实状态，不包含视觉识别或通用车道跟踪。低速末段仍可能轻微走停，控制平顺性有待改进。2m是工程调试目标，本实验不计算官方成绩。

脚本需独占服务器。有其他车辆、行人或传感器时拒绝执行；空闲Town01直接复用，其他地图会加载Town01。正常退出恢复原推进设置；切换后的地图不恢复。

### 常见问题

| 现象 | 处理 |
| --- | --- |
| No module named carla | 使用work/carla/venv/Scripts/python.exe，并确认安装了carla==0.9.15 |
| 连接超时 | 确认CARLA已启动、地图加载完成、地址端口正确；首次加载完成后再检查 |
| 能连接但读不到连续帧 | 检查是否有控制程序启用了同步模式却未推进tick，先让原程序恢复推进或正常退出 |
| 客户端与服务器版本不同 | 使用配套0.9.15版本，不混装其他版本客户端 |
| 卡顿或内存不足 | 使用低画质、小窗口，关闭无关占用程序，减少车辆与传感器数量 |
| 脚本被强制终止后场景停住或有残留对象 | 确认其他程序不再使用服务器，再重启CARLA并重新连接 |
| Server is occupied | 先正常退出使用该服务器的其他实验；确认无其他使用者后才重启，避免清除别人的场景 |
| CARLA窗口意外退出 | 本次实验不算通过；重启仿真器、检查连接后重跑。查看本机CarlaUE4崩溃日志，保留失败报告 |

## 同步与提交

开始前用git status检查本地改动，保存好已有工作；工作树干净后同步：

```powershell
git switch main
git pull --ff-only origin main
# 编辑文件，运行对应检查或实验
git diff
$changedFile = Read-Host '输入本次要提交的文件相对路径（不加引号）'
git add -- $changedFile
git diff --cached
git commit -m "说明解决了什么问题"
git pull --rebase origin main
# 若引入队友改动，重新验证受影响部分
git push origin main
```

commit保存本地版本，push发布到GitHub。修改前在团队内说明正在改的文件或接口，减少同时改动同一处造成的冲突；涉及公共接口时说明输入输出变化及影响范围。

pull --rebase会把自己尚未发布的提交接到队友最新提交之后。出现冲突时先停止，核对双方修改意图，保留需要的逻辑；解决后执行git add和git rebase --continue，重新验证再推送。不确定时用git rebase --abort返回同步前状态，与相关队友核对。

推送提示non-fast-forward时，再次同步、解决冲突并验证，不使用--force覆盖队友提交。已发布的错误通过新的修复提交纠正。

## 文档维护

有实质代码进展时更新[团队共享开发进度](开发进度.md)：只记录远端已有代码能力、对应验证和直接影响开发的待办或阻塞。本机安装、资料调研、文档整理不作为开发进度。提交说明记录对应修改，实验记录保存代码版本和参数，方便其他人复现。

公开文档分工固定：根README说明目标与目录，本文说明正式开发和运行方法，开发进度记录动态状态，模块README说明该模块的用法。实验数值和阶段结论不在多份使用文档中重复更新，不新增并行进度表，也不分配成员具体工作方向。

教程以队员从新电脑克隆仓库为前提，写清系统、终端、执行目录、依赖版本、资料来源、首次配置、运行命令、成功标准、结果位置和故障处理。仓库内使用相对路径，个人外部路径集中输入；输出目录每次新建。缺资料或尚未验证跨机器运行时如实说明。

文档整理检查内容、链接和命令语法；算法或接口修改运行对应测试，不能用已有测试通过代替本轮验证。

## 源码与资料共享

code_pre/保存学长交接原件，不直接修改。实质开发在独立副本中进行并记录来源，保留原作者与第三方版权信息。

源码、测试、必要配置和团队文档进入Git。AI记录、原始私人材料、模型、视频、安装包、缓存、实验输出和凭据留在本地，所需大文件通过团队约定的私有位置共享。提交前检查暂存清单，.gitignore不能代替人工核对。

实验记录包含代码版本、平台/SDK/依赖版本、地图、车型、场景、参数、结果与限制；涉及模型或视频时额外记录SHA256和人工真值。不要把账号密码或许可证写入仓库。
