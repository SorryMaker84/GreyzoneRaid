# 灰域突围 / Greyzone Raid — R4 中文单机搜打撤

UE 5.8.3 / Windows 的小型第一人称原型，核心逻辑为可读 C++。启动主菜单 → 开始行动 → 搜索、交火与撤离 → 成功或死亡结算 → 返回菜单或再来一局。地图约 48 × 38 米，包含集装箱、仓库外区、医疗补给与东北角 E-07 撤离站。没有账号、联网、商城或局外仓库经济。

R4 将主菜单、说明、战斗 HUD、物资与撤离提示、背包、结算及地图标牌改为中文。键位名称和区域编号保留。中文字体随项目和游戏包分发，无需安装字体。编辑器与独立包均通过菜单循环 198/198、玩法回归 67/67，独立包另通过 1280×720 界面验证。完整结果见 [TEST_RESULTS.md](TEST_RESULTS.md)，交付记录见 [R4_CHECKPOINT.md](R4_CHECKPOINT.md)。

## 打开与运行

项目路径：`C:\Users\ggbo\Documents\Codex\2026-10-06\task\GreyzoneRaid`。

- 已验证的 R4 独立包：运行 `Packaged_R4\Windows\GreyzoneRaid.exe`。需保留整个 Windows 文件夹，不能只复制入口 exe。具体测试边界见 [TEST_RESULTS.md](TEST_RESULTS.md)。
- 编辑器：双击 `GreyzoneRaid.uproject`，打开 `Content/Maps/Depot`，点击 Play，再点击视口。首次进入主菜单。Shift + F1 释放鼠标，编辑器 Stop 结束 PIE。
- 使用本机 UE 启动可见独立窗口：在项目目录执行 `.\Scripts\Run.ps1`。正常游玩有声音；自动测试全部静音。

菜单按钮为“开始行动”“操作说明”“退出游戏”；说明页和结算页可“返回主菜单”，结算后可“再次行动”。开始与重开均重新加载保存的 Depot，恢复生命、弹药、敌人、物资和空背包。

## 操作与目标

| 操作 | 按键 |
|---|---|
| 移动 / 视角 | W A S D / 鼠标 |
| 跳跃 / 冲刺 | Space / 按住左 Shift |
| 开火 / 连射 | 鼠标左键 / 按住左键 |
| 机瞄 / 换弹 | 按住右键 / R |
| 搜索物资 | 瞄准近处容器或物资，按住 E 约 1.1 秒；松开或移开视线取消 |
| 背包 | Tab；查看容量、数量和带出价值，游戏不会暂停 |
| 使用医疗包 | Q；需要已拾取医疗包 |
| 菜单开始 / 结算重开 | Enter |
| 说明返回 / 结算回菜单 | Escape |
| 行动中放弃本局 | Escape；进入失败结算，本局物资丢失 |

至少携带一件物资，沿罗盘“撤离点”指引到地图东北角的 E-07。进入地面标记范围后保持 8 秒；离开或受伤会重置倒计时。成功结算带出数量和价值；死亡或放弃没有收益。价值仅为当局统计，没有持久货币或局外交易。

背包上限 8 件，满包不销毁未拾取物资。医疗包恢复最多 45 HP、上限 100，并消耗一件。步枪弹匣 12 发、初始储备 60 发、命中伤害 34、换弹 1.5 秒。弹药箱增加储备弹药，也计入携带数量。敌人依靠视线发现玩家，使用 UE 导航巡逻、追击与短距离换位；墙体和掩体可挡住射线。

## 从哪里读 C++

代码在 `Source/GreyzoneRaid`，每个模块为同名 `.h / .cpp`。

| 模块 | 职责 |
|---|---|
| RaidGameMode | 主菜单、说明、部署、结果、输入模式与干净重开 |
| RaidHUD | Canvas 可点击菜单、说明、生命弹药、搜索进度、受击方向、背包与结算 |
| RaidPlayer | 第一人称移动、射击、机瞄、换弹、手臂动画、库存与治疗 |
| RaidEnemy | 巡逻、视线检查、追击、举枪攻击、命中反馈和侧移 |
| RaidLoot | 物资类型、数量、价值、交互与拾取 |
| RaidExtractionZone | 撤离范围、物资条件、倒计时、离开/受伤复位 |
| RaidText | 为世界中的 TextRender 标签加载并应用中文 Offline 字体；HUD 使用单独的 Runtime 字体 |
| RaidNavBounds | 动态导航范围 |
| RaidTestDirector | 玩法集成验证：方法和输入动作委托、碰撞、计时器、导航与跨关卡重开 |
| RaidFlowDirector | 主菜单点击区域与真实完整循环验证、跨关卡新局检查及截图 |
| RaidVisualDirector | 受控实机截图演练；仅演练过程抑制 AI 压力 |

蓝图配置在 `Content/Blueprints`：`BP_RaidPlayer`、`BP_DepotGuard`、`BP_DepotSupply`、`BP_DepotExtraction`、`BP_DepotGameMode`。修改 Class Defaults 后 Compile、Save。敌人 PatrolPoints 是世界坐标（厘米）。UE 组件、导航、材质和蓝图负责数据配置，玩法不依赖复杂蓝图逻辑。

