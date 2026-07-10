"""Blender-background integration check for Yohsai mesh loading."""

from __future__ import annotations

import copy
import sys
from pathlib import Path


repo_parent = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo_parent))

import bpy  # noqa: E402
import yohsai  # noqa: E402
import yohsai.ui as ui  # noqa: E402
from yohsai.mesh_loader import MeshLoadError  # noqa: E402


svg_path = Path.home() / "Desktop" / "test2.svg"
if not svg_path.is_file():
    raise RuntimeError(f"Missing integration input: {svg_path}")

yohsai.register()
try:
    bpy.context.scene.yohsai.svg_path = str(svg_path)
    result = bpy.ops.yohsai.load_svg()
    assert result == {"FINISHED"}, result
    assert ui._parse_process is not None
    ui._parse_process.wait(timeout=30)
    ui._poll_svg_parser()
    status = bpy.context.scene.yohsai.parse_status
    assert status.startswith("Loaded CLOTHES_001: 2 panel(s)"), status

    collection = bpy.data.collections["CLOTHES_001"]
    assert len(collection.objects) == 1
    obj = collection.objects[0]
    mesh = obj.data
    assert obj.name == "CLOTHES_001"
    assert obj.type == "MESH"
    assert len(obj.modifiers) == 0
    assert len(mesh.vertices) > 100
    assert len(mesh.polygons) > 100
    assert all(abs(vertex.co.y + 1.0) < 1.0e-7 for vertex in mesh.vertices)
    assert abs(min(vertex.co.z for vertex in mesh.vertices) - 0.01) < 1.0e-6
    min_x = min(vertex.co.x for vertex in mesh.vertices)
    max_x = max(vertex.co.x for vertex in mesh.vertices)
    assert abs(min_x + max_x) < 1.0e-6
    assert all(polygon.normal.y < -0.999 for polygon in mesh.polygons)

    attribute_names = set(mesh.attributes.keys())
    assert {"sewing_A", "sewing_B", "fold", "panel_index"} <= attribute_names
    assert sum(item.value for item in mesh.attributes["sewing_A"].data) > 0
    assert sum(item.value for item in mesh.attributes["sewing_B"].data) > 0
    assert sum(item.value for item in mesh.attributes["fold"].data) > 0
    assert set(item.value for item in mesh.attributes["panel_index"].data) == {0, 1}

    adjacency = {vertex.index: set() for vertex in mesh.vertices}
    for edge in mesh.edges:
        a, b = edge.vertices
        adjacency[a].add(b)
        adjacency[b].add(a)
    remaining = set(adjacency)
    component_count = 0
    while remaining:
        component_count += 1
        pending = [remaining.pop()]
        while pending:
            for neighbor in adjacency[pending.pop()]:
                if neighbor in remaining:
                    remaining.remove(neighbor)
                    pending.append(neighbor)
    assert component_count == 2

    panel_vertices: dict[int, set[int]] = {0: set(), 1: set()}
    panel_attr = mesh.attributes["panel_index"].data
    for polygon in mesh.polygons:
        panel_vertices[panel_attr[polygon.index].value].update(polygon.vertices)
    bounds = []
    for panel_index in (0, 1):
        xs = [mesh.vertices[index].co.x for index in panel_vertices[panel_index]]
        bounds.append((min(xs), max(xs)))
    bounds.sort()
    assert bounds[1][0] - bounds[0][1] >= 0.099

    invalid = copy.deepcopy(ui._loaded_pattern_json)
    invalid["panels"][0]["segments"][1]["fold"] = True
    try:
        ui.create_clothes_mesh(bpy.context, invalid)
    except MeshLoadError as exc:
        assert "more than one fold" in str(exc)
    else:
        raise AssertionError("A panel with multiple fold segments was accepted")

    # A second first-time import avoids name collisions instead of overwriting.
    obj2 = ui.create_clothes_mesh(bpy.context, ui._loaded_pattern_json)
    assert obj2.name == "CLOTHES_002"
    assert "CLOTHES_002" in bpy.data.collections
    print(
        "YOHSAI_MESH_OK",
        f"verts={len(mesh.vertices)}",
        f"faces={len(mesh.polygons)}",
        f"fold_edges={sum(item.value for item in mesh.attributes['fold'].data)}",
    )
finally:
    yohsai.unregister()
