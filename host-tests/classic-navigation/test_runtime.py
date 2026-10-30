#!/usr/bin/env python3
"""Physical-key consumer regression against the actual Classic simulator.

Run with: uv run --with pillow python host-tests/classic-navigation/test_runtime.py
  --output qa-artifacts/classic-navigation/after
Use --program to exercise a preserved pre-fix binary. No hardware or touch input.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
PARSER = argparse.ArgumentParser(description=__doc__)
PARSER.add_argument("--program", type=Path, default=ROOT / ".pio/build/simulator_x4_classic/program")
PARSER.add_argument("--output", type=Path, required=True)
ARGS = PARSER.parse_args()
PROGRAM = ARGS.program.resolve()
OUTPUT = ARGS.output.resolve()
OUTPUT.mkdir(parents=True, exist_ok=True)
RESULTS = {}


def run(name, keys, shots, seed, work):
    """Each process receives an independent copy of a real simulator SD fixture."""
    target = OUTPUT / name
    target.mkdir(exist_ok=True)
    card = work / name
    shutil.copytree(seed, card)
    script = ";".join(f"{at}:{key}" for at, key in keys)
    captures = ";".join(f"{at}:{target / label}.bmp" for at, label in shots)
    env = dict(os.environ, CROSSPOINT_SIM_SD=str(card), CROSSPOINT_SIM_HIDDEN="1",
               SDL_MAC_BACKGROUND_APP="1", CROSSPOINT_SIM_INPUT_SCRIPT=script,
               CROSSPOINT_SIM_SCREENSHOTS=captures)
    (target / "input.txt").write_text(script + "\n")
    process = subprocess.run([str(PROGRAM)], cwd=ROOT, env=env, capture_output=True, timeout=45)
    log = process.stdout.decode(errors="replace") + process.stderr.decode(errors="replace")
    (target / "runtime.log").write_text(log)
    assert process.returncode == 0, (name, process.returncode)
    assert "classic=1 touch=0 frontlight=0 home=0" in log, "not the Classic no-touch program"
    for _, label in shots:
        bmp = target / f"{label}.bmp"
        with Image.open(bmp) as image:
            image.convert("RGB").save(bmp.with_suffix(".png"))
        bmp.unlink()
    return card, log


def image(name, label):
    return Image.open(OUTPUT / name / f"{label}.png").convert("L")


def row_selected(name, label, row):
    # Probe the blank left gutter of the rendered rows, away from text/icons.
    # The active list row must be inverted, not an invisible interaction cursor.
    im = image(name, label)
    return im.getpixel((48, 210 + 132 * row)) < 64


def book_selected(name, label):
    # The framed book's inset outline is separate from the cover and menu.
    im = image(name, label)
    return im.getpixel((44, 240)) < 64


def reader_opened(log, path, after):
    for entry in re.finditer(r"^\[(\d+)\].*Entering activity: EpubReader$", log, re.MULTILINE):
        if int(entry.group(1)) <= after:
            continue
        segment = log[entry.end():].split("Entering activity:", 1)[0]
        if any(line.endswith("Loaded ePub: " + path) for line in segment.splitlines()):
            return True
    return False


def check(name, condition):
    RESULTS[name] = bool(condition)
    print(f"{'PASS' if condition else 'FAIL'} {name}", flush=True)


with tempfile.TemporaryDirectory(prefix="classic-navigation-") as temporary:
    work = Path(temporary)
    empty = work / "empty"
    (empty / "books").mkdir(parents=True)
    (empty / ".crosspoint").mkdir()
    shutil.copy2(ROOT / "test/epubs/test_reader_menu_progress.epub", empty / "books/Navigation.epub")
    # Populate RecentBooksStore through FileBrowser/reader, not fabricated JSON.
    opened, reader_log = run("seed-reader", [(800, "ENTER"), (1800, "ENTER"), (2800, "ENTER"),
                              (4300, "BACK"), (5600, "QUIT")], [(3900, "reader"), (5100, "home")], empty, work)
    assert "Loaded ePub: /books/Navigation.epub" in reader_log
    assert reader_log.rfind("Entering activity: Home") > reader_log.find("Entering activity: EpubReader")

    for folder, steps, expected in [("games", 4, "Checkers"), ("apps", 5, "Pet")]:
        entry = [(800 + n * 300, "DOWN") for n in range(steps)] + [(2400, "ENTER")]
        keys = entry + [(3400, "RIGHT"), (4400, "LEFT"), (5400, "RIGHT"), (6400, "ENTER"), (7800, "QUIT")]
        _, log = run(folder, keys, [(3100, "initial"), (4000, "next"), (5000, "previous"), (7200, "opened")], empty, work)
        check(f"{folder}-front-row-and-open", row_selected(folder, "initial", 0) and
              row_selected(folder, "next", 1) and not row_selected(folder, "next", 0) and
              row_selected(folder, "previous", 0) and f"Entering activity: {expected}" in log)

    entry = [(800 + n * 300, "DOWN") for n in range(4)] + [(2100, "ENTER")]
    keys = entry + [(3000 + n * 220, "RIGHT:25") for n in range(9)] + [(5700, "LEFT"), (6900, "QUIT")]
    run("pages", keys, [(5400, "second-page"), (6400, "first-page-last-row")], empty, work)
    check("front-crosses-pages", row_selected("pages", "second-page", 0) and
          row_selected("pages", "first-page-last-row", 8))

    keys = entry + [(3000 + n * 220, "RIGHT:25") for n in range(16)] + [
        (7200, "ENTER"), (8300, "ENTER"), (9700, "QUIT")]
    _, log = run("pager", keys, [(6900, "focused"), (8000, "previous-page")], empty, work)
    check("pager-control-reachable", "Page 1 -> 0" in log and
          row_selected("pager", "previous-page", 0) and "Entering activity: Chess" in log)

    keys = entry + [(3000, "LEFT"), (3900, "ENTER"), (4800, "RIGHT"), (5700, "ENTER"),
                    (6600, "BACK"), (7500, "ENTER"), (8900, "QUIT")]
    _, log = run("chooser", keys, [(3500, "chooser-focus"), (4400, "choosing"),
                                    (6200, "hidden"), (7100, "browse")], empty, work)
    # Inspect contrast over the header gutter, not a rasterizer's exact edge pixel.
    header_gutter = image("chooser", "chooser-focus").crop((0, 16, 32, 120))
    check("chooser-hide-and-return", sum(value > 128 for value in header_gutter.tobytes()) > 100 and
          "Entering activity: Checkers" in log)

    keys = entry + [(3000, "DOWN"), (4000, "UP"), (5000, "DOWN:450"), (6500, "QUIT")]
    run("sides", keys, [(3700, "next"), (4700, "previous"), (6100, "page")], empty, work)
    check("side-short-and-held", row_selected("sides", "next", 1) and
          row_selected("sides", "previous", 0) and row_selected("sides", "page", 0) and
          image("sides", "previous").crop((120, 180, 700, 315)).tobytes() !=
          image("sides", "page").crop((120, 180, 700, 315)).tobytes())

    keys = entry + [(3000, "LEFT"), (3900, "UP:450"), (5200, "ENTER"), (6600, "QUIT")]
    _, log = run("clamped-side", keys, [(3500, "chrome"), (4900, "row")], empty, work)
    check("clamped-side-repaints-row", row_selected("clamped-side", "row", 0) and
          "Entering activity: Chess" in log)

    keys = entry + [(3000, "LEFT"), (3800, "ENTER"), (4600, "RIGHT")]
    for n in range(16):
        keys.append((5000 + n * 420, "ENTER"))
        if n < 15:
            keys.append((5200 + n * 420, "RIGHT"))
    keys += [(12000, "BACK"), (12800, "BACK"), (13600, "ENTER"), (14600, "RIGHT"),
             (15500, "ENTER"), (16400, "RIGHT"), (17300, "ENTER"), (18200, "BACK"),
             (19100, "ENTER"), (20500, "QUIT")]
    _, log = run("all-hidden", keys, [(14300, "empty"), (15100, "chooser"), (18800, "restored")], empty, work)
    check("all-hidden-recovery", row_selected("all-hidden", "restored", 0) and
          "Entering activity: Chess" in log and log.count("Entering activity: ShelfFolder") == 2)

    _, log = run("continue", [(1000, "LEFT"), (2300, "ENTER"), (4200, "QUIT")],
                 [(700, "initial"), (1800, "focused"), (3600, "opened")], opened, work)
    check("continue-focus-and-matching-book", not book_selected("continue", "initial") and
          book_selected("continue", "focused") and reader_opened(log, "/books/Navigation.epub", 2300))

    run("clear-focus", [(1000, "LEFT"), (2200, "RIGHT"), (3900, "QUIT")],
        [(1800, "focused"), (3100, "menu")], opened, work)
    check("continue-focus-clears", book_selected("clear-focus", "focused") and
          not book_selected("clear-focus", "menu"))

    for name, seed in [("no-recent", empty), ("missing-recent", opened)]:
        missing = work / (name + "-seed")
        shutil.copytree(seed, missing)
        if name == "missing-recent":
            (missing / "books/Navigation.epub").unlink()
        _, log = run(name, [(1000, "LEFT"), (2200, "ENTER"), (3600, "QUIT")],
                     [(1800, "menu")], missing, work)
        check(name, "Entering activity: Settings" in log and "Entering activity: EpubReader" not in log)

    remapped = work / "remapped-seed"
    shutil.copytree(empty, remapped)
    (remapped / ".crosspoint/settings.json").write_text(json.dumps({
        "frontButtonBack": 2, "frontButtonConfirm": 3, "frontButtonLeft": 0, "frontButtonRight": 1,
        "frontButtonFollowOrientation": True,
    }))
    keys = [(800 + n * 300, "DOWN") for n in range(4)] + [(2100, "RIGHT"),
             (3100, "ENTER"), (4100, "BACK"), (5100, "ENTER"), (6100, "RIGHT"), (7500, "QUIT")]
    _, log = run("remap", keys, [(3800, "next"), (4800, "previous"), (6900, "opened")], remapped, work)
    check("remapped-front-roles", row_selected("remap", "next", 1) and
          row_selected("remap", "previous", 0) and "Entering activity: Checkers" in log)

    second_seed = work / "second-book-seed"
    shutil.copytree(opened, second_seed)
    shutil.copy2(ROOT / "test/epubs/test_supsub.epub", second_seed / "books/Second.epub")
    multi, log = run("seed-second", [(800, "ENTER"), (1800, "ENTER"), (2400, "DOWN"), (2800, "ENTER"),
                                    (4300, "BACK"), (5600, "QUIT")], [(5100, "home")], second_seed, work)
    assert reader_opened(log, "/books/Second.epub", 2800)
    for name, settings, lefts, expected in [
        ("carousel", {"homeLayout": 1}, 1, "/books/Second.epub"),
        ("cover-grid", {"homeLayout": 0, "uiTheme": 4}, 2, "/books/Second.epub"),
        ("cover-grid-second", {"homeLayout": 0, "uiTheme": 4}, 1, "/books/Navigation.epub"),
    ]:
        seed = work / (name + "-seed")
        shutil.copytree(multi, seed)
        (seed / ".crosspoint/settings.json").write_text(json.dumps(settings))
        # Walk book/menu boundaries in both directions before selecting a book.
        keys = [(1000, "LEFT"), (1500, "RIGHT"), (2000, "RIGHT"), (2500, "LEFT")]
        keys += [(3000 + n * 500, "LEFT") for n in range(lefts)]
        keys += [(4600, "ENTER"), (6300, "QUIT")]
        _, log = run(name, keys, [(700, "initial"), (1300, "book"), (1800, "menu"),
                                  (4200, "selected"), (5700, "reader")], seed, work)
        # Clock/status is excluded; book/menu focus must change and clear.
        initial = image(name, "initial").crop((0, 160, 960, 1560)).tobytes()
        focused = image(name, "book").crop((0, 160, 960, 1560)).tobytes()
        restored = image(name, "menu").crop((0, 160, 960, 1560)).tobytes()
        check(name + "-focus-path", initial != focused and initial == restored and
              reader_opened(log, expected, 4600))

report = {"program": str(PROGRAM), "sha256": hashlib.sha256(PROGRAM.read_bytes()).hexdigest(), "checks": RESULTS}
(OUTPUT / "results.json").write_text(json.dumps(report, indent=2) + "\n")
raise SystemExit(0 if all(RESULTS.values()) else 1)
