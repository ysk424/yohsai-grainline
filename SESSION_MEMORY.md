# Yohsai Session Memory

Recorded: 2026-07-17 (Asia/Tokyo)

## Current contract

- Sewing supplies exact seam connectivity only and is invoked automatically by
  a GRAVITY button rather than exposed as a separate action.
- State and the single deformation Lock are independent per-part attributes.
  Parts progress monotonically from `PLACED` to `PENDING` to `DONE`. Load stores
  the initial Object Mode matrix; a moved placed part becomes pending at the
  next GRAVITY click, and a successful step makes pending parts done.
- Placed parts are excluded from Sewing and GRAVITY. Pending parts are the new
  Sewing work and have their Lock cleared at the GRAVITY click; done parts
  remain as connectivity anchors.
- Unresolvable sewing paths remain pending. Adding one sleeve resolves that
  side of a multipart `C` group without waiting for the other sleeve.
- Load enables and explicitly applies Auto, locking placed and done parts and
  unlocking pending parts. Switching Auto off unlocks non-placed parts;
  switching it on explicitly applies Auto again.
- GRAVITY completion changes pending parts to done without changing Lock, so
  the same unlocked parts can receive repeated GRAVITY.
- The Lock check directly changes the selected parts' single Lock attribute.
- Kitsuke starts from positioned source-panel vertices.
- Seam targets are fixed at zero and never shorten per click.
- Before 2 mm capture, seam closure is a fixed distance per substep, independent
  of how far apart the pair still is.
- Sewing is an operator instruction, not a force. The drag runs once per substep
  ahead of the prediction, and the endpoints of an uncaptured pair take zero
  velocity for that substep, so neither the drag nor the material's reaction to
  it becomes momentum.
- Pattern rest lengths connect ordinary vertices mechanically, in both
  directions: a span resists compression exactly as firmly as extension.
- Square cells use their authored 2D shear metric; proxy diagonals carry no force.
- Warp and weft are stiff while shear is soft. That is what makes grain and bias
  behave differently, and it is why a split yoke can read as a split yoke.
- Straight warp/weft triples provide zero-curvature bending. With compression
  resisted, bending is what sets the fold scale, so `bend_relaxation` is a
  material knob and not a stability tweak.
- Body contact is dissipative only: a contacting vertex keeps
  `contact_velocity_retention` of its velocity, so contact can remove kinetic
  energy but never add any. Non-contacting cloth keeps its inertia.
- No material term reads Body geometry, Body normals, or bones.
- Body geometry enters only through collision candidate lookup and contact.
- Self-contact and Body-relative shape matching are absent.
- Gravity is chosen per click with adjacent buttons: Zero gravity applies 0 and
  Normal gravity applies 9.81 m/s² in world -Z. They can be alternated without
  resetting live state.
- The product always uses native Square-Lattice Cloth with 20 iterations. Solver
  and iteration controls are intentionally absent from the beginner-facing UI.
- Finite per-click movement has no rollback threshold; only non-finite state is
  rolled back.
- `Prepare for ZOZO` is a post-GRAVITY, non-destructive hand-off. It creates
  dedicated cloth and Body copies, gives every loose seam edge at least 2.21 mm
  initial separation, and configures only Yohsai-named SHELL/STATIC groups via
  ZOZO MCP on localhost port 9633. It never starts Transfer or Run Simulation.
- Blender 5.2 XPBD uses the official Geometry Nodes `Cloth Dynamics
  (Experimental)` and `Collider` assets, not a new Cloth modifier. It requires
  a separate future button because its stitch edges need zero `rest_length`,
  the opposite of ZOZO's positive initial seam gap. See
  `XPBD_HANDOFF_DESIGN.md`.

## Interpretation rule

Implement only explicitly requested behavior. Never infer garment shape, fit,
volume, or Body-relative placement.

## What 0.5.11 got wrong, and how it was found

0.5.11 fixed a real defect — sewing injected a velocity impulse of thirty times
gravity — and introduced two regressions while doing it. Neither test suite
caught either one. Both were found only by measuring the live scene.

- **Compression was left unresisted**, on the reasoning that cloth buckles into
  wrinkles rather than resisting in-plane. That is true of the sheet and false of
  a span: the centimetre between two crossings does not shorten, because cloth
  folds by bending the lattice out of plane with its cells still a centimetre
  across. Because the edge projection skipped any span shorter than rest,
  compression became a one-way ratchet no later pass could undo, and spans
  reached -99% of their authored length.
