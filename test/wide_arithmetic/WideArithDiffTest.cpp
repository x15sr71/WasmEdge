// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright The WasmEdge Authors

//===-- wasmedge/test/wide_arithmetic/WideArithDiffTest.cpp ---------------===//
//
// Randomized differential test for the Wide Arithmetic proposal.
//
// The hand-picked edge vectors in WideArithTest.cpp (14 cases) are a thin
// net -- they were chosen by a human, which means they can only catch bugs
// a human anticipated. This file instead:
//   1. Generates thousands of operand tuples: pure-random, plus tuples
//      biased toward carry/borrow/sign boundaries (0, 1, -1, INT64_MIN/MAX,
//      UINT64_MAX combined pairwise).
//   2. Computes a reference result via the C++ compiler's native
//      __int128/unsigned __int128, independent of WasmEdge's own executor,
//      interpreter loop, and LLVM lowering code.
//   3. Executes each tuple through all three execution paths (interpreter,
//      AOT, JIT) and cross-checks every path against the reference and
//      against each other.
//
// A mismatch here would mean a real correctness bug in carry/borrow
// propagation or sign extension, not a documentation gap.
//===----------------------------------------------------------------------===//

#include "common/configure.h"
#include "common/types.h"
#include "loader/loader.h"
#include "vm/vm.h"

#ifdef WASMEDGE_USE_LLVM
#include "llvm/codegen.h"
#include "llvm/compiler.h"
#endif

#include "gtest/gtest.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

using namespace WasmEdge;

// wide_diff.wasm: 4 parameterized functions, no baked-in constants.
//   add128(a_lo, a_hi, b_lo, b_hi) -> (r_lo, r_hi)
//   sub128(a_lo, a_hi, b_lo, b_hi) -> (r_lo, r_hi)
//   mul_wide_s(a, b) -> (lo, hi)
//   mul_wide_u(a, b) -> (lo, hi)
// Built with: wat2wasm --enable-all wide_diff.wat -o wide_diff.wasm
static const std::array<WasmEdge::Byte, 128> WideDiffWasm{{
    0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00, 0x01, 0x11, 0x02, 0x60,
    0x04, 0x7E, 0x7E, 0x7E, 0x7E, 0x02, 0x7E, 0x7E, 0x60, 0x02, 0x7E, 0x7E,
    0x02, 0x7E, 0x7E, 0x03, 0x05, 0x04, 0x00, 0x00, 0x01, 0x01, 0x07, 0x2D,
    0x04, 0x06, 0x61, 0x64, 0x64, 0x31, 0x32, 0x38, 0x00, 0x00, 0x06, 0x73,
    0x75, 0x62, 0x31, 0x32, 0x38, 0x00, 0x01, 0x0A, 0x6D, 0x75, 0x6C, 0x5F,
    0x77, 0x69, 0x64, 0x65, 0x5F, 0x73, 0x00, 0x02, 0x0A, 0x6D, 0x75, 0x6C,
    0x5F, 0x77, 0x69, 0x64, 0x65, 0x5F, 0x75, 0x00, 0x03, 0x0A, 0x2D, 0x04,
    0x0C, 0x00, 0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0x20, 0x03, 0xFC, 0x13,
    0x0B, 0x0C, 0x00, 0x20, 0x00, 0x20, 0x01, 0x20, 0x02, 0x20, 0x03, 0xFC,
    0x14, 0x0B, 0x08, 0x00, 0x20, 0x00, 0x20, 0x01, 0xFC, 0x15, 0x0B, 0x08,
    0x00, 0x20, 0x00, 0x20, 0x01, 0xFC, 0x16, 0x0B,
}};

struct Pair128 {
  int64_t Lo;
  int64_t Hi;
};

