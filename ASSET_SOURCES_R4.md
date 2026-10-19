# R4 资源来源与中文化记录

记录日期：2026-10-07。R4 延用 [R3 派生资源记录](ASSET_SOURCES_R3.md) 与 [R2 来源和许可记录](ASSET_SOURCES_R2.md)。本轮唯一新增外部资源为中文字体；没有新增外部模型、贴图、动作或声音，没有安装新组件、登录账号、购买或发布，也没有使用商业游戏提取资源。

## 新增字体

| 内容 | 来源与许可 | 本地记录 |
|---|---|---|
| Noto Sans CJK SC Regular 2.004 | 官方 `notofonts/noto-cjk`，固定提交 `f8d157532fbfaeda587e826d4cd5b21a49186f7c`；SIL Open Font License 1.1 | `Downloads/Fonts/NotoSansCJKsc-Regular.otf`、`LICENSE.NotoSansCJK.txt`、`COPYRIGHT.NotoSansCJK.txt`、`SOURCES.json` |
| HUD Runtime 字体 | 使用完整原始 OTF，通过 `FF_RaidChinese` 的 Inline 载荷嵌入，`F_RaidChinese` 提供复合字体入口 | `Content/UI/Fonts/`；`Scripts/import_r4_font.py` |
| 世界 Offline 字体 | 同一原始字体生成 `F_RaidChineseWorld` 单页图集；487 字符，当前所需 485 字符全部覆盖 | `Evidence/R4_FontImport.json`、`Evidence/R4_FontValidation.json` |

完整官方下载链接、文件大小与 SHA-256、版权、OFL 分发条件、导入步骤及引擎实现核对均见 [ASSET_FONT_R4.md](ASSET_FONT_R4.md)。玩家不需要安装字体；未复制 Windows 系统字体。导入只在当前进程临时注册下载字体，退出前移除。

`Scripts/Package.ps1` 在打包成功后将完整 OFL 许可和版权文件复制至 `Packaged_R4/Windows/ThirdPartyLicenses/`。字体字节嵌入 Cook 后的资源，独立包不依赖 `Downloads` 或本机字体目录。独立包实际加载与最终画面验收仍以 [TEST_RESULTS.md](TEST_RESULTS.md) 的 R4 结果为准。

## 项目内修改与延用内容

| 内容 | 来源与处理 | 复现 / 证据 |
|---|---|---|
| 菜单、说明、HUD、背包、结算 | 项目原有 C++ Canvas 布局改为中文，并调整文字测量、字号与布局 | `Source/GreyzoneRaid/RaidHUD.h/.cpp` |
| 物资、搜索、治疗与撤离提示 | 项目玩法源码中的中文显示文字；物资内部 ID 和玩法数值保持原有语义 | `RaidPlayer`、`RaidLoot`、`RaidExtractionZone`、`RaidGameMode` |
| 地图标牌和保存的物资名称 | 项目编写中文词表并迁移 Depot、RaidYard；103 个世界文字组件应用中文字体，保留现有几何与摆放 | `Scripts/r4_text_catalog.json`、`Scripts/localize_r4_map.py`、`Evidence/R4_MapLocalization.json` |
| 世界标签字体辅助模块 | 项目新增 `RaidText`，为 TextRender 组件加载并应用 Offline 字体 | `Source/GreyzoneRaid/RaidText.h/.cpp` |
| 枪械、手臂、手套、环境、声音与守卫 | 延用 R3/R2 已记录资产；CC0 美术与音效、Epic Manny / 模板动画继续分别记录许可 | [R3 记录](ASSET_SOURCES_R3.md)、[R2 记录](ASSET_SOURCES_R2.md) |

截图由 UE 实际运行输出，保留原始 PNG，JPEG 仅用于方便查看；未合成或修饰游戏画面。自动测试和截图演练静音，声音资源来源继续沿用 R2 记录。

R3 独立包保留在 `Packaged_R3/Windows`；修改前快照为 `Backups/R3_20261007/GreyzoneRaid_R3_SourceContent.zip`（259,172,927 字节），原 README 与测试结果另存同目录。R4 输出目标为 `Packaged_R4/Windows`，构建、打包与验收是否完成请查看最终测试记录。
