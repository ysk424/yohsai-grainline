# SPDX-License-Identifier: GPL-3.0-or-later
"""ctypes bridge for the versioned Stable Cosserat native solver C ABI."""

from __future__ import annotations

import ctypes
import os
from pathlib import Path

import numpy as np


API_VERSION = 2
_ERROR_CAPACITY = 1024


class NativeCosseratError(RuntimeError):
    """The native Stable Cosserat backend cannot be loaded or advanced."""


FloatPointer = ctypes.POINTER(ctypes.c_float)
IntPointer = ctypes.POINTER(ctypes.c_int32)


class _Config(ctypes.Structure):
    _fields_ = [
        ("time_step", ctypes.c_float),
        ("substeps", ctypes.c_int32),
        ("iterations", ctypes.c_int32),
        ("stretch_stiffness", ctypes.c_float),
        ("bend_stiffness", ctypes.c_float),
        ("quad_shear_stiffness", ctypes.c_float),
        ("quad_area_stiffness", ctypes.c_float),
        ("straight_pair_cosine", ctypes.c_float),
        ("seam_projection_passes", ctypes.c_int32),
        ("velocity_damping_per_second", ctypes.c_float),
        ("maximum_speed", ctypes.c_float),
        ("maximum_position_correction", ctypes.c_float),
        ("contact_thickness", ctypes.c_float),
    ]


class _CreateDesc(ctypes.Structure):
    _fields_ = [
        ("vertex_count", ctypes.c_int32),
        ("positions", FloatPointer),
        ("velocities", FloatPointer),
        ("rest_frame_positions", FloatPointer),
        ("material_rest_positions", FloatPointer),
        ("inverse_masses", FloatPointer),
        ("locked", IntPointer),
        ("edge_count", ctypes.c_int32),
        ("edges", IntPointer),
        ("edge_rest_lengths", FloatPointer),
        ("quad_count", ctypes.c_int32),
        ("quads", IntPointer),
        ("seam_count", ctypes.c_int32),
        ("seams", IntPointer),
        ("face_count", ctypes.c_int32),
        ("faces", IntPointer),
        ("body_vertex_count", ctypes.c_int32),
        ("body_positions", FloatPointer),
        ("body_face_count", ctypes.c_int32),
        ("body_faces", IntPointer),
    ]


class _AdvanceDesc(ctypes.Structure):
    _fields_ = [
        ("gravity", ctypes.c_float * 3),
        ("seam_closure", ctypes.c_float),
        ("iterations", ctypes.c_int32),
        ("body_candidate_count", ctypes.c_int32),
        ("body_candidates", IntPointer),
        ("self_candidate_count", ctypes.c_int32),
        ("self_candidates", IntPointer),
    ]


class _Stats(ctypes.Structure):
    _fields_ = [
        ("substeps", ctypes.c_int32),
        ("iterations", ctypes.c_int32),
        ("segment_count", ctypes.c_int32),
        ("angle_count", ctypes.c_int32),
        ("quad_count", ctypes.c_int32),
        ("body_candidate_count", ctypes.c_int32),
        ("self_candidate_count", ctypes.c_int32),
        ("maximum_displacement", ctypes.c_float),
        ("maximum_edge_strain", ctypes.c_float),
        ("stretch_energy", ctypes.c_float),
        ("bend_energy", ctypes.c_float),
        ("shear_energy", ctypes.c_float),
        ("area_energy", ctypes.c_float),
    ]


def _float_array(values, shape: tuple[int, ...], name: str) -> np.ndarray:
    result = np.ascontiguousarray(values, dtype=np.float32)
    if result.size == 0 and np.prod(shape, dtype=np.int64) == 0:
        result = np.empty(shape, dtype=np.float32)
    if result.shape != shape or not np.all(np.isfinite(result)):
        raise NativeCosseratError(f"{name} must be a finite float32 array with shape {shape}.")
    return result


