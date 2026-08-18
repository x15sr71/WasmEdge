// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright The WasmEdge Authors

//===-- wasmedge/test/wide_arithmetic/WideArithBenchTest.cpp --------------===//
//
// Compares the native i64.add128 / i64.mul_wide_u opcodes against hand
// emulations built only from pre-proposal i64 ops -- i.e. what a user had
// to write before this proposal existed. Two things happen here, in order:
//
//   1. Correctness: the emulated functions are cross-checked against the
//      native opcodes (and, transitively, against WideArithDiffTest's
//      __int128 reference) across random inputs. A benchmark comparing
//      against a baseline nobody verified is not evidence of anything.
//   2. Timing: once the emulation is proven correct, time both paths in
//      the interpreter. Reported, not asserted -- wall-clock thresholds in
//      CI are a flaky-test generator, not a correctness signal.
//===----------------------------------------------------------------------===//

#include "common/configure.h"
#include "common/types.h"
#include "vm/vm.h"

#include "gtest/gtest.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

namespace {

using namespace WasmEdge;

// wide_bench.wasm: add128/mul_wide_u (native) plus add128_emulated/
// mul_wide_u_emulated (built only from i64.add/sub/mul/shift/compare).
// Built with: wat2wasm --enable-all wide_bench.wat -o wide_bench.wasm
static const std::array<WasmEdge::Byte, 289> WideBenchWasm{{
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x11, 0x02, 0x60,
    0x04, 0x7E, 0x7E, 0x7E, 0x7E, 0x02, 0x7E, 0x7E, 0x60, 0x02, 0x7E, 0x7E,
    0x02, 0x7E, 0x7E, 0x03, 0x05, 0x04, 0x00, 0x01, 0x00, 0x01, 0x07, 0x3F,
    0x04, 0x06, 0x61, 0x64, 0x64, 0x31, 0x32, 0x38, 0x00, 0x00, 0x0A, 0x6D,
    0x75, 0x6C, 0x5F, 0x77, 0x69, 0x64, 0x65, 0x5F, 0x75, 0x00, 0x01, 0x0F,
    0x61, 0x64, 0x64, 0x31, 0x32, 0x38, 0x5F, 0x65, 0x6D, 0x75, 0x6C, 0x61,
    0x74, 0x65, 0x64, 0x00, 0x02, 0x13, 0x6D, 0x75, 0x6C, 0x5F, 0x77, 0x69,
    0x64, 0x65, 0x5F, 0x75, 0x5F, 0x65, 0x6D, 0x75, 0x6C, 0x61, 0x74, 0x65,
    0x64, 0x00, 0x03, 0x0A, 0xBB, 0x01, 0x04, 0x0C, 0x00, 0x20, 0x00, 0x20,
    0x01, 0x20, 0x02, 0x20, 0x03, 0xFC, 0x13, 0x0B, 0x08, 0x00, 0x20, 0x00,
    0x20, 0x01, 0xFC, 0x16, 0x0B, 0x1B, 0x01, 0x02, 0x7E, 0x20, 0x00, 0x20,
    0x02, 0x7C, 0x22, 0x04, 0x20, 0x00, 0x54, 0xAD, 0x21, 0x05, 0x20, 0x04,
    0x20, 0x01, 0x20, 0x03, 0x7C, 0x20, 0x05, 0x7C, 0x0B, 0x86, 0x01, 0x01,
    0x0A, 0x7E, 0x20, 0x00, 0x42, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x83, 0x21,
    0x02, 0x20, 0x00, 0x42, 0x20, 0x88, 0x21, 0x03, 0x20, 0x01, 0x42, 0xFF,
    0xFF, 0xFF, 0xFF, 0x0F, 0x83, 0x21, 0x04, 0x20, 0x01, 0x42, 0x20, 0x88,
    0x21, 0x05, 0x20, 0x02, 0x20, 0x04, 0x7E, 0x21, 0x06, 0x20, 0x02, 0x20,
    0x05, 0x7E, 0x21, 0x07, 0x20, 0x03, 0x20, 0x04, 0x7E, 0x21, 0x08, 0x20,
    0x03, 0x20, 0x05, 0x7E, 0x21, 0x09, 0x20, 0x06, 0x21, 0x0A, 0x42, 0x00,
    0x21, 0x0B, 0x20, 0x0A, 0x20, 0x0B, 0x20, 0x07, 0x42, 0x20, 0x86, 0x20,
    0x07, 0x42, 0x20, 0x88, 0x10, 0x02, 0x21, 0x0B, 0x21, 0x0A, 0x20, 0x0A,
    0x20, 0x0B, 0x20, 0x08, 0x42, 0x20, 0x86, 0x20, 0x08, 0x42, 0x20, 0x88,
    0x10, 0x02, 0x21, 0x0B, 0x21, 0x0A, 0x20, 0x0A, 0x20, 0x0B, 0x42, 0x00,
    0x20, 0x09, 0x10, 0x02, 0x21, 0x0B, 0x21, 0x0A, 0x20, 0x0A, 0x20, 0x0B,
    0x0B,
}};

struct Pair128 {
  int64_t Lo;
  int64_t Hi;
};

Pair128 call2(VM::VM &V, std::string_view Func, int64_t A, int64_t B) {
  std::vector<ValVariant> P{ValVariant(A), ValVariant(B)};
  std::vector<ValType> T{ValType(TypeCode::I64), ValType(TypeCode::I64)};
  auto Res = V.execute(Func, P, T);
  return {(*Res)[0].first.get<int64_t>(), (*Res)[1].first.get<int64_t>()};
}
Pair128 call4(VM::VM &V, std::string_view Func, int64_t ALo64, int64_t AHi64,
              int64_t BLo64, int64_t BHi64) {
  std::vector<ValVariant> P{ValVariant(ALo64), ValVariant(AHi64), ValVariant(BLo64),
                             ValVariant(BHi64)};
  std::vector<ValType> T{ValType(TypeCode::I64), ValType(TypeCode::I64),
                          ValType(TypeCode::I64), ValType(TypeCode::I64)};
  auto Res = V.execute(Func, P, T);
  return {(*Res)[0].first.get<int64_t>(), (*Res)[1].first.get<int64_t>()};
}

class WideArithBench : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(VM.loadWasm(WideBenchWasm));
    ASSERT_TRUE(VM.validate());
    ASSERT_TRUE(VM.instantiate());
  }
  Configure Conf{Proposal::WideArithmetic};
  VM::VM VM{Conf};
};

