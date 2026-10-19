# R4 中文字体来源、导入与验证

2026-10-07，在本机 UE **5.8.3-58210709** 中完成实际导入。字体选用官方 **Noto Sans CJK SC Regular 2.004**，为简体中文无衬线黑体。没有复制 Windows 系统字体，没有安装字体或新组件。

## 官方来源与分发许可

官方仓库：[notofonts/noto-cjk](https://github.com/notofonts/noto-cjk)。固定下载版本为提交 `f8d157532fbfaeda587e826d4cd5b21a49186f7c`，使用完整静态 OTF。

| 本地文件（项目相对路径） | 字节数 | SHA-256 |
| --- | ---: | --- |
| `Downloads/Fonts/NotoSansCJKsc-Regular.otf` | 16,437,364 | `2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b` |
| `Downloads/Fonts/LICENSE.NotoSansCJK.txt` | 4,301 | `6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2` |

下载地址：[完整 OTF](https://raw.githubusercontent.com/notofonts/noto-cjk/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf)、[完整官方许可](https://raw.githubusercontent.com/notofonts/noto-cjk/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans/LICENSE)。许可为 **SIL Open Font License 1.1**，允许字体随软件嵌入和分发，需保留版权与许可，不可将字体单独售卖。字体本身及其修改版本继续遵守 OFL；这不要求游戏改为 OFL。

`Downloads/Fonts/COPYRIGHT.NotoSansCJK.txt` 从原始 OTF 的 `name` 表提取原始版权与许可说明；原始版权为 `© 2014-2021 Adobe (http://www.adobe.com/).`。`Downloads/Fonts/SOURCES.json` 记录来源、固定提交、文件大小、哈希、字体内部版本、覆盖输入文件及其哈希。

发布独立包时，将以下两份可读文本一并放入 `Windows/ThirdPartyLicenses/`：

- `LICENSE.NotoSansCJK.txt`
- `COPYRIGHT.NotoSansCJK.txt`

完整 OTF 原件保留在项目下载目录，游戏包通过 FontFace 的 Inline 载荷嵌入完整字节，无需依赖玩家安装字体或访问本机字体路径。

## 资产与渲染方式

| 资产路径 | 用途与实测设置 |
| --- | --- |
| `/Game/UI/Fonts/F_RaidChinese.F_RaidChinese` | HUD/Canvas 用 Runtime `UFont`，CompositeFont 默认字体面名 `Regular`；LegacyFontName=`Regular`，LegacyFontSize=16 |
| `/Game/UI/Fonts/FF_RaidChinese.FF_RaidChinese` | `UFontFace`，LoadingPolicy=`Inline`，Hinting=`Default`，LayoutMethod=`Metrics`，原始完整 OTF |
| `/Game/UI/Fonts/F_RaidChineseWorld.F_RaidChineseWorld` | 世界 TextRender 用 Offline `UFont`，48 pt、2048×2048 最大页、单页、距离场 alpha、生成倍率2、扫描半径倍率0.5、字间留白4 |

HUD 应以实际字号请求字体缓存：

```cpp
HUDFont = LoadObject<UFont>(nullptr,
    TEXT("/Game/UI/Fonts/F_RaidChinese.F_RaidChinese"));
const FSlateFontInfo Info(HUDFont, ActualPointSize, TEXT("Regular"));
FCanvasTextItem Item(Position, Text, Info, Color);
Item.Scale = FVector2D::UnitVector;
Canvas->DrawItem(Item);
```

字号/UI缩放应计入 `ActualPointSize`。把已有小号栅格通过 `Item.Scale` 放大，会继续出现发虚；测量与绘制必须使用同一 FontInfo 和 DPI。`FSlateFontInfo` 需要指向 Runtime **UFont**，不能直接指向 FontFace。

UE5.8 的 `UTextRenderComponent` 明确拒绝 Runtime 字体。地图标牌和动态世界标签必须使用 `F_RaidChineseWorld`；保留现有 TextRender 组件。引擎的 TextRender MID 缓存会将带 FontSampleParameter 的文字材质参数绑定为组件所选字体；本次离线子集使用单页，兼容现有默认文字材质流程。

## 导入及无安装加载

入口为 `Scripts/import_r4_font.py`。它只创建/更新上述 R4 字体资产，读取源码和文本目录来构造字表，不修改地图或玩法源码。

Runtime 导入使用实际验证过的 `unreal.FontFileImportFactory`、`AssetImportTask`、`FontFactory`、`FontCacheType.RUNTIME`、`FontLoadingPolicy.INLINE`。CompositeFont 嵌套类型的 `UPROPERTY` 没有 Python 编辑器 setter，脚本使用引擎公开的 `StructBase.import_text()` 写入 `DefaultTypeface.Fonts[0].Font.FontFaceAsset`，并验证导出文本中的资产引用和 `Regular` 面名。

世界字体使用 `unreal.TrueTypeFontFactory` 的 `import_options.data`。脚本只在当前 Python/UE 进程内通过 Windows `AddFontResourceExW(..., FR_PRIVATE=0x10)` 注册下载字体，并在 `finally` 中调用 `RemoveFontResourceExW`。没有写系统字体目录或字体注册表。导入前会检查 GDI 实际选中的字体名称，并读取 GDI 字体字节与原始 OTF 比较完整 SHA-256，防止系统替代字体被误烘焙。

**必须用隐藏/离屏编辑器 Python 执行模式。** 本机实测纯 `-run=pythonscript` commandlet 在字体导入后的 `UFontFace::PostEditChangeProperty` 中刷新未初始化的 Slate 缓存并断言。使用 UE 自身正常 Slate 初始化解决，未改引擎，也不进行原生 UI 自动化。

示例 PowerShell（先协调 UE/编译使用时段，受限环境通过正常执行审批运行）：

```powershell
$fontEditorArgs = @(
  '"C:\Users\ggbo\Documents\Codex\2026-10-06\task\GreyzoneRaid\GreyzoneRaid.uproject"',
  '-ExecutePythonScript="C:\Users\ggbo\Documents\Codex\2026-10-06\task\GreyzoneRaid\Scripts\import_r4_font.py"',
  '-unattended', '-nop4', '-nosound', '-NoSplash', '-RenderOffscreen',
  '-abslog="C:\Users\ggbo\Documents\Codex\2026-10-06\task\GreyzoneRaid\Evidence\R4_FontImport.log"'
)
Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe' `
  -ArgumentList $fontEditorArgs -WindowStyle Hidden -Wait
```

`-ExecutePythonScript` 执行完成后由 UE 的 `EditorPythonExecuter` 正常请求 `QUIT_EDITOR`；本次运行已正常退出。成功标志为日志里的 `R4_FONT_IMPORT_OK` 和 `Evidence/R4_FontImport.json`。

## 字符覆盖和实际验证

2026-10-07 实测：完整 OTF Unicode cmap 包含 **44,810** 个码点。导入字表共 **487** 个可打印字符，包含 ASCII、中文及项目使用的标点，源字体缺字 **0**，离线字体已用页索引集合 **[0]**。字表输入包含：

- `Source` 下全部 `.cpp`、`.h` 的非 ASCII 可打印字符。
- `Scripts/*r4*.py` 与 `Scripts/*r4*.json`，包括最终 `r4_text_catalog.json`。
- `Downloads/Fonts/R4_world_text.txt` 和导入脚本内初始世界词表。

脚本用 Python 标准库解析 OpenType `cmap` format 4/12 与 `name` 表，无需安装第三方包。执行以下检查不启动 UE：

```powershell
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' `
  .\Scripts\import_r4_font.py --verify-source
& 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\ThirdParty\Python3\Win64\python.exe' `
  .\Scripts\import_r4_font.py --verify-baked
```

第一项验证原件、完整许可、字体内部名称和所有当前字符的 cmap 覆盖；第二项将当前文本与导入报告中的已烘焙字表比较，同时核对三个资产文件并确认 FontFace 内含原始 OTF 的完整连续字节。`Evidence/R4_FontValidation.json` 已记录 `R4_FONT_BAKED_OK`、缺字0、资产哈希以及原始 OTF 在 FontFace 文件中的字节偏移1668。

| 实际保存的资产 | 大小（字节） |
| --- | ---: |
| `Content/UI/Fonts/F_RaidChinese.uasset` | 6,764 |
| `Content/UI/Fonts/FF_RaidChinese.uasset` | 16,439,088 |
| `Content/UI/Fonts/F_RaidChineseWorld.uasset` | 583,991 |

新增世界汉字后，先更新文本目录再重新执行导入脚本；脚本发现新增字符时只重新生成 `F_RaidChineseWorld`，保留原来字表并合并新增字符，不静默复用缺字子集。单页断言不通过时应调整页尺寸或子集范围后重导，不忽略结果。当前独立包画面效果及运行时加载已由 R4 回归和截图验证，结果见 [TEST_RESULTS.md](TEST_RESULTS.md)。

## Cook 注意事项与核对的引擎实现

在 `Config/DefaultGame.ini` 的 `[/Script/UnrealEd.ProjectPackagingSettings]` 中保留：

```ini
+DirectoriesToAlwaysCook=(Path="/Game/UI/Fonts")
```

项目为 `bCookAll=False`，仅靠运行时 `LoadObject` 字符串不是确定的 Cook 入口；上述目录规则和地图/类的硬引用保证字体根资产被纳入。Runtime UFont 的 CompositeFont 序列化了 FontFace 对象引用，FontFace 的 Inline 策略将字节写入 Cook 资产，不需要原始 Downloads 路径或外置 `.ufont`。离线 atlas 为 UFont 包内子对象，随字体序列化。

本机源码核对点（均位于 `C:\Program Files\Epic Games\UE_5.8\Engine\Source`）：

- `Runtime/Engine/Private/FontFace.cpp:126`：Inline 载荷保存；`:254`：只有非 Inline 才输出额外 `.ufont`；`:224`：PostEdit 的 Slate 字体缓存刷新。
- `Runtime/SlateCore/Private/Fonts/CompositeFont.cpp:356`：Cook 序列化 `FontFaceAsset` 引用。
- `Runtime/Engine/Public/CanvasItem.h:448`：FontInfo 的对象转换为 UFont；`Private/UserInterface/CanvasItem.cpp:1193`：按 FontInfo 与 DPI 获取字形，`:1299`：Scale 对已生成字形再缩放。
- `Runtime/Engine/Private/Components/TextRenderComponent.cpp:729`：拒绝 Runtime 字体；`:334`–`:359`：材质字体参数按组件字体与页绑定。
- `Editor/UnrealEd/Private/Factories/TTFontImport.cpp:1521`：CreateFontW；`:1565`：GetFontData 转 FreeType；`:1952`：显式 Chars 也约束 ASCII；`:1965`：缺字会被替换为空格，故必须先做 cmap 检查。
- 同文件 `:1876` 的临时位图会按距离场倍率放大；本次显式用2，避免2048页采用默认16导致巨额内存甚至32位长度溢出。
- `Developer/AssetTools/Public/IAssetTools.h:349`：`CreateAsset(..., bOverwriteExisting)` 支持无弹框更新同名 R4 字体。
- `Runtime/Engine/Classes/Engine/FontImportOptions.h`：已实际使用的离线参数定义。

验收独立包时同时检查 Cook/资产清单中的三个字体路径、Runtime FontFace 的 Inline 设置、世界标签显示以及玩家可读的 OFL/版权文本。独立包测试应在 `-nosound` 下运行。
