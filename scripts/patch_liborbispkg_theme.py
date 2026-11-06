#!/usr/bin/env python3
"""Patches LibOrbisPkg (OpenOrbis, MIT) for system theme packages. Two changes:
1. PkgBuilder.cs: theme volumes get the theme IRO tag in the PKG header
   (upstream always writes IROTag.None). According to PS4 Ultimate Theme
   Creator (pkg.py) the console recognises a system theme by the
   pkg_ps4_theme volume together with IRO_TAG = 2 (SS_THEME, PKG/Enums.cs).
2. Gp4Validator.cs: accept pkg_ps4_theme (upstream only allows app and the
   two additional-content types, although the builder handles themes as
   additional content).
usage: patch_liborbispkg_theme.py <LibOrbisPkg checkout>"""
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
p = root / "LibOrbisPkg" / "PKG" / "PkgBuilder.cs"
src = p.read_text(encoding="utf-8")
old = "        iro_tag = IROTag.None,"
new = ("        iro_tag = volType == GP4.VolumeType.pkg_ps4_theme ? IROTag.SS_THEME\n"
       "                : volType == GP4.VolumeType.pkg_ps4_sf_theme ? IROTag.SF_THEME : IROTag.None,")
if src.count(old) != 1:
    raise SystemExit(f"{p}: expected exactly one '{old.strip()}' (upstream changed?)")
p.write_text(src.replace(old, new), encoding="utf-8")
print("patched", p)

v = root / "LibOrbisPkg" / "GP4" / "Gp4Validator.cs"
src = v.read_text(encoding="utf-8")
old = "        case VolumeType.pkg_ps4_ac_nodata:\n          break;\n        default:"
new = ("        case VolumeType.pkg_ps4_ac_nodata:\n          break;\n"
       "        case VolumeType.pkg_ps4_theme:\n          break;\n        default:")
if src.replace("\r\n", "\n").count(old) != 1:
    raise SystemExit(f"{v}: volume type switch not found (upstream changed?)")
crlf = "\r\n" in src
src = src.replace("\r\n", "\n").replace(old, new)
v.write_text(src.replace("\n", "\r\n") if crlf else src, encoding="utf-8")
print("patched", v)