def _int_array(values, columns: int, name: str) -> np.ndarray:
    result = np.ascontiguousarray(values, dtype=np.int32)
    if result.size == 0:
        return np.empty((0, columns), dtype=np.int32)
    expected = (len(result), columns)
    if result.shape != expected:
        raise NativeCosseratError(f"{name} must be an int32 array with shape (N, {columns}).")
    return result


def _float_pointer(values: np.ndarray) -> FloatPointer:
    return values.ctypes.data_as(FloatPointer)


def _int_pointer(values: np.ndarray) -> IntPointer:
    return values.ctypes.data_as(IntPointer)


def _candidate_array(values, name: str) -> np.ndarray:
    result = np.ascontiguousarray(values, dtype=np.int32)
    if result.size == 0:
        return np.empty((0, 2), dtype=np.int32)
    if result.ndim != 2 or result.shape[1] != 2:
        raise NativeCosseratError(f"{name} must have shape (N, 2).")
    return result


def _library_candidates() -> tuple[Path, ...]:
    root = Path(__file__).resolve().parent
    explicit = os.environ.get("YOHSAI_COSSERAT_DLL", "").strip()
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit).expanduser())
    candidates.extend(
        (
            root / "bin" / "yohsai_cosserat.dll",
            root / "build" / "bin" / "Release" / "yohsai_cosserat.dll",
            root / "build" / "bin" / "Debug" / "yohsai_cosserat.dll",
        )
    )
    return tuple(candidates)


def _load_library() -> ctypes.CDLL:
    attempted: list[str] = []
    for path in _library_candidates():
        attempted.append(str(path))
        if not path.is_file():
            continue
        try:
            library = ctypes.CDLL(str(path))
        except OSError as exc:
            raise NativeCosseratError(f"Cannot load Stable Cosserat library {path}: {exc}") from exc
        _configure_library(library)
        version = int(library.ysc_get_api_version())
        if version != API_VERSION:
            raise NativeCosseratError(
                f"Stable Cosserat library API {version} does not match the Yohsai API {API_VERSION}."
            )
        return library
    raise NativeCosseratError(
        "Stable Cosserat native library was not found. Build it with build_native.ps1. "
        f"Searched: {', '.join(attempted)}"
    )


def _configure_library(library: ctypes.CDLL) -> None:
    library.ysc_get_api_version.argtypes = []
    library.ysc_get_api_version.restype = ctypes.c_int32
    library.ysc_default_config.argtypes = [ctypes.POINTER(_Config)]
    library.ysc_default_config.restype = ctypes.c_int32
    library.ysc_create.argtypes = [
        ctypes.POINTER(_CreateDesc),
        ctypes.POINTER(_Config),
        ctypes.POINTER(ctypes.c_void_p),
        ctypes.c_char_p,
        ctypes.c_int32,
    ]
    library.ysc_create.restype = ctypes.c_int32
    library.ysc_destroy.argtypes = [ctypes.c_void_p]
    library.ysc_destroy.restype = None
    library.ysc_get_counts.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.POINTER(ctypes.c_int32),
        ctypes.c_char_p,
        ctypes.c_int32,
    ]
    library.ysc_get_counts.restype = ctypes.c_int32
    library.ysc_replace_state.argtypes = [
        ctypes.c_void_p,
        FloatPointer,
        FloatPointer,
        IntPointer,
        ctypes.c_int32,
        ctypes.c_char_p,
        ctypes.c_int32,
    ]
    library.ysc_replace_state.restype = ctypes.c_int32
    library.ysc_copy_state.argtypes = [
        ctypes.c_void_p,
        FloatPointer,
        FloatPointer,
        ctypes.c_char_p,
        ctypes.c_int32,
    ]
    library.ysc_copy_state.restype = ctypes.c_int32
    for name in ("ysc_replace_orientations", "ysc_copy_orientations"):
        function = getattr(library, name)
        function.argtypes = [ctypes.c_void_p, FloatPointer, ctypes.c_char_p, ctypes.c_int32]
        function.restype = ctypes.c_int32
    for name in ("ysc_replace_seam_state", "ysc_copy_seam_state"):
        function = getattr(library, name)
        function.argtypes = [ctypes.c_void_p, FloatPointer, ctypes.c_char_p, ctypes.c_int32]
        function.restype = ctypes.c_int32
    library.ysc_advance.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(_AdvanceDesc),
        ctypes.POINTER(_Stats),
        ctypes.c_char_p,
        ctypes.c_int32,
    ]
    library.ysc_advance.restype = ctypes.c_int32


