# R5 资源来源与交互参考

记录日期：2026-10-08。R5 沿用 [R4 资源记录](ASSET_SOURCES_R4.md)、[R3 派生记录](ASSET_SOURCES_R3.md) 和 [R2 来源与许可记录](ASSET_SOURCES_R2.md)。本轮未新增外部模型、贴图、音效或商业游戏艺术资源；中文显示继续使用已有 Noto 字体，并扩充世界标签所需的烘焙字符子集。项目仍包含第三方素材，不能把“R5 未新增下载”表述成“项目没有第三方资源”。

## 本轮新增与修改

| 内容 | 来源与处理 | 对应文件 |
| --- | --- | --- |
| 容器/背包双栏、按钮、未知条目与搜索进度 | 项目 C++ Canvas 绘制的布局、色块、线条与中文文案；未导入参考游戏界面图片。 | `Source/GreyzoneRaid/RaidHUD.h/.cpp` |
| 搜索、揭示、逐件拿取/放回、消耗品使用 | 项目 C++ 玩法逻辑；已揭示与已获得分开记录。 | `RaidPlayer.h/.cpp`、`RaidLoot.h/.cpp` |
| 初次开箱、已开箱和空箱表现 | 使用已有 UE 基础几何体和项目材质，增加箱盖运动与状态显示。 | `RaidLoot.h/.cpp` |
| 敌人巡逻恢复与安全掉包 | 项目 C++ 导航、地面/空间/遮挡检查；掉落容器复用本轮搜刮流程。 | `RaidEnemy.h/.cpp` |
| R5 中文字符覆盖 | 继续使用 R4 保存的同一份 Noto Sans CJK SC Regular 2.004；扩充世界文字 Offline 字体子集。 | `Scripts/import_r5_font.py`、`Scripts/r5_text_catalog.json`、`Evidence/R5_FontImport.json` |
| 专项测试与预览 | 项目 UE 内部测试导演和实际运行截图；自动运行静音。测试场地、原生守卫与低血量夹具不构成新增外部艺术资源。 | `RaidContainerTestDirector.h/.cpp`、`Scripts/ContainerTest.ps1`；阶段与最终证据分开归档。 |

本轮不需要安装新组件、登录账号或接受新协议。网页仅用于玩法研究，没有从商业游戏提取或下载模型、贴图、音频、视频素材或界面资源。

## 继承资产与许可类别

| 资产 | 既有来源与归属 | 当前记录 |
| --- | --- | --- |
| AK47、R3 独立枪身与弹匣 | Stein Games，Classic Weapons Pack v1.1；既有记录为 CC0 1.0。R3 拆分派生保留原始模型属性。 | `ASSET_SOURCES_R2.md`、`ASSET_SOURCES_R3.md`；包内 `Downloads/Models/SteinGames/Extracted/WeaponsPack/license.txt`。 |
| 第一人称手臂、骨架与 R3 派生动画 | para；原人体网格与贴图归功于 MakeHuman 团队，既有记录为 CC0。握持/开火/换弹关键帧由项目制作。 | `ASSET_SOURCES_R2.md`、`ASSET_SOURCES_R3.md`。 |
| 水泥、沥青、铁皮、木板、锈金属贴图 | ambientCG / Poly Haven；各作者及 CC0 来源链接见 R2 表格。 | `ASSET_SOURCES_R2.md`、`Downloads/resource_manifest_R2.json`。 |
| 枪声与拟音 | The Free Firearm Sound Library 的 Ben Jaszczak、Brian Nelson、Kevin Heras、Matthew Nanney；Kenney Impact Sounds。既有记录为 CC0。 | `ASSET_SOURCES_R2.md`、`Downloads/Audio/Prepared/audio_manifest.json`；Kenney 原始 `License.txt`。 |
| 敌人 Manny、骨架及 UE 模板动画 | 来自本机已安装 Epic / Unreal Engine 模板，沿用其对应许可记录；不归入 CC0，也不称为项目原创人物。 | `ASSET_SOURCES_R2.md` 中“本机 Epic 模板”一节。 |
| UE 基础几何体、引擎组件 | 已安装 Unreal Engine 内容。 | `ASSET_SOURCES.md` 的引擎内容条目；该文件是 R1 历史记录，首段不代表 R5 的第三方资产现状。 |
| 中文字体 | Noto Sans CJK SC Regular 2.004，SIL Open Font License 1.1。 | [ASSET_FONT_R4.md](ASSET_FONT_R4.md) 与下节。 |

音效文件存在及来源记录不代表音效已经试听验收。自动测试与截图演练使用 `-nosound`，本轮不声称已经验证真实键鼠手感、窗口焦点或声音效果。

## 字体复用与随包文件

