#!/usr/bin/env python3
"""Fail when the classic frontend is accidentally duplicated during a merge."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UI_MANAGER = ROOT / "src/core/frontend/manager/UIManager.cpp"
ADVANCED_EDITOR = ROOT / "src/core/frontend/manager/AdvancedEditor.cpp"
MENU = ROOT / "src/game/frontend/Menu.cpp"


def source(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def count_definition(text: str, qualified_name: str) -> int:
    pattern = rf"\b(?:void|bool)\s+{re.escape(qualified_name)}\s*\("
    return len(re.findall(pattern, text))


def strip_comments_and_literals(text: str) -> str:
    pattern = r'//[^\n]*|/\*.*?\*/|R".*?\(.*?\).*?"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    return re.sub(pattern, "", text, flags=re.DOTALL)


def braces_are_balanced(text: str) -> bool:
    depth = 0
    for character in strip_comments_and_literals(text):
        if character == "{":
            depth += 1
        elif character == "}":
            depth -= 1
            if depth < 0:
                return False
    return depth == 0


def main() -> int:
    ui_manager = source(UI_MANAGER)
    advanced_editor = source(ADVANCED_EDITOR)
    menu = source(MENU)

    failures: list[str] = []
    expected_once = (
        (ui_manager, "UIManager::HandleKeyImpl"),
        (ui_manager, "UIManager::RenderImpl"),
        (advanced_editor, "AdvancedEditor::Open"),
        (advanced_editor, "AdvancedEditor::Draw"),
        (advanced_editor, "AdvancedEditor::IsOpen"),
    )
    for text, name in expected_once:
        count = count_definition(text, name)
        if count != 1:
            failures.append(f"{name}: esperada 1 definicao, encontradas {count}")

    for path, text in ((UI_MANAGER, ui_manager), (ADVANCED_EDITOR, advanced_editor), (MENU, menu)):
        if not braces_are_balanced(text):
            failures.append(f"{path.relative_to(ROOT)}: chaves desbalanceadas")

    if "ApplyMenuColors(" in menu:
        failures.append("Menu.cpp: chamada residual ApplyMenuColors()")

    if failures:
        print("Falha na estrutura do frontend:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print("Frontend verificado: implementacoes unicas, chaves balanceadas e tema antigo ausente.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
