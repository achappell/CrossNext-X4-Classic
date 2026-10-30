# Getting Started with CrossNext

CrossNext builds X4 Pro firmware and local X4 Classic candidates (ESP32-S3).
This distribution includes the FreeInk SDK snapshot and its local UI changes.
Keep the included `freeink-sdk/` sources when building this release.

## Obtain the sources

```sh
git clone https://github.com/manuelx900/CrossNext.git
cd CrossNext
```

The source ZIP provides the same source tree without Git history. The SDK is
included as ordinary source files; a separate SDK checkout is unnecessary.
The inherited `.gitmodules` file is not the SDK revision used by this export.

## Prerequisites

- Git, Python 3.12 or newer, and PlatformIO Core; this delivery used Core 6.2.0.
- The Python packages listed in `requirements.txt`.
- SDL2 for the native desktop simulator on Linux/macOS. For the Windows browser
  simulator, use [the Windows setup guide](../simulator-windows.md).
- A matching X4 Pro and a USB cable for physical testing or USB installation.

In a fresh development environment, create a local Python environment and install
the tools. In an already configured checkout, keep the existing environment and
build cache.

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install platformio==6.2.0 -r requirements.txt
```

On Windows PowerShell, create it with `py -m venv .venv`, then activate
`.\.venv\Scripts\Activate.ps1`. The pinned platform in `platformio.ini` supplies
the device framework dependencies on the first build.

## Select the S3 target explicitly

The inherited default environment targets ESP32-C3 devices. Always select the
intended Pro or Classic environment; a successful build does not verify hardware.

| Environment | Purpose |
| --- | --- |
| `x4pro` | X4 Pro development firmware. |
| `gh_release_x4pro` | X4 Pro release firmware. |
| `simulator_x4_pro` | Desktop simulator with SDL2. |
| `x4c` | Local Classic development firmware, with development input bridge. |
| `gh_release_x4c` | Local Classic release candidate, without that bridge. |
| `simulator_x4_classic` | Native six-key Classic profile, no touch/frontlight/Home. |

```sh
pio run -e x4pro
pio run -e gh_release_x4pro
pio run -e x4c
pio run -e gh_release_x4c
pio run -e simulator_x4_classic
```

Run one build at a time: simultaneous builds can race shared PlatformIO packages.
In the configured integration workspace, `scripts_local/check.sh` owns the shared
build lock and additional checks. Its inherited target list may include other
boards; use the explicit commands above for these S3 targets.

For the Windows browser simulator, run `Start-Simulator.cmd` after completing its
[setup instructions](../simulator-windows.md#setup-from-a-source-export).

## Verify a change

Run the relevant suite under `host-tests/`, inspect the matching simulator
screens, and format changed C/C++ files through the repository wrapper:

```sh
./bin/clang-format-fix -g
```

Native and browser checks do not measure physical e-ink refresh time, power or
peak device heaps. Validate the affected behavior on X4 Pro before publishing a
release. [The device test plan](../interface-update.md) covers navigation,
wallpapers, light controls and image zoom.

## Installation and release files

Use [the installation guide](../install.md) to choose the application or factory
image and its correct installation method. The SD updater takes the application
image; the factory image contains bootloader and partitions.

CrossNext releases are published manually. Inherited CrossPlay workflow jobs are
restricted to their original repository; they do not provide CrossNext CI or
release builds. Online updates query CrossNext GitHub Releases and require the
raw `CrossNext-X4PRO-OTA.bin` application alias on each newer stable Release.
See [the release contract](../install.md#online-update-source) and verify a physical
X4 Pro update before publishing.

Classic packaging is local: run `python3 tools_local/package_crossnext.py --device x4c`
after the current `gh_release_x4c` build. The packaged application and pioarduino
factory image come from that environment's own outputs. Preserve the manifest
and checksums. Do not publish a Classic OTA alias or follow Pro online-update
instructions. Generated-layout fit is not installed-layout fit; the runtime slot
guard and the separate physical identity/lock/backup/recovery gate remain.

Classic smoke evidence belongs to actual key-only app paths, including nested
controls, cancellation and persisted reopens; opening each shelf entry is not
acceptance. Use the matching resource packs and populated images/quotes/decks/
vocabulary rather than substituting empty states. Shared changes also require
Pro touch and side-button regression. The bundled SDK is ordinary tracked source
in this checkout: include its text changes in review/export, not just a gitlink.

The Carnivoro activity's native pointer-size allowance is simulator-only
(17 KiB); the embedded activity retains its 16 KiB assertion. Do not weaken
device memory/partition budgets to make a host build pass.

The final post-review Classic evidence is indexed by
`qa-artifacts/x4c-coverage.json` and
`qa-artifacts/x4c-integration/final/verification-summary.json`: all 32 affected
native runners, fresh target builds, actual Pro touch/side/queued-Confirm smoke
and current-source package checks passed. Original failed observations remain
beside repaired runs. UI tests check actionable bounds, enabled targets,
content preservation and semantic routing, not incidental wording or geometry.
Formatter executables were unavailable; no formatting pass is recorded.

Keep the original [licence](../../LICENSE), [third-party notices](../../THIRD-PARTY-NOTICES.md)
and [upstream credits](../crossnext-changes.md). For architecture and app
interfaces, see [the architecture overview](architecture.md),
[building apps](../building-apps.md) and [the shelf contract](../shelf.md).
