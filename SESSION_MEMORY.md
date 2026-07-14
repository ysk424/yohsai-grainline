# Yohsai session memory

Status: v0.5.3 @TUBE workflow owner-accepted as the current main development
version; broader clothing simulation remains experimental
Recorded: 2026-07-15 (Asia/Tokyo)
Latest build prepared: Yohsai 0.5.3 experimental

Yohsai 0.5.3 uses a 5 mm grain-aligned square material lattice and retains the
native Stable Cosserat default backend introduced in 0.4.0. The product
viewpoint below remains authoritative, but current solver state and validation
must be read from `FABRIC_EXTENSION_DESIGN.md`, `GRAINLINE_DESIGN.md`, and
`README.md`.

## 0.5.2 incomplete inextensible-material milestone

The native ABI is version 4. The former coupled `stretch_stiffness` is split
into `director_alignment_stiffness` and `extension_compliance`. Compliance is
inverse axial rigidity `1/(EA)`. Kitsuke currently supplies the fixed value
zero, which invokes a simultaneous mass-weighted equality projection for all
structural rest lengths. A positive value is implemented for later elastic
testing but is not designed or selected yet.

No PDF-to-material JSON lookup or UI is present. The retained future material
identifiers are `_lawn60` for 60-count lawn, the first inextensible test target,
and `_jersey` for a soft extensible comparison. The current PDF labels parse as
`OMOTE_LAWN60` and `URA_LAWN60`; do not infer a final filename contract from
that spelling.

Native CTest, the Python bridge and parser suite, and the current density/Sewing
integration pass. Current `test2.pdf` counts are 19,692 vertices, 38,468 proxy
triangles, 17,767 quads, 18,302 warp edges, 18,014 weft edges, 4,075 transition
edges, and 448 sewing springs. The normal flat first click remains incomplete:
a distant Body and 32 threads leave 0.079530 maximum relative strain after all
320 projection updates; the Body-contact check also rejects. Native failure
propagates to Kitsuke, which restores positions, velocities, seam state, and
orientations. Do not weaken this rollback merely to produce output.

## 0.5.3 @TUBE experiment

The parser now accepts `@TUBE` on exactly two non-RING, non-`@M` panels. Sewing
requires the selected Body and a designer-aligned world placement, selects two
long open warp-direction path pairs, and generates opposing circular arches on
shared authored-length rails. Search uses 10 mm effective-radius steps, four
refinements, one Body BVH, and at most 99 candidates. Failure is atomic. The
verified preview supplies the first Kitsuke positions and @TUBE director frame.

Sub-1-mm transition edges use a 1 µm absolute projection floor because their
relative `1e-4` target is below float32 resolution near one-metre coordinates.
All edges at or above 1 mm, including normal 5 mm warp/weft, retain the exact
`1e-4` relative gate. Failure diagnostics identify the worst segment and
measured error.

The synthetic `blender_tube_check.py` test uses all 19,692 `test2.pdf` vertices,
finds radius 0.193879 m in 95 candidates, and completes two Body/self-contact
Kitsuke clicks at 0.00005734 maximum normal-edge strain. One and 32 OpenMP
threads match exactly with SHA-256 state
`5d1a09c37634bebcb1c4c95e56f3dea0fa5566fee59d7d04dd67224eb3269a33`.
On 2026-07-15, the owner confirmed that the packaged 0.5.3 workflow functions
correctly in interactive use and chose it as the current main development
version. This qualitative acceptance promotes the @TUBE direction without
claiming that the overall clothing simulation is complete. The exact synthetic
test remains the reproducible regression record.

## 0.5.1 performance milestone and retained plan

Self-contact was the measured first-click bottleneck. Version 0.5.1 moves its
spatial hash, exact triangle-AABB filtering, and point-triangle evaluation from
Python into native C++, retains candidates with a 5 mm motion skin, and uses
deterministic per-vertex OpenMP loops without atomics. Contact and seam scratch
storage is reused. The C ABI is version 3.

