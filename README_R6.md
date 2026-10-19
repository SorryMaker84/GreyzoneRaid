# 灰区突围 / Greyzone Raid · R6

UE 5.8.3、Windows 单机第一人称搜打撤原型。进入仓库，搜索容器、管理背包，与敌人交战，再携带物资前往 E-07 撤离。R6 将库存规则、交互流程和界面命令拆开，适合对照 C++ 源码学习 UE 客户端职责与对象生命周期。

## 1. 从哪里启动

本机完整项目目录：

```text
C:\Users\ggbo\Documents\Codex\2026-10-06\task\GreyzoneRaid
```

**直接玩打包版**：双击下面的程序，无需先打开编辑器；搬到其他位置时请保留整个 `Windows` 文件夹。

```text
C:\Users\ggbo\Documents\Codex\2026-10-06\task\GreyzoneRaid\Packaged_R6\Windows\GreyzoneRaid.exe
```

**在 Editor 中运行**：用 UE 5.8 打开下面的项目。默认地图为 `/Game/Maps/Depot`；点击工具栏 Play，再按 Enter 开始行动。需要鼠标控制游戏时，先点击游戏画面。

```text
C:\Users\ggbo\Documents\Codex\2026-10-06\task\GreyzoneRaid\GreyzoneRaid.uproject
```

本机开发环境为 **UE 5.8.3、Visual Studio 2026、MSVC 14.50、Windows SDK 10.0.22621.0**。首次在新机器编译，安装 VS 的“使用 C++ 的游戏开发”工作负载及对应工具链，生成 Visual Studio 项目文件，打开 `GreyzoneRaid.sln`，选择 `Development Editor / Win64` 构建 `GreyzoneRaid`，再打开 `.uproject`。双击打包程序与编译源码是两个独立入口。

## 2. 操作与第一局路线

修改 C++ 后，先关闭编辑器，在项目根目录的 PowerShell 中运行 `.\Scripts\Build.ps1`；构建成功后重新打开项目。重新生成独立包使用 `.\Scripts\Package.ps1`，输出到 `Packaged_R6`。现有 R5 包不受此脚本影响。

| 输入 | 功能 |
| --- | --- |
| WASD / 鼠标 | 移动 / 转向 |
| 左 Shift / Space | 冲刺 / 跳跃 |
| 鼠标左键 / 右键 | 开火 / 瞄准 |
| R | 换弹 |
| E | 与附近容器交互 |
| Tab | 打开或关闭背包 |
| Q | 使用背包中的医疗包 |
| Esc | 先关闭打开的面板；行动中无面板时放弃本局 |
| Enter | 菜单开始行动；结算后重新开始 |

1. 靠近容器按 **E**，点击“开始搜索”，等待未知条目逐个揭示。打开箱子本身不会自动拿走物品。
2. 点击取物按钮，每次取一件。背包容量为 **8 件**，按数量总和计算，同一行的多件物品也占多个容量。打开容器时可把背包物品存回；单独打开背包没有存回目标。
3. 医疗包恢复最多 **45 点生命**，上限 100，满血不会消耗。弹药物品补充 **24 发备弹**，不会直接装入弹匣，之后按 R 换弹。
4. 保留至少一件物资，到 **E-07** 撤离区连续停留 **8 秒**。离开区域或受到伤害会重置倒计时；空背包不能撤离。

打开库存界面不会暂停战斗。面板打开时移动、视角和射击受到限制；受伤会关闭面板并中断搜索。已揭示的条目保留，未完成搜索需要重新计时。

## 3. C++ 从哪里读

先读头文件中的职责和接口，再沿 `.cpp` 跟一次取物流程。详细跳转、引擎源码证据与面试自测见 [R6 源码学习导览](R6_CODE_GUIDE.md)。

| 模块入口 | 负责什么 |
| --- | --- |
| [InventoryTypes](Source/GreyzoneRaid/RaidInventoryTypes.h)、[Model](Source/GreyzoneRaid/RaidInventoryModel.h) | 物品定义、稳定条目 ID、版本号，以及容量、数量守恒和转移规则 |
| [InventoryComponent](Source/GreyzoneRaid/RaidInventoryComponent.h)、[ItemCatalog](Source/GreyzoneRaid/RaidItemCatalog.h) | 持有库存状态、提交后通知；从资源定义物品效果 |
| [InteractionComponent](Source/GreyzoneRaid/RaidInteractionComponent.h)、[Loot](Source/GreyzoneRaid/RaidLoot.h) | 当前容器、搜索计时、取放与使用；箱子的场景表现 |
| [PlayerController](Source/GreyzoneRaid/RaidPlayerController.h)、[HUD](Source/GreyzoneRaid/RaidHUD.h) | 输入模式与命令上下文校验；Canvas 绘制和点击事件 |
| [Player](Source/GreyzoneRaid/RaidPlayer.h) | Character 移动、枪械、生命值及默认组件 |
| [GameMode](Source/GreyzoneRaid/RaidGameMode.h)、[ExtractionZone](Source/GreyzoneRaid/RaidExtractionZone.h) | 菜单、对局与结算；撤离条件和倒计时 |

本版是单机实现，未实现网络复制或服务器权威。库存的重入保护也不是跨线程锁。Canvas 界面、Character 中的枪械仍保留；这些边界及拆分理由在源码导览中有说明。

## 4. 版本与验证入口

- [TEST_RESULTS.md](TEST_RESULTS.md)：以这里的 R6 记录查看实际构建、玩法回归、截图及交付审计结果；测试代码存在不代表已经通过运行。
- [R6_ENGINE_EXPERIMENT.md](R6_ENGINE_EXPERIMENT.md)：基线与 R6 的测量方法、数据和局限；本说明不承诺 FPS 提升。
- 实际独立包截图：[容器搜索](Packaged_R6/Windows/Screenshots/R6_ContainerSearch.png)、[敌人交战](Packaged_R6/Windows/Screenshots/R6_Combat.png)、[撤离成功](Packaged_R6/Windows/Screenshots/R6_ExtractionSuccess.png)。完整原始截图与日志见 `Evidence/R6Final`。
- [R5 源码与资源备份](Backups/R5_BeforeR6_20261008T071118Z/GreyzoneRaid_R5_SourceContent.zip)及[备份校验回执](Backups/R5_BeforeR6_20261008T071118Z/BackupReceipt.json)可用于恢复改造前版本。
- [README_R5.md](README_R5.md) 与 [R5_BEGINNER_GUIDE.md](R5_BEGINNER_GUIDE.md)：保留的 R5 说明和玩法入门；历史验证见 [TEST_RESULTS_R5.md](TEST_RESULTS_R5.md)。R5 打包目录 `Packaged_R5` 保留，R6 使用独立的 `Packaged_R6`。

阅读建议：先完成一次搜索、取物和撤离，再按导览跟踪一次取物调用链，最后看版本校验、失败回滚与组件退出清理。
