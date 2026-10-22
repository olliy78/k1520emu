#!/usr/bin/env python3
"""Publish maintained Markdown pages to the GitHub Wiki worktree."""

from __future__ import annotations

import argparse
import os
import posixpath
import re
import shutil
from pathlib import Path
from urllib.parse import quote, unquote

REPOSITORY_ROOT = Path(__file__).resolve().parents[1]
WIKI_SOURCE = REPOSITORY_ROOT / "wiki"
SOURCE_PAGES = {
    "Architecture.md": "doc/K1520_architecture.md",
    "Emulator-Handbook.md": "app/help/handbuch.md",
    "DiskTool-Handbook.md": "app/disktool/help/handbuch.md",
    "Kermit-Handbook.md": "app/kermittool/help/handbuch.md",
    "Debugger-Handbook.md": "doc/handbuch_k1520dbg.md",
}
LINK_RE = re.compile(r"(?P<image>!)?\[(?P<label>[^\]]*)\]\((?P<target>[^)]+)\)")
FENCE_RE = re.compile(r"^\s*(`{3,}|~{3,})")


def _rewrite_link(target: str, source: str, repository: str, ref: str, image: bool) -> str:
    if target.startswith(("https://", "http://", "mailto:", "#", "data:")):
        return target

    path, separator, fragment = target.partition("#")
    path = unquote(path)
    resolved = posixpath.normpath(posixpath.join(posixpath.dirname(source), path))
    encoded_path = quote(resolved, safe="/")
    if image:
        url = f"https://raw.githubusercontent.com/{repository}/{ref}/{encoded_path}"
    else:
        url = f"https://github.com/{repository}/blob/{ref}/{encoded_path}"
    return url + (separator + fragment if separator else "")


def _rewrite_links(text: str, source: str, repository: str, ref: str) -> str:
    def replace(match: re.Match[str]) -> str:
        target = _rewrite_link(
            match.group("target"),
            source,
            repository,
            ref,
            image=bool(match.group("image")),
        )
        image_prefix = match.group("image") or ""
        return f"{image_prefix}[{match.group('label')}]({target})"

    lines = []
    fence = None
    for line in text.splitlines(keepends=True):
        match = FENCE_RE.match(line)
        if match:
            marker = match.group(1)
            if fence is None:
                fence = marker
            elif marker[0] == fence[0] and len(marker) >= len(fence):
                fence = None
            lines.append(line)
        elif fence is None:
            lines.append(LINK_RE.sub(replace, line))
        else:
            lines.append(line)
    return "".join(lines)


def build_pages(
    destination: Path,
    repository: str,
    ref: str,
    source_dir: Path = WIKI_SOURCE,
) -> None:
    destination.mkdir(parents=True, exist_ok=True)

    for page in source_dir.rglob("*.md"):
        if page.name == "README.md":
            continue
        target = destination / page.relative_to(source_dir)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(page, target)

    version = (REPOSITORY_ROOT / "VERSION").read_text(encoding="utf-8").strip()
    for page_name, source in SOURCE_PAGES.items():
        source_path = REPOSITORY_ROOT / source
        content = source_path.read_text(encoding="utf-8")
        content = content.replace("{{VERSION}}", version)
        content = _rewrite_links(content, source, repository, ref)
        source_url = f"https://github.com/{repository}/blob/{ref}/{source}"
        banner = (
            f"> Automatically synchronized from "
            f"[`{source}`]({source_url}). Edit the source file in the project repository.\n\n"
        )
        (destination / page_name).write_text(banner + content, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument(
        "--repository",
        default=os.environ.get("GITHUB_REPOSITORY", "olliy78/k1520emu"),
    )
    parser.add_argument("--ref", default=os.environ.get("GITHUB_REF_NAME", "main"))
    args = parser.parse_args()
    build_pages(args.destination, args.repository, args.ref)


if __name__ == "__main__":
    main()
