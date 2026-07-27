(module
  ;; Stack: a_lo a_hi b_lo b_hi  ->  res_lo res_hi

  ;; 0 + 0 = 0
  (func (export "add128_zero") (result i64 i64)
    i64.const 0  i64.const 0  i64.const 0  i64.const 0
    i64.add128
  )

  ;; 1 + 1 = 2, no carry
  (func (export "add128_simple") (result i64 i64)
    i64.const 1  i64.const 0  i64.const 1  i64.const 0
    i64.add128
  )

  ;; 0xFFFFFFFFFFFFFFFF + 1 -> carry: lo=0, hi=1
  (func (export "add128_carry") (result i64 i64)
    i64.const -1  i64.const 0  i64.const 1  i64.const 0
    i64.add128
  )

  ;; max128 + 1 -> wraps to 0
  (func (export "add128_wrap") (result i64 i64)
    i64.const -1  i64.const -1  i64.const 1  i64.const 0
    i64.add128
  )

  ;; 10 - 3 = 7
  (func (export "sub128_simple") (result i64 i64)
    i64.const 10  i64.const 0  i64.const 3  i64.const 0
    i64.sub128
  )

  ;; borrow: 0x100000000 - 1 = 0xFFFFFFFF across 64-bit boundary
  (func (export "sub128_borrow") (result i64 i64)
    i64.const 0  i64.const 1  i64.const 1  i64.const 0
    i64.sub128
  )

  ;; mul_wide_u: 3 * 5 = 15
  (func (export "mul_wide_u_simple") (result i64 i64)
    i64.const 3  i64.const 5
    i64.mul_wide_u
  )

  ;; mul_wide_u: 0xFFFFFFFFFFFFFFFF * 0xFFFFFFFFFFFFFFFF
  ;; = 0xFFFFFFFFFFFFFFFE_0000000000000001
  (func (export "mul_wide_u_max") (result i64 i64)
    i64.const -1  i64.const -1
    i64.mul_wide_u
  )

  ;; mul_wide_s: -1 * -1 = 1
  (func (export "mul_wide_s_neg") (result i64 i64)
    i64.const -1  i64.const -1
    i64.mul_wide_s
  )

  ;; mul_wide_s: INT64_MIN * -1 = 2^63 (lo=0x8000000000000000, hi=0)
  (func (export "mul_wide_s_minval") (result i64 i64)
    i64.const -9223372036854775808  i64.const -1
    i64.mul_wide_s
  )
)
