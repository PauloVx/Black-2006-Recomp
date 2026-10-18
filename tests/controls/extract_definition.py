"""Writes the built-in native menu definition (black_menu_definition in src/black_native_menu.inc) to a file,
exactly as the game writes local\\menus.ini, and prints its FNV-1a 64 (the replaced-defaults fingerprint)."""
import argparse
import ast
import re

parser = argparse.ArgumentParser()
parser.add_argument("--source", required=True)
parser.add_argument("--output", required=True)
args = parser.parse_args()

src = open(args.source, encoding="utf-8").read()
start = src.index("static const char black_menu_definition[] =")
body = src[src.index("=", start) + 1:src.index(";\n", start)]
parts = re.findall(r'"((?:[^"\\]|\\.)*)"', body)
text = "".join(ast.literal_eval('"' + p + '"') for p in parts).encode("utf-8")
h = 0xCBF29CE484222325
for b in text:
    h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
with open(args.output, "wb") as f:
    f.write(text)
print("0x%016X" % h)
