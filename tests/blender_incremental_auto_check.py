"""Blender-background regression check for incremental Sewing/Kitsuke Auto stages."""

from __future__ import annotations

import importlib
import sys
import tempfile
from pathlib import Path

import bpy


repo = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(repo / "tests"))
for wheel in sorted((repo / "wheels").glob("*.whl")):
    sys.path.insert(0, str(wheel))

from source_package import load_source_package  # noqa: E402


SVG = """<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 260 100">
  <g id="CLOTHES">
    <path id="p1" d="M 20 20 L 60 20 L 60 60 L 20 60 Z"/>
    <path id="p2" d="M 80 20 L 120 20 L 120 60 L 80 60 Z"/>
    <path id="p3" d="M 140 20 L 180 20 L 180 60 L 140 60 Z"/>
    <path id="p4" d="M 200 20 L 240 20 L 240 60 L 200 60 Z"/>
    <text x="40" y="40">#P1</text>
    <text x="100" y="40">#P2</text>
    <text x="160" y="40">#P3</text>
    <text x="220" y="40">#P4</text>
    <text x="61" y="40">A</text>
    <text x="79" y="40">A</text>
    <text x="121" y="40">B</text>
    <text x="139" y="40">B</text>
    <text x="181" y="40">C</text>
    <text x="199" y="40">C</text>
    <text x="10" y="85">@S100cm</text>
    <path d="M 20 90 L 120 90"/>
  </g>
</svg>
"""


yohsai = load_source_package(repo)
kitsuke = sys.modules[f"{yohsai.__name__}.kitsuke"]
mesh_loader = sys.modules[f"{yohsai.__name__}.mesh_loader"]
parser = importlib.import_module(f"{yohsai.__name__}.yohsai_svg_parser")


yohsai.register()
try:
    with tempfile.TemporaryDirectory() as directory:
        source = Path(directory) / "incremental.svg"
        source.write_text(SVG, encoding="utf-8")
        document = parser._parse_legacy_svg(source)
        assert sorted(document["sewing_groups"]) == ["A", "B", "C"]

        collection = mesh_loader.create_clothes_mesh(bpy.context, document)
        bpy.context.scene.yohsai.clothes_collection = collection
        parts = sorted(
            (obj for obj in collection.objects if obj.get("yohsai_role") == "part"),
            key=lambda obj: int(obj["yohsai_panel_index"]),
        )
        assert len(parts) == 4
        assert not mesh_loader.participating_parts(collection)

        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(100.0, 100.0, 100.0))
        bpy.context.scene.yohsai.body_object = bpy.context.object

        parts[0].location.y += 0.01
        parts[1].location.y += 0.01
        bpy.context.view_layer.update()
        first_plan = mesh_loader.build_sewing_plan(collection)
        assert first_plan.parts == tuple(parts[:2])
        assert first_plan.labels == ("A",)
        assert bpy.ops.yohsai.sewing() == {"FINISHED"}
        preview = next(obj for obj in collection.objects if obj.get("yohsai_role") == "sewn")
        assert list(preview["yohsai_source_parts"]) == [obj.name for obj in parts[:2]]
        assert not parts[2].hide_get() and not parts[3].hide_get()
        assert bpy.ops.yohsai.kitsuke_zero_gravity() == {"FINISHED"}
        first_session = kitsuke._sessions[collection.as_pointer()]
        assert [part.obj for part in first_session.parts] == parts[:2]
        kitsuke.clear_sessions()
        assert kitsuke.completed_kitsuke_parts(collection) == parts[:2]
        assert bpy.ops.yohsai.lock_auto() == {"FINISHED"}
        assert [bool(obj.get(mesh_loader.LOCKED_OBJECT_KEY, False)) for obj in parts] == [
            True, True, False, False
        ]
        assert bpy.context.scene.yohsai.lock_selection
        assert not kitsuke.has_kitsuke_session(collection)
        assert not bool(collection["yohsai_sewing_verified"])

        parts[2].location.y += 0.01
        bpy.context.view_layer.update()
        second_plan = mesh_loader.build_sewing_plan(collection)
        assert second_plan.parts == tuple(parts[:3])
        assert second_plan.labels == ("A", "B")
        assert bpy.ops.yohsai.sewing() == {"FINISHED"}
        assert bpy.ops.yohsai.kitsuke_zero_gravity() == {"FINISHED"}
        second_session = kitsuke._sessions[collection.as_pointer()]
        assert [part.obj for part in second_session.parts] == parts[:3]
        assert [part.locked for part in second_session.parts] == [True, True, False]
        assert bpy.ops.yohsai.lock_auto() == {"FINISHED"}
        assert [bool(obj.get(mesh_loader.LOCKED_OBJECT_KEY, False)) for obj in parts] == [
            True, True, True, False
        ]

        parts[3].location.y += 0.01
        bpy.context.view_layer.update()
        final_plan = mesh_loader.build_sewing_plan(collection)
        assert final_plan.parts == tuple(parts)
        assert final_plan.labels == ("A", "B", "C")
        assert bpy.ops.yohsai.sewing() == {"FINISHED"}
        final_preview = next(obj for obj in collection.objects if obj.get("yohsai_role") == "sewn")
        assert list(final_preview["yohsai_source_parts"]) == [obj.name for obj in parts]

        print("YOHSAI_INCREMENTAL_AUTO_OK stages=2,3,4")
finally:
    yohsai.unregister()
