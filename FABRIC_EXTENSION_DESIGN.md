# Fabric Extension Design

Status: v0.5.3 @TUBE workflow implemented, synthetically verified, and accepted
by the owner in interactive use; broader clothing simulation remains experimental
Recorded: 2026-07-15 (Asia/Tokyo)

## 1. Scope and material names

This milestone adds one solver-level fabric property. It does not yet define
the PDF-to-material JSON contract, expose a panel control, or assign different
properties to different panels. The fixed test value applies to the complete
native Kitsuke session.

The owner supplied these material identifiers and meanings for the later
material-data phase:

- `_lawn60`: 60-count lawn, a thin, fine cotton woven cloth. This is the first
  approximately inextensible test target;
- `_jersey`: a soft, extensible cloth. This is reserved for the following
  positive-compliance comparison.

The current PDF parser accepts the underscore form as part of an existing panel
label and normalizes labels case-insensitively. The current production fixture
therefore reads as `OMOTE_LAWN60` and `URA_LAWN60`. Splitting a panel label into
a panel identity and a material identity, finding a same-folder JSON file, and
choosing units/ranges are deliberately deferred. Version 0.5.2 must not silently
turn this naming convention into a material-file contract.

## 2. Solver variable and energy split

The native configuration defines `extension_compliance`. It is the inverse
axial rigidity `1/(EA)`, with SI unit `1/N`. For a structural segment with rest
length `L0`, current length `L`, unit tangent `t`, and engineering strain

```text
epsilon = L / L0 - 1,
```

a positive compliance `chi` contributes

```text
E_extension = L0 * epsilon^2 / (2 chi)
gradient    = (epsilon / chi) t
H_GN        = 1 / (chi L0).
```

The former `stretch_stiffness` mixed axial extension with alignment between the
segment tangent and its Cosserat material director. Version 0.5.2 separates
these concepts. `director_alignment_stiffness` retains the accepted director
energy and orientation update, while `extension_compliance` controls only axial
length response.

The production experiment is defined in `kitsuke.py` as
`KITSUKE_EXTENSION_COMPLIANCE = 0.0`. Zero is not represented by an extremely
large penalty. It selects the equality constraint

```text
L - L0 = 0.
```

This distinction is necessary so a later `_jersey` value can be positive
without changing Cosserat director alignment or bend response.

## 3. Zero-compliance projection

After the existing material, contact, and seam relaxation in each substep, the
native solver performs a simultaneous mass-weighted length projection. For the
constraint Jacobian `J`, inverse mass matrix `M^-1`, residual `C`, and a small
redundancy regularizer `rho`, it solves

```text
(J M^-1 J^T + rho I) lambda = -C
delta_x = M^-1 J^T lambda.
```

The matrix is applied without assembling a dense matrix and solved by
deterministic preconditioned conjugate gradients. Current experimental limits
are 256 PCG iterations inside at most 320 nonlinear direction updates. A global
position-correction scale retains the existing 5 mm safety cap. The required
maximum relative structural-edge strain is `1e-4` (0.01%). If the projection
cannot meet it, native advance throws and Kitsuke restores the previous
positions, velocities, seams, and orientations. A visibly stretched result is
therefore not committed merely to make the operation appear successful.

This first implementation intentionally allocates its projection work arrays
per substep and runs the projection serially. Mathematics and failure behavior
are being established before data-layout and parallel optimization resumes.

The production fixture also contains cut-boundary transition edges shorter than
1 mm; the shortest measured case is approximately 0.161 mm. A relative `1e-4`
target there is about 16 nm, below float32 position resolution for metre-scale
world coordinates. Those sub-1-mm edges therefore use a 1 µm absolute error
floor. Edges at or above 1 mm retain the exact relative `1e-4` gate, so the
normal 5 mm warp/weft tolerance remains unchanged at 0.5 µm. The PCG
regularizer is `1e-4`. A failed projection reports its worst segment index,
endpoints, rest/current lengths, absolute error, and normalized tolerance ratio.

## 4. Verified behavior

The current source state passes:

- the native CTest suite, including a locked 5 mm segment under strong axial
  acceleration for ten advances and a distorted 5 mm square grid;
- the Python `ctypes` suite, including ten advances of a locked inextensible
  chain;
- all nine Python parser/native bridge tests;
- the current `test2.pdf` load, material-axis, density, and Sewing check:
  19,692 vertices, 38,468 proxy triangles, 17,767 material quads, 18,302 warp
  edges, 18,014 weft edges, 4,075 transition edges, and 448 sewing springs at a
  5 mm lattice pitch;
- parsing the revised fixture labels as `OMOTE_LAWN60` and `URA_LAWN60`.

The native tests require less than `1e-5` relative strain for the simple locked
chain and no more than `1e-4` for the distorted grid. The 5 mm segment's final
absolute length error is required to be below `5e-8 m`.

## 5. Known incomplete result

The current full-garment first-click test does not yet converge. With the
revised `test2.pdf`, 32 OpenMP threads, the normal flat Sewing step, and a
distant non-contacting Body, the zero-compliance projection exhausted all 320
nonlinear updates with maximum relative structural-edge strain `0.079530`
(7.953%). The worst normalized failure was segment 148, endpoints 1615/408,
rest length 1.183 mm, current length 1.089 mm, and 94 µm absolute error. The
same initial workflow with the Body contact test also rejected the click.
Kitsuke correctly rolled back both attempts.

Therefore v0.5.2 proves the solver variable, energy separation, hard-constraint
path, ABI, rollback behavior, and small-system mathematics, but it is not yet an
accepted inextensible garment solver. It must not be described as complete or
used as evidence that `_lawn60` is already simulated correctly.

