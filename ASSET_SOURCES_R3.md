# R3 资源与派生文件

R3 延用 [R2 来源与许可记录](ASSET_SOURCES_R2.md)。本轮没有新增外部下载、安装、账号登录、购买或发布；没有使用《三角洲行动》的模型、贴图、声音或界面图。军事仓库与搜打撤流程仅作类型参考。

| 本轮内容 | 来源与处理 | 复现 / 证据 |
|---|---|---|
| 独立枪身、可动弹匣 | Stein Games 的 CC0 AK47 派生；按原骨骼权重拆分，保留原顶点、法线、UV、材质与坐标变换，未修改源文件 | `Downloads/Models/R3RifleParts/split_rifle.cpp`、`RiflePartsReport.json`、`SplitVerification.txt`；导入脚本 `Scripts/import_r3_assets.py` |
| 握持、开火、换弹骨骼动画 | para / MakeHuman 的 CC0 手臂骨架与网格；本项目计算肩肘腕姿势和手指弯曲关键帧，不使用原作者展示动画作为待机 | `Scripts/author_r3_arms.py`；`Evidence/R3ArmsAnimations.json`；独立 `A_FPSArms_*_R3` 资产 |
| 手套布料 | 本项目原创程序材质，橄榄色、高粗糙度和细微织纹 | `/Game/Materials/M_R3GloveFabric`；`Evidence/ImportedAssetsR3.json` |
| 医疗指示、木箱掩体、撤离箭头 | UE 内置几何体与已记录的 R2 材质；本项目原创摆放与标记 | `Scripts/polish_r3_scene.py`；`Evidence/R3ScenePolish.json` |
| 主菜单、说明、结算、受击方向和搜索反馈 | C++ Canvas 原创布局、文字、线条和色块；使用 UE 内置字体 | `RaidHUD.h/.cpp`、`RaidGameMode.h/.cpp` |
| 守卫动画与装备 | 延用 Epic Manny / UE 模板动画，许可类别保持 Epic，不能归入 CC0；追加代码驱动举枪、后仰和侧移 | `RaidEnemy.h/.cpp`；详见 R2 来源记录 |

枪身 14,352 个三角形，弹匣 2,226 个三角形，合计等于原模型的 16,578 个三角形。拆分工具重新读取导出文件核验原顶点属性与坐标，保留检查结果。当前动画为原型级程序制作，仍有手指贴合、换弹轨迹和守卫步态限制。

截图来自 UE 实际运行。保留原始 PNG，只为传输另存 JPEG，不合成或修饰游戏画面。截图演练及自动测试一律 `-nosound`；音效文件来源继续使用 R2 记录。

R2 的可运行包保留在 `Packaged_R2/Windows`，源码与 Content 备份为 `Backups/R2_20261006/GreyzoneRaid_R2_SourceContent.zip`。R3 独立输出到 `Packaged_R3/Windows`，完成状态以 [TEST_RESULTS.md](TEST_RESULTS.md) 为准。
