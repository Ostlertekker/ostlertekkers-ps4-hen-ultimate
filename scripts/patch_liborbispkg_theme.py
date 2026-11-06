#!/usr/bin/env python3
"""Patches LibOrbisPkg (OpenOrbis, MIT) so that theme volumes get the theme
IRO tag in the PKG header. Upstream always writes IROTag.None
(LibOrbisPkg/PKG/PkgBuilder.cs, BuildPkg). According to PS4 Ultimate Theme
Creator (pkg.py) the console recognises a system theme by the pkg_ps4_theme
volume together with IRO_TAG = 2 (SS_THEME, LibOrbisPkg/PKG/Enums.cs).
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
