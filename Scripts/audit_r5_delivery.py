"""Audit final R5 evidence and package; only write Evidence/R5_DeliveryManifest.json.

Run after the final package and all archived test runs are complete. No UE process
is started. Check counts come from the actual result markers, with minimums below.
"""
import hashlib
import json
import platform
import re
import struct
from datetime import datetime, timezone
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "Evidence"
PACKAGE = ROOT / "Packaged_R5/Windows"
UTC = timezone.utc
TIME_TOLERANCE = 2.0  # Log timestamps have milliseconds; filesystem clocks vary.
FLOW_SCREENS = (
    "01_main_menu.png", "02_instructions.png", "03_running.png",
    "03a_container_unknown.png", "03b_container_revealed.png",
    "03c_container_empty.png", "04_extraction_countdown.png",
    "05_extracted_success.png", "06_dead_failure.png", "07_restart_clean.png",
    "08_return_menu.png", "09_abandoned.png",
)
CONTAINER_SCREENS = (
    "01_unknown.png", "02_search_partial.png", "03_first_revealed.png",
    "04_chosen_first_item.png", "05_full_backpack.png", "06_inventory_choice.png",
    "07_enemy_loot_unknown.png", "08_enemy_loot_taken.png",
    "09_extraction_countdown.png", "10_extraction_success.png",
    "11_death_during_search.png", "12_live_combat_interrupt.png",
    "13_live_combat_death.png",
)
VISUAL_SCREENS = (
    "01_depot_hip.png", "02_depot_ads.png", "03_rifle_fire.png", "04_rifle_reload.png",
    "05_guard_midrange.png", "06_extraction_landmark.png", "07_guard_hit_reaction.png",
    "08_damage_direction.png", "09_container_unknown.png", "10_container_searching.png",
    "11_container_revealed.png", "12_container_taken.png", "13_backpack.png",
    "14_extraction_countdown.png", "15_extracted_result.png",
)
LICENSES = {
    "Downloads/Fonts/LICENSE.NotoSansCJK.txt": "LICENSE.NotoSansCJK.txt",
    "Downloads/Fonts/COPYRIGHT.NotoSansCJK.txt": "COPYRIGHT.NotoSansCJK.txt",
    "Downloads/Models/SteinGames/Extracted/WeaponsPack/license.txt":
        "SteinGames_ClassicWeapons_LICENSE.txt",
    "Downloads/Audio/KenneyImpact/Extracted/License.txt":
        "Kenney_ImpactSounds_LICENSE.txt",
}
SOURCE_DOCS = (
    "ASSET_SOURCES_R2.md", "ASSET_SOURCES_R3.md", "ASSET_SOURCES_R4.md",
    "ASSET_FONT_R4.md", "ASSET_SOURCES_R5.md",
)
LOG_TIME = re.compile(r"\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]")
FAIL_MARKER = re.compile(r"\bFAIL\b|\bR5_VISUAL_(?:REVIEW|CAPTURE)_FAILED\b")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def read(path):
    require(path.is_file() and path.stat().st_size > 0, f"Missing/empty file: {path}")
    return path.read_text(encoding="utf-8-sig", errors="replace")


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def record(path):
    stat = path.stat()
    require(stat.st_size > 0, f"Empty file: {path}")
    return {
        "path": path.relative_to(ROOT).as_posix(), "bytes": stat.st_size,
        "modified_utc": datetime.fromtimestamp(stat.st_mtime, UTC).isoformat(),
        "sha256": sha(path),
    }


def timestamp(value):
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    require(parsed.tzinfo is not None, f"Timestamp has no timezone: {value}")
    return parsed.timestamp()


def log_window(log, label):
    times = LOG_TIME.findall(log)
    require(times, f"No UTC engine timestamps: {label}")
    parsed = [datetime.strptime(value, "%Y.%m.%d-%H.%M.%S:%f").replace(tzinfo=UTC).timestamp()
              for value in (times[0], times[-1])]
    require(parsed[0] <= parsed[1], f"Invalid log time window: {label}")
    return parsed


