#!/usr/bin/env python3
"""PS4 HEN ULTIMATE - completes the <rootdir> of a GP4 made by create-gp4.

create-gp4 (OpenOrbis v0.5.4) writes a fixed directory tree (sce_sys/about,
sce_module, assets/{audio,fonts,images,misc,videos}). PkgTool.Core then
fails with "Sequence contains no elements" for files in any other folder.
This adds a <dir> entry for every folder used by a <file targ_path>.

Usage: python3 scripts/fix_gp4.py pkg.gp4
"""
import sys
import xml.etree.ElementTree as ET


def main(path):
    tree = ET.parse(path)
    root = tree.getroot()
    rootdir = root.find("rootdir")
    files = root.find("files")
    if rootdir is None or files is None:
        print("fix_gp4: <rootdir> or <files> missing", file=sys.stderr)
        return 1
    added = []
    for f in files.findall("file"):
        parts = f.get("targ_path", "").split("/")[:-1]
        node = rootdir
        for i, name in enumerate(parts):
            child = next((d for d in node.findall("dir") if d.get("targ_name") == name), None)
            if child is None:
                child = ET.SubElement(node, "dir", {"targ_name": name})
                added.append("/".join(parts[: i + 1]))
            node = child
    tree.write(path, encoding="utf-8", xml_declaration=True)
    print("fix_gp4: added dirs:", ", ".join(added) if added else "(none)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]) if len(sys.argv) == 2 else 2)
