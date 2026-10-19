# R6：从实际调用追到引擎，再决定是否优化

[返回代码导览](R6_CODE_GUIDE.md) · [测试结果](TEST_RESULTS.md) · [原始源码审阅](Evidence/R6EngineResearch.md)

## 结论与边界

R5 HUD 的撤离点查询在本项目打包版中很小：三进程中位数的中位数约 **0.827 微秒/次**。满背包时，关闭和打开面板所需查询组合分别约 **2.293、3.141 微秒/轮**。因此 R6 保留 `FindExtraction`，没有增加撤离点注册表、缓存失效系统或额外 Subsystem。

背包 UI 改成事件维护的只读快照，是为了让显示和命令使用同一版本的数据、明确生命周期。医疗包计数随快照一起更新；这不是一项已经证明能提高 FPS 的优化。本实验不测完整 HUD 绘制、游戏帧率、GPU 时间或内存分配次数。

## 固定环境

| 项目 | 本次实际记录 |
| --- | --- |
| 电脑 | DESKTOP-TR04BB0 |
| CPU / 内存 | i5-9600K，6 核 6 线程；31.93 GiB |
| GPU / 系统 | RTX 3060；Windows 10 19045 |
| 引擎 | UE 5.8.3，CL 58210709 |
| 本轮构建使用工具链 | MSVC 14.50.35739（目录 14.50.35717），Windows SDK 10.0.22621.0 |
| 构建 / 地图 | Development；`/Game/Maps/Depot` |
| 渲染 | 1280×720，离屏、无声、关闭 VSync；保留渲染 |
| 基线规模 | 1 个撤离点；954 个世界 Actor 槽位；0、3、8 行背包 |

实际构建日志优先于“机器安装了哪些版本”的推断。[编辑器构建日志](Evidence/R6_Baseline_EditorBuild.log)、[独立基线打包日志](Evidence/R6_Baseline_Package.log)都成功；基线包单独存放在 `Packaged_R6_Baseline`，原 `Packaged_R5` 未用于覆盖或改写。

## 1. 问题从哪里来

原 HUD 每次正常绘制中，关闭背包会调用一次 `FindExtraction`、两次 `CountMedkits`；打开背包会调用两次和两次。主菜单、说明和结算界面不走这条路径。这是源码调用次数，不是采样得到的每秒调用次数。

`FindExtraction` 用 `TActorIterator<ARaidExtractionZone>` 找距离最近的有效撤离区。`CountMedkits` 扫描最多八行背包，用 `FName("Medkit")` 比较并累加数量。先测实际函数，而不是看到 Tick、Canvas、TArray 或迭代器就判定“慢”。

## 2. 追到本机引擎源码

以下路径相对 `C:/Program Files/Epic Games/UE_5.8/Engine/Source/`；详细行号和逐项核对见[源码审阅记录](Evidence/R6EngineResearch.md)。

1. `Runtime/Engine/Public/EngineUtils.h:582`：`TActorIterator` 构造基类，然后推进到首个候选。基类在约 428 行创建 `FActorIteratorState`。
2. 同文件 188–258 行：`WITH_EDITOR` 分支先取目标类对象，还会复制当前世界的 Actor 列表并求交集；`!WITH_EDITOR` 分支直接先取类实例候选，迭代时再筛世界。不能把两种二进制结果混在一起。
3. `Runtime/CoreUObject/Private/UObject/UObjectHash.cpp:1860`：`GetObjectsOfClass` 经 `ForEachObjectOfClass` 把结果加入数组。约 1883 行按类查已有桶，约 1912 行持有 UObject 哈希表锁并收集派生类。
4. `EngineUtils.h:261`、265：迭代器还会登记、移除 Actor 生成委托。只测 `for` 循环体会漏掉构造和析构成本。