def in_window(path, started, finished):
    modified = path.stat().st_mtime
    require(started - TIME_TOLERANCE <= modified <= finished + TIME_TOLERANCE,
            f"Stale/out-of-run evidence: {path}")


def run_mode(log, packaged, label):
    directories = re.findall(r"LogInit: Base Directory:\s*([^\r\n]+)", log)
    require(len(directories) == 1, f"Missing/ambiguous executable directory: {label}")
    actual = directories[0].strip().replace("\\", "/").rstrip("/").casefold()
    if packaged:
        expected = (PACKAGE / "GreyzoneRaid/Binaries/Win64").as_posix().casefold()
        require(actual == expected, f"Evidence was not run from the R5 package: {label}")
    else:
        require(actual.endswith("/ue_5.8/engine/binaries/win64"), f"Evidence is not an UE 5.8 editor run: {label}")


def result_marker(log, prefix, minimum, label):
    require(not FAIL_MARKER.search(log), f"FAIL marker found: {label}")
    markers = [line for line in log.splitlines() if prefix + ":" in line]
    require(len(markers) == 1, f"Expected one {prefix} result: {label}")
    match = re.search(re.escape(prefix) + r":\s+PASS\s+checks=(\d+)\s+failures=(\d+)\b", markers[0])
    require(match is not None, f"No passing result: {label}")
    checks, failures = map(int, match.groups())
    require(checks >= minimum and failures == 0,
            f"Incomplete/failed checks ({checks}, {failures}): {label}")
    check_prefix = "RAID_CHECK" if prefix == "RAID_SELFTEST_RESULT" else prefix.replace("_RESULT", "_CHECK")
    pass_count = len(re.findall(re.escape(check_prefix) + r":\s*PASS\b", log))
    require(pass_count == checks, f"Result/check-line count mismatch ({checks}/{pass_count}): {label}")
    tail = log[log.index(markers[0]) + len(markers[0]):]
    require("LogExit: Exiting." in tail, f"No normal shutdown after result: {label}")
    return {"result": markers[0], "checks": checks, "failures": failures}, tail


def screenshots(base, names, count, dimensions, started, finished):
    paths = sorted(path for path in base.iterdir() if path.is_file() and path.suffix.lower() == ".png")
    require(len(paths) == count, f"Expected {count} screenshots, found {len(paths)}: {base}")
    if names is not None:
        require({path.name for path in paths} == set(names), f"Unexpected screenshot names: {base}")
    for path in paths:
        require(path.stat().st_size > 128, f"Incomplete PNG: {path}")
        with path.open("rb") as stream:
            header = stream.read(24)
        require(header[:8] == b"\x89PNG\r\n\x1a\n" and header[8:16] == b"\x00\x00\x00\rIHDR",
                f"Invalid PNG signature/IHDR: {path}")
        require(struct.unpack(">II", header[16:24]) == dimensions, f"Wrong PNG dimensions: {path}")
        in_window(path, started, finished)
    return paths


def container_live_evidence(log, label):
    # Require evidence emitted before the final result, from the live-AI phase.
    before_result = log.split("RAID_CONTAINER_RESULT:", 1)[0]
    require("RAID_CONTAINER_SCOPE: normal_AI_looting_risk;" in before_result,
            f"Missing normal-AI looting-risk scope: {label}")
    events = {}
    for name in ("INTERRUPTED", "DEATH"):
        lines = [line for line in before_result.splitlines() if f"RAID_CONTAINER_LIVE_{name}:" in line]
        require(lines, f"Missing live-AI {name.lower()} evidence: {label}")
        events[name.lower()] = lines[-1]
    hits = [line for line in before_result.splitlines() if "RAID_CONTAINER_LIVE_HIT:" in line
            and all(re.search(r"\b" + flag + r"=1\b", line)
                    for flag in ("normal_AI", "panel_open", "searching"))]
    require(hits, f"No normal-AI hit while the container panel and search are active: {label}")
    checks = (
        "normal_AI_next_bullet_is_the_real_lethal_search_interrupt",
        "live_AI_hit_bound_MoveForward_produces_real_lane_movement",
        "live_phase_never_disables_or_weakens_any_guard",
    )
    for name in checks:
        require(re.search(r"RAID_CONTAINER_CHECK:\s*PASS\s+" + re.escape(name) + r"(?=\s|\||$)", before_result),
                f"Missing required live-AI PASS assertion {name}: {label}")
    return {"scope": "normal_AI_looting_risk", "events": events,
            "normal_ai_hits_during_search": hits, "required_pass_assertions": list(checks)}


