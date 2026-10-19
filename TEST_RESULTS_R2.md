# R2 实际验证记录

日期：2026-10-06（以下时间为 UTC）；电脑：**DESKTOP-TR04BB0**。项目独立创建于 `C:\Users\ggbo\Documents\Codex\2026-10-06\task\GreyzoneRaid`。

实际环境：UE **5.8.3 CL58210709**；VS Community 2026；构建使用 MSVC **14.51.36260**、Windows SDK **10.0.26100.0**；i5-9600K、约 32 GiB 内存、RTX 3060。新版图形日志确认 D3D12 / **SM6**，硬件支持 Shader Model 6.7。初始磁盘与工具清单见 `Evidence/Environment.txt`；初始发现的 MSVC 安装目录版本与构建工具报告的编译器版本分别保留。

## 本轮结果

| 检查 | 状态 | 证据与实际范围 |
|---|---|---|
| R2 Development Editor 构建 | 通过 | `Evidence/EditorBuild.log`：编译、UHT、链接成功 |
| 手臂资源保存与独立进程重载 | 通过 | `Evidence/ArmsPersistenceVerify.json`；骨架、网格、动画均持久保存 |
| 握枪姿态制作与蓝图应用 | 通过 | `author_grip_pose_Grip04.log`、`AuthorGrip05.log`、`configure_slice_Grip04.log`；原始动画未覆盖 |
| 编辑器真实图形玩法集成 | **通过：67 / 67** | `Evidence/SelfTest_R2_Editor.log`；14:18:21，0 失败，检查阶段 34.14 秒 |
| Windows Development 打包 | **通过** | `Evidence/Package_R2.log`：Build / Cook / Stage / Archive 成功，UAT ExitCode=0，81.21 秒 |
| 独立程序真实图形玩法集成 | **通过：67 / 67** | `Evidence/SelfTest_R2_Packaged.log`；14:22:03，0 失败，检查阶段 36.66 秒；脚本返回成功 |
| 打包程序退出 | 通过，有明显延迟 | 首轮测试结果后约 50 秒完成清理；14:22:53 记录正常 Exiting / log closed，无需强制结束 |
| 1600×900 实机视觉截图 | 通过生成、已查看 | `Evidence/R2FinalVisual`；14:15:52–14:15:59，6 张原始 PNG；4 张约 117–162 KiB JPEG 已保存 Library |
| 独立打包版最终截图 | 通过生成、已查看 | `Evidence/R2PackagedVisual`；14:24:13–14:24:21，6 张 PNG；持枪和机瞄 JPEG 已保存 Library；14:24:22 正常退出 |
| 资源来源与文件清单核验 | 通过 | `ASSET_SOURCES_R2.md`；CC0 / Epic 资源分类；下载文件实际大小和 SHA256 核验 |
| 实体键鼠、编辑器 PIE 人工试玩 | **未运行** | 当前原生 UI 工具禁用；不能把方法 / 动作委托测试称为硬件输入测试 |
| 音效听感与手感人工验收 | **未运行** | 资源已接入；所有自动演练静音，正常 Run.ps1 才启用声音 |
| 商业 FPS 级画面或用户体验验收 | **未通过此类验收** | 当前是可运行原型；不以自动测试数量证明美术质量或枪感 |

独立程序：`Packaged_R2/Windows/GreyzoneRaid.exe`。Windows 目录在测试截图加入前约 **1022.1 MiB**（含 Development 调试符号）；不要只移动入口 exe。打包和测试均使用本机现有组件，未安装额外软件、登录账号、购买或发布。

## 67 项检查覆盖了什么

- 真实移动：步行约 321 cm / 秒测试窗口，冲刺位移约 483 cm、速度 560 cm/s；跳跃进入 Falling 并落地；转向改变视角。
- 机瞄：已有输入动作委托按下 / 释放，FOV 95→65→95，瞄准移速 245，释放恢复。
- 射击：真实射线击中敌人，HP 100→66，扣一发；击杀统计；空弹匣；换弹计时约 1.5 秒与正确弹药转移。
- 搜索：按住 E 产生进度，提前松开取消且延迟后仍不拾取，连续约 1.1 秒只收取一份指定物资。
- 背包与治疗：容量 8、满包不删除物资；Tab 面板状态与阻止开火；医疗包恢复 45、封顶 100、库存扣除与无药情况。
- AI：动态导航与完整 800 cm 路径；真实巡逻、发现、追击和攻击。加入碰撞掩体后跨越多个攻击间隔 HP 不变，移除掩体后再次受伤。
- 撤离：携带物资开始倒计时，离开复位，连续 8 秒后成功，结算准确件数与价值。
- 死亡与重开：真实敌人攻击造成 HP=0 和失败，物资不计收益；成功及死亡后均重新加载世界，恢复 HP100、弹匣12、储备60、空背包与 Running。
- 检查含 5 次截图保存断言；67 是检查点数量，不表示 67 个独立玩法系统。

测试通过实际 UE 世界内的玩法方法、现有动作委托、CharacterMovement、碰撞、导航、射线和计时器执行。测试夹具替换运行世界内的敌人和物资，不修改磁盘地图。视觉演练另行抑制 AI 攻击以拍照，不能将其误认为正常战斗录像。

## 画面、警告与限制

R2 已使用带材质的步枪、骨骼手臂与固定握持姿势、Manny 动画守卫、工业材料、仓库细节、细网围栏和撤离检查点。截图是 UE 实际渲染；只将 PNG 转为小体积 JPEG，未修改画面内容。上传回执：`Evidence/R2FinalVisual/LibraryReceipts.json`。

新版关闭默认运动模糊，避免开火后坐造成整幅画面拖影。`R2FinalVisual` 的六张图拍摄于关闭之前，其中第 3 张保留原始问题；打包版后续截图单独保存，不替换历史证据。

手臂握持已从原始展示动画改为本项目固定姿态，仍有原型级贴合与材质限制；换弹没有逐指拆装弹匣动画。敌人装备仍使用简单原创几何体，不等于完成商业角色美术。

非致命警告保留在日志：当前 MSVC 比 UE 首选版本更新；Cook 提示若干从 FString 初始化的 FText 应重新保存；结束关卡时出现 CrowdManager 找不到正在销毁的导航实例的警告。实际导航、打包和功能检查均通过。没有为消除警告而安装组件或改动全局设置。

本轮制作期间的失败尝试如导入轴向、原始骨架多根、动画 Python 接口差异均有日志，已经修复到可运行状态。历史灰盒测试、旧打包和中断的补测记录在 [TEST_RESULTS_HISTORY.md](TEST_RESULTS_HISTORY.md)，不得用 R1 的结果代替 R2。

## 重跑

关闭其他此项目实例，在项目 PowerShell 执行：

```powershell
.\Scripts\Build.ps1
.\Scripts\Test.ps1
.\Scripts\Package.ps1
.\Scripts\Test.ps1 -GameExecutable "$PWD\Packaged_R2\Windows\GreyzoneRaid.exe"
.\Scripts\VisualReview.ps1 -GameExecutable "$PWD\Packaged_R2\Windows\GreyzoneRaid.exe"
```

`Test.ps1` 要求本次新日志出现 `RAID_SELFTEST_RESULT: PASS` 并且进程结束后才返回成功。测试和视觉演练均静音，普通游玩请使用 `.\Scripts\Run.ps1` 或直接打开独立 exe。
