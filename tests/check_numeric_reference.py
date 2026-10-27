"""Compare supported, defined numeric operations with a compiled C program."""

import argparse
import os
from pathlib import Path
import random
import subprocess
import tempfile


def main():
    root = Path(__file__).resolve().parent.parent
    default_newt = root / ("newt.exe" if os.name == "nt" else "newt")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--newt", type=Path, default=default_newt)
    parser.add_argument("--cc", default="gcc", help="C compiler executable")
    args = parser.parse_args()
    newt = args.newt.resolve()
    if not newt.is_file():
        parser.error("Build Newt before running the numeric reference check.")

    c_lines = ["#include <stdio.h>", "#include <math.h>", "#include <limits.h>",
               "static void emit(double n) {",
               "  if (isfinite(n) && n >= (double)LLONG_MIN && n < 9223372036854775808.0 && trunc(n) == n)",
               '    printf("%lld\\n", (long long)n);',
               '  else printf("%g\\n", n);', "}", "int main(void) {"]
    newt_lines = []
    count = 0

    def operation(type_name, left, operator, right):
        nonlocal count
        # Each result is stored in its declared type before being printed.
        names = (f"a{count}", f"b{count}", f"r{count}")
        a, b, result = names
        declarations = [f"{type_name} {a} = {left};", f"{type_name} {b} = {right};",
                        f"{type_name} {result} = {a} {operator} {b};"]
        c_lines.extend(declarations + [f"emit({result});"])
        newt_lines.extend(declarations + [f"print {result};"])
        count += 1

    for values in [(-2147483648, "/", 1), (2147483647, "-", 1),
                   (-5, "/", 2), (5, "/", -2), (-5, "/", -2)]:
        operation("int", *values)
    operation("float", 16777216, "+", 1)
    operation("double", 16777216, "+", 1)
    rng = random.Random(3001)
    for _ in range(40):
        # Bounds avoid C signed-overflow UB; denominators are never zero.
        a, b = rng.randint(-20000, 20000), rng.choice([-1, 1]) * rng.randint(1, 20000)
        operation("int", a, rng.choice(["+", "-", "*", "/"]), b)
    for type_name in ["float", "double"]:
        for _ in range(40):
            a = rng.randint(-20000, 20000) / 8
            b = rng.choice([-1, 1]) * rng.randint(1, 20000) / 16
            operation(type_name, repr(a), rng.choice(["+", "-", "*", "/"]), repr(b))
    c_lines.extend(["emit((int)-2.9);", "emit((float)16777217);", "return 0;", "}"])
    newt_lines.extend(["print int(-2.9);", "print float(16777217);"])
    count += 2

    with tempfile.TemporaryDirectory(prefix="newt-numeric-") as directory:
        directory = Path(directory)
        c_file, nt_file = directory / "reference.c", directory / "reference.nt"
        binary = directory / ("reference.exe" if os.name == "nt" else "reference")
        c_file.write_text("\n".join(c_lines) + "\n", encoding="utf-8")
        nt_file.write_text("\n".join(newt_lines) + "\n", encoding="utf-8")
        subprocess.run([args.cc, "-std=c11", "-Wall", "-Wextra", "-pedantic",
                        str(c_file), "-o", str(binary), "-lm"], check=True)
        expected = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
        actual = subprocess.run([str(newt), str(nt_file)], check=True, capture_output=True, text=True)
        if expected.stderr or actual.stderr:
            raise SystemExit(f"Unexpected diagnostic:\n{expected.stderr}{actual.stderr}")
        if expected.stdout != actual.stdout:
            want, got = expected.stdout.splitlines(), actual.stdout.splitlines()
            for index in range(max(len(want), len(got))):
                left = want[index] if index < len(want) else "<missing>"
                right = got[index] if index < len(got) else "<missing>"
                if left != right:
                    raise SystemExit(f"Numeric result {index + 1}: C={left}, Newt={right}")
    print(f"Matched {count} numeric results against C.")


if __name__ == "__main__":
    main()
