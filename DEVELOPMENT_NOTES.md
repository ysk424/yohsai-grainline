# Yohsai Development Notes

Status: initial scaffold.

Current version: 0.1.0.

## Direction

Yohsai will own clothing creation. The intended pipeline is:

1. Describe garment data in JSON.
2. Read and write Curve object collections from that JSON.
3. Convert Curve collections into mesh panels.
4. Stitch mesh panels into garments.
5. Fit or dress the garment onto a character.

The first month should stay focused on data shape, coordinate conventions,
Curve object ownership, and a minimal reproducible panel-to-mesh path before
adding sewing or fitting behavior.

## Current State

- The add-on registers a `Yohsai` N-panel in the 3D View sidebar.
- No operators, data model, mesh generation, sewing, or fitting behavior is
  implemented yet.
- The extension is safe to load as a project placeholder.
