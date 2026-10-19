# 灰域突围 R8.1

继续 R8 的玩家体验审查。本轮只修正地图遮挡战场时的输入衔接，未扩张场景、导入贴图或改变难度。

## 运行

- 最新独立包：`Packaged_R81/Windows/GreyzoneRaid.exe`，保留完整 Windows 文件夹。
- 源码：`GreyzoneRaid.uproject`，UE 5.8.3 打开，默认 Frontier，点 Play。
- R8 原包 `Packaged_R8` 保留；修改前源码/资源恢复包为 `Backups/R8_BeforeR81_Source.zip`，363 项已逐文件回读校验。

## 玩家能感知的变化

- 打开 M 地图时停止持续射击和瞄准，避免战场被遮住后仍浪费弹药。
- 地图中重新按射击、瞄准或 E，会返回现场并执行对应操作。
- 地图仍不暂停世界，仍可移动、受伤；Esc 先关地图，再按才打开暂停菜单。
- 暂停、放弃确认、治疗、死亡结算、双撤离与背包规则沿用 R8。

WASD 移动；鼠标左键射击、右键瞄准；Shift 冲刺，Space 跳跃；R 换弹，E 交互，Tab 背包，Q 治疗，M 地图，Esc 关闭面板/暂停，Enter 继续或重开。

## C++ 阅读入口

- `RaidPlayerController::PrepareTacticalMapInput` 停止持续开火/瞄准；`ReturnToWorldView` 关闭地图。
- `RaidPlayer::StartFire/StartAim` 与 Controller 的 E 输入先返回现场，再执行原有玩法。
- `RaidHUD::ToggleTacticalMap` 通知 Controller 处理输入，地图侧栏说明新行为。
- `RaidR81MapProbe` 是显式开启的自动输入检查，不是普通玩法。
- `RaidTestDirector` 的 `R81MovementProbe` 只记录诊断数据；`R81InjectHitch` 明确注入一次 450 ms 卡帧。正常运行均不启用。

完整基础玩法与模块分工见 `README_R8.md`、`R7_CODE_GUIDE.md`；本轮验证见 `TEST_RESULTS_R81.md`。
未使用新资源，许可仍见 `ASSET_SOURCES_R7.md` 及包内 ThirdPartyLicenses。Git 保持暂停。
