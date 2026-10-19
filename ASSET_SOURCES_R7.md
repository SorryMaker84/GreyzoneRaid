# R7 资源来源与视觉制作范围

R7 继续使用工程内已有的合法资源，没有新增外部下载、安装、购买、账号登录、商业游戏资源提取或对外发布。既有来源与许可记录保留在 [R2](ASSET_SOURCES_R2.md)、[R3](ASSET_SOURCES_R3.md)、[R4](ASSET_SOURCES_R4.md)、[R5](ASSET_SOURCES_R5.md)、[R6](ASSET_SOURCES_R6.md) 及 [字体记录](ASSET_FONT_R4.md)。本文只记录本轮新增制作和复用边界，不把既有第三方资产写成本项目原创。

## R7 材质

[Scripts/r7_visual_assets.py](Scripts/r7_visual_assets.py) 创建独立的 `/Game/Materials/R7` 目录，提供 `MATERIAL_PATHS` 和惰性的 `build_materials()` 接口。普通 Python 导入不会启动 Unreal、改写文件、创建资产或切换地图；材质构建须由统一的 UE 编辑器任务调用。

本轮原创内容是材质图、颜色、粗糙度重映射、法线强度、世界坐标投影比例和程序黄黑斜纹。贴图仍引用已有 `/Game/Textures/Depot` 资产，不重新导入或修改 R2 纹理，不覆盖 `/Game/Materials/Depot` 材质。

| 复用纹理 | 已有作者与来源 | 现有许可记录 | R7 用途 |
|---|---|---|---|
| `Concrete034` | ambientCG，[资源页](https://ambientcg.com/a/Concrete034) | CC0，R2 来源记录与本地原始文件 | 混凝土、浅色混凝土、室内抹灰 |
| `asphalt_02` | Rob Tuytel，[Poly Haven 资源页](https://polyhaven.com/a/asphalt_02) | CC0，R2 来源记录与本地元数据 | 沥青、湿地表；棕色泥土地表为原有颗粒纹理调色，不是新采集的土壤扫描 |
| `corrugated_iron_02` | Jenelle van Heerden、Sergej Majboroda，[Poly Haven 资源页](https://polyhaven.com/a/corrugated_iron_02) | CC0，R2 来源记录与本地元数据 | 蓝色/橄榄色铁皮、波纹屋面 |
| `rusty_metal_02` | Rob Tuytel，[Poly Haven 资源页](https://polyhaven.com/a/rusty_metal_02) | CC0，R2 来源记录与本地元数据 | 锈钢；低强度去色磨损用于设备钢壳，减少波纹重复 |
| `weathered_planks` | Dario Barresi、Dimitrios Savva，[Poly Haven 资源页](https://polyhaven.com/a/weathered_planks) | CC0，R2 来源记录与本地元数据 | 旧木材 |

主要比例为混凝土每 240 厘米、沥青每 300 厘米、波纹金属每 150 厘米重复。安全黄、路漆、橡胶、撤离绿灯、暖灯、红色信号灯、不透明深色玻璃和黄黑斜纹由项目材质节点构成，不使用新的图片。玻璃是有意采用的不透明有色面板，不宣称为透明玻璃模拟；湿地表为材质效果，不是水体模拟。

构建成功后由脚本写入 `Evidence/R7_VisualAssets.json`，逐项记录材质路径、参数、引用纹理以及材质编译错误列表。文档和脚本存在本身不代表已通过 UE 编译或游戏画面验收，最终以该报告、导入日志和实机截图为准。

## 几何和地图

R7 的建筑结构、平台、工业设备组合、道路与场景布局由本项目制作；基础网格沿用已安装 Unreal Engine 的 BasicShapes。Epic 提供的基础几何、Manny 及既有动画仍属于 Epic 内容范畴，不能归为 CC0。没有复制其他商业游戏地图、模型、界面、品牌或贴图。R7 新地图采用独立 `/Game/Maps/Frontier` 路径，R6 的可运行包与归档由基线保护流程保留。

## 中文字体

[Scripts/import_r7_font.py](Scripts/import_r7_font.py) 从已有 R5 字体导入流程提炼，复用本地 Noto Sans CJK SC Regular 2.004 和完整 SIL Open Font License 1.1，不下载或安装字体。固定官方来源、提交、原始 OTF SHA-256 和许可 SHA-256 与 [字体记录](ASSET_FONT_R4.md) 一致，导入前重新校验；本轮不更改字体设计。

运行时继续加载 `/Game/UI/Fonts/F_RaidChinese`、`FF_RaidChinese`，世界文字继续加载 `F_RaidChineseWorld`，避免新旧文字使用不一致路径。扫描当前全部 Source 文本代码和名称含 `r7` 的 Python/JSON 脚本，并纳入可选的 R7 世界标签清单；旧 R5 字符和已有世界字体字符集合均予保留。重烘焙时将离线图集上限设为 4096×4096，随后验证所有实际字形仍位于同一页，防止 TextRender 漏字。

字体仅在导入进程内使用 Windows 私有字体注册，退出时移除；不写 Windows Fonts 目录、不做系统字体安装。字体导入需要带 Slate 初始化的编辑器环境，不能用纯 Python commandlet 代替。由主构建流程在所有新代码和标签完成后执行；输出为 `Evidence/R7_FontSource.json`、`R7_FontImport.json` 和 `R7_FontValidation.json`。`--verify-source` 核对原始字体和请求字符；`--verify-baked` 在导入后核对当前字符集合与保存证据，并验证 FontFace 包含完整原始 OTF 字节。

## 武器、角色、音效与验收图片

既有 CC0 武器、手臂、音效和 Epic 角色/动画继续按 R2–R6 记录分别归属。本轮材质与字体脚本没有导入新武器、角色、动画或录音。后续手部修正使用 `author_r7_hands.py` 在既有 CC0 手臂骨架上重新制作三份 R7 姿势动画，未下载第三方动画、未修改纹理。原始 R3 动画与来源记录保留；详见 [R7_HANDS_NOTES.md](R7_HANDS_NOTES.md)。场景和玩法新增代码属于本项目制作，不能据此更改所复用素材的原始作者与许可。

交付截图须来自实际 UE 运行画面，原图保留。截图是否生成、材质是否成功编译、中文是否完整显示、室内外亮度和地图通行是否合格，由本轮实测证据确认；本文件不替代这些验收结果。
