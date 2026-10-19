# R3 测试与交付记录

日期：2026-10-06，本文时间全部为 UTC。机器：DESKTOP-TR04BB0。R3 现在包含真实启动主菜单、操作说明、部署、局内搜集交火、撤离/死亡结算、回菜单与重新部署。以下结论来自本轮实际编译、运行和打包，不使用 R2 的结果替代。

## 实际结果

| 检查 | 状态 | 证据 |
|---|---|---|
| 本机环境 | 通过 | UE 5.8.3 CL58210709；VS Community 2026；MSVC 编译器实际报告 14.51.36260（安装目录 14.51.36231）；SDK 10.0.26100.0；RTX 3060；`Evidence/Environment.txt` |
| Editor C++ 编译 | 通过 | `Evidence/EditorBuild_R3.log`；本轮最终构建 16:04:18 完成 |
| 编辑器真实图形完整菜单循环 | **通过：189 / 189，0 失败** | `Evidence/R3Flow_Editor/FlowTest.log`、`FlowTestResult.json`；16:05:32；测试阶段 31.31 秒，进程退出码 0 |
| 编辑器真实图形玩法回归 | **通过：67 / 67，0 失败** | `Evidence/SelfTest_R3_Editor.log`；16:06:51；测试阶段 35.33 秒 |
| Win64 Development 打包 | **通过** | `Evidence/Package_R3.log`：Build / Cook / Stage / Archive 完成，BuildCookRun 56.59 秒，ExitCode=0；16:10:17 结束；Cook 0 errors / 27 warnings |
| 独立打包版完整菜单循环 | **通过：189 / 189，0 失败** | `Evidence/R3Flow_Packaged/FlowTest.log`、`FlowTestResult.json`；16:11:14；测试阶段 24.93 秒，菜单 Quit 后正常退出，退出码 0 |
| 独立打包版玩法回归 | **通过：67 / 67，0 失败** | `Evidence/SelfTest_R3_Packaged.log`；16:12:13；测试阶段 34.14 秒，脚本成功返回 |
| 独立包资源与实机截图 | 通过生成，关键画面已查看 | `Evidence/R3Flow_Packaged` 8 张流程 PNG；`Evidence/R3Visual_Packaged` 13 张玩法 PNG；最后一次 16:13:36 完成并正常退出 |
| 真实物理键鼠、Windows 窗口焦点手感、人工 PIE | **未运行** | 原生 UI 工具在本会话禁用；没有绕过此限制 |
| 音效听感、音量平衡、人工枪感 | **未运行** | 所有自动测试和截图演练均 `-nosound`；正常 Run.ps1 / 手动启动 exe 才使用声音 |
| 商业 FPS 级画质或与《三角洲行动》1:1 体验 | **不作此通过声明** | 当前为可运行的小型单机原型，模型贴合和动画仍有局限 |

独立运行入口：`Packaged_R3/Windows/GreyzoneRaid.exe`。打包目录在额外截图和文档加入前约 **1019.4 MiB**，包括 Development 调试符号；必须保留整个 Windows 目录。没有安装额外组件、登录账号、购买、发布或改动系统音量。

## 189 个流程检查点覆盖

这是一条真实 UE 进程中的连续流程：普通启动 MainMenu → How to Play → Back → Start → 第一局搜索并成功撤离 → Main Menu → 第二局死亡 → Retry → 第三局干净重开并放弃 → Main Menu → Quit。每次部署/返回均使用实际 OpenLevel，合计六个世界阶段。

- 菜单实际绘制按钮区域，使用 `FHUDHitBox::Contains` 找区域中心，并调用 UE `UpdateAndDispatchHitBoxClickEvents` 分发。验证区域存在、回调和状态变化；不模拟 OS 鼠标。
- 检查菜单/说明/结算显示鼠标、启用点击悬停并忽略移动视角；Running 恢复相反状态。检查隐藏的开始按钮不能在说明页触发。
- 菜单将一个保持攻击能力的敌人移到附近，等待并确认玩家未受伤、行动计时为 0、不能开火换弹搜索移动或加入物资。菜单安全检查前不关闭敌人 AI。
- 每次新世界核对新 Pawn / 敌人 / 物资实例、3 名敌人、9 份物资、HP100、弹匣12、储备60、库存与价值清零、无残留瞄准/换弹/搜索状态。
- 使用保存地图里的真实物资、视线检测、已绑定 E 动作和搜索计时器完成收集，验证数量、价值和原物资移除。
- 检查空包不能撤离、真实 Trigger 重叠、离开复位、有效伤害复位、重置后等待完整 8 秒、成功结算准确收益。
- 使用真实 ApplyDamage 触发死亡，确认收益为 0；结果阻止战斗。结果页按钮和 Escape 路径均覆盖；放弃不伪造伤害，仍不计收益。
- 每次截图写完后才切换世界。最终调用真实菜单 Quit；脚本独立验证 PASS 后出现 Quit 日志、进程正常退出和新鲜 PNG 签名。