On the then-current `test2.pdf`, native advance fell from 8.483 s to 1.370 s and complete session
advance from 10.231 s to 1.596 s. The maximum neighbor list was 678,499 pairs,
with 42 rebuilds and 71,752,885 candidate tests. These are local regression
values. The retained CPU material/data-layout work in `GRAINLINE_DESIGN.md` was
deferred when the owner prioritized material mathematics. CUDA remains a later
whole-solver experiment with the CPU/OpenMP path retained as fallback.

## Product viewpoint that must not drift

Yohsai is pattern-designer software. The Illustrator PDF pattern owns the
cloth pieces, dimensions, labels, sewing connectivity, and future construction
annotations. Blender meshes and simulation state are replaceable realizations.
Update means recutting revised cloth, transferring the old 3D pose only as a
placement convenience. Read `DESIGN_PHILOSOPHY.md` before changing architecture.

The production N-panel intentionally contains three inputs followed by four
actions:

1. Pattern Path, Clothes, Body;
2. Lock/Auto;
3. Load, Update, Sewing, Kitsuke.

Lock is a literal object-level deformation exclusion for selected mesh objects.
It preserves sewing information and does not infer garment meaning or dressing
order. It is exclusive, not additive: checking Lock switches the locked set to
the current selection, and unchecking clears the selected Clothes lock state.
Auto is intentionally not implemented yet.

Silhouette export is a once-per-character Scripting utility in `UTIL/`, not a
normal Yohsai button. Gravity and seam closure are fixed internal values, not
production UI controls.

## Working workflow

1. Save the annotated Illustrator PDF.
2. Load it; Yohsai creates separate packed panel objects.
3. Translate and rotate panels in Object Mode.
4. Select the character skin Mesh as Body; @TUBE Sewing needs it for search.
5. Run Sewing and visually verify connectivity and any construction arch.
6. Alternate Kitsuke clicks with manual Object Mode translation/rotation.
7. After editing and saving the same pattern, use Update; use Sewing again only
   when the authored sewing signature changed.

The legacy Taichi solver values recorded for this historical handoff are 8
substeps at 1/240 s, user-adjustable 1-128
constraint iterations/substep with default 16, Gravity 1.0 m/s², Seam Pull
30 mm/click, ratcheting seam
maximum-distance constraints, four post-contact seam projection passes, bend
stiffness 0.08, stretch stiffness 0.95, maximum speed 1.0 m/s, maximum
constraint correction 5 mm/iteration, and 5 mm contact thickness.
2026-07-12 user testing confirmed that higher Iterations reduce visible stretch
and make the cloth behave stiffer. Keep the Iterations UI because the developer
machine is much faster than a typical user PC; slower users need a direct way to
lower quality instead of inheriting a fixed high-cost solver setting.

Important design rule: visible seam opening is the unacceptable failure mode.
When a garment is too small or Body collision conflicts with sewing, Yohsai
prefers local cloth distortion or Body penetration over a seam coming apart.
This is intentional because future versions are expected to weld or otherwise
topologically connect sewn seams.

2026-07-12 real-character retest: the 0.2.3 seam-priority projection produced a
satisfactory shoulder result and resolved the visible seam-opening failure. Keep
this rule explicit; generic cloth-simulation instincts may otherwise move
collision resolution back after sewing and reintroduce the problem.

0.2.4 adds manual Lock. A locked mesh object stays in the sewing graph but its
vertices are not deformation targets during Kitsuke. This is a major staging
primitive for not-yet-worn parts and dressing-order control. Keep it free of
garment-semantic assumptions.

0.2.5 fixes manual Lock switching. Rechecking Lock with a different selection
must replace the old locked set; otherwise staged dressing cannot move from one
reserved part to another.

0.2.6 fixes manual Lock clearing. Unlock must clear the whole selected Clothes
scope resolved from selected objects' `yohsai_collection`, not just the current
selection or a possibly stale N-panel Clothes pointer. The expected workflow is
front locked/back deforming, then back locked/front deforming, then no lock so
both deform. Manual testing confirmed this sequence works in 0.2.6.

## 0.2.0 Undo/Redo design

Blender Undo restores meshes but cannot restore Python/Taichi objects. Each
successful Kitsuke click therefore writes same-runtime recovery data into
undoable Blender datablocks:

