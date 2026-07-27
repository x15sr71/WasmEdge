#!/bin/bash
# Run wide arithmetic interpreter tests
# Usage: ./run_tests.sh [path/to/wasmedge]
WASMEDGE=${1:-../../build/tools/wasmedge/wasmedge}
WASM=wide_arith.wasm
PASS=0; FAIL=0

check() {
  local name=$1 lo_exp=$2 hi_exp=$3
  local out
  out=$($WASMEDGE --reactor $WASM $name 2>/dev/null)
  local lo hi
  lo=$(echo "$out" | sed -n 1p)
  hi=$(echo "$out" | sed -n 2p)
  if [ "$lo" = "$lo_exp" ] && [ "$hi" = "$hi_exp" ]; then
    echo "PASS  $name -> lo=$lo hi=$hi"
    PASS=$((PASS+1))
  else
    echo "FAIL  $name -> got lo=$lo hi=$hi, expected lo=$lo_exp hi=$hi_exp"
    FAIL=$((FAIL+1))
  fi
}

check add128_zero        0   0
check add128_carry       0   1
check add128_wrap        0   0
check sub128_simple      7   0
check mul_wide_u_simple  15  0
check mul_wide_s_neg     1   0
check mul_wide_u_max     1   -2

echo ""
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
