#!/usr/bin/env python3
"""Generate the MFC tree item inventory the ResourceModel port is checked against.

The port of Sources/src/editor's tree items must carry the same property tables
and write the same project XML as the MFC classes (D-04, D-07). Hand-copied lists
drift, so this script reads the MFC sources and writes
tools/zig/fixtures/resource_editor/mfc-item-inventory.json, which
tools/zig/resource_model_test.cpp asserts every port item class against.

Sources read:
  * Sources/src/editor/TreeItem.h            - the ETreeItemType enum (ClassTypeIDs)
  * Sources/src/editor/TreeItemFactory.cpp   - REGISTER_CLASS: ClassTypeID -> class
  * Sources/src/editor/*TreeItem.{h,cpp}     - class bases, InitDefaultValues bodies
    (props in push order and default children), operator&( IDataTree & ) overrides
  * Sources/src/editor/*Frm.cpp, GUIFrame.cpp - project root tag and extension, and the
    elements the frames' SaveMyData adds to an item's serialisation
  * Sources/src/editor/COI/CtrlObjectInspector.h - the DT_* domain type numbers

InitDefaultValues is executed symbolically: every `prop.<field> = ...` updates a
current SProp, every `defaultValues.push_back( prop )` appends a copy. A statement
the interpreter does not understand is recorded under `unparsed` for its class
instead of being dropped, and --check fails if any appears, so an unmodelled
construct can never shrink a property table unnoticed.

Usage:
  python3 tools/zig/mfc_item_inventory.py          write the inventory
  python3 tools/zig/mfc_item_inventory.py --check  fail if the tracked inventory is stale,
                                                   a statement is unparsed, or the review's
                                                   spot counts (weapon 48, mesh 108) differ
"""

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EDITOR = ROOT / "Sources" / "src" / "editor"
OUTPUT = ROOT / "tools" / "zig" / "fixtures" / "resource_editor" / "mfc-item-inventory.json"

# Totals the 2026-10-05 review counted (.gsd/OVERRIDES.md): 48 for the weapon and
# 108 for the mesh sub-editor. They are `grep -c szDefaultName` over the .cpp, so
# they count every named InitDefaultValues entry: props (42 / 82) plus default
# children (6 / 26). If the parser disagrees, it is the parser that is wrong.
SPOT_COUNTS = {"WeaponTreeItem": 48, "MeshTreeItem": 108}

# The frame-existence guard several items open InitDefaultValues with. With no
# frame (a batch tool, or the port) MFC returns an empty table; the editor always
# has its frame, so the props after the guard are the table. Recorded, not run.
FRAME_GUARD_RE = re.compile(r"if\s*\(\s*!\s*g_frameManager\.GetFrame\s*\([^)]*\)\s*\)\s*\{\s*values\s*=\s*defaultValues\s*;\s*return\s*;\s*\}")

# Element names CTreeItem::operator&( IDataTree & ) (TreeItem.cpp) writes, in order.
# The class attribute on the enclosing <item> comes from the CPtr container writer
# (Sources/src/StreamIO/DTHelper.h AddInternal): it writes ClassTypeID and reads
# ClassTypeID, falling back to the older writer's `type` (WinSniper.unt has `type`).
ITEM_TYPE_ATTRIBUTE = "ClassTypeID"
LEGACY_ITEM_TYPE_ATTRIBUTE = "type"
BASE_ELEMENTS = ["default_name", "display_name", "values", "expand", "childs"]
# SProp::operator&( IDataTree & ): one <item> per prop with these two elements.
PROP_ELEMENTS = ["default_name", "value"]
# The MFC IDataTree XML writer puts a scalar on the enclosing element as an
# attribute (type="...", expand="0", float_value="...") and everything else -
# strings, vectors, containers - in a child element. An override's extra field
# lands on one side or the other by its member's declared type.
SCALAR_TYPES = {"int", "float", "bool", "double", "DWORD", "WORD", "BYTE", "short", "long", "unsigned", "char"}


def read(path):
    # The sources are windows-1251 (Russian comments, display names). latin-1
    # keeps every byte so string literals survive as the bytes MFC wrote.
    return path.read_bytes().decode("latin-1").replace("\r\n", "\n")


