# R5 final delivery — 2026-10-08 UTC

Project: `C:\Users\ggbo\Documents\Codex\2026-10-06\task\GreyzoneRaid`.
Run: `Packaged_R5/Windows/GreyzoneRaid.exe`; keep the whole Windows folder.
Instructions: `README.md`, `R5_BEGINNER_GUIDE.md`, `TEST_RESULTS.md`, packaged `START_HERE.txt`.

Implemented: first-person extraction map, firing/reload/damage, navigation-based guards, dual container/backpack panels, sequential reveals, explicit take/store/use, interruptible search, persistent same-raid containers, enemy bags, extraction/death/restart. Polish includes Chinese typography, row progress, action feedback, capacity messages, blocked patrol recovery and safer drop placement. No multiplayer, operator skills or persistent economy.

Final Editor build: succeeded in 16.92 seconds. Final Win64 Development BuildCookRun: succeeded in 50.69 seconds, ExitCode 0. Source, Content and Config were frozen before packaging and all final runs.

| Final evidence under Evidence | Result | PNGs |
| --- | --- | --- |
| R5Container_Editor | 152/152, exit 0, 1600x900 | 13 |
| R5Flow_Editor | 251/251, real Quit and exit 0, 1600x900 | 12 |
| SelfTest_R5_Editor.log | 75/75, graceful exit logged | 0 |
| R5Visual_Editor | complete, 1600x900 | 15 |
| R5Container_Packaged | 152/152, exit 0, 1600x900 | 13 |
| R5Flow_Packaged | 251/251, real Quit and exit 0, 1600x900 | 12 |
| SelfTest_R5_Packaged.log | 75/75, graceful exit logged | 0 |
| R5Visual_Packaged | complete, 1600x900 | 15 |
| R5Container_Packaged720 | 152/152, exit 0, 1280x720 | 13 |
| R5Flow_Packaged720 | 251/251, real Quit and exit 0, 1280x720 | 12 |

Ten final runs and 105 PNGs are archived. SelfTest and Visual have normal-shutdown logs, but no numeric process-exit receipts. Flow and Container record exit 0.

Container Phase 2 keeps three saved guards active and adds one native guard at unchanged defaults. Real AI damage interrupts search at 100 to 92 HP, closes UI and restores bound movement/look/fire. A disclosed 4-HP fixture then proves real-AI lethal damage cancels search and secures zero value. Isolated UI phases and the background-AI-suppressed Visual director do not substitute for this combat evidence.

Nine final JPEGs are under `Packaged_R5/Windows/Screenshots`. Seven existing Library previews were replaced in place as version 1; two normal-AI screenshots were created as version 0. All nine writes succeeded. Exact identities and local paths: `Evidence/R5Library_Delivered_Result.json`. Windows could not apply POSIX xattrs; helper failures are recorded separately and identities retained in the receipt. No sharing or publishing was performed.

Historical backups remain: R4 package/source-content archive, `Backups/R5_CoreValidated_20261008`, `Evidence/R5Core_BeforePolish`, failed logs, full first R5 package under `Backups/R5_BeforeVisualHarnessFix_20261008`, and `Evidence/R5Offscreen_BeforeForceRes` containing the 888x500 captures. The failed Visual was an abandoned raid, not proven AI death; the original input source was not logged. Final automation uses RenderOffscreen plus ForceRes and checks PNG dimensions. Normal `Run.ps1` retains a game window.

Font: 511 requested characters, 522 baked world characters, zero missing, full original Noto font embedded. Raw licenses and attribution accompany the package; see `ASSET_SOURCES_R5.md`.

Delivery manifest: **PASS** — `Evidence/R5_DeliveryManifest.json` verifies ten runs, 105 PNGs, final input/package hashes, font coverage, copied licenses and beginner guide. No Source/Content/Config files changed after packaging.

Native desktop UI automation is unavailable. Physical keyboard/mouse focus, subjective handling and audio listening remain untested. Silent automation uses UE methods, bound input delegates and Canvas hitbox dispatch, not manual gameplay.