- **The repeated edge sweeps were removed** as a workaround for the old impulse.
  They were not a workaround. A Gauss-Seidel pass carries a length correction
  only about one span further into the sheet, so one pass per iteration never
  reached the middle of a panel — the part furthest from any anchor — and the
  lattice grew under load instead of settling.

Together these made the solver diverge: a quarter of all material spans sat
outside the crimp reserve, the worst at twice its rest length, and each further
click made it worse rather than better.

The lesson worth keeping: a suite that asserts the shipped behaviour cannot see a
regression in the shipped behaviour. Measuring strain against the authored rest
length in the real scene is the check that works, and the mesh already carries
everything needed — `yohsai_pattern_edge_rest` per edge, and
`yohsai_grainline_family` to separate warp, weft, and transition spans from
rendering proxies. Edge length alone cannot do it: a sheared cell's diagonal
lands in the same range as a warp span.

## Verification

Native and Blender tests cover fixed seam targets, distance-independent seam
closure at 50 cm and 5 cm, 2 mm capture, rigid-transform/rest invariance, edge
load transmission, quad shear reduction, axial bend reduction,
Body-candidate-only contact, alternating 0↔9.81 gravity buttons,
Undo/Redo reconstruction, two→three→four-part Auto staging, partial multipart
`C` resolution (83 connections for one sleeve, 166 for both), and full pattern
data. The user also confirmed the final simplified interface can produce the
intended dressed result in the live character scene.

Convergence is covered by none of them and must be measured. A 24x24 lattice of
1 cm cells hung from its top row, at the previously shipped 16 iterations, holds
every span inside the crimp reserve, peaks at +0.47%, and is flat from the third
click onward. That lattice has no seams and no Body contact, so it bounds the
material terms only; the garment scene remains the real check.

## First end-to-end ZOZO simulation with an animated Body (2026-07-18)

The whole pipeline ran end to end for the first time: PDF pattern -> GRAVITY
kitsuke -> `Prepare for ZOZO` -> ZOZO MCP configure -> deformation capture ->
ppf session build -> a full 250-frame solve. The garment stayed a clean draped
short-sleeve dress on the posed figure; it did not tangle or blow up.

Facts worth keeping:

- **An animated Body needs no Alembic/MDD/shape-key bake.** The Body copy keeps
  its deforming Armature; ZOZO's `Capture Static Deformation` records the
  Armature-evaluated mesh per frame into the deformation cache. Baking FK onto
  the Armature is enough — the modifier deforms the mesh on the timeline and the
  capture reads it. Order is Capture -> Transfer -> Run Simulation; running
  before the cache exists is rejected with a "no deformation cache" error.
- **Capture length is bounded by the influencing action's last keyframe, not by
  `scene.frame_end`.** Shrinking the scene range does NOT truncate the cache
  (ppf's `_effective_frame_range`). A Body driven by a 742-keyframe mocap action
  cached 742 frames even though the scene ended at 250, inflating the cache to
  ~1.9 GB (225k-vert Body) and timing out the transfer. Fix: trim the driving
  action's keyframes to the intended sim length (here 250 -> ~644 MB). Keep a
  fake-user backup of the full action first; it is safe to trim in place only
  when the action has a single user.
- **Solve time scales with how much the Body moves, not with cloth failure.**
  Progressive per-frame slowdown on a high-motion Body is the contact cost
  rising with Body displacement, not divergence. A heavier mocap simply takes
  longer; it is not a sign the garment is failing.
- **One residual near-degenerate self-intersection at the right-sleeve underarm
  did not block session build or the solve.** ppf's exact predicate can still
  flag a single-shared-vertex fold that Yohsai's float resolver reports as
  clean, but it was not fatal here. The heavy 225k-vert Body is a candidate for
  decimation to shrink the cache independent of frame count.

## Release

Current packaged release: `yohsai-0.7.7.zip` (built 2026-07-18 into `dist/`,
617,496 bytes, 36 entries). Verified to contain the auto-capture ZOZO MCP client
(`zozo_mcp_client.py` calls `capture_static_deformation`), both native DLLs
(`bin/yohsai_cosserat.dll`, `bin/vcomp140.dll`), and the pypdf parsing wheel.
The manifest is 0.7.7.
Its exact size and SHA-256 are reported alongside the built artifact so
this packaged document does not contain a self-invalidating archive hash.

The archive contains 32 entries and its bundled native DLL matches
`bin/yohsai_cosserat.dll`. Keep current source, manifest, the PDF parsing wheel,
native binaries, licenses, and current documentation. Exclude build output,
caches, temporary parser output, local PDFs, and older archives from future
ZIPs.

The release bundles only the native Square-Lattice solver and the PDF parser
wheel.