def receipt_run(folder, kind, minimum, names, dimensions, earliest, artifacts):
    base = EVIDENCE / folder
    log_path = base / f"{kind}Test.log"
    receipt_path = base / f"{kind}TestResult.json"
    log = read(log_path)
    run_mode(log, "Packaged" in folder, folder)
    result, tail = result_marker(log, f"RAID_{kind.upper()}_RESULT", minimum, folder)
    receipt = json.loads(read(receipt_path))
    for key, expected in (("status", "PASS"), ("checks", result["checks"]),
                          ("failures", 0), ("process_exit_code", 0),
                          ("width", dimensions[0]), ("height", dimensions[1])):
        require(receipt.get(key) == expected, f"Receipt {key} mismatch: {folder}")
    require(receipt.get("hardware_input_tested") is False, f"Unexpected hardware-input claim: {folder}")
    if kind == "Container":
        require(receipt.get("audio_listening_tested") is False, f"Unexpected audio claim: {folder}")
        result["live_combat_evidence"] = container_live_evidence(log, folder)
    if kind == "Flow":
        require(receipt.get("real_frontend_quit_after_result") is True and
                "RAID_QUIT: requested from frontend" in tail, f"Missing actual frontend Quit: {folder}")
        require(tail.index("RAID_QUIT: requested from frontend") < tail.index("LogExit: Exiting."),
                f"Frontend Quit was not before shutdown: {folder}")
    names_key = "required_screenshots" if kind == "Flow" else "screenshots"
    receipt_names = receipt.get(names_key)
    require(isinstance(receipt_names, list) and len(receipt_names) == len(names)
            and set(receipt_names) == set(names), f"Receipt screenshot mismatch: {folder}")
    started, finished = timestamp(receipt["started_utc"]), timestamp(receipt["finished_utc"])
    require(earliest <= started <= finished <= datetime.now(UTC).timestamp() + TIME_TOLERANCE,
            f"Receipt predates final inputs/package or has invalid dates: {folder}")
    first_log, last_log = log_window(log, folder)
    require(started - TIME_TOLERANCE <= first_log <= last_log <= finished + TIME_TOLERANCE,
            f"Log does not belong to receipt run: {folder}")
    in_window(log_path, started, finished)
    require(receipt_path.stat().st_mtime >= finished - TIME_TOLERANCE, f"Stale receipt: {folder}")
    capture_match = re.search(r"\bcaptures=(\d+)\b", result["result"])
    require(capture_match is not None and int(capture_match[1]) == len(names),
            f"Result capture count mismatch: {folder}")
    pngs = screenshots(base, names, len(names), dimensions, started, finished)
    artifacts.extend([log_path, receipt_path, *pngs])
    result.update(folder=folder, screenshots=len(pngs), screenshot_names=[path.name for path in pngs],
                  width=dimensions[0], height=dimensions[1], process_exit_code=0,
                  started_utc=receipt["started_utc"], finished_utc=receipt["finished_utc"],
                  graceful_exit_logged=True,
                  normal_exit_evidence="receipt exit code 0 and LogExit after PASS")
    return result


