# 灰域突围 / Greyzone Raid — R5 容器搜刮

UE **5.8.3** / Windows 单机第一人称搜打撤原型，核心逻辑使用 C++。从主菜单开始行动，探索仓库、搜刮物资、与守卫交战，再携带物资前往 E-07 撤离。没有账号、联网、商城或局外仓库经济。

## 打开与运行

项目目录：`C:\Users\ggbo\Documents\Codex\2026-10-06\task\GreyzoneRaid`。

- **独立包**：运行入口为 `Packaged_R5\Windows\GreyzoneRaid.exe`；取得完整包后双击运行，需保留整个 `Windows` 文件夹。已完成独立包自动回归；`START_HERE.txt`、初学者指南及 `Screenshots` 随包提供。
- **编辑器**：使用已有 UE 5.8.3 打开 `GreyzoneRaid.uproject`，打开 `Content/Maps/Depot`，点击 Play，再点击视口。
- **本机 UE 游戏窗口**：在项目 PowerShell 运行 `.\Scripts\Run.ps1`。项目字体随资源分发，无需另行安装。

## R5 的核心操作

准星对准近处容器，按一下 **E**，打开左侧来源容器与右侧背包。点击“开始搜索”，物资按条计时揭示；**揭示后还要点击“拿取 1”才进入背包**。普通箱初始有 2–3 个可搜索条目，未知条目不显示物品身份、数量、价值或稀有度。状态摘要、当前搜索行高亮和取空记录帮助辨认进度，敌人战利品包使用同一流程。

背包最多 **8 件**，可“放回 1”腾出空间。弹药进入背包后点击“补充弹药”，消耗 1 件、备用弹药 **+24**；医疗包使用后恢复最多 **45 HP**。满包仍能搜索。关闭或受伤等中断会清零当前未完成条目的计时，已揭示状态与剩余物资在同一局保留；空箱保留，新局重置。

WASD 移动，鼠标瞄准，左键开火，右键机瞄，R 换弹，Tab 背包，Q 医疗包。Escape 先关闭已打开的面板；面板关闭时按 Escape 会放弃本局。完整操作表、首次游玩步骤和界面解释见 [R5 初学者指南](R5_BEGINNER_GUIDE.md)。

## 代码与交付记录

源码位于 `Source/GreyzoneRaid`：`RaidPlayer` 管理玩家、背包与搜索，`RaidLoot` 保存容器内容，`RaidHUD` 绘制界面，`RaidEnemy` 管理守卫，`RaidGameMode` 管理菜单与每局状态。建议从 [指南中的模块说明](R5_BEGINNER_GUIDE.md#c-初学者从哪里读) 开始。

- [完整测试记录](TEST_RESULTS.md)：10 组最终运行全部通过，共 105 张 PNG。容器专项每组 152 项、菜单流程每组 251 项、基础玩法每组 75 项；覆盖编辑器、独立包及 1280×720 界面。正常 AI 下的搜刮中断、输入恢复和搜索中死亡已有独立日志证据。
- [R5_CHECKPOINT.md](R5_CHECKPOINT.md)：最终交付与历史备份位置；截图位于 `Packaged_R5/Windows/Screenshots`。
- [ASSET_SOURCES_R5.md](ASSET_SOURCES_R5.md)：继承 R4/R3/R2 的资源来源、第三方许可与玩法参考边界。
- [README_R4.md](README_R4.md)：保留的 R4 历史说明；对应旧包仍在 `Packaged_R4/Windows`。

自动验证通过 UE 内部导演、输入绑定和 Canvas 界面调用执行，全部静音。物理键鼠、窗口焦点、主观手感与音效试听尚未实测。本项目是可学习的小型原型，不声称逐点复刻参考商业游戏。
