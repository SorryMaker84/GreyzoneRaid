# GreyzoneRaid — R10 UE5 extraction prototype

可读 C++ 实现的单机搜集、战斗、撤离原型。当前版本包含敌人巡逻/追击、枪械与自动换弹、容器搜索、背包负重、可中断治疗、双撤离点、结算和重开。

## 克隆与运行

此仓库使用 Git LFS 保存 UE 资产及再导入源文件。请先安装 Git LFS，再执行：

```powershell
git clone https://github.com/SorryMaker84/GreyzoneRaid.git
cd GreyzoneRaid
git lfs pull
```

使用 UE 5.8.3 打开 `GreyzoneRaid.uproject`，生成项目文件并编译 `GreyzoneRaidEditor`，打开 Frontier 地图后 Play。本机原验证环境为 VS 2026 / MSVC 14.50 / Windows SDK 10.0.22621；需要 Unreal 所要求的 C++ 工具链。若引擎位于其他位置，脚本的 `EngineRoot` 参数需相应调整。

本仓库不附带 `Packaged_R10`、引擎、缓存或完整测试录像/日志。构建打包入口是 `Scripts/Package_R10.ps1`。`TEST_RESULTS_R10.md` 记录此前本机 R10 包的验证结果；本次上传准备没有重新构建或重新执行这些测试。历史文档中的 `Evidence` 和备份链接可能只在原机器存在；编辑器打开与构建不依赖这些日志。

使用现成 Content 无需再次导入素材；历史再导入脚本及清单可能包含原机器绝对路径，重导前应调整路径。重量、治疗时间等为本项目自定简化参数。详细操作见 [README_R10.md](README_R10.md)，代码阅读见 [R10_CHANGE_GUIDE.md](R10_CHANGE_GUIDE.md)。

第三方内容范围、许可证及公开分发依据见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。本次没有为作者代码擅自添加开源许可证。
