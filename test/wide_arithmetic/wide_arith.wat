(module
  ;; Wide Arithmetic proposal (Phase 3) — manual test module
  ;;
  ;; Proposal stack convention (per spec):
  ;;   i64.add128 / i64.sub128:
  ;;     inputs  (bottom→top): lhs_low lhs_high rhs_low rhs_high
  ;;     outputs (bottom→top): result_low result_high
  ;;   i64.mul_wide_s / i64.mul_wide_u:
  ;;     inputs  (bottom→top): lhs rhs
  ;;     outputs (bottom→top): result_low result_high
  ;;
  ;; A 128-bit value V is represented as the pair (low, high) where
  ;;   V = high * 2^64 + low   (unsigned interpretation)

  ;; --- i64.add128 ---

  ;; 0 + 0 = 0
  (func (export "add128_zero") (result i64 i64)
    i64.const 0  ;; lhs_low
    i64.const 0  ;; lhs_high
    i64.const 0  ;; rhs_low
    i64.const 0  ;; rhs_high
    i64.add128
    ;; result_low=0  result_high=0
  )

  ;; 1 + 1 = 2, no carry out of low word
  (func (export "add128_simple") (result i64 i64)
    i64.const 1  ;; lhs_low
    i64.const 0  ;; lhs_high
    i64.const 1  ;; rhs_low
    i64.const 0  ;; rhs_high
    i64.add128
    ;; result_low=2  result_high=0
  )

  ;; Carry: lhs=0xFFFFFFFFFFFFFFFF + rhs=1 -> carry propagates into high word
  ;; lhs=(low=0xFFFFFFFFFFFFFFFF, high=0)  rhs=(low=1, high=0)
  ;; result=(low=0, high=1)
  (func (export "add128_carry") (result i64 i64)
    i64.const -1  ;; lhs_low  = 0xFFFFFFFFFFFFFFFF
    i64.const 0   ;; lhs_high = 0
    i64.const 1   ;; rhs_low  = 1
    i64.const 0   ;; rhs_high = 0
    i64.add128
    ;; result_low=0  result_high=1
  )

  ;; Wraparound: max128 + 1 = 0 (mod 2^128)
  ;; lhs=(low=0xFFFF…FFFF, high=0xFFFF…FFFF)  rhs=(low=1, high=0)
  ;; result=(low=0, high=0)
  (func (export "add128_wrap") (result i64 i64)
    i64.const -1  ;; lhs_low  = 0xFFFFFFFFFFFFFFFF
    i64.const -1  ;; lhs_high = 0xFFFFFFFFFFFFFFFF
    i64.const 1   ;; rhs_low  = 1
    i64.const 0   ;; rhs_high = 0
    i64.add128
    ;; result_low=0  result_high=0
  )

  ;; --- i64.sub128 ---

  ;; Simple: 10 - 3 = 7, no borrow
  (func (export "sub128_simple") (result i64 i64)
    i64.const 10  ;; lhs_low
    i64.const 0   ;; lhs_high
    i64.const 3   ;; rhs_low
    i64.const 0   ;; rhs_high
    i64.sub128
    ;; result_low=7  result_high=0
  )

  ;; Borrow: lhs=(low=0, high=1) - rhs=(low=1, high=0)
  ;; = 2^64 - 1 = (low=0xFFFFFFFFFFFFFFFF, high=0)
  (func (export "sub128_borrow") (result i64 i64)
    i64.const 0   ;; lhs_low  = 0
    i64.const 1   ;; lhs_high = 1  -> lhs = 2^64
    i64.const 1   ;; rhs_low  = 1
    i64.const 0   ;; rhs_high = 0
    i64.sub128
    ;; result_low=0xFFFFFFFFFFFFFFFF (-1 as i64)  result_high=0
  )

  ;; --- i64.mul_wide_u ---

  ;; Simple: 3 * 5 = 15
  (func (export "mul_wide_u_simple") (result i64 i64)
    i64.const 3   ;; lhs (zero-extended to 128 bits)
    i64.const 5   ;; rhs (zero-extended to 128 bits)
    i64.mul_wide_u
    ;; result_low=15  result_high=0
  )

  ;; Max: 0xFFFFFFFFFFFFFFFF * 0xFFFFFFFFFFFFFFFF
  ;; = 0xFFFFFFFFFFFFFFFE_0000000000000001
  ;; result_low  = 0x0000000000000001 (= 1)
  ;; result_high = 0xFFFFFFFFFFFFFFFE (= -2 as signed i64, same bit pattern)
  (func (export "mul_wide_u_max") (result i64 i64)
    i64.const -1  ;; lhs = 0xFFFFFFFFFFFFFFFF
    i64.const -1  ;; rhs = 0xFFFFFFFFFFFFFFFF
    i64.mul_wide_u
    ;; result_low=1  result_high=0xFFFFFFFFFFFFFFFE
  )

  ;; --- i64.mul_wide_s ---

  ;; (-1) * (-1) = 1
  (func (export "mul_wide_s_neg") (result i64 i64)
    i64.const -1  ;; lhs = -1 (sign-extended to 128 bits = 0xFFFF…FFFF)
    i64.const -1  ;; rhs = -1
    i64.mul_wide_s
    ;; result_low=1  result_high=0
  )

  ;; INT64_MIN * (-1) = 2^63
  ;; result_low=0x8000000000000000  result_high=0
  (func (export "mul_wide_s_minval") (result i64 i64)
    i64.const -9223372036854775808  ;; lhs = INT64_MIN
    i64.const -1                    ;; rhs = -1
    i64.mul_wide_s
    ;; result_low=0x8000000000000000 (-9223372036854775808 as i64)  result_high=0
  )
)