字体继续使用官方 [notofonts/noto-cjk](https://github.com/notofonts/noto-cjk) 固定提交 `f8d157532fbfaeda587e826d4cd5b21a49186f7c` 的原始 OTF，原始字体字节与许可文件未更换。来源、原始版权文本、导入方式及历史核对详见 [ASSET_FONT_R4.md](ASSET_FONT_R4.md)。

| 现有文件 | 用途 |
| --- | --- |
| `Downloads/Fonts/NotoSansCJKsc-Regular.otf` | 原始字体，SHA-256：`2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`。 |
| `Downloads/Fonts/LICENSE.NotoSansCJK.txt` | 原始 SIL OFL 1.1 许可文本。 |
| `Downloads/Fonts/COPYRIGHT.NotoSansCJK.txt` | 从原始字体提取的版权记录。 |
| `Downloads/Fonts/SOURCES.json` | 固定来源、版本、字节数和哈希记录。 |

HUD 的 `F_RaidChinese` / `FF_RaidChinese` 继续通过 Inline FontFace 嵌入原始字体；世界标签使用 `F_RaidChineseWorld` 的 Offline 字形图集。R5 只扩充已有字体的世界标签字符覆盖，不需要把字体安装到 Windows。修改中文文案后需更新字符覆盖并重新核对，不能直接沿用 R4 的字符数量作为 R5 验收数据。

最新 [R5_FontValidation.json](Evidence/R5_FontValidation.json) 记录当前需要 511 个字符，世界图集已烘焙 522 个字符，源字体及世界子集缺字均为 0。独立包完整运行结果见 [TEST_RESULTS.md](TEST_RESULTS.md)，字体资产哈希保存在核对 JSON 中。

以下均为已经存在的许可或来源文件，可供 R5 打包核对。来源清单用于追溯，不应冒充原作者许可原文。

| 现有路径 | 随包用途 |
| --- | --- |
| `Downloads/Fonts/LICENSE.NotoSansCJK.txt`、`Downloads/Fonts/COPYRIGHT.NotoSansCJK.txt` | 保留字体许可与版权；目标目录 `Packaged_R5/Windows/ThirdPartyLicenses/`。 |
| `Downloads/Models/SteinGames/Extracted/WeaponsPack/license.txt` | Stein Games 包内 CC0 许可及作者记录；复制时使用明确名称，避免与其他 `License.txt` 重名。 |
| `Downloads/Audio/KenneyImpact/Extracted/License.txt` | Kenney Impact Sounds 包内 CC0 许可及作者记录。 |
| `ASSET_SOURCES_R2.md`、`ASSET_SOURCES_R3.md`、`ASSET_SOURCES_R4.md`、`ASSET_FONT_R4.md`、`ASSET_SOURCES_R5.md` | 随包保留完整来源、派生过程、第三方类别和字体说明。 |
| `Downloads/resource_manifest_R2.json`、`Downloads/Audio/Prepared/audio_manifest.json` | 可选审计附件，记录资源文件及音效派生来源；不是许可文本。 |

未在既有 FPSArms / FreeFirearm 解压树中找到独立 LICENSE 文本；其作者、来源页面和既有许可记录保存在 `ASSET_SOURCES_R2.md`，本轮没有替原作者编造许可文件。最新 `Evidence/Package_R5.log` 记录 BuildCookRun 50.69 秒、ExitCode=0，且许可、来源文档及指南已复制；随包来源与许可保存在 `Packaged_R5/Windows/ThirdPartyLicenses/`，交付核对统一记录于 [TEST_RESULTS.md](TEST_RESULTS.md)。

## 玩法参考及边界

- [用户提供的参考视频](https://www.youtube.com/watch?v=1NjvTrqUNmY)：保留为玩法研究入口，不作为已逐帧核对或下载视频素材的证明。
- [17173：搜容器双击 F、钻保险按 ESC 的技巧文章](https://news.17173.com/content/01082026/132443673.shtml)：文章讨论首次打开与已经打开容器的表现差异，用于区分容器状态的交互设计参考。它是攻略文章，不能当作参考游戏所有版本的完整规则规格。

R5 实际采用的规则是：按 E 打开容器，点击按钮逐条搜索，已揭示物资逐件拿取；取消搜索后当前未完成计时清零，已完成揭示在同一局保留；空箱保留，新局重置。这些由项目代码定义，不声称逐点复刻《三角洲行动》，也未实现参考文章中的双击 F 或保险箱小游戏技巧。

## 截图与测试素材边界

截图来自真实 UE 运行。初次预览在 `Evidence/R5Visual_FirstPreview/`；后来一轮离屏截图受窗口尺寸限制，PNG 实际为 888×500，已保留在 `Evidence/R5Offscreen_BeforeForceRes/`。它们不充当 1600×900 证据。增加 `-ForceRes` 并修正 PNG 实际宽高读取后，编辑器已重新生成并验证 40 张 1600×900 PNG（容器 13、流程 12、视觉演练 15）；SelfTest 使用 1280×720 且无截图，不混入图片统计。未通过放大旧图、重绘或合成补足验收画面，此项修正未改 Source/Content。

视觉导演停用背景 AI 仅为隔离截图场景。容器正常 AI 阶段保留地图 3 名守卫，另加 1 名原生守卫，全部核对正常默认战斗参数；真实首发攻击为 100→92 HP。死亡检查明确把玩家设置为 4 HP，再由守卫实际射击造成 4→0，用于验证死亡处理，不能解释为满血自然生存时长或手感验收。本阶段的测试设置不是玩家正常出生配置，也没有使用新增外部角色素材。

最终已完成 10 组运行，归档 105 张原始 PNG：80 张实际为 1600×900，25 张实际为 1280×720，均通过脚本尺寸检查。精选 9 张 JPEG 保存于 `Packaged_R5/Windows/Screenshots` 并已保存到 Library，作为便于查看的副本，未替换原始 PNG。最终专测、独立包运行与证据范围见 [TEST_RESULTS.md](TEST_RESULTS.md)。
