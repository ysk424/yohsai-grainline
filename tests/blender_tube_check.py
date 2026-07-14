"""Blender integration check for the opt-in @TUBE Sewing construction pose."""

from __future__ import annotations

import os
import sys
import importlib
import hashlib
from pathlib import Path

import bpy
import numpy as np
from mathutils import Vector


installed_check = os.environ.get("YOHSAI_INSTALLED_CHECK") == "1"
if installed_check:
    from bl_ext.user_default import yohsai
    from bl_ext.user_default.yohsai import kitsuke, mesh_loader, yohsai_svg_parser
else:
    repo = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(repo / "tests"))
    from source_package import load_source_package

    yohsai = load_source_package(repo)
    kitsuke = sys.modules[f"{yohsai.__name__}.kitsuke"]
    mesh_loader = sys.modules[f"{yohsai.__name__}.mesh_loader"]
    yohsai_svg_parser = importlib.import_module(f"{yohsai.__name__}.yohsai_svg_parser")


source = Path.home() / "Desktop" / "test2.pdf"
if not source.is_file():
    raise RuntimeError("Missing integration input: Desktop/test2.pdf")

if not installed_check:
    yohsai.register()


def state_hash(*arrays: np.ndarray) -> str:
    digest = hashlib.sha256()
    for value in arrays:
        array = np.ascontiguousarray(value)
        digest.update(str(array.dtype).encode("ascii"))
        digest.update(np.asarray(array.shape, dtype=np.int64).tobytes())
        digest.update(array.tobytes())
    return digest.hexdigest()

document = yohsai_svg_parser.parse_pdf(source)
assert len(document["panels"]) == 2
for panel in document["panels"]:
    panel["tube"] = True

collection = mesh_loader.create_clothes_mesh(bpy.context, document)
parts = sorted(
    (obj for obj in collection.objects if obj.get("yohsai_role") == "part"),
    key=lambda obj: int(obj["yohsai_panel_index"]),
)
assert len(parts) == 2
assert all(bool(part["yohsai_tube"]) for part in parts)

# @TUBE consumes the designer's placed world-space state. Align the packed
# source panels as the normal pre-Sewing front/back placement instead of
# asking construction to interpret the Load-time side-by-side display layout.
centers = []
for part in parts:
    points = [part.matrix_world @ vertex.co for vertex in part.data.vertices]
    centers.append(sum((point for point in points), Vector((0.0, 0.0, 0.0))) / len(points))
parts[1].location.x += centers[0].x - centers[1].x
bpy.context.view_layer.update()

plan = mesh_loader.build_sewing_plan(collection)
rail_pairs = mesh_loader._tube_long_rail_pairs(plan)
assert len(rail_pairs) == 2
rails = tuple(mesh_loader._build_tube_rail(*pair) for pair in rail_pairs)
first_midpoint = mesh_loader._curve_value(rails[0].world_nodes, 0.5)
second_midpoint = mesh_loader._curve_value(rails[1].world_nodes, 0.5)
center = (first_midpoint + second_midpoint) * 0.5
chord = (second_midpoint - first_midpoint).normalized()
lower_center = (
    mesh_loader._curve_value(rails[0].world_nodes, 0.49)
    + mesh_loader._curve_value(rails[1].world_nodes, 0.49)
) * 0.5
upper_center = (
    mesh_loader._curve_value(rails[0].world_nodes, 0.51)
    + mesh_loader._curve_value(rails[1].world_nodes, 0.51)
) * 0.5
axis = upper_center - lower_center
axis -= chord * axis.dot(chord)
axis.normalize()
normal = axis.cross(chord).normalized()

# The flat candidate is clear. Increasing curvature moves one opposing arch
# toward this compact Body cube and must produce a finite contact bracket.
body_center = center + normal * 0.13
bpy.ops.mesh.primitive_cube_add(location=body_center, scale=(0.08, 0.02, 0.15))
body = bpy.context.object
body.name = "YOHSAI_TUBE_TEST_BODY"

source_positions = np.asarray(
    [
        tuple(part.matrix_world @ vertex.co)
        for part in parts
        for vertex in part.data.vertices
    ],
    dtype=np.float32,
)
preview = mesh_loader.create_sewn_mesh(bpy.context, collection, body)
assert bool(preview["yohsai_tube_constructed"])
assert 0 < int(preview["yohsai_tube_candidate_count"]) < 100
assert float(preview["yohsai_tube_effective_radius_m"]) > 0.0
assert float(preview["yohsai_tube_body_distance_m"]) > mesh_loader.TUBE_CONTACT_THICKNESS_M

preview_positions = np.asarray(
    [tuple(preview.matrix_world @ vertex.co) for vertex in preview.data.vertices],
    dtype=np.float32,
)
assert float(np.linalg.norm(preview_positions - source_positions, axis=1).max()) > 0.01

# Both long B side seams are the shared construction rails. The shorter A
# shoulder seams remain ordinary sewing springs.
b_springs = preview.data.attributes["sewing_spring_B"]
b_pairs = [
    preview.data.edges[index].vertices[:]
    for index, value in enumerate(b_springs.data)
    if bool(value.value)
]
assert b_pairs
b_maximum_squared_distance = max(
    (preview_positions[a] - preview_positions[b]).dot(preview_positions[a] - preview_positions[b])
    for a, b in b_pairs
)
assert b_maximum_squared_distance < (mesh_loader.MESH_SPACING_M * 1.5) ** 2

# The first transient native state and director frame must come from the
# verified preview, while hidden source meshes stay flat until a successful
# Kitsuke advance scatters an accepted state.
session = kitsuke._KitsukeSession(
    bpy.context,
    collection,
    body,
    preview,
    kitsuke.KITSUKE_BACKEND_STABLE_COSSERAT,
)
radius = float(preview["yohsai_tube_effective_radius_m"])
candidate_count = int(preview["yohsai_tube_candidate_count"])
try:
    assert np.array_equal(session.positions, preview_positions)
    assert not np.array_equal(session.positions, source_positions)
    assert all(part.hide_get() and part.hide_render for part in parts)
    for _click in range(2):
        session.advance(bpy.context, 0.0, 0.0, 4)
    assert np.all(np.isfinite(session.positions))
    final_lengths = np.linalg.norm(
        session.positions[session.edges[:, 1]] - session.positions[session.edges[:, 0]], axis=1
    )
    final_errors = np.abs(final_lengths - session.edge_rest)
    normal_edges = session.edge_rest >= 0.001
    short_edges = ~normal_edges
    maximum_normal_strain = float(
        np.max(final_errors[normal_edges] / session.edge_rest[normal_edges])
    )
    assert maximum_normal_strain <= 1.0e-4
    assert float(np.max(final_errors[short_edges])) <= 1.0e-6
    assert all(not part.hide_get() and not part.hide_render for part in parts)
    assert not any(obj.get("yohsai_role") == "sewn" for obj in collection.objects)
    state_digest = state_hash(
        session.positions,
        session.velocities,
        session.runtime.orientation_state(),
        session.runtime.seam_state(),
    )
finally:
    session.runtime.close()

print(
    "YOHSAI_TUBE_OK "
    f"radius={radius:.6f} candidates={candidate_count} "
    f"vertices={len(preview_positions)} max_normal_strain={maximum_normal_strain:.8f} "
    f"state={state_digest}"
)
