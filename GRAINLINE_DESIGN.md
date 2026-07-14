# Yohsai Grainline Design

Status: grain-aligned quad-lattice fork boundary recorded; implementation not
yet started
Recorded: 2026-07-14 (Asia/Tokyo)

## 1. Authoritative garment convention

The project owner is an expert garment maker and pattern drafter. Product and
material decisions supplied by the owner are authoritative domain input, not
beginner assumptions that the software should reinterpret.

Every pattern is drafted with its grain aligned to the vertical direction of
the pattern paper. The warp direction runs from the top of the page to the
bottom. Yohsai Grainline therefore uses this fixed material convention:

- pattern-page vertical is warp;
- pattern-page horizontal is weft;
- no per-panel grain annotation or automatic grain inference is required;
- the first implementation does not treat bias cutting as a normal workflow.

The simulation lattice must preserve this authored page orientation. Object
placement in Blender changes the garment pose, not its material grain.

## 2. Fork objective

The validated Stable Cosserat v0.4.1 implementation is retained as the
behavioral and numerical baseline. The new solver will replace its internal
three-direction triangular rod network with a structured square lattice in
pattern space:

- vertical lattice edges form warp Cosserat chains;
- horizontal lattice edges form weft Cosserat chains;
- quad cells supply explicit in-plane shear and area response;
- triangulated faces remain a collision, Blender, and rendering proxy;
- proxy diagonals are not structural Cosserat segments.

The interior should remain a regular square grid. Arbitrary authored panel
boundaries may require clipped boundary cells or a narrow quad-dominant
transition, but boundary treatment must not rotate or reinterpret the material
grain.

## 3. Product invariants retained from Yohsai Cosserat

The Illustrator PDF pattern remains authoritative. `Load`, `Update`, `Sewing`,
manual Object Mode placement, Lock, incremental Kitsuke, Undo/Redo, progressive
seam closure, Body contact, and self-contact remain in scope. The first
grainline milestone changes the internal material discretization rather than
the established garment-construction workflow.

The existing `yohsai-cosserat` repository remains the trusted triangular
baseline for A/B comparison. This repository carries its complete history and
continues development under GPL-3.0-or-later.
