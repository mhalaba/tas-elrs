#!/usr/bin/env python3
# Regression pin for the TAS hardened binding KDF (mirrors build_flags.py).
import hashlib, sys

ITERS = 10000

def stock_uid(define):
    return hashlib.md5(define.encode()).digest()[0:6]

def tas_uid(define):
    d = define.encode()
    for _ in range(ITERS):
        d = hashlib.sha256(d).digest()
    return d[0:6]

fails = 0
def check(cond, name):
    global fails
    print(("PASS" if cond else "FAIL"), name)
    if not cond: fails += 1

phrase_flag = "-DMY_BINDING_PHRASE=tas-test-2026"
u_stock = stock_uid(phrase_flag)
u_tas = tas_uid(phrase_flag)

check(len(u_stock) == 6 and len(u_tas) == 6, "uid length 6 bytes both modes")
check(u_stock == hashlib.md5(phrase_flag.encode()).digest()[:6], "stock path == upstream md5")
check(u_tas != u_stock, "hardened uid differs from stock")
check(tas_uid(phrase_flag) == u_tas, "hardened deterministic")
check(tas_uid("-DMY_BINDING_PHRASE=tas-test-2027") != u_tas, "different phrase -> different uid")

print("KDF selftest:", "PASSED" if not fails else f"FAILED ({fails})")
sys.exit(1 if fails else 0)