_library: ctypes.CDLL | None = None


def native_library_available() -> bool:
    """Return whether a compatible DLL can be loaded without creating a simulation."""
    try:
        _get_library()
        return True
    except NativeCosseratError:
        return False


def _get_library() -> ctypes.CDLL:
    global _library
    if _library is None:
        _library = _load_library()
    return _library


class NativeCosseratRuntime:
    """Own one native solver handle and expose the existing Kitsuke runtime contract."""

    def __init__(
        self,
        positions,
        velocities,
        rest_frame_positions,
        material_rest_positions,
        edges,
        edge_rest,
        quads,
        seams,
        faces,
        body,
        locked,
    ):
        self._library = _get_library()
        self._handle = ctypes.c_void_p()
        self.vertex_count = int(len(positions))
        self.segment_count = int(len(edges))
        self.quad_count = int(len(quads))
        self.seam_count = int(len(seams))

        positions_array = _float_array(positions, (self.vertex_count, 3), "positions")
        velocities_array = _float_array(velocities, (self.vertex_count, 3), "velocities")
        rest_array = _float_array(rest_frame_positions, (self.vertex_count, 3), "rest frame positions")
        material_rest_array = _float_array(
            material_rest_positions, (self.vertex_count, 3), "material rest positions"
        )
        edges_array = _int_array(edges, 2, "edges")
        edge_rest_array = _float_array(edge_rest, (self.segment_count,), "edge rest lengths")
        quads_array = _int_array(quads, 4, "quads")
        seams_array = _int_array(seams, 2, "seams")
        faces_array = _int_array(faces, 3, "faces")
        body_vertices = _float_array(body.vertices, (len(body.vertices), 3), "Body vertices")
        body_faces = _int_array(body.faces, 3, "Body faces")
        locked_array = np.ascontiguousarray(locked, dtype=np.int32)
        if locked_array.shape != (self.vertex_count,):
            raise NativeCosseratError(f"locked must have shape ({self.vertex_count},).")
        inverse_masses = np.ones(self.vertex_count, dtype=np.float32)

        config = _Config()
        if self._library.ysc_default_config(ctypes.byref(config)) != 0:
            raise NativeCosseratError("Native solver did not provide a default configuration.")

        desc = _CreateDesc(
            self.vertex_count,
            _float_pointer(positions_array),
            _float_pointer(velocities_array),
            _float_pointer(rest_array),
            _float_pointer(material_rest_array),
            _float_pointer(inverse_masses),
            _int_pointer(locked_array),
            self.segment_count,
            _int_pointer(edges_array),
            _float_pointer(edge_rest_array),
            self.quad_count,
            _int_pointer(quads_array),
            self.seam_count,
            _int_pointer(seams_array),
            len(faces_array),
            _int_pointer(faces_array),
            len(body_vertices),
            _float_pointer(body_vertices),
            len(body_faces),
            _int_pointer(body_faces),
        )
        self._call("ysc_create", ctypes.byref(desc), ctypes.byref(config), ctypes.byref(self._handle))
        if not self._handle:
            raise NativeCosseratError("Native solver returned no handle.")

        vertex_count = ctypes.c_int32()
        segment_count = ctypes.c_int32()
        angle_count = ctypes.c_int32()
        quad_count = ctypes.c_int32()
        seam_count = ctypes.c_int32()
        self._call(
            "ysc_get_counts",
            self._handle,
            ctypes.byref(vertex_count),
            ctypes.byref(segment_count),
            ctypes.byref(angle_count),
            ctypes.byref(quad_count),
            ctypes.byref(seam_count),
        )
        if (
            vertex_count.value != self.vertex_count
            or segment_count.value != self.segment_count
            or quad_count.value != self.quad_count
            or seam_count.value != self.seam_count
        ):
            self.close()
            raise NativeCosseratError("Native solver count validation failed.")
        self.angle_count = int(angle_count.value)
        self.last_stats: dict[str, float | int] = {}

    def _call(self, function_name: str, *arguments) -> None:
        if function_name != "ysc_create" and not self._handle:
            raise NativeCosseratError("Stable Cosserat runtime is closed.")
        error = ctypes.create_string_buffer(_ERROR_CAPACITY)
        status = int(getattr(self._library, function_name)(*arguments, error, _ERROR_CAPACITY))
        if status != 0:
            message = error.value.decode("utf-8", errors="replace").strip()
            raise NativeCosseratError(message or f"{function_name} failed with native status {status}.")

    def close(self) -> None:
        if getattr(self, "_handle", None):
            self._library.ysc_destroy(self._handle)
            self._handle = ctypes.c_void_p()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def replace_state(self, positions, velocities, locked, *, reinitialize_orientations: bool = True) -> None:
        positions_array = _float_array(positions, (self.vertex_count, 3), "positions")
        velocities_array = _float_array(velocities, (self.vertex_count, 3), "velocities")
        locked_array = np.ascontiguousarray(locked, dtype=np.int32)
        if locked_array.shape != (self.vertex_count,):
            raise NativeCosseratError(f"locked must have shape ({self.vertex_count},).")
        self._call(
            "ysc_replace_state",
            self._handle,
            _float_pointer(positions_array),
            _float_pointer(velocities_array),
            _int_pointer(locked_array),
            1 if reinitialize_orientations else 0,
        )

    def state(self) -> tuple[np.ndarray, np.ndarray]:
        positions = np.empty((self.vertex_count, 3), dtype=np.float32)
        velocities = np.empty_like(positions)
        self._call(
            "ysc_copy_state",
            self._handle,
            _float_pointer(positions),
            _float_pointer(velocities),
        )
        return positions, velocities

    def orientation_state(self) -> np.ndarray:
        orientations = np.empty((self.segment_count, 4), dtype=np.float32)
        self._call("ysc_copy_orientations", self._handle, _float_pointer(orientations))
        return orientations

    def replace_orientation_state(self, orientations) -> None:
        values = _float_array(orientations, (self.segment_count, 4), "orientations")
        self._call("ysc_replace_orientations", self._handle, _float_pointer(values))

    def seam_state(self) -> np.ndarray:
        values = np.empty(self.seam_count, dtype=np.float32)
        self._call("ysc_copy_seam_state", self._handle, _float_pointer(values))
        return values

    def replace_seam_state(self, values) -> None:
        state = _float_array(values, (self.seam_count,), "seam state")
        self._call("ysc_replace_seam_state", self._handle, _float_pointer(state))

    def advance(
        self,
        body_candidates,
        self_candidates,
        gravity_magnitude: float,
        seam_closure: float,
        solver_iterations: int,
    ) -> None:
        body = _candidate_array(body_candidates, "Body candidates")
        self_contact = _candidate_array(self_candidates, "self-contact candidates")
        desc = _AdvanceDesc(
            (ctypes.c_float * 3)(0.0, 0.0, -float(gravity_magnitude)),
            float(seam_closure),
            int(solver_iterations),
            len(body),
            _int_pointer(body),
            len(self_contact),
            _int_pointer(self_contact),
        )
        stats = _Stats()
        self._call("ysc_advance", self._handle, ctypes.byref(desc), ctypes.byref(stats))
        self.last_stats = {name: getattr(stats, name) for name, _ctype in stats._fields_}