def strip_comments(text):
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def match_brace(text, open_index):
    depth, i, n = 0, open_index, len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            i = j + 1
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ValueError("unbalanced braces")


def split_statements(body):
    """Top-level statements of a function body, `;`-separated, strings respected."""
    stmts, cur, i, n, depth = [], [], 0, len(body), 0
    while i < n:
        c = body[i]
        if c == '"' or c == "'":
            j = i + 1
            while j < n and body[j] != c:
                j += 2 if body[j] == "\\" else 1
            cur.append(body[i:j + 1])
            i = j + 1
            continue
        if c in "({[":
            depth += 1
        elif c in ")}]":
            depth -= 1
        if c == ";" and depth == 0:
            stmts.append("".join(cur).strip())
            cur = []
        elif c == "{" and depth == 1 and "".join(cur).strip() == "":
            pass
        else:
            cur.append(c)
        i += 1
    tail = "".join(cur).strip()
    if tail:
        stmts.append(tail)
    return [" ".join(s.split()) for s in stmts if s.strip()]


def c_string(literal):
    """Decode a C string literal (or adjacent literals) to text."""
    parts = re.findall(r'"((?:[^"\\]|\\.)*)"', literal)
    out = []
    for p in parts:
        out.append(re.sub(r"\\(.)", lambda m: {"n": "\n", "t": "\t", "\\": "\\", '"': '"'}.get(m.group(1), m.group(1)), p))
    return "".join(out)


def is_string_literal(expr):
    return re.fullmatch(r'(\s*"(?:[^"\\]|\\.)*"\s*)+', expr) is not None


def parse_enum_values(text, prefix, base_name=None):
    values, base, nxt = {}, 0, 0
    for m in re.finditer(r"\b(" + prefix + r"\w*)\s*(?:=\s*([^,}\n]+))?\s*[,}\n]", text):
        name, expr = m.group(1), (m.group(2) or "").strip()
        if expr:
            expr = expr.split("//")[0].strip()
            if base_name and base_name in expr:
                off = re.search(r"\+\s*(0x[0-9a-fA-F]+|\d+)", expr)
                val = base + (int(off.group(1), 0) if off else 0)
            else:
                val = int(expr, 0)
            if name == base_name:
                base = val
        else:
            val = nxt
        values.setdefault(name, val)
        nxt = val + 1
    return values


def parse_tree_item_types():
    text = strip_comments(read(EDITOR / "TreeItem.h"))
    m = re.search(r"EDITOR_TREE_BASE_VALUE\s*=\s*(0x[0-9a-fA-F]+)", text)
    base = int(m.group(1), 16)
    types = {}
    for m in re.finditer(r"\b(E_\w+)\s*=\s*EDITOR_TREE_BASE_VALUE\s*(?:\+\s*(\d+))?", text):
        types[m.group(1)] = base + int(m.group(2) or 0)
    return types


def parse_domen_types():
    text = strip_comments(read(EDITOR / "COI" / "CtrlObjectInspector.h"))
    start = text.index("DT_ERROR")
    block = text[text.rfind("{", 0, start) + 1:text.index("}", start)]
    values, nxt = {}, 0
    for entry in block.split(","):
        entry = entry.strip()
        if not entry:
            continue
        name, _, expr = entry.partition("=")
        name = name.strip()
        val = int(expr.strip(), 0) if expr.strip() else nxt
        values[name] = val
        nxt = val + 1
    return values


def parse_factory():
    text = strip_comments(read(EDITOR / "TreeItemFactory.cpp"))
    return [(m.group(1), m.group(2)) for m in re.finditer(r"REGISTER_CLASS\s*\(\s*this\s*,\s*(E_\w+)\s*,\s*(C\w+)\s*\)", text)]


def parse_frames():
    """Project extension and root tag per CParentFrame subclass."""
    frames = []
    for path in sorted(EDITOR.glob("*.cpp")):
        text = read(path)
        ext = re.search(r'szExtension\s*=\s*"\*\.(\w+)"', text)
        tag = re.search(r'szComposerSaveName\s*=\s*"(\w+)"', text)
        root = re.search(r"nTreeRootItemID\s*=\s*(E_\w+)", text)
        if ext and tag:
            frames.append({"extension": ext.group(1), "root_tag": tag.group(1), "root_type_name": root.group(1) if root else None,
                           "frame_source": path.name})
    return frames


