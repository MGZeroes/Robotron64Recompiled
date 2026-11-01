#!/usr/bin/env python3
"""Splits icons/app.svg into the two layers of an Android adaptive icon (called by tools/make_icons.sh).

A launcher cuts adaptive icons to its own shape and only shows the middle of a 108 dp canvas, so the background
has to fill the whole square and the robot has to be smaller.

Usage: make_android_icon_layers.py icons/app.svg <output folder>
"""

import re
import sys
from pathlib import Path

# The share of the canvas the robot keeps. The inner 66/108 is always visible; the robot's corners may go a
# little past that circle because they are empty.
ROBOT_SCALE = 0.65

svg = Path(sys.argv[1]).read_text(encoding="utf-8")
out = Path(sys.argv[2])

robot = re.search(r'<g transform="translate\(48 44\).*?\n  </g>', svg, re.S)
if robot is None:
    sys.exit("robot group not found in app.svg")
header = svg[:svg.index("<defs>")]

background = svg.replace(robot.group(0), "").replace('<g clip-path="url(#tile)">', "<g>")
(out / "android_background.svg").write_text(background, encoding="utf-8")

offset = 256 * (1 - ROBOT_SCALE)
foreground = f'{header}<g transform="translate({offset} {offset}) scale({ROBOT_SCALE})">\n{robot.group(0)}\n</g>\n</svg>\n'
(out / "android_foreground.svg").write_text(foreground, encoding="utf-8")