流程测试为隔离验证，会传送玩家、在战斗阶段压低测试世界敌人的发现/攻击范围。实际移动和敌人战斗由独立的 67 项玩法回归覆盖；保存的 Depot 和正常游戏 AI 不被改动。189 是断言数量，含重复的新局检查和截图检查，不代表 189 个独立玩法系统。

## 67 个玩法回归检查点覆盖

真实 CharacterMovement 步行、冲刺、跳跃落地和视角；机瞄按下/释放、FOV 与速度；射线命中伤害和击杀、自动开火限制、空弹匣、1.5 秒换弹和弹药转移；按住搜索、松开取消与完成；8 件背包容量、满包保护、治疗与消耗；动态导航、巡逻、发现、追击、攻击；掩体阻挡和移除后恢复伤害；携物撤离、离区复位、成功收益；真实敌人造成死亡与胜败后重载恢复。与 R2 相同的基准在当前 R3 源码和独立包上重新执行，均通过。

## 实机证据与可见限制

流程截图文件名依次为：`01_main_menu`、`02_instructions`、`03_running`、`04_extraction_countdown`、`05_extracted_success`、`06_dead_failure`、`07_restart_clean`、`08_return_menu`。玩法目录另含机瞄、开火、换弹、守卫受击、玩家受击方向、按住搜索、收获提示、背包和撤离地标。

以下是最终独立包原始截图另存的 JPEG（只压缩，没有修饰画面）；Library 回执为 `Evidence/R3Flow_Packaged/LibraryReceipts.json`：

- [主菜单](https://chatgpt.com/api/library/files/libfile_b85b28d60d408191b1fac59996c2dafa/download)
- [部署后的局内](https://chatgpt.com/api/library/files/libfile_22675ab3fd4c8191a094cb09195a0b32/download)
- [撤离倒计时](https://chatgpt.com/api/library/files/libfile_01b94206af508191867d1e33bc9db061/download)
- [成功结算](https://chatgpt.com/api/library/files/libfile_cd4dc46083c08191b52b6cc8305ecb42/download)
- [死亡结算](https://chatgpt.com/api/library/files/libfile_174bf42ab0e88191be252bb3e2059a79/download)

R3 保留军事仓库与 PBR 材质，加入独立弹匣和左臂换弹关键帧、布料材质、守卫举枪/受击/侧移、受击方向、拾取通知、容量与带出价值、医疗指示和两处额外掩体。已消除先前换弹枪托近裁剪造成的大块遮屏；仍存在手指不完全贴合弹匣、程序动画不够自然、守卫装备几何较简单、Canvas 大字偏软等限制。截图不能代替实际枪感或高压战斗平衡的人工验收。

## 失败尝试、警告与保留版本

本轮首次菜单流程运行在搜索步骤失败，日志保留于 `Evidence/R3Flow_FirstAttempt/FlowTest.log`：测试先把角色传送到物资旁的空中并立即搜索，落地改变视线，按既有规则取消了搜索。测试夹具已改为落地等待后重新瞄准，再执行原有搜索动作。修复后的编辑器与打包版完整流程均通过；没有移除搜索时的视线校验。

Cook 的 27 个非致命警告主要来自 FString 创建 FText 的地图标记；编译还提示已安装 MSVC 高于首选版本，以及 UE Engine 自身 Character.h 的弃用 API。关卡销毁中保留 CrowdManager / 导航清理提示。实际构建、导航与运行检查通过；没有为消除这些提示安装或修改引擎。

R2 包 `Packaged_R2/Windows` 与源码/资源 ZIP `Backups/R2_20261006/GreyzoneRaid_R2_SourceContent.zip` 均保留。R2 结果在 [TEST_RESULTS_R2.md](TEST_RESULTS_R2.md)，R1 历史在 [TEST_RESULTS_HISTORY.md](TEST_RESULTS_HISTORY.md)。当前项目的操作、模块说明和重跑命令见 [README.md](README.md)，来源与派生记录见 [ASSET_SOURCES_R3.md](ASSET_SOURCES_R3.md)。