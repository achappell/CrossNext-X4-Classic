# Installing CrossNext

The X4 Pro release path and the X4 Classic local software-build path are separate.
Both select ESP32-S3 profiles; other inherited board environments remain outside
these checks. Classic has not been flashed or physically/recovery validated.

| Model | Package | Application image | Factory image |
| --- | --- | --- | --- |
| X4 Pro | `CrossNext-X4PRO-Firmware-v1.3.1.zip` | `CrossNext-X4PRO-Firmware-v1.3.1.bin` | `CrossNext-X4PRO-Firmware-v1.3.1-factory.bin` |
| X4 Classic, local only | `CrossNext-X4C-Firmware-v1.3.1.zip` | `CrossNext-X4C-Firmware-v1.3.1.bin` | `CrossNext-X4C-Firmware-v1.3.1-factory.bin` |

Packages are generated in `dist/`; raw images are in `dist/CrossNext/`.
Check the package's `SHA256SUMS.txt` before installation. Back up the microSD.
New settings default to English; choose Spanish in the language settings.
An existing settings file retains its saved language.

## X4 Classic gate and current-build producers

`pio run -e x4c` builds development firmware; `pio run -e gh_release_x4c`
builds the local release candidate without the development serial input bridge.
`python3 tools_local/package_crossnext.py --device x4c` packages the release
environment's `.pio/build/gh_release_x4c/firmware.bin` application and
`firmware.factory.bin` factory output, with manifest, SHA-256 checksums and notes.
The factory image is the actual pioarduino merge of bootloader, generated
partitions, boot-app data and application, not a renamed application image.

The tagged application fits the **generated** partition layout. The partition
table installed on a reader is unknown: the SD updater's runtime slot-size
rejection remains required. A present wrong-board tag rejects; historical
untagged-image compatibility is retained, but does not verify that image's board.

The 2026-10-30 post-review `gh_release_x4c` application is 7,618,176 bytes against
two generated 8,323,072-byte app slots (704,896 bytes of image headroom). Its factory
image was checked against that same build's bootloader at `0x0`, partitions at
`0x8000`, framework boot-app data at `0xe000`, and application at `0x10000`.
`qa-artifacts/x4c-integration/final/package-validation.json` records the exact
input, packaged-image and archive hashes. These numbers do not establish fit in
an unknown installed partition table.

Classic installation is local only. Use the application through an already
compatible SD updater; never give that updater the factory image. Factory USB
at `0x0` is unavailable until separate physical identity, USB-lock, backup and
recovery checks authorize it. No such gate, flash, recovery or public publishing
was performed here. Do not use the Pro online-update asset or online-update
instructions below for Classic.

## Update an existing compatible installation from microSD

1. Extract the model's package and copy its **application** `.bin` to the microSD.
2. Open **Settings > Device > Update firmware** on the reader.
3. Select that application image and confirm the model and update prompt.
4. Keep power stable until the update and restart finish.

The SD picker accepts the selected `.bin` path; CrossNext's filenames are valid.
Do not select `-factory.bin` here: it includes bootloader/partitions rather than
an application alone. The updater checks board identity and available slot size.
An older partition table with smaller application slots may require factory USB
installation; an SD update does not rewrite the partition table.

## Factory installation over USB

Use the **factory** image at offset `0x0`. It contains the bootloader, partition
table and application. For example, with esptool installed and the device in
download mode, use the matching model's command:

```sh
esptool.py --chip esp32s3 --baud 921600 write_flash 0x0 CrossNext-X4PRO-Firmware-v1.3.1-factory.bin
```

An application-only image belongs at `0x10000` when deliberately writing an
application over USB; writing it at `0x0` will not install a bootloader.
USB-locked devices cannot use the factory route. Do not assume an SD update can
enlarge a locked device's application partition. No verified X4 Pro
recovery procedure is supplied by this export. Use device-specific recovery
instructions from the device vendor; do not apply X4/X3 ESP32-C3 procedures
to these S3 devices.

## Online update source

On X4 Pro, open **Settings → Device → Check for Updates**, connect to Wi-Fi and
wait for the release check. The firmware queries only
`https://api.github.com/repos/manuelx900/CrossNext/releases/latest`.
It compares the latest stable tag with the installed version and asks before
installing. It does not query the CrossPlay proxy or CrossPoint releases.

For each public GitHub Release, use a semantic tag such as `1.3.1` and attach
`CrossNext-X4PRO-OTA.bin` directly to the release. The packaging tool creates this
stable application-only alias alongside the versioned application and factory
images. Its bytes match the versioned application. The updater deliberately
ignores ZIPs, factory images and differently named assets. Keep the alias name
unchanged in future releases so installed devices can find it.

The updater retains chip, board and OTA-slot size checks. A reader with an older,
smaller partition table may need a factory USB installation first. Back up the SD
card and test the update on a physical X4 Pro before publishing the release.
A source repository alone is not an update: until a public Release and its raw
OTA asset exist, there is no online firmware to install. A GitHub/API connection
failure is reported on the device; it never falls back to upstream firmware.

Maintainers: publish the firmware ZIP, `CrossNext-X4PRO-OTA.bin`, resources and
checksums as Release attachments. Source commits remain separate. To test OTA,
keep the reader on `1.1.5`, then publish the newer `1.3.1` build/tag; the
same version or an older version must not be offered as an update.

## Required app contents

Install the matching `CrossNext-Resources.zip` alongside this firmware. Extract
its folders to the microSD root and keep the bundled licences. It includes the
reader fonts/dictionaries and `/apps/reading-adventure/`, `/apps/pet/`, `/apps/intercept/`.
PET needs all six of its content packs; The Intercept needs `graphics.bin`.
Missing or incompatible contents show an installation notice instead of the app.

Content packs contain no personal saves. Preserve `/.crosspoint/` when updating
resources; PET and The Intercept keep their saved data separately there. These
internal folders are excluded from the Library. Files hides dot folders by
default; enabling Show hidden files can reveal them. Restart after installing.
