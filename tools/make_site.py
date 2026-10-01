"""Assemble the web-flasher site for GitHub Pages.

Runs in CI after the firmware builds. Reads the board list from
platformio.ini (envs that set custom_firmware_name), copies each board's
merged firmware into the site, and writes one ESP Web Tools manifest per
board plus boards.json for the page.

Usage:
    python tools/make_site.py --bins <dir with CYD_*.bin> --version <v> --out <site dir>
"""
import argparse
import configparser
import datetime
import json
import pathlib
import shutil

ROOT = pathlib.Path(__file__).resolve().parent.parent


def boards_from_ini():
    cfg = configparser.ConfigParser(interpolation=None, strict=False)
    cfg.read(ROOT / "platformio.ini", encoding="utf-8")
    boards = []
    for section in cfg.sections():
        if not section.startswith("env:"):
            continue
        env = cfg[section]
        name = env.get("custom_firmware_name")
        if not name:
            continue
        boards.append({
            "env": section[4:],
            "file": f"{name}.bin",
            "name": name,
            "title": env.get("custom_board_title", name),
            "hint": env.get("custom_board_hint", ""),
            "tested": env.get("custom_board_tested", "yes").strip().lower() != "no",
        })
    return boards


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bins", required=True, type=pathlib.Path)
    ap.add_argument("--version", required=True)
    ap.add_argument("--out", required=True, type=pathlib.Path)
    args = ap.parse_args()

    out = args.out
    if out.exists():
        shutil.rmtree(out)
    (out / "firmware").mkdir(parents=True)
    (out / "manifests").mkdir()
    shutil.copytree(ROOT / "web", out, dirs_exist_ok=True)

    published = []
    for b in boards_from_ini():
        src = args.bins / b["file"]
        if not src.exists():
            print(f"skip {b['env']}: {src.name} not built")
            continue
        shutil.copy2(src, out / "firmware" / b["file"])
        manifest = {
            "name": f"CYD Sudoku - {b['title']}",
            "version": args.version,
            "new_install_prompt_erase": True,
            "builds": [{
                "chipFamily": "ESP32",
                # Merged factory image: bootloader + partitions + app, from 0x0
                "parts": [{"path": f"../firmware/{b['file']}", "offset": 0}],
            }],
        }
        (out / "manifests" / f"{b['name']}.json").write_text(
            json.dumps(manifest, indent=2), encoding="utf-8")
        b["size"] = src.stat().st_size
        published.append(b)
        print(f"added {b['file']}")

    if not published:
        raise SystemExit("no firmware found - nothing to publish")

    (out / "boards.json").write_text(json.dumps({
        "version": args.version,
        "built": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC"),
        "boards": published,
    }, indent=2), encoding="utf-8")
    (out / ".nojekyll").write_text("", encoding="utf-8")


if __name__ == "__main__":
    main()
