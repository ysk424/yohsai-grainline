"""Report the production PDF mesh density without running Kitsuke."""

from __future__ import annotations

import os
import sys
import time
from pathlib import Path

import bpy


installed_check = os.environ.get("YOHSAI_INSTALLED_CHECK") == "1"
if installed_check:
    from bl_ext.user_default import yohsai  # noqa: E402
    from bl_ext.user_default.yohsai import mesh_loader, ui  # noqa: E402
else:
    repo = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(repo / "tests"))
    from source_package import load_source_package  # noqa: E402

    yohsai = load_source_package(repo)
    mesh_loader = sys.modules[f"{yohsai.__name__}.mesh_loader"]
    ui = sys.modules[f"{yohsai.__name__}.ui"]

source = Path.home() / "Desktop" / "test2.pdf"
if not source.is_file():
    raise RuntimeError("Missing integration input: Desktop/test2.pdf")

if not installed_check:
    yohsai.register()
try:
    started = time.perf_counter()
    bpy.context.scene.yohsai.svg_path = str(source)
    assert bpy.ops.yohsai.load_svg() == {"FINISHED"}
    assert ui._parse_process is not None
    ui._parse_process.wait(timeout=30)
    ui._poll_svg_parser()

    collection = bpy.context.scene.yohsai.clothes_collection
    assert collection is not None
    parts = [
        obj
        for obj in collection.objects
        if obj.type == "MESH" and obj.get("yohsai_role") == "part"
    ]
    vertices = sum(len(obj.data.vertices) for obj in parts)
    faces = sum(len(obj.data.polygons) for obj in parts)

    assert bpy.ops.yohsai.sewing() == {"FINISHED"}
    sewn = next(obj for obj in collection.objects if obj.get("yohsai_role") == "sewn")
    spring_edges = {
        edge_index
        for name in ("sewing_spring_A", "sewing_spring_B")
        for edge_index, item in enumerate(sewn.data.attributes[name].data)
        if item.value
    }
    elapsed = time.perf_counter() - started

    assert mesh_loader.MESH_SPACING_M == 0.005
    assert vertices == 19_454
    assert faces == 38_030
    print(
        "YOHSAI_DENSITY_OK",
        f"parts={len(parts)}",
        f"verts={vertices}",
        f"faces={faces}",
        f"springs={len(spring_edges)}",
        f"spacing_mm={mesh_loader.MESH_SPACING_M * 1000.0:g}",
        f"load_seconds={elapsed:.3f}",
    )
finally:
    if not installed_check:
        yohsai.unregister()
