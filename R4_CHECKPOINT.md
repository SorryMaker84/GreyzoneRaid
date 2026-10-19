# R4 中文版工作检查点

2026-10-07 UTC，DESKTOP-TR04BB0，同一 GreyzoneRaid 项目，UE 5.8.3。本文件记录阶段状态；最终验收以 [TEST_RESULTS.md](TEST_RESULTS.md) 的 R4 记录为准。

## 已完成的修改

- 主菜单、操作说明、战斗 HUD、罗盘、物资/搜索/治疗/撤离提示、背包与结算已中文化。键位、内部资产名和区域编号保留。
- Depot 与 RaidYard 两张保存地图中的物资显示名和标牌已迁移，共 103 个世界文字组件应用中文字体（Depot 61 个，RaidYard 42 个）。迁移报告未发现未处理的英文词组；默认游玩与打包地图仍为 Depot。证据：`Evidence/R4_MapLocalization.json`。
- Noto Sans CJK SC Regular 2.004 已导入。HUD 使用嵌入完整 OTF 的 Runtime 字体，世界 TextRender 使用单页 Offline 字体；`RaidText` 统一为运行时标签应用世界字体。
- 世界图集已烘焙 487 字符，当前所需 485 字符全部覆盖。`Evidence/R4_FontValidation.json` 为 `R4_FONT_BAKED_OK`，源字体和图集缺字均为 0。来源、许可和复现见 [ASSET_FONT_R4.md](ASSET_FONT_R4.md)。
- 打包配置包含字体目录，`Package.ps1` 输出目标为 Packaged_R4，并在成功后附带字体许可与版权文本。项目文档已更新为 R4 中文说明。

## 验证结果与证据

首轮编辑器环境预览于 2026-10-07 06:31:10–06:31:58 UTC 完成：菜单与完整行动循环 **189/189 通过、0 失败、进程退出码 0**。阶段证据保存在 `Evidence/R4Flow_FirstPreview/`。随后已统一品牌名称并修正损失说明，完成以下编辑器环境验证：

| 验证 | 当前证据 |
|---|---|
| 最终编辑器构建 | 成功，用时 13.28 秒；`Evidence/EditorBuild_R4_Final_Console.log` |
| 菜单与完整行动循环 | 198/198 通过、0 失败、退出码 0，约 06:37 UTC 完成；`Evidence/R4Flow_Editor/`，共 9 张截图，含放弃行动结算 |
| Depot 世界文字运行时检查 | 61 个标签，263 个字符（181 个中文字符）、81 个独立字符，缺字 0；记录于最终流程测试 |
| 玩法回归 | 67/67 通过，约 06:38 UTC 完成；`Evidence/SelfTest_R4_Editor.log` |
| 编辑器画面演练 | 13 张截图，约 06:39 UTC 完成；`Evidence/R4Visual_Editor/` |

**R4 已完成交付验证。** Win64 Development 打包成功，BuildCookRun 62.03 秒；独立包1600×900与1280×720流程各198/198通过，玩法67/67通过，13张玩法截图生成并正常退出。最后一次小窗口流程于06:44:28 UTC完成。字体Inline字节和世界字形在Cook包中通过验证；许可随包保存。测试进程已退出。最终截图、证据与未运行项见 [TEST_RESULTS.md](TEST_RESULTS.md)。

自动测试在真实 UE 世界中经游戏方法、输入动作委托和 HUD 点击区域分发驱动，全部静音；未测试实体键鼠、人工 PIE、鼠标焦点体验、音效听感或真实枪感。原生 UI 工具禁用，未绕过限制。

## 保留的 R3 与后续入口

R3 独立包保留在 `Packaged_R3/Windows`。修改前源码、Content、Config、Scripts 和文档快照为 `Backups/R3_20261007/GreyzoneRaid_R3_SourceContent.zip`，大小 **259,172,927 字节**；原 README 与测试结果另存同目录。R4 使用独立输出目录 `Packaged_R4/Windows`。

运行、控制与 C++ 阅读入口见 [README.md](README.md)，资源和许可见 [ASSET_SOURCES_R4.md](ASSET_SOURCES_R4.md)。本轮没有安装新组件、登录、购买或发布；新增外部资源仅为已记录的 OFL 字体。
