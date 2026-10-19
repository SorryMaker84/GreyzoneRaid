# Greyzone Raid：单机搜打撤

项目：`C:\Users\ggbo\Documents\Codex\2026-10-06\task\GreyzoneRaid`  
本机：DESKTOP-TR04BB0；UE **5.8.3**；Windows / D3D12 / SM6。

R2 地图 **Depot** 包含工业货场、仓库、集装箱、3 个守卫、9 处物资和一个撤离检查点。主要玩法由可读 C++ 实现，蓝图保存参数与资源引用。联机、干员技能和复杂经济暂不实现。

## 打开与运行

双击 `GreyzoneRaid.uproject`，等待加载与首次着色器编译。默认地图为 `Content/Maps/Depot`；若编辑器恢复其他地图，手动打开 Depot。点击 **Play**，再点击视口取得键鼠焦点。**Shift + F1** 释放鼠标，工具栏 **Stop** 结束 PIE。

也可在项目目录的 PowerShell 中执行以下命令，以普通可见窗口和正常声音启动游戏：

```powershell
.\Scripts\Run.ps1
```

R2 独立程序的输出目录为 `Packaged_R2/Windows`，入口是 `GreyzoneRaid.exe`。本轮是否实际打包与验证成功，以 [TEST_RESULTS.md](TEST_RESULTS.md) 为准。复制时保留整个 Windows 文件夹，不能只复制 exe。旧 `Packaged/Windows`、`Backups/R1_20261006` 是 R1 历史版本。

## 操作与目标

| 操作 | 按键 |
|---|---|
| 移动 / 转向 | W A S D / 鼠标 |
| 跳跃 / 冲刺 | Space / 按住左 Shift |
| 开火 / 连射 | 鼠标左键 / 按住左键 |
| 机瞄 / 换弹 | 按住鼠标右键 / R |
| 搜索物资 | 对准附近物资，按住 E 约 1.1 秒；松开或移开视线取消 |
| 查看背包 | Tab；游戏继续运行 |
| 治疗 | Q；需要已拾取的医疗包 |
| 结算后重新开始 / 退出 | Enter / Escape |

至少携带一件物资，前往地图东北侧 **EXFIL** 检查点，连续停留 **8 秒**完成撤离。离开会重置倒计时。成功结算物资价值；生命降到零则失败。两种结算后均可 Enter 重开，重置玩家、敌人和物资。

背包上限 8 件；医疗包恢复最多 45 HP，并扣除一件。步枪弹匣 12 发、初始储备 60 发、伤害 34、换弹约 1.5 秒。弹药物资增加储备弹药并计入携带统计。敌人通过视线发现玩家，使用 UE 导航巡逻和追击；墙壁、集装箱可阻挡射击。

## 修改 C++ 与参数

修改代码后先关闭此项目的 UE / 游戏实例，再编译，避免 Live Coding 占用。打开 `GreyzoneRaid.sln`，选择 **Development Editor / Win64**；或运行 `.\Scripts\Build.ps1`。日志在 `Evidence/EditorBuild.log`。

| 模块（Source/GreyzoneRaid 下同名 .h / .cpp） | 职责 |
|---|---|
| RaidPlayer | 输入、移动、机瞄、射击、后坐、换弹、搜索、背包、治疗 |
| RaidEnemy | 巡逻、视线发现、导航追击、掩体检查、攻击、死亡 |
| RaidLoot | 物资类型、数量、价值、搜索时间与拾取 |
| RaidExtractionZone | 范围、倒计时、离开复位与撤离完成 |
| RaidGameMode | 一局状态、成功/失败结算、重开 |
| RaidHUD | 生命、弹药、罗盘、搜索、背包和结算界面 |
| RaidNavBounds | 动态导航范围 |
| RaidTestDirector | 仅 -RaidSelfTest 开启的真实 UE 世界集成检查 |
| RaidVisualDirector | 仅 -RaidVisualReview 开启的截图演练；演示中抑制 AI 攻击 |

可调蓝图位于 `Content/Blueprints`：`BP_RaidPlayer`、`BP_DepotGuard`、`BP_DepotSupply`、`BP_DepotExtraction`、`BP_DepotGameMode`。修改 **Class Defaults** 后 Compile、Save。敌人 `PatrolPoints` 使用世界坐标，单位厘米；移动敌人后同步检查巡逻点与导航覆盖。

## 测试与打包

```powershell
.\Scripts\Build.ps1
.\Scripts\Test.ps1
.\Scripts\Package.ps1
.\Scripts\Test.ps1 -GameExecutable "$PWD\Packaged_R2\Windows\GreyzoneRaid.exe"
```

脚本默认已有引擎位于 `C:\Program Files\Epic Games\UE_5.8`，可用 `-EngineRoot` 指定其他已有安装。打包只包含 Depot，输出 Win64 Development 版。本项目关闭 ZenStore，采用本地文件 Cook 和 IoStore，未改变系统级 UE 设置。

自动检查与视觉演练均静音。检查调用真实玩法方法、动作委托、碰撞、导航和计时器，在临时世界里替换测试夹具，不修改保存地图。它们不等于实体键鼠试玩；本会话原生 UI 工具不可用，人工键鼠与 PIE 体验不能标记为通过。

地图与资源已经保存，普通运行不需要执行制作脚本。`upgrade_map.py`、`configure_slice.py`、`polish_slice.py` 会更新生成物；手动改图后重跑前先备份。`author_grip_pose.py` 为 CC0 手臂制作固定握持姿态，保留原始动画。

## 证据、资源与限制

- 新旧版本的验证分别记录在 [TEST_RESULTS.md](TEST_RESULTS.md)，原始日志与截图保存在 `Evidence`。
- 资源来源、CC0 / Epic 许可分类和文件校验见 [ASSET_SOURCES_R2.md](ASSET_SOURCES_R2.md)。未提取商业游戏资源，未购买或发布。
- 枪臂采用固定握持姿势，换弹由整组枪臂运动与进度表现，尚无逐指拆装弹匣动画。
- 敌人采用 UE Manny 骨骼动画与简单原创装备部件，仍属于原型美术；不是商业 FPS 美术品质。
- 音效已接入，自动演练关闭声音；听感需在正常游戏窗口确认。
