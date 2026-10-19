# 灰区突围 / Greyzone Raid · R7

UE 5.8.3、Windows 单人第一人称搜打撤原型。R7 的 Frontier 将行动扩大到五个区域：外围补给、中央高价值搜刮、通讯站情报目标，以及南北两条撤离路线。初次接触建议先按 [新手指南](R7_BEGINNER_GUIDE.md) 完成一局，再读 [R7 源码导览](R7_CODE_GUIDE.md)。

## 当前交付状态

- **Editor 源码编译已通过**：[R7_FinalBuild_11.log](Evidence/R7_FinalBuild_11.log) 记录 `Result: Succeeded`，总耗时 23.77 秒。
- Frontier 已实际生成并保存，且已设为编辑器与游戏的默认地图；规模见 [地图生成记录](Evidence/R7MapBuild.json)。
- 独立包完整玩法已通过 549 项检查、0 失败，完成 4 局流程；Editor 与独立包旧地图回归各 396 项通过。包内手部关键帧、场景预览和三次性能采样已完成。完整结果、失败记录与未运行项见 [TEST_RESULTS_R7.md](TEST_RESULTS_R7.md)。

## 从哪里启动

源码项目入口：

```text
C:\Users\ggbo\Documents\Codex\2026-10-06\task\GreyzoneRaid\GreyzoneRaid.uproject
```

用 UE 5.8 打开项目，默认进入 **Content / Maps / Frontier**，点击 Play，再在游戏菜单按 Enter 开始。需要控制视角时先点击游戏画面。

[默认地图配置](Config/DefaultEngine.ini) 指向 Frontier。旧 Depot 保留用于回归验证；打开它会进入较小的 R6 场景。新手不需要手动运行关卡生成脚本。

独立包入口为：

```text
C:\Users\ggbo\Documents\Codex\2026-10-06\task\GreyzoneRaid\Packaged_R7\Windows\GreyzoneRaid.exe
```

搬运时需保留整个 `Windows` 文件夹。源码修改后的编译入口是 [Build.ps1](Scripts/Build.ps1)；R7 的独立打包入口是 [Package_R7.ps1](Scripts/Package_R7.ps1)，输出使用 `Packaged_R7`。现有 R6 文档和 `Packaged_R6` 保留。

## R7 这一局做什么

关卡配置约 **160 × 140 米**，包含 **7 栋可进入建筑、24 个初始搜索容器、12 名敌人**。五区分别是后勤检查站、营房医疗区、中央仓储区、燃料与排水区、通讯台地区。外围敌人较少；仓库夹层和通讯站上层放有更高价值物资。击毙敌人后还可能产生战利品包。

| 路线／目标 | 条件与作用 |
| --- | --- |
| 南侧公路撤离 S-01 | 无需完成情报目标；位于地图东南侧 |
| 通讯站情报 | 到通讯台地区上层，靠近并看向终端，**长按 E 6 秒** |
| 北侧铁路撤离 N-02 | 完成情报后解锁，适合从通讯区域撤出 |
| 两处撤离共有条件 | 携带至少 **1 件物资**，在实际撤离区连续停留 **8 秒**；离开或受伤会重置 |

情报是可选目标：可以直接带物资走南路，也可以承担深入风险，解锁北路。下载时松手、移开视线、走远或受伤都会中断并归零；完成后有终端绿灯、HUD 提示和本局结算记录。M 地图可查看目标、出口位置与开放状态，打开地图和背包都不会暂停战斗。

## 常用操作

| 输入 | 功能 |
| --- | --- |
| WASD / 鼠标 | 移动 / 转向 |
| 左 Shift / Space | 冲刺 / 跳跃 |
| 鼠标左键 / 右键 | 开火 / 瞄准 |
| R | 换弹 |
| E | 打开附近容器；面板打开时关闭面板 |
| 长按 E | 聚焦通讯终端时下载情报 |
| Tab | 打开或关闭背包 |
| Q | 使用背包里的医疗包 |
| M | 在 Frontier 行动中打开／关闭战术地图；先关闭库存面板 |
| Esc | 先关地图或库存；行动中无面板时放弃本局 |
| Enter | 菜单开始行动；结算后重新开始 |

