# SPDX-License-Identifier: GPL-3.0-or-later
"""Yohsai -- Blender clothing construction extension scaffold."""

from __future__ import annotations

from . import ui


def register():
    ui.register()


def unregister():
    ui.unregister()
