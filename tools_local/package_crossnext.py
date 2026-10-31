"""Give release copies CrossNext names without changing PlatformIO/OTA filenames."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import zipfile
import struct

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_VERSION = "1.4.0"
OTA_ASSET = "CrossNext-X4PRO-OTA.bin"


def release_name(device):
    if device == "X4Pro":
        return f"CrossNext-X4PRO-Firmware-v{FIRMWARE_VERSION}"
    return f"CrossNext-X4C-Firmware-v{FIRMWARE_VERSION}" if device == "X4C" else f"CrossNext-{device}"


def classic_boot_app():
    """Resolve this checkout's framework producer using PlatformIO's standard package locations."""
    core_dir = Path(os.environ.get("PLATFORMIO_CORE_DIR") or "~/.platformio").expanduser()
    packages_dir = Path(os.environ.get("PLATFORMIO_PACKAGES_DIR") or core_dir / "packages").expanduser()
    return packages_dir / "framework-arduinoespressif32/tools/partitions/boot_app0.bin"


def classic_inputs(source, boot_app_path):
    """Validate all factory inputs against the current build and framework producer."""
    app = (source / "firmware.bin").read_bytes()
    factory = (source / "firmware.factory.bin").read_bytes()
    partitions = (source / "partitions.bin").read_bytes()
    bootloader = (source / "bootloader.bin").read_bytes()
    boot_app_path = Path(boot_app_path).resolve()
    boot_app = boot_app_path.read_bytes()
    if not boot_app:
        raise ValueError("Framework boot_app0.bin must not be empty")
    if len(app) < 24 or app[0] != 0xE9 or struct.unpack_from("<H", app, 12)[0] != 9:
        raise ValueError("Classic application must be an ESP32-S3 image")
    tags = []
    start = 0
    magic = b"CROSSPOINT-BOARD-V1:"
    while (start := app.find(magic, start)) >= 0:
        end = app.find(b";", start + len(magic))
        tags.append(app[start + len(magic):end] if end >= 0 else b"")
        start += len(magic)
    if not tags or any(tag != b"x4c" for tag in tags):
        raise ValueError("Classic application must carry only the x4c board tag")
    slots = []
    for at in range(0, len(partitions) - 31, 32):
        magic_value, kind, subtype, offset, size, _, _ = struct.unpack_from("<HBBII16sI", partitions, at)
        if magic_value == 0x50AA and kind == 0:
            slots.append((offset, size))
    if not slots or any(len(app) > size for _, size in slots):
        raise ValueError("Classic application exceeds a generated application slot")
    if factory[:len(bootloader)] != bootloader or factory[0x8000:0x8000 + len(partitions)] != partitions:
        raise ValueError("Factory image does not contain this build's bootloader/partition table")
    if factory[slots[0][0]:slots[0][0] + len(app)] != app:
        raise ValueError("Factory image does not contain this build's application")
    if factory[0xE000:0xE000 + len(boot_app)] != boot_app:
        raise ValueError("Factory image does not contain this framework's boot_app0.bin")
    return dict(board="x4c", chip="esp32s3", generated_slots=[dict(offset=o, bytes=s) for o, s in slots],
                installed_table_fit="unknown; runtime slot-size rejection retained",
                hardware_validation="not performed",
                boot_app0=dict(source=str(boot_app_path), offset=0xE000,
                               bytes=len(boot_app), sha256=hashlib.sha256(boot_app).hexdigest(),
                               producer="framework-arduinoespressif32/tools/partitions/boot_app0.bin"),
                build_inputs={**{name: hashlib.sha256((source / name).read_bytes()).hexdigest()
                                 for name in ("firmware.bin", "firmware.factory.bin", "bootloader.bin", "partitions.bin")},
                              "boot_app0.bin": hashlib.sha256(boot_app).hexdigest()})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--device", choices=("x4pro", "x4c", "sticky", "all"), default="x4pro")
    selected = parser.parse_args().device
    devices = {"x4pro": ("gh_release_x4pro", "X4Pro"), "x4c": ("gh_release_x4c", "X4C"),
               "sticky": ("gh_release_sticky", "Sticky")}
    targets = list(devices.values()) if selected == "all" else [devices[selected]]
    output = ROOT / "dist/CrossNext"
    output.mkdir(parents=True, exist_ok=True)
    manifest = []
    for environment, device in targets:
        if device == "X4C":
            # No historical dist/development fallback: both files must be outputs of this release build.
            source = ROOT / ".pio/build" / environment
            identity = classic_inputs(source, classic_boot_app())
        else:
            candidates = [ROOT / parent / name for name in (environment, "x4pro" if device == "X4Pro" else "sticky")
                          for parent in ("dist/firmware", ".pio/quest-sd-build", ".pio/build")]
            candidates = [p for p in candidates if (p / "firmware.bin").is_file() and (p / "firmware.factory.bin").is_file()]
            if not candidates:
                raise FileNotFoundError(f"No complete application/factory images for {device}; build this target through scripts_local/check.sh first")
            source = max(candidates, key=lambda p: (p / "firmware.bin").stat().st_mtime_ns)
            identity = {}
        for original, suffix in (("firmware.bin", ""), ("firmware.factory.bin", "-factory")):
            path = source / original
            if not path.is_file():
                raise FileNotFoundError(path)
            target = output / f"{release_name(device)}{suffix}.bin"
            shutil.copy2(path, target)
            manifest.append(dict(file=target.name, bytes=target.stat().st_size,
                                 sha256=hashlib.sha256(target.read_bytes()).hexdigest(),
                                 environment=environment, source=path.relative_to(ROOT).as_posix(),
                                 producer="PlatformIO buildprog" if not suffix else "pioarduino builder/main.py merge-bin post-build",
                                 artifact="factory" if suffix else "application", **identity))
        if device == "X4Pro":
            ota = output / OTA_ASSET
            shutil.copy2(source / "firmware.bin", ota)
            manifest.append(dict(file=ota.name, bytes=ota.stat().st_size,
                                 sha256=hashlib.sha256(ota.read_bytes()).hexdigest()))
    (output/"SHA256SUMS.txt").write_text("".join(f"{item['sha256']}  {item['file']}\n" for item in manifest), encoding="utf-8")
    (output/"manifest.json").write_text(json.dumps(manifest, indent=2)+"\n", encoding="utf-8")
    (output/"README.txt").write_text("CrossNext firmware\n\nChoose the device that matches your reader.\nClassic packages are local installation only, software artifacts not hardware-validated.\nApplication: compatible SD updater; installed-table fit is unknown and runtime slot-size rejection is retained.\nFactory: bootloader/partitions/boot_app0/application, USB at 0x0 only after a separate device identity, lock, backup and recovery gate.\nDo not use another model's image. See each model ZIP for its instructions.\n", encoding="utf-8")
    for _, device in targets:
        entries = [item for item in manifest if item["file"].startswith(release_name(device)) or (device == "X4Pro" and item["file"] == OTA_ASSET)]
        notes = (
            f"{release_name(device)}\n\n"
            "EN: Back up your microSD. Use only this device model's images.\n"
            "The application .bin is for a compatible update/OTA or offset 0x10000.\n"
            "The -factory.bin contains bootloader and partitions; flash at 0x0.\n"
            "Verify SHA256SUMS.txt. Device testing is required before release.\n"
            "Online updates use manuelx900/CrossNext GitHub Releases.\n"
            "Upload CrossNext-X4PRO-OTA.bin as a raw release asset for device updates.\n\n"
            "ES: Guarda una copia de tu microSD. Usa solo las imagenes de este modelo.\n"
            "El .bin de aplicacion sirve para actualizacion/OTA compatible o 0x10000.\n"
            "El -factory.bin incluye arranque y particiones; se instala en 0x0.\n"
            "Comprueba SHA256SUMS.txt y prueba el dispositivo antes de publicar.\n"
            "La actualizacion en linea usa GitHub Releases de manuelx900/CrossNext.\n"
            "Adjunta CrossNext-X4PRO-OTA.bin directamente al Release para habilitar OTA.\n\n"
            "Resources and source code are distributed in separate ZIP files.\n"
            "See README.es.md / docs/install.md in the source archive for details.\n"
        )
        if device == "X4C":
            notes = (
                f"{release_name(device)}\n\n"
                "Local installation only. These software artifacts are not hardware-validated.\n"
                "Application .bin: use only a compatible SD updater on an identified X4 Classic.\n"
                "Generated-layout fit was checked; installed-table fit is unknown. Runtime slot-size rejection remains.\n"
                "The -factory.bin combines this build's bootloader, partition table, framework boot_app0 and application.\n"
                "Factory installation uses USB at 0x0 ONLY after the separate device identity/lock/backup/recovery gate.\n"
                "This package does not authorize flashing, publishing or erasing existing data.\n"
                "Verify SHA256SUMS.txt. manifest.json identifies each current-build input and producer.\n"
                "Untagged third-party firmware compatibility is not board verification.\n"
            )
        archive_path = ROOT / "dist" / f"{release_name(device)}.zip"
        with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for item in entries:
                archive.write(output / item["file"], item["file"])
            archive.writestr("README.txt", notes)
            archive.writestr("SHA256SUMS.txt", "".join(f"{item['sha256']}  {item['file']}\n" for item in entries))
            archive.writestr("manifest.json", json.dumps(entries, indent=2) + "\n")
            archive.write(ROOT / "LICENSE", "LICENSE")
            archive.write(ROOT / "THIRD-PARTY-NOTICES.md", "THIRD-PARTY-NOTICES.md")
            archive.write(ROOT / "third_party/stb/LICENSE", "licenses/stb_image.txt")
            # Keep full notices beside binary distributions, including embedded fonts/icons.
            for directory in ("freeink-sdk", "lib", "src/apps_local", "assets_local", "third_party", "licenses"):
                for notice in sorted((ROOT / directory).rglob("*")):
                    if not notice.is_file() or {".git", "node_modules", "__pycache__"} & set(notice.parts):
                        continue
                    name = notice.name.lower()
                    if name.startswith(("license", "licence", "copying", "copyright", "ofl", "notice")) or name in {"upstream-readme.md", "readme_en_us.txt"}:
                        archive.write(notice, "licenses/" + notice.relative_to(ROOT).as_posix())
        with zipfile.ZipFile(archive_path) as archive:
            if archive.testzip() is not None:
                raise ValueError(f"Corrupt archive: {archive_path}")
            for item in entries:
                if hashlib.sha256(archive.read(item["file"])).hexdigest() != item["sha256"]:
                    raise ValueError(f"Image differs in archive: {item['file']}")
        print(archive_path)
    print(output)


if __name__ == "__main__":
    main()