这不是遍历整个 `GUObjectArray`，也不是直接拿到“已经按世界筛好”的常数时间列表。类桶、派生类、世界过滤、临时数组和委托都可能贡献开销。源码能够说明机制，但不能单凭这些位置给出固定的 malloc 次数。

哈希表锁也不等于允许在工作线程读 Actor。这个迭代器构造明确检查游戏线程；本实验和库存命令均在游戏线程运行。

## 3. 测量方法

[基准导演](Source/GreyzoneRaid/R6FrameworkBenchmarkDirector.cpp)通过 HUD 的 friend 声明和成员函数指针，直接调用真实私有方法，包含迭代器的完整构造、遍历、析构。没有复制一个近似算法替代它。

- Editor 和独立 Windows 包各启动三个新进程，分别汇总。
- 初始预热至少 10 秒及 120 帧；每个场景额外预热。
- 每场景校准循环到约 2 毫秒一批，采集 48 批；共 7 场景、每进程 336 个批样本。
- 计时区间之外完成生成、物品夹具、结果校验、日志和导出。AI、角色移动 Tick、撤离碰撞和倒计时被隔离，避免干扰查询夹具。
- 校验真实返回的最近撤离点和医疗物品数量；用可观察结果阻止把查询当作无用工作删除。
- 单独报告空测试框架开销，不机械地从数据里减掉它。

Windows `Cycles64` 使用 QPC。这是作用域经过时间，包含调度等待，不是纯线程 CPU 占用时间。重复紧循环会预热缓存和分配器；每批平均值的 p95 也不是实际 HUD 每帧延迟的 p95。

## 4. 改动前结果

单位：**微秒/一次调用或组合轮次**。表内数字为三个进程各自中位数，再取中位数；原始 CSV 保留了各进程与每批分布。

| 场景 | Editor，WITH_EDITOR=1 | Windows 包，WITH_EDITOR=0 |
| --- | ---: | ---: |
| 空测试框架 | 0.00118 | 0.00121 |
| 查最近撤离点 | 7.623 | 0.827 |
| 医疗包计数，0 行 | 0.00361 | 0.00317 |
| 医疗包计数，3 行 | 0.236 | 0.238 |
| 医疗包计数，8 行 | 0.619 | 0.642 |
| 关闭面板查询组合，1 次查点 + 2 次计数 | 9.217 | 2.293 |
| 打开面板查询组合，2 次查点 + 2 次计数 | 16.572 | 3.141 |

[Editor 汇总](Evidence/R6FrameworkBenchmark_Editor_20261008_072902_6641308Z/FrameworkBenchmarkAcrossRuns.csv) · [Editor 完整清单](Evidence/R6FrameworkBenchmark_Editor_20261008_072902_6641308Z/FrameworkBenchmarkManifest.json)

[Windows 包汇总](Evidence/R6FrameworkBenchmark_Packaged_20261008_073419_5844636Z/FrameworkBenchmarkAcrossRuns.csv) · [Windows 包完整清单](Evidence/R6FrameworkBenchmark_Packaged_20261008_073419_5844636Z/FrameworkBenchmarkManifest.json)

编辑器查点更贵，与源码中的世界 Actor 列表复制、交集路径相符；这是对机制的解释，不是排除了所有其他因素的因果证明。组合计时是实际重复调用组合的经过时间，也不等于完整 `DrawHUD` 或帧耗时。

## 5. R6 改动与最终复测

`FindExtraction` 保留原算法。`CountMedkits` 改为读取库存事件维护的快照；医疗物品按数据定义的 `UseEffect::Heal` 识别。正式四种物品的行为与 R5 保持一致，但规则已从固定 ID 改为数据定义，不能称作任意输入下完全相同的循环优化。

R6 最终复测已完成，Editor 与独立包各三个新进程，每进程 7 场景、336 个批样本。70 项框架专项在两种目标中均通过，覆盖背包事件更新快照、切 Pawn 后旧库存不刷新新 HUD、关箱和销毁解除绑定、稳定状态重复绘制不重建库存列表，以及真实物品目录的加载与使用。具体断言见[最终测试记录](TEST_RESULTS.md)。计数查询变快也不覆盖事件发生时的复制与统计成本，不能据此推导游戏 FPS 增益。

