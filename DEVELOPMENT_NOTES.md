# Yohsai Development Notes

Status: public development preview.

Current version: 0.5.3 experimental, current main development version.

The authoritative Kitsuke design, tuning log, known limitations, and resume
checklist are maintained in `KITSUKE_DESIGN.md`.
The v0.4 native Stable Cosserat backend and its acceptance record are maintained
in `COSSERAT_DESIGN.md`; `KITSUKE_DESIGN.md` retains the legacy Taichi baseline.
The grain-aligned square-lattice fork contract is maintained in
`GRAINLINE_DESIGN.md`.
The current extension-compliance mathematics, fabric identifiers, tests, and
known non-convergence are maintained in `FABRIC_EXTENSION_DESIGN.md`.
The product-level pattern-designer perspective and anti-drift rules are
authoritative in `DESIGN_PHILOSOPHY.md`.
The latest tested handoff, deferred issues, release procedure, and next priority
are summarized in `SESSION_MEMORY.md`.

## 0.5.2 Experimental Extension Compliance

Version 0.5.2 introduces native `extension_compliance`, defined as inverse
axial rigidity `1/(EA)`, and separates it from
`director_alignment_stiffness`. The fixed Kitsuke experiment uses zero
compliance for an equality length constraint; positive compliance is present in
the solver for a later extensible-fabric test but has not been parameterized.
No material UI or PDF-adjacent JSON lookup is implemented.

The owner-defined material names retained for the later data contract are
`_lawn60` for 60-count lawn (thin, fine cotton woven cloth) and `_jersey` for a
soft extensible cloth. The revised source PDF parses the current panel labels as
`OMOTE_LAWN60` and `URA_LAWN60`.

The zero-compliance path solves all structural length constraints together with
a deterministic matrix-free PCG projection. It rejects a click unless maximum
relative structural-edge strain is at most `1e-4`, restoring the complete prior
native state on failure. Native CTest, the Python bridge suite, and all nine
Python tests pass. The current density/Sewing integration also passes at 19,692
vertices, 38,468 triangles, 17,767 quads, and 448 sewing springs.

This implementation is intentionally recorded as incomplete. The first full
Kitsuke click on the revised `test2.pdf`, with a distant Body and 32 OpenMP
threads, exhausts 320 nonlinear projection updates at maximum relative strain
`0.079530` after the short-boundary precision treatment; Body-contact placement
also rejects. Both attempts roll back. See
`FABRIC_EXTENSION_DESIGN.md` for equations, exact gates, and the required
failure decomposition before selecting a remedy.

## 0.5.3 @TUBE Construction Experiment

Version 0.5.3 accepts `@TUBE` on exactly two non-RING, non-mirrored
panels. Sewing selects exactly two paired open paths spanning at least half of
both panels' page-warp extent, maps them to shared authored-length rail curves,
and constructs opposing circular arches. A selected Body supplies one evaluated
BVH. The search uses a flat candidate, 10 mm effective-radius steps, four bounded
refinements, and at most 99 total candidates; any failure occurs before source
objects are hidden or changed. A verified construction preview now supplies the
first transient Kitsuke positions and the @TUBE Cosserat director frame.

The real fixture contains transition edges as short as 0.161 mm. Requiring
`1e-4` relative error on such an edge means roughly 16 nm at world coordinates
near one metre, below float32 position resolution. Projection therefore uses a
1 µm absolute floor only for rest lengths below 1 mm. Edges at and above 1 mm,
including every normal 5 mm warp/weft edge, retain the exact `1e-4` relative
gate. Failure diagnostics now report the worst segment, endpoints, rest/current
length, absolute error, and tolerance ratio.

