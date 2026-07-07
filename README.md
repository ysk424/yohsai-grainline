# Yohsai 0.1.0

Yohsai is a Blender extension scaffold for clothing construction.

The planned direction is:

- JSON <=> collections of Curve objects.
- Curve collections -> garment mesh generation.
- Seam construction and stitching.
- Dressing the generated garment onto a character.

This first repository state is intentionally empty: it only provides a loadable
Blender extension shell, an N-panel, and project notes.

## Package

The extension manifest is `blender_manifest.toml`. The source package contains:

- `__init__.py`
- `ui.py`
- `yohsai_defaults.json`
- `README.md`
- `DEVELOPMENT_NOTES.md`
- `LICENSE`