def parse_frame_save_my_data():
    """Elements a frame's SaveMyData( item, saver ) adds, keyed by the item class."""
    out = {}
    for path in sorted(EDITOR.glob("*.cpp")):
        text = strip_comments(read(path))
        for m in re.finditer(r"void\s+C\w+::SaveMyData\s*\(\s*(C\w+)\s*\*\s*\w+\s*,\s*CTreeAccessor\s+\w+\s*\)\s*\{", text):
            body = text[m.end() - 1:match_brace(text, m.end() - 1)]
            out[m.group(1)] = re.findall(r'\.Add\s*\(\s*"(\w+)"\s*,\s*&\s*(?:\w+->)?(\w+)', body)
    return out


CLASS_RE = re.compile(r"\bclass\s+(C\w+)\s*:\s*public\s+(C\w+)\s*\{")


def parse_class_headers(paths):
    classes = {}
    for path in paths:
        text = strip_comments(read(path))
        for m in CLASS_RE.finditer(text):
            end = match_brace(text, m.end() - 1)
            body = text[m.end():end]
            members = {mm.group(2): mm.group(1) for mm in re.finditer(r"^\s*([A-Za-z_][\w:<>, ]*?)\s+(\w+)\s*;", body, re.M)}
            classes[m.group(1)] = {
                "members": members,
                "base": m.group(2),
                "header": path.name,
                # None when the class does not assign it; then a base's constructor decides.
                "serialize_childs": (lambda sm: None if not sm else sm[-1] == "true")(re.findall(r"bSerializeChilds\s*=\s*(true|false)", body)),
                "overrides_data_tree": bool(re.search(r"operator\s*&\s*\(\s*IDataTree\s*&", body)),
                "inline_init_default_values": bool(re.search(r"InitDefaultValues\s*\(\s*\)\s*\{", body)),
            }
    return classes


def parse_data_tree_overrides(paths):
    """Element names each operator&( IDataTree & ) override adds after AddTypedSuper."""
    out = {}
    for path in paths:
        text = strip_comments(read(path))
        for m in re.finditer(r"int\s+(C\w+)::operator\s*&\s*\(\s*IDataTree\s*&\s*\w+\s*\)\s*\{", text):
            body = text[m.end() - 1:match_brace(text, m.end() - 1)]
            out[m.group(1)] = {
                "typed_super": "AddTypedSuper" in body,
                "elements": re.findall(r'saver\.Add\s*\(\s*"(\w+)"\s*,\s*&\s*(?:\w+->)?(\w+)', body),
                "frame_data": bool(re.search(r"->\s*SaveMyData\s*\(", body)),
                "source": path.name,
            }
    return out


def ai_class_combo():
    """The strings Reference.cpp's LoadAIClassCombo pushes, in order."""
    text = strip_comments(read(EDITOR / "Reference.cpp"))
    m = re.search(r"void LoadAIClassCombo\(\s*SProp \*pProp\s*\)\s*\{(.*?)\}", text, re.S)
    if not m:
        raise SystemExit("Reference.cpp: LoadAIClassCombo not found")
    return [c_string(a) for a in re.findall(r'pProp->szStrings\.push_back\(\s*("(?:[^"\\]|\\.)*")\s*\)', m.group(1))]