`tests/blender_tube_check.py` sets the two current fixture panels to @TUBE in
memory, aligns their placed centers, constructs against a compact synthetic
Body, and completes two full Kitsuke clicks. The 19,692-vertex check found an
effective radius of 0.193879 m in 95 candidates and a maximum normal-edge strain
of 0.00005734. One and 32 OpenMP threads produced the identical final state hash
`5d1a09c37634bebcb1c4c95e56f3dea0fa5566fee59d7d04dd67224eb3269a33`.
The density/Sewing regression remains unchanged. The existing RING sleeve
advance still rejects under the new zero-compliance gate: its worst 0.521 mm
transition edge retains 1.81 µm absolute error (tolerance ratio 1.81). It is not
claimed as accepted by this experiment.

On 2026-07-15, the owner confirmed that the packaged 0.5.3 workflow functions
correctly in interactive use and selected it as the current main development
version. This is qualitative functional acceptance of the @TUBE direction; it
does not claim that the broader clothing simulation is finished or replace the
exact automated evidence above. Further solver changes should begin from this
working checkpoint and from a fresh assessment of the remaining visible gap.

## First v0.4 Owner Acceptance

On 2026-07-14, the installed Stable Cosserat build was accepted in interactive
testing: no visible Body penetration was observed, CPU response was considered
practical, and its behavior was judged closer to the intended result than the
project's XPBD and finite-element prototypes. This is qualitative owner
feedback; it does not replace measured benchmarks or automated regression
tests. The owner also reported lower-than-expected implementation time and
model-token use.

## 0.5.1 Native Self-Contact Parallelization

Version 0.5.1 moves the production self-contact broad phase from Python into
the native solver. A deterministic triangle spatial hash builds a point-face
neighbor list using 20 mm cells and a 10 mm padded triangle AABB. The list has
a 5 mm motion skin and is rebuilt when any vertex has moved more than 2.5 mm
from the build position. Structural edges, all collision-proxy edges (including
quad diagonals), face edges, and sewing pairs remain excluded.

Candidate lookup, exact AABB filtering, and point-triangle contact evaluation
run as OpenMP vertex-parallel loops. Every vertex retains face order, writes
only its own correction, and uses no atomic floating-point reduction. Contact
scratch buffers and flattened CSR candidate storage are reused across solver
iterations. The C ABI is version 3 and adds collision-proxy edges, an internal
self-collision sentinel, broad-phase rebuild count, and candidate-test count.

On the fixed `test2.pdf` first-click state, the maximum active candidate list
contains 678,499 point-face pairs, is rebuilt 42 times, and performs 71,752,885
candidate tests over the complete advance. Native advance time changed from
8.483 s in 0.5.0 to 1.370 s in 0.5.1 (6.19x); the surrounding Kitsuke session
advance changed from 10.231 s to 1.596 s (6.41x). These are local regression
measurements, not cross-machine guarantees.

The same state and statistics were obtained at every tested thread count. A
separate production-fixture regression produced the identical SHA-256 state
hash with one and 32 threads. The native advance measured 3.996 s with one
thread, 1.795 s with eight, 1.546 s with sixteen, and 1.370 s with the machine
default of 32 logical threads. The material position/orientation sweep remains
serial in this release; its colored parallelization and compact data layout are
the next CPU optimization phase.

The complete source Load/Sewing/ten-click Kitsuke/Update regression took
106.0 s, down from the 0.5.0 record of 288.065 s (2.72x). The packaged and
installed 0.5.1 build repeated it in 105.3 s. Density, mirrored RING sleeve,
native/Python unit suites, and exact Undo/Redo/replay also pass.

## 0.5.0 Grainline Material Lattice

Version 0.5.0 retains the accepted 5 mm linear resolution but samples a global
page-aligned square grid. Pattern vertical is warp and pattern horizontal is
weft for every panel. Complete cells use four warp/weft Cosserat sides plus
explicit quad shear and area energies; their Blender/collision diagonal is a
non-structural proxy. Irregular cut boundaries retain a narrow triangular
transition network. No per-panel grain annotation or inference was added.

On `test2.pdf`, the two panels contain 16,948 vertices, 33,018 proxy triangles,
15,102 material quads, and 448 sewing constraints. The material graph contains
15,626 warp, 15,365 weft, and 3,871 boundary/transition segments; 15,102 proxy
diagonals are excluded. The native ABI is version 2 and reports separate shear
and area energy statistics.