初读建议按 `RaidGameMode → RaidPlayer → RaidLoot / RaidExtractionZone → RaidHUD` 的顺序。`.h` 看成员与可调参数，`.cpp` 看实现；`BeginPlay` 是进入世界后的初始化，`Tick` 是每帧更新。先改一项参数、编译、运行验证，再继续下一项。

## 中文文字与字体

HUD 的 `F_RaidChinese` 是 Runtime 复合字体，引用 Inline 加载的 `FF_RaidChinese`，嵌入完整 Noto Sans CJK SC Regular 2.004 OTF。按实际显示字号渲染和测量文字，避免把小字号栅格直接放大。世界标签使用独立的 `F_RaidChineseWorld` Offline 字体；UE 的 TextRender 不支持 Runtime 字体。

世界字体已烘焙 487 个可打印字符，当前文本需要 485 个，源字体和已烘焙字表的缺字数均为 0。`Depot` 的 61 个和保留的 `RaidYard` 的 42 个世界文字组件，共 103 个，均已迁移为中文字体，物资显示名也已迁移。默认游玩与打包地图仍为 Depot。

修改 HUD/玩法文字可从相应 C++ 模块入手；已保存地图的静态标牌和物资文字对应 `Scripts/r4_text_catalog.json`。修改代码中的默认值不会自动替换地图已保存的值。新增世界标签汉字后，先更新文字目录，再按 [ASSET_FONT_R4.md](ASSET_FONT_R4.md) 重导 Offline 字体并运行 `--verify-source`、`--verify-baked` 检查。地图迁移脚本 `Scripts/localize_r4_map.py` 更新文字及字体，不重建几何体；执行前应保留自己的地图备份。

## 构建、测试与打包

先关闭此项目的 UE / 游戏实例，避免 Live Coding 占用。使用本机 VS 打开 `GreyzoneRaid.sln`，选择 Development Editor / Win64；也可以在项目 PowerShell 执行：

```powershell
.\Scripts\Build.ps1
.\Scripts\Test.ps1
.\Scripts\FlowTest.ps1
.\Scripts\VisualReview.ps1
.\Scripts\Package.ps1
.\Scripts\Test.ps1 -GameExecutable "$PWD\Packaged_R4\Windows\GreyzoneRaid.exe"
.\Scripts\FlowTest.ps1 -GameExecutable "$PWD\Packaged_R4\Windows\GreyzoneRaid.exe"
.\Scripts\VisualReview.ps1 -GameExecutable "$PWD\Packaged_R4\Windows\GreyzoneRaid.exe"
```

脚本默认引擎路径 `C:\Program Files\Epic Games\UE_5.8`，可传 `-EngineRoot`。按顺序执行，上一条成功后再继续。打包只 Cook Depot，输出 Win64 Development 到 Packaged_R4；`Config/DefaultGame.ini` 显式包含 `/Game/UI/Fonts`。`Package.ps1` 成功后将字体许可与版权文本复制到 `Windows/ThirdPartyLicenses`。项目使用文件 Cook + Pak / IoStore，未修改引擎或系统全局设置。

测试在真实 UE 世界和图形后端中执行，通过游戏方法、已绑定动作和 HUD 点击区域分发来驱动；它们不是实体键鼠自动化。此会话原生 UI 工具禁用，人工 PIE、鼠标焦点体验、手感与音效听感均不能标记为通过。测试夹具和截图导演只修改当次运行世界，不改保存的 Depot。

## 资源、证据与限制

素材来源见 [ASSET_SOURCES_R4.md](ASSET_SOURCES_R4.md)、[R3 派生资源](ASSET_SOURCES_R3.md) 与 [R2 来源许可](ASSET_SOURCES_R2.md)。R4 唯一新增外部资源是 OFL 1.1 中文字体，完整来源与嵌入方法见 [ASSET_FONT_R4.md](ASSET_FONT_R4.md)。既有外部美术和声音为已记录的 CC0；Epic Manny / 模板动画单独按 Epic 许可记录。没有提取商业游戏资源、安装新组件、登录、购买或发布。

本轮测试结论见 [TEST_RESULTS.md](TEST_RESULTS.md)，原始日志和 PNG 在 Evidence。图片仅另存较小 JPEG 方便查看，未修改游戏画面。R3 包仍在 `Packaged_R3/Windows`；修改前的源码、资源、配置、脚本和文档保存在 `Backups/R3_20261007/GreyzoneRaid_R3_SourceContent.zip`（259,172,927 字节），原 README 与测试结果也单独留在同一备份目录。R2 包 `Packaged_R2/Windows` 与完整源码/资源备份 `Backups/R2_20261006/GreyzoneRaid_R2_SourceContent.zip` 保留；旧 R1 在 Packaged 与 Backups/R1_20261006。

当前是小型可玩原型，参考搜打撤类型，不等同于《三角洲行动》的完成度。换弹有独立弹匣和左臂动作，仍有手指贴合与轨迹限制；守卫装备和程序动作较简单。自动演练静音，正常游戏的音量平衡和实际枪感需要用户人工试听试玩。

资产生成脚本已经执行并保存结果，正常游玩无需重跑。`upgrade_map.py` 会重建地图，手工编辑后不要直接运行；R3 新增 `author_r3_arms.py`、`import_r3_assets.py`、`polish_r3_scene.py`，源文件和 R2 独立资产均保留。
