# Third-party content and public source snapshot

Reviewed 2026-10-19. This repository contains the R10 Unreal project, not an engine distribution or a packaged game. It does not grant a blanket open-source license to third-party content or select a new license for the project author's code.

- Epic template content: all 128 files under `Content/Characters/Mannequins` and all eight files under `Content/Weapons/Rifle` were SHA-256 matched against this machine's installed UE 5.8 `Templates` directory. Epic's EULA section 1 defines content from Templates as Examples; section 5(b) permits distribution of Examples, including modifications, in source or object form. These assets remain Epic content, not CC0. See https://www.unrealengine.com/eula/unreal . The engine's BasicShapes, default fonts and engine code are referenced through the installed engine, not copied into this repository.
- Material textures: Poly Haven assets are CC0 (https://polyhaven.com/license); ambientCG Concrete034 is CC0 (https://docs.ambientcg.com/license/). Original asset names, authors and source paths are recorded in `ASSET_SOURCES_R2.md` and `ASSET_SOURCES_R7.md`.
- Classic Weapons Pack v1.1: Stein Games, CC0. Existing downloaded source and original license retained; no new Fab download or agreement. https://stein-indie.itch.io/classic-weapons-pack
- FPS arms: para, with MakeHuman mesh/texture attribution, CC0. https://opengameart.org/content/fps-arms-rigged-only
- Firearm recordings: Ben Jaszczak, Brian Nelson, Kevin Heras, Matthew Nanney, CC0. https://opengameart.org/content/the-free-firearm-sound-library
- Impact sounds: Kenney, CC0. https://kenney.nl/assets/impact-sounds
- Noto Sans CJK SC Regular: Adobe copyright, SIL OFL 1.1. Full original copyright and license are included in `ThirdPartyLicenses` and `Downloads/Fonts`; the font is not relicensed as project code.

The early `ASSET_SOURCES.md` describes the initial graybox only. Later R2-R7 attribution records supersede its statement about no third-party downloads. Historical test/asset documents describe the local work at their original dates, not a claim that this repository has never been published.

Build products, caches, downloaded archives, private backups, bulk evidence and local credentials are excluded. A local Android File Server SecurityToken is omitted from the exported configuration. Existing gameplay source and Content bytes are unchanged.
