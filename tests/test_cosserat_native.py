# SPDX-License-Identifier: GPL-3.0-or-later
"""Host-Python smoke tests for the Stable Cosserat DLL bridge."""

from __future__ import annotations

from types import SimpleNamespace
import unittest

import numpy as np

import cosserat_native


@unittest.skipUnless(cosserat_native.native_library_available(), "Stable Cosserat DLL is not built")
class NativeCosseratBridgeTests(unittest.TestCase):
    def runtime(self, locked=(0, 0, 0)):
        positions = np.asarray(((0, 0, 0), (1, 0, 0), (2, 0, 0)), dtype=np.float32)
        edges = np.asarray(((0, 1), (1, 2)), dtype=np.int32)
        body = SimpleNamespace(
            vertices=np.empty((0, 3), dtype=np.float32),
            faces=np.empty((0, 3), dtype=np.int32),
        )
        return cosserat_native.NativeCosseratRuntime(
            positions,
            np.zeros_like(positions),
            positions,
            positions,
            edges,
            np.ones(2, dtype=np.float32),
            np.empty((0, 4), dtype=np.int32),
            np.empty((0, 2), dtype=np.int32),
            np.empty((0, 3), dtype=np.int32),
            body,
            np.asarray(locked, dtype=np.int32),
        )

    def test_counts_state_and_unit_orientations(self):
        runtime = self.runtime()
        try:
            self.assertEqual(runtime.vertex_count, 3)
            self.assertEqual(runtime.segment_count, 2)
            self.assertEqual(runtime.angle_count, 1)
            self.assertEqual(runtime.quad_count, 0)
            runtime.advance(
                np.empty((0, 2), dtype=np.int32),
                np.empty((0, 2), dtype=np.int32),
                0.0,
                0.0,
                4,
            )
            positions, velocities = runtime.state()
            self.assertTrue(np.all(np.isfinite(positions)))
            self.assertTrue(np.all(np.isfinite(velocities)))
            orientations = runtime.orientation_state()
            self.assertTrue(np.allclose(np.linalg.norm(orientations, axis=1), 1.0, atol=1.0e-5))
            runtime.replace_orientation_state(-orientations)
            self.assertTrue(np.allclose(runtime.orientation_state(), -orientations, atol=1.0e-6))
        finally:
            runtime.close()

    def test_locked_vertices_can_be_unlocked_between_clicks(self):
        runtime = self.runtime((1, 1, 1))
        try:
            before, _velocities = runtime.state()
            runtime.replace_state(before, np.zeros_like(before), np.zeros(3, dtype=np.int32))
            runtime.advance(
                np.empty((0, 2), dtype=np.int32),
                np.empty((0, 2), dtype=np.int32),
                1.0,
                0.0,
                2,
            )
            after, _velocities = runtime.state()
            self.assertLess(float(after[:, 2].mean()), float(before[:, 2].mean()))
        finally:
            runtime.close()

    def test_quad_connectivity_and_energy_stats_cross_the_abi(self):
        positions = np.asarray(
            ((0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1)), dtype=np.float32
        )
        edges = np.asarray(((0, 1), (1, 2), (2, 3), (3, 0)), dtype=np.int32)
        body = SimpleNamespace(
            vertices=np.empty((0, 3), dtype=np.float32),
            faces=np.empty((0, 3), dtype=np.int32),
        )
        runtime = cosserat_native.NativeCosseratRuntime(
            positions,
            np.zeros_like(positions),
            positions,
            positions,
            edges,
            np.ones(4, dtype=np.float32),
            np.asarray(((0, 1, 2, 3),), dtype=np.int32),
            np.empty((0, 2), dtype=np.int32),
            np.asarray(((0, 1, 2), (0, 2, 3)), dtype=np.int32),
            body,
            np.zeros(4, dtype=np.int32),
        )
        try:
            self.assertEqual(runtime.quad_count, 1)
            runtime.advance(
                np.empty((0, 2), dtype=np.int32),
                np.empty((0, 2), dtype=np.int32),
                0.0,
                0.0,
                4,
            )
            self.assertEqual(runtime.last_stats["quad_count"], 1)
            self.assertLess(abs(float(runtime.last_stats["shear_energy"])), 1.0e-6)
            self.assertLess(abs(float(runtime.last_stats["area_energy"])), 1.0e-6)
        finally:
            runtime.close()


if __name__ == "__main__":
    unittest.main()