打开容器后点击“开始搜索”，未知条目会逐个揭示，**物品仍需手动取走**。背包按物品数量计，占满为 8 件；医疗包最多恢复 45 点生命，弹药物品使用后增加 24 发备弹，再按 R 装入弹匣。使用完唯一一件物资后，需再携带一件才能撤离。

本版没有跨局仓库或持久经济。结算记录本次保全数量、价值和情报完成情况；重开会创建一局新的物资、敌人和任务状态。

## C++ 模块分工

下面的源码链接以项目根目录为基准；阅读源码时，请打开项目目录中的这份说明。

| 从哪里读 | 主要职责 |
| --- | --- |
| [RaidPlayer](Source/GreyzoneRaid/RaidPlayer.h)、[RaidPlayerController](Source/GreyzoneRaid/RaidPlayerController.cpp) | 角色、移动视角、枪械射线伤害、换弹、生命与输入路由 |
| [RaidArmsAnimInstance](Source/GreyzoneRaid/RaidArmsAnimInstance.cpp)、[author_r7_hands.py](Scripts/author_r7_hands.py) | 第一人称握持、开火手势、与弹匣同步的换弹及取消回位 |
| [RaidEnemy](Source/GreyzoneRaid/RaidEnemy.cpp) | 基于 UE 导航的巡逻、发现、追击、攻击与死亡 |
| [RaidLoot](Source/GreyzoneRaid/RaidLoot.cpp)、[RaidInteractionComponent](Source/GreyzoneRaid/RaidInteractionComponent.cpp) | 容器搜索、聚焦与交互会话 |
| [RaidInventoryModel](Source/GreyzoneRaid/RaidInventoryModel.h)、[RaidInventoryComponent](Source/GreyzoneRaid/RaidInventoryComponent.cpp) | 可独立理解的库存规则，以及角色／容器与规则的连接 |
| [RaidMissionObjective](Source/GreyzoneRaid/RaidMissionObjective.cpp)、[RaidExtractionZone](Source/GreyzoneRaid/RaidExtractionZone.cpp) | 可选下载任务、出口解锁、连续停留计时 |
| [RaidGameMode](Source/GreyzoneRaid/RaidGameMode.cpp)、[RaidHUD](Source/GreyzoneRaid/RaidHUD.cpp) | 菜单、行动、成功／死亡结算、重开与界面绘制 |
| [create_r7_map.py](Scripts/create_r7_map.py)、[RaidPropCluster](Source/GreyzoneRaid/RaidPropCluster.cpp) | 编辑器关卡生成、实例化场景物件；运行时不需要 Python |

先沿“按 E → 交互组件 → 终端 → GameMode → HUD／出口”读一次，再尝试改任务 DA 的下载时间。测试导演是自动验收辅助代码，普通启动不会启用，不必作为学习玩法的第一份文件。

## 继续阅读

- [R7_BEGINNER_GUIDE.md](R7_BEGINNER_GUIDE.md)：第一局走南路，第二局尝试情报目标，常见卡点处理。
- [R7_CODE_GUIDE.md](R7_CODE_GUIDE.md)：跟踪 DA → 终端 → GameMode → HUD → 撤离规则。
- [R6_CODE_GUIDE.md](R6_CODE_GUIDE.md)：R7 继续使用的库存、搜索、条目身份和生命周期基础。
- [TEST_RESULTS_R7.md](TEST_RESULTS_R7.md)：实际验证记录；旧版本说明见 [README_R6.md](README_R6.md) 与 [TEST_RESULTS_R6.md](TEST_RESULTS_R6.md)。
