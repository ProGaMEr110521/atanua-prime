#!/usr/bin/env python3
"""Mine the chip pinout catalog from the C++ sources.

Reads basechipfactory.cpp (palette names, categories, name->class map)
and each src/chip/*.cpp constructor (mPin push order, tooltips,
mReadOnly flags) and writes tools/chips.json.

Deterministic: chips sorted by name, fixed formatting. The test suite
regenerates to a temp file and diffs against the committed catalog, so
any chip change without re-running this script fails tests.

Usage: python tools/mine_pins.py [output.json [output.h]]
Defaults write tools/chips.json and src/include/chipdb.h.
"""
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FACTORY = os.path.join(REPO, "src", "core", "basechipfactory.cpp")
CHIPDIR = os.path.join(REPO, "src", "chip")
TAB_NAMES = ["Base", "Chips", "In", "Out", "Misc"]


def unescape_c(s):
    out = []
    i = 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            nxt = s[i + 1]
            out.append({"n": "\n", "t": "\t", '"': '"', "'": "'",
                        "\\": "\\"}.get(nxt, nxt))
            i += 2
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def main():
    out_path = (sys.argv[1] if len(sys.argv) > 1
                else os.path.join(REPO, "tools", "chips.json"))
    header = (sys.argv[2] if len(sys.argv) > 2
              else os.path.join(REPO, "src", "include", "chipdb.h"))
    with open(FACTORY, encoding="utf-8") as f:
        fac = f.read()

    varmap = {m.group(1): unescape_c(m.group(2)) for m in
              re.finditer(r'static const char \*(\w+)\s*=\s*"((?:[^"\\]|\\.)*)"',
                          fac)}

    def resolve(token):
        token = token.strip()
        if token.startswith('"'):
            return unescape_c(token[1:-1])
        return varmap[token]

    categories = {}
    for m in re.finditer(
            r"aChipList\[(\d)\]\.push_back\(mystrdup\((\w+|\"(?:[^\"\\]|\\.)*\")\)\);",
            fac):
        idx, var = int(m.group(1)), m.group(2)
        if var == "temp":
            continue
        name = resolve(var)
        categories.setdefault(name, TAB_NAMES[idx])

    # sprintf-generated palette names: button/switch '0'-'9', 'a'-'z'.
    loop_re = (r"for\s*\(\s*i\s*=\s*0\s*;\s*i\s*<\s*(\d+)\s*;"
               r"[^)]*\)\s*\{\s*sprintf\(temp,\s*(\w+),\s*'([^']*)'\s*\+\s*i\s*\);"
               r"\s*aChipList\[(\d)\]\.push_back\(mystrdup\(temp\)\);")
    for m in re.finditer(loop_re, fac):
        count, var, start, idx = (int(m.group(1)), m.group(2),
                                  m.group(3), int(m.group(4)))
        fmt = resolve(var)
        for i in range(count):
            name = fmt.replace("%c", chr(ord(start) + i))
            categories.setdefault(name, TAB_NAMES[idx])

    # Name -> (class, ctor args) from the build() strcmp chain.
    buildmap = {}
    for m in re.finditer(
            r"strcmp\(\s*(\w+|\"(?:[^\"\\]|\\.)*\")\s*,\s*aChipId\s*\)"
            r"\s*==\s*0\)"
            r"\s*return new (\w+)\(([^;]*)\);", fac):
        if m.group(1) == "temp":
            continue  # loop-generated names, expanded below
        buildmap[resolve(m.group(1))] = (m.group(2), m.group(3).strip())
    for m in re.finditer(
            r"for\s*\(\s*i\s*=\s*0\s*;\s*i\s*<\s*(\d+)\s*;"
            r"[^)]*\)\s*\{\s*sprintf\(temp,\s*(\w+),\s*'([^']*)'\s*\+\s*i\s*\);"
            r"[^}]*?strcmp\(temp,\s*aChipId\)\s*==\s*0\)"
            r"\s*return new (\w+)\(([^;]*)\);", fac, re.DOTALL):
        count, var, start, cls, args = (int(m.group(1)), m.group(2),
                                        m.group(3), m.group(4),
                                        m.group(5).strip())
        fmt = resolve(var)
        for i in range(count):
            buildmap[fmt.replace("%c", chr(ord(start) + i))] = (cls, args)
    for lit, cls, args in [("switch (' ')", "SwitchChip", "32"),
                           ("switch ('shift')", "SwitchChip", "SDLK_LSHIFT"),
                           ("switch ('return')", "SwitchChip", "SDLK_RETURN")]:
        buildmap[lit] = (cls, args)

    # Class -> source file via constructor definition.
    classfile = {}

    def find_file(cls):
        if cls in classfile:
            return classfile[cls]
        for fn in sorted(os.listdir(CHIPDIR)):
            if not fn.endswith(".cpp"):
                continue
            with open(os.path.join(CHIPDIR, fn), encoding="utf-8",
                      errors="replace") as f:
                if re.search(r"\b%s::%s\s*\(" % (cls, cls), f.read()):
                    classfile[cls] = fn
                    return fn
        classfile[cls] = None
        return None

    chips = []
    for name in sorted(set(categories) | set(buildmap)):
        cls, args = buildmap.get(name, (None, ""))
        entry = {"name": name,
                 "category": categories.get(name),
                 "class": cls,
                 "ctor_args": args,
                 "dynamic": False,
                 "pins": []}
        fn = find_file(cls) if cls else None
        entry["file"] = fn
        if fn:
            with open(os.path.join(CHIPDIR, fn), encoding="utf-8",
                      errors="replace") as f:
                body = f.read()
            # Expand one-line pin macros (e.g. 74163's DEF_PIN) textually,
            # then drop the define lines so the template push is not mined.
            macros = []
            for dm in re.finditer(
                    r"#define\s+(\w+)\s*\(([^)]*)\)\s*([^\n]*mPin\.push_back[^\n]*)",
                    body):
                macros.append((dm.group(1),
                               [p.strip() for p in dm.group(2).split(",")],
                               dm.group(3)))
            body = re.sub(r"#define[^\n]*mPin\.push_back[^\n]*\n", "", body)
            for macro, params, mbody in macros:
                def expand(mm, params=params, mbody=mbody):
                    given = [a.strip() for a in mm.group(1).split(",")]
                    out = mbody
                    for formal, actual in zip(params, given):
                        out = re.sub(r"\b%s\b" % formal, actual, out)
                    return out

                body = re.sub(r"\b%s\(([^;{}]*)\)\s*;?" % macro, expand,
                              body)
            m = re.search(r"\b%s::%s\s*\([^;]*?\)\s*[^{};]*\{" % (cls, cls),
                          body)
            if m:
                depth, j = 0, m.end() - 1
                while j < len(body):
                    if body[j] == "{":
                        depth += 1
                    elif body[j] == "}":
                        depth -= 1
                        if depth == 0:
                            break
                    j += 1
                ctor = body[m.end():j]
                raw = re.findall(r"mPin\.push_back\(&([\w\[\]]+)\)", ctor)
                pushes = [p for p in raw
                          if not re.fullmatch(r"\w+\[i\]", p)]
                # Loop pushes (&mFoo[i]) expand via a literal bound or the
                # ctor size argument (ledgrid style: 2 * first build arg).
                loopm = re.search(
                    r"for\s*\([^;]*;\s*\w+\s*<\s*(.+?)\s*;"
                    r"[^)]*\)\s*(?:\{\s*)?mPin\.push_back\(&(\w+)\[i\]\);",
                    ctor)
                if loopm:
                    bound, base = loopm.group(1), loopm.group(2)
                    count = None
                    if "mSize" in bound:
                        try:
                            count = 2 * int(args.split(",")[0].strip())
                        except (ValueError, IndexError):
                            count = None
                    elif re.fullmatch(r"\d+", bound.strip()):
                        count = int(bound.strip())
                    if count is not None:
                        pushes.extend("%s[%d]" % (base, k)
                                      for k in range(count))
                    else:
                        print("warning: unexpandable loop in %s" % fn,
                              file=sys.stderr)
                base_ro = set()
                for b in re.finditer(r"(\w+)\[\w+\]\.mReadOnly\s*=\s*1", ctor):
                    base_ro.add(b.group(1))
                for idx, member in enumerate(pushes):
                    sets = re.findall(
                        re.escape(member) +
                        r"\.set\((?:[^;\"']|\"(?:[^\"\\]|\\.)*\")+\)", ctor)
                    label = ""
                    if sets:
                        lits = re.findall(r'"((?:[^"\\]|\\.)*)"', sets[0])
                        if lits:
                            label = unescape_c(lits[-1])
                    base = member.split("[")[0]
                    readonly = bool(
                        re.search(re.escape(member) + r"\.mReadOnly\s*=\s*1",
                                  ctor) or base in base_ro)
                    # Behavioral direction from the whole file: a pin the
                    # chip writes is an output, one it only reads is an
                    # input, both is bidir (e.g. MCU ports).
                    writes = bool(
                        re.search(re.escape(member) + r"\.setState", body) or
                        re.search(r"\b%s\[\w+\]\.setState" % base, body))
                    reads = bool(
                        re.search(re.escape(member) + r"\.mNet", body) or
                        re.search(r"\b%s\[\w+\]\.mNet" % base, body))
                    if writes and not reads:
                        role = "out"
                    elif reads and not writes:
                        role = "in"
                    elif writes and reads:
                        role = "bidir"
                    else:
                        low = label.lower()
                        if "output" in low:
                            role = "out"
                        elif any(k in low for k in
                                 ("input", "select", "enable", "address",
                                  "clock", "clear", "reset", "preset",
                                  "load")):
                            role = "in"
                        else:
                            role = "unknown"
                    if readonly:
                        role = "in"
                    entry["pins"].append({
                        "index": idx,
                        "member": member,
                        "label": label,
                        "readonly": readonly,
                        "role": role,
                    })
        else:
            print("warning: no source for class %r (chip %r)" % (cls, name),
                  file=sys.stderr)
        chips.append(entry)

    # Box: any *.atanua name builds a Box whose pins come from the subfile.
    chips.append({"name": "*.atanua (Box)", "category": None, "class": "Box",
                  "ctor_args": "", "dynamic": True, "file": "box.cpp",
                  "pins": []})
    chips.sort(key=lambda c: c["name"])
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump({"version": 1, "chips": chips}, f, indent=2,
                  ensure_ascii=False)
        f.write("\n")
    print("wrote %s (%d chips)" % (out_path, len(chips)))

    # C table for the headless validator (name -> pin count). Same data,
    # no JSON parser needed in the app. Regenerated together; the drift
    # test covers both files.

    def cstr(s):
        return '"%s"' % s.replace("\\", "\\\\").replace('"', '\\"')

    with open(header, "w", encoding="utf-8") as f:
        f.write("#ifndef CHIPDB_GEN_H\n#define CHIPDB_GEN_H\n")
        f.write("/* Generated by tools/mine_pins.py - do not edit. */\n")
        f.write("#include <string.h>\n\n")
        f.write("struct ChipDbEntry { const char *name; int pins; };\n\n")
        f.write("static const ChipDbEntry g_chipDb[] = {\n")
        for c in chips:
            if c["dynamic"]:
                continue  # Box subfiles resolve pins at load, not here
            f.write("    {%s, %d},\n" % (cstr(c["name"]), len(c["pins"])))
        f.write("};\n\n")
        f.write("static const int g_chipDbCount = "
                "sizeof(g_chipDb) / sizeof(g_chipDb[0]);\n\n")
        f.write("/* ASCII case-insensitive compare (chip names are ASCII). */\n")
        f.write("static int chipNameEq(const char *a, const char *b)\n")
        f.write("{\n")
        f.write("    int i;\n")
        f.write("    if (!a || !b)\n")
        f.write("        return 0;\n")
        f.write("    for (i = 0; a[i] && b[i]; i++)\n")
        f.write("    {\n")
        f.write("        char ca = a[i], cb = b[i];\n")
        f.write("        if (ca >= 'A' && ca <= 'Z')\n")
        f.write("            ca += 'a' - 'A';\n")
        f.write("        if (cb >= 'A' && cb <= 'Z')\n")
        f.write("            cb += 'a' - 'A';\n")
        f.write("        if (ca != cb)\n")
        f.write("            return 0;\n")
        f.write("    }\n")
        f.write("    return a[i] == b[i];\n")
        f.write("}\n\n")
        f.write("/* Pin count for a chip name, or -1 when unknown. */\n")
        f.write("static int chipPinCount(const char *aName)\n")
        f.write("{\n")
        f.write("    int i;\n")
        f.write("    for (i = 0; i < g_chipDbCount; i++)\n")
        f.write("        if (chipNameEq(g_chipDb[i].name, aName))\n")
        f.write("            return g_chipDb[i].pins;\n")
        f.write("    return -1;\n")
        f.write("}\n\n#endif\n")
    print("wrote %s" % header)


if __name__ == "__main__":
    main()
