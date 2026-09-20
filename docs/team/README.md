# 四人协作与首次运行

本仓库用于四人共同开发。团队统一采用“本地修改 → 验证 → 同步远端最新改动 → 直接推送 `main`”的流程，后续 AI 辅助开发也遵循此约定，不要求建立功能分支或创建 PR（合并申请）。

## 首次下载与离线测试

安装 Git 和 Python 3.10，在 PowerShell 中执行：

```powershell
git clone https://github.com/indulgeuuu-del/virtual-sim-icv.git
cd virtual-sim-icv
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r tests/requirements.txt
.\.venv\Scripts\python.exe -m unittest discover -s tests -p "test_perception.py" -v
.\.venv\Scripts\python.exe -m perception.main --help
```

下载后默认使用 `main`。本文不要求激活虚拟环境，直接使用 `.venv` 内的 Python，避免误用机器上的其他环境。

测试应全部通过、帮助命令应列出模型/视频/输出目录参数。测试使用人工构造的检测框，不需要权重、视频或仿真平台；不能据此判断真实识别准确率或比赛成绩。`tests/requirements.txt` 只定义离线测试环境；完整推理使用另一独立环境，见 [视频推理运行说明](视频推理运行.md)。这套测试是辅助项目的检查，不是四个人开始驾驶开发的前置条件。

离线环境基线为 Windows/Python 3.10、NumPy 2.2.6、opencv-python 4.12.0.88；完整推理环境使用单独锁定的依赖。当前测试数量以运行输出为准。默认源下载慢时，可在安装命令中添加 `--index-url https://pypi.tuna.tsinghua.edu.cn/simple` 使用清华镜像。

## 每次修改的流程

开始前运行 `git status`，先处理自己尚未保存的改动，不覆盖队友工作。工作树干净时：

```powershell
git switch main
git pull --ff-only origin main
# 编辑相关文件，并运行对应测试
git diff
git add 具体修改的文件
git commit -m "说明解决了什么问题"
git pull --rebase origin main
# 若同步带来了队友的新改动，重新运行受影响模块的测试
git push origin main
```

`commit` 把修改保存为本地版本，`push` 才会发布到 GitHub。提交说明或团队文档中记录修改目的、实际测试结果和未验证项。

提交后的 `pull --rebase` 会先取回队友的新提交，再把自己尚未发布的提交接在后面。发生冲突时停止后续步骤，检查冲突文件并保留双方需要的逻辑；解决后用 `git add 文件路径` 和 `git rebase --continue` 继续，完成后重新验证再推送。不确定如何解决时用 `git rebase --abort` 回到同步前的状态，与相关队友核对。

若推送提示远端已有更新（non-fast-forward），再次同步、处理冲突并验证，然后重试；不要用 `--force` 覆盖队友提交。已经发布的错误通过新的修复提交纠正。

公开仓库允许读取，但只有被授权的协作者能直接推送。仓库所有者需在 GitHub 的 Settings → Collaborators 中添加另外三人的账号，队友接受邀请后才获得相应权限。账号及权限尚未在本仓库中配置。

## 建议的四个工作方向

以下是待团队讨论的分工，不预设具体人员。每人负责一个方向，也能审阅其他方向的小改动。

| 方向 | 当前入口 | 第一项可验收工作 |
| --- | --- | --- |
| 场景规则与决策 | 正式命题/评分、主基线 `TrajectoryControl/` | 对齐场景编号、停车/让行/变道条件与扣分规则 |
| 路径与驾驶控制 | 主基线 `TrajectoryControl/` | 核实坐标与控制单位，建立跟踪/制动反例 |
| 自动泊车 | 主基线 `AVP/` | 整理进入、倒车、等待、驶离的切换条件 |
| 平台集成与评测 | README、团队文档 | 维护环境、场景、规则对应关系与实验记录 |

主基线路径见 [仓库首页](../../README.md)。`code_pre/` 保存交接原件，不直接修改；C++ 开发时另建副本并记录来源。公共工具或接口有改动时，在提交说明或团队文档中说明对其他模块的影响。Python 感知独立处理视频，目前不是 C++ 驾驶的输入模块。

## 文件与实验共享

源码、测试、配置示例和 `docs/team/` 进入 Git。AI 私人记录、原始材料、模型、视频、缓存、实验输出和账号信息留本地。`.gitignore` 只是防止误提交的过滤规则，提交前仍需检查 `git diff --cached`。

模型和视频由团队通过约定的私有共享位置传递；仓库不包含下载链接或完整环境。拿到同名文件仍应核对内容：

```powershell
Get-FileHash -Algorithm SHA256 "本地路径/best.pt"
Get-FileHash -Algorithm SHA256 "本地路径/sample.mp4"
```

每次实验记录：代码提交号、Python/依赖版本、模型与视频 SHA-256、类别表、参数、人工真值、输出及误差。平台开放后增加平台/SDK版本、地图、车型、场景名及官方分数。账号密码不要写进代码或实验记录。

按 [开发路线](开发路线.md) 推进驾驶/泊车主线。官方赛事入口不可用时，同时调查SimOne本地端与SDK、做纯算法验证；具备可运行环境后按“官方例程 → 最小控制闭环 → 交接基线 → 分场景改进”推进。没有正式独立视频题要求前，不安排专人以视频计数为主要备赛任务。
