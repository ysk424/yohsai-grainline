# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import yohsai_svg_parser as parser


MINIMAL_SVG = """<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 300 200">
  <g id="CLOTHES">
    <path id="front" d="M 100 100 L 200 100 C 210 100 210 150 200 150 L 100 150 Z"/>
    <text transform="matrix(1 0 0 1 150 95)">A</text>
    <text transform="matrix(1 0 0 1 95 125)">@W</text>
    <text x="50" y="25">@s100cm</text>
    <path d="M 0 20 H 100"/>
  </g>
</svg>
"""


class SvgParserTests(unittest.TestCase):
    def _parse_text(self, svg: str) -> dict:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "input.svg"
            path.write_text(svg, encoding="utf-8")
            return parser.parse_svg(path)

    def test_minimal_document_preserves_cubic_and_uses_meters(self) -> None:
        document = self._parse_text(MINIMAL_SVG)
        self.assertEqual(document["schema"], "yohsai-pattern")
        self.assertEqual(document["units"], "m")
        self.assertAlmostEqual(document["scale"]["meters_per_svg_unit"], 0.01)
        self.assertEqual(len(document["panels"]), 1)
        self.assertEqual(document["panels"][0]["id"], "front")
        self.assertIn("cubic", [segment["type"] for segment in document["panels"][0]["segments"]])
        self.assertEqual(len(document["sewing_groups"]["A"]), 1)
        self.assertEqual(sum(segment["fold"] for segment in document["panels"][0]["segments"]), 1)

    def test_atomic_fixed_output_replaces_previous_document(self) -> None:
        document = self._parse_text(MINIMAL_SVG)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / parser.OUTPUT_FILENAME
            output.write_text('{"old": true}', encoding="utf-8")
            written = parser.write_fixed_output(document, directory)
            self.assertEqual(written, output)
            self.assertFalse((Path(directory) / parser.TEMP_FILENAME).exists())
            self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["schema"], "yohsai-pattern")

    def test_missing_clothes_layer_is_an_error(self) -> None:
        with self.assertRaisesRegex(parser.ParseError, "id='CLOTHES'"):
            self._parse_text(MINIMAL_SVG.replace('id="CLOTHES"', 'id="CLITHES"'))

    def test_supplied_test2(self) -> None:
        source = Path.home() / "Desktop" / "test2.svg"
        if not source.is_file():
            self.skipTest("The user-supplied Desktop/test2.svg is not available.")
        svg = source.read_text(encoding="utf-8").replace('id="CLITHES"', 'id="CLOTHES"', 1)
        document = self._parse_text(svg)
        self.assertEqual(len(document["panels"]), 2)
        self.assertAlmostEqual(document["scale"]["reference_length_m"], 0.3)
        self.assertEqual(len(document["sewing_groups"]["A"]), 2)
        self.assertEqual(len(document["sewing_groups"]["B"]), 4)
        self.assertNotIn("W", document["sewing_groups"])
        self.assertEqual(
            sum(segment["fold"] for panel in document["panels"] for segment in panel["segments"]),
            2,
        )


if __name__ == "__main__":
    unittest.main()
