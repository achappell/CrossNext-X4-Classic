# CrossNext changes

## Local X4 Classic software candidate — 2026-10-30

- Restore the standard WiFi picker in public Classic builds; retain the Classic port and navigation fixes.
- Correct Classic Apps/Games front-button row navigation across pages, retaining
  chooser/page controls and side navigation. Include displayed recent books in
  Classic Home's physical selection ring, with visible Continue focus and Confirm
  opening the selected book; Pro/C3 keep their existing menu-only Home ring.
  Native Classic before/after runtime scenarios reproduce then pass both paths.
  Final navigation proof also passes seven host suites plus the CI shell suite,
  16 Classic runtime checks, current Pro touch/side smoke, and both Classic
  development/release device builds.
  This navigation correction has not been installed or verified on hardware.
- Add separate S3/16 MB/8 MB PSRAM/SDMMC development and release targets and a
  genuine Classic simulator with six navigation keys plus Power, no touch,
  frontlight or Home key; reject touch-assisted scripted Classic input.
- Add opt-in chrome focus and explicit coordinate/adjustment modes across active
  app/game paths, with nonmutating editor entry, cancellation and held-release
  suppression. Retain Pro/C3 input behavior and device memory/slot guards.
- Adapt Classic board instructions and full-height screen hints to actual keys;
  retain touch instructions and layout on Pro.
- Restore the existing phone-typing entry in Classic Notes and named-list options.
  Disconnected simulator runs open the Wi-Fi child and can cancel safely; they do
  not establish a running phone service or change existing note/task storage.
- Make installed Flashcards Photo, Undo and question-summary controls reachable
  by Classic focus as well as touch; keep held-answer history input from leaking
  a short release. Retain installed-deck scheduling, history and resume formats.
- Size translated Checkers Help lessons from their wrapped text so complete
  Spanish capture rules fit without overlapping the next lesson or page controls.
- Add local Classic application/factory ZIP, manifest and hash packaging using
  current build outputs. SD requires a compatible updater; USB factory use still
  requires a separate identity, lock, backup and recovery gate.
- Keep simulator/build evidence distinct from physical support: no Classic
  flash, hardware recovery, online release or publishing authorization is claimed.
- Account for all 29 companion rows with nested keys, production boundary fixtures
  and persisted reopens. After independent review corrections, all 32 affected
  native runners, fresh Classic development/release and Pro native builds pass.
  Actual Pro touch/side/configured-Confirm smoke includes the changed Notes queue.
  The final application is 7,618,176 bytes with 704,896 generated-slot headroom;
  current factory inputs, boot-app initialization, ZIP and SHA-256 manifests pass.
- Review corrections preserve queued focus/Confirm gestures, validate the selected
  published interaction generation, reject synthetic touch through the shared
  no-touch parser, verify factory boot-app bytes, and retain real clipped note
  previews. Classic-only held-button guidance no longer changes C3 instructions.
  Actual staged-Back/card-reset/durable-Undo evidence and final global checks cover
  the corrections. No findings remain deferred. Formatter executables were
  unavailable, not passed; physical-device and installed-table gates remain open.

## CrossNext v1.1.0 — 2026-10-25

- Add PET: three companions, reading-based growth, care, four minigames,
  240 decorations and 50 progressively discovered memory types.
- Refresh the album and accessory previews; allow removing furnishings while
  keeping ownership. Fit English/Spanish stage names on one line.
- Store The Intercept artwork on microSD. PET and The Intercept show the same
  missing-content installation guidance as Reading Quest.
- Integrate reader startup/prefetch, EPUB event caching, batched page/index writes,
  idle deadlines, glyph drawing and Home return fixes. No device speed or battery
  figures are claimed without physical measurements.
- Review PET's English/Spanish wording and update the bilingual README with
  the maintainer's PET illustration.
- Prepare X4 Pro application, factory and OTA release files with version 1.1.0.

See [release notes](docs/releases/CrossNext-v1.1.0.md).

## Local release candidate — 2026-10-21

- Replace Words' Anki shortcut and small gear with a full-width Settings bar;
  Anki importing remains in the dedicated Flashcards app.
- Remove the redundant divider before Words' labeled example sentence.
- Prepare high-resolution Anki images at import; open that store and allocate its
  4 KiB read band only after Zoom plus. Normal cards keep their small preview.
- Use the same built-in Latin fonts in Words and Flashcards. Question-side Words
  cards show only the term; answers retain bold Word/Definition/Example labels.
- Smart sizing probes one page before reducing, keeps a readable 12 pt floor for
  ordinary long text and paginates; manual 10 pt and oversized-word fallback remain.
- Queue Counter taps and button presses while rendering, and merge their updates.
- Toggle Calendar, Clock and Anki image rotation between two positions.
- Bold quote title/author metadata; buffer quote scanning and reuse its index.
- Reuse Notes/Tasks page starts, debounce Pomodoro interaction saves, and accelerate
  Habits checksums without changing its verified two-slot persistence.
- Reading Quest resources use a verified index and per-image checksums instead of
  reading the entire artwork pack at startup; history scanning reads eight records
  per SD operation. Install the updated Resources ZIP for the artwork improvement.
- Reserve margins above/below Wallpapers pagination and retain swipes/buttons.
- Build and package this delivery for Xteink X4 Pro only.

- Add separate question/word and content text sizes (10, 12, 14, 16, 18 pt)
  to Flashcards and Words, with Smart sizing enabled by default.
- Label vocabulary word, definition and example clearly, retain full pagination,
  and reuse page positions instead of measuring the complete entry on every turn.
- Make Pomodoro's physical button toggle Start/Pause and simplify timeline arrows.
- Keep Library cover generation off the input path and merge pending selection
  changes before presenting the final cursor position.
- Prepare CrossNext source, device and microSD packages and a populated simulator.

See [the fork summary and upstream credits](docs/crossnext-changes.md) for the
inherited CrossPoint/CrossPlay foundation and other local interface changes.
This candidate still requires physical-device verification before public release.
