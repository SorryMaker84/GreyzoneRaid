# R6 资源来源与范围

R6 主要改造 C++ 框架，没有下载新模型、纹理、字体或音频，没有购买、商业游戏资源提取或发布。

- `Content/Data/DA_RaidItems.uasset`：本项目原创物品定义，由 [create_r6_item_catalog.py](Scripts/create_r6_item_catalog.py) 生成。Tech、Medkit、Ammo、Rations 延续现有玩法；医疗包恢复 45，弹药盒补充 24。运行时标签、单价变体继续保留容器原配置。
- 灰盒地图、原创呈现及既有第三方资源沿用 R5。来源和许可见 [R2](ASSET_SOURCES_R2.md)、[R3](ASSET_SOURCES_R3.md)、[R4](ASSET_SOURCES_R4.md)、[字体](ASSET_FONT_R4.md)、[R5](ASSET_SOURCES_R5.md)。
- Windows 包继续附带 Noto 字体 OFL、SteinGames 和 Kenney 原许可与来源说明，位于 `ThirdPartyLicenses`。
- 本机 UE 源码仅用于阅读和定位证据；项目未复制或分发引擎实现源码。

新物品定义只是数值、名称及效果类型，不依赖额外账号、外部服务或新协议。
