# R5 测试与交付记录

2026-10-08，时间均为 UTC。环境为本机 UE **5.8.3-58210709** / Windows，目标为 Win64 Development。R5 实现容器与背包双栏、逐条搜索揭示、手动转移、消耗品使用、搜索中断、同局容器状态保留，以及敌人巡逻恢复和安全掉包。

**R5 已完成编辑器四组、独立包六组自动验证，共 10 组运行、105 张原始 PNG；所有图片均通过实际尺寸检查。Source/Content 已冻结，构建、字体复查和 Win64 打包成功。实体键鼠、主观手感及音效试听未验收。**

## 已确认的编辑器与打包结果

下表时间取实际结果日志行；玩法测试用时为导演报告的 `elapsed`，不包含完整进程启动时间，和外层 JSON 回执的开始/结束时间差不同。

下表使用最新构建、字体、玩法及 `-ForceRes` 重跑证据。截图脚本读取 PNG 的 IHDR 实际宽高，不只核对启动参数或回执中的请求尺寸；归档小图只作为问题追溯证据。

| 检查 | 已确认结果 | 证据 |
| --- | --- | --- |
| 最新 Editor C++ 构建 | `Result: Succeeded`，**16.92 秒** | [EditorBuild_R5_Integrated_Console.log](Evidence/EditorBuild_R5_Integrated_Console.log) |
| 最新中文字体覆盖 | 需要 511 字符，世界图集已烘焙 522 字符；源字体与图集缺字均为 0 | [R5_FontValidation.json](Evidence/R5_FontValidation.json)、[R5_FontImport.json](Evidence/R5_FontImport.json)、`Evidence/R5_FontImport_Final.log` |
| 编辑器 1600×900 容器专项，含正常 AI 实战 | **152/152，0 失败**；13 张 PNG；03:18:25，61.28 秒；退出码 0；实际图片均为 1600×900 | [ContainerTest.log](Evidence/R5Container_Editor/ContainerTest.log)、[ContainerTestResult.json](Evidence/R5Container_Editor/ContainerTestResult.json) |
| 编辑器 1600×900 菜单与完整循环 | **251/251，0 失败**；12 张 PNG；03:19:18，37.03 秒；真实菜单退出，退出码 0；实际图片均为 1600×900 | [FlowTest.log](Evidence/R5Flow_Editor/FlowTest.log)、[FlowTestResult.json](Evidence/R5Flow_Editor/FlowTestResult.json) |
| 编辑器玩法回归（1280×720，无截图） | **75/75，0 失败**；03:10:35，**36.80 秒**；结果后请求正常退出 | [SelfTest_R5_Editor.log](Evidence/SelfTest_R5_Editor.log) |
| 编辑器 1600×900 真实画面演练 | **完成 15 张 PNG**；03:20:09；结果后正常 `LogExit`；实际图片均为 1600×900 | [VisualReview.log](Evidence/R5Visual_Editor/VisualReview.log)、同目录 PNG |
| 最新 Packaged_R5 打包 | **构建打包成功**；BuildCookRun **50.69 秒**；`BUILD SUCCESSFUL`，AutomationTool ExitCode=0；日志完成于约 03:11 UTC | [Package_R5.log](Evidence/Package_R5.log) |

运行入口为 `Packaged_R5/Windows/GreyzoneRaid.exe`。最新打包日志记录许可、来源说明与初学者指南已复制，独立包回归结果见下表。运行时必须保留整个 `Windows` 文件夹。修复视觉导演前的旧包完整保存在 `Backups/R5_BeforeVisualHarnessFix_20261008/Windows`。

正常 AI 阶段包含在每次容器专项运行内，不单独增加运行组数。修正分辨率前的 152/152、13 张截图仅作历史机制证据，其图片实际为 888×500；本记录的最终结果均采用重新执行并通过实际尺寸检查的新证据。

## 独立包实际结果

以下六组均来自本次 `Packaged_R5`，没有用编辑器结果代替。Container/Flow 的 JSON 回执记录数字退出码；SelfTest/Visual 以完成标记后的正常 `LogExit` 为退出证据，不补写没有回执支持的数字退出码。

