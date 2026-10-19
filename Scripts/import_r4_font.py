"""Import the pinned OFL Noto font for R4; no installation or native UI.

Standalone source/coverage check (uses only Python's standard library):
    python import_r4_font.py --verify-source
    python import_r4_font.py --verify-baked
UE hidden/offscreen editor (coordinate with other UE/build tasks first):
    UnrealEditor.exe GreyzoneRaid.uproject \
        -ExecutePythonScript=Scripts/import_r4_font.py -unattended -nop4 \
        -nosound -NoSplash -RenderOffscreen
UE5.8's FontFace PostEditChangeProperty needs initialized Slate; the pure
Python commandlet does not initialize it and is unsuitable for font import.

Only /Game/UI/Fonts/F_RaidChinese*, FF_RaidChinese, and their new assets are
created. Existing R1-R3 assets, map files, and gameplay source are not changed.
"""
from contextlib import contextmanager
from pathlib import Path
import ctypes
import hashlib
import json
import os
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
FONT_DIR = ROOT / "Downloads/Fonts"
FONT_FILE = FONT_DIR / "NotoSansCJKsc-Regular.otf"
LICENSE_FILE = FONT_DIR / "LICENSE.NotoSansCJK.txt"
COMMIT = "f8d157532fbfaeda587e826d4cd5b21a49186f7c"
FONT_SHA256 = "2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b"
LICENSE_SHA256 = "6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2"
PACKAGE = "/Game/UI/Fonts"
RUNTIME_PATH = PACKAGE + "/F_RaidChinese"
FACE_PATH = PACKAGE + "/FF_RaidChinese"
WORLD_PATH = PACKAGE + "/F_RaidChineseWorld"
FAMILY = "Noto Sans CJK SC"
# Include root's initial world labels before all localized source is ready.
# Source/*.cpp, *.h, Scripts/*r4*.py/json and R4_world_text.txt are also read.
WORLD_TEXT = """医疗物资 5.56 弹药 口粮 回收物资 稀有 精良 普通
搜索中 %02d%% 按住 E 搜索 撤离点 / E-07 待命 区内停留 %02d秒
撤离完成 物资已保全 信号中断 行动失败 需要物资 搜索仓储区
撤离倒计时 %04.1f 秒 回收 信号装置 医疗包 弹药补给 电子零件
加密资料 加密电台 起始点 安全区 仓库 医疗站 装卸区 货运站 台阶
入口 出口 危险 禁止通行 配电室 发电机 边界 狙击点 集装箱
"""


def digest(data):
    return hashlib.sha256(data).hexdigest()


def sfnt_tables(data):
    assert data[:4] in (b"OTTO", b"\x00\x01\x00\x00"), "Not an OpenType font"
    result = {}
    for index in range(struct.unpack_from(">H", data, 4)[0]):
        tag, _, offset, length = struct.unpack_from(">4sIII", data, 12 + 16 * index)
        assert offset + length <= len(data), "Truncated font table"
        result[tag.decode("ascii")] = (offset, length)
    return result


def font_names(data):
    base, _ = sfnt_tables(data)["name"]
    _, count, strings = struct.unpack_from(">HHH", data, base)
    result = {}
    for index in range(count):
        platform, _, language, name_id, size, offset = struct.unpack_from(">HHHHHH", data, base + 6 + 12 * index)
        if language != 1033 and platform != 0:
            continue
        value = data[base + strings + offset:base + strings + offset + size]
        value = value.decode("utf-16-be" if platform in (0, 3) else "latin-1")
        result.setdefault(str(name_id), value)
    return result


def cmap_codepoints(data):
    """Read Unicode cmap format 4/12 directly; reject glyph zero (missing)."""
    base, _ = sfnt_tables(data)["cmap"]
    count = struct.unpack_from(">H", data, base + 2)[0]
    result = set()
    for index in range(count):
        platform, encoding, offset = struct.unpack_from(">HHI", data, base + 4 + 8 * index)
        if not (platform == 0 or (platform == 3 and encoding in (1, 10))):
            continue
        table = base + offset
        fmt = struct.unpack_from(">H", data, table)[0]
        if fmt == 12:
            groups = struct.unpack_from(">I", data, table + 12)[0]
            for group in range(groups):
                first, last, first_glyph = struct.unpack_from(">III", data, table + 16 + 12 * group)
                result.update(range(first + (first_glyph == 0), last + 1))
        elif fmt == 4:
            segments = struct.unpack_from(">H", data, table + 6)[0] // 2
            end_base = table + 14
            start_base = end_base + 2 * segments + 2
            delta_base = start_base + 2 * segments
            range_base = delta_base + 2 * segments
            for segment in range(segments):
                first = struct.unpack_from(">H", data, start_base + 2 * segment)[0]
                last = struct.unpack_from(">H", data, end_base + 2 * segment)[0]
                delta = struct.unpack_from(">h", data, delta_base + 2 * segment)[0]
                relative = struct.unpack_from(">H", data, range_base + 2 * segment)[0]
                for codepoint in range(first, min(last, 65534) + 1):
                    if relative:
                        glyph = struct.unpack_from(">H", data, range_base + 2 * segment + relative + 2 * (codepoint - first))[0]
                        glyph = (glyph + delta) & 65535 if glyph else 0
                    else:
                        glyph = (codepoint + delta) & 65535
                    if glyph:
                        result.add(codepoint)
    assert result, "No Unicode cmap format 4/12 found"
    return result