class Interp:
    """Symbolic executor for one InitDefaultValues body."""

    def __init__(self, enum_types):
        self.enum_types = enum_types
        self.props = []
        self.childs = []
        self.prop = self.new_prop()
        self.child = {"type_name": None, "default_name": None, "display_name": None}
        self.unparsed = []
        self.values = []
        self.locals = {}
        self.keyframe = {}
        self.pair = None
        self.frames = []

    @staticmethod
    def new_prop():
        return {"id": None, "domen": None, "default_name": None, "display_name": None, "value": None, "strings": []}

    def value_of(self, expr):
        # `(int)0xff808080` is a negative int in MFC's CVariant; `(int64) 0` is 0.
        m = re.fullmatch(r"\( ?(int|int64) ?\) ?(-?(?:0x[0-9a-fA-F]+|\d+))", expr)
        if m:
            v = int(m.group(2), 0)
            if m.group(1) == "int" and v >= 1 << 31:
                v -= 1 << 32
            return {"kind": "int", "text": str(v)}
        if is_string_literal(expr):
            return {"kind": "str", "text": c_string(expr)}
        num = expr.rstrip("fF") if re.fullmatch(r"-?\d+\.\d*[fF]?|-?\d*\.\d+[fF]?|-?\d+[fF]", expr) else None
        if num is not None:
            return {"kind": "float", "text": num}
        if re.fullmatch(r"-?(0x[0-9a-fA-F]+|\d+)", expr):
            return {"kind": "int", "text": str(int(expr, 0))}
        if expr in ("true", "false"):
            return {"kind": "bool", "text": expr}
        return {"kind": "expr", "text": expr}

    def runtime_string(self, expr):
        # A combo/browse entry computed at run time (file filters, editor data
        # dirs, AI class lists). The port must produce the same list from the same
        # source; the inventory keeps the expression so the test can name it.
        m = re.fullmatch(r"(\w+)\.c_str\(\)", expr)
        if m and m.group(1) in self.locals:
            expr = self.locals[m.group(1)]
        return {"runtime": expr}

    def run(self, stmts):
        for s in stmts:
            if not self.step(s):
                self.unparsed.append(s)

    def step(self, s):
        if s in ("SProp prop", "SChildItem child", "defaultValues.clear()", "defaultChilds.clear()",
                 "values = defaultValues", "prop.szStrings.clear()", "return"):
            if s == "prop.szStrings.clear()":
                self.prop["strings"] = []
            return True
        m = re.fullmatch(r"prop\.(\w+) = (.+)", s)
        if m:
            field, expr = m.groups()
            if field == "nId":
                self.prop["id"] = int(expr, 0)
            elif field == "nDomenType":
                self.prop["domen"] = expr
            elif field in ("szDefaultName", "szDisplayName") and is_string_literal(expr):
                self.prop["default_name" if field == "szDefaultName" else "display_name"] = c_string(expr)
            elif field == "value":
                self.prop["value"] = self.value_of(expr)
            else:
                return False
            return True
        m = re.fullmatch(r"prop\.szStrings\.push_back\( ?(.+?) ?\)", s)
        if m:
            arg = m.group(1)
            self.prop["strings"].append(c_string(arg) if is_string_literal(arg) else self.runtime_string(arg))
            return True
        m = re.fullmatch(r"if \( ?(.+?) ?\) prop\.szStrings\.push_back\( ?(.+?) ?\)", s)
        if m:
            entry = self.runtime_string(m.group(2))
            entry["if"] = m.group(1)
            self.prop["strings"].append(entry)
            return True
        m = re.fullmatch(r"else prop\.szStrings\.push_back\( ?(.+?) ?\)", s)
        if m and self.prop["strings"] and isinstance(self.prop["strings"][-1], dict) and "if" in self.prop["strings"][-1]:
            arg = m.group(1)
            self.prop["strings"][-1]["else"] = c_string(arg) if is_string_literal(arg) else arg
            return True
        # LoadAIClassCombo (Reference.cpp) appends fixed literals, so they are
        # read from its body; FillVectorOfSides reads partys.xml at run time.
        if re.fullmatch(r"LoadAIClassCombo\( ?&prop ?\)", s):
            self.prop["strings"].extend(ai_class_combo())
            return True
        m = re.fullmatch(r"(FillVectorOfSides)\( ?prop\.szStrings ?\)", s)
        if m:
            self.prop["strings"].append({"runtime": m.group(1) + "()"})
            return True
        m = re.fullmatch(r"(?:std::)?string (\w+) = (.+)", s)
        if m:
            self.locals[m.group(1)] = m.group(2)
            return True
        m = re.fullmatch(r"(\w+) \+= (.+)", s)
        if m and m.group(1) in self.locals:
            self.locals[m.group(1)] += " + " + m.group(2)
            return True
        if re.fullmatch(r"CParentFrame \*p = g_frameManager\.GetActiveFrame\(\)", s):
            return True
        if s == "defaultValues = values":
            self.props = [dict(p) for p in self.values]
            return True
        if s == "values.clear()":
            self.values = []
            return True
        # CKeyFrameTreeItem members: the curve's axis ranges and its default keys,
        # which operator& writes under Key_frames.
        m = re.fullmatch(r"(f(?:Min|Max)Val[XY]|fStep[XY]|bResizeMode) = (.+)", s)
        if m:
            self.keyframe[m.group(1)] = self.value_of(m.group(2))
            return True
        m = re.fullmatch(r"(?:std::)?pair<float, ?float> para\( ?(.+?) ?, ?(.+?) ?\)", s)
        if m:
            self.pair = [self.value_of(m.group(1))["text"], self.value_of(m.group(2))["text"]]
            return True
        m = re.fullmatch(r"para\.(first|second) = (.+)", s)
        if m and self.pair is not None:
            self.pair[0 if m.group(1) == "first" else 1] = self.value_of(m.group(2))["text"]
            return True
        if s == "framesList.push_back( para )" or s == "framesList.push_back(para)":
            self.frames.append(list(self.pair))
            return True
        if s == "framesList.clear()":
            self.frames = []
            return True
        if re.fullmatch(r"defaultValues\.push_back\( ?prop ?\)", s):
            p = dict(self.prop)
            p["strings"] = list(self.prop["strings"])
            self.props.append(p)
            return True
        m = re.fullmatch(r"child\.(\w+) = (.+)", s)
        if m:
            field, expr = m.groups()
            if field == "nChildItemType" and expr in self.enum_types:
                self.child["type_name"] = expr
            elif field in ("szDefaultName", "szDisplayName") and is_string_literal(expr):
                self.child["default_name" if field == "szDefaultName" else "display_name"] = c_string(expr)
            else:
                return False
            return True
        if re.fullmatch(r"defaultChilds\.push_back\( ?child ?\)", s):
            self.childs.append(dict(self.child))
            return True
        return False