The native unit suite, Python bridge suite, density/axis validation, full
Load/Sewing/Kitsuke/Update integration, mirrored RING sleeve test, and exact
Undo replay all pass. The final full source integration suite took 288.065
seconds and the density-only fixture took 4.878 seconds on the development machine. The
solver remains single-threaded; these timings are local regression values.

## 0.4.1 Mesh Density

Version 0.4.1 halves the nominal cloth spacing from 10 mm to 5 mm. On the fixed
`test2.pdf` integration input, the two panels now contain 19,454 vertices and
38,030 triangles, with 448 sewing constraints. The previous 0.4.0 mesh had
4,821 vertices, so the new vertex count is 4.035 times larger.

The source integration test completed ten Stable Cosserat Kitsuke operations
without rollback on 2026-07-14. The complete Load, Sewing, Kitsuke, and Update
test took 388.210 seconds on the development machine. A density-only Load and
Sewing check took 4.714 seconds. These timings are regression observations for
this machine, not cross-machine performance guarantees. High-density testing
also exposed and fixed a quaternion-normalization mismatch between live
continuation and Undo reconstruction; replay now has zero state and seam delta.

The first real-character 0.1.9 trial produces a recognizable fitted dress from
the incremental workflow. Remaining visible holes at the shoulder, chest, and
abdomen make Body contact refinement the next primary engineering task.

## Version and Build Policy

Extension versions use `MAJOR.MINOR.PATCH`. A feature milestone may increment
MINOR and reset PATCH, as 0.2.0 does for Undo/Redo synchronization; subsequent
compatible fixes increment PATCH. Every distributed build must have a version
not previously distributed. The version in `blender_manifest.toml`, the
documented current version, and the generated archive name must agree.

Existing project documentation and specifications must remain in the repository
when a new build is produced. Amend or supersede them explicitly instead of
silently removing the current record.

This repository is public-facing but still under active development. Interfaces,
data formats, utility output details, and packaging may change before a stable
release.

## Direction

The Illustrator PDF pattern and its annotations are the source of truth.
The implemented pipeline parses that pattern to fixed JSON, cuts separate
grain-aligned material lattices with triangulated Blender proxies, verifies
Sewing, and incrementally dresses them with Kitsuke. Update recuts labeled
panels while transferring only a useful initial 3D placement. Future work must
extend this pattern-designer workflow instead of treating Blender mesh identity
as authoritative.

## Current State

- The add-on registers a `Yohsai` N-panel in the 3D View sidebar.
- The N-panel groups Pattern Path, Clothes, and Body at the top, followed only
  by Lock/Auto and the four primary actions: Load, Update, Sewing, and Kitsuke.
  Lock marks selected mesh objects as excluded from Kitsuke deformation. It is
  an exclusive staging set: checking Lock switches the locked set to the current
  selection, and unchecking clears the selected Clothes lock state. Auto is
  present as a reserved button and has no automatic behavior yet.
- `Pattern Path` and `Load` accept PDF, run the parser in a separate process,
  and asynchronously load its fixed, atomically written JSON result. PDF uses
  bundled `pypdf` and `typing_extensions` wheels. SVG input is no longer
  supported.
- Load expands fold-cut panels and `@M` mirror instances. Two `RING` boundaries
  are sampled equally and welded after `@TOP`-oriented tube construction. Load
  creates one packed Mesh object per resulting instance in a new numbered
  `CLOTHES_###` collection. Its 5 mm page-aligned square material cells retain
  triangulated Blender/collision proxies and a narrow boundary transition.
  Sewing and fold membership are preserved as mesh attributes; RING is a
  reserved construction word.
- `#` text inside a panel provides a normalized, human-authored identity for
  Update. Whitespace is removed; ASCII letters compare case-insensitively; and
  digits, underscore, and hyphen are accepted.
