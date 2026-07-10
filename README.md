# Yohsai 0.1.3

Yohsai is a public, in-development Blender extension for clothing construction.
The API, data shape, and generated output are still experimental.

The planned direction is:

- JSON <=> collections of Curve objects.
- Curve collections -> garment mesh generation.
- Seam construction and stitching.
- Dressing the generated garment onto a character.

This first repository state provides a loadable Blender extension shell, an
N-panel, body selection, and silhouette export.

## Silhouette Export

In the 3D View sidebar, open `Yohsai`, pick a mesh body in the `Body` field
using the object selector or eyedropper, set `Output`, then press `Silhouette`.

The exporter writes two Adobe Illustrator-readable SVG files:

- `<Body>_shadow_xz.svg`
- `<Body>_shadow_yz.svg`

SVG dimensions are written in millimeters. Yohsai converts Blender world units
through the scene unit scale, so the path keeps real size for pattern drafting.

## Pattern SVG Load

The `SVG Path` and `Load` controls start a standalone, standard-library-only
parser in Blender's bundled Python. The Blender UI remains responsive while the
parser converts the exact `CLOTHES` layer into a fixed, atomically replaced JSON
document in Blender's private Yohsai data directory. Blender then expands `@W`
fold panels, creates an approximately 2 cm constrained triangular mesh for each
closed pattern panel, and packs the separate objects into one numbered
`CLOTHES_###` collection at Y = -1 m. Sewing labels and fold edges are retained
as mesh attributes.

After positioning the separate parts, `Sewing` infers each seam's direction from
its endpoints. It preserves the original parts as hidden source objects and
creates one combined `<collection>_SEWN` mesh with loose sewing-spring edges.
This step does not add a Cloth modifier.

The input and JSON contracts are documented in `SVG_TO_JSON_SPEC.md`.

## Package

The extension manifest is `blender_manifest.toml`. The source package contains:

- `__init__.py`
- `ui.py`
- `mesh_loader.py`
- `yohsai_svg_parser.py`
- `yohsai_defaults.json`
- `SVG_TO_JSON_SPEC.md`
- `README.md`
- `DEVELOPMENT_NOTES.md`
- `LICENSE`

## License

GNU General Public License v3.0 or later.