def parse_inserts(paths, registered):
    """Which registered item each item's handlers create with `new` (MyKeyDown,
    MyRButtonClick, ...): the parent/child pairs the insert test reproduces."""
    out = {}
    for path in paths:
        text = strip_comments(read(path))
        for m in re.finditer(r"\b\w[\w\s\*&]*?\s(C\w+)::(\w+)\s*\([^;{)]*\)\s*\{", text):
            body = text[m.end() - 1:match_brace(text, m.end() - 1)]
            for child in re.findall(r"\bnew\s+(C\w+)\b", body):
                if child in registered and m.group(1) in registered:
                    lst = out.setdefault(m.group(1), [])
                    if child not in lst:
                        lst.append(child)
    return out


def parse_init_default_values(paths, enum_types):
    out = {}
    for path in paths:
        text = strip_comments(read(path))
        for m in re.finditer(r"void\s+(C\w+)::InitDefaultValues\s*\(\s*\)\s*\{", text):
            body = text[m.end():match_brace(text, m.end() - 1)]
            guarded = bool(FRAME_GUARD_RE.search(body))
            body = FRAME_GUARD_RE.sub("", body)
            interp = Interp(enum_types)
            interp.run(split_statements(body))
            out[m.group(1)] = {"source": path.name, "props": interp.props, "childs": interp.childs, "unparsed": interp.unparsed,
                               "frame_guard": guarded, "keyframe": interp.keyframe, "key_frames": interp.frames}
    return out


