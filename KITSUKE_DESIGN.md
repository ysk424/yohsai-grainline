# Kitsuke Design

Status: current square-lattice cloth contract

## Purpose

GRAVITY advances the separately positioned pattern parts after its automatic
Sewing phase records exact cross-panel vertex pairs. It must not infer garment fit, volume, intended
Body-relative placement, or a Body-shaped rest curvature.

## Initial state

The first click of a pending stage starts from the current world-space vertices
of the source part objects. The transient Sewing preview supplies connectivity only. Pattern attributes
supply the intrinsic material metric. Body geometry defines neither initial
positions nor material rest state.

## Free-air dynamics

The external free-air inputs are downward gravity and constant-magnitude seam
attraction. The cloth distributes the seam load using
three Body-independent internal terms:

1. authored rest lengths on all non-proxy material edges;
2. the authored 2D shear metric of each square cell;
3. weak zero-curvature bending along straight warp and weft triples.

The proxy diagonal used to render each square as triangles has no spring. There
is no area-to-Body, normal-to-bone, director, silhouette, or shape-matching term.

## Seam

Every explicit sewing pair has target length zero from runtime creation onward.
Clicks do not shorten, replace, or ratchet this target. Until capture, the force
magnitude is constant and only its direction follows the endpoint line; a 50 cm
and a 5 cm gap therefore receive the same force. At 2 mm, or when the endpoints
cross during a substep, the pair is captured and held at zero distance.

## Body contact

Body is a fixed collider. Candidate lookup uses the evaluated world-space Body
mesh and a 40 mm search radius. A 5 mm contact thickness is applied only in the
contact path. Each contact correction is capped at 0.20 mm per iteration so an
initial penetration is removed gradually rather than becoming an impulse.
Self-contact is absent.

## Runtime control and fixed values

- time step: 1/240 s;
- substeps per click: 8;
- gravity per click: either 0 or 9.81 m/s² in world -Z;
- material/contact iterations: fixed at 20;
- seam attraction: 300 force units (300 m/s² at unit inverse mass);
- seam capture distance: 2 mm;
- edge stretch relaxation per sweep: 1.0, with four alternating sweeps;
- quad shear relaxation per iteration: 0.02;
- axial bend relaxation per iteration: 0.0001;
- Body candidate radius: 40 mm;
- Body contact thickness: 5 mm.

`Zero gravity` advances with no gravitational acceleration. `Normal gravity`
applies `(0, 0, -9.81)` m/s². Either button can follow the other within the same
live session without resetting positions, velocities, or seam state. The solver
is always the native CPU Square-Lattice Cloth backend.

After the coupled material iterations, additional alternating edge/seam sweeps
converge the strong sewing load into the lattice so a stitch vertex cannot run
ahead and leave a torn one-edge spike.

## Editing and recovery

Moving or rotating a part between clicks replaces that part's positions and
clears its velocity. Scaling and vertex-count changes are rejected. Lock keeps
seam and material connectivity but prevents the locked vertices from moving.

Load stores each part's initial Object Mode matrix and initializes its monotonic
state as `PLACED`. At a GRAVITY click, a placed part whose Object Mode matrix has
changed becomes `PENDING`. Automatic Sewing uses pending parts as the new work,
retains `DONE` parts as possible sewing anchors, and omits placed parts. A
GRAVITY click clears the independent deformation Lock from all pending parts
before Sewing, so each pending part is deformable. A successful simulation
changes every pending participant to `DONE` and does not change its Lock.

State and Lock are separate per-part attributes. Auto is an explicit lock
operation, not a continuously derived policy. Load turns Auto on and applies it:
placed and done parts become locked, while pending parts become unlocked.
Switching Auto off unlocks non-placed parts; switching it on applies the same
Auto-lock operation again. `Lock` directly changes the selected parts' single
deformation Lock. Placed parts remain outside the runtime regardless of Lock.
Unresolvable paths remain pending. A newly moved part resolves every valid
connection available among the current participants, including one side of a
multipart label whose other side is still placed.

Undoable state stores seam pairs, the fixed seam state, velocity, revision,
backend, runtime epoch, and Object Mode matrices. Recovery is limited to the
current add-on runtime. A click is rolled back only if the returned particle
state is non-finite; there is no finite-distance movement threshold.
