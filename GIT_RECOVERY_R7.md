# R7 本地版本与 R6 恢复

项目唯一仓库根目录为本文件所在的 GreyzoneRaid。R7 分支为
`r7/from-r6-20261011`。没有配置远程，没有 push，也没有修改全局 Git 配置。
提交身份只使用用户确认的本仓库姓名和邮箱；身份未确认时，基线先保留为冻结的
Git index/tree，不能把 R7 编辑再次加入该 index 后冒充原始基线。
提交及 tree 的实际编号见本地 `Evidence/R7_GitBaseline.json`。

## 快照

完整原始快照位于 `Backups/R6_BeforeR7_20261011T061303Z/`：

- `R6_CurrentInputs_RAW.zip`：源码、Content、Config、Scripts、SourceArt、
  Downloads、项目文件、根文档等当前原始输入，688 文件。
- `Packaged_R6_COMPLETE.zip`：完整 Packaged_R6，131 文件；原目录保持不动。
- 各 ZIP 的 `.manifest.json` 包含每个文件的 SHA256；`SnapshotReceipt.json`
  记录归档哈希及回读验证结果。
- `R6_InputComparison.json`：与 R6 交付清单声明的全部 265 个输入对比，原始快照
  无改动、缺失或该输入范围内新增。此结论只覆盖清单声明范围。

恢复时先用 `Get-FileHash -Algorithm SHA256` 核对归档清单，再将 ZIP 解压到新的
空目录检查，不直接覆盖当前项目。源码 ZIP 的路径以 Source、Content 等为根；
打包 ZIP 的路径以 Packaged_R6 为根。快照保存的是真实资产，离线恢复不需要 LFS。
原始配置含本地 Android 文件服务令牌，快照只在本机保留，不进入 Git。
Git 基线仅删除 Config/DefaultEngine.ini 的 SecurityToken 一行，其他设置保留。

## Git 与资产

Source、Config、Content、脚本、文档及许可证进入版本记录。生成文件、引擎缓存、
Packaged*、Backups、旧集成目录、大批证据和下载缓存被忽略。
Downloads 有 42 个明确例外，保证当前打包、字体/音频/模型/纹理再导入所需文件存在。
Evidence/ArmsPoseSample.json 是手臂编辑脚本输入，另行保留。

本机已有 Git 2.53.0 和 Git LFS 3.7.1。uasset、umap、fbx、otf、png、wav 通过本地
LFS 跟踪。必须同时保存完整 `.git`（尤其 `.git/lfs/objects`）和对应工作树；
仅复制 LFS pointer、仅 git bundle 或仅 git archive 都不是完整资产备份。
需要独立离线恢复时，优先使用上述真实文件 ZIP；迁移仓库时同时复制本地 LFS 对象。
没有做 LFS 清理、历史重写或远程上传。所有文件保持原始行尾，不在基线中归一化。

沙盒身份可能不同于项目所有者。工具进程需使用每次命令专用的
`git -c safe.directory=<项目绝对路径> -C <项目绝对路径> ...`，无需修改全局配置。

现有 SourceArt 纹理清单和 Prepared 音频清单含本机绝对路径；换目录再导入时需
先调整该既有路径约束。此基线不修改素材清单。生成音频的上游原始下载材料全部
保存在原始 ZIP，Git 只跟踪当前再导入直接使用的 Prepared 文件。
