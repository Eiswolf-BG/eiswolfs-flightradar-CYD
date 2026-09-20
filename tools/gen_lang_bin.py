#!/usr/bin/env python3
"""Generates assets/lang_XX.bin from src/i18n_XX.h for all 8 languages.

Run this manually whenever translated UI text changes (new StringIds
added, or existing wording corrected) - NOT part of the firmware build.
After running, bump Config::I18N_CONTENT_VERSION in src/config.h if this
run did NOT add/remove any StringId (i.e. StringId::COUNT is unchanged)
but the wording of an EXISTING string changed - otherwise devices would
never notice the change and keep serving the old cached SD copy forever.
If StringId::COUNT changed, no manual version bump is needed - the count
itself already makes old files detectably stale (see i18n.cpp).

Format (see i18n.cpp for the firmware-side parser, kept in sync manually):
  offset 0: magic "LNG2" (4 bytes)
  offset 4: format version (1 byte) = 1
  offset 5: language index (1 byte, EN=0..NL=7, matches I18n::TABLES order)
  offset 6: content version (uint16 LE) = Config::I18N_CONTENT_VERSION
  offset 8: string count (uint16 LE) = StringId::COUNT at generation time
  offset 10: [count] x uint16 LE byte-lengths (one per string, in StringId order)
  offset 10+count*2: concatenated UTF-8 string bytes, back to back, NOT
                      null-terminated (the firmware inserts terminators when
                      loading into RAM)

BUGFIX HISTORY (v6.7.6): an earlier version of this script used regex
.match() (first hit only) instead of .finditer() when a source line packed
multiple short string literals together (e.g. the compass abbreviations
"N", "NE", "E", ... on one line in i18n_en.h). That silently dropped 7
strings and shifted every translation after that point to the wrong
StringId in all 8 generated files. The firmware's StringId::COUNT check
caught the mismatch and fell back to English rather than showing broken
text, but the language files never actually worked until this was fixed.
Always cross-check the printed count against StringId::COUNT (see
count_enum_entries() below, parsed independently from i18n.h) before
trusting a generated file.
"""
import os
import re
import struct
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(SCRIPT_DIR, "..", "src")
ASSETS_DIR = os.path.join(SCRIPT_DIR, "..", "assets")

# Must match I18n::TABLES order in i18n.cpp exactly.
LANGS = ["en", "de", "fr", "tr", "es", "it", "pt", "nl"]
CONTENT_VERSION = 1  # must match Config::I18N_CONTENT_VERSION in config.h

STRING_LITERAL_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def unescape_c_string(s):
    # Handle the small set of escapes actually used in these files.
    out = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s):
            nxt = s[i + 1]
            if nxt == 'n':
                out.append('\n')
            elif nxt == 't':
                out.append('\t')
            elif nxt == '"':
                out.append('"')
            elif nxt == '\\':
                out.append('\\')
            else:
                out.append(nxt)
            i += 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def extract_strings(path):
    strings = []
    in_array = False
    with open(path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not in_array:
                if line.startswith("static const char* const I18N_"):
                    in_array = True
                continue
            if line.startswith("};"):
                break
            if line.startswith("//"):
                continue
            if not line.startswith('"'):
                continue
            # A line may contain MULTIPLE short literals (e.g. the compass
            # abbreviations "N", "NE", "E", ... on one line) - finditer()
            # captures all of them, not just the first (see bug history
            # above for what goes wrong with a plain .match()).
            matches = list(STRING_LITERAL_RE.finditer(line))
            if not matches:
                raise ValueError(f"{path}: could not parse literal from line: {raw_line!r}")
            for m in matches:
                strings.append(unescape_c_string(m.group(1)))
    return strings


def count_enum_entries(i18n_h_path):
    """Independently counts StringId entries by parsing the enum itself,
    as a cross-check against the per-language extracted string counts -
    catches a parser bug in extract_strings() even if it affects EVERY
    language file identically (which a plain cross-language comparison
    would miss)."""
    in_enum = False
    count = 0
    with open(i18n_h_path, "r", encoding="utf-8") as f:
        for raw_line in f:
            line = raw_line.strip()
            if not in_enum:
                if line.startswith("enum class StringId"):
                    in_enum = True
                continue
            if line.startswith("};"):
                break
            if not line or line.startswith("//"):
                continue
            if line == "COUNT":
                continue  # sentinel, not a real entry
            count += 1
    return count


def main():
    i18n_h = os.path.join(SRC_DIR, "i18n.h")
    expected_count = count_enum_entries(i18n_h)
    print(f"StringId::COUNT (parsed from i18n.h): {expected_count}")

    counts = {}
    all_strings = {}
    for lang in LANGS:
        path = os.path.join(SRC_DIR, f"i18n_{lang}.h")
        strings = extract_strings(path)
        counts[lang] = len(strings)
        all_strings[lang] = strings
        print(f"{lang}: {len(strings)} strings")

    distinct_counts = set(counts.values())
    if len(distinct_counts) != 1:
        print("ERROR: language files have mismatched string counts:", counts)
        sys.exit(1)

    count = distinct_counts.pop()
    if count != expected_count:
        print(f"ERROR: extracted count {count} does not match StringId::COUNT "
              f"{expected_count} - a parsing bug or a genuine missing/extra "
              f"string is likely. Refusing to write possibly-misaligned "
              f"lang_*.bin files.")
        sys.exit(1)
    print(f"All 8 languages have {count} strings - matches StringId::COUNT.")

    os.makedirs(ASSETS_DIR, exist_ok=True)
    for idx, lang in enumerate(LANGS):
        strings = all_strings[lang]
        encoded = [s.encode("utf-8") for s in strings]
        for i, b in enumerate(encoded):
            if len(b) > 65535:
                raise ValueError(f"{lang} string {i} too long ({len(b)} bytes)")
        header = struct.pack("<4sBBHH", b"LNG2", 1, idx, CONTENT_VERSION, count)
        lengths = b"".join(struct.pack("<H", len(b)) for b in encoded)
        body = b"".join(encoded)
        out_path = os.path.join(ASSETS_DIR, f"lang_{lang}.bin")
        with open(out_path, "wb") as f:
            f.write(header)
            f.write(lengths)
            f.write(body)
        print(f"wrote {out_path}: {len(header) + len(lengths) + len(body)} bytes")


if __name__ == "__main__":
    main()
