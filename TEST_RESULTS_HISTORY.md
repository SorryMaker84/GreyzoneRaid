# 实际验证记录

日期：2026-10-06；电脑：DESKTOP-TR04BB0；UE 5.8.3；VS Community 2026 / MSVC 14.51.36260；Windows SDK 10.0.26100.0。
实际图形运行使用 D3D12 / NVIDIA GeForce RTX 3060，1280×720。UE 日志确认 CPU 为 i5-9600K，内存约 32 GiB。

## 当前开发状态与上一轮补测

用户认为灰盒体验不满足要求，当前正在制作 R2 军事工业区垂直切片；下面的旧版结果仅代表历史逻辑检查，不代表新版画面、枪感或玩法体验已验收。R1 的源码与完整可执行目录保存在 `Backups/R1_20261006`。

上一轮新增了冲刺、跳跃、医疗包及 Tab 状态检查：编辑器 `-game` 的 `Evidence/SelfTest_Supplemental_Editor.log` 为 56 项通过、0 失败。独立程序已更新（`Evidence/SupplementalPackage.log` 成功），但最后运行收到关闭请求，只有 55 条通过且没有 `RAID_SELFTEST_RESULT`，因此完整打包版补测未通过验收、也不记为功能失败。记录保留在 `Evidence/SelfTest.log`。动作委托检查不证明实体键鼠或 PIE 人工操作。

四张原截图已真实保存到 Library，身份回执在 `Evidence/LibraryTransfer/windows_host_upload_1791288911881/screenshots_library_receipts.json`。Windows 无法写入本地 xattrs，远端文件本身均保存成功。

## R1 历史结果

| 检查 | 状态 | 证据与范围 |
|---|---|---|
| VS 工程生成 | 通过 | `Evidence/GenerateProjectFiles.log`：Succeeded |
| Development Editor C++ 构建 | 通过 | `Evidence/EditorBuild.log`：Succeeded；包含 UHT 与链接 |
| 保存地图与蓝图重新加载 | 通过 | `AssetVerify.log`：3 个守卫、9 个物资、28 个场景块、撤离区、导航范围、PlayerStart；命令返回 0 |
| 真实图形玩法集成运行 | 通过 | `SelfTest_Editor.log`：39 项，0 失败，25.27 秒（不含引擎完整启动时间） |
| 玩家移动与转向 | 通过 | 1 秒真实位移 321.8 cm，Walking 模式，速度 350 cm/s；转向改变视角 |
| 开火、伤害与击杀 | 通过 | 敌人 HP 100→66；每发扣 1 发；3 发击杀，击杀统计递增 |
| 弹夹空与定时换弹 | 通过 | 空弹夹不消耗储备；约 1.5 秒装填 12 发；储备 60→48 |
| 视线拾取与背包 | 通过 | Interact→TryCollect 增加库存并移除物资；满 8 件后拾取失败且物资保留 |
| 动态导航与 AI | 通过 | RuntimeGeneration=Dynamic；800 cm 完整路径；真实巡逻、发现、追击位移及攻击；HP 100→92 |
| 撤离倒计时与离开复位 | 通过 | 携带物资开始；离开复位到 8 秒；连续 8 秒后结算准确的物资数和价值 |
| 死亡与失败结算 | 通过 | 敌人实际攻击造成 HP=0、Dead；携带物资不计入成功收益 |
| 成功/死亡后重开 | 通过 | 两次 OpenLevel；新世界与新局状态：HP100、弹夹12、储备60、空背包、Running |
| 截图保存与画面检查 | 通过 | 四张 1280×720 PNG 已保存并查看；HUD、场景、成功/失败面板清晰可见 |
| Win64 Development 打包 | 通过 | `Evidence/Package.log`：Build、Cook、Stage、Archive 全部成功，UAT ExitCode=0；输出 `Packaged/Windows` |
| 打包程序真实图形运行 | 通过 | `Evidence/SelfTest_Packaged.log`：39 项，0 失败，22.96 秒；`Test.ps1 -GameExecutable` 返回 0；D3D12 实际渲染 |
| 实际键盘鼠标 / UE 编辑器 PIE 人工操作 | 未运行 | 此会话原生 UI 工具禁用，应用列表为空；不能据自动检查宣称键鼠实操通过 |

