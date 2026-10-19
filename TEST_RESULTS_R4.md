# R4 测试与交付记录

2026-10-07，以下时间为 UTC。机器：DESKTOP-TR04BB0。R4 将菜单、操作说明、局内提示、物资、背包、地图标牌和结算改为中文，游戏名统一为“灰域突围”。以下结果来自当前源码与本轮新生成的独立包。

## 实际结果

| 检查 | 结果 | 证据 |
|---|---|---|
| 本机环境 | 已核实 | UE 5.8.3 CL58210709；VS Community 2026；MSVC 14.51.36260（目录14.51.36231）；SDK10.0.26100.0；RTX3060；Evidence/Environment.txt |
| 最终 Editor C++ 构建 | 通过，13.28秒 | Evidence/EditorBuild_R4.log |
| 编辑器菜单与完整行动循环 | **198/198，0失败** | Evidence/R4Flow_Editor/FlowTest.log；06:37:30；31.96秒；9张PNG；正常菜单退出 |
| 编辑器玩法回归 | **67/67，0失败** | Evidence/SelfTest_R4_Editor.log；06:38:51；35.16秒 |
| 编辑器真实画面演练 | 完成13张PNG | Evidence/R4Visual_Editor/VisualReview.log；06:39:39 |
| Win64 Development 打包 | **通过** | Evidence/Package_R4.log；BuildCookRun62.03秒；ExitCode=0；Cook 0 errors / 27 warnings |
| 独立包1600×900菜单、字体与完整循环 | **198/198，0失败** | Evidence/R4Flow_Packaged/FlowTest.log、FlowTestResult.json；06:42:46；25.66秒；9张新鲜PNG；菜单Quit后退出码0 |
| 独立包玩法回归 | **67/67，0失败** | Evidence/SelfTest_R4_Packaged.log；06:43:24；34.21秒 |
| 独立包真实画面演练 | 完成13张PNG并正常退出 | Evidence/R4Visual_Packaged/VisualReview.log；06:43:58 |
| 独立包1280×720菜单与界面 | **198/198，0失败** | Evidence/R4Flow_Packaged720；06:44:28；25.41秒；9张PNG；菜单、说明、局内截图已查看 |
| 源字体与世界图集 | 通过，缺字0 | Evidence/R4_FontImport.json、R4_FontValidation.json；当前485个所需字符，487个已烘焙字符 |
| 独立包字体与世界文字 | 通过，缺字0 | 完整Inline字节16,437,364；世界单页图集；61个文字组件、263个实际字符、81种字符全部覆盖；9个物资名为中文 |
| 地图保存文字审计 | 通过，无未识别英文词 | Evidence/R4_MapLocalization.json：Depot 61个标签，保留的RaidYard 42个标签；两图各9个物资实例；只有Depot被打包 |
| 实体键鼠、人工PIE、窗口焦点体验 | **未运行** | 会话原生UI工具禁用；未绕过限制 |
| 音效听感、音量平衡、实际枪感 | **未运行** | 所有自动运行均-nosound；未改系统音量 |

可直接运行 `Packaged_R4/Windows/GreyzoneRaid.exe`，完整目录约 **1032.8 MiB**（不含测试截图、运行缓存和附加文档，含Development符号）。必须保留整个Windows目录。没有新增组件安装、账号登录、购买或发布。当前没有阻止本机运行的已知阻塞。

## 检查覆盖与边界

198是断言次数，包含反复的新局状态和截图校验，不等于198个独立功能。流程为正常启动菜单 → 操作说明 → 返回 → 开始 → 搜集 → 撤离成功 → 返回菜单 → 再次开始 → 死亡 → 重开 → 主动放弃 → 返回菜单 → 真实退出。每次部署与返回使用OpenLevel。

- 菜单通过实际Canvas HitBox查找和UE点击分发验证；检查鼠标显示、点击悬停、移动视角输入模式、不可跨页触发隐藏按钮。
- 菜单期间保留一个近处守卫的攻击能力，确认玩家无伤、行动时钟为0且战斗动作被阻止。
- 新局检查新世界、新玩家与新敌人物资实例，3名守卫、9份物资、100生命、12/60弹药、空包和清零结果。
- 搜索使用实际物资、视线与绑定输入委托；检查搜索计时、物资销毁和收益。
- 撤离检查空包无效、Trigger重叠、离开和受伤复位、重新等待完整8秒、成功收益。
- ApplyDamage造成真实死亡；主动放弃保留玩家生命但损失收益，界面显示“行动已放弃”；结果页停止战斗并支持重开与菜单。
- 字体检查加载实际Cook资源，核对Runtime字体、Inline完整字节、Offline单页图集、每个实际世界字形及物资中文名。
- 每张截图写完后才切换世界；外层脚本检查PNG签名、新鲜时间、PASS后的真实Quit和正常退出。

