# R2 素材来源与导入记录

核对日期：2026-10-06。外部下载的 PBR 贴图、枪械、手臂与音效按下列来源记录为 CC0 1.0；另有本机已安装 Unreal Engine 模板中的 Epic Manny 与动画，须单独记录，不能归为 CC0。没有购买、登录、接受额外弹窗协议或提取商业游戏资源。CC0 不强制署名，这里保留作者与来源方便追溯。项目没有对外发布。

## 写实材质

每组使用 2K PNG：基色、粗糙度和 DirectX 法线。其中 `Concrete034` 的三张实际贴图为 2048×1024，其余四组为 2048×2048；尺寸证据见 `Downloads/resource_manifest_R2.json`。源文件保存在 `Downloads/Materials/`；绝对文件路径在 `SourceArt/Depot/texture_manifest.json`。`Scripts/upgrade_map.py` 负责导入地图纹理与创建世界坐标投影材质。

| 用途 / 文件夹 | 作者 | 官方来源 | 许可 |
|---|---|---|---|
| 水泥 `Concrete034` | ambientCG | [Concrete034](https://ambientcg.com/a/Concrete034)；[原始 ZIP](https://ambientcg.com/get?file=Concrete034_2K-PNG.zip) | [ambientCG CC0 1.0](https://docs.ambientcg.com/license/) |
| 沥青 `asphalt_02` | Rob Tuytel | [Asphalt 02](https://polyhaven.com/a/asphalt_02) | [Poly Haven CC0](https://polyhaven.com/license) |
| 波纹铁皮 `corrugated_iron_02` | Jenelle van Heerden、Sergej Majboroda | [Corrugated Iron 02](https://polyhaven.com/a/corrugated_iron_02) | [Poly Haven CC0](https://polyhaven.com/license) |
| 旧木板 `weathered_planks` | Dario Barresi、Dimitrios Savva | [Weathered Planks](https://polyhaven.com/a/weathered_planks) | [Poly Haven CC0](https://polyhaven.com/license) |
| 锈金属 `rusty_metal_02` | Rob Tuytel | [Rusty Metal 02](https://polyhaven.com/a/rusty_metal_02) | [Poly Haven CC0](https://polyhaven.com/license) |

Poly Haven 文件来自各资产官方公开 `https://api.polyhaven.com/files/<asset-id>` 返回的 2K PNG 下载地址；每组 `source_files.json` 保留文件元数据。仅使用上述 CC0 资产文件，没有使用站点标志、用户图片或网页预览渲染。ambientCG ZIP 共 28,322,068 字节，所需三通道已解压；原始 ZIP 保留。

## 武器与手臂

| 资源 | 作者 | 来源与许可 | 下载与使用 |
|---|---|---|---|
| Free Classic Weapons Pack v1.1 | Stein Games | [作者发布页](https://stein-indie.itch.io/classic-weapons-pack)，CC0；包内 `WeaponsPack/license.txt` 亦明确 CC0 1.0 | `Downloads/Models/SteinGames/Classic Weapons Pack v1.1.zip`，130,441,022 字节。通过页面的免费选项下载。使用 AK47 文件夹 `SKM_AK47.fbx`、`T_AK47_C.png`、`T_AK47_N.png`、`T_AK47_RMAO.png`；其余模型保留在源包，不导入。 |
| fps arms (rigged only) | para；原始人体网格与贴图归功于 MakeHuman 团队 | [作者发布页](https://opengameart.org/content/fps-arms-rigged-only)，CC0；[原始 7z](https://opengameart.org/sites/default/files/fps%20arms.7z) | `Downloads/Models/FPSArms/fps arms.7z`，1,406,250 字节。使用 `FPS ARMS RIG 1 test.fbx` 的修复派生文件 `FPSArms_SingleRoot.fbx` 与 `new_diff.png`，所有原始文件保留。作者提供了粗略示例动画，并非专业步枪握持/换弹动画。 |

导入脚本 `Scripts/import_assets.py` 将 AK47 合并为 `/Game/Weapons/SM_ClassicRifle`，材质为 `/Game/Weapons/M_ClassicRifle`。RMAO 贴图按文件命名连接 R=粗糙度、G=金属度、B=环境遮蔽；颜色贴图 sRGB，法线与遮罩为线性采样。模型高模、原作者没有 LOD；只用于当前单机原型，性能须在本机运行中检查。

作者 FBX 首次实际 UE 导入出现多根错误。`Downloads/Models/FPSArms/repair_root.cpp` 使用已经安装的 UE FBX SDK 2020.2 与 VS 编译：原公共父节点没有骨骼 NodeAttribute，工具为其指定正式 Skeleton eRoot 并命名 `ArmsRoot`，保留原骨骼、皮肤、动画与绑定姿势，输出独立派生文件 `FPSArms_SingleRoot.fbx`，不覆盖源文件。`BuildRoot.cmd` 可复现修复工具编译，没有安装额外组件。

修复后手臂实际成功导入 `/Game/Weapons/SK_FPSArms`，Skeleton 为 `/Game/Weapons/SK_FPSArms_Skeleton`，共 63 根骨骼。示例动画 `/Game/Weapons/A_FPSArms_Idle` 长 2.25 秒。实际手臂包围盒尺寸为 `[1158.98, 456.35, 545.61]` cm，中心约 `[0, 141.70, -176.90]` cm；源作者单位导致手臂约大十倍，可用组件 0.1 缩放后进行握持拟合。真实 AK47 包围盒尺寸为 `[7.7694, 103.0567, 28.7002]` cm，中心 `[-1.0854, 20.4661, 0.7822]` cm，长轴为 Y。完整数值与骨骼名见 `Evidence/ImportedAssetsR2.json`；枪管朝向、手臂握持与动画仍需游戏视觉验证。

持久化修复：首次 FBX 导入虽然返回成功，自动创建的 Skeleton 没有落盘，另一个 UE 进程加载时动画 Skeleton 为 null。当前 `Scripts/import_assets.py` 在本次进程关闭 Interchange FBX，以明确的 legacy `FbxImportUI` 创建资源；显式按 Skeleton、mesh、animation 顺序保存并检查 `.uasset` 文件。两份最初失效 R2 资产保存在 `Downloads/Models/FPSArms/FailedImportBackup`；第二次 Interchange 修复产生的失效 mesh/animation 和临时 Skeleton 已移入 `QuarantinedSecondImport`，没有删除或覆盖原始素材。

`Scripts/verify_imported_arms.py` 已在另外一个独立新 UE 进程实际加载确认 mesh 与动画的 Skeleton 均非 null，且同为 `/Game/Weapons/SK_FPSArms_Skeleton`。磁盘文件 Skeleton=14,620 bytes、mesh=1,157,421 bytes、animation=193,600 bytes。证据为 `Evidence/ArmsPersistenceVerify.json`（PASS）和 `.log`（exit 0，0 errors / 0 warnings）。

手腕姿势进一步只读检查保存在 `Downloads/Models/FPSArms/ArmsHandFrames_FBX.json`：FBX SDK 在动画 t=0 和 t=1.125 秒读取左右手腕全局位置与旋转，坐标未转换为 UE。组件缩放 0.1 后，这两帧右腕移动约 14 cm、左腕约 9.7 cm；作者粗略示例不适合作为稳定步枪握持循环，主任务应选定固定帧后拟合或另做专用握持动画。

## 音效

| 资源 | 作者 | 官方/作者发布来源 | 许可与原始下载 |
|---|---|---|---|
| The Free Firearm Sound Library | Ben Jaszczak、Brian Nelson、Kevin Heras、Matthew Nanney | [作者声明与来源页](https://opengameart.org/content/the-free-firearm-sound-library) | CC0；[Prepared SFX Library.7z](https://opengameart.org/sites/default/files/Prepared%20SFX%20Library.7z)，193,954,738 字节 |
| Impact Sounds | Kenney | [官方资源页](https://kenney.nl/assets/impact-sounds) | CC0；[原始 ZIP](https://kenney.nl/media/pages/assets/impact-sounds/87b4ddecda-1677589768/kenney_impact-sounds.zip)，800,850 字节；解压目录包含 `License.txt` |

原始文件保存在 `Downloads/Audio/FreeFirearm/` 和 `Downloads/Audio/KenneyImpact/`。`Downloads/Audio/prepare_audio.py` 使用已安装 UE 的 libsndfile 解码，再裁切、淡入淡出、峰值归一化和重采样成 48 kHz、单声道、16-bit PCM WAV；没有安装额外库。可重复生成，逐文件来源记录在 `Downloads/Audio/Prepared/audio_manifest.json`。

| UE 导入名 | 内容与处理 |
|---|---|
| `S_RifleShot` | AK-47 `C_28P.wav` 的近距离实录单发，裁取主攻击前 25ms 与约 1.15 秒尾音。 |
| `S_RifleShotFar` | AK-47 `C_31P.wav` 的中距离实录，约 1.15 秒。 |
| `S_Footstep`、`S_Footstep_01`–`04` | Kenney `footstep_concrete_000`–`004` 五种混凝土地面脚步。 |
| `S_Hit` | Kenney `impactPunch_medium_001`，击中反馈。 |
| `S_Empty` | Kenney `impactMetal_light_000` 的短机械点击。 |
| `S_Reload` | Kenney 四种金属/铁片/罐体轻敲组合成 2.1 秒换弹拟音。它不是 AK47 真实换弹录音。 |
| `S_Alert` | Kenney `impactBell_heavy_000`，短提示音。 |

以上 11 个 WAV 由 `Scripts/import_assets.py` 导入为 `/Game/Audio/<导入名>`。当前下载的真实枪声库只包含射击录音，因此机械换弹采用许可明确的实物拟音组合。尚未增加环境音录音。

## 本机 Epic 模板：Manny 与敌人动画

来源为已安装 UE 5.8 的 `C:\Program Files\Epic Games\UE_5.8\Templates\TemplateResources\High\Characters\Content\Mannequins`，复制后的资源位于 `Content/Characters/Mannequins`。这是 Epic 引擎安装附带的模板内容；沿用项目 `ASSET_SOURCES.md` 对 Unreal Engine 自带内容的独立记录，不赋予 CC0 许可。本次仅核实本机文件来源，没有新增下载、登录或代为接受协议。

`Source/GreyzoneRaid/RaidEnemy.cpp` 的资源引用确认敌人使用下列 Manny 网格与步枪姿态动画。2026-10-06 只读核对时，三个项目文件均与上述模板目录下相同相对路径的源文件 SHA-256 一致：

| 相对于 `Mannequins` 的资源路径 | 字节数 | 源文件与项目文件共同的 SHA-256 |
|---|---:|---|
| `Meshes/SKM_Manny_Simple.uasset` | 15,825,101 | `be9f011191ee1887b97cd13bce09886a6066c2fd62c619991070ec142184e1bf` |
| `Anims/Rifle/MF_Rifle_Idle_ADS.uasset` | 1,700,234 | `cfa32b19207a25546bb5bc2dde8454dc633cc27c3de45747e941756e23da5d4d` |
| `Anims/Rifle/Walk/MF_Rifle_Walk_Fwd.uasset` | 490,267 | `540a6a77c48027e13be84d483c6afd18d82fb115b7532334597ebefe2249ecd0` |

项目目录还包含模板骨架、材质、贴图、Rig 与其他动画依赖。以上只核对三个直接引用文件，不代表对整个模板目录逐文件完成清单或许可审阅。`Downloads/resource_manifest_R2.json` 记录外部下载资源，当前不包含这些 Epic 模板文件。敌人的 `M_GuardCloth` 由本项目脚本创建，不能据此把 Manny 网格或动画列为原创。文件一致性仅证明本机来源，不证明 R2 动画、材质或游戏画面已经验收。

## 检查状态

- 已通过：所有选定源文件下载并解压；15 张地图 PNG 存在；11 个派生 WAV 格式与源记录已生成；导入脚本用本机 Python 通过语法检查；修复后实际 UE commandlet 导入武器/手臂/11 音效并保存，返回 0，生成模型包围盒、骨架和动画记录。
- 最终导入日志：`Evidence/AssetsR2Import_PersistSkeleton_Legacy.log` 为成功运行，exit 0，0 errors / 2 warnings。两项警告是作者未导出 smoothing groups、12 个未顶点加权的末端骨骼未列入 bindpose。最终独立重载日志 `ArmsPersistenceVerify.log` 为 exit 0，0 errors / 0 warnings。
- 保留的先前记录：`AssetsR2Import.log` 为最初多根失败；`AssetsR2Import_SingleRoot.log` 是沙箱阻止 AppData DDC 写入、尚未执行脚本的失败启动；`AssetsR2Import_SingleRoot_2.log` 为仅导入成功但未保存 Skeleton 的记录；`AssetsR2Import_PersistSkeleton.log` 为 Interchange 重导入导致旧动画骨架未修复的失败记录。最终判断以上述 legacy 导入与独立重载证据为准。
- 未由资源任务运行：握持与动画视觉验收、游戏播放与音量验收、Cook/Package。主任务按单 UE 进程序列执行，结果应以其相应日志及 `Evidence/ImportedAssetsR2.json` 为准。
- 现有 UE 模板人物由主任务复制并管理；不归上述外部 CC0 下载范围，应沿用项目原有 UE 资源许可记录。

补充完整性清单见 `Downloads/resource_manifest_R2.json`：文件字节数、SHA-256、PNG 尺寸和 WAV 格式。原始下载包、原始音效和模型未覆盖或删除。
