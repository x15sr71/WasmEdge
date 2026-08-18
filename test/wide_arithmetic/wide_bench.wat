(module
  ;; ---- native opcodes ----
  (func (export "add128") (param $a_lo i64) (param $a_hi i64) (param $b_lo i64) (param $b_hi i64) (result i64 i64)
    local.get $a_lo
    local.get $a_hi
    local.get $b_lo
    local.get $b_hi
    i64.add128)
  (func (export "mul_wide_u") (param $a i64) (param $b i64) (result i64 i64)
    local.get $a
    local.get $b
    i64.mul_wide_u)

  ;; ---- hand-emulated equivalents, using only pre-proposal i64 ops ----
  ;; This is what a user had to write before these opcodes existed.
  (func $add128e (export "add128_emulated")
        (param $a_lo i64) (param $a_hi i64) (param $b_lo i64) (param $b_hi i64)
        (result i64 i64)
    (local $r_lo i64) (local $carry i64)
    local.get $a_lo
    local.get $b_lo
    i64.add
    local.tee $r_lo
    local.get $a_lo
    i64.lt_u
    i64.extend_i32_u
    local.set $carry
    local.get $r_lo
    local.get $a_hi
    local.get $b_hi
    i64.add
    local.get $carry
    i64.add)

  (func (export "mul_wide_u_emulated") (param $a i64) (param $b i64) (result i64 i64)
    (local $a_lo32 i64) (local $a_hi32 i64) (local $b_lo32 i64) (local $b_hi32 i64)
    (local $t0 i64) (local $t1 i64) (local $t2 i64) (local $t3 i64)
    (local $r_lo i64) (local $r_hi i64)
    local.get $a
    i64.const 0xFFFFFFFF
    i64.and
    local.set $a_lo32
    local.get $a
    i64.const 32
    i64.shr_u
    local.set $a_hi32
    local.get $b
    i64.const 0xFFFFFFFF
    i64.and
    local.set $b_lo32
    local.get $b
    i64.const 32
    i64.shr_u
    local.set $b_hi32

    local.get $a_lo32
    local.get $b_lo32
    i64.mul
    local.set $t0
    local.get $a_lo32
    local.get $b_hi32
    i64.mul
    local.set $t1
    local.get $a_hi32
    local.get $b_lo32
    i64.mul
    local.set $t2
    local.get $a_hi32
    local.get $b_hi32
    i64.mul
    local.set $t3

    ;; R = (t0, 0)
    local.get $t0
    local.set $r_lo
    i64.const 0
    local.set $r_hi

    ;; R += (t1 << 32, t1 >>u 32)
    local.get $r_lo
    local.get $r_hi
    local.get $t1
    i64.const 32
    i64.shl
    local.get $t1
    i64.const 32
    i64.shr_u
    call $add128e
    local.set $r_hi
    local.set $r_lo

    ;; R += (t2 << 32, t2 >>u 32)
    local.get $r_lo
    local.get $r_hi
    local.get $t2
    i64.const 32
    i64.shl
    local.get $t2
    i64.const 32
    i64.shr_u
    call $add128e
    local.set $r_hi
    local.set $r_lo

    ;; R += (0, t3)
    local.get $r_lo
    local.get $r_hi
    i64.const 0
    local.get $t3
    call $add128e
    local.set $r_hi
    local.set $r_lo

    local.get $r_lo
    local.get $r_hi))
