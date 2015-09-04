//------------------------------------------------------------------------------
// Tooling sample. Demonstrates:
//
// * How to write a simple source tool using libTooling.
// * How to use RecursiveASTVisitor to find interesting AST nodes.
// * How to use the Rewriter API to rewrite the source code.
//
// Eli Bendersky (eliben@gmail.com)
// This code is in the public domain
//------------------------------------------------------------------------------
#pragma warning(disable:4146)
#include <sstream>
#include <string>
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/raw_ostream.h"
using namespace clang;
using namespace clang::driver;
using namespace clang::tooling;
static llvm::cl::OptionCategory ToolingSampleCategory("Tooling Sample");
// By implementing RecursiveASTVisitor, we can specify which AST nodes
// we're interested in by overriding relevant methods.
class MyASTVisitor : public RecursiveASTVisitor<MyASTVisitor> {
public:
	MyASTVisitor(Rewriter &R) : TheRewriter(R) {}
	bool VisitStmt(Stmt *s) {
		// Only care about If statements.
		if (isa<IfStmt>(s)) {
			IfStmt *IfStatement = cast<IfStmt>(s);
			Stmt *Then = IfStatement->getThen();
			TheRewriter.InsertText(Then->getLocStart(), "// the 'if' part\n", true,
				true);
			Stmt *Else = IfStatement->getElse();
			if (Else)
				TheRewriter.InsertText(Else->getLocStart(), "// the 'else' part\n",
				true, true);
		}
		return true;
	}
	bool VisitFunctionDecl(FunctionDecl *f) {
		// Only function definitions (with bodies), not declarations.
		if (f->hasBody()) {
			Stmt *FuncBody = f->getBody();
			// Type name as string
			QualType QT = f->getReturnType();
			std::string TypeStr = QT.getAsString();
			// Function name
			DeclarationName DeclName = f->getNameInfo().getName();
			std::string FuncName = DeclName.getAsString();
			// Add comment before
			std::stringstream SSBefore;
			SSBefore << "// Begin function " << FuncName << " returning " << TypeStr
				<< "\n";
			SourceLocation ST = f->getSourceRange().getBegin();
			TheRewriter.InsertText(ST, SSBefore.str(), true, true);
			// And after
			std::stringstream SSAfter;
			SSAfter << "\n// End function " << FuncName;
			ST = FuncBody->getLocEnd().getLocWithOffset(1);
			TheRewriter.InsertText(ST, SSAfter.str(), true, true);
		}
		return true;
	}
private:
	Rewriter &TheRewriter;
};
// Implementation of the ASTConsumer interface for reading an AST produced
// by the Clang parser.
class MyASTConsumer : public ASTConsumer {
public:
	MyASTConsumer(Rewriter &R) : Visitor(R) {}
	// Override the method that gets called for each parsed top-level
	// declaration.
	bool HandleTopLevelDecl(DeclGroupRef DR) override {
		for (DeclGroupRef::iterator b = DR.begin(), e = DR.end(); b != e; ++b) {
			// Traverse the declaration using our AST visitor.
			Visitor.TraverseDecl(*b);
			(*b)->dump();
		}
		return true;
	}
private:
	MyASTVisitor Visitor;
};
// For each source file provided to the tool, a new FrontendAction is created.
class MyFrontendAction : public ASTFrontendAction {
public:
	MyFrontendAction() {}
	void EndSourceFileAction() override {
		SourceManager &SM = TheRewriter.getSourceMgr();
		llvm::errs() << "** EndSourceFileAction for: "
			<< SM.getFileEntryForID(SM.getMainFileID())->getName() << "\n";
		// Now emit the rewritten buffer.
		TheRewriter.getEditBuffer(SM.getMainFileID()).write(llvm::outs());
	}
	std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
		StringRef file) override {
		llvm::errs() << "** Creating AST consumer for: " << file << "\n";
		TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
		return llvm::make_unique<MyASTConsumer>(TheRewriter);
	}
private:
	Rewriter TheRewriter;
};
int main(int argc, const char **argv) {
	CommonOptionsParser op(argc, argv, ToolingSampleCategory);
	ClangTool Tool(op.getCompilations(), op.getSourcePathList());
	// ClangTool::run accepts a FrontendActionFactory, which is then used to
	// create new objects implementing the FrontendAction interface. Here we use
	// the helper newFrontendActionFactory to create a default factory that will
	// return a new MyFrontendAction object every time.
	// To further customize this, we could create our own factory class.
	return Tool.run(newFrontendActionFactory<MyFrontendAction>().get());
}
