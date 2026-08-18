# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: Copyright The WasmEdge Authors

wasmedge_add_executable(wasmedgeWideArithTests
  WideArithTest.cpp
  WideArithDiffTest.cpp
)

add_test(wasmedgeWideArithTests wasmedgeWideArithTests)

target_link_libraries(wasmedgeWideArithTests
  PRIVATE
  std::filesystem
  ${GTEST_BOTH_LIBRARIES}
  wasmedgeVM
  wasmedgeLoader
)

if(WASMEDGE_USE_LLVM)
  target_compile_definitions(wasmedgeWideArithTests
    PRIVATE
    -DWASMEDGE_USE_LLVM
  )
  target_link_libraries(wasmedgeWideArithTests
    PRIVATE
    wasmedgeLLVM
  )
endif()
