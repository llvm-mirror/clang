set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  asmparser
  support
  mc
  )

add_clang_executable(clang-func-mapping
  ClangFnMapGen.cpp
  )

target_link_libraries(clang-func-mapping
  clangTooling
  clangAST
  clangBasic
  clangFrontend
  clangRewriteFrontend
  clangIndex
  )

install(TARGETS clang-func-mapping
  RUNTIME DESTINATION bin)