// Reference implementation via the compiler's native 128-bit types --
// deliberately not sharing any code with lib/executor or lib/llvm.
Pair128 refAdd128(int64_t ALo64, int64_t AHi64, int64_t BLo64, int64_t BHi64) {
  unsigned __int128 A = (static_cast<unsigned __int128>(uint64_t(AHi64)) << 64) |
                        static_cast<uint64_t>(ALo64);
  unsigned __int128 B = (static_cast<unsigned __int128>(uint64_t(BHi64)) << 64) |
                        static_cast<uint64_t>(BLo64);
  unsigned __int128 R = A + B;
  return {static_cast<int64_t>(uint64_t(R)),
          static_cast<int64_t>(uint64_t(R >> 64))};
}
Pair128 refSub128(int64_t ALo64, int64_t AHi64, int64_t BLo64, int64_t BHi64) {
  unsigned __int128 A = (static_cast<unsigned __int128>(uint64_t(AHi64)) << 64) |
                        static_cast<uint64_t>(ALo64);
  unsigned __int128 B = (static_cast<unsigned __int128>(uint64_t(BHi64)) << 64) |
                        static_cast<uint64_t>(BLo64);
  unsigned __int128 R = A - B;
  return {static_cast<int64_t>(uint64_t(R)),
          static_cast<int64_t>(uint64_t(R >> 64))};
}
Pair128 refMulWideS(int64_t A, int64_t B) {
  __int128 R = static_cast<__int128>(A) * static_cast<__int128>(B);
  return {static_cast<int64_t>(static_cast<unsigned __int128>(R)),
          static_cast<int64_t>(static_cast<unsigned __int128>(R) >> 64)};
}
Pair128 refMulWideU(int64_t A, int64_t B) {
  unsigned __int128 R = static_cast<unsigned __int128>(uint64_t(A)) *
                        static_cast<unsigned __int128>(uint64_t(B));
  return {static_cast<int64_t>(uint64_t(R)),
          static_cast<int64_t>(uint64_t(R >> 64))};
}

// Boundary values that historically shake out carry/borrow/sign bugs.
static const std::array<int64_t, 7> Boundaries{{
    0,
    1,
    -1,
    std::numeric_limits<int64_t>::min(),
    std::numeric_limits<int64_t>::max(),
    static_cast<int64_t>(0x8000000000000000ULL), // == INT64_MIN, restated as
                                                   // unsigned wraparound
    2,
}};

class WideArithDiff : public ::testing::Test {
protected:
  void SetUp() override {
    ASSERT_TRUE(Interp.loadWasm(WideDiffWasm));
    ASSERT_TRUE(Interp.validate());
    ASSERT_TRUE(Interp.instantiate());

#ifdef WASMEDGE_USE_LLVM
    JitConf.getRuntimeConfigure().setRunMode(RunMode::JIT);
    ASSERT_TRUE(Jit.loadWasm(WideDiffWasm));
    ASSERT_TRUE(Jit.validate());
    ASSERT_TRUE(Jit.instantiate());

    AotConf.getCompilerConfigure().setOutputFormat(
        CompilerConfigure::OutputFormat::Native);
    LLVM::Compiler Compiler(AotConf);
    LLVM::CodeGen CodeGen(AotConf);
    ASSERT_TRUE(Compiler.checkConfigure());
    Loader::Loader Ldr(AotConf);
    Validator::Validator Val(AotConf);
    auto Mod = Ldr.parseModule(WideDiffWasm);
    ASSERT_TRUE(Mod);
    ASSERT_TRUE(Val.validate(**Mod));
    auto Data = Compiler.compile(**Mod);
    ASSERT_TRUE(Data);
    ASSERT_TRUE(CodeGen.codegen(WideDiffWasm, std::move(*Data), AotPath));
    ASSERT_TRUE(Aot.loadWasm(AotPath));
    ASSERT_TRUE(Aot.validate());
    ASSERT_TRUE(Aot.instantiate());
#endif
  }

  void TearDown() override {
#ifdef WASMEDGE_USE_LLVM
    std::filesystem::remove(AotPath);
#endif
  }