- `Update` asynchronously rereads the same PDF, recuts all labeled panels,
  transfers the current 3D pose through stored flat-pattern coordinates, and
  atomically swaps new Mesh datablocks into the existing objects. New pattern
  geometry owns rest lengths and velocities reset to zero.
- Unchanged sewing signatures remain verified across Update. Changed sewing
  membership invalidates verification, and Kitsuke reports `Sewing required`
  until the independent Sewing operation succeeds again.
- `Sewing` orders each marked boundary path, infers its direction from the
  positioned world-space endpoints, and creates a combined mesh with loose
  sewing-spring edges. A closed RING sleeve path pairs with the virtual loop
  formed by its front and back body paths. The separate source parts remain
  hidden in the same collection for future update work.
- `Kitsuke` treats the separate panel objects as the editable 3D realization.
  The first click constructs transient native Stable Cosserat warp/weft and
  transition frames, quad shear/area state, VBD positions, sewing, Body-contact,
  and self-contact state; later clicks
  reuse it while synchronizing supported Object Mode placement. The original
  Taichi implementation remains selectable as `Legacy Taichi PBD`. Each click
  advances a fixed short interval and scatters the result back to the original
  objects. Translation and rotation clear velocity only on the moved parts.
  Sewing maximum distances close by 30 mm per click and ratchet down whenever
  the current seam distance becomes shorter. Seam projection runs after Body
  and self-contact; if a small garment cannot satisfy every constraint, seam
  separation is treated as the unacceptable failure mode, while nearby cloth
  distortion or Body penetration is accepted as the lesser failure. Gravity
  defaults to 1.0 m/s². Stable Cosserat uses eight 1/240-second substeps,
  sixteen alternating iterations by default, capped 0.5 mm Body corrections,
  and bounded 1.0 m/s velocity. Body contact uses one nearest triangle per
  nearby or penetrating cloth vertex; unstable steps are rolled back before
  Blender mesh data is changed.
  Locked mesh objects remain in the sewing graph but their vertices are fixed
  during Kitsuke; this supports staged dressing and future part-addition
  workflows without inferring garment semantics.
  Gravity and seam closure use the tested fixed defaults and are intentionally
  absent from the production N-panel.
- Each successful Kitsuke click mirrors the non-undoable runtime's exact seam
  pairs and targets, per-vertex velocities, revision, and transforms into
  Blender data. Undo/Redo handlers discard stale native/Taichi runtimes; the
  next click reconstructs them
  from the state restored by Blender. A new Blender/add-on runtime ignores the
  old recovery epoch. Cross-restart continuation of a partially dressed state
  is unsupported; the supported workflow starts again from Load/Sewing.
- PDF parsing emits explicitly closed line/cubic paths containing unique `#`
  labels. Unlabeled closed artwork still participates in containment testing,
  so it must not enclose a labeled panel in the current implementation. PDF
  page points supply physical scale. `@S<number>cm` is no longer required and
  is rejected as obsolete metadata. The contract is documented in
  `SVG_TO_JSON_SPEC.md`.
- The panel owns a mesh body pointer, with Blender's object selector/eyedropper.
- `UTIL/silhouette_export.py` is a standalone `bpy` preparation script for the
  Scripting workspace. It exports XZ and YZ orthographic silhouette SVGs and is
  normally run once per character; it is not registered as an add-on operator.
- No Blender Cloth modifier is used. The active native/Taichi runtime is in memory;
  Blender data contains only same-runtime Undo/Redo recovery state. Reopening
  and continuing a partially dressed session is unsupported.

## Known implementation gaps intentionally deferred

- Same-vertex-count topology edits and direct vertex edits are unsupported but
  are not yet reliably rejected. Pattern topology must be changed in Illustrator.
- PDF close-and-paint operators and implicit fill closure are not yet treated as
  explicit closed panels; current Illustrator test data uses an explicit close.
- The intended annotation contract is ASCII-only, but validation still has a
  known Unicode case-folding gap. Do not depend on accidental acceptances.
