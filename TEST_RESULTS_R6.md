# R6 验证与交付记录

日期：2026-10-08。时间采用 UTC；环境为本机 Windows / Unreal Engine 5.8.3-58210709，Win64 Development。

**最终代码已通过 Editor 构建和独立包 BuildCookRun，12 组功能回归全部通过：1499 次断言、115 张 PNG，UE 与包装脚本退出码均为 0。基线与最终版本共 12 个性能进程完成，整体交付审计为 PASS。**

本轮 Framework 已加入三个生产物品目录断言，成功门槛由 67 提高到 70。此前 67 项版本的回归属于历史证据，不代替本轮结果。

## 最终构建依据

| 项目 | 已确认结果 | 原始证据 |
| --- | --- | --- |
| Editor C++ 构建 | `Result: Succeeded`，32.63 秒 | [R6_EditorBuild_Final.log](Evidence/R6_EditorBuild_Final.log) |
| 独立包构建、cook、stage、archive | `BUILD SUCCESSFUL`，BuildCookRun 60.38 秒；AutomationTool 退出码 0 | [R6_Package.log](Evidence/R6_Package.log) |
| 独立包入口 | `Packaged_R6/Windows/GreyzoneRaid.exe`；回执另外记录实际 `Binaries/Win64` 内层 exe | [Packaged_R6](Packaged_R6/Windows/) |

最终回执记录以下输入指纹；完整逐文件清单位于各回执。最终审计已确认全部回执与当前工程一致。

- Source 聚合 SHA-256：`60e3c8def509f2d58f6f23f27d58d88cac1a8099ecd2fdac3aa7d64bab671271`
- Source、Content、Config、uproject 聚合 SHA-256：`140a228b3decc73a74345ec438dd438a3e8fca50b3c737e05177ba15a20d226f`

## 最终功能与截图回归

以下是本轮固定组合。Framework、Container、Flow、Self 的数字为实际断言门槛；Visual 的 15 指截图数量，没有将截图数充当断言数。

| 模式 | 套件 | 分辨率 | 本轮实际结果 | 本轮回执 |
| --- | --- | --- | --- | --- |
| Editor | Framework | 1600×900 | **70/70，0 失败；0 PNG；UE 与包装脚本退出码均为 0** | [receipt.json](Evidence/R6Final/Editor_Framework_1600x900/receipt.json) |
| Editor | Container | 1600×900 | **152/152，0 失败；13 PNG；UE 与包装脚本退出码均为 0** | [receipt.json](Evidence/R6Final/Editor_Container_1600x900/receipt.json) |
| Editor | Flow | 1600×900 | **251/251，0 失败；12 PNG；UE 与包装脚本退出码均为 0** | [receipt.json](Evidence/R6Final/Editor_Flow_1600x900/receipt.json) |
| Editor | Self | 1280×720 | **75/75，0 失败；5 PNG；UE 与包装脚本退出码均为 0** | [receipt.json](Evidence/R6Final/Editor_Self_1280x720/receipt.json) |
| Editor | Visual | 1600×900 | **完成 15 PNG；UE 与包装脚本退出码均为 0** | [receipt.json](Evidence/R6Final/Editor_Visual_1600x900/receipt.json) |
| Packaged | Framework | 1600×900 | **70/70，0 失败；0 PNG；UE 与包装脚本退出码均为 0** | [receipt.json](Evidence/R6Final/Packaged_Framework_1600x900/receipt.json) |
| Packaged | Container | 1600×900 | **152/152，0 失败；13 PNG；UE 与包装脚本退出码均为 0** | [receipt.json](Evidence/R6Final/Packaged_Container_1600x900/receipt.json) |
| Packaged | Flow | 1600×900 | **251/251，0 失败；12 PNG；UE 与包装脚本退出码均为 0** | [receipt.json](Evidence/R6Final/Packaged_Flow_1600x900/receipt.json) |
| Packaged | Self | 1280×720 | **75/75，0 失败；5 PNG；UE 与包装脚本退出码均为 0** | [receipt.json](Evidence/R6Final/Packaged_Self_1280x720/receipt.json) |
| Packaged | Visual | 1600×900 | **完成 15 PNG；UE 与包装脚本退出码均为 0** | [receipt.json](Evidence/R6Final/Packaged_Visual_1600x900/receipt.json) |
| Packaged | Container | 1280×720 | **152/152，0 失败；13 PNG；UE 与包装脚本退出码均为 0** | [receipt.json](Evidence/R6Final/Packaged_Container_1280x720/receipt.json) |
| Packaged | Flow | 1280×720 | **251/251，0 失败；12 PNG；UE 与包装脚本退出码均为 0** | [receipt.json](Evidence/R6Final/Packaged_Flow_1280x720/receipt.json) |

