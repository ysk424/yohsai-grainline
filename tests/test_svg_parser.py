# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path

import yohsai_svg_parser as parser


class SvgParserTests(unittest.TestCase):
    def test_svg_input_is_rejected(self) -> None:
        with self.assertRaisesRegex(parser.ParseError, "must be a .pdf"):
            parser.parse_pattern("input.svg")
        with self.assertRaisesRegex(parser.ParseError, "no longer supported"):
            parser.parse_svg("input.svg")

    @unittest.skipUnless(importlib.util.find_spec("pypdf"), "pypdf is not installed in this test interpreter")
    def test_supplied_illustrator_pdf(self) -> None:
        source = Path.home() / "Desktop" / "test2.pdf"
        if not source.is_file():
            self.skipTest("The user-supplied Desktop/test2.pdf is not available.")
        document = parser.parse_pdf(source)
        self.assertEqual(document["source"]["input_format"], "pdf")
        self.assertEqual([panel["label"] for panel in document["panels"]], ["OMOTE", "URA"])
        self.assertNotIn("annotation", document["scale"])
        self.assertNotIn("reference_length_m", document["scale"])
        self.assertNotIn("reference_length_svg", document["scale"])
        self.assertAlmostEqual(document["scale"]["meters_per_svg_unit"], 0.0254 / 72.0)
        self.assertEqual(len(document["sewing_groups"]["A"]), 2)
        self.assertEqual(len(document["sewing_groups"]["B"]), 4)
        self.assertEqual(
            sum(segment["fold"] for panel in document["panels"] for segment in panel["segments"]),
            2,
        )


if __name__ == "__main__":
    unittest.main()