def requested_characters():
    texts = [WORLD_TEXT]
    inputs = []
    paths = list((ROOT / "Source").rglob("*.cpp")) + list((ROOT / "Source").rglob("*.h"))
    paths += list((ROOT / "Scripts").glob("*r4*.py"))
    paths += list((ROOT / "Scripts").glob("*r4*.json"))
    extra = FONT_DIR / "R4_world_text.txt"
    if extra.exists():
        paths.append(extra)
    for path in sorted(set(paths)):
        text = path.read_text(encoding="utf-8-sig")
        texts.append(text)
        inputs.append({"path": path.relative_to(ROOT).as_posix(), "sha256": digest(path.read_bytes())})
    # All printable ASCII must be explicitly present in Chars: UE's factory
    # skips ASCII absent from Chars even when include_ascii_range is True.
    chars = set(chr(value) for value in range(32, 127))
    chars.update(char for text in texts for char in text if ord(char) >= 128 and char.isprintable())
    assert all(ord(char) <= 65535 for char in chars), "Offline UFont only supports BMP characters; use runtime font for supplementary characters"
    return "".join(sorted(chars)), inputs


def verify_source():
    data = FONT_FILE.read_bytes()
    license_data = LICENSE_FILE.read_bytes()
    assert digest(data) == FONT_SHA256 and len(data) == 16437364, "Pinned font checksum or size changed"
    assert digest(license_data) == LICENSE_SHA256, "Pinned complete license checksum changed"
    assert b"SIL OPEN FONT LICENSE Version 1.1" in license_data
    names = font_names(data)
    assert names["1"] == FAMILY and names["2"] == "Regular"
    chars, inputs = requested_characters()
    coverage = cmap_codepoints(data)
    missing = ["U+%04X %s" % (ord(char), char) for char in chars if ord(char) not in coverage]
    assert not missing, "Font lacks requested characters: " + repr(missing)
    report = {
        "official_repository": "https://github.com/notofonts/noto-cjk",
        "pinned_commit": COMMIT,
        "license": "SIL Open Font License 1.1",
        "font": {"file": FONT_FILE.relative_to(ROOT).as_posix(), "bytes": len(data), "sha256": digest(data), "url": "https://raw.githubusercontent.com/notofonts/noto-cjk/" + COMMIT + "/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf"},
        "license_file": {"file": LICENSE_FILE.relative_to(ROOT).as_posix(), "bytes": len(license_data), "sha256": digest(license_data), "url": "https://raw.githubusercontent.com/notofonts/noto-cjk/" + COMMIT + "/Sans/LICENSE"},
        "name_table": names,
        "unicode_cmap_codepoints": len(coverage),
        "world_requested_characters": len(chars),
        "world_characters": chars,
        "world_missing_characters": missing,
        "coverage_inputs": inputs,
    }
    (FONT_DIR / "COPYRIGHT.NotoSansCJK.txt").write_text(names["0"] + "\n\n" + names["13"] + "\n" + names["14"] + "\n", encoding="utf-8")
    (FONT_DIR / "SOURCES.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report, chars


@contextmanager
def private_windows_font():
    """Process-private registration, always removed; no fonts-folder/registry writes."""
    assert os.name == "nt", "Offline subset import currently verified only on Windows"
    from ctypes import wintypes
    gdi = ctypes.WinDLL("gdi32", use_last_error=True)
    gdi.AddFontResourceExW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, ctypes.c_void_p]
    gdi.AddFontResourceExW.restype = ctypes.c_int
    gdi.RemoveFontResourceExW.argtypes = gdi.AddFontResourceExW.argtypes
    gdi.RemoveFontResourceExW.restype = wintypes.BOOL
    assert gdi.AddFontResourceExW(str(FONT_FILE), 0x10, None) > 0, "Private font loading failed"
    try:
        # Detect GDI family substitution before UE's factory could silently bake
        # a different system font or replace unsupported glyphs with spaces.
        gdi.CreateCompatibleDC.argtypes = [wintypes.HDC]
        gdi.CreateCompatibleDC.restype = wintypes.HDC
        gdi.CreateFontW.argtypes = [ctypes.c_int] * 5 + [wintypes.DWORD] * 8 + [wintypes.LPCWSTR]
        gdi.CreateFontW.restype = wintypes.HANDLE
        gdi.SelectObject.argtypes = [wintypes.HDC, wintypes.HANDLE]
        gdi.SelectObject.restype = wintypes.HANDLE
        gdi.GetTextFaceW.argtypes = [wintypes.HDC, ctypes.c_int, wintypes.LPWSTR]
        gdi.GetFontData.argtypes = [wintypes.HDC, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD]
        gdi.GetFontData.restype = wintypes.DWORD
        gdi.DeleteObject.argtypes = [wintypes.HANDLE]
        gdi.DeleteDC.argtypes = [wintypes.HDC]
        dc = gdi.CreateCompatibleDC(None)
        assert dc, "Unable to create font memory DC"
        font = gdi.CreateFontW(-64, 0, 0, 0, 400, 0, 0, 0, 1, 0, 0, 4, 2, FAMILY)
        assert font, "Unable to select private font"
        old = gdi.SelectObject(dc, font)
        try:
            face_name = ctypes.create_unicode_buffer(128)
            gdi.GetTextFaceW(dc, len(face_name), face_name)
            assert face_name.value == FAMILY, "GDI substituted font: " + face_name.value
            size = gdi.GetFontData(dc, 0, 0, None, 0)
            assert size not in (0, 0xFFFFFFFF), "GDI cannot read private font data"
            data = ctypes.create_string_buffer(size)
            assert gdi.GetFontData(dc, 0, 0, data, size) == size
            assert digest(data.raw) == FONT_SHA256, "GDI selected bytes differ from the pinned OTF"
        finally:
            gdi.SelectObject(dc, old)
            gdi.DeleteObject(font)
            gdi.DeleteDC(dc)
        yield {"registration": "FR_PRIVATE (process only)", "family": face_name.value, "selected_font_sha256": FONT_SHA256}
    finally:
        assert gdi.RemoveFontResourceExW(str(FONT_FILE), 0x10, None), "Private font cleanup failed"


