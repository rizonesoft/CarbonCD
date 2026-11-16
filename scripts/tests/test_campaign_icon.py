"""Tests of `scripts/campaign-icon.py`: a campaign launcher's icon, the project's icon with its runner's badge."""
from __future__ import annotations

import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "campaign-icon.py"
_spec = importlib.util.spec_from_file_location("campaign_icon", SCRIPT)
assert _spec is not None and _spec.loader is not None
icon = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(icon)
TILE = (16, 34, 26)  # a dark green tile, like the SDK's own


def windowless():
    """No console window of its own for any process a test starts on Windows (a console child of a parent with
    none opens one over the owner's work: 2026-11-12, Office's launcher tests flashed 5 to 10); 0 elsewhere."""
    return subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0


def solid(size, colour=TILE):
    return bytearray(bytes((*colour, 255)) * (size * size))


def pixel(rgba, size, x, y):
    o = (y * size + x) * 4
    return tuple(rgba[o:o + 4])


def close(a, b, tolerance=6):
    return all(abs(p - q) <= tolerance for p, q in zip(a, b))


class PngAndIcoTest(unittest.TestCase):
    def test_a_png_reads_back_as_written(self):
        rgba = bytearray(range(256)) * (8 * 8 * 4 // 256)
        width, height, back = icon.read_png(icon.write_png(8, 8, rgba))
        self.assertEqual((width, height, back), (8, 8, rgba))

    def test_an_icon_holds_every_frame_smallest_first(self):
        frames = [(size, icon.write_png(size, size, solid(size))) for size in (256, 16, 48)]
        data = icon.pack_ico(frames)
        read = icon.read_ico(data)
        self.assertEqual(sorted(read), [16, 48, 256])
        self.assertEqual(read[48][2], solid(48))
        self.assertEqual(data[6], 16, "the first entry is the smallest frame")
        self.assertEqual(data[6 + 32], 0, "a 256-pixel frame is written as 0")

    def test_area_resizing_keeps_a_solid_colour_and_brings_no_dark_fringe(self):
        self.assertEqual(icon.resize(64, 64, solid(64), 16), solid(16))
        half = bytearray(bytes((255, 255, 255, 255)) * 32 + bytes((0, 0, 0, 0)) * 32) * 64  # white left, clear right
        edge = pixel(icon.resize(64, 64, half, 16), 16, 7, 8)
        self.assertEqual(edge[:3], (255, 255, 255), "a transparent neighbour darkened the edge")


class PathTest(unittest.TestCase):
    def bounds(self, name):
        points = [p for poly in icon.flatten_path(icon.MARKS[name][1]) for p in poly]
        xs, ys = [p[0] for p in points], [p[1] for p in points]
        return min(xs), min(ys), max(xs), max(ys)

    def test_each_mark_fills_its_own_box(self):
        for name, size in (("claude", 100), ("gemini", 65)):
            with self.subTest(mark=name):
                left, top, right, bottom = self.bounds(name)
                self.assertGreaterEqual(min(left, top), -0.5)
                self.assertLessEqual(max(right, bottom), size + 0.5)
                self.assertGreater(right - left, size * 0.9)

    def test_arcs_with_packed_flags_are_read(self):
        # "a1 1 0 001 1" is a quarter circle whose flags (0 0) run into the next number.
        polys = icon.flatten_path("M0 0a1 1 0 001 1z")
        self.assertAlmostEqual(polys[0][-1][0], 1.0, places=6)
        self.assertAlmostEqual(polys[0][-1][1], 1.0, places=6)

    def test_a_square_covers_its_pixels_exactly(self):
        cover = icon.coverage([[(1, 1), (3, 1), (3, 3), (1, 3)]], 4, 1, 0, 0)
        self.assertEqual([round(c, 3) for c in cover[4:8]], [0, 1, 1, 0])
        self.assertEqual(round(sum(cover), 3), 4)


class BadgeTest(unittest.TestCase):
    def badge(self, runner, size):
        fill, mark = icon.RUNNERS[runner]
        return icon.badge_frame(size, solid(size), fill, mark)

    def test_the_badge_sits_bottom_right_in_the_runners_colour_and_leaves_the_rest(self):
        for runner, colour in (("claude", (0xD9, 0x77, 0x57)), ("agy", (0x4C, 0x8D, 0xF6)),
                               ("codex", (0x10, 0xA3, 0x7F))):
            with self.subTest(runner=runner):
                out = self.badge(runner, 64)
                self.assertTrue(close(pixel(out, 64, 4, 4), (*TILE, 255)), "the badge reached the top left")
                radius = 64 * 0.24
                centre = 64 - radius - 0.64
                # Between the mark (a box 1.15 radii wide) and the white ring: the runner's colour alone.
                inner = pixel(out, 64, round(centre - radius * 0.8), round(centre))
                self.assertTrue(close(inner, (*colour, 255), 12), (runner, inner))
                ring = pixel(out, 64, round(centre), round(centre + radius - 0.9))
                self.assertTrue(close(ring[:3], (255, 255, 255), 60), ("the white outline", ring))

    def test_a_mark_is_drawn_from_32_pixels_up_and_not_below(self):
        def whites(out, size):
            return sum(1 for i in range(size * size) if out[i * 4:i * 4 + 3] == bytearray((255, 255, 255)))
        plain_16, mark_16 = icon.badge_frame(16, solid(16), "#D97757", None), self.badge("claude", 16)
        self.assertEqual(plain_16, mark_16, "a mark was drawn at 16 pixels")
        plain_64, mark_64 = icon.badge_frame(64, solid(64), "#D97757", None), self.badge("claude", 64)
        self.assertGreater(whites(mark_64, 64), whites(plain_64, 64) + 20, "no mark at 64 pixels")
        codex = self.badge("codex", 64)
        plain = icon.badge_frame(64, solid(64), "#10A37F", None)
        self.assertGreater(whites(codex, 64), whites(plain, 64) + 20, "Codex must carry its logo")

    def test_the_command_writes_an_icon_with_every_size(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp) / "base.png"
            base.write_bytes(icon.write_png(256, 256, solid(256)))
            target = Path(tmp) / "out" / "claude.ico"
            done = subprocess.run([sys.executable, "-B", str(SCRIPT), "badge", str(base), "claude", str(target)],
                                  capture_output=True, text=True, timeout=600, creationflags=windowless())
            self.assertEqual(done.returncode, 0, done.stderr)
            self.assertEqual(sorted(icon.read_ico(target.read_bytes())), list(icon.SIZES))
            self.assertEqual([p.name for p in target.parent.iterdir()], ["claude.ico"], "a temporary file was left")
            bad = subprocess.run([sys.executable, "-B", str(SCRIPT), "badge", str(base), "gemini", str(target)],
                                 capture_output=True, text=True, timeout=60, creationflags=windowless())
            self.assertEqual(bad.returncode, 2)
            self.assertIn("usage: campaign-icon.py", bad.stderr)


if __name__ == "__main__":
    unittest.main()
