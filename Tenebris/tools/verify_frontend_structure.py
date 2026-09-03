#!/usr/bin/env python3
"""Verifica a integridade estrutural do frontend após merges."""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = {
    "ui": ROOT / "src/core/frontend/manager/UIManager.cpp",
    "editor": ROOT / "src/core/frontend/manager/AdvancedEditor.cpp",
    "menu": ROOT / "src/game/frontend/Menu.cpp",
}


def clean_source(text: str) -> str:
    return re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', "", text, flags=re.DOTALL)


def balanced(text: str) -> bool:
    level = 0
    for char in clean_source(text):
        level += char == "{"
        level -= char == "}"
        if level < 0:
            return False
    return level == 0


def definitions(text: str, name: str) -> int:
    return len(re.findall(rf"\b(?:void|bool)\s+{re.escape(name)}\s*\(", text))


def main() -> int:
    sources = {name: path.read_text(encoding="utf-8") for name, path in FILES.items()}
    errors: list[str] = []
    for key, name in (
        ("ui", "UIManager::HandleKeyImpl"),
        ("ui", "UIManager::RenderImpl"),
        ("editor", "AdvancedEditor::Open"),
        ("editor", "AdvancedEditor::Draw"),
        ("editor", "AdvancedEditor::IsOpen"),
    ):
        count = definitions(sources[key], name)
        if count != 1:
            errors.append(f"{name}: {count} definicoes; esperado: 1")
    for key, path in FILES.items():
        if not balanced(sources[key]):
            errors.append(f"{path.relative_to(ROOT)}: chaves desbalanceadas")
    if "ApplyMenuColors(" in sources["menu"]:
        errors.append("Menu.cpp: chamada antiga ApplyMenuColors() encontrada")
    items_header = (ROOT / "src/game/frontend/items/Items.hpp").read_text(encoding="utf-8")
    if "Ferramenta avancada" in items_header or "Ferramenta avançada" in items_header:
        errors.append("Items.hpp: rotulo generico de ferramenta avancada encontrado")
    if errors:
        print("Frontend invalido:", *[f"\n- {error}" for error in errors], file=sys.stderr)
        return 1
    print("Frontend integro: sem duplicacoes, chaves quebradas ou ApplyMenuColors().")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