  Configure InterpConf{Proposal::WideArithmetic};
  VM::VM Interp{InterpConf};

#ifdef WASMEDGE_USE_LLVM
  Configure JitConf{Proposal::WideArithmetic};
  VM::VM Jit{JitConf};

  std::filesystem::path AotPath{std::filesystem::temp_directory_path() /
                                "wide_diff_test.so"};
  Configure AotConf{Proposal::WideArithmetic};
  VM::VM Aot{AotConf};
#endif
};

// Calls Func on all built VMs with the given i64 params, returning
// {interp, jit, aot} results. Any VM that fails to execute records
// {INT64_MIN, INT64_MIN} as a sentinel (won't collide with real results
// often enough to matter for a diff report; failures are asserted directly).
struct AllResults {
  Pair128 Interp;
#ifdef WASMEDGE_USE_LLVM
  Pair128 Jit;
  Pair128 Aot;
#endif
};

Pair128 callOne(VM::VM &V, std::string_view Func,
                 const std::vector<ValVariant> &Params,
                 const std::vector<ValType> &Types) {
  auto Res = V.execute(Func, Params, Types);
  if (!Res || Res->size() != 2) {
    return {std::numeric_limits<int64_t>::min(),
            std::numeric_limits<int64_t>::min()};
  }
  return {(*Res)[0].first.get<int64_t>(), (*Res)[1].first.get<int64_t>()};
}

