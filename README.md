# Yohsai 0.4.1 — Stable Cosserat Kitsuke

Yohsai is a public, in-development Blender extension for clothing construction.
The API, data shape, and generated output are still experimental.

This repository is the `yohsai-grainline` research continuation of the
validated Stable Cosserat implementation. Its next solver keeps the pattern
paper's vertical direction as warp and replaces the internal triangular rod
network with a grain-aligned square lattice while retaining a triangulated
Blender and collision proxy. The fixed material convention and fork boundary
are recorded in `GRAINLINE_DESIGN.md`. Until that milestone is implemented, the
current code remains the tested v0.4.1 triangular baseline.

The Illustrator pattern is authoritative; Blender meshes are replaceable
physical realizations of that pattern. The normal Yohsai workflow is
intentionally concentrated into four operations:
`Load`, `Update`, `Sewing`, and `Kitsuke`.

## N-panel workflow

The top of the Yohsai N-panel contains all three inputs:

- `Pattern Path`: the current Illustrator PDF;
- `Clothes`: the loaded Yohsai clothes collection;
- `Body`: the fixed collision mesh used by Kitsuke.

Below Body, `Lock` marks the currently selected mesh object(s) as excluded from
Kitsuke deformation. The adjacent `Auto` button is reserved for a later
automatic selection workflow. Below them are `Load`, `Update`, `Sewing`, and
`Kitsuke`, in workflow order.
`Solver` selects the native `Stable Cosserat` backend (the default) or the
original `Legacy Taichi PBD` backend for comparison and recovery.
Gravity and seam pull use the tested Yohsai defaults and are no longer exposed
as N-panel debugging controls.

## Silhouette preparation utility

Silhouette export is character preparation, normally run only once per
character, so it is not part of the Yohsai N-panel. Select the character mesh
and run `UTIL/silhouette_export.py` from Blender's Scripting workspace. It
writes `<Body>_shadow_xz.svg` and `<Body>_shadow_yz.svg` for Illustrator.

SVG dimensions are written in millimeters. Yohsai converts Blender world units
through the scene unit scale, so the path keeps real size for pattern drafting.
Complete instructions are in `UTIL/README.md`.

## Pattern PDF Load

The `Pattern Path` and `Load` controls accept Adobe Illustrator PDF. Yohsai
emits closed paths that contain a unique `#` panel label as panels. Unlabeled
artwork is not emitted, subject to the containment limitation recorded in
`SVG_TO_JSON_SPEC.md`. SVG input is no longer supported.

The standalone parser runs asynchronously with Blender's bundled Python and
writes a fixed, atomically replaced JSON document in Blender's private Yohsai
data directory. PDF parsing uses the bundled `pypdf` and `typing_extensions`
wheels. Blender expands `@W` fold panels and duplicates a panel containing
`@M` as authored-left and mirrored-right parts. Two boundary edges marked
`RING` are reserved construction edges: Load wraps the panel into a tube,
welds those boundaries, and uses the internal `@TOP` position as the
maximum-Z circumferential direction. It then creates an approximately 5 mm
constrained triangular mesh and packs the separate objects into one numbered
`CLOTHES_###` collection. Sewing labels and fold edges remain mesh attributes;
`RING` does not become a sewing variable.

Version 0.4.1 halves the previous 10 mm nominal spacing. The fixed `test2.pdf`
integration garment now contains 19,454 vertices and 38,030 triangles, versus
4,821 vertices and 9,212 triangles in 0.4.0. This is 4.035 times as many
vertices and provides the requested twofold linear mesh resolution.

On a RING panel, a single-letter sewing marker extends over its complete
boundary arc between the two RING edges. A closed sleeve `C` can therefore sew
to the composite path formed by the front `C` followed by the back `C`.
Normalized pattern distance preserves ordering even when the sleeve path is
longer, leaving the authored excess length to form gathers.

After positioning the separate parts, `Sewing` infers each seam's direction from
its endpoints. It preserves the original parts as hidden source objects and
creates one combined `<collection>_SEWN` mesh with loose sewing-spring edges.
This step does not add a Cloth modifier.

## Pattern Update

Place one unique `#` text label inside every closed pattern panel, for example
`#FRONT01` or `#BACK-BODICE`. Label whitespace is removed, ASCII letter case is
ignored, and digits, underscore, and hyphen are supported. After changing and
saving the same Illustrator PDF, press `Update` instead of Load.

Update recuts every labeled panel into new mesh data and transfers the old
garment's current 3D pose as an initial placement. Existing panel objects,
materials, transforms, and collection ownership remain. New flat-pattern
coordinates become the authoritative stretch and bend rest state, and velocity
is reset. The operation is atomic: label, panel-count, parsing, triangulation,
or transfer failures leave the existing garment unchanged.

If authored sewing membership is unchanged, press Kitsuke directly. If it
changed, Kitsuke reports `Sewing required`; inspect a new Sewing preview before
continuing.

## Incremental Kitsuke

