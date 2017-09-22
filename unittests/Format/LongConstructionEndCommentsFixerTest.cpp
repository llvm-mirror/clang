//===- LongConstructionEndCommentsFixerTest.cpp - Formatting unit tests ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/Format/Format.h"

#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "llvm/Support/Debug.h"
#include "gtest/gtest.h"

#define DEBUG_TYPE "long-construction-end-comments-fixer-test"

namespace clang {
namespace format {
namespace {

class LongConstructionEndCommentsFixerTest : public ::testing::Test {
protected:
  std::string
  fixOtherEndComments(llvm::StringRef Code,
                          const std::vector<tooling::Range> &Ranges,
                          const FormatStyle &Style = getLLVMStyle()) {
    DEBUG(llvm::errs() << "---\n");
    DEBUG(llvm::errs() << Code << "\n\n");
    tooling::Replacements Replaces =
        clang::format::fixOtherEndComments(Style, Code, Ranges, "<stdin>");
    auto Result = applyAllReplacements(Code, Replaces);
    EXPECT_TRUE(static_cast<bool>(Result));
    DEBUG(llvm::errs() << "\n" << *Result << "\n\n");
    return *Result;
  }

  std::string
  fixOtherEndComments(llvm::StringRef Code,
                          const FormatStyle &Style = getLLVMStyle()) {
    return fixOtherEndComments(
        Code,
        /*Ranges=*/{1, tooling::Range(0, Code.size())}, Style);
  }
};

TEST_F(LongConstructionEndCommentsFixerTest, AddsEndComment) {
	FormatStyle Style = getLLVMStyle();


	EXPECT_EQ(	"for (;;) {\n"
				"  int i;\n"
				"  int j;\n"
				"}",
				fixOtherEndComments("for (;;) {\n"
									"  int i;\n"
									"  int j;\n"
									"}", Style));

  Style.LongConstructionEndCommentsLenLimit = 1;

  	EXPECT_EQ(	"if (FileNames.size() != 1 && (Offsets.empty()) && (Offsets.empty()) &&\n"
				"  (Offsets.empty())) {\n"
				"  errs() << \"error: -offset, -length and -lines can only be used for \"\n"
				" \"single file.\n\";\n"
				"  return 1; \n"
				"} // if (FileNames.size() != 1 && (Offsets.empty()) && (Offsets.empty()) &&",
				fixOtherEndComments("if (FileNames.size() != 1 && (Offsets.empty()) && (Offsets.empty()) &&\n"
									"  (Offsets.empty())) {\n"
									"  errs() << \"error: -offset, -length and -lines can only be used for \"\n"
									" \"single file.\n\";\n"
									"  return 1; \n"
									"}", Style));

  // for 
  EXPECT_EQ("for (;;) {\n"
            "  int i;\n"
            "  int j;\n"
            "} // for (;;)",
            fixOtherEndComments("for (;;) {\n"
								"  int i;\n"
								"  int j;\n"
								"}", Style));

  // if 
    EXPECT_EQ("if (a) {\n"
            "  int i;\n"
            "  int j;\n"
            "} // if (a)",
            fixOtherEndComments("if (a) {\n"
								"  int i;\n"
								"  int j;\n"
								"}", Style));

	// if-else
	EXPECT_EQ("if (a) {\n"
		"  int i;\n"
		"  int j;\n"
		"} else if (a) {\n"
		"  int i;\n"
		"  int j;\n"
		"} else {\n"
		"  int i;\n"
		"  int j;\n"
		"}",
		fixOtherEndComments("if (a) {\n"
							"  int i;\n"
							"  int j;\n"
							"} else if (a) {\n"
							"  int i;\n"
							"  int j;\n"
							"} else {\n"
							"  int i;\n"
							"  int j;\n"
							"}", Style));

	// while 
    EXPECT_EQ("while (a) {\n"
            "  int i;\n"
            "  int j;\n"
            "} // while (a)",
            fixOtherEndComments("while (a) {\n"
								"  int i;\n"
								"  int j;\n"
								"}", Style));


	// switch 
    EXPECT_EQ(	"switch (x) {\n"
				"case 1: {\n"
				"  f();\n"
				"  f();\n"
				"  break;\n"
				"} // case 1:\n"
				"case 2: {\n"
				"  f();\n"
				"  f();\n"
				"  break;\n"
				"} // case 2:\n"
				"case 3:\n"
				"f();\n"
				"f();\n"
				"break;\n"
				"} // switch (x)",
            fixOtherEndComments("switch (x) {\n"
								"case 1: {\n"
								"  f();\n"
								"  f();\n"
								"  break;\n"
								"}\n"
								"case 2: {\n"
								"  f();\n"
								"  f();\n"
								"  break;\n"
								"}\n"
								"case 3:\n"
								"f();\n"
								"f();\n"
								"break;\n"
								"}", Style));

  // lenght
  Style.LongConstructionEndCommentsLenLimit = 5;
  // short and long
  EXPECT_EQ("if (a) {\n"
            "  int i;\n"
            "  int ii;\n"
            "}\n"
			"if (a) {\n"
			"  int i;\n"
			"  int ii;\n"
			"  int iii;\n"
			"  int iiii;\n"
			"  int iiiii;\n"
			"  int iiiiii;\n"
			"} // if (a)",
            fixOtherEndComments("if (a) {\n"
								"  int i;\n"
								"  int ii;\n"
								"}\n"
								"if (a) {\n"
								"  int i;\n"
								"  int ii;\n"
								"  int iii;\n"
								"  int iiii;\n"
								"  int iiiii;\n"
								"  int iiiiii;\n"
								"}", Style));

  Style.LongConstructionEndCommentsLenLimit = 1;

  // nested
  EXPECT_EQ(  "for (;;) {\n"
			  "  if (a) {\n"
			  "    int i;\n"
			  "    int j;\n"
			  "  } // if (a)\n"
			  "} // for (;;)",
			  fixOtherEndComments("for (;;) {\n"
								  "  if (a) {\n"
								  "    int i;\n"
								  "    int j;\n"
								  "  }\n"
								  "}", Style));
  // in function
  EXPECT_EQ(	"void foo () {"
				"  for (;;) {\n"
				"    int i;\n"
				"    int j;\n"
				"  } // for (;;)\n"
				"}",
				fixOtherEndComments("void foo () {"
									"  for (;;) {\n"
									"    int i;\n"
									"    int j;\n"
									"  }\n"
									"}", Style));

  // replace test
  EXPECT_EQ(  "for (;;) {\n"
			  "  int i;\n"
			  "  int j;\n"
			  "} // for (;;)",
			  fixOtherEndComments("for (;;) {\n"
								  "  int i;\n"
								  "  int j;\n"
								  "}//some comment", Style));
  // two lines statement
  EXPECT_EQ(  "if (aaaaa\n"
			  " && bbbbb) {\n"
			  "  int i;\n"
			  "  int j;\n"
			  "} // if (aaaaa && bbbbb)",
	  fixOtherEndComments("if (aaaaa\n"
						  " && bbbbb) {\n"
						  "  int i;\n"
						  "  int j;\n"
						  "}", Style));

  // sequence
  EXPECT_EQ("if (aaaaa\n"
			  " && bbbbb) {\n"
			  "  int i;\n"
			  "  int j;\n"
			  "} // if (aaaaa && bbbbb)\n"
			  "if (aaaaa\n"
			  " && bbbbb) {\n"
			  "  int i;\n"
			  "  int j;\n"
			  "} // if (aaaaa && bbbbb)",
			  fixOtherEndComments("if (aaaaa\n"
									" && bbbbb) {\n"
									"  int i;\n"
									"  int j;\n"
									"}\n"
									"if (aaaaa\n"
									" && bbbbb) {\n"
									"  int i;\n"
									"  int j;\n"
									"}", Style));

EXPECT_EQ(	"if (a) {\n"
            "  int i;\n"
            "  int ii;\n"
            "} // if (a)\n"
            "// comment about b\n"
            "int b;",
            fixOtherEndComments("if (a) {\n"
								"  int i;\n"
								"  int ii;\n"
								"}\n"
								"// comment about b\n"
								"int b;", Style));

  // nested
  EXPECT_EQ(  "if (a) {\n"
			  "  if (b) {\n"
			  "    if (c) {\n"
			  "      if (d) {\n"
			  "        for (int i=0;i<5;i++) {\n"
	          "          int i; \n"
	          "          int j;\n"
			  "        } // for (int i = 0; i < 5; i++)\n"
			  "      } // if (d)\n"
			  "    } // if (c)\n"
	          "  } // if (b)\n"
			  "} // if (a)",
			  fixOtherEndComments(	"if (a) {\n"
									"  if (b) {\n"
									"    if (c) {\n"
									"      if (d) {\n"
									"        for (int i=0;i<5;i++) {\n"
									"          int i; \n"
									"          int j;\n"
									"        }\n"
									"      }\n"
									"    }\n"
									"  }\n"
									"}", Style));
}

TEST_F(LongConstructionEndCommentsFixerTest, AddsNewlineIfNeeded) {
  FormatStyle Style = getLLVMStyle();
  Style.LongConstructionEndCommentsLenLimit = 1;

  EXPECT_EQ("for (;;) {\n"
			"  int i;\n"
			"  int j;\n"
			"} // for (;;)\n"
			"; int k;",
            fixOtherEndComments("for (;;) {\n"
								"  int i;\n"
								"  int j;\n"
								"}; int k;", Style));

  EXPECT_EQ("for (;;) {\n"
			"  int i;\n"
			"  int j;\n"
			"} // for (;;)\n"
			" int k;",
            fixOtherEndComments("for (;;) {\n"
								"  int i;\n"
								"  int j;\n"
								"} int k;", Style));
}
//
//TEST_F(LongConstructionEndCommentsFixerTest, DoesNotAddCommentAfterRBraceInPPDirective) {
//  FormatStyle Style = getLLVMStyle();
//  Style.LongConstructionEndCommentsLenLimit = 1;
//
//  EXPECT_EQ("#define SAD \\\n"
//            "namespace A { \\\n"
//            "  int i; \\\n"
//            "}",
//            fixOtherEndComments("#define SAD \\\n"
//                                    "namespace A { \\\n"
//                                    "  int i; \\\n"
//                                    "}", Style));
//}

//TEST_F(LongConstructionEndCommentsFixerTest, KeepsValidEndComment) {
//  FormatStyle Style = getLLVMStyle();
//
//  EXPECT_EQ("namespace {\n"
//            "  int i;\n"
//            "} // end anonymous namespace",
//            fixOtherEndComments("namespace {\n"
//                                    "  int i;\n"
//                                    "} // end anonymous namespace", Style));
//  EXPECT_EQ("namespace A {\n"
//            "  int i;\n"
//            "} /* end of namespace A */",
//            fixOtherEndComments("namespace A {\n"
//                                    "  int i;\n"
//                                    "} /* end of namespace A */", Style));
//  EXPECT_EQ("namespace A {\n"
//            "  int i;\n"
//            "}   //   namespace A",
//            fixOtherEndComments("namespace A {\n"
//                                    "  int i;\n"
//                                    "}   //   namespace A", Style));
//  EXPECT_EQ("namespace A::B {\n"
//            "  int i;\n"
//            "} // end namespace A::B",
//            fixOtherEndComments("namespace A::B {\n"
//                                    "  int i;\n"
//                                    "} // end namespace A::B", Style));
//  EXPECT_EQ("namespace A {\n"
//            "  int i;\n"
//            "}; // end namespace A",
//            fixOtherEndComments("namespace A {\n"
//                                    "  int i;\n"
//                                    "}; // end namespace A", Style));
//  EXPECT_EQ("namespace {\n"
//            "  int i;\n"
//            "}; /* unnamed namespace */",
//            fixOtherEndComments("namespace {\n"
//                                    "  int i;\n"
//                                    "}; /* unnamed namespace */", Style));
//}

TEST_F(LongConstructionEndCommentsFixerTest, UpdatesInvalidEndLineComment) {
  FormatStyle Style = getLLVMStyle();
  Style.LongConstructionEndCommentsLenLimit = 1;

  // replace test
  EXPECT_EQ(  "for (;;) {\n"
			  "  int i;\n"
			  "  int j;\n"
			  "} // for (;;)",
			  fixOtherEndComments("for (;;) {\n"
								  "  int i;\n"
								  "  int j;\n"
								  "}//some comment", Style));

}

//TEST_F(LongConstructionEndCommentsFixerTest,
//       DoesNotAddEndCommentForNamespacesControlledByMacros) {
//  FormatStyle Style = getLLVMStyle();
//  EXPECT_EQ("#ifdef 1\n"
//            "namespace A {\n"
//            "#elseif\n"
//            "namespace B {\n"
//            "#endif\n"
//            "  int i;\n"
//            "}\n"
//            "}\n",
//            fixOtherEndComments("#ifdef 1\n"
//                                    "namespace A {\n"
//                                    "#elseif\n"
//                                    "namespace B {\n"
//                                    "#endif\n"
//                                    "  int i;\n"
//                                    "}\n"
//                                    "}\n", Style));
//}
//
//TEST_F(LongConstructionEndCommentsFixerTest, AddEndCommentForNamespacesAroundMacros) {
//  // Conditional blocks around are fine
//  FormatStyle Style = getLLVMStyle();
//
//  EXPECT_EQ("namespace A {\n"
//            "#if 1\n"
//            "int i;\n"
//            "#endif\n"
//            "}// namespace A",
//            fixOtherEndComments("namespace A {\n"
//                                    "#if 1\n"
//                                    "int i;\n"
//                                    "#endif\n"
//                                    "}", Style));
//  EXPECT_EQ("#if 1\n"
//            "#endif\n"
//            "namespace A {\n"
//            "int i;\n"
//            "int j;\n"
//            "}// namespace A",
//            fixOtherEndComments("#if 1\n"
//                                    "#endif\n"
//                                    "namespace A {\n"
//                                    "int i;\n"
//                                    "int j;\n"
//                                    "}", Style));
//  EXPECT_EQ("namespace A {\n"
//            "int i;\n"
//            "int j;\n"
//            "}// namespace A\n"
//            "#if 1\n"
//            "#endif",
//            fixOtherEndComments("namespace A {\n"
//                                    "int i;\n"
//                                    "int j;\n"
//                                    "}\n"
//                                    "#if 1\n"
//                                    "#endif", Style));
//  EXPECT_EQ("#if 1\n"
//            "namespace A {\n"
//            "int i;\n"
//            "int j;\n"
//            "}// namespace A\n"
//            "#endif",
//            fixOtherEndComments("#if 1\n"
//                                    "namespace A {\n"
//                                    "int i;\n"
//                                    "int j;\n"
//                                    "}\n"
//                                    "#endif", Style));
//
//  // Macro definition has no impact
//  EXPECT_EQ("namespace A {\n"
//            "#define FOO\n"
//            "int i;\n"
//            "}// namespace A",
//            fixOtherEndComments("namespace A {\n"
//                                    "#define FOO\n"
//                                    "int i;\n"
//                                    "}", Style));
//  EXPECT_EQ("#define FOO\n"
//            "namespace A {\n"
//            "int i;\n"
//            "int j;\n"
//            "}// namespace A",
//            fixOtherEndComments("#define FOO\n"
//                                    "namespace A {\n"
//                                    "int i;\n"
//                                    "int j;\n"
//                                    "}", Style));
//  EXPECT_EQ("namespace A {\n"
//            "int i;\n"
//            "int j;\n"
//            "}// namespace A\n"
//            "#define FOO\n",
//            fixOtherEndComments("namespace A {\n"
//                                    "int i;\n"
//                                    "int j;\n"
//                                    "}\n"
//                                    "#define FOO\n", Style));
//
//  // No replacement if open & close in different conditional blocks
//  EXPECT_EQ("#if 1\n"
//            "namespace A {\n"
//            "#endif\n"
//            "int i;\n"
//            "int j;\n"
//            "#if 1\n"
//            "}\n"
//            "#endif",
//            fixOtherEndComments("#if 1\n"
//                                    "namespace A {\n"
//                                    "#endif\n"
//                                    "int i;\n"
//                                    "int j;\n"
//                                    "#if 1\n"
//                                    "}\n"
//                                    "#endif", Style));
//  EXPECT_EQ("#ifdef A\n"
//            "namespace A {\n"
//            "#endif\n"
//            "int i;\n"
//            "int j;\n"
//            "#ifdef B\n"
//            "}\n"
//            "#endif",
//            fixOtherEndComments("#ifdef A\n"
//                                    "namespace A {\n"
//                                    "#endif\n"
//                                    "int i;\n"
//                                    "int j;\n"
//                                    "#ifdef B\n"
//                                    "}\n"
//                                    "#endif", Style));
//
//  // No replacement inside unreachable conditional block
//  EXPECT_EQ("#if 0\n"
//            "namespace A {\n"
//            "int i;\n"
//            "int j;\n"
//            "}\n"
//            "#endif",
//            fixOtherEndComments("#if 0\n"
//                                    "namespace A {\n"
//                                    "int i;\n"
//                                    "int j;\n"
//                                    "}\n"
//                                    "#endif", Style));
//}
//
//TEST_F(LongConstructionEndCommentsFixerTest,
//       DoesNotAddEndCommentForNamespacesInMacroDeclarations) {
//  FormatStyle Style = getLLVMStyle();
//
//  EXPECT_EQ("#ifdef 1\n"
//            "namespace A {\n"
//            "#elseif\n"
//            "namespace B {\n"
//            "#endif\n"
//            "  int i;\n"
//            "}\n"
//            "}\n",
//            fixOtherEndComments("#ifdef 1\n"
//                                    "namespace A {\n"
//                                    "#elseif\n"
//                                    "namespace B {\n"
//                                    "#endif\n"
//                                    "  int i;\n"
//                                    "}\n"
//                                    "}\n", Style));
//  EXPECT_EQ("namespace {\n"
//            "  int i;\n"
//            "  int j;\n"
//            "}// namespace\n"
//            "#if A\n"
//            "  int i;\n"
//            "#else\n"
//            "  int j;\n"
//            "#endif",
//            fixOtherEndComments("namespace {\n"
//                                    "  int i;\n"
//                                    "  int j;\n"
//                                    "}\n"
//                                    "#if A\n"
//                                    "  int i;\n"
//                                    "#else\n"
//                                    "  int j;\n"
//                                    "#endif", Style));
//  EXPECT_EQ("#if A\n"
//            "namespace A {\n"
//            "#else\n"
//            "namespace B {\n"
//            "#endif\n"
//            "int i;\n"
//            "int j;\n"
//            "}",
//            fixOtherEndComments("#if A\n"
//                                    "namespace A {\n"
//                                    "#else\n"
//                                    "namespace B {\n"
//                                    "#endif\n"
//                                    "int i;\n"
//                                    "int j;\n"
//                                    "}", Style));
//  EXPECT_EQ("#if A\n"
//            "namespace A {\n"
//            "#else\n"
//            "namespace B {\n"
//            "#endif\n"
//            "int i;\n"
//            "int j;\n"
//            "} // namespace A",
//            fixOtherEndComments("#if A\n"
//                                    "namespace A {\n"
//                                    "#else\n"
//                                    "namespace B {\n"
//                                    "#endif\n"
//                                    "int i;\n"
//                                    "int j;\n"
//                                    "} // namespace A", Style));
//  EXPECT_EQ("#if A\n"
//            "namespace A {\n"
//            "#else\n"
//            "namespace B {\n"
//            "#endif\n"
//            "int i;\n"
//            "int j;\n"
//            "} // namespace B",
//            fixOtherEndComments("#if A\n"
//                                    "namespace A {\n"
//                                    "#else\n"
//                                    "namespace B {\n"
//                                    "#endif\n"
//                                    "int i;\n"
//                                    "int j;\n"
//                                    "} // namespace B", Style));
//  EXPECT_EQ("namespace A\n"
//            "int i;\n"
//            "int j;\n"
//            "#if A\n"
//            "}\n"
//            "#else\n"
//            "}\n"
//            "#endif",
//            fixOtherEndComments("namespace A\n"
//                                    "int i;\n"
//                                    "int j;\n"
//                                    "#if A\n"
//                                    "}\n"
//                                    "#else\n"
//                                    "}\n"
//                                    "#endif", Style));
//  EXPECT_EQ("namespace A\n"
//            "int i;\n"
//            "int j;\n"
//            "#if A\n"
//            "} // namespace A\n"
//            "#else\n"
//            "} // namespace A\n"
//            "#endif",
//            fixOtherEndComments("namespace A\n"
//                                    "int i;\n"
//                                    "int j;\n"
//                                    "#if A\n"
//                                    "} // namespace A\n"
//                                    "#else\n"
//                                    "} // namespace A\n"
//                                    "#endif", Style));
//}
} // end namespace
} // end namespace format
} // end namespace clang
