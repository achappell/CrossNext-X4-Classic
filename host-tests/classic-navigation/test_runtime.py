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
import struct
import tempfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
PARSER = argparse.ArgumentParser(description=__doc__)
PARSER.add_argument("--program", type=Path, default=ROOT / ".pio/build/simulator_x4_classic/program")
PARSER.add_argument("--output", type=Path, required=True)
PARSER.add_argument("--pro-program", type=Path, help="Also exercise shared touch/side behavior on the real Pro program")
ARGS = PARSER.parse_args()
PROGRAM = ARGS.program.resolve()
OUTPUT = ARGS.output.resolve()
OUTPUT.mkdir(parents=True, exist_ok=True)
RESULTS = {}


def run(name, keys, shots, seed, work, program=None):
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
    process = subprocess.run([str(program or PROGRAM)], cwd=ROOT, env=env, capture_output=True, timeout=45)
    log = process.stdout.decode(errors="replace") + process.stderr.decode(errors="replace")
    (target / "runtime.log").write_text(log)
    assert process.returncode == 0, (name, process.returncode)
    identity = "classic=0 touch=1 frontlight=1 home=1" if program else "classic=1 touch=0 frontlight=0 home=0"
    assert identity in log, "wrong simulator board/capabilities"
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

    # SectionNavigation starts at 350ms, then repeats at 300ms. Leave a normal
    # long hold enough margin for capture across the renderer's busy interval.
    keys = entry + [(3000, "DOWN"), (4000, "UP"), (5000, "DOWN:1200"), (7200, "QUIT")]
    _, sides_log = run("sides", keys, [(3700, "next"), (4700, "previous"), (6700, "page")], empty, work)
    check("side-short-and-held", row_selected("sides", "next", 1) and
          row_selected("sides", "previous", 0) and row_selected("sides", "page", 0) and
          image("sides", "previous").crop((120, 180, 700, 315)).tobytes() !=
          image("sides", "page").crop((120, 180, 700, 315)).tobytes())
    held_pages = [(int(at), int(target), int(count)) for at, target, count in re.findall(
        r"^\[(\d+)\].*\[SHELF\] Page \d+ -> (\d+) of (\d+)", sides_log, re.MULTILINE)
                  if 5000 <= int(at) <= 6700]
    check("side-held-repeats-within-page-bounds", len({at for at, _, _ in held_pages}) >= 2 and
          all(0 <= target < count for _, target, count in held_pages))

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

    # Feed the real completion-index consumer using the journal's public binary
    # format. These books have distinct content IDs, not two copies of one EPUB.
    completed_seed = work / "completed-seed"
    shutil.copytree(multi, completed_seed)
    journal = bytearray(bytes.fromhex("43525331800100000000000000000000"))
    ids = []
    epoch = 1793102400
    day = epoch // 86400
    for filename, title in [("Navigation.epub", "Reader Menu Progress"), ("Second.epub", "Second")]:
        data = (completed_seed / "books" / filename).read_bytes()
        document = hashlib.md5(b"".join(data[offset:offset + 1024] for offset in
                                      [0] + [1024 << (2 * n) for n in range(11)]
                                      if offset < len(data))).hexdigest()
        ids.append(document)
        for kind in ([1] if filename == "Navigation.epub" else [1, 2]):
            record = bytearray(384)
            record[:4] = b"RSE1"
            record[12], record[13] = int(kind == 1), kind
            struct.pack_into("<Hq", record, 14, 10000 if kind == 1 else 4500, epoch)
            record[104:136] = document.encode()
            record[137:137 + len(title)] = title.encode()
            record[329] = int(kind == 1)
            struct.pack_into("<IIII", record, 330, day, epoch % 86400,
                             day if kind == 1 else 0, day if kind == 2 else 0)
            record[346] = int(kind == 2)
            checksum = 2166136261
            for byte in record[:380]:
                checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
            struct.pack_into("<I", record, 380, checksum)
            journal.extend(record)
            epoch += 1000
    assert ids[0] != ids[1], "completion fixtures must have distinct document IDs"
    history = completed_seed / ".crosspoint/reading-history"
    history.mkdir(exist_ok=True)
    (history / "sessions.bin").write_bytes(journal)
    for display, name, moves, expected in [
        (7, "library-finished-visible", 2, "/books/Navigation.epub"),
        (6, "library-finished-hidden", 1, "/books/Second.epub"),
    ]:
        seed = work / (name + "-seed")
        shutil.copytree(completed_seed, seed)
        (seed / ".crosspoint/settings.json").write_text(json.dumps(
            {"libraryDisplay": display, "libraryClassicGrid": 0}))
        keys = [(800, "DOWN"), (1500, "ENTER")]
        keys += [(2700 + n * 500, "RIGHT") for n in range(moves)]
        keys += [(3900, "ENTER"), (5700, "QUIT")]
        _, log = run(name, keys, [(2400, "rows"), (3500, "selected"), (5100, "reader")], seed, work)
        check(name + "-matching-path", "Entering activity: Library" in log and reader_opened(log, expected, 3900))
    check("library-finished-removes-visible-completed-row",
          image("library-finished-visible", "rows").crop((0, 160, 960, 1200)).tobytes() !=
          image("library-finished-hidden", "rows").crop((0, 160, 960, 1200)).tobytes())

    seed = work / "library-filter-seed"
    shutil.copytree(multi, seed)
    (seed / ".crosspoint/settings.json").write_text(json.dumps({"libraryClassicGrid": 0}))
    _, log = run("library-filter", [
        (800, "DOWN"), (1500, "ENTER"), (2700, "ENTER"),
        (3500, "RIGHT"), (4100, "LEFT"), (4800, "ENTER"),
        (5700, "ENTER"), (6600, "RIGHT"), (7400, "ENTER"), (9300, "QUIT"),
    ], [(3200, "categories"), (3900, "next-category"), (4500, "first-category"),
        (5400, "values"), (6300, "books"), (8600, "reader")], seed, work)
    check("library-filter-physical-focus-and-open",
          image("library-filter", "next-category").crop((0, 160, 960, 1200)).tobytes() !=
          image("library-filter", "first-category").crop((0, 160, 960, 1200)).tobytes() and
          reader_opened(log, "/books/Navigation.epub", 7400))

    # Classic Confirm enters the real daily-goal popup, then opens its calendar
    # and changes the target through that same existing popup.
    goal_card, log = run("daily-goal", [
        (800, "DOWN"), (1100, "DOWN"), (1400, "DOWN"), (2000, "ENTER"),
        (3300, "ENTER"), (4000, "RIGHT"), (4700, "ENTER"),
        (5700, "ENTER"), (6500, "ENTER"), (7200, "RIGHT"), (7900, "ENTER"),
        (8800, "BACK"), (9800, "QUIT"),
    ], [(3000, "today"), (3700, "choose"), (5400, "configured"),
        (6200, "calendar"), (6900, "change"), (8500, "changed")], empty, work)
    goal_settings = json.loads((goal_card / ".crosspoint/settings.json").read_text())
    check("daily-goal-physical-setup-calendar-change",
          "Entering activity: ReadingStats" in log and goal_settings.get("readingDailyGoal") == 2 and
          image("daily-goal", "today").tobytes() != image("daily-goal", "choose").tobytes() and
          image("daily-goal", "configured").tobytes() != image("daily-goal", "calendar").tobytes())

    failure_seed = work / "goal-save-failure-seed"
    shutil.copytree(empty, failure_seed)
    # The native HAL writes settings directly, unlike the device's .tmp writer.
    (failure_seed / ".crosspoint/settings.json").write_text(json.dumps(
        {"homeLayout": 0, "homeLayoutVersion": 2, "readingDailyGoal": 0}))
    (failure_seed / ".crosspoint/settings.json").chmod(0o444)
    failed_card, _ = run("daily-goal-save-failure", [
        (800, "DOWN"), (1100, "DOWN"), (1400, "DOWN"), (2000, "ENTER"),
        (3300, "ENTER"), (4000, "RIGHT"), (4700, "ENTER"), (5900, "ENTER"), (7600, "QUIT"),
    ], [(3000, "before"), (5500, "error"), (7000, "restored")], failure_seed, work)
    goal_region = (0, 400, 960, 1450)
    check("daily-goal-save-failure-restores-target",
          json.loads((failed_card / ".crosspoint/settings.json").read_text()).get("readingDailyGoal") == 0 and
          image("daily-goal-save-failure", "before").crop(goal_region).tobytes() ==
          image("daily-goal-save-failure", "restored").crop(goal_region).tobytes() and
          image("daily-goal-save-failure", "error").crop(goal_region).tobytes() !=
          image("daily-goal-save-failure", "restored").crop(goal_region).tobytes())

    _, log = run("epub-contents-return", [
        (800, "LEFT"), (1500, "ENTER"), (2800, "ENTER"), (3600, "ENTER"),
        (4400, "RIGHT"), (5200, "ENTER"), (6800, "BACK"),
        (8300, "BACK:1200"), (10400, "QUIT"),
    ], [(2500, "origin"), (6300, "destination"), (7800, "returned"), (10000, "left-reader")], opened, work)
    region = (0, 160, 960, 1300)
    check("epub-contents-short-back-restores",
          image("epub-contents-return", "origin").crop(region).tobytes() !=
          image("epub-contents-return", "destination").crop(region).tobytes() and
          image("epub-contents-return", "origin").crop(region).tobytes() ==
          image("epub-contents-return", "returned").crop(region).tobytes())
    check("epub-contents-long-back-retains-navigation",
          any(int(at) > 8300 for at in re.findall(r"^\[(\d+)\].*Entering activity: FileBrowser$", log, re.MULTILINE)))

    _, log = run("settings-scroll", [
        (900, "LEFT"), (1700, "ENTER"), (2700, "RIGHT"), (3500, "ENTER"),
        *[(4500 + n * 300, "RIGHT") for n in range(12)],
        (8800, "BACK"), (10300, "QUIT"),
    ], [(4100, "initial"), (8400, "scrolled"), (9800, "categories")], empty, work)
    check("settings-physical-scroll-and-back",
          image("settings-scroll", "initial").crop((0, 160, 960, 1450)).tobytes() !=
          image("settings-scroll", "scrolled").crop((0, 160, 960, 1450)).tobytes() and
          "Entering activity: Settings" in log)

    # A real two-chapter XTC: chapter records precede the page table, with
    # contrasting bitmap pages so restoration is observable through rendering.
    xtc_seed = work / "xtc-return-seed"
    shutil.copytree(empty, xtc_seed)
    (xtc_seed / "books/Navigation.epub").unlink()
    table, payload = 56 + 2 * 96, 480 * 800 // 8
    page_size = 22 + payload
    first_page = table + 2 * 16
    xtc = bytearray(first_page + 2 * page_size)
    struct.pack_into("<4sBBHBBBBIQQQQII", xtc, 0, b"XTC\0", 1, 0, 2,
                     0, 0, 0, 1, 1, 0, table, first_page, 0, 56, 0)
    for n in range(2):
        at = 56 + n * 96
        title = f"Chapter {n + 1}".encode()
        xtc[at:at + len(title)] = title
        struct.pack_into("<HH", xtc, at + 80, n + 1, n + 1)
        at = first_page + n * page_size
        struct.pack_into("<QIHH", xtc, table + n * 16, at, page_size, 480, 800)
        struct.pack_into("<4sHHBBIQ", xtc, at, b"XTG\0", 480, 800, 0, 0, payload, 0)
        xtc[at + 22:at + page_size] = bytes([0xaa if n else 0xff]) * payload
    (xtc_seed / "books/Return.xtc").write_bytes(xtc)
    _, log = run("xtc-contents-return", [
        (800, "ENTER"), (1600, "ENTER"), (2400, "ENTER"), (4000, "ENTER"),
        (4800, "RIGHT"), (5600, "ENTER"), (7100, "BACK"), (8600, "BACK"), (10300, "QUIT"),
    ], [(3500, "origin"), (6600, "destination"), (8100, "returned"), (9700, "home")], xtc_seed, work)
    check("xtc-contents-short-back-and-inactive-back",
          image("xtc-contents-return", "origin").crop(region).tobytes() !=
          image("xtc-contents-return", "destination").crop(region).tobytes() and
          image("xtc-contents-return", "origin").crop(region).tobytes() ==
          image("xtc-contents-return", "returned").crop(region).tobytes() and
          any(int(at) > 8600 for at in re.findall(r"^\[(\d+)\].*Entering activity: Home$", log, re.MULTILINE)))
    _, log = run("xtc-active-return-long-back", [
        (800, "ENTER"), (1600, "ENTER"), (2400, "ENTER"), (4000, "ENTER"),
        (4800, "RIGHT"), (5600, "ENTER"), (7100, "BACK:1200"), (9800, "QUIT"),
    ], [(6600, "active-return"), (9300, "browser")], xtc_seed, work)
    check("xtc-active-return-long-back-retains-navigation",
          any(int(at) > 7100 for at in re.findall(
              r"^\[(\d+)\].*Entering activity: FileBrowser$", log, re.MULTILINE)))
    for name, settings, lefts, expected in [
        ("carousel", {"homeLayout": 1}, 1, "/books/Second.epub"),
        ("cover-grid", {"homeLayout": 0, "uiTheme": 4}, 2, "/books/Second.epub"),
        ("cover-grid-second", {"homeLayout": 0, "uiTheme": 4}, 1, "/books/Navigation.epub"),
        ("carousel-2", {"homeLayout": 2, "homeLayoutVersion": 2}, 1, "/books/Second.epub"),
        ("bookshelf-continue", {"homeLayout": 3, "uiTheme": 4}, 3, "/books/Second.epub"),
        ("bookshelf-second", {"homeLayout": 3, "uiTheme": 4}, 1, "/books/Navigation.epub"),
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

    seed = work / "carousel-2-settings-seed"
    shutil.copytree(multi, seed)
    (seed / ".crosspoint/settings.json").write_text(json.dumps({"homeLayout": 2, "homeLayoutVersion": 2}))
    _, log = run("carousel-2-settings", [(1000, "LEFT"), (1800, "LEFT"), (2700, "ENTER"), (4300, "QUIT")],
                 [(700, "initial"), (2300, "focused"), (3600, "opened")], seed, work)
    check("carousel-2-settings-focus-open",
          image("carousel-2-settings", "initial").crop((0, 0, 960, 120)).tobytes() !=
          image("carousel-2-settings", "focused").crop((0, 0, 960, 120)).tobytes() and
          any(int(at) > 2700 for at in re.findall(r"^\[(\d+)\].*Entering activity: Settings$", log, re.MULTILINE)))

    paged_seed = work / "bookshelf-paged-seed"
    shutil.copytree(multi, paged_seed)
    for n in range(7):
        shutil.copy2(ROOT / "test/epubs/test_reader_menu_progress.epub", paged_seed / f"books/Page{n}.epub")
    (paged_seed / ".crosspoint/settings.json").write_text(json.dumps(
        {"homeLayout": 3, "uiTheme": 4, "homeRecentBookCount": 2}))
    _, log = run("bookshelf-pages", [
        (1000, "BACK"), (1800, "LEFT"), (2700, "ENTER"), (3500, "LEFT"), (4300, "LEFT"),
        (5200, "ENTER"), (6400, "ENTER"), (8200, "QUIT"),
    ], [(2300, "first"), (3100, "second"), (5800, "restored"), (7400, "reader")], paged_seed, work)
    def page_band(label):
        im = image("bookshelf-pages", label)
        return im.crop((int(im.width * .45), int(im.height * .72),
                        int(im.width * .55), int(im.height * .83))).tobytes()
    check("bookshelf-page-controls-return-and-open", page_band("first") != page_band("second") and
          page_band("first") == page_band("restored") and reader_opened(log, "/books/Second.epub", 6400))

    if ARGS.pro_program:
        _, log = run("pro-shared-input", [
            (1000, "TAP:120,200"), (2800, "BACK"), (4000, "LEFT"), (5100, "ENTER"), (6100, "BACK"),
            (7100, "UP"), (7400, "UP"), (7900, "ENTER"), (9000, "DOWN"), (10100, "UP"),
            (11200, "TAP:200,195"), (12800, "QUIT"),
        ], [(700, "home"), (2200, "reader"), (4600, "legacy-menu"), (5700, "settings"),
            (8500, "games"), (9700, "page-two"), (10700, "page-one"), (12000, "checkers")],
            opened, work, program=ARGS.pro_program.resolve())
        check("pro-touch-opens-matching-book", reader_opened(log, "/books/Navigation.epub", 1000))
        check("pro-legacy-menu-settings", any(int(at) > 5100 for at in re.findall(
            r"^\[(\d+)\].*Entering activity: Settings$", log, re.MULTILINE)))
        check("pro-side-pages-and-touch-row", "Page 0 -> 1" in log and "Page 1 -> 0" in log and
              any(int(at) > 11200 for at in re.findall(
                  r"^\[(\d+)\].*Entering activity: Checkers$", log, re.MULTILINE)))

report = {"program": str(PROGRAM), "sha256": hashlib.sha256(PROGRAM.read_bytes()).hexdigest(), "checks": RESULTS}
if ARGS.pro_program:
    report["pro_program"] = str(ARGS.pro_program.resolve())
    report["pro_sha256"] = hashlib.sha256(ARGS.pro_program.read_bytes()).hexdigest()
(OUTPUT / "results.json").write_text(json.dumps(report, indent=2) + "\n")
raise SystemExit(0 if all(RESULTS.values()) else 1)
