# Yohsai Development Notes

Status: public development preview.

Current version: 0.1.1.

## Version and Build Policy

The final numeric component of the extension version is the build number. It
must be incremented before every extension package build. The version in
`blender_manifest.toml`, the documented current version, and the generated
archive name must agree.

Existing project documentation and specifications must remain in the repository
when a new build is produced. Amend or supersede them explicitly instead of
silently removing the current record.

This repository is public-facing but still under active development. Interfaces,
data formats, silhouette output details, and packaging may change before a stable
release.

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
- `SVG Path` and `Load` run a standard-library-only SVG parser in a separate
  process and asynchronously load its fixed, atomically written JSON result.
- Load expands fold-cut panels and creates one packed, cloth-ready triangular
  Mesh object in a new numbered `CLOTHES_###` collection. Sewing and fold
  membership are preserved as mesh attributes.
- SVG parsing currently covers the exact `CLOTHES` layer, closed line/cubic
  paths, meter scaling through `@S<number>cm`, single-letter sewing groups, and
  `@W` fold edges. The contract is documented in `SVG_TO_JSON_SPEC.md`.
- The panel owns a mesh body pointer, with Blender's object selector/eyedropper.
- `Silhouette` exports XZ and YZ orthographic silhouette shadows as SVG files
  readable by Adobe Illustrator.
- No sewing execution, Cloth modifier setup, dressing, or update/shape-transfer
  behavior is implemented yet.
