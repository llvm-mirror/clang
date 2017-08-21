//===- unittest/Tooling/CrossTranslationUnitTest.cpp - Tooling unit tests -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/AST/ASTConsumer.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CrossTranslationUnit.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/Support/Path.h"
#include "gtest/gtest.h"
#include <cassert>

namespace clang {
namespace tooling {

namespace {
StringRef IndexFileName = "index.txt";
StringRef ASTFileName = "f.ast";
StringRef DefinitionFileName = "input.cc";

class CTUASTConsumer : public clang::ASTConsumer {
public:
  explicit CTUASTConsumer(clang::CompilerInstance &CI, bool *Success)
      : CTU(CI), Success(Success) {}

  void HandleTranslationUnit(ASTContext &Ctx) {
    const TranslationUnitDecl *TU = Ctx.getTranslationUnitDecl();
    const FunctionDecl *FD = nullptr;
    for (const Decl *D : TU->decls()) {
      FD = dyn_cast<FunctionDecl>(D);
      if (FD && FD->getName() == "f")
        break;
    }
    assert(FD);
    bool OrigFDHasBody = FD->hasBody();

    // Prepare the index file and the AST file.
    std::error_code EC;
    llvm::raw_fd_ostream OS(IndexFileName, EC, llvm::sys::fs::F_Text);
    OS << "c:@F@f#I# " << ASTFileName << "\n";
    OS.flush();
    StringRef SourceText = "int f(int) { return 0; }\n";
    // This file must exist since the saved ASTFile will reference it.
    llvm::raw_fd_ostream OS2(DefinitionFileName, EC, llvm::sys::fs::F_Text);
    OS2 << SourceText;
    OS2.flush();
    std::unique_ptr<ASTUnit> ASTWithDefinition =
        buildASTFromCode(SourceText);
    ASTWithDefinition->Save(ASTFileName);

    // Load the definition from the AST file.
    const FunctionDecl *NewFD =
        CTU.getCrossTUDefinition(FD, ".", IndexFileName);

    *Success = NewFD && NewFD->hasBody() && !OrigFDHasBody;
  }

private:
  CrossTranslationUnit CTU;
  bool *Success;
};

class CTUAction : public clang::ASTFrontendAction {
public:
  CTUAction(bool *Success) : Success(Success) {}

protected:
  std::unique_ptr<clang::ASTConsumer>
  CreateASTConsumer(clang::CompilerInstance &CI, StringRef) override {
    return llvm::make_unique<CTUASTConsumer>(CI, Success);
  }

private:
  bool *Success;
};

} // end namespace

TEST(CrossTranslationUnit, CanLoadFunctionDefinition) {
  bool Success = false;
  EXPECT_TRUE(runToolOnCode(new CTUAction(&Success), "int f(int);"));
  EXPECT_TRUE(Success);
  EXPECT_TRUE(llvm::sys::fs::exists(IndexFileName));
  EXPECT_FALSE((bool)llvm::sys::fs::remove(IndexFileName));
  EXPECT_TRUE(llvm::sys::fs::exists(ASTFileName));
  EXPECT_FALSE((bool)llvm::sys::fs::remove(ASTFileName));
  EXPECT_TRUE(llvm::sys::fs::exists(DefinitionFileName));
  EXPECT_FALSE((bool)llvm::sys::fs::remove(DefinitionFileName));
}

} // end namespace tooling
} // end namespace clang
