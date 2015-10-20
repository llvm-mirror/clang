##===- tools/tooling-sample/Makefile -----------------------------*- Makefile -*-===##
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
##===----------------------------------------------------------------------===##
CLANG_LEVEL := ../..

TOOLNAME = tooling-sample 

# No plugins, optimize startup time.
TOOL_NO_EXPORTS := 1

# Don't install this.
NO_INSTALL = 1

include $(CLANG_LEVEL)/../../Makefile.config
LINK_COMPONENTS := $(TARGETS_TO_BUILD) asmparser bitreader support mc option
USEDLIBS = clangAST.a clangAnalysis.a clangBasic.a clangDriver.a clangEdit.a \
		   clangFrontend.a clangFrontendTool.a clangLex.a clangParse.a \
		   clangSema.a clangEdit.a clangASTMatchers.a clangRewrite.a \
		   clangRewriteFrontend.a clangStaticAnalyzerFrontend.a \
		   clangStaticAnalyzerCheckers.a clangStaticAnalyzerCore.a \
		   clangSerialization.a clangTooling.a

include $(CLANG_LEVEL)/Makefile