After inspecting the `Sewing` preview, select a fixed mesh `Body` and press
`Kitsuke`. The default backend constructs a transient native C++ Stable
Cosserat rod graph. Every panel edge is a segment with its authoritative
pattern length, each segment owns a unit material-frame quaternion, and
near-collinear incident edges are paired for bending/twist. Triangle edges
together preserve the sheet's in-plane metric; RING parts use their non-flat
construction coordinates only to initialize directors.

Each click advances eight 1/240-second substeps with alternating local VBD
position sweeps and the Stable Cosserat closed-form orientation update. The
default 16 nonlinear iterations are followed by material relaxation so local
seam and contact corrections propagate into the rod graph. Each click shortens
the transient seam maximum distance by 30 mm. Deep Body penetration is resolved
over several clicks with capped corrections instead of tearing fine triangles
in one projection. Self-contact accepts point-triangle pairs only when the
normal projection lies in the triangle interior, avoiding false in-plane
repulsion on a flat sheet.

Kitsuke uses 1.0 m/s² downward acceleration so the user has time to reposition
parts between clicks. Yohsai removes the combined preview and restores every
pattern panel as a separate object. Move and rotate any panels in Object Mode,
press `Kitsuke` again, and repeat while seams close and the garment approaches
the body. `Legacy Taichi PBD` remains selectable for A/B comparison.

Selecting mesh object(s) and enabling `Lock` keeps those objects in the sewing
graph but removes their vertices from Kitsuke deformation. Lock is exclusive:
checking it replaces the previous locked set with the current selection, while
unchecking it clears the selected Clothes lock state. Locked parts can therefore
anchor or reserve sewn pieces while other parts continue dressing.

The pattern topology and its original edge lengths remain authoritative.
Scaling and vertex-count changes during Kitsuke are rejected. Other direct mesh
edits are unsupported but are not yet completely detected; edit topology only
in the pattern. Moving or rotating a part clears that part's velocity; untouched
parts retain theirs. The Body is evaluated once for a live session and remains
a fixed collider. Body/cloth and cloth/cloth contact thickness is 5 mm, while
paired seam points progressively approach 0 mm.
The Kitsuke `Iterations` box controls constraint iterations per substep; lower
it on slow PCs and raise it on stronger CPUs when stretch is still visible.

Kitsuke supports Blender Undo and Redo. Each successful click stores its exact
seam vertex pairs and targets, per-vertex velocities, Stable Cosserat edge
quaternions, revision, and Object Mode transforms in undoable Blender data.
After Undo or Redo, the non-undoable runtime is discarded and rebuilt from the
restored Blender state before the next click.
Opening the file in a new Blender/add-on runtime intentionally ignores that
recovery state. Continuing an abandoned, partially dressed session across a
restart is not supported; begin again from Load/Sewing when required.

The native backend currently runs on the CPU through a versioned C ABI loaded
with `ctypes`, so it is not tied to Blender's exact CPython patch version.
The legacy backend asks Taichi for an available GPU and falls back to its CPU.
Version 0.4.1 bundles the native Windows x64 DLL and the CPython 3.13 Windows
x64 wheels.

The input and JSON contracts are documented in `SVG_TO_JSON_SPEC.md`.
The complete Kitsuke workflow, solver invariants, tuning history, current
parameters, and resume checklist are recorded in `KITSUKE_DESIGN.md`.
The Stable Cosserat graph mapping, native boundary, contact scope, tests, and
licensing decisions are recorded in `COSSERAT_DESIGN.md`.
The pattern-designer viewpoint that governs Update, Sewing, Kitsuke, annotation
design, and future automation is recorded in `DESIGN_PHILOSOPHY.md`.
The current resume handoff and deliberately deferred issues are recorded in
`SESSION_MEMORY.md`.

## Package

The extension manifest is `blender_manifest.toml`. The source package contains:

- `__init__.py`
- `ui.py`
- `kitsuke.py`
- `cosserat_native.py`
- `mesh_loader.py`
- `yohsai_svg_parser.py`
- `yohsai_defaults.json`
- `SVG_TO_JSON_SPEC.md`
- `KITSUKE_DESIGN.md`
- `COSSERAT_DESIGN.md`
- `THIRD_PARTY_NOTICES.md`
- `DESIGN_PHILOSOPHY.md`
- `README.md`
- `DEVELOPMENT_NOTES.md`
- `SESSION_MEMORY.md`
- `UTIL/silhouette_export.py`
- `UTIL/README.md`
- `LICENSE`
- `bin/yohsai_cosserat.dll`
- `CMakeLists.txt`, `build_native.ps1`, and `native/` (corresponding C++ source
  and tests for the bundled DLL)

## Native development

Visual Studio 2022 Build Tools and CMake are sufficient for the current CPU
backend. From a Developer PowerShell, run:

```powershell
.\build_native.ps1 -Configuration Release
```

This configures `build/`, builds the DLL and native tests, runs CTest, and
installs the release DLL to `bin/yohsai_cosserat.dll`. The checked-in DLL lets
normal Windows users install the Blender extension without a compiler.

## License

GNU General Public License v3.0 or later. The full extension and its source are
available for independent users to run, study, modify, and redistribute. See
`THIRD_PARTY_NOTICES.md` for the Stable Cosserat paper and reference-code
attribution.