- collection runtime epoch and revision;
- exact global seam-vertex pair array;
- current seam-rest array;
- one `yohsai_kitsuke_velocity` point attribute per panel;
- the last accepted Object Mode matrix per panel.

`undo_post` and `redo_post` clear live Taichi sessions. The next Kitsuke rebuilds
from Blender's restored positions, velocities, seam targets, and transforms.
Undoing click 2 restores click 1 exactly; repeating Kitsuke performs click 2
again instead of skipping to click 3. A new Blender/add-on runtime has a new
epoch and deliberately ignores old recovery data. Cross-restart continuation
of an abandoned partially dressed session is unsupported; restart from
Load/Sewing. A persistent `load_pre` handler rotates the epoch whenever Blender
loads a `.blend`, including another file opened in the same process.

Tests:

- `tests/blender_density_check.py`: exact 5 mm production-fixture vertex,
  triangle, and sewing counts without running a full dressing sequence;
- `tests/blender_mesh_check.py`: PDF Load, Sewing, repeated Kitsuke, Update, and
  sewing-change rejection;
- `tests/blender_undo_check.py`: exact seam/velocity Undo and Redo restoration,
  plus replay of click 2.
- `tests/blender_thread_determinism_check.py`: first-click SHA-256 state hash
  for comparing OpenMP thread counts; one and 32 threads must match exactly.
- `tests/blender_sleeve_check.py`: mirrored RING sleeve construction, composite
  seam connection, and finite Stable Cosserat advance.

## Current supported input

The original integration input is `C:\Users\azoo\Desktop\test2.pdf`. It currently
produces two labeled panels (`OMOTE_LAWN60`, `URA_LAWN60`) and sewing groups A/B. Bundled dependencies are
Taichi 1.7.4, pypdf 6.14.2, and their listed wheels for Blender 5.2 / CPython
3.13 on Windows x64.

## Explicitly deferred issues

These were reviewed on 2026-07-11 and intentionally not fixed in 0.2.6:

- direct vertex edits and same-vertex-count topology changes are unsupported but
  not fully detected by Kitsuke;
- an unlabeled closed PDF outline enclosing a labeled panel makes containment
  ambiguous even though unlabeled artwork is not emitted;
- PDF close-and-paint and implicit fill closure are not recognized as explicit
  closed panels;
- the ASCII label regex accidentally accepts a few Unicode case-folding
  characters.

Do not reinterpret these accidental acceptances as product requirements.

## Next engineering priority

Keep 0.5.3 as the working baseline and examine the remaining gap before choosing
another solver architecture or large feature phase. The owner judges that a
complete clothing-simulation result is still some distance away, but also notes
that the decisive improvement may be simple; preserve room for that reassessment
instead of assuming complexity. Turn the next visible failure into a small,
measured fixture and first test whether construction placement, graph
conditioning, or seam/contact interaction explains it. Do not relax the normal
5 mm warp/weft tolerance or atomic rollback. Positive `_jersey` compliance,
compact CSR/SoA incidence, direct quad-corner lookup, reused quad evaluations,
colored sweeps, SIMD, and whole-solver CUDA remain candidates rather than the
automatic next step.

## Release procedure

1. Fully close interactive Blender and Blender MCP before replacing the
   extension; the Taichi native module can lock installed wheel files.
2. Build with Blender's `--command extension build` command.
3. Validate the resulting ZIP.
4. Install the ZIP with `extension install-file -r user_default -e`.
5. Run both Blender integration scripts against the installed extension.
6. Confirm `git status --short` is empty, then commit and push `main`.

The sleeve integration input is `C:\Users\azoo\Desktop\test3.pdf`. It adds
`#SODE`, `@M`, `@TOP`, two `RING` edges, and a C seam whose sleeve path is
longer than the combined front/back C path. Load creates left/right sleeve
instances, wraps each into a welded cylinder, and Sewing pairs each closed C
loop with one composite body C loop.

The current expected archive is `dist/yohsai-0.5.3.zip`. It is the owner-accepted
main experimental checkpoint and retains the documented normal-flat rollback.
