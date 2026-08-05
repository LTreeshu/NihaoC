#!/usr/bin/env python3
"""NihaoC regression test runner.

Layout:
  tests/pos/<name>.nc         must compile and run
  tests/pos/<name>.expect     optional expected stdout (exact, line-based)
  tests/err/<name>.nc         must FAIL to compile
  tests/err/<name>.expect     optional error-message substring that must appear

Usage:
  python tests/run_tests.py [filter]
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NCC = os.path.join(ROOT, "build", "ncc.exe" if os.name == "nt" else "ncc")
POS = os.path.join(ROOT, "tests", "pos")
ERR = os.path.join(ROOT, "tests", "err")
TMP = os.path.join(ROOT, "build", "testrun")
os.makedirs(TMP, exist_ok=True)


def norm(lines):
    return [ln.rstrip("\r\n") for ln in lines]


def has_main(src):
    with open(src, encoding="utf-8") as f:
        return "func main" in f.read() or "main(" in f.read()


def run_pos(fname):
    stem = fname[:-3]
    src = os.path.join(POS, fname)
    exe = os.path.join(TMP, stem + (".exe" if os.name == "nt" else ""))
    expect_file = os.path.join(POS, stem + ".expect")

    # Library modules (no main): compile-only check, no link/run.
    if not has_main(src):
        r = subprocess.run([NCC, "build", "-c", src, "-o",
                            os.path.join(TMP, stem)], capture_output=True, text=True)
        if r.returncode != 0:
            return "FAIL", "compile error:\n" + r.stderr.strip() + r.stdout.strip()
        return "PASS", ""

    r = subprocess.run([NCC, "build", src, "-o", exe],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return "FAIL", "compile error:\n" + r.stderr.strip() + r.stdout.strip()

    if not os.path.exists(exe):
        return "FAIL", "no executable produced"

    out = subprocess.run([exe], capture_output=True, text=True)
    if out.returncode != 0:
        return "FAIL", "runtime exit %d, stderr=%s" % (out.returncode,
                                                       out.stderr.strip())

    if os.path.exists(expect_file):
        with open(expect_file, encoding="utf-8") as f:
            want = norm(f.read().splitlines())
        got = norm(out.stdout.splitlines())
        if want != got:
            return "FAIL", "output mismatch\n  want: %r\n  got:  %r" % (want, got)
    return "PASS", ""


def run_err(fname):
    stem = fname[:-3]
    src = os.path.join(ERR, fname)
    expect_file = os.path.join(ERR, stem + ".expect")

    r = subprocess.run([NCC, "build", src, "-o",
                        os.path.join(TMP, stem)], capture_output=True, text=True)
    if r.returncode == 0:
        return "FAIL", "expected compile error, but it compiled"

    if os.path.exists(expect_file):
        with open(expect_file, encoding="utf-8") as f:
            want = f.read().strip()
        combined = r.stdout + r.stderr
        if want not in combined:
            return "FAIL", "error message missing %r\n  got: %s" % (want, combined.strip())
    return "PASS", ""


def main():
    filt = sys.argv[1] if len(sys.argv) > 1 else ""
    passed = failed = 0
    failures = []

    pos_cases = sorted(f for f in os.listdir(POS) if f.endswith(".nc"))
    err_cases = sorted(f for f in os.listdir(ERR) if f.endswith(".nc"))

    for fname in pos_cases:
        if filt and filt not in fname:
            continue
        status, why = run_pos(fname)
        print("  [%s] pos/%s" % (status, fname))
        if status == "PASS":
            passed += 1
        else:
            failed += 1
            failures.append(("pos/" + fname, why))

    for fname in err_cases:
        if filt and filt not in fname:
            continue
        status, why = run_err(fname)
        print("  [%s] err/%s" % (status, fname))
        if status == "PASS":
            passed += 1
        else:
            failed += 1
            failures.append(("err/" + fname, why))

    print()
    print("== %d passed, %d failed ==" % (passed, failed))
    for name, why in failures:
        print("-- %s --" % name)
        print(why)
        print()
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
