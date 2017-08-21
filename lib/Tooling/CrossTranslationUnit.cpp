//===--- CrossTranslationUnit.cpp - -----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file provides an interface to load binary AST dumps on demand. This
//  feature can be utilized for tools that require cross translation unit
//  support.
//
//===----------------------------------------------------------------------===//
#include "clang/Tooling/CrossTranslationUnit.h"
#include "clang/AST/ASTImporter.h"
#include "clang/AST/Decl.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/Frontend/ASTUnit.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendDiagnostic.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Index/USRGeneration.h"
#include "clang/Tooling/JSONCompilationDatabase.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>

namespace {
#define DEBUG_TYPE "CrossTranslationUnit"
STATISTIC(NumGetCTUCalled, "The # of getCTUDefinition function called");
STATISTIC(NumNoUnit, "The # of getCTUDefinition NoUnit");
STATISTIC(
    NumNotInOtherTU,
    "The # of getCTUDefinition called but the function is not in other TU");
STATISTIC(NumIterateNotFound, "The # of iteration not found");
STATISTIC(NumGetCTUSuccess, "The # of getCTUDefinition successfully return the "
                            "requested function's body");
}

namespace clang {
namespace tooling {

CrossTranslationUnit::CrossTranslationUnit(CompilerInstance &CI)
    : CI(CI), Context(CI.getASTContext()) {}

CrossTranslationUnit::~CrossTranslationUnit() {}

std::string CrossTranslationUnit::getLookupName(const NamedDecl *ND) {
  SmallString<128> DeclUSR;
  bool Ret = index::generateUSRForDecl(ND, DeclUSR);
  assert(!Ret);
  (void)Ret;
  return DeclUSR.str();
}

/// Recursively visit the funtion decls of a DeclContext, and looks up a
/// function based on mangled name.
const FunctionDecl *
CrossTranslationUnit::findFunctionInDeclContext(const DeclContext *DC,
                                                StringRef LookupFnName) {
  if (!DC)
    return nullptr;
  for (const Decl *D : DC->decls()) {
    const auto *SubDC = dyn_cast<DeclContext>(D);
    if (const auto *FD = findFunctionInDeclContext(SubDC, LookupFnName))
      return FD;

    const auto *ND = dyn_cast<FunctionDecl>(D);
    const FunctionDecl *ResultDecl;
    if (!ND || !ND->hasBody(ResultDecl))
      continue;
    if (getLookupName(ResultDecl) != LookupFnName)
      continue;
    return ResultDecl;
  }
  return nullptr;
}

const FunctionDecl *CrossTranslationUnit::getCrossTUDefinition(
    const FunctionDecl *FD, StringRef CrossTUDir, StringRef IndexName,
    StringRef CompilationDatabase) {
  assert(!FD->hasBody() && "FD has a definition in current translation unit!");
  ++NumGetCTUCalled;

  std::string LookupFnName = getLookupName(FD);
  if (LookupFnName.empty())
    return nullptr;
  ASTUnit *Unit = nullptr;
  auto FnUnitCacheEntry = FunctionAstUnitMap.find(LookupFnName);
  if (FnUnitCacheEntry == FunctionAstUnitMap.end()) {
    if (FunctionFileMap.empty()) {
      SmallString<256> ExternalFunctionMap = CrossTUDir;
      llvm::sys::path::append(ExternalFunctionMap, IndexName);
      std::ifstream ExternalFnMapFile(ExternalFunctionMap.c_str());
      if (!ExternalFnMapFile) {
        Context.getDiagnostics().Report(diag::err_fe_error_opening)
            << ExternalFunctionMap << "required by the CrossTU functionality";
        return nullptr;
      }

      StringRef FunctionName, FileName;
      std::string Line;
      unsigned LineNo = 0;
      while (std::getline(ExternalFnMapFile, Line)) {
        size_t Pos = Line.find(" ");
        StringRef LineRef{Line};
        if (Pos > 0 && Pos != std::string::npos) {
          FunctionName = LineRef.substr(0, Pos);
          FileName = LineRef.substr(Pos + 1);
          SmallString<256> FilePath;
          if (llvm::sys::path::is_absolute(FileName)) {
            FilePath = FileName;
          } else {
            FilePath = CrossTUDir;
            llvm::sys::path::append(FilePath, FileName);
            if (!CompilationDatabase.empty()) {
              Context.getDiagnostics().Report(diag::err_fnmap_absolute)
                  << ExternalFunctionMap << (LineNo + 1);
            }
          }
          FunctionFileMap[FunctionName] = FilePath.str().str();
        } else {
          Context.getDiagnostics().Report(diag::err_fnmap_parsing)
              << ExternalFunctionMap << (LineNo + 1);
        }
        LineNo++;
      }
    }

    StringRef ASTFileName;
    auto It = FunctionFileMap.find(LookupFnName);
    if (It == FunctionFileMap.end()) {
      ++NumNotInOtherTU;
      return nullptr; // No definition found even in some other build unit.
    }
    ASTFileName = It->second;
    auto ASTCacheEntry = FileASTUnitMap.find(ASTFileName);
    if (ASTCacheEntry == FileASTUnitMap.end()) {
      IntrusiveRefCntPtr<DiagnosticOptions> DiagOpts = new DiagnosticOptions();
      TextDiagnosticPrinter *DiagClient =
          new TextDiagnosticPrinter(llvm::errs(), &*DiagOpts);
      IntrusiveRefCntPtr<DiagnosticIDs> DiagID(new DiagnosticIDs());
      IntrusiveRefCntPtr<DiagnosticsEngine> Diags(
          new DiagnosticsEngine(DiagID, &*DiagOpts, DiagClient));

      std::unique_ptr<ASTUnit> LoadedUnit;
      if (CompilationDatabase.empty()) {
        LoadedUnit = ASTUnit::LoadFromASTFile(
            ASTFileName, CI.getPCHContainerOperations()->getRawReader(),
            ASTUnit::LoadEverything, Diags, CI.getFileSystemOpts());
      } else {
        std::string Error;
        std::unique_ptr<JSONCompilationDatabase> CompDb =
            JSONCompilationDatabase::loadFromFile(
                CompilationDatabase, Error, JSONCommandLineSyntax::AutoDetect);
        if (!CompDb) {
          llvm::errs() << Error << "\n";
          return nullptr;
        }
        SmallVector<std::string, 1> Files;
        Files.push_back(ASTFileName);
        ClangTool Tool(*CompDb, Files, CI.getPCHContainerOperations());
        std::vector<std::unique_ptr<ASTUnit>> ASTs;
        Tool.buildASTs(ASTs);
        assert(ASTs.size() == 1);
        if (ASTs.empty())
          return nullptr;
        LoadedUnit = std::move(ASTs[0]);
      }
      Unit = LoadedUnit.get();
      FileASTUnitMap[ASTFileName] = std::move(LoadedUnit);
    } else {
      Unit = ASTCacheEntry->second.get();
    }
    FunctionAstUnitMap[LookupFnName] = Unit;
  } else {
    Unit = FnUnitCacheEntry->second;
  }

  if (!Unit) {
    ++NumNoUnit;
    return nullptr;
  }
  assert(&Unit->getFileManager() ==
         &Unit->getASTContext().getSourceManager().getFileManager());
  ASTImporter &Importer = getOrCreateASTImporter(Unit->getASTContext());
  TranslationUnitDecl *TU = Unit->getASTContext().getTranslationUnitDecl();
  if (const FunctionDecl *ResultDecl =
          findFunctionInDeclContext(TU, LookupFnName)) {
    auto *ToDecl = cast<FunctionDecl>(
        Importer.Import(const_cast<FunctionDecl *>(ResultDecl)));
    assert(ToDecl->hasBody());
    assert(FD->hasBody() && "Functions already imported should have body.");
    ++NumGetCTUSuccess;
    return ToDecl;
  }
  ++NumIterateNotFound;
  return nullptr;
}

ASTImporter &CrossTranslationUnit::getOrCreateASTImporter(ASTContext &From) {
  auto I = ASTUnitImporterMap.find(From.getTranslationUnitDecl());
  if (I != ASTUnitImporterMap.end())
    return *I->second;
  ASTImporter *NewImporter =
      new ASTImporter(Context, Context.getSourceManager().getFileManager(),
                      From, From.getSourceManager().getFileManager(), false);
  ASTUnitImporterMap[From.getTranslationUnitDecl()].reset(NewImporter);
  return *NewImporter;
}

} // namespace tooling
} // namespace clang