单位仍为微秒，统计量与改动前表相同。各场景单独采样，不把各行相加拼成一帧。

| 场景 | Editor 改动前 → R6 | Windows 包改动前 → R6 |
| --- | ---: | ---: |
| 空测试框架 | 0.00118 → 0.00160 | 0.00121 → 0.00121 |
| 查最近撤离点 | 7.623 → 7.694 | 0.827 → 0.871 |
| 医疗包计数，0 行 | 0.00361 → 0.00770 | 0.00317 → 0.00709 |
| 医疗包计数，3 行 | 0.236 → 0.00722 | 0.238 → 0.00812 |
| 医疗包计数，8 行 | 0.619 → 0.00752 | 0.642 → 0.00674 |
| 关闭面板查询组合 | 9.217 → 7.402 | 2.293 → 0.919 |
| 打开面板查询组合 | 16.572 → 15.094 | 3.141 → 1.699 |

[最终 Editor 汇总](Evidence/R6FrameworkBenchmark_Editor_20261008_084823_3442848Z/FrameworkBenchmarkAcrossRuns.csv) · [完整清单](Evidence/R6FrameworkBenchmark_Editor_20261008_084823_3442848Z/FrameworkBenchmarkManifest.json)

[最终 Windows 包汇总](Evidence/R6FrameworkBenchmark_Packaged_20261008_084954_4293763Z/FrameworkBenchmarkAcrossRuns.csv) · [完整清单](Evidence/R6FrameworkBenchmark_Packaged_20261008_084954_4293763Z/FrameworkBenchmarkManifest.json)

[复测版本绑定记录](Evidence/R6PostBenchmarkBuildBinding.json)确认这六个进程运行前后所有 Source 文件、实际 Editor 项目 DLL 和独立包内部 exe 均未改变。回归期间无其他构建与性能进程并行；性能复测在整套玩法回归结束后串行执行。08:00 的旧 Editor 复测保留为中间版本证据，不用于本表。

满背包时读取已维护的计数确实减少了本实验的查询工作，但空背包查询反而由约 3–4 ns 变为 7–8 ns。未修改的查点路径也有波动；最终打包版三个进程的查点中位数范围约 0.837–1.167 微秒。不能把每个差值都归因于优化，更不能将局部的微秒差值转换成 FPS。最终选择仍是保留简单的撤离点迭代器，以事件快照明确库存 UI 的一致性和生命周期。

## 6. 复现和进一步实验

从项目根目录运行：

```powershell
.\Scripts\FrameworkBenchmark.ps1 -Runs 3
.\Scripts\FrameworkBenchmark.ps1 -PackageExe .\Packaged_R6\Windows\GreyzoneRaid.exe -Runs 3
.\Scripts\FrameworkTest.ps1 -GameExecutable .\Packaged_R6\Windows\GreyzoneRaid.exe
```

`-Stress` 可额外生成 32/256 个隐藏撤离点进行合成规模实验。本轮正常地图结果不能外推到该规模；没有运行的配置不列为通过。源文件和引擎相关文件哈希、命令行、CPU、世界规模、编译宏、可执行文件哈希都写入每次实验清单。

若以后地图显著变大，应先用 Unreal Insights 或等价帧级采样确认热点，再比较缓存失效成本与查询成本。CPU/GPU 瓶颈、目标硬件和 profiler 自身开销也需要分别考虑，参见 [Epic 的性能分析说明](https://dev.epicgames.com/documentation/en-us/unreal-engine/introduction-to-performance-profiling-and-configuration-in-unreal-engine)。本轮没有采集完整帧级 Insights 轨迹，没有声称降低 Draw Call、分配次数、GPU 时间或提高 FPS。
