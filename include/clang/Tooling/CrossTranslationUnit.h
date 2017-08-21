//===--- CrossTranslationUnit.h - -------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the CrossTranslationUnit interface.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_TOOLING_CROSSTRANSLATIONUNIT_H
#define LLVM_CLANG_TOOLING_CROSSTRANSLATIONUNIT_H

#include "clang/Basic/LLVM.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"

namespace clang {
class CompilerInstance;
class ASTContext;
class ASTImporter;
class ASTUnit;
class DeclContext;
class FunctionDecl;
class NamedDecl;
class TranslationUnitDecl;

namespace tooling {

/// \brief This class can be used for tools that requires cross translation
///        unit capability.
///
/// This class can load function definitions from external AST files.
/// The loaded definition will be merged back to the original AST using the
/// AST Importer.
/// In order to use this class, an index file is required that describes
/// the locations of the AST files for each function definition.
///
/// Note that this class also implements caching.
class CrossTranslationUnit {
public:
  CrossTranslationUnit(CompilerInstance &CI);
  ~CrossTranslationUnit();

  /// \brief This function can load a function definition from an external AST
  ///        file and merge it into the original AST.
  ///
  /// This method should only be used on functions that have no definitions in
  /// the current translation unit. A function definition with the same
  /// declaration will be looked up in the index file which should be in the
  /// \p CrossTUDir directory, called \p IndexName. In case the declaration is
  /// found in the index the corresponding AST file will be loaded and the
  /// definition of the function will be merged into the original AST using
  /// the AST Importer. The declaration with the definition will be returned.
  ///
  /// Note that the AST files should also be in the \p CrossTUDir.
  const FunctionDecl *getCrossTUDefinition(const FunctionDecl *FD,
                                           StringRef CrossTUDir,
                                           StringRef IndexName,
                                           StringRef CompilationDatabase = "");

  std::string getLookupName(const NamedDecl *ND);
private:
  ASTImporter &getOrCreateASTImporter(ASTContext &From);
  const FunctionDecl *findFunctionInDeclContext(const DeclContext *DC,
                                                StringRef LookupFnName);

  llvm::StringMap<std::unique_ptr<clang::ASTUnit>> FileASTUnitMap;
  llvm::StringMap<clang::ASTUnit *> FunctionAstUnitMap;
  llvm::StringMap<std::string> FunctionFileMap;
  llvm::DenseMap<TranslationUnitDecl *, std::unique_ptr<ASTImporter>>
      ASTUnitImporterMap;
  CompilerInstance &CI;
  ASTContext &Context;
};

} // namespace tooling
} // namespace clang

#endif // LLVM_CLANG_TOOLING_CROSSTRANSLATIONUNIT_H
