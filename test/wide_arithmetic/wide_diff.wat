(module
  (func (export "add128") (param $a_lo i64) (param $a_hi i64) (param $b_lo i64) (param $b_hi i64) (result i64 i64)
    local.get $a_lo
    local.get $a_hi
    local.get $b_lo
    local.get $b_hi
    i64.add128)
  (func (export "sub128") (param $a_lo i64) (param $a_hi i64) (param $b_lo i64) (param $b_hi i64) (result i64 i64)
    local.get $a_lo
    local.get $a_hi
    local.get $b_lo
    local.get $b_hi
    i64.sub128)
  (func (export "mul_wide_s") (param $a i64) (param $b i64) (result i64 i64)
    local.get $a
    local.get $b
    i64.mul_wide_s)
  (func (export "mul_wide_u") (param $a i64) (param $b i64) (result i64 i64)
    local.get $a
    local.get $b
    i64.mul_wide_u))