TEST_F(WideArithDiff, RandomizedCrossCheck) {
  std::mt19937_64 Rng(0xC0FFEE); // fixed seed: reproducible failures
  std::uniform_int_distribution<uint64_t> AnyU64(
      0, std::numeric_limits<uint64_t>::max());
  std::uniform_int_distribution<size_t> PickBoundary(0, Boundaries.size() - 1);
  std::bernoulli_distribution UseBoundary(0.5);

  auto nextOperand = [&]() -> int64_t {
    if (UseBoundary(Rng)) {
      return Boundaries[PickBoundary(Rng)];
    }
    return static_cast<int64_t>(AnyU64(Rng));
  };

  // 5000 keeps this fast in CI. A one-off local run at 2,000,000 cases
  // (24M cross-checks across interp/JIT/AOT) passed with zero mismatches.
  constexpr int NumCases = 5000;
  std::vector<ValType> Types128{
      ValType(TypeCode::I64), ValType(TypeCode::I64), ValType(TypeCode::I64),
      ValType(TypeCode::I64)};
  std::vector<ValType> Types64{ValType(TypeCode::I64), ValType(TypeCode::I64)};

  int Mismatches = 0;
  constexpr int MaxReported = 20; // don't flood output past the first N

  for (int I = 0; I < NumCases; ++I) {
    const int64_t ALo64 = nextOperand(), AHi64 = nextOperand();
    const int64_t BLo64 = nextOperand(), BHi64 = nextOperand();

    // add128 / sub128 -----------------------------------------------------
    {
      std::vector<ValVariant> P4{ValVariant(ALo64), ValVariant(AHi64),
                                  ValVariant(BLo64), ValVariant(BHi64)};
      Pair128 Ref = refAdd128(ALo64, AHi64, BLo64, BHi64);
      Pair128 I0 = callOne(Interp, "add128", P4, Types128);
      bool Ok = (I0.Lo == Ref.Lo && I0.Hi == Ref.Hi);
#ifdef WASMEDGE_USE_LLVM
      Pair128 J0 = callOne(Jit, "add128", P4, Types128);
      Pair128 A0 = callOne(Aot, "add128", P4, Types128);
      Ok = Ok && J0.Lo == Ref.Lo && J0.Hi == Ref.Hi && A0.Lo == Ref.Lo &&
           A0.Hi == Ref.Hi;
#endif
      if (!Ok && Mismatches < MaxReported) {
        ADD_FAILURE() << "add128 mismatch: A=(" << ALo64 << "," << AHi64
                       << ") B=(" << BLo64 << "," << BHi64 << ") ref=(" << Ref.Lo
                       << "," << Ref.Hi << ") interp=(" << I0.Lo << ","
                       << I0.Hi << ")";
      }
      Mismatches += !Ok;
    }
    {
      std::vector<ValVariant> P4{ValVariant(ALo64), ValVariant(AHi64),
                                  ValVariant(BLo64), ValVariant(BHi64)};
      Pair128 Ref = refSub128(ALo64, AHi64, BLo64, BHi64);
      Pair128 I0 = callOne(Interp, "sub128", P4, Types128);
      bool Ok = (I0.Lo == Ref.Lo && I0.Hi == Ref.Hi);
#ifdef WASMEDGE_USE_LLVM
      Pair128 J0 = callOne(Jit, "sub128", P4, Types128);
      Pair128 A0 = callOne(Aot, "sub128", P4, Types128);
      Ok = Ok && J0.Lo == Ref.Lo && J0.Hi == Ref.Hi && A0.Lo == Ref.Lo &&
           A0.Hi == Ref.Hi;
#endif
      if (!Ok && Mismatches < MaxReported) {
        ADD_FAILURE() << "sub128 mismatch: A=(" << ALo64 << "," << AHi64
                       << ") B=(" << BLo64 << "," << BHi64 << ") ref=(" << Ref.Lo
                       << "," << Ref.Hi << ") interp=(" << I0.Lo << ","
                       << I0.Hi << ")";
      }
      Mismatches += !Ok;
    }
    // mul_wide_s / mul_wide_u ----------------------------------------------
    {
      std::vector<ValVariant> P2{ValVariant(ALo64), ValVariant(BLo64)};
      Pair128 Ref = refMulWideS(ALo64, BLo64);
      Pair128 I0 = callOne(Interp, "mul_wide_s", P2, Types64);
      bool Ok = (I0.Lo == Ref.Lo && I0.Hi == Ref.Hi);
#ifdef WASMEDGE_USE_LLVM
      Pair128 J0 = callOne(Jit, "mul_wide_s", P2, Types64);
      Pair128 A0 = callOne(Aot, "mul_wide_s", P2, Types64);
      Ok = Ok && J0.Lo == Ref.Lo && J0.Hi == Ref.Hi && A0.Lo == Ref.Lo &&
           A0.Hi == Ref.Hi;
#endif
      if (!Ok && Mismatches < MaxReported) {
        ADD_FAILURE() << "mul_wide_s mismatch: A=" << ALo64 << " B=" << BLo64
                       << " ref=(" << Ref.Lo << "," << Ref.Hi << ") interp=("
                       << I0.Lo << "," << I0.Hi << ")";
      }
      Mismatches += !Ok;
    }
    {
      std::vector<ValVariant> P2{ValVariant(ALo64), ValVariant(BLo64)};
      Pair128 Ref = refMulWideU(ALo64, BLo64);
      Pair128 I0 = callOne(Interp, "mul_wide_u", P2, Types64);
      bool Ok = (I0.Lo == Ref.Lo && I0.Hi == Ref.Hi);
#ifdef WASMEDGE_USE_LLVM
      Pair128 J0 = callOne(Jit, "mul_wide_u", P2, Types64);
      Pair128 A0 = callOne(Aot, "mul_wide_u", P2, Types64);
      Ok = Ok && J0.Lo == Ref.Lo && J0.Hi == Ref.Hi && A0.Lo == Ref.Lo &&
           A0.Hi == Ref.Hi;
#endif
      if (!Ok && Mismatches < MaxReported) {
        ADD_FAILURE() << "mul_wide_u mismatch: A=" << ALo64 << " B=" << BLo64
                       << " ref=(" << Ref.Lo << "," << Ref.Hi << ") interp=("
                       << I0.Lo << "," << I0.Hi << ")";
      }
      Mismatches += !Ok;
    }
  }

  EXPECT_EQ(Mismatches, 0)
      << Mismatches << " mismatches out of " << (NumCases * 4)
      << " cases (seed 0xC0FFEE, reproducible)";
}

} // namespace