Before choosing a remedy, the failure must be decomposed by offending edge
length/family, absolute versus relative error, constraint graph, locked mass,
and conflict with seam/contact ordering. In particular, any tolerance treatment
for very short cut-boundary edges must leave the normal 5 mm warp/weft target
unchanged. Coupled or persistent multiplier methods, projection placement within
the nonlinear loop, and a different sparse solve are candidates, not decisions.

## 6. Acceptance gate for the next step

The inextensible experiment becomes acceptable only when:

1. the first and repeated real-garment clicks finish without rollback;
2. every normal 5 mm warp/weft edge stays within 0.01% of its authored length;
3. seam closure, Body contact, self-contact, Lock, and exact Undo/replay retain
   their established behavior;
4. the failure check remains active rather than silently accepting stretch;
5. only after that, a positive `_jersey` compliance is chosen and compared on
   the same garment and placement.

## 7. Implemented experiment: Body-stopped Sewing construction arch

Decision recorded before shutdown on 2026-07-14: the first implementation on
the next work session is a Sewing-time construction pose for the common case of
two flat panels joined by two long seams. This addresses a geometric
degeneracy, not a weak sewing spring. If both panels remain coplanar and
zero-thickness while their two seams close, there is no volume for the Body to
occupy. Exact extension, seam closure, and collision can then request mutually
incompatible motion from the first solver step.

The intended construction is two opposing kamaboko-shaped arches. Their paired
seam chains become the two shared longitudinal rails, while the panels bow in
opposite directions and enclose a finite volume. This is analogous in purpose
to the existing non-flat `RING` construction state: flat PDF coordinates remain
the only material rest state, while the arch is only an initial 3D pose and
Cosserat director frame.

The explicit PDF command is `@TUBE`. It is intentionally opt-in;
Yohsai must not infer garment meaning from panel names such as OMOTE/URA or from
an arbitrary two-seam graph. The first strict form will accept exactly two
`@TUBE` panels connected to each other by exactly two open, long sewing groups.
Other topologies retain ordinary Sewing or return a clear validation error.
The parser and mesh loader now enforce these semantics. `@TUBE` cannot combine
with `@M` or RING in version 1.

### 7.1 Construction search

`Sewing` uses the panels' already positioned world-space state and the selected
Body. Pattern-page Y/warp is the longitudinal tube axis. The Body direction
selects the concave/inward side; the two panels bow away from the Body in
opposite directions. The paired sewing vertices are placed on shared rails, so
the preview is a closed lens/tube rather than two flat layers joined only by
zero-length springs.

The construction pose belongs to a one-parameter family from flat
(`effective radius = infinity`) toward increasing curvature. The search changes
effective radius by 10 mm per candidate and stops when any cloth candidate first
reaches the Body contact boundary. A complete Cosserat advance is not run for
every radius. Each candidate is generated directly and tested against one Body
BVH snapshot, so fewer than 100 candidates should remain an interactive
operation at the current mesh density.

The first contacting and preceding clear candidates bracket the result. A few
bounded refinements may place the final surface near the existing 5 mm contact
thickness without retaining a 10 mm overshoot. Search has a hard limit of 100
candidates and a fixed refinement count; it can never become an unbounded
collision-recovery loop. Failure to find a valid bracket reports the measured
condition and leaves the pre-Sewing state unchanged.

### 7.2 Blender state boundary

The current Sewing preview already preserves concatenated source-part vertex
order, but the first Kitsuke session currently initializes positions from the
hidden source parts rather than from preview vertices. The implementation must
change that boundary: when a verified preview exists, its world-space vertex
positions are the first transient Kitsuke positions. The hidden source meshes,
their `yohsai_pattern_position` values, material edge rest lengths, and object
transforms remain authoritative and unchanged until a successful Kitsuke click
writes the accepted pose back. Undoing Sewing therefore restores the exact
placed flat parts.

### 7.3 Penetration policy

The construction search aims to stop at first contact, but later simulation is
allowed to contain temporary Body penetration. Penetration is not itself a
reason for infinite work or whole-click rejection. Body recovery retains fixed
iteration counts, bounded correction per pass, bounded displacement per click,
and finite-state checks. If it cannot clear all penetration in one click, it
commits bounded progress and later clicks continue recovery, provided the
material solve itself is valid.

This policy does not permit unbounded search, NaN/Inf state, excessive motion,
or silently stretched inextensible edges. The immediate hypothesis is that a
finite-volume construction pose will remove the seam/zero-thickness conflict
which currently prevents the hard material projection from becoming valid.
Instrumentation must still report offending edge family and length if the first
`@TUBE` experiment does not pass.

### 7.4 Synthetic verification result

`tests/blender_tube_check.py` marks the current two production-fixture panels
as @TUBE in memory, aligns their placed centers, and constructs against a small
synthetic Body. Load-time side-by-side packing is deliberately not treated as a
meaningful Sewing placement. The long B side paths become the two rails; the
short A shoulder seams remain ordinary sewing constraints.

The 19,692-vertex construction found a clear 0.193879 m effective radius in 95
candidates. Its first and second complete Kitsuke clicks succeeded with Body
and self-contact active. Edges at or above 1 mm ended with maximum relative
strain `0.00005734`; shorter edges stayed within the 1 µm absolute floor. One
and 32 OpenMP threads produced the same final state hash
`5d1a09c37634bebcb1c4c95e56f3dea0fa5566fee59d7d04dd67224eb3269a33`.

This validates the construction search, shared-rail parameterization, preview
state boundary, short-edge precision rule, repeated-click path, and thread
determinism. On 2026-07-15, the owner additionally confirmed that the packaged
0.5.3 workflow functions correctly in interactive use and promoted it to the
current main development version. That qualitative acceptance is deliberately
separate from the exact synthetic measurements: it accepts the @TUBE workflow,
not the completeness of the broader clothing simulation.