def build_inventory():
    enum_types = parse_tree_item_types()
    domen = parse_domen_types()
    factory = parse_factory()
    # Every header the factory includes declares registered classes (this also
    # brings in localization.h for CLocalizationItem); each has a .cpp partner.
    factory_text = read(EDITOR / "TreeItemFactory.cpp")
    item_hs = {EDITOR / h for h in re.findall(r'#include\s+"(\w+\.h)"', factory_text) if (EDITOR / h).exists()}
    item_hs |= set(EDITOR.glob("*TreeItem.h"))
    item_hs.discard(EDITOR / "TreeItemFactory.h")
    item_cpps = {h.with_suffix(".cpp") for h in item_hs if h.with_suffix(".cpp").exists()}
    item_cpps = sorted(item_cpps, key=lambda p: p.name)
    item_hs = sorted(item_hs, key=lambda p: p.name)
    headers = parse_class_headers(item_hs)
    overrides = parse_data_tree_overrides(item_cpps)
    inits = parse_init_default_values(item_cpps, enum_types)
    frame_data = parse_frame_save_my_data()
    registered = {cls for _, cls in factory}
    inserts = parse_inserts(item_cpps, registered)
    inserted_by = {}
    for parent, kids in inserts.items():
        for kid in kids:
            inserted_by.setdefault(kid, []).append(parent)

    def resolve(cls, key, default):
        # Walk the base chain: a class without its own InitDefaultValues or
        # operator& inherits the nearest base's.
        seen = cls
        while seen:
            table = inits if key == "init" else overrides
            if seen in table:
                return seen, table[seen]
            seen = headers.get(seen, {}).get("base")
            if seen == "CTreeItem":
                break
        return None, default

    classes = []
    for type_name, cls in factory:
        if type_name not in enum_types:
            raise SystemExit(f"factory registers {cls} under {type_name}, which TreeItem.h does not define")
        header = headers.get(cls)
        if header is None:
            raise SystemExit(f"no class declaration found for {cls}")
        init_owner, init = resolve(cls, "init", {"source": None, "props": [], "childs": [], "unparsed": [], "frame_guard": False, "keyframe": {}, "key_frames": []})
        over_owner, over = resolve(cls, "over", None)
        # A constructor body runs after its base's, so the nearest class that
        # assigns bSerializeChilds decides (CTemplatesTreeItem's false holds for
        # CStaticsTreeItem); CTreeItem's own default is true.
        serialize_childs, seen = None, cls
        while seen and seen != "CTreeItem" and serialize_childs is None:
            serialize_childs = headers.get(seen, {}).get("serialize_childs")
            seen = headers.get(seen, {}).get("base")
        serialize_childs = True if serialize_childs is None else serialize_childs
        extra = []
        if over:
            if not over["typed_super"]:
                raise SystemExit(f"{over_owner}::operator&( IDataTree & ) does not call AddTypedSuper; model it explicitly")
            fields = list(over["elements"])
            if over["frame_data"]:
                fields += frame_data.get(over_owner, [])
            owner_members = headers[over_owner]["members"]
            for name, member in fields:
                mtype = owner_members.get(member)
                if mtype is None:
                    raise SystemExit(f"{over_owner}: no member declaration for {member} (written as {name})")
                extra.append({"name": name, "member": member, "member_type": mtype,
                              "as": "attribute" if mtype.split()[-1] in SCALAR_TYPES else "element"})
        # MFC writes, in call order, onto the <item> the container opened with its
        # ClassTypeID attribute: default_name, display_name, values, expand, childs, then
        # the override's fields. Scalars become attributes, the rest elements.
        attributes = [ITEM_TYPE_ATTRIBUTE, "expand"] + [e["name"] for e in extra if e["as"] == "attribute"]
        elements = ["default_name", "display_name", "values"] + (["childs"] if serialize_childs else [])
        elements += [e["name"] for e in extra if e["as"] == "element"]
        props = []
        for order, p in enumerate(init["props"]):
            if p["domen"] not in domen:
                raise SystemExit(f"{cls}: prop {p['default_name']!r} has unknown domain type {p['domen']!r}")
            props.append({
                "order": order,
                "id": p["id"],
                "default_name": p["default_name"],
                "domen_type": p["domen"],
                "domen_value": domen[p["domen"]],
                "default": p["value"],
                "strings": p["strings"],
            })
        classes.append({
            "class": cls,
            "type_name": type_name,
            "type_id": enum_types[type_name],
            "header": header["header"],
            "source": init["source"],
            "init_from": init_owner,
            "props": props,
            "default_childs": [{"type_name": c["type_name"], "type_id": enum_types[c["type_name"]], "default_name": c["default_name"]} for c in init["childs"]],
            "serialise": {
                "attributes": attributes,
                "elements": elements,
                "extra_fields": extra,
                "serialize_childs": serialize_childs,
                "override_from": over_owner,
            },
            "inserts": inserts.get(cls, []),
            "inserted_by": inserted_by.get(cls, []),
            "frame_guard": init["frame_guard"],
            "unparsed": init["unparsed"],
        })
        if init["keyframe"] or init["key_frames"]:
            classes[-1]["keyframe"] = {"members": init["keyframe"], "default_frames": init["key_frames"]}

    # Per source file, over every InitDefaultValues body in it (registered or
    # not), the props and default children - the review's unit of counting.
    # The sub-editor (frame) each class belongs to: the frame whose root item is
    # declared in the same header; for the shared classes (CLocalizationItem,
    # CKeyFrameTreeItem) the editor of the class that inserts or lists them.
    frames = parse_frames()
    for f in frames:
        f["root_type_id"] = enum_types.get(f["root_type_name"])
        f["root_class"] = next((c["class"] for c in classes if c["type_name"] == f["root_type_name"]), None)
    frame_by_header = {}
    for f in frames:
        root_cls = next((c for c in classes if c["class"] == f["root_class"]), None)
        if root_cls:
            frame_by_header.setdefault(root_cls["header"], f)
    by_class = {c["class"]: c for c in classes}
    for c in classes:
        f = frame_by_header.get(c["header"])
        if f is None:
            owners = c["inserted_by"] + [o["class"] for o in classes if any(d["type_name"] == c["type_name"] for d in o["default_childs"])]
            owners += [o["class"] for o in classes if headers.get(o["class"], {}).get("base") == c["class"]]
            f = next((frame_by_header[by_class[o]["header"]] for o in owners if by_class[o]["header"] in frame_by_header), None)
        if f is None:
            raise SystemExit(f"{c['class']}: cannot tell which sub-editor it belongs to")
        c["editor"] = {"extension": f["extension"], "root_tag": f["root_tag"], "root_class": f["root_class"], "root_type_id": f["root_type_id"]}

    by_source = {}
    for cls, init in sorted(inits.items()):
        stem = init["source"].rsplit(".", 1)[0]
        entry = by_source.setdefault(stem, {"props": 0, "default_childs": 0, "named_entries": 0})
        entry["props"] += len(init["props"])
        entry["default_childs"] += len(init["childs"])
        entry["named_entries"] = entry["props"] + entry["default_childs"]

    return {
        "generated_by": "tools/zig/mfc_item_inventory.py",
        "note": "Generated from Sources/src/editor. Do not edit by hand; run the script.",
        "tree_item_base_value": 0x11000000,
        "base_elements": BASE_ELEMENTS,
        "item_type_attribute": ITEM_TYPE_ATTRIBUTE,
        "legacy_item_type_attribute": LEGACY_ITEM_TYPE_ATTRIBUTE,
        "prop_elements": PROP_ELEMENTS,
        "domen_types": domen,
        "frames": frames,
        "prop_counts_by_source": dict(sorted(by_source.items())),
        "classes": classes,
    }