def log_run(mode, kind, earliest, artifacts):
    is_visual = kind == "Visual"
    path = EVIDENCE / (f"R5Visual_{mode}/VisualReview.log" if is_visual else f"SelfTest_R5_{mode}.log")
    log = read(path)
    run_mode(log, mode == "Packaged", path.name)
    started, finished = log_window(log, path.name)
    require(started >= earliest, f"Run predates final inputs/package: {path}")
    in_window(path, started, finished)
    if is_visual:
        require(not FAIL_MARKER.search(log), f"FAIL marker found: {path}")
        matches = list(re.finditer(r"R5_VISUAL_REVIEW_COMPLETE:\s+screenshots=(\d+)\b", log))
        require(len(matches) == 1 and int(matches[0][1]) == 15, f"Incomplete visual review: {path}")
        require("LogExit: Exiting." in log[matches[0].end():], f"No normal shutdown after review: {path}")
        pngs = screenshots(path.parent, VISUAL_SCREENS, 15, (1600, 900), started, finished)
        artifacts.extend(pngs)
        result = {"folder": path.parent.name, "result": matches[0][0], "screenshots": len(pngs),
                  "screenshot_names": [item.name for item in pngs], "width": 1600, "height": 900}
    else:
        result, _ = result_marker(log, "RAID_SELFTEST_RESULT", 75, path.name)
        result["log"] = path.name
    artifacts.append(path)
    result.update(started_utc=datetime.fromtimestamp(started, UTC).isoformat(),
                  finished_utc=datetime.fromtimestamp(finished, UTC).isoformat(),
                  process_exit_code=None, graceful_exit_logged=True,
                  normal_exit_evidence="LogExit after result; no numeric exit-code receipt")
    return result


def copied_file(source, destination):
    read(source)
    read(destination)
    source_record, destination_record = record(source), record(destination)
    require(source_record["sha256"] == destination_record["sha256"], f"Copied file differs: {destination}")
    return {"source": source_record, "packaged": destination_record}


