#!/usr/bin/env python3
"""NihaoC regression test runner.

Layout:
  tests/pos/<name>.nc         must compile and run
  tests/pos/<name>.expect     optional expected stdout (exact, line-based)
  tests/err/<name>.nc         must FAIL to compile
  tests/err/<name>.expect     optional error-message substring that must appear

Usage:
  python tests/run_tests.py [filter] [--backend c|native|ir-c|ir-native] [--all]

Examples:
  python tests/run_tests.py                 # default backend: c
  python tests/run_tests.py hello           # filter by substring
  python tests/run_tests.py --backend native
  python tests/run_tests.py --backend ir-native
  python tests/run_tests.py --all           # full 4-backend matrix
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NCC = os.path.join(ROOT, "build", "ncc.exe" if os.name == "nt" else "ncc")
POS = os.path.join(ROOT, "tests", "pos")
ERR = os.path.join(ROOT, "tests", "err")
TMP = os.path.join(ROOT, "build", "testrun")
os.makedirs(TMP, exist_ok=True)

BACKENDS = ["c", "native", "ir-c", "ir-native"]

# IR 中间层（方案 B）目前只支持最小子集语法。
# pos 中的用例分两类：
#  - IR_SUBSET：IR 双后端（ir-c/ir-native）可编译运行的通用用例（全量后端也能跑）
#  - IR_ONLY ：IR 专属用例——子集语法（如无类型指针声明），全量 parser 无法编译
# 每扩展一个语法点，就把对应的回归用例加入对应集合。
IR_SUBSET = {"hello", "ir_demo"}
IR_ONLY = {"ir_ptr"}
IR_ONLY = {"ir_ptr"}
IR_ONLY = {"ir_ptr"}


def norm(lines):
    return [ln.rstrip("\r\n") for ln in lines]


def has_main(src):
    with open(src, encoding="utf-8") as f:
        return "func main" in f.read() or "main(" in f.read()


def backend_args(backend):
    return ["-backend=" + backend] if backend != "c" else []


def run_pos(fname, backend):
    stem = fname[:-3]
    src = os.path.join(POS, fname)
    exe = os.path.join(TMP, "%s_%s%s" % (stem, backend,
                                         ".exe" if os.name == "nt" else ""))
    expect_file = os.path.join(POS, stem + ".expect")
    extra = backend_args(backend)

    # Library modules (no main): compile-only check, no link/run.
    if not has_main(src):
        r = subprocess.run([NCC, "build", "-c", src, "-o",
                            os.path.join(TMP, stem)] + extra,
                           capture_output=True, text=True)
        if r.returncode != 0:
            return "FAIL", "compile error:\n" + r.stderr.strip() + r.stdout.strip()
        return "PASS", ""

    r = subprocess.run([NCC, "build", src, "-o", exe] + extra,
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


def run_backend(backend, filt):
    passed = failed = skipped = 0
    failures = []
    is_ir = backend in ("ir-c", "ir-native")

    pos_cases = sorted(f for f in os.listdir(POS) if f.endswith(".nc"))
    err_cases = sorted(f for f in os.listdir(ERR) if f.endswith(".nc"))

    for fname in pos_cases:
        if filt and filt not in fname:
            continue
        stem = fname[:-3]
        if is_ir:
            if stem not in IR_SUBSET and stem not in IR_ONLY:
                skipped += 1
                print("  [SKIP] pos/%s (IR 子集未覆盖)" % fname)
                continue
        else:
            if stem in IR_ONLY:
                skipped += 1
                print("  [SKIP] pos/%s (IR 专属用例，全量 parser 不支持子集语法)" % fname)
                continue
        status, why = run_pos(fname, backend)
        print("  [%s] pos/%s" % (status, fname))
        if status == "PASS":
            passed += 1
        else:
            failed += 1
            failures.append(("pos/" + fname, why))

    for fname in err_cases:
        if filt and filt not in fname:
            continue
        if is_ir:
            # IR 前端暂未实现 M2 静态检查，错误用例对其无意义
            skipped += 1
            print("  [SKIP] err/%s (IR 前端暂无静态检查)" % fname)
            continue
        status, why = run_err(fname)
        print("  [%s] err/%s" % (status, fname))
        if status == "PASS":
            passed += 1
        else:
            failed += 1
            failures.append(("err/" + fname, why))

    print()
    print("== [%s] %d passed, %d failed, %d skipped =="
          % (backend, passed, failed, skipped))
    for name, why in failures:
        print("-- %s --" % name)
        print(why)
        print()
    return 1 if failed else 0


def main():
    parser = argparse.ArgumentParser(
        description="NihaoC regression test runner (multi-backend)")
    parser.add_argument("filter", nargs="?", default="",
                        help="substring filter on test file names")
    parser.add_argument("--backend", default="c", choices=BACKENDS,
                        help="compiler backend to test (default: c)")
    parser.add_argument("--all", action="store_true",
                        help="run the full %d-backend matrix" % len(BACKENDS))
    args = parser.parse_args()

    backends = BACKENDS if args.all else [args.backend]
    rc = 0
    for be in backends:
        print(">>> backend: %s" % be)
        if run_backend(be, args.filter) != 0:
            rc = 1
        print()
    return rc


if __name__ == "__main__":
    sys.exit(main())
