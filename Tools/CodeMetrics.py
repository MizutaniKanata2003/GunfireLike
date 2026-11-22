#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GunfireLike コード指標の測定（フェーズI1）。

C++のソース（.h / .cpp）を静的に読み、規約02の13節の基準に対する超過を一覧にする。
  - 関数: 本体の行数、ネストの深さ、引数の数
  - クラス・構造体: 本体の行数、メンバー変数の数
  - ファイル: 物理行数（500行上限）
  - 重複: 10行以上の同じ並び（識別子と数値の違いを無視）
ソースは変更しない。出力は、CSV 4つとMarkdownのレポート1つ。

使い方（リポジトリーのルートで）:
  python Tools/CodeMetrics.py
  python Tools/CodeMetrics.py --root . --source Source --out Tools/CodeMetricsOutput

簡易的な字句解析で数えるため、結果は目安。数え方の限界は、レポートの末尾に記す。
"""

import argparse
import csv
import re
import sys
from collections import defaultdict
from pathlib import Path

# 規約02の13節の基準。引数で変更できる。
DEFAULTS = {
    "max_function_lines": 50,
    "max_nesting": 3,
    "max_parameters": 4,
    "max_class_lines": 400,
    "max_class_members": 15,
    "max_file_lines": 500,
    "duplicate_lines": 10,
}
SOURCE_SUFFIXES = {".h", ".hpp", ".cpp"}
DEFAULT_EXCLUDES = ["Imgui"]  # 外部ライブラリー（Dear ImGui）のフォルダー名
CONTROL_WORDS = re.compile(r"\b(if|for|while|switch|do)\b")
LAMBDA_HEAD = re.compile(r"\[[^\]]*\]\s*(\([^)]*\))?\s*(mutable\s*)?(noexcept\s*)?(->\s*[\w:<>,\s\*&]+)?\s*$")
NOT_FUNCTION_WORDS = {"if", "for", "while", "switch", "catch", "return", "sizeof", "decltype", "alignas", "static_assert"}
KEYWORDS = {
    "alignas", "auto", "bool", "break", "case", "catch", "char", "class", "const", "constexpr", "continue", "default",
    "delete", "do", "double", "else", "enum", "explicit", "extern", "false", "final", "float", "for", "friend", "if",
    "inline", "int", "long", "namespace", "new", "noexcept", "nullptr", "operator", "override", "private", "protected",
    "public", "return", "short", "signed", "sizeof", "static", "struct", "switch", "template", "this", "true",
    "try", "typedef", "typename", "union", "unsigned", "using", "virtual", "void", "volatile", "while",
}


def strip_source(text):
    """コメント・文字列・文字・プリプロセッサ行を空白にする。行の数と位置は保つ。"""
    out = []
    i, n = 0, len(text)
    line_start = True
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if line_start and c in " \t":
            out.append(c)
            i += 1
            continue
        if line_start and c == "#":
            # プリプロセッサ行（行末の\\で続く場合は、続きも含める）
            while i < n:
                if text[i] == "\n":
                    if i > 0 and text[i - 1] == "\\":
                        out.append("\n")
                        i += 1
                        continue
                    break
                out.append(" ")
                i += 1
            line_start = False
            continue
        line_start = False
        if c == "/" and nxt == "/":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
        elif c == "/" and nxt == "*":
            out.append("  ")
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append("  ")
            i += 2
        elif c in "\"'":
            out.append(c)
            i += 1
            while i < n and text[i] != c and text[i] != "\n":
                if text[i] == "\\" and i + 1 < n:
                    out.append("  ")
                    i += 2
                    continue
                out.append(" ")
                i += 1
            if i < n and text[i] == c:
                out.append(c)
                i += 1
        else:
            out.append(c)
            if c == "\n":
                line_start = True
            i += 1
    return "".join(out)


def split_parameters(param_text):
    """括弧の中身を、最上位のカンマで分け、引数の数を返す。(void)と空は0個。"""
    body = param_text.strip()
    if body in ("", "void"):
        return 0
    depth = 0
    count = 1
    for ch in body:
        if ch in "(<[{":
            depth += 1
        elif ch in ")>]}":
            depth -= 1
        elif ch == "," and depth == 0:
            count += 1
    return count


def parse_function_head(head):
    """関数の頭（{の直前の文）から、名前と引数の数を取り出す。関数でなければNone。"""
    for match in re.finditer(r"([~\w:]+(?:<[^<>()]*>)?(?:::~?\w+)*|operator\s*\S+?)\s*\(", head):
        name = match.group(1)
        if name in NOT_FUNCTION_WORDS:
            continue
        depth = 0
        start = match.end()
        for k in range(start, len(head)):
            if head[k] == "(":
                depth += 1
            elif head[k] == ")":
                if depth == 0:
                    return name, split_parameters(head[start:k])
                depth -= 1
        return None
    return None


def find_matching_brace(code, open_index):
    """open_indexの{に対応する}の位置を返す。見つからなければ-1。"""
    depth = 0
    for k in range(open_index, len(code)):
        if code[k] == "{":
            depth += 1
        elif code[k] == "}":
            depth -= 1
            if depth == 0:
                return k
    return -1


def count_code_lines(lines, first, last):
    """first〜last行（0始まり、両端を含む）のうち、空でない行の数。"""
    return sum(1 for k in range(first, last + 1) if lines[k].strip())


def analyze_file(path, rel):
    """1ファイルを走査し、関数・クラス・ファイルの指標を返す。"""
    raw = path.read_text(encoding="utf-8-sig", errors="replace")
    physical_lines = raw.count("\n") + (0 if raw.endswith("\n") or raw == "" else 1)
    code = strip_source(raw)
    lines = code.split("\n")
    line_of = []  # 文字位置→行番号（0始まり）
    for number, text in enumerate(lines):
        line_of.extend([number] * (len(text) + 1))
    functions, classes = [], []
    scopes = []  # {kind, level, open, name, head_start, members}
    head_start = 0
    max_level_in_function = 0
    function_info = None
    paren_depth = 0
    i = 0
    n = len(code)
    while i < n:
        c = code[i]
        if c == "(":
            paren_depth += 1
        elif c == ")":
            paren_depth = max(0, paren_depth - 1)
        elif c == ";" and paren_depth > 0:
            pass  # for文の括弧内の;は、文の区切りではない
        elif c == ";":
            scope = scopes[-1] if scopes else None
            if scope and scope["kind"] == "class":
                statement = re.sub(r"\b(public|private|protected)\s*:", "", code[head_start:i])
                stripped = re.sub(r"<[^<>]*>", "", statement)  # テンプレート引数内の括弧を除く
                first_word = (statement.split() or [""])[0]
                if (stripped.strip() and "(" not in stripped and first_word not in
                        ("using", "typedef", "friend", "static_assert", "class", "struct", "enum", "public", "private", "protected")):
                    scope["members"] += 1
            head_start = i + 1
        elif c == "{":
            head = code[head_start:i]
            head_one = " ".join(head.split())
            scope = scopes[-1] if scopes else None
            in_function = any(s["kind"] in ("function", "block") for s in scopes)
            if in_function:
                parent_level = scope["level"]
                level = parent_level
                if CONTROL_WORDS.search(head_one) or re.search(r"\belse\b", head_one) or LAMBDA_HEAD.search(head_one):
                    level = parent_level + 1
                scopes.append({"kind": "block", "level": level, "open": i})
                function_info["max_level"] = max(function_info["max_level"], level)
                head_start = i + 1
            elif re.search(r"\bnamespace\b", head_one) or re.match(r'^extern\s+"', head_one):
                scopes.append({"kind": "namespace", "open": i})
                head_start = i + 1
            elif re.search(r"\b(class|struct|union)\b", head_one) and "(" not in head_one:
                name_match = re.search(r"\b(?:class|struct|union)\s+(?:\w+\s+)*?(\w+)\s*(?:final\s*)?(?::[^{]*)?$", head_one)
                scopes.append({"kind": "class", "open": i, "name": name_match.group(1) if name_match else "?", "members": 0})
                head_start = i + 1
            elif re.search(r"\benum\b", head_one):
                end = find_matching_brace(code, i)
                i = end if end >= 0 else n
                head_start = i + 1
            elif ("(" in re.sub(r"<[^<>]*>", "", head_one) and parse_function_head(head_one)
                  and not (re.search(r"\)\s*:", head_one) and re.search(r"\w\s*$", head_one))):
                # コンストラクターの初期化リスト内の m_b{2} は、本体ではないため、上の条件で除く。
                name, parameter_count = parse_function_head(head_one)
                function_info = {"name": name, "params": parameter_count, "max_level": 0, "open": i}
                scopes.append({"kind": "function", "level": 0, "open": i})
                head_start = i + 1
            else:
                # メンバー変数などの初期化子（float m_x{};）。中身を空白にして、文の続きとして扱う。
                end = find_matching_brace(code, i)
                if end < 0:
                    end = n - 1
                blanked = "".join("\n" if ch == "\n" else " " for ch in code[i + 1:end])
                code = code[:i + 1] + blanked + code[end:]
                i = end
        elif c == "}":
            if scopes:
                scope = scopes.pop()
                first_line, last_line = line_of[scope["open"]], line_of[min(i, len(line_of) - 1)]
                if scope["kind"] == "function":
                    body = (count_code_lines(lines, first_line + 1, last_line - 1) if last_line > first_line else 1)
                    functions.append({
                        "file": rel, "name": function_info["name"], "line": first_line + 1, "lines": body,
                        "nesting": function_info["max_level"], "params": function_info["params"]})
                    function_info = None
                elif scope["kind"] == "class":
                    body = count_code_lines(lines, first_line + 1, last_line - 1) if last_line > first_line else 1
                    classes.append({"file": rel, "name": scope["name"], "line": first_line + 1, "lines": body,
                                    "members": scope["members"]})
            head_start = i + 1
        i += 1
    return {"file": rel, "physical": physical_lines,
            "code": sum(1 for t in lines if t.strip())}, functions, classes, lines


def normalize_for_duplicates(lines):
    """識別子と数値を記号へ置き換え、変数名・定数だけが違う並びを同じと見なす。"""
    result = []
    for number, text in enumerate(lines):
        stripped = text.strip()
        if not stripped or stripped in ("{", "}", "};"):
            continue
        normalized = re.sub(r"\b\d[\w.]*\b", "N", stripped)
        normalized = re.sub(r"\b[A-Za-z_]\w*\b", lambda m: m.group(0) if m.group(0) in KEYWORDS else "I", normalized)
        result.append((number + 1, re.sub(r"\s+", "", normalized)))
    return result


def find_duplicates(file_lines, window):
    """windowの行数以上の、同じ並びを探す。戻り値は、{位置の一覧, 行数}の一覧。"""
    sequences = {rel: normalize_for_duplicates(lines) for rel, lines in file_lines.items()}
    buckets = defaultdict(list)
    for rel, seq in sequences.items():
        for k in range(len(seq) - window + 1):
            chunk = tuple(item[1] for item in seq[k:k + window])
            if len(set(chunk)) >= 4:  # 同じ行ばかり（表の行など）の並びは除く
                buckets[chunk].append((rel, k))
    covered = defaultdict(set)
    groups = []
    for chunk, places in sorted(buckets.items(), key=lambda kv: (kv[1][0][0], kv[1][0][1])):
        if len(places) < 2:
            continue
        places = [p for p in places if p[1] not in covered[p[0]]]
        if len(places) < 2:
            continue
        base_rel, base_k = places[0]
        base = sequences[base_rel]
        length = window
        while all(k + length < len(sequences[r]) and base_k + length < len(base)
                  and sequences[r][k + length][1] == base[base_k + length][1] for r, k in places[1:]):
            length += 1
        spans = [(r, k) for r, k in places]
        overlapping = any(r1 == r2 and abs(k1 - k2) < length for a, (r1, k1) in enumerate(spans) for (r2, k2) in spans[a + 1:])
        for r, k in places:
            covered[r].update(range(k, k + length - window + 1))
        if overlapping:
            continue
        groups.append({"length": length, "places": [(r, sequences[r][k][0]) for r, k in spans]})
    return sorted(groups, key=lambda g: -g["length"])


def write_csv(path, rows, fields):
    with open(path, "w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def table(rows, headers):
    if not rows:
        return "（なし）\n"
    lines = ["| " + " | ".join(headers) + " |", "|" + "---|" * len(headers)]
    lines += ["| " + " | ".join(str(v) for v in row) + " |" for row in rows]
    return "\n".join(lines) + "\n"


def build_report(limits, files, functions, classes, duplicates):
    over_fn = sorted([f for f in functions if f["lines"] > limits["max_function_lines"]], key=lambda f: -f["lines"])
    over_nest = sorted([f for f in functions if f["nesting"] > limits["max_nesting"]], key=lambda f: -f["nesting"])
    over_param = sorted([f for f in functions if f["params"] > limits["max_parameters"]], key=lambda f: -f["params"])
    over_cls = sorted([c for c in classes if c["lines"] > limits["max_class_lines"] or c["members"] > limits["max_class_members"]],
                      key=lambda c: -c["lines"])
    over_file = sorted([f for f in files if f["physical"] > limits["max_file_lines"]], key=lambda f: -f["physical"])
    out = ["# コード指標レポート（I1）", "",
           f"対象: ファイル {len(files)} 個、関数 {len(functions)} 個、クラス・構造体 {len(classes)} 個", "",
           "## 1. 基準を超えた件数", "",
           table([("関数の本体の行数", f"{limits['max_function_lines']}行", len(over_fn)),
                  ("ネストの深さ", f"{limits['max_nesting']}段", len(over_nest)),
                  ("関数の引数の数", f"{limits['max_parameters']}個", len(over_param)),
                  ("クラスの行数・メンバー数", f"{limits['max_class_lines']}行・{limits['max_class_members']}個", len(over_cls)),
                  ("ファイルの物理行数", f"{limits['max_file_lines']}行", len(over_file)),
                  ("重複（同じ並び）", f"{limits['duplicate_lines']}行以上", len(duplicates))], ["項目", "基準", "超過の件数"]), "",
           "## 2. 関数の本体の行数（上位30）", "",
           table([(f["file"], f["name"], f["line"], f["lines"]) for f in over_fn[:30]], ["ファイル", "関数", "行", "本体の行数"]), "",
           "## 3. ネストの深さ（上位30）", "",
           table([(f["file"], f["name"], f["line"], f["nesting"]) for f in over_nest[:30]], ["ファイル", "関数", "行", "深さ"]), "",
           "## 4. 引数の数（上位30）", "",
           table([(f["file"], f["name"], f["line"], f["params"]) for f in over_param[:30]], ["ファイル", "関数", "行", "引数"]), "",
           "## 5. クラス・構造体", "",
           table([(c["file"], c["name"], c["line"], c["lines"], c["members"]) for c in over_cls[:30]],
                 ["ファイル", "名前", "行", "本体の行数", "メンバー数"]), "",
           "## 6. ファイルの物理行数", "",
           table([(f["file"], f["physical"], f["code"]) for f in over_file], ["ファイル", "物理行数", "コードの行数"]), "",
           "## 7. 重複（上位30）", ""]
    rows = [(g["length"], "<br>".join(f"{r}:{n}" for r, n in g["places"])) for g in duplicates[:30]]
    out += [table(rows, ["行数", "場所（ファイル:開始行）"]), "",
            "## 8. 数え方の限界", "",
            "- 字句解析による簡易的な測定。マクロや特殊な書き方では、関数・クラスを取り違えることがある。",
            "- 関数の本体の行数は、{ と } を含む行を除き、コメントと空行を除いて数える。",
            "- ネストは、波括弧を持つ if・else・for・while・do・switch・Lambda を数える。波括弧のない制御文は数えない。",
            "- 引数は、最上位のカンマの数から求める。仮想関数のoverride・外部APIの形に合わせた宣言も含むため、例外にするかは目視で決める。",
            "- メンバー数は、クラス直下の、括弧を含まない文を数える（static・定数メンバーを含む）。",
            "- 重複は、識別子と数値を無視して比較する。表の行・失敗検出の並びも検出するため、例外にするかは目視で決める。", ""]
    return "\n".join(out)


def collect_files(source_root, excludes):
    found = []
    for path in sorted(source_root.rglob("*")):
        if path.suffix.lower() in SOURCE_SUFFIXES and not any(part in excludes for part in path.relative_to(source_root).parts):
            found.append(path)
    return found


def main():
    parser = argparse.ArgumentParser(description="GunfireLikeのコード指標を測定する。")
    parser.add_argument("--root", default=str(Path(__file__).resolve().parent.parent), help="リポジトリーのルート")
    parser.add_argument("--source", default="Source", help="ルートからのソースのフォルダー")
    parser.add_argument("--out", default="Tools/CodeMetricsOutput", help="ルートからの出力フォルダー")
    parser.add_argument("--exclude", nargs="*", default=DEFAULT_EXCLUDES, help="除外するフォルダー名")
    for key, value in DEFAULTS.items():
        parser.add_argument("--" + key.replace("_", "-"), type=int, default=value)
    args = parser.parse_args()
    limits = {key: getattr(args, key) for key in DEFAULTS}
    root = Path(args.root)
    source_root = root / args.source
    if not source_root.is_dir():
        print(f"ソースのフォルダーが見つかりません: {source_root}", file=sys.stderr)
        return 1
    files, functions, classes, file_lines = [], [], [], {}
    for path in collect_files(source_root, set(args.exclude)):
        rel = path.relative_to(root).as_posix()
        file_info, file_functions, file_classes, lines = analyze_file(path, rel)
        files.append(file_info)
        functions += file_functions
        classes += file_classes
        file_lines[rel] = lines
    duplicates = find_duplicates(file_lines, limits["duplicate_lines"])
    out_dir = root / args.out
    out_dir.mkdir(parents=True, exist_ok=True)
    write_csv(out_dir / "files.csv", files, ["file", "physical", "code"])
    write_csv(out_dir / "functions.csv", functions, ["file", "name", "line", "lines", "nesting", "params"])
    write_csv(out_dir / "classes.csv", classes, ["file", "name", "line", "lines", "members"])
    write_csv(out_dir / "duplicates.csv", [{"length": g["length"], "places": " | ".join(f"{r}:{n}" for r, n in g["places"])}
                                          for g in duplicates], ["length", "places"])
    (out_dir / "CodeMetrics_Report.md").write_text(build_report(limits, files, functions, classes, duplicates), encoding="utf-8")
    print(f"ファイル {len(files)} 個、関数 {len(functions)} 個、クラス {len(classes)} 個、重複 {len(duplicates)} 件")
    print(f"出力: {out_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