def import_assets(report, chars):
    import unreal
    assets = unreal.EditorAssetLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    assets.make_directory(PACKAGE)
    face = unreal.load_asset(FACE_PATH) if assets.does_asset_exist(FACE_PATH) else None
    if face is None:
        factory = unreal.FontFileImportFactory()
        factory.set_editor_property("batch_create_font_asset", unreal.BatchCreateFontAsset.NO)
        task = unreal.AssetImportTask()
        task.set_editor_properties({"filename": str(FONT_FILE), "destination_path": PACKAGE, "destination_name": "FF_RaidChinese", "automated": True, "replace_existing": False, "save": False, "factory": factory})
        tools.import_asset_tasks([task])
        face = unreal.load_asset(FACE_PATH)
    assert face and isinstance(face, unreal.FontFace), "FontFace import failed"
    face.set_editor_property("loading_policy", unreal.FontLoadingPolicy.INLINE)
    face.set_editor_property("hinting", unreal.FontHinting.DEFAULT)
    face.set_editor_property("layout_method", unreal.FontLayoutMethod.METRICS)
    assert assets.save_loaded_asset(face, only_if_is_dirty=False)
    font = unreal.load_asset(RUNTIME_PATH) if assets.does_asset_exist(RUNTIME_PATH) else None
    if font is None:
        font = tools.create_asset("F_RaidChinese", PACKAGE, unreal.Font, unreal.FontFactory())
    assert font
    font.set_editor_property("font_cache_type", unreal.FontCacheType.RUNTIME)
    font.set_editor_property("legacy_font_size", 16)
    font.set_editor_property("legacy_font_name", "Regular")
    # CompositeFont's nested UPROPERTY members intentionally lack Python editor
    # setters. UE's supported StructBase.import_text writes the actual reflected
    # FFontData.FontFaceAsset reference; no external filename dependency.
    composite = font.get_editor_property("composite_font")
    data_text = '(DefaultTypeface=(Fonts=((Name="Regular",Font=(FontFaceAsset="/Script/Engine.FontFace\'%s.FF_RaidChinese\'")))))' % FACE_PATH
    assert composite.import_text(data_text), "CompositeFont import_text failed"
    font.set_editor_property("composite_font", composite)
    exported = font.get_editor_property("composite_font").export_text()
    assert "FF_RaidChinese" in exported and "Regular" in exported, exported
    assert assets.save_loaded_asset(font, only_if_is_dirty=False)

    # Regenerate only the R4 world subset when the requested character set changes.
    world = unreal.load_asset(WORLD_PATH) if assets.does_asset_exist(WORLD_PATH) else None
    needs_world_import = world is None
    if world:
        prior_chars = world.get_editor_property("import_options").get_editor_property("chars")
        needs_world_import = not set(chars).issubset(set(prior_chars))
        chars = "".join(sorted(set(chars) | set(prior_chars)))
    private_report = None
    if needs_world_import:
        with private_windows_font() as private_report:
            factory = unreal.TrueTypeFontFactory()
            options = factory.get_editor_property("import_options")
            data = options.get_editor_property("data")
            data.set_editor_properties({"font_name": FAMILY, "height": 48.0, "chars": chars, "include_ascii_range": True, "create_printable_only": True, "texture_page_width": 2048, "texture_page_max_height": 2048, "x_padding": 4, "y_padding": 4, "use_distance_field_alpha": True, "distance_field_scale_factor": 2, "distance_field_scan_radius_scale": 0.5, "enable_legacy_mode": False})
            options.set_editor_property("data", data)
            world = tools.create_asset("F_RaidChineseWorld", PACKAGE, unreal.Font, factory, overwrite_existing=True)
            assert world, "World offline font creation failed"
    assert world.get_editor_property("font_cache_type") == unreal.FontCacheType.OFFLINE
    characters = world.get_editor_property("characters")
    used_texture_indices = sorted({c.get_editor_property("texture_index") for c in characters if c.get_editor_property("u_size") > 0})
    assert used_texture_indices == [0], "World font needs more than one atlas: " + repr(used_texture_indices)
    assert assets.save_loaded_asset(world, only_if_is_dirty=False)
    report["unreal"] = {
        "engine": unreal.SystemLibrary.get_engine_version(),
        "runtime_font": font.get_path_name(), "font_face": face.get_path_name(),
        "runtime_cache_type": str(font.get_editor_property("font_cache_type")),
        "face_loading_policy": str(face.get_editor_property("loading_policy")),
        "composite_font": exported, "typeface_name": "Regular",
        "world_font": world.get_path_name(), "world_cache_type": str(world.get_editor_property("font_cache_type")),
        "world_import_options": world.get_editor_property("import_options").export_text(),
        "world_texture_indices": used_texture_indices,
        "baked_world_characters": chars,
        "private_font": private_report,
    }
    (ROOT / "Evidence/R4_FontImport.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("R4_FONT_IMPORT_OK " + json.dumps(report["unreal"], ensure_ascii=True))


def verify_baked(report, chars):
    """Compare saved import evidence and on-disk payload with current sources."""
    imported = json.loads((ROOT / "Evidence/R4_FontImport.json").read_text(encoding="utf-8"))
    baked_chars = imported["unreal"].get("baked_world_characters", imported["world_characters"])
    missing = sorted(set(chars) - set(baked_chars))
    assert not missing, "World font must be regenerated for these new characters: " + repr(missing)
    records = []
    for name in ("F_RaidChinese", "FF_RaidChinese", "F_RaidChineseWorld"):
        asset = ROOT / "Content/UI/Fonts" / (name + ".uasset")
        data = asset.read_bytes()
        record = {"file": asset.relative_to(ROOT).as_posix(), "bytes": len(data), "sha256": digest(data)}
        if name == "FF_RaidChinese":
            record["complete_original_otf_payload_offset"] = data.find(FONT_FILE.read_bytes())
            assert record["complete_original_otf_payload_offset"] >= 0, "FontFace does not contain the complete original OTF bytes"
        records.append(record)
    result = {"status": "R4_FONT_BAKED_OK", "font_cmap_missing": [], "world_subset_missing": missing, "current_requested_characters": len(chars), "baked_world_characters": len(baked_chars), "assets": records}
    (ROOT / "Evidence/R4_FontValidation.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    source_report, world_chars = verify_source()
    if "--verify-source" in sys.argv:
        print(json.dumps({"status": "R4_FONT_SOURCE_OK", "bytes": source_report["font"]["bytes"], "sha256": FONT_SHA256, "unicode_cmap_codepoints": source_report["unicode_cmap_codepoints"], "requested": len(world_chars), "missing": []}))
    elif "--verify-private-font" in sys.argv:
        with private_windows_font() as result:
            print(json.dumps(result))
    elif "--verify-baked" in sys.argv:
        verify_baked(source_report, world_chars)
    else:
        import_assets(source_report, world_chars)
