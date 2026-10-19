# 灰域突围 R8：玩家体验迭代

基于 R7 Frontier，同一 UE 5.8.3 C++ 项目。地图、资源、敌人难度与容量规则不变。

## 打开与运行

- 源码：`GreyzoneRaid.uproject`，UE 编辑器打开后点 Play。
- 最新版本：`Packaged_R8/Windows/GreyzoneRaid.exe`。保留整个 Windows 文件夹。
- 第一检查点独立保留在 `Packaged_R8_Checkpoint1/Windows/GreyzoneRaid.exe`。
- R7 可玩包和逐文件回读验证的恢复快照仍保留；见 `Evidence/R8_PreflightReceipt.json`。

## 本轮玩家可见变化

- 无面板时 Esc 打开真正的暂停菜单；敌人、换弹、撤离和行动计时冻结。
- Esc 或 Enter 恢复。放弃需点击“放弃本次行动”，再明确确认；Enter 永远继续行动，不确认损失。
- 跳跃途中暂停会保留物理速度；恢复后继续原来的跳跃，不突然下落。
- 有背包或地图时 Esc 先关闭当前面板。这些面板仍不暂停战斗。
- 空包时常驻提示先搜索、手动拿取；已有物资时引导南侧 S-01 撤离。情报保持可选。
- 阵亡结算显示真实致命射击的方向和距离；未知伤害来源不会假称巡逻队。新一局清空死亡原因。
- 医疗包显示实际恢复量，95→100 显示恢复 5 点；满血不消耗。成功结算明确下局重新开始，没有跨局仓库。

## 操作

WASD 移动，鼠标转向，Shift 冲刺，Space 跳跃；左键射击，右键瞄准，R 换弹。
E 打开容器，点击开始搜索，再逐件拿取；通讯终端长按 E 6 秒。
Tab 背包，Q 医疗包，M 地图，Esc 关闭面板或暂停；Enter 开始、继续或重开。
带至少 1 件物资进入开放撤离区，存活 8 秒。受伤或离开重置计时。

## 初学者源码入口

- `RaidGameMode` 保存暂停菜单与确认状态，调用 UE 世界暂停；结算前恢复世界。
- `RaidPlayerController` 将 Esc / Enter 设置为暂停中仍可触发，并管理鼠标与输入模式。
- `RaidPlayer::PreparePauseInput` 释放开火、瞄准和移动意图，保留换弹计时器。
- `RaidHUD` 绘制暂停/确认菜单和随背包状态变化的目标提示。
- `RaidPlayer::TakeDamage` 在致命伤害发生时保存来源信息；`RaidInteractionComponent::UseEntry` 报告实际治疗量。
- `RaidR8TestDirector` 仅在 `-RaidR8Test` 下运行，使用 UE 模拟按键和实际 Canvas 命中框。

原始玩法与组件说明继续参考 `R7_BEGINNER_GUIDE.md`、`R7_CODE_GUIDE.md`。
资源来源和许可沿用 `ASSET_SOURCES_R7.md` 及其历史引用，本轮未引入新素材。
验证记录见 `TEST_RESULTS_R8.md`。Git 保持暂停。