最终功能回执覆盖至 `2026-10-08T08:47:20.434430+00:00`：12 次运行、1499 次断言、115 张 PNG 均已归档，所有回执的 Source 与完整输入聚合哈希一致。主验证流水线退出码为 0，见 [R6_Final_Verification.log](Evidence/R6_Final_Verification.log)。

本轮共完成 12 次运行、115 张 PNG、1499 次重复执行的断言：`2 × (70 + 152 + 251 + 75) + 152 + 251 = 1499`。这不是 1499 种独立行为。截图实际为 Editor 45 张、Packaged 主组合 45 张、额外 720p 组合 25 张。各回执经过 capture 校验，跨回执与当前交付文件的一致性已通过最终审计。

Self 成功路径固定为 `01_combat_inventory.png`、`02_extracted_success.png`、`03_dead_failure.png`、`04_restart_clean.png`、`05_inventory_panel.png`，共 5 张；失败分支截图不能计入成功交付数量。

## 生产目录断言与覆盖范围

Framework 的以下三项必须分别出现 `RAID_FRAMEWORK_CHECK:PASS`，并位于最终结果之前；仅有总数 70 不足以通过 Capture 或 Audit。

1. `production_catalog_asset_loaded_without_fallback`：生产 Player 使用真实 `/Game/Data/DA_RaidItems.DA_RaidItems` 资产，验证资产身份、目录有效性和定义集合。
2. `production_catalog_medkit_grant_and_use`：经生产 `Player.AddItem` 解析定义、入包，再走实际使用流程验证医疗效果和消耗。
3. `production_catalog_ammo_grant_and_use`：经相同生产入口验证弹药定义、使用效果及消耗；入包与使用仍是两个动作。

Framework 同时保留 `kernel`、`registered_components`、`interaction_lifecycle` 三阶段。Container 覆盖真实开箱、逐件搜索、手动拿取、容量、放回、消耗、过期 UI 操作、访问中断、敌人掉落、撤离／死亡／重开及正常 AI 搜刮风险。Flow 保留实际 Canvas 命中与点击分发、菜单和连续局流程；Self 保留移动、射击、瞄准、换弹、AI、遮挡、医疗和结算回归；Visual 生成真实渲染截图，包括敌人受击和实际 AI 伤害阶段。

这些覆盖说明对应导演的验证路径；最终通过状态仍以本轮表格和日志为准。

## 性能实验

四组实验均已完成，每组 3 个独立进程，每进程 7 个正常规模场景、每场景 48 批次，产生 336 个原始批次样本；全部 12 个进程退出码为 0，共 4032 个批次样本。PASS 表示查询结果及证据产物校验通过，不是速度必须提高的断言。

| 版本与模式 | 独立进程 | 每进程场景／样本 | 结果与原始证据 |
| --- | --- | --- | --- |
| R5 基线，Editor | 3 | 7／336 | [PASS，跨进程汇总](Evidence/R6FrameworkBenchmark_Editor_20261008_072902_6641308Z/FrameworkBenchmarkAcrossRuns.csv) |
| R5 基线，Packaged | 3 | 7／336 | [PASS，跨进程汇总](Evidence/R6FrameworkBenchmark_Packaged_20261008_073419_5844636Z/FrameworkBenchmarkAcrossRuns.csv) |
| 最终 R6，Editor | 3 | 7／336 | [PASS，跨进程汇总](Evidence/R6FrameworkBenchmark_Editor_20261008_084823_3442848Z/FrameworkBenchmarkAcrossRuns.csv) |
| 最终 R6，Packaged | 3 | 7／336 | [PASS，跨进程汇总](Evidence/R6FrameworkBenchmark_Packaged_20261008_084954_4293763Z/FrameworkBenchmarkAcrossRuns.csv) |