| 独立包运行 | 实际结果 | 归档证据位置 |
| --- | --- | --- |
| 1600×900 容器专项，含正常 AI 实战阶段 | **152/152，0 失败**；13 张 PNG；03:21:39，58.16 秒；退出码 0 | [ContainerTest.log](Evidence/R5Container_Packaged/ContainerTest.log)、[ContainerTestResult.json](Evidence/R5Container_Packaged/ContainerTestResult.json) |
| 1600×900 菜单与完整循环 | **251/251，0 失败**；12 张 PNG；03:22:15，31.25 秒；真实菜单退出，退出码 0 | [FlowTest.log](Evidence/R5Flow_Packaged/FlowTest.log)、[FlowTestResult.json](Evidence/R5Flow_Packaged/FlowTestResult.json) |
| 玩法回归（1280×720，无截图） | **75/75，0 失败**；03:22:55，35.86 秒；结果后正常 `LogExit` | [SelfTest_R5_Packaged.log](Evidence/SelfTest_R5_Packaged.log) |
| 1600×900 真实画面演练 | **完成 15 张 PNG**；03:23:31；结果后正常 `LogExit` | [VisualReview.log](Evidence/R5Visual_Packaged/VisualReview.log)、同目录 PNG |
| 1280×720 容器专项与界面，含正常 AI 实战阶段 | **152/152，0 失败**；13 张 PNG；03:24:34，57.98 秒；退出码 0 | [ContainerTest.log](Evidence/R5Container_Packaged720/ContainerTest.log)、[ContainerTestResult.json](Evidence/R5Container_Packaged720/ContainerTestResult.json) |
| 1280×720 菜单与完整循环 | **251/251，0 失败**；12 张 PNG；03:25:09，31.08 秒；真实菜单退出，退出码 0 | [FlowTest.log](Evidence/R5Flow_Packaged720/FlowTest.log)、[FlowTestResult.json](Evidence/R5Flow_Packaged720/FlowTestResult.json) |

本轮已归档 **10 次运行、105 张 PNG**：编辑器四组包含 Container 13、Flow 12、Visual 15，共 40 张 1600×900 PNG；独立包主回归四组同样产生 40 张 1600×900 PNG；独立包另外两组 1280×720 的 Container/Flow 产生 25 张。脚本已逐图核对 PNG 签名与实际宽高。两次 SelfTest 都按 1280×720 运行且无截图，不计入图片尺寸统计。三次容器专项均包含 `12_live_combat_interrupt.png` 和 `13_live_combat_death.png`。预览图、旧日志副本和 JPEG 不重复计入 105 张。

精选 9 张 JPEG 已保存在 [Packaged_R5/Windows/Screenshots](Packaged_R5/Windows/Screenshots)，并保存到 Library：7 张更新已有文件，2 张为正常 AI 中断和死亡的新图。JPEG 用于便于查看的副本，原始 PNG 保留；没有重绘或替换游戏内容。

交付清单最终核对：**PASS**。`Scripts/audit_r5_delivery.py` 已核对 10 组运行、105 张原始 PNG 的尺寸、时间窗及结果行，以及字体覆盖、许可和指南副本、程序与内容哈希；打包后 Source/Content/Config 无修改。完整结果见 [R5_DeliveryManifest.json](Evidence/R5_DeliveryManifest.json)。

## 检查覆盖与证据边界

152、251、75 等数字是各导演的断言次数，包含重复状态检查，不代表同等数量的独立功能；最终采用修正运行参数后各次日志的实际断言数。

- **容器专项**：打开后保持未知；顺序定时揭示；揭示不入包；拿取和放回各转移一件；满包仍可搜索；弹药与医疗包明确使用；中断清零当前计时并保留既有揭示；同局复访、空箱保留、新局重置；敌人掉包、撤离与搜索期间死亡；过期界面按钮不能误操作新状态。
- **关闭后的控制恢复**：通过已经绑定的动作/轴委托驱动实际位移、视角变化和开火，检查一发弹药消耗及松开开火后跨多个射击间隔停止。验证的是 UE 内部输入路径，不是物理鼠标键盘。
- **正常 AI 实战阶段**：容器专项 Phase 2 保留地图原有 3 名守卫，另在受控场地添加 1 名原生守卫；全部 4 名守卫均核对正常的侦测、射程、伤害、间隔、Tick、移动 Tick 和 AIController，没有为本阶段停用或削弱守卫。归档机制验证显示真实首发攻击令玩家 **100→92 HP**，关闭面板并中断搜索，随后通过绑定输入验证恢复。
- **搜索中死亡的低血量夹具**：导演在确认真实搜索开始后将玩家生命设为 **4 HP**，再由正常守卫的生产攻击造成 **4→0 HP**，核对失败结算、面板关闭及停止搜索。本阶段不由导演直接调用 `ApplyDamage` 注入伤害，但确实使用了 4 HP 的测试设置；因此它证明的是致命受击的处理逻辑，**不证明从满血开始的自然生存时长**。其他阶段的确定性伤害夹具与此实战证据分开记录。
- **菜单与完整行动循环**：主菜单、操作说明、开始行动、容器搜刮、撤离成功、死亡、重开、主动放弃、返回菜单和真实退出；重新载入时核对新世界、玩家、敌人、容器、背包及初始数值。
- **玩法回归**：CharacterMovement 的移动、冲刺、跳跃与落地；瞄准、射击、连射、空弹匣、换弹；生命、容量与消耗品；导航、敌人发现/追击/伤害、真实掩体遮挡，以及撤离倒计时和结算。
- **中文与界面**：检查已嵌入的 Runtime 字体、世界文字 Offline 图集及字符覆盖；截图呈现未知、搜索、揭示、转移、背包与结算等状态。独立包 1600×900 和 1280×720 均完成相应流程与容器检查，图片实际尺寸符合各自目标。