67项玩法回归另外覆盖真实CharacterMovement步行、冲刺、跳跃落地、视角、机瞄FOV和速度，射线伤害、击杀、连射、空弹匣、换弹与弹药转移，搜索取消/完成，8件背包容量、满包保护、医疗包治疗和消耗，UE动态导航巡逻、发现、追击、攻击，掩体阻挡，携物撤离、死亡和重开。

自动流程通过游戏方法、绑定动作委托和HUD分发驱动，**没有注入实体键鼠**。菜单流程和视觉导演会在当次运行中传送玩家、抑制部分AI压力；正常地图和敌人配置不被保存改动。实际敌人战斗由独立玩法回归覆盖。截图不能证明人工枪感、音效或商业FPS级完成度。

## 最终实机截图

原始PNG保留；JPEG仅作质量78压缩，没有修饰、重绘或替换画面。以下五张来自最终R4独立包，单张64–182KB。回执：`Evidence/R4Flow_Packaged/LibraryReceipts.json`。

- [中文主菜单](https://chatgpt.com/api/library/files/libfile_b00847c74ae881919c36eaed37cd402a/download)
- [中文局内界面](https://chatgpt.com/api/library/files/libfile_097245a74340819182167ce58e30c1d7/download)
- [撤离倒计时](https://chatgpt.com/api/library/files/libfile_026fdab10168819189930c7c4ee42888/download)
- [撤离成功](https://chatgpt.com/api/library/files/libfile_380d310da6e88191953664a7fd181afe/download)
- [死亡失败](https://chatgpt.com/api/library/files/libfile_60bd8825d7348191acbff807d5f8221f/download)

流程截图共9张，额外包含说明、干净重开、返回菜单和主动放弃。玩法截图共13张，包含机瞄、射击、换弹、守卫受击、玩家受击方向、按住搜索、获得物资、背包与撤离站。1280×720流程图单独保存在R4Flow_Packaged720。根线程和独立审查已查看关键中文排版；未发现妨碍使用的缺字或裁切。

## 修复、警告与限制

初次字体导入遇到普通沙盒下Zen缓存权限及无Slate的Python commandlet断言，未产生可用字体；改用获准的隐藏完整编辑器导入后成功。记录见R4_FontImport日志和[字体说明](ASSET_FONT_R4.md)。用于离线图集的OTF仅在编辑器进程内以FR_PRIVATE加载并清理，没有安装系统字体。HUD使用完整随包字体，正常游玩不依赖Downloads或系统字体。

第一轮中文预览189项全部通过，保存在R4Flow_FirstPreview；后续统一“灰域突围”标题、区分死亡和主动放弃、调整弹药条及金额说明间距，再以最终版本完成198项回归。首轮预览与最终独立包证据分开保存。

Cook的27条非致命警告来自动态FString创建的FText。工具链提示MSVC版本高于引擎首选版本，UE头文件包含弃用API提示，关卡销毁时有CrowdManager导航清理提示。实际构建、字体、导航与游戏回归均通过，未为消除提示修改引擎或安装其他工具。

这是小型单机原型。换弹手指与弹匣贴合、守卫装备和程序动画仍可改进；远处小型世界标牌的可读性受距离与光照影响。未实现联机、干员技能或局外经济，也没有将其计入通过项。

## 版本与复现

R3包完整保留于Packaged_R3；修改前项目快照为Backups/R3_20261007/GreyzoneRaid_R3_SourceContent.zip，**259,172,927字节**。R3记录另存[TEST_RESULTS_R3.md](TEST_RESULTS_R3.md)。R2、R1历史也保留。

当前Source、Content、Config、uproject与关键打包二进制的SHA256，以及所有PNG尺寸与结果检查，见`Evidence/R4_DeliveryManifest.json`。脚本`Scripts/audit_r4_delivery.py`检查这些证据；编译输入没有在打包完成后被改动。运行方法、按键、C++模块分工见[README.md](README.md)，来源许可见[ASSET_SOURCES_R4.md](ASSET_SOURCES_R4.md)。