最终后测在 `2026-10-08T08:48:21.7386409Z` 至 `2026-10-08T08:50:50.4363245Z` 独立串行执行。[R6PostBenchmarkBuildBinding.json](Evidence/R6PostBenchmarkBuildBinding.json) 状态为 **PASS**：运行前后 43 个 Source 文件、实际 Editor 项目 DLL 和包内 exe 的记录一致。中间版本的后测目录不作为最终 R6 结果。数值分析见 [R6_ENGINE_EXPERIMENT.md](R6_ENGINE_EXPERIMENT.md)，本报告记录实验是否完成及其边界。

方法固定为同机、同 Depot 地图、同 1280×720 离屏渲染；初始至少 10 秒及 120 帧热身，每场景再热身，校准约 2 毫秒的批次，一帧执行一批。记录原始批次、摘要、输入／执行文件信息和游戏世界规模。基线实际为 1 个撤离点、954 个世界 actor 槽位、953 个有效 actor；普通背包测试为 0／3／8 行。

测量调用真实 HUD 查询，完整包含 iterator 构造和析构；Windows QPC 记录作用域经过时间，包含调度噪声。结果保留间接调用、校验和及测试框架开销，单列空框架；批次均值的 P95 不能解释成单帧延迟 P95。

R5 `CountMedkits` 按 `ItemId == Medkit` 遍历背包，R6 从库存变更事件维护的缓存读取、按 `UseEffect == Heal` 计数。夹具上的返回值一致，但这不是“完全相同语义下只优化循环”；缓存建立与刷新成本在查询批次之外。`FindExtraction` 保留原查询算法。Editor 与独立包的 UE iterator 编译路径不同，结果分开报告。

**这些实验不证明 FPS 提升，也没有设置性能提升比例作为通过条件。** 未执行 32／256 撤离点的可选放大实验，未用 Unreal Insights 验证整帧瓶颈。

## 已修复问题及历史证据

| 历史问题 | 修复与最终验证方式 | 保留证据 |
| --- | --- | --- |
| 首次 Editor 编译 C4458 | `RaidPlayerController` 的参数／局部 `Player` 遮蔽继承成员，重命名后编译通过；本轮以 32.63 秒的最终构建为准 | [R6_Initial_EditorBuild.log](Evidence/R6_Initial_EditorBuild.log) |
| 首次独立包漏 cook 生产目录 | 当时 Framework 打印 67 项 PASS，但运行日志同时出现 `LogTemp: Error: RAID_ITEM_CATALOG: missing/invalid player catalog`，因此该包不合格。Config 增加 `/Game/Data` 的必 cook 目录，并新增上述三个生产路径断言；本轮重新构建打包并重跑固定组合 | [失败运行日志](Evidence/R6_Framework_Packaged_Attempt1_MissingCatalog.log)、[首次打包日志](Evidence/R6_Package_Attempt1_MissingCatalog.log) |

旧 67 项版本的整轮结果保存在 `Evidence/R6PreCatalogAssertion` 等历史证据中；它们用于追溯问题，不计入本轮 115 张截图或最终通过表。

## 交付审计与边界

审计工具为 [audit_r6_delivery.py](Scripts/audit_r6_delivery.py)。最终 [R6_DeliveryManifest.json](Evidence/R6_DeliveryManifest.json) 在 `2026-10-08T08:52:03.839593+00:00` 记录 **PASS**：12 次功能运行、115 张截图、12 个性能进程，错误列表为空。审计进程退出码为 0，摘要见 [R6_DeliveryAudit.log](Evidence/R6_DeliveryAudit.log)。

最终审计要求所有回执与当前 Source、Content、Config、uproject 的逐文件哈希一致，记录实际 Editor exe＋项目 DLL或独立包内层 exe，并由 UE 日志的 `Base Directory` 校验运行身份。截图检查文件名、数量、时间窗口、PNG CRC、zlib 数据、扫描行和实际尺寸；UE 原生退出码与包装脚本退出码分别取证。功能结果中的 PASS 不会覆盖 `LogTemp: Error:`、失败或致命错误。

Source 时间对应编译后的游戏 binary；Content／Config／uproject 时间对应 cook 载荷，包内 `.pak/.utoc/.ucas` 哈希与成功 UAT 日志共同归档。仅改数据或配置后重新 cook 可以合法复用 exe，因此不能用 exe 时间代替 cook 时间。最终审计已收录 12 个性能进程的独立元信息与历史输出哈希，不作 FPS 断言。

未做实际鼠标／键盘硬件输入验收、手工操作手感验收或音频听测；自动调用真实绑定／Canvas 分发不能代替这些测试。PNG 结构审计也不能代替人工画面、字形与交互体验审阅。
