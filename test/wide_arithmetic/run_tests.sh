#!/bin/bash
# Run wide arithmetic interpreter tests.
# Usage: ./run_tests.sh [path/to/wasmedge]
WASMEDGE=${1:-../../build/tools/wasmedge/wasmedge}
WASM=wide_arith.wasm
PASS=0; FAIL=0

check() {
  local name=$1 lo_exp=$2 hi_exp=$3 note=$4
  local out lo hi
  out=$($WASMEDGE --reactor $WASM $name 2>/dev/null)
  lo=$(echo "$out" | sed -n 1p)
  hi=$(echo "$out" | sed -n 2p)
  if [ "$lo" = "$lo_exp" ] && [ "$hi" = "$hi_exp" ]; then
    echo "PASS  $name -> result_low=$lo result_high=$hi${note:+  # $note}"
    PASS=$((PASS+1))
  else
    echo "FAIL  $name -> got result_low=$lo result_high=$hi, expected result_low=$lo_exp result_high=$hi_exp"
    FAIL=$((FAIL+1))
  fi
}

# i64.add128
check add128_zero        0   0
check add128_carry       0   1
check add128_wrap        0   0

# i64.sub128
check sub128_simple      7   0

# i64.mul_wide_u
check mul_wide_u_simple  15  0
# result_high = 0xFFFFFFFFFFFFFFFE; wasmedge prints signed i64 so it shows as -2
check mul_wide_u_max     1   -2  "result_high bit pattern = 0xFFFFFFFFFFFFFFFE"

# i64.mul_wide_s
check mul_wide_s_neg     1   0

echo ""
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