// Step 1: prove the emulated baseline matches the native opcode before
// trusting any timing comparison against it.
TEST_F(WideArithBench, EmulatedMatchesNative) {
  std::mt19937_64 Rng(0xBEEF);
  std::uniform_int_distribution<uint64_t> AnyU64(
      0, std::numeric_limits<uint64_t>::max());
  int Mismatches = 0;
  for (int I = 0; I < 20000; ++I) {
    int64_t ALo64 = int64_t(AnyU64(Rng)), AHi64 = int64_t(AnyU64(Rng));
    int64_t BLo64 = int64_t(AnyU64(Rng)), BHi64 = int64_t(AnyU64(Rng));
    Pair128 Native = call4(VM, "add128", ALo64, AHi64, BLo64, BHi64);
    Pair128 Emulated = call4(VM, "add128_emulated", ALo64, AHi64, BLo64, BHi64);
    if (Native.Lo != Emulated.Lo || Native.Hi != Emulated.Hi) {
      ADD_FAILURE() << "add128_emulated diverged at case " << I;
      ++Mismatches;
    }
    int64_t A = int64_t(AnyU64(Rng)), B = int64_t(AnyU64(Rng));
    Pair128 NativeM = call2(VM, "mul_wide_u", A, B);
    Pair128 EmulatedM = call2(VM, "mul_wide_u_emulated", A, B);
    if (NativeM.Lo != EmulatedM.Lo || NativeM.Hi != EmulatedM.Hi) {
      ADD_FAILURE() << "mul_wide_u_emulated diverged at case " << I;
      ++Mismatches;
    }
  }
  EXPECT_EQ(Mismatches, 0);
}

// Step 2: only runs the timing comparison; no CI assertion on wall time.
TEST_F(WideArithBench, InterpreterTiming) {
  using Clock = std::chrono::steady_clock;
  constexpr int Iters = 2000000;
  std::mt19937_64 Rng(0x5EED);
  std::uniform_int_distribution<uint64_t> AnyU64(
      0, std::numeric_limits<uint64_t>::max());

  auto timeCalls = [&](std::string_view Func, int NumArgs) {
    auto Start = Clock::now();
    for (int I = 0; I < Iters; ++I) {
      if (NumArgs == 4) {
        call4(VM, Func, int64_t(AnyU64(Rng)), int64_t(AnyU64(Rng)),
              int64_t(AnyU64(Rng)), int64_t(AnyU64(Rng)));
      } else {
        call2(VM, Func, int64_t(AnyU64(Rng)), int64_t(AnyU64(Rng)));
      }
    }
    return std::chrono::duration<double>(Clock::now() - Start).count();
  };

  double AddNative = timeCalls("add128", 4);
  double AddEmulated = timeCalls("add128_emulated", 4);
  double MulNative = timeCalls("mul_wide_u", 2);
  double MulEmulated = timeCalls("mul_wide_u_emulated", 2);

  std::printf("\n[wide-arithmetic interpreter benchmark, %d iterations each]\n",
              Iters);
  std::printf("  add128:     native=%.3fs emulated=%.3fs  (%.2fx)\n",
              AddNative, AddEmulated, AddEmulated / AddNative);
  std::printf("  mul_wide_u: native=%.3fs emulated=%.3fs  (%.2fx)\n",
              MulNative, MulEmulated, MulEmulated / MulNative);
}

} // namespace