另由审查实际查看 6 张 1280×720 与 1 张 1600×900 图片，未发现关键裁切、重叠或缺字；满包、满血禁用、空箱、敌人包以及实战受击/死亡状态与日志一致。这是指定图片的画面复核，不是对全部 105 张逐图人工验收，也不代表手感或音效验收。

自动验证在真实 UE 进程中运行，通过游戏方法、输入绑定委托和 Canvas HitBox 分发驱动。部分导演会传送玩家、设置测试夹具并控制当次运行的 AI 压力；这些临时状态不作为地图编辑保存。视觉导演停用背景 AI 仅用于隔离截图场景，不代替真实战斗验证，也不能解释此前画面失败的原因。正常战斗证据来自独立玩法回归与容器 Phase 2 的实际守卫攻击日志；低血量、场地布置及窗口尺寸的限制均单独披露。

原生桌面 UI 自动化不可用，没有注入物理键鼠，也没有把内部调用说成人工 PIE 试玩。全部自动运行使用 `-nosound`：**键鼠焦点体验、主观枪感、动画手感、音量平衡和音效试听均未验收。截图只证明对应运行画面，不能证明这些体验。**

## 保留的失败记录与修复

**首个独立包 Visual 失败：行动被放弃，不能归因 AI 击杀。** [VisualReview_R5_Packaged_FirstFailed.log](Evidence/VisualReview_R5_Packaged_FirstFailed.log) 中出现 `RAID_RESULT: DEAD`，但对应的[原始首张 PNG](Backups/R5_BeforeVisualHarnessFix_20261008/Windows/GreyzoneRaid/Evidence/R5Preview/01_depot_hip.png) 明确显示“行动已放弃”。HUD 只在失败且玩家仍存活时显示该标题；因此这不是玩家被敌人击杀的画面。`Back` / Escape 路径在行动中调用 `EndRaid(false)`，而该函数把放弃和死亡都记成 `DEAD`。旧日志没有记录触发 Back/ESC 的具体输入来源，不能进一步指认物理输入、自动导演或其他来源。

该轮已经进入失败结算，后续日志可见瞄准未生效、弹药未消耗，以及 `R5_VISUAL_DAMAGE: no live enemy damage before deadline`；这些不能算作有效动作截图验收。后续视觉隔离修改和异常结算防护已纳入最新成功构建；编辑器与独立包分别重新生成 15 张实际 1600×900 图片并正常退出。背景 AI 停用不作为“修复了 AI 击杀原因”的证据。旧包和失败图片完整保留在 `Backups/R5_BeforeVisualHarnessFix_20261008/Windows`。

视觉导演修改期间还出现一次测试代码局部变量 `Mode` 遮蔽导致的 C4456 构建失败，记录为 `Evidence/EditorBuild_R5_VisualIsolation_FirstFailed.log`；调整为 `ActiveMode` 后，最新构建已成功，用时 16.92 秒。失败记录保留。

**离屏截图尺寸不符。** 修正前容器已出现 152/152、流程 251/251、视觉演练 15 张的完成标记，但 PNG 的 IHDR 实际尺寸为 **888×500**。离屏虚拟桌面环境下 UE 的窗口尺寸限制使画面缩小，命令行写了 1600×900 不能代替图片尺寸证据。原图和日志归档于 `Evidence/R5Offscreen_BeforeForceRes/{R5Container_Editor,R5Flow_Editor,R5Visual_Editor}`，不计作最终 1600×900 验收。四个自动运行脚本补入 `-ForceRes`，产生 PNG 的三个脚本读取 IHDR 实际宽高；这是测试启动参数与证据检查修正，未改 Source/Content，也没有将旧小图放大冒充新图。编辑器与独立包全部最终图现已通过目标尺寸检查。