def render(inventory):
    # CRLF like every tracked text file (.gitattributes).
    return (json.dumps(inventory, indent=1, ensure_ascii=False) + "\n").replace("\n", "\r\n").encode("latin-1")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true", help="verify instead of writing")
    args = ap.parse_args()
    inventory = build_inventory()
    errors = []
    for stem, expected in SPOT_COUNTS.items():
        got = inventory["prop_counts_by_source"].get(stem, {}).get("named_entries")
        if got != expected:
            errors.append(f"spot count {stem}: expected {expected} named entries (props + default children), parser found {got}")
    for c in inventory["classes"]:
        for s in c["unparsed"]:
            errors.append(f"{c['class']}: unparsed InitDefaultValues statement: {s}")
    data = render(inventory)
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_bytes() != data:
            errors.append(f"{OUTPUT.relative_to(ROOT)} is stale; run python3 tools/zig/mfc_item_inventory.py")
    else:
        OUTPUT.write_bytes(data)
    n_props = sum(len(c["props"]) for c in inventory["classes"])
    print(f"mfc_item_inventory: {len(inventory['classes'])} classes, {n_props} props, {len(inventory['frames'])} frames")
    for stem, count in inventory["prop_counts_by_source"].items():
        print(f"  {stem}: {count['props']} props + {count['default_childs']} default children = {count['named_entries']}")
    for e in errors:
        print(f"FAIL {e}", file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
