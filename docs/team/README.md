# 正式开发指南

本文面向从远端克隆仓库的队员，说明如何配置环境、定位代码、按规则开发和提交修改。当前能力与待办统一见[团队共享开发进度](开发进度.md)。

## 首次准备

使用Windows PowerShell，安装[Git](https://git-scm.com/download/win)、[Python 3.10（64位）](https://www.python.org/downloads/release/python-31011/)及Visual Studio的“使用C++的桌面开发”工作负载，包含MSVC、Windows SDK和CMake工具。Python保留Launcher安装选项。其他系统尚未验证。

在自己选定的父目录打开PowerShell：

```powershell
git clone https://github.com/indulgeuuu-del/virtual-sim-icv.git
Set-Location virtual-sim-icv
git status
py -3.10 --version
```

最后一行应显示Python 3.10.x。后续命令均在包含README.md、tools和tests的仓库根目录执行。推送前需在GitHub接受仓库协作者邀请；首次提交时按Git提示配置自己的提交者姓名与邮箱。

先检查C++工具链：

```powershell
py -3.10 tools/check_driving_environment.py
```

命令会编译并运行仓库中的C++探测程序。终端结果中 `compiler_smoke_passed=true` 仅表示编译器可用；失败时按error提示补齐安装组件，若编译失败则查看提示的编译日志。

## 接入比赛平台

从[国创中心竞赛平台](https://dc.nevc.com.cn:4430/competition/icvsim/srs/)取得对应训练许可、配套客户端、完整SDK和场景。[SimOne官方入口](https://www.51sim.com/products/simone)可用于申请试用，但需另行核对与赛事环境的兼容性。

取得SDK后，在同一仓库根目录输入自己的SDK路径：

```powershell
$simoneSdk = Read-Host '输入SDK根目录的完整路径（不加引号）'
py -3.10 tools/check_driving_environment.py --sdk-root $simoneSdk
```

核对 `sdk_supplied=true` 和 `sdk_missing_files`，然后按[官方开发说明](https://simone-docs.51sim.com/20_Developer_Manual/01_Developer_Quick_Start/index.html)运行配套例程。文件齐全不代表版本、授权或运行兼容。

交接工程的CMakeLists依赖SDK上层工程、头文件和链接库，不能直接作为独立工程构建。首次打通后，将实际验证过的编译、启动和结束命令随开发模块提供，构建输出放在源码目录之外。完整比赛构建的当前状态见开发进度。

## 定位开发入口

基线目录：

`code_pre/2025 智能网联汽车-曾熙桐/代码工程/【20250725.8 Traj】last version/`

| 模块 | 阅读入口 | 职责 |
| --- | --- | --- |
| TrajectoryControl | `TrajectoryControl/src/main.cpp` | 驾驶模式选择，衔接跟车、路径与控制 |
| AVP | `AVP/src/main.cpp` | 自动泊车流程 |
| util | `util/` | 地图、路径和控制等公共工具 |

首次实质开发从该基线建立独立开发副本，复制需要的源码、配置和第三方声明，记录源路径及原始文件哈希，并在开发进度登记入口。其他队员沿用同一开发副本，避免各自另建版本。保留code_pre原件，不带入历史缓存或编译产物；源码混合UTF-8与GB18030，编辑前确认目标编码。

## 按本届规则实现功能

从[赛事官网](http://gcxl.edu.cn/new/index.html)取得2027届《命题与运行》《评分与规则》及补充通知。每项开发遵循以下步骤：

1. **明确验收条件。** 记录适用文件版本、页码、条款、场景名和评分编号，明确动作、距离、速度、灯光、时间及扣分边界。旧代码的caseIdx不能直接等同于本届命题或评分编号。
2. **定位已有逻辑。** 沿入口和调用链读清输入、判断、输出，在复现行为的基础上适配本届差异。优先做范围明确的小修改。
3. **实现并测试。** 针对问题构造反例，修复后验证边界和受影响的已有功能。SDK尚不可用时，可先验证不依赖平台的配置、几何和决策逻辑。
4. **接入真实场景。** 从官方例程和最小控制闭环开始，再验证分组场景、连续赛道及不同初始条件；根据实际扣分调整，最后固定参赛版本。

驾驶主链为“车辆/道路/障碍物状态 → 决策与路径 → 目标速度和转向 → 平台执行 → 下一帧反馈”。修改时核对坐标系、单位、控制模式与帧号；Speed模式的throttle表示m/s目标速度，不是油门百分比。旧地图坐标和车型参数应重新校准。

验证记录应包含代码提交号、平台/SDK版本、地图车型、场景参数、规则依据、结果与失败原因。分别标明离线测试、真实SDK编译、平台运行和官方评分，不将某一层通过推断为全部通过。

功能覆盖顺序为停车/跟车、车道控制、信号灯/停止线、变道避障与路口让行；泊车独立验证入库、驻停、出库，最后进行综合回归。

## 同步与提交

统一采用本地修改、验证、同步远端后直接推送main。开始前检查工作树；已有改动先保存，工作树干净后执行：

```powershell
git switch main
git pull --ff-only origin main
```

修改前协调正在编辑的文件和公共接口。完成对应验证后，检查差异并暂存；多个文件可重复输入路径及git add两行：

```powershell
git diff
$changedFile = Read-Host '输入本次要提交的文件相对路径（不加引号）'
git add -- $changedFile
git diff --cached
$commitMessage = Read-Host '说明本次解决了什么问题'
git commit -m $commitMessage
```

提交源码、必要配置、测试和使用说明，保留版权声明；不提交凭据、许可证或构建产物。只在有实质代码进展时更新开发进度，记录能力、验证和直接待办。

提交后先同步，再推送：

```powershell
git pull --rebase origin main
# 同步引入改动后，重新验证受影响部分，再执行下一行
git push origin main
```

出现冲突时先核对双方修改意图，解决后执行git add和git rebase --continue；不确定时用git rebase --abort返回同步前状态。推送被拒绝则再次同步，不使用强制推送覆盖队友提交。

## 文档维护

仅开发进度经常更新；根README与本文在目录、接口或使用方法变化时修改。说明必须基于克隆可得的内容，外部依赖给出官方获取方式，命令写清前提、执行位置和成功标准，不依赖某位成员已有的资料或环境。

CARLA工具和历史视频感知代码仅作辅助参考，按具体开发需要使用；正式验收以本届规则和官方平台为准。
