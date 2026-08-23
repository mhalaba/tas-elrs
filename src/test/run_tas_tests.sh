#!/usr/bin/env bash
# TAS-ELRS full native test suite (Krok 6).
set -u
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"   # ExpressLRS/
OUT=${TMPDIR:-/tmp}/tas-elrs-tests
mkdir -p "$OUT"
FAIL=0

TAS="$ROOT/src/lib/TAS"
INC="-I$ROOT/src/include -I$ROOT/src/lib/TAS -I$ROOT/src/lib/OTA -I$ROOT/src/lib/CRC -I$ROOT/src/lib/FIFO -I$ROOT/src/lib/logging -I$ROOT/src/lib/FHSS"
BASE="-DPROGMEM= -DUNIT_TEST=1 -DTARGET_NATIVE"

step() { # step <name> <cmd...>
    local name="$1"; shift
    echo "== $name"
    if ! "$@"; then echo "!! BUILD/RUN FAILED: $name"; FAIL=1; fi
}

# 1) Crypto audit (RFC vectors)
step "crypto audit" c++ -std=c++17 -Wall -Wextra -O2 -I"$TAS" \
    "$ROOT/src/test/test_tas_crypto/test_main.cpp" "$TAS/TasCrypto.cpp" "$TAS/TasSession.cpp" \
    -o "$OUT/crypto"
[ "$FAIL" = 0 ] && "$OUT/crypto" || true

# 2) AFH / jam classification / FHSS prune simulation
if [ "$FAIL" = 0 ]; then
step "afh sim" c++ -std=c++17 -Wall -Wextra -O2 -I"$TAS" \
    "$ROOT/src/test/test_tas_afh/test_main.cpp" "$TAS/TasAfh.cpp" "$TAS/TasCrypto.cpp" \
    -o "$OUT/afh"
"$OUT/afh" || FAIL=1
fi

# 3) Wake token + sleep budget + radio watchdog
if [ "$FAIL" = 0 ]; then
step "wake+watchdog" c++ -std=c++17 -Wall -Wextra -O2 -I"$TAS" \
    "$ROOT/src/test/test_tas_wake/test_main.cpp" "$TAS/TasWake.cpp" "$TAS/TasFailsafe.cpp" \
    "$TAS/TasCrypto.cpp" "$TAS/TasSession.cpp" \
    -o "$OUT/wake"
"$OUT/wake" || FAIL=1
fi

# Stream stub for OTA tests
STUB="$OUT/stream_stub.cpp"
cat > "$STUB" << 'EOF'
#include "native.h"
#include "logging.h"
class FakeStream : public Stream {
public:
    size_t write(uint8_t) override { return 1; }
    size_t write(const uint8_t *, size_t n) override { return n; }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};
static FakeStream fakeStream;
Stream *BackpackOrLogStrm = &fakeStream;
EOF

# 4) OTA integration, hardened mode (real OTA.cpp)
if [ "$FAIL" = 0 ]; then
step "ota roundtrip (TAS)" c++ -std=gnu++11 -w $BASE -DTAS_HARDENING $INC \
    "$ROOT/src/test/test_tas_ota/test_main.cpp" "$ROOT/src/lib/OTA/OTA.cpp" "$ROOT/src/lib/CRC/crc.cpp" "$STUB" \
    "$TAS/TasOta.cpp" "$TAS/TasCrypto.cpp" "$TAS/TasSession.cpp" \
    -o "$OUT/ota_mil"
"$OUT/ota_mil" || FAIL=1
fi

# 5) OTA stock regression (no TAS flag)
if [ "$FAIL" = 0 ]; then
step "ota stock regression" c++ -std=gnu++11 -w $BASE $INC \
    "$ROOT/src/test/test_tas_ota/test_main.cpp" "$ROOT/src/lib/OTA/OTA.cpp" "$ROOT/src/lib/CRC/crc.cpp" "$STUB" \
    "$TAS/TasCrypto.cpp" "$TAS/TasSession.cpp" \
    -o "$OUT/ota_stock"
"$OUT/ota_stock" || FAIL=1
fi

echo
# 6) binding KDF selftest (python3 optional)
if command -v python3 >/dev/null 2>&1; then
step "binding KDF selftest" python3 "$ROOT/src/python/tas_kdf_selftest.py"
fi

if [ "$FAIL" = "0" ]; then echo "=== KROK 6: WSZYSTKIE TESTY PASSED ==="; else echo "=== KROK 6: FAILURES ==="; exit 1; fi