尺寸检查的第一版还曾直接对 Byte 移位，将实际 1600×900 误读为 64×132，导致外层脚本报错；该次容器内部虽然达到 152/152，但没有补造成功回执。三个 PNG 检查脚本改为先转 `[int]` 再移位后，重新执行编辑器三组并生成有效回执。最终采用的是重跑结果。

**首次 SelfTest 的四项失败。** [SelfTest_R5_Editor_FirstFailed.log](Evidence/SelfTest_R5_Editor_FirstFailed.log) 记录 74 次检查中 4 项失败，涉及敌人发现、实际伤害、掩体阻挡和移除掩体后的恢复。原因是前一阶段创建的三个测试箱仍挡在独立 AI 视线路径上，敌人持续 Patrol，玩家生命保持 100。

修复仅在空箱持久性检查完成后，清理该导演自己创建的三个箱子，再进入独立 AI 阶段。原四条 AI 断言与空箱断言均保留，并增加清理后视线断言。最终日志的 `RAID_SELFTEST_FIXTURE_CLEANUP` 显示 `removed=3`，清理前命中 `RaidLoot_1`，之后射线到达 `BP_RaidPlayer_C_0`；最终实测为 75/75。没有通过削弱生产敌人逻辑或删除失败断言来获得通过。

**首次 R5 字体导入失败。** `Evidence/R5_FontImport.log` 记录已有世界字体被引用，重建路径未能创建字体并触发 `World offline font creation failed`。改用保留引用的原位导入任务后成功，过程记录在 `R5_FontImport_Retry.log`。界面文案完善后，第一次覆盖复查再次发现新增字符尚未烘焙，失败记录保存在 `R5_FontValidation_PostPolish_First.log`；随后完成最终重导入与复查。当前 `R5_FontValidation.json` 确认 522 个已烘焙字符覆盖 511 个当前所需字符，两个 missing 数组均为空。

`R5_FontImport_Final.log` 的引擎初始化阶段仍保留 `UnifiedErrorTest` 输出及 13 条 `LogAutomationTest: Error: Condition failed`。因此本记录不声称该导入日志“零错误”；字体完成依据是随后出现的 `R5_FONT_IMPORT_OK`、实际保存的字体资产，以及独立字符覆盖与字体载荷核对。原始 Noto OTF 与许可沿用已有版本，未安装系统字体。

继续完善前的核心流程证据单独保存在 `Evidence/R5Core_BeforePolish/`，当时容器断言为 117；其后已有一轮编辑器容器结果为 123，但尚未包含新增正常 AI 阶段。早期预览与历史检查点保留用于追溯，不计入最终清单。

## 构建警告与原型限制

最新 `Package_R5.log`（BuildCookRun 50.69 秒）的 Cook 汇总为 **0 errors / 27 warnings**。27 条警告是 `/Game/Maps/Depot` 中由 FString 初始化的 FText 保存提示，日志的去重摘要列出 4 项。该次打包日志另有 **2 条** MSVC 14.51.36260 非引擎首选版本提示，以及 **4 条**来自引擎 `Character.h` 的 C4996 弃用 API 提示；这里没有沿用首包的 11 条计数。上述数量分别统计，不把摘要重复算成新增警告，也不声称整份打包日志没有警告。它们没有阻止本次构建、Cook、Stage 和 Archive 完成。

本项目采用固定的 **2–3 个初始容器条目与 8 件背包容量**，按数量管理拿取、放回和消耗，是为了便于学习 C++ 的简化规则。未实现物品格子体积、重量负担、随机经济、联机或干员技能；这些不在通过项中。搜索中断清零当前条目计时属于本原型规则，不声称逐点复刻商业游戏。动画贴合、守卫表现和人工操作体验也不能仅凭自动断言或截图认定达到商业游戏水平。

## 文档与版本

操作和模块阅读入口见 [R5_BEGINNER_GUIDE.md](R5_BEGINNER_GUIDE.md)，来源与许可见 [ASSET_SOURCES_R5.md](ASSET_SOURCES_R5.md)。项目仍使用既有第三方素材；R5 没有新增外部艺术资源下载，没有新增安装、账号登录或代为接受协议。

R4 历史包保留在 `Packaged_R4/Windows`，修改前快照为 `Backups/R4_20261008/GreyzoneRaid_R4_SourceContent.zip`，历史说明与结果见 [README_R4.md](README_R4.md)、[TEST_RESULTS_R4.md](TEST_RESULTS_R4.md)。本记录对应上述 R5 最终自动运行；早期失败、开发检查点与旧版文件分别保留。