集成检查在真实 UE 世界中调用与按键绑定相同的玩法方法，使用真正的碰撞、CharacterMovement、AIController、导航、射线和计时器。编辑器 `-game` 和打包程序各运行了一次最终完整检查；两次均为 39 项通过。测试关卡中的守卫和物资实例被替换为受控测试实例；保存的地图不被测试修改。截图中测试背包价值与普通关卡物资价值可能不同。没有测试联机、干员技能或复杂经济。医疗包、冲刺、跳跃及 Tab 面板有实现，但未单独做自动行为检查；实际键鼠输入绑定也仍需人工试玩确认。

独立程序入口是 `Packaged/Windows/GreyzoneRaid.exe`，内部游戏二进制在 `GreyzoneRaid/Binaries/Win64`。交付为 Win64 Development 版，包含调试符号，完整 Windows 文件夹约 901.5 MiB；不要单独复制入口 exe。运行已使用本机现有组件，没有安装新组件。

## 可用截图

- [打包版战斗与背包统计](Evidence/PackagedScreenshots/01_combat_inventory.png)
- [打包版撤离成功结算](Evidence/PackagedScreenshots/02_extracted_success.png)
- [打包版死亡失败结算](Evidence/PackagedScreenshots/03_dead_failure.png)
- [打包版重开后的初始状态](Evidence/PackagedScreenshots/04_restart_clean.png)

四张打包版截图已实际打开复核；查看原始分辨率时文字完整。编辑器运行截图仍保留在 `Evidence` 根目录。

四张 PNG 的便于交付副本也已合并为 [GreyzoneRaid_Screenshots.zip](Evidence/GreyzoneRaid_Screenshots.zip)。

## 发现并修复的问题

首次运行 39 项中有 2 项失败，记录保留在 `SelfTest_FirstRun.log`：玩家输入重开函数名 `Restart()` 意外覆盖 UE 的角色初始化虚函数，导致移动模式一直为 None；已改名 `RequestRaidRestart`，保留引擎正常初始化。第二项是测试使用可复用 UObject 编号判断新世界；已改为跟踪对象生命周期的弱引用。

诊断运行记录在 `SelfTest_MovementDiagnostic.log`；修复后的最后编辑器运行全部通过。地图脚本首次创建资源时产生“路径不存在”的错误日志，已改为先检查资源存在；随后真实重新加载验证返回成功。箱体内医疗包、倒置招牌也已修正并重新查看游戏画面。

第一次打包的 Build 和 Cook 成功，但 Stage 读取 Zen 本地服务失败，UAT 三次重试后返回 1；失败日志保留在 `Evidence/Package_ZenAttempt.log`。已在本项目包装设置中关闭 `bUseZenStore`，并在打包脚本传入 `-SkipZenStore`，使用 UE 支持的文件 Cook 和 IoStore 打包。旧的项目 Cook 目录保留为 `Saved/Cooked/Windows_ZenAttempt`，未更改 UE 全局配置或读取存储认证信息。之后完整打包和独立程序检查均成功。

最终核对把包装配置字段更正为 UE 的 `UsePakFile`；最后一次打包 26.13 秒完成，ExitCode=0。此核对未改玩法代码或地图，UBT 确认两个目标均已是最新，执行 0 个编译动作。曾有一次执行请求因服务连接中断未创建进程，连接恢复后正常完成；当前无执行环境阻塞，构建和自动测试进程均已结束。

UE 对当前 MSVC 较首选版本更新给出警告；现有编译与运行通过，不需要为此额外安装编译器。引擎自身的弃用 API 警告保留在构建日志中。

## 重跑方式

关闭其他此项目游戏实例，在项目 PowerShell 中执行：

```powershell
.\Scripts\Build.ps1
.\Scripts\Test.ps1
.\Scripts\Package.ps1
.\Scripts\Test.ps1 -GameExecutable "$PWD\Packaged\Windows\GreyzoneRaid.exe"
```

`Test.ps1` 只有发现本次运行产生的 `RAID_SELFTEST_RESULT: PASS` 才成功返回；超时或任何失败会报错。普通游玩运行 `Run.ps1`，不要加 `-SelfTest`。
# 2026-10-06 本轮安全检查点补充

R2新C++已编译通过；Depot已生成并有六张真实场景阶段图。最终手臂骨架通过独立进程保存/重载验证（0错误0警告）。R2的67项检查尚未运行，新版尚未打包，枪与手臂握持/完整视觉尚未验收。自动演练现已静音，UE/game进程已全部结束。续接详情见 `R2_CHECKPOINT.md`，不得把下文历史R1检查当作R2完成证据。
