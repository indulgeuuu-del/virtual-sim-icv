# 四人协作与首次运行

本仓库用于共同开发。当前开发基线按仓库所有者要求直接发布到 `main`。后续多人同时修改时建议使用分支：分支是自己的修改路线；PR（Pull Request，合并申请）把差异和测试结果集中给队友检查。分支合并前不影响 `main`，以下流程作为协作建议。

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

功能尚在 PR 时，先按 PR 的分支名执行 `git fetch origin` 和 `git switch 分支名`，再安装、测试。本文不要求激活虚拟环境，直接使用 `.venv` 内的 Python，避免误用机器上的其他环境。

预期结果为 10 项测试全部通过、帮助命令列出模型/视频/输出目录参数。测试使用人工构造的检测框，不需要权重、视频或仿真平台；不能据此判断真实识别准确率或比赛成绩。`tests/requirements.txt` 只定义离线测试环境；完整推理环境需另行验证，见 [感知说明](../../perception/README.md)。

2026-09-20 已在 Windows 的新建 Python 3.10 虚拟环境中验证：NumPy 2.2.6、opencv-python 4.12.0.88，10 项测试与帮助入口通过，`pip check` 无依赖冲突。默认源下载慢时，可在安装命令中添加 `--index-url https://pypi.tuna.tsinghua.edu.cn/simple` 使用清华镜像。

## 每次修改的流程

开始前运行 `git status`，先处理自己尚未保存的改动，不覆盖队友工作。工作树干净时：

```powershell
git switch main
git pull --ff-only
git switch -c feat/你的任务名
# 编辑相关文件，并运行对应测试
git diff
git add 具体修改的文件
git commit -m "说明解决了什么问题"
git push -u origin feat/你的任务名
```

在 GitHub 创建 PR，填写问题、修改、测试和未验证项。建议至少另一位队友审阅后再合并；这目前是团队约定，不代表服务器已启用强制保护。遇到冲突，先理解双方修改，再共同保留需要的逻辑，不用强制推送解决。

公开仓库允许读取，但只有被授权的协作者能直接推送。仓库所有者需在 GitHub 的 Settings → Collaborators 中添加另外三人的账号，队友接受邀请后才获得相应权限。账号及权限尚未在本仓库中配置。

## 建议的四个工作方向

以下是待团队讨论的分工，不预设具体人员。每人负责一个方向，也能审阅其他方向的小改动。

| 方向 | 当前入口 | 第一项可验收工作 |
| --- | --- | --- |
| 感知识别与计数 | `perception/`、`tests/` | 准备可共享的视频片段与人工计数，验证完整推理 |
| 驾驶控制 | 主基线 `TrajectoryControl/` | 整理平台输入、速度/转向输出和 SDK 依赖 |
| 自动泊车 | 主基线 `AVP/` | 整理进入、倒车、等待、驶离的切换条件 |
| 平台集成与评测 | README、团队文档 | 维护环境、场景、规则对应关系与实验记录 |

主基线路径见 [仓库首页](../../README.md)。`code_pre/` 保存交接原件，不直接修改；C++ 开发时另建副本并记录来源。公共工具或接口有改动时先在 PR 中说明对其他模块的影响。Python 感知独立处理视频，目前不是 C++ 驾驶的输入模块。

## 文件与实验共享

源码、测试、配置示例和 `docs/team/` 进入 Git。AI 私人记录、原始材料、模型、视频、缓存、实验输出和账号信息留本地。`.gitignore` 只是防止误提交的过滤规则，提交前仍需检查 `git diff --cached`。

模型和视频由团队通过约定的私有共享位置传递；仓库不包含下载链接或完整环境。拿到同名文件仍应核对内容：

```powershell
Get-FileHash -Algorithm SHA256 "本地路径/best.pt"
Get-FileHash -Algorithm SHA256 "本地路径/sample.mp4"
```

每次实验记录：代码提交号、Python/依赖版本、模型与视频 SHA-256、类别表、参数、人工真值、输出及误差。平台开放后增加平台/SDK版本、地图、车型、场景名及官方分数。账号密码不要写进代码或实验记录。

先让四个人都能运行同一套离线测试，再恢复完整视频推理。平台可进入后按“官方例程 → 最小控制闭环 → 交接基线 → 分场景改进”推进。
