"""Capture immutable R6 run evidence, or audit the complete 12-run delivery.

No engine/build process is started. ``capture`` writes one new R6Final directory;
``audit`` only writes Evidence/R6_DeliveryManifest.json and a stdout summary.
Uses Python's standard library, including full PNG CRC/zlib/scanline validation.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import shutil
import struct
import sys
import zlib
from datetime import datetime, timezone
from pathlib import Path


UTC = timezone.utc
TOLERANCE = 2.0
SCHEMA = 1
SUITES = ("Framework", "Container", "Flow", "Self", "Visual")
MIN_CHECKS = {"Framework": 70, "Container": 152, "Flow": 251, "Self": 75}
FRAMEWORK_CATALOG_CHECKS = (
    "production_catalog_asset_loaded_without_fallback",
    "production_catalog_medkit_grant_and_use",
    "production_catalog_ammo_grant_and_use",
)
SCREENS = {
    "Framework": (),
    "Container": (
        "01_unknown.png", "02_search_partial.png", "03_first_revealed.png",
        "04_chosen_first_item.png", "05_full_backpack.png", "06_inventory_choice.png",
        "07_enemy_loot_unknown.png", "08_enemy_loot_taken.png",
        "09_extraction_countdown.png", "10_extraction_success.png",
        "11_death_during_search.png", "12_live_combat_interrupt.png", "13_live_combat_death.png",
    ),
    "Flow": (
        "01_main_menu.png", "02_instructions.png", "03_running.png",
        "03a_container_unknown.png", "03b_container_revealed.png", "03c_container_empty.png",
        "04_extraction_countdown.png", "05_extracted_success.png", "06_dead_failure.png",
        "07_restart_clean.png", "08_return_menu.png", "09_abandoned.png",
    ),
    "Self": (
        "01_combat_inventory.png", "02_extracted_success.png", "03_dead_failure.png",
        "04_restart_clean.png", "05_inventory_panel.png",
    ),
    "Visual": (
        "01_depot_hip.png", "02_depot_ads.png", "03_rifle_fire.png", "04_rifle_reload.png",
        "05_guard_midrange.png", "06_extraction_landmark.png", "07_guard_hit_reaction.png",
        "08_damage_direction.png", "09_container_unknown.png", "10_container_searching.png",
        "11_container_revealed.png", "12_container_taken.png", "13_backpack.png",
        "14_extraction_countdown.png", "15_extracted_result.png",
    ),
}
LOG_TIME = re.compile(r"\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]")
FAILURE = re.compile(
    r"\bFAIL\b|\bFatal error\s*:|\bAssertion failed\s*:|\bUnhandled Exception\s*:|"
    r"\bCritical error\s*:|\bR6_VISUAL_(?:REVIEW|CAPTURE)_FAILED\b|"
    r"\bR6_VISUAL_INVALID_STATE\b|\bR6_VISUAL_REVIEW:\s*timeout\b|"
    r"\bR6_VISUAL_DAMAGE:\s*no live enemy damage\b|\bLog\w+:\s*Error:", re.IGNORECASE
)
AGGREGATE_DESCRIPTION = "SHA256 of sorted UTF-8 (relative_posix_path + NUL + lowercase_file_sha256 + LF), no header"
EXPECTED_SCENARIOS = {
    "harness_empty", "find_normal", "count_empty", "count_typical", "count_full",
    "hud_closed_full", "hud_open_full",
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def utc(timestamp=None):
    return datetime.fromtimestamp(timestamp, UTC).isoformat() if timestamp is not None else datetime.now(UTC).isoformat()


def epoch(value):
    parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    require(parsed.tzinfo is not None, f"UTC offset is required: {value}")
    return parsed.timestamp()


def sha(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def read_text(path):
    require(path.is_file() and path.stat().st_size > 0, f"Missing/empty text file: {path}")
    data = path.read_bytes()
    if data.startswith((b"\xff\xfe", b"\xfe\xff")):
        return data.decode("utf-16")
    return data.decode("utf-8-sig", errors="replace")


def read_json(path):
    value = json.loads(read_text(path))
    require(isinstance(value, dict), f"Expected JSON object: {path}")
    return value


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def under(root, relative):
    require(not Path(relative).is_absolute(), f"Archive path must be relative: {relative}")
    resolved = (root / relative).resolve()
    require(resolved.is_relative_to(root.resolve()), f"Path escapes its evidence directory: {relative}")
    return resolved


def file_record(path, relative_to=None):
    require(path.is_file() and not path.is_symlink(), f"Expected regular file: {path}")
    before = path.stat()
    digest = sha(path)
    after = path.stat()
    require((before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns), f"File changed while hashing: {path}")
    return {
        "path": path.relative_to(relative_to).as_posix() if relative_to else str(path.resolve()),
        "bytes": after.st_size, "sha256": digest, "mtime_utc": utc(after.st_mtime),
    }


def aggregate(files):
    digest = hashlib.sha256()
    for item in sorted(files, key=lambda row: row["path"]):
        digest.update((item["path"] + "\0" + item["sha256"].lower() + "\n").encode("utf-8"))
    return digest.hexdigest()


def input_manifest(root):
    projects = sorted(root.glob("*.uproject"))
    require(len(projects) == 1, f"Expected exactly one uproject under {root}")
    paths = [projects[0]]
    for name in ("Source", "Content", "Config"):
        folder = root / name
        require(folder.is_dir() and not folder.is_symlink(), f"Missing/nonregular input directory: {folder}")
        for path in folder.rglob("*"):
            require(not path.is_symlink(), f"Symlink is not a reproducible project input: {path}")
            if path.is_file():
                paths.append(path)
    records = [file_record(path, root) for path in sorted(paths)]
    return {"algorithm": AGGREGATE_DESCRIPTION, "aggregate_sha256": aggregate(records), "files": records}


def source_hash(manifest):
    return aggregate([item for item in manifest["files"] if item["path"].startswith("Source/")])


def validate_manifest(manifest):
    require(manifest.get("algorithm") == AGGREGATE_DESCRIPTION, "Unknown input aggregate algorithm")
    files = manifest.get("files", [])
    require(files and len({row["path"] for row in files}) == len(files), "Missing or duplicate manifest paths")
    require(aggregate(files) == manifest.get("aggregate_sha256"), "Input manifest aggregate is invalid")
    return {(row["path"], row["bytes"], row["sha256"].lower()) for row in files}


def log_window(text):
    stamps = LOG_TIME.findall(text)
    require(stamps, "Engine log has no UTC timestamp prefixes")
    times = [datetime.strptime(stamp, "%Y.%m.%d-%H.%M.%S:%f").replace(tzinfo=UTC).timestamp()
             for stamp in (stamps[0], stamps[-1])]
    require(times[0] <= times[1], "Engine log timestamps are reversed")
    return times


def validate_log(path, suite):
    text = read_text(path)
    bad = FAILURE.search(text)
    require(not bad, f"Failure/error marker in {path}: {bad.group(0) if bad else ''}")
    if suite == "Visual":
        results = list(re.finditer(r"R6_VISUAL_REVIEW_COMPLETE:\s*screenshots=(\d+)", text))
        require(len(results) == 1 and int(results[0][1]) == 15, f"Expected one Visual completion with 15 screenshots: {path}")
        result = {"checks": None, "failures": 0, "screenshots": 15}
    else:
        prefix = {"Self": "RAID_SELFTEST", "Framework": "RAID_FRAMEWORK", "Container": "RAID_CONTAINER", "Flow": "RAID_FLOW"}[suite]
        results = list(re.finditer(rf"{prefix}_RESULT:\s*PASS\s+checks=(\d+)\s+failures=(\d+)([^\r\n]*)", text))
        require(len(results) == 1, f"Expected exactly one {suite} PASS result: {path}")
        checks, failures = int(results[0][1]), int(results[0][2])
        require(checks >= MIN_CHECKS[suite] and failures == 0, f"Incomplete/failed {suite}: checks={checks}, failures={failures}")
        check_prefix = "RAID_CHECK" if suite == "Self" else prefix + "_CHECK"
        require(len(re.findall(rf"\b{check_prefix}:\s*PASS\b", text)) == checks, f"Individual PASS count disagrees with {suite} summary")
        if suite in ("Container", "Flow"):
            captures = re.search(r"\bcaptures=(\d+)", results[0][3])
            require(captures and int(captures[1]) == len(SCREENS[suite]), f"Capture count disagrees with {suite} contract")
        if suite == "Framework":
            for phase in ("kernel", "registered_components", "interaction_lifecycle"):
                require(re.search(rf"RAID_FRAMEWORK_PHASE:\s*{phase}\b", text), f"Framework phase missing: {phase}")
            before_result = text[:results[0].start()]
            for name in FRAMEWORK_CATALOG_CHECKS:
                markers = re.findall(rf"\bRAID_FRAMEWORK_CHECK:\s*PASS[ \t]+{re.escape(name)}[ \t]+\|", before_result)
                require(len(markers) == 1, f"Framework requires exactly one production catalog PASS before its result: {name}")
        result = {"checks": checks, "failures": failures, "screenshots": len(SCREENS[suite])}
    final = results[0]
    exit_match = re.search(r"LogExit:\s*Exiting\.", text[final.end():])
    require(exit_match, f"Normal UE LogExit after result is absent: {path}")
    if suite == "Flow":
        quit_match = re.search(r"RAID_QUIT:\s*requested from frontend", text[final.end():])
        require(quit_match and quit_match.start() < exit_match.start(), "Flow did not quit through the frontend after its result")
    result["result_marker"] = final.group(0)
    result["engine_started_utc"], result["engine_finished_utc"] = map(utc, log_window(text))
    return result


def validate_log_executable(path, executables, mode):
    directories = re.findall(r"LogInit: Base Directory:\s*([^\r\n]+)", read_text(path))
    require(len(directories) == 1, f"Missing/ambiguous engine executable directory: {path}")
    role = "editor_exe" if mode == "Editor" else "packaged_inner_exe"
    executable = next(row for row in executables if row["role"] == role)
    expected = Path(executable["path"]).resolve().parent.as_posix().rstrip("/").casefold()
    actual = directories[0].strip().replace("\\", "/").rstrip("/").casefold()
    require(actual == expected, f"Engine log ran from {actual}, not the recorded {role} directory {expected}")


def native_exit_proof(wrapper_path, suite, mode, run_id, kind, root):
    text = read_text(wrapper_path)
    require(not FAILURE.search(text), f"Failure/error marker in wrapper output: {wrapper_path}")
    native = re.findall(r"R6_PROCESS_EXIT_CODE:\s*(-?\d+)", text)
    run_exits = re.findall(r"R6_RUN_EXIT:\s*run_id=([^\s]+)\s+(?:native_exit_code|exit_code)=(-?\d+)", text)
    named = [value for identity, value in run_exits if identity == run_id]
    require(all(int(value) == 0 for value in native + named), f"Nonzero native process exit marker: {wrapper_path}")
    if kind == "marker":
        values = native + named
        require(values and all(int(value) == 0 for value in values), f"No unambiguous native exit-zero marker in {wrapper_path}")
        return {"kind": "native_exit_marker", "native_exit_code": 0,
                "markers": re.findall(r"R6_(?:PROCESS_EXIT_CODE|RUN_EXIT):[^\r\n]*", text)}
    require(suite in ("Framework", "Container", "Flow"), "checked-wrapper proof is only supported for the three historically exit-checking wrappers")
    patterns = {
        "Framework": rf"R6 framework {mode} PASS:\s*(\d+) checks\.",
        "Container": r"Verified 13 fresh container screenshots and normal exit\.",
        "Flow": r"Real frontend Quit and normal process exit verified; twelve fresh PNG screenshots verified\.",
    }
    matches = list(re.finditer(patterns[suite], text))
    require(len(matches) == 1, f"Expected the explicit checked-wrapper success text for {suite}")
    if suite == "Framework":
        require(int(matches[0][1]) >= MIN_CHECKS["Framework"],
                f"Historical Framework wrapper result is below {MIN_CHECKS['Framework']} checks")
    script_path = root / "Scripts" / {"Framework": "FrameworkTest.ps1", "Container": "ContainerTest.ps1", "Flow": "FlowTest.ps1"}[suite]
    script = read_text(script_path)
    require(re.search(r"\.ExitCode\s+-ne\s+0", script), f"Native exit-code guard is absent from checked wrapper: {script_path}")
    return {
        "kind": "checked_wrapper_success", "native_exit_code": 0,
        "matched_line": matches[0].group(0), "guard_source": file_record(script_path),
        "qualification": "Native zero is inferred from the wrapper's explicit success line and audited ExitCode guard; no historical numeric native marker was invented. Guard source is recorded at capture time.",
    }


def validate_png(path, width, height):
    data = path.read_bytes()
    require(data[:8] == b"\x89PNG\r\n\x1a\n", f"Invalid PNG signature: {path}")
    offset, header, compressed, ended, saw_idat = 8, None, bytearray(), False, False
    palette_entries = 0
    while offset < len(data):
        require(offset + 12 <= len(data), f"Truncated PNG chunk header: {path}")
        length = struct.unpack_from(">I", data, offset)[0]
        kind = data[offset + 4:offset + 8]
        require(offset + 12 + length <= len(data), f"Truncated PNG {kind!r} chunk: {path}")
        body = data[offset + 8:offset + 8 + length]
        crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
        require(zlib.crc32(kind + body) & 0xffffffff == crc, f"PNG chunk CRC mismatch: {path}")
        if header is None:
            require(kind == b"IHDR" and length == 13, f"PNG does not begin with IHDR: {path}")
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IHDR":
            raise ValueError(f"Duplicate PNG IHDR: {path}")
        if kind == b"PLTE":
            require(not palette_entries and not saw_idat and 0 < length <= 768 and length % 3 == 0,
                    f"Invalid PNG palette: {path}")
            palette_entries = length // 3
        if kind == b"IDAT":
            saw_idat = True
            compressed.extend(body)
        if kind == b"IEND":
            require(length == 0 and saw_idat, f"Invalid PNG IEND: {path}")
            ended = True
            offset += 12
            break
        offset += length + 12
    require(ended and offset == len(data), f"PNG is truncated or has trailing data: {path}")
    image_width, image_height, depth, color, compression, filtering, interlace = header
    require((image_width, image_height) == (width, height), f"PNG dimensions {image_width}x{image_height}, expected {width}x{height}: {path}")
    allowed = {0: (1, 2, 4, 8, 16), 2: (8, 16), 3: (1, 2, 4, 8), 4: (8, 16), 6: (8, 16)}
    require(color in allowed and depth in allowed[color] and compression == filtering == 0 and interlace in (0, 1), f"Unsupported/invalid PNG IHDR: {path}")
    if color == 3:
        require(0 < palette_entries <= 2 ** depth, f"Indexed PNG has no valid palette: {path}")
    if color in (0, 4):
        require(palette_entries == 0, f"Grayscale PNG must not contain a palette: {path}")
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[color]
    passes = ((0, 0, 1, 1),) if interlace == 0 else ((0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2))
    rows = []
    for x, y, dx, dy in passes:
        columns, lines = max(0, (width - x + dx - 1) // dx), max(0, (height - y + dy - 1) // dy)
        if columns and lines:
            rows.extend([1 + (columns * channels * depth + 7) // 8] * lines)
    expected_bytes = sum(rows)
    inflater = zlib.decompressobj()
    raw = inflater.decompress(compressed, expected_bytes + 1)
    require(len(raw) == expected_bytes and inflater.eof and not inflater.unconsumed_tail and not inflater.unused_data, f"Incomplete/invalid PNG image stream: {path}")
    position = 0
    for row_bytes in rows:
        require(raw[position] <= 4, f"Invalid PNG scanline filter: {path}")
        position += row_bytes
    return {"width": width, "height": height, "png_crc_zlib_scanlines_valid": True}


def fresh(path, started, finished, label):
    modified = path.stat().st_mtime
    require(started - TOLERANCE <= modified <= finished + TOLERANCE, f"{label} is outside the recorded run window: {path} ({utc(modified)})")


def executable_records(args, root):
    if args.mode == "Editor":
        exe = Path(args.editor_exe).resolve()
        dll = Path(args.project_dll).resolve() if args.project_dll else root / "Binaries/Win64/UnrealEditor-GreyzoneRaid.dll"
        pairs = [("editor_exe", exe), ("project_dll", dll)]
    else:
        require(args.package_exe, "Packaged capture requires --package-exe (inner or bootstrap executable)")
        requested = Path(args.package_exe).resolve()
        inner = requested
        if not (inner.parent.name.lower() == "win64" and inner.parent.parent.name.lower() == "binaries"):
            candidates = sorted((requested.parent / "GreyzoneRaid/Binaries/Win64").glob("GreyzoneRaid*.exe"))
            require(len(candidates) == 1, "Cannot uniquely resolve packaged inner executable; pass it explicitly")
            inner = candidates[0]
        pairs = [("packaged_inner_exe", inner)]
        if requested != inner:
            pairs.append(("packaged_launcher_exe", requested))
    return [dict(file_record(path), role=role) for role, path in pairs]


def validate_package_build_log(path):
    text = read_text(path)
    require(re.search(r"\bBUILD SUCCESSFUL\b", text), f"UAT BUILD SUCCESSFUL marker missing: {path}")
    require(re.search(r"AutomationTool exiting with ExitCode=0\b", text), f"UAT exit-zero marker missing: {path}")
    require(not re.search(r"\bBUILD FAILED\b|AutomationTool exiting with ExitCode=(?!0\b)\d+|\bFatal error\s*:", text, re.IGNORECASE),
            f"UAT failure marker: {path}")


def validate_input_build_times(manifest, executables, payloads):
    binary = next(row for row in executables if row["role"] in ("project_dll", "packaged_inner_exe"))
    require(all(epoch(row["mtime_utc"]) <= epoch(binary["mtime_utc"]) + TOLERANCE
                for row in manifest["files"] if row["path"].startswith("Source/")),
            "Project Source is newer than the compiled game DLL/executable")
    if binary["role"] == "packaged_inner_exe":
        require(payloads, "Packaged run has no cooked .pak/.utoc/.ucas evidence")
        cooked_at = max(epoch(row["mtime_utc"]) for row in payloads)
        require(all(epoch(row["mtime_utc"]) <= cooked_at + TOLERANCE for row in manifest["files"]
                    if not row["path"].startswith("Source/")),
                "Content/Config/uproject is newer than the latest cooked package payload")


def capture(args):
    root = Path(args.project_root).resolve()
    run_id = f"{args.mode}_{args.suite}_{args.width}x{args.height}"
    archive = root / "Evidence/R6Final" / run_id
    require(not archive.exists(), f"Archive already exists; it will not be overwritten: {archive}")
    require(args.wrapper_exit_code == 0, "Wrapper process did not exit zero")
    log_path, wrapper_path = Path(args.log).resolve(), Path(args.wrapper_log).resolve()
    result = validate_log(log_path, args.suite)
    proof = native_exit_proof(wrapper_path, args.suite, args.mode, run_id, args.exit_proof, root)
    require(bool(args.started_utc) == bool(args.finished_utc), "Supply both UTC times or neither")
    started = epoch(args.started_utc or result["engine_started_utc"])
    finished = epoch(args.finished_utc or result["engine_finished_utc"])
    require(started <= finished, "Run end precedes run start")
    require(started - TOLERANCE <= epoch(result["engine_started_utc"]) <= epoch(result["engine_finished_utc"]) <= finished + TOLERANCE, "Engine log time window does not fit the supplied process window")
    fresh(log_path, started, finished, "Engine log")
    expected = SCREENS[args.suite]
    images = []
    if expected:
        require(args.capture_dir, f"{args.suite} requires --capture-dir")
        folder = Path(args.capture_dir).resolve()
        require(folder.is_dir(), f"Screenshot directory is absent: {folder}")
        actual_names = {path.name for path in folder.glob("*.png")}
        require(actual_names == set(expected), f"Wrong {args.suite} PNG set; missing={set(expected) - actual_names}, extra={actual_names - set(expected)}")
        for name in expected:
            path = folder / name
            fresh(path, started, finished, "Screenshot")
            images.append((path, validate_png(path, args.width, args.height)))
    manifest = input_manifest(root)
    require(all(epoch(row["mtime_utc"]) <= started + TOLERANCE for row in manifest["files"]), "A project input was modified after this run began")
    executables = executable_records(args, root)
    validate_log_executable(log_path, executables, args.mode)
    require(all(epoch(row["mtime_utc"]) <= started + TOLERANCE for row in executables), "An executable was modified after this run began")
    payloads = []
    package_build_log = None
    if args.mode == "Packaged":
        package_build_log = Path(args.package_build_log).resolve() if args.package_build_log else root / "Evidence/R6_Package.log"
        validate_package_build_log(package_build_log)
        inner = Path(next(row["path"] for row in executables if row["role"] == "packaged_inner_exe"))
        payload_folder = inner.parents[2] / "Content/Paks"
        payload_paths = sorted(path for path in payload_folder.rglob("*") if path.suffix.lower() in (".pak", ".utoc", ".ucas") and path.is_file())
        payloads = [file_record(path) for path in payload_paths]
        require(payloads, f"No cooked payloads under {payload_folder}")
        require(all(epoch(row["mtime_utc"]) <= started + TOLERANCE for row in payloads), "A cooked payload changed after this run began")
        require(max(epoch(row["mtime_utc"]) for row in payloads) <= package_build_log.stat().st_mtime + TOLERANCE <= started + 2 * TOLERANCE,
                "UAT evidence does not fall between cooked payload completion and the run")
    validate_input_build_times(manifest, executables, payloads)
    archive.mkdir(parents=True)
    shutil.copy2(log_path, archive / "run.log")
    shutil.copy2(wrapper_path, archive / "wrapper.log")
    archived_images = []
    if images:
        (archive / "screens").mkdir()
    for path, dimensions in images:
        destination = archive / "screens" / path.name
        shutil.copy2(path, destination)
        archived_images.append(dict(file_record(destination, archive), origin_path=str(path), **dimensions))
    if proof["kind"] == "checked_wrapper_success":
        source = Path(proof["guard_source"]["path"])
        shutil.copy2(source, archive / "checked_wrapper.ps1")
        proof["archived_guard_source"] = file_record(archive / "checked_wrapper.ps1", archive)
    if package_build_log:
        shutil.copy2(package_build_log, archive / "package_build.log")
    receipt = {
        "schema_version": SCHEMA, "run_id": run_id, "mode": args.mode, "suite": args.suite,
        "width": args.width, "height": args.height, "started_utc": utc(started), "finished_utc": utc(finished),
        "timing_origin": "wrapper_process_window" if args.started_utc else "engine_log",
        "wrapper_exit_code": args.wrapper_exit_code, "native_exit_code": proof["native_exit_code"],
        "source_of_native_exit": proof, "source_sha256": source_hash(manifest), "input_manifest": manifest,
        "executables": executables, "log": dict(file_record(archive / "run.log", archive), origin_path=str(log_path)),
        "package_payloads": payloads,
        "package_build_log": dict(file_record(archive / "package_build.log", archive), origin_path=str(package_build_log)) if package_build_log else None,
        "wrapper_log": dict(file_record(archive / "wrapper.log", archive), origin_path=str(wrapper_path)),
        "screenshots": archived_images, "result": result, "captured_utc": utc(),
        "limitations": "Automation exercises gameplay and Canvas calls. This receipt does not establish physical keyboard/mouse, listening, multiplayer, or FPS behavior.",
    }
    write_json(archive / "receipt.json", receipt)
    print(f"R6_CAPTURE_PASS: {run_id} checks={result['checks']} screenshots={len(images)} native_exit=0 wrapper_exit=0 receipt={archive / 'receipt.json'}")


def verify_record(record, base=None):
    path = under(base, record["path"]) if base else Path(record["path"]).resolve()
    current = file_record(path, base)
    require(current["bytes"] == record["bytes"] and current["sha256"] == record["sha256"], f"Recorded artifact changed: {path}")
    require(abs(epoch(current["mtime_utc"]) - epoch(record["mtime_utc"])) <= TOLERANCE, f"Recorded artifact timestamp changed: {path}")
    return path


def audit_run(folder, root, current_inputs):
    receipt = read_json(folder / "receipt.json")
    require(receipt.get("schema_version") == SCHEMA and receipt.get("run_id") == folder.name, f"Invalid receipt identity: {folder}")
    suite, mode = receipt["suite"], receipt["mode"]
    require(suite in SUITES and mode in ("Editor", "Packaged"), f"Invalid mode/suite: {folder}")
    require(folder.name == f"{mode}_{suite}_{receipt['width']}x{receipt['height']}", f"Receipt dimensions disagree with directory: {folder}")
    require(validate_manifest(receipt["input_manifest"]) == validate_manifest(current_inputs), f"Run input files differ from final Source/Content/Config/uproject: {folder}")
    require(receipt["source_sha256"] == source_hash(current_inputs), f"Run Source aggregate differs from final Source: {folder}")
    require(receipt.get("wrapper_exit_code") == receipt.get("native_exit_code") == 0, f"Nonzero/absent native or wrapper exit code: {folder}")
    log_path = verify_record(receipt["log"], folder)
    wrapper_path = verify_record(receipt["wrapper_log"], folder)
    result = validate_log(log_path, suite)
    require(result == receipt["result"], f"Stored result does not match engine log: {folder}")
    proof = receipt["source_of_native_exit"]
    if proof["kind"] == "native_exit_marker":
        native_exit_proof(wrapper_path, suite, mode, folder.name, "marker", root)
    else:
        require(proof["kind"] == "checked_wrapper_success" and suite in ("Framework", "Container", "Flow"), "Unsupported native exit proof")
        checked_proof = native_exit_proof(wrapper_path, suite, mode, folder.name, "checked-wrapper", root)
        require(checked_proof["matched_line"] == proof["matched_line"], "Archived wrapper no longer contains its claimed exit proof")
        guard_path = verify_record(proof["archived_guard_source"], folder)
        require(re.search(r"\.ExitCode\s+-ne\s+0", read_text(guard_path)), "Archived checked wrapper lacks native ExitCode guard")
        require(sha(guard_path) == proof["guard_source"]["sha256"], "Checked-wrapper guard snapshot differs from receipt")
    started, finished = epoch(receipt["started_utc"]), epoch(receipt["finished_utc"])
    require(started <= finished, "Reversed receipt time window")
    require(started - TOLERANCE <= epoch(result["engine_started_utc"]) <= epoch(result["engine_finished_utc"]) <= finished + TOLERANCE, "Archived log is outside run window")
    fresh(log_path, started, finished, "Archived log")
    require(len(receipt["screenshots"]) == len(SCREENS[suite]), f"Incorrect receipt screenshot count: {folder}")
    names = [Path(row["path"]).name for row in receipt["screenshots"]]
    require(set(names) == set(SCREENS[suite]) and len(set(names)) == len(names), f"Incorrect screenshot names: {folder}")
    actual_images = {path.relative_to(folder).as_posix() for path in folder.rglob("*.png")}
    require(actual_images == {row["path"] for row in receipt["screenshots"]}, f"Unreceipted/missing PNGs in {folder}")
    for record in receipt["screenshots"]:
        path = verify_record(record, folder)
        require((record["width"], record["height"]) == (receipt["width"], receipt["height"]), "Receipt screenshot dimensions disagree")
        validate_png(path, receipt["width"], receipt["height"])
        fresh(path, started, finished, "Archived screenshot")
    roles = {row["role"] for row in receipt["executables"]}
    require(len(roles) == len(receipt["executables"]), "Duplicate executable roles")
    require((roles == {"editor_exe", "project_dll"}) if mode == "Editor" else
            ("packaged_inner_exe" in roles and roles <= {"packaged_inner_exe", "packaged_launcher_exe"}),
            "Actual runtime executable/DLL identity is absent or has unexpected roles")
    for executable in receipt["executables"]:
        verify_record(executable)
        require(epoch(executable["mtime_utc"]) <= started + TOLERANCE, "Executable is newer than the archived run")
    validate_log_executable(log_path, receipt["executables"], mode)
    require(all(epoch(row["mtime_utc"]) <= started + TOLERANCE for row in receipt["input_manifest"]["files"]), "Recorded input is newer than its run")
    payloads = receipt.get("package_payloads", [])
    validate_input_build_times(receipt["input_manifest"], receipt["executables"], payloads)
    if mode == "Packaged":
        inner = Path(next(row["path"] for row in receipt["executables"] if row["role"] == "packaged_inner_exe"))
        actual_payloads = {str(path.resolve()) for path in (inner.parents[2] / "Content/Paks").rglob("*")
                           if path.is_file() and path.suffix.lower() in (".pak", ".utoc", ".ucas")}
        require(actual_payloads == {str(Path(row["path"]).resolve()) for row in payloads},
                "Packaged payload set changed after the recorded run")
        for payload in payloads:
            verify_record(payload)
            require(epoch(payload["mtime_utc"]) <= started + TOLERANCE, "Cooked payload is newer than its run")
        build_log = verify_record(receipt["package_build_log"], folder)
        validate_package_build_log(build_log)
        require(max(epoch(row["mtime_utc"]) for row in payloads) <= build_log.stat().st_mtime + TOLERANCE <= started + 2 * TOLERANCE,
                "Archived UAT evidence does not cover cooked payload completion before the run")
    return {"run_id": folder.name, "receipt": file_record(folder / "receipt.json", root),
            "mode": mode, "suite": suite, "width": receipt["width"], "height": receipt["height"],
            "checks": result["checks"], "screenshots": len(names), "native_exit_code": 0, "wrapper_exit_code": 0,
            "source_of_native_exit": proof, "executables": receipt["executables"],
            "package_payloads": payloads, "package_build_log": receipt.get("package_build_log"),
            "log": receipt["log"], "images": receipt["screenshots"], "started_utc": receipt["started_utc"], "finished_utc": receipt["finished_utc"]}


def audit_benchmark(folder, label):
    manifest_path = folder / "FrameworkBenchmarkManifest.json"
    manifest = read_json(manifest_path)
    require(manifest.get("status") == "PASS", f"Benchmark runner manifest is not PASS: {manifest_path}")
    runs = manifest.get("runs", [])
    require(len(runs) == 3, f"Expected three independent benchmark processes: {folder}")
    require(len({run["run_id"] for run in runs}) == 3, f"Duplicate benchmark run IDs: {folder}")
    output = {"label": label, "directory": str(folder), "manifest": file_record(manifest_path), "runs": []}
    for index, run in enumerate(runs, 1):
        require(run.get("status") == "PASS" and run.get("exit_code") == 0 and not run.get("timed_out"), f"Failed benchmark process: {folder}/run{index:02d}")
        sub = folder / f"run{index:02d}"
        metadata_path = sub / "FrameworkBenchmarkMetadata.json"
        metadata = read_json(metadata_path)
        require(metadata["run_id"] == run["run_id"], "Benchmark metadata identity mismatch")
        require(metadata["with_editor"] == label.endswith("editor"), "Benchmark target mode mismatch")
        require((metadata["viewport_width"], metadata["viewport_height"]) == (1280, 720), "Benchmark viewport differs from controlled baseline")
        require(not metadata["stress_enabled"] and metadata["batches_per_scenario"] == 48, "Expected the normal-scale 48-batch benchmark")
        log_path = sub / "FrameworkBenchmark.log"
        text = read_text(log_path)
        require(not FAILURE.search(text), f"Benchmark failure marker: {log_path}")
        markers = list(re.finditer(r"RAID_FRAMEWORK_BENCHMARK_RESULT:\s*PASS\s+scenarios=(\d+)\s+samples=(\d+)", text))
        require(len(markers) == 1 and tuple(map(int, markers[0].groups())) == (7, 336), "Incomplete benchmark result")
        require("LogExit: Exiting." in text[markers[0].end():], "Benchmark lacks normal UE exit")
        raw_path, summary_path = sub / "FrameworkBenchmark.csv", sub / "FrameworkBenchmarkSummary.csv"
        with raw_path.open(encoding="utf-8-sig", newline="") as stream:
            raw = list(csv.DictReader(stream))
        with summary_path.open(encoding="utf-8-sig", newline="") as stream:
            summary = list(csv.DictReader(stream))
        require(len(raw) == 336 and len(summary) == 7, "Benchmark CSV sample counts disagree")
        require({row["scenario"] for row in summary} == EXPECTED_SCENARIOS, "Benchmark scenario set changed")
        for name in EXPECTED_SCENARIOS:
            samples = [row for row in raw if row["scenario"] == name]
            require(len(samples) == 48 and {int(row["batch_index"]) for row in samples} == set(range(48)), "Benchmark batch sequence is incomplete")
        require(all(row["run_id"] == run["run_id"] for row in raw + summary), "Benchmark CSV run IDs disagree")
        require(all(int(row["iterations"]) > 0 and math.isfinite(float(row["ns_per_iteration"])) and float(row["ns_per_iteration"]) >= 0 for row in raw), "Invalid benchmark samples")
        historical = {Path(record["path"]).name: record for record in run.get("outputs", [])}
        expected_files = (metadata_path, log_path, raw_path, summary_path)
        require(set(historical) == {path.name for path in expected_files}, "Historical benchmark output manifest is incomplete")
        artifact_records = []
        for path in expected_files:
            current = file_record(path)
            saved = historical[path.name]
            require(current["bytes"] == saved["bytes"] and current["sha256"].casefold() == saved["sha256"].casefold(),
                    f"Benchmark artifact differs from its historical runner hash: {path}")
            artifact_records.append(current)
        output["runs"].append({"run_id": run["run_id"], "native_exit_code": run["exit_code"], "metadata": metadata,
                               "files": artifact_records})
    output["interpretation"] = "Query-scope elapsed microbenchmark only; warm-cache dispatch/checksum overhead remains. CountMedkits predicate/cache semantics change in R6 and refresh work is outside timed batches. No FPS assertion or pass/fail speed threshold."
    return output


def audit(args):
    root = Path(args.project_root).resolve()
    output_path = root / "Evidence/R6_DeliveryManifest.json"
    delivery = {"schema_version": SCHEMA, "status": "FAIL", "audited_utc": utc(), "project_root": str(root),
                "auditor": file_record(Path(__file__).resolve()), "runs": [], "benchmarks": [], "errors": []}
    try:
        inputs = input_manifest(root)
        delivery["input_manifest"] = inputs
        delivery["source_sha256"] = source_hash(inputs)
        expected = set()
        for mode in ("Editor", "Packaged"):
            for suite in SUITES:
                width, height = (1280, 720) if suite == "Self" else (args.primary_width, args.primary_height)
                expected.add(f"{mode}_{suite}_{width}x{height}")
        expected.update(("Packaged_Container_1280x720", "Packaged_Flow_1280x720"))
        require(len(expected) == 12, "Primary resolution must differ from the additional 1280x720 coverage")
        archive_root = root / "Evidence/R6Final"
        actual = {path.parent.name for path in archive_root.glob("*/receipt.json")}
        require(actual == expected, f"Final receipt set mismatch; missing={expected - actual}, extra={actual - expected}")
        for identity in sorted(expected):
            try:
                delivery["runs"].append(audit_run(archive_root / identity, root, inputs))
            except (ValueError, OSError, KeyError, TypeError, zlib.error) as error:
                delivery["errors"].append(f"{identity}: {error}")
        require(not delivery["errors"], "One or more final run receipts failed")
        total_images = sum(run["screenshots"] for run in delivery["runs"])
        require(total_images == 115, f"Final PNG count is {total_images}, expected 115")
        delivery["screenshots_total"] = total_images
        for role in ("editor_exe", "project_dll", "packaged_inner_exe"):
            files = [exe for run in delivery["runs"] for exe in run["executables"] if exe["role"] == role]
            require(files and len({item["sha256"] for item in files}) == 1, f"Final runs used different {role} binaries")
        packaged_runs = [run for run in delivery["runs"] if run["mode"] == "Packaged"]
        payload_sets = {tuple(sorted((row["path"], row["bytes"], row["sha256"]) for row in run["package_payloads"])) for run in packaged_runs}
        require(len(payload_sets) == 1, "Final packaged runs used different cooked payloads")
        require(len({run["package_build_log"]["sha256"] for run in packaged_runs}) == 1, "Final packaged runs refer to different UAT/cook evidence")
        delivery["package_input_mtime_guard"] = {
            "status": "PASS", "source_guard": "Source <= compiled game binary", "data_guard": "Content/Config/uproject <= latest cooked payload <= successful UAT log <= run",
            "qualification": "A data/config-only recook may correctly reuse the executable. Hash/receipt agreement, payload hashes, UAT success and timestamp ordering are checked; timestamps alone are not cryptographic build provenance.",
        }
        for label in ("baseline_editor", "baseline_packaged", "post_editor", "post_packaged"):
            directory = getattr(args, label)
            require(directory, f"Complete delivery requires --{label.replace('_', '-')} for all 12 benchmark processes")
            delivery["benchmarks"].append(audit_benchmark(Path(directory).resolve(), label))
        all_benchmark_ids = [run["run_id"] for group in delivery["benchmarks"] for run in group["runs"]]
        require(len(set(all_benchmark_ids)) == 12, "Baseline/post benchmark processes were reused")
        delivery["benchmark_processes_total"] = 12
        delivery["status"] = "PASS"
        delivery["limitations"] = [
            "Automated functional/Canvas and screenshot evidence; physical keyboard/mouse and audio listening are not established.",
            "PNG structure, CRC, zlib completeness and dimensions are checked; visual composition and glyph quality require human image review.",
            "Performance artifacts are retained without FPS or whole-frame speed assertions; cache-refresh costs are outside query batches.",
        ]
    except (ValueError, OSError, KeyError, TypeError, zlib.error) as error:
        delivery["errors"].append(str(error))
    write_json(output_path, delivery)
    if delivery["status"] != "PASS":
        print("R6_DELIVERY_AUDIT: FAIL\n" + "\n".join(delivery["errors"]))
        return 1
    print(f"R6_DELIVERY_AUDIT: PASS runs=12 screenshots=115 benchmark_processes=12 source_sha256={delivery['source_sha256']} manifest={output_path}")
    return 0


def parser():
    command = argparse.ArgumentParser(description=__doc__)
    modes = command.add_subparsers(dest="command", required=True)
    project_default = str(Path(__file__).resolve().parents[1])
    cap = modes.add_parser("capture", help="Validate and archive one completed run; refuse overwriting an existing archive")
    cap.add_argument("--project-root", default=project_default)
    cap.add_argument("--mode", choices=("Editor", "Packaged"), required=True)
    cap.add_argument("--suite", choices=SUITES, required=True)
    cap.add_argument("--width", type=int, required=True)
    cap.add_argument("--height", type=int, required=True)
    cap.add_argument("--capture-dir")
    cap.add_argument("--log", required=True)
    cap.add_argument("--wrapper-log", required=True)
    cap.add_argument("--wrapper-exit-code", type=int, required=True)
    cap.add_argument("--exit-proof", choices=("marker", "checked-wrapper"), default="marker")
    cap.add_argument("--started-utc")
    cap.add_argument("--finished-utc")
    cap.add_argument("--editor-exe", default=r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe")
    cap.add_argument("--project-dll")
    cap.add_argument("--package-exe")
    cap.add_argument("--package-build-log", "--cook-log", dest="package_build_log")
    check = modes.add_parser("audit", help="Read all final evidence; write only the final delivery manifest")
    check.add_argument("--project-root", default=project_default)
    check.add_argument("--primary-width", type=int, default=1600)
    check.add_argument("--primary-height", type=int, default=900)
    for name in ("baseline-editor", "baseline-packaged", "post-editor", "post-packaged"):
        check.add_argument("--" + name)
    return command


def main():
    args = parser().parse_args()
    try:
        if args.command == "capture":
            require(1 <= args.width <= 16384 and 1 <= args.height <= 16384, "Invalid requested screenshot dimensions")
            capture(args)
            return 0
        return audit(args)
    except (ValueError, OSError, KeyError, TypeError, zlib.error) as error:
        print(f"R6_{args.command.upper()}_FAIL: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