def main():
    build_path, cook_path = EVIDENCE / "EditorBuild_R5_Integrated_Console.log", EVIDENCE / "Package_R5.log"
    build, cook = read(build_path), read(cook_path)
    require("Result: Succeeded" in build, "Editor build did not succeed")
    require("BUILD SUCCESSFUL" in cook and "AutomationTool exiting with ExitCode=0" in cook,
            "Package build did not succeed")
    package_finished = cook_path.stat().st_mtime
    inputs = [ROOT / "GreyzoneRaid.uproject"]
    for name in ("Source", "Content", "Config"):
        folder = ROOT / name
        require(folder.is_dir(), f"Missing project input directory: {folder}")
        inputs.extend(path for path in folder.rglob("*") if path.is_file())
    late = [path.relative_to(ROOT).as_posix() for path in inputs if path.stat().st_mtime > package_finished]
    require(not late, f"Project inputs changed after packaging: {late}")
    latest_input = max(path.stat().st_mtime for path in inputs)
    artifacts = [build_path, cook_path]
    runs = []
    for mode, dimensions in (("Editor", (1600, 900)), ("Packaged", (1600, 900)), ("Packaged720", (1280, 720))):
        earliest = latest_input if mode == "Editor" else max(latest_input, package_finished)
        runs.append(receipt_run(f"R5Flow_{mode}", "Flow", 251, FLOW_SCREENS, dimensions, earliest, artifacts))
        runs.append(receipt_run(f"R5Container_{mode}", "Container", 152, CONTAINER_SCREENS,
                                dimensions, earliest, artifacts))
    for mode in ("Editor", "Packaged"):
        earliest = latest_input if mode == "Editor" else max(latest_input, package_finished)
        runs.append(log_run(mode, "SelfTest", earliest, artifacts))
        runs.append(log_run(mode, "Visual", earliest, artifacts))
    require(len(runs) == 10 and sum(run.get("screenshots", 0) for run in runs) == 105,
            "Final R5 delivery must contain ten runs and 105 screenshots")

    font_path = EVIDENCE / "R5_FontValidation.json"
    font = json.loads(read(font_path))
    require(font.get("status") == "R5_FONT_BAKED_OK", "Font validation did not pass")
    require(font.get("font_cmap_missing") == [] and font.get("world_subset_missing") == [], "Missing font glyphs")
    require(0 < font["current_requested_characters"] <= font["baked_world_characters"], "Invalid font coverage counts")
    expected_fonts = {f"Content/UI/Fonts/{name}.uasset" for name in
                      ("F_RaidChinese", "FF_RaidChinese", "F_RaidChineseWorld")}
    require(len(font["assets"]) == 3 and {item["file"] for item in font["assets"]} == expected_fonts,
            "Font validation does not cover the three delivered assets")
    for item in font["assets"]:
        asset = ROOT / item["file"]
        require(asset.stat().st_size == item["bytes"] and sha(asset) == item["sha256"], f"Stale font validation: {asset}")
        if asset.name == "FF_RaidChinese.uasset":
            require(item.get("complete_original_otf_payload_offset", -1) >= 0, "FontFace lacks complete-font evidence")
    artifacts.append(font_path)
    license_copies = [copied_file(ROOT / source, PACKAGE / "ThirdPartyLicenses" / destination)
                      for source, destination in LICENSES.items()]
    documentation = [copied_file(ROOT / name, PACKAGE / "ThirdPartyLicenses" / name) for name in SOURCE_DOCS]
    documentation.append(copied_file(ROOT / "R5_BEGINNER_GUIDE.md", PACKAGE / "R5_BEGINNER_GUIDE.md"))
    backup = ROOT / "Backups/R4_20261008/GreyzoneRaid_R4_SourceContent.zip"
    require(backup.is_file() and backup.stat().st_size == 256578737, "Original R4 backup is missing or size changed")
    for relative in ("GreyzoneRaid.exe", "GreyzoneRaid/Binaries/Win64/GreyzoneRaid.exe"):
        require((PACKAGE / relative).is_file(), f"Missing packaged executable: {relative}")
    excluded_parts = {"evidence", "saved", "docs", "screenshots"}
    payload = [path for path in PACKAGE.rglob("*") if path.is_file() and path.name != "START_HERE.txt"
               and not excluded_parts.intersection(part.lower() for part in path.relative_to(PACKAGE).parts)]
    extensions = {".exe", ".pak", ".utoc", ".ucas"}
    binaries = [path for path in payload if path.suffix.lower() in extensions]
    require({path.suffix.lower() for path in binaries} == extensions, "Package lacks exe/pak/utoc/ucas payloads")
    result = {
        "status": "PASS", "checked_utc": datetime.now(UTC).isoformat(), "machine": platform.node(),
        "tests": runs, "hardware_input_tested": False, "audio_listening_tested": False,
        "evidence_screenshot_count": sum(run.get("screenshots", 0) for run in runs),
        "original_r4_backup_bytes": backup.stat().st_size, "original_r4_backup": record(backup),
        "package_finished_utc": datetime.fromtimestamp(package_finished, UTC).isoformat(),
        "source_content_config_count": len(inputs), "inputs_modified_after_package": late,
        "package_payload_bytes": sum(path.stat().st_size for path in payload),
        "font_validation": font, "license_copies": license_copies, "documentation_copies": documentation,
        "source_content_config": [record(path) for path in sorted(inputs)],
        "package_binaries": [record(path) for path in sorted(binaries)],
        "evidence_files": [record(path) for path in sorted(set(artifacts))],
    }
    output = EVIDENCE / "R5_DeliveryManifest.json"
    output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": "PASS", "manifest": str(output), "runs": len(runs),
                      "screenshots": result["evidence_screenshot_count"],
                      "checks": {run.get("folder", run.get("log")): run["checks"]
                                 for run in runs if "checks" in run},
                      "hardware_input_tested": False, "audio_listening_tested": False}, indent=2))


if __name__ == "__main__":
    main()
