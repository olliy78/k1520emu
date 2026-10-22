from pathlib import Path

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
import sync_wiki


def test_sync_generates_source_pages_and_keeps_added_wiki_pages(tmp_path):
    source_dir = tmp_path / "source"
    source_dir.mkdir()
    (source_dir / "Home.md").write_text("# Home\n", encoding="utf-8")
    (source_dir / "Future-Page.md").write_text("# Future page\n", encoding="utf-8")
    destination = tmp_path / "published"
    sync_wiki.build_pages(destination, "olliy78/k1520emu", "main", source_dir)

    architecture = (destination / "Architecture.md").read_text(encoding="utf-8")
    emulator = (destination / "Emulator-Handbook.md").read_text(encoding="utf-8")

    assert "doc/K1520_architecture.md" in architecture
    assert "https://github.com/olliy78/k1520emu/blob/main/doc/design/03_k2526_zre.md" in architecture
    assert "[this](bool lvl)" in architecture
    version = (sync_wiki.REPOSITORY_ROOT / "VERSION").read_text(encoding="utf-8").strip()
    assert f"Version {version}" in emulator
    assert "{{VERSION}}" not in emulator
    assert (destination / "Future-Page.md").read_text(encoding="utf-8") == "# Future page\n"
    assert (destination / "Home.md").is_file()
