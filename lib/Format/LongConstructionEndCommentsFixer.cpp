//===--- LongConstructionEndCommentsFixer.cpp --------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief This file implements LongConstructionEndCommentsFixer, a TokenAnalyzer that
/// fixes long construction end comments.
///
//===----------------------------------------------------------------------===//

#include "LongConstructionEndCommentsFixer.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Regex.h"

#define DEBUG_TYPE "long-construction-end-comments-fixer"

namespace clang {
namespace format {

namespace {


std::string format(llvm::StringRef Code, const FormatStyle &Style) {
	std::vector<tooling::Range> Ranges(1, tooling::Range(0, Code.size()));
	FormattingAttemptStatus Status;
	tooling::Replacements Replaces =
		reformat(Style, Code, Ranges, "<stdin>", &Status);
	int ReplacementCount = Replaces.size();
	auto Result = applyAllReplacements(Code, Replaces);
	DEBUG(llvm::errs() << "\n" << *Result << "\n\n");
	return *Result;
}

// Computes the name of a long-construction given the token.
std::string computeName(const FormatToken *Tok, tok::TokenKind Kind, const FormatStyle &Style) {
  assert(Tok && Tok->is(Kind) &&
         "expecting a token");
  std::string name = "";
  // Collects all the non-comment tokens between token and '{'.
  name += Tok->TokenText;
  name += " ";
  const FormatToken *NextTok = Tok->getNextNonComment();
  while (NextTok && !NextTok->is(tok::l_brace)) {
	//addToken(name, NextTok);
	 name += NextTok->TokenText;
	 name += " ";
	NextTok = NextTok->getNextNonComment();
  }
  // reformat the string
  name = format(name, Style);

  // truncate the string after format
  size_t FirstEol = name.find_first_of("\n");
  if (FirstEol != std::string::npos) {
	  name = name.substr(0, FirstEol);
  }
  return name;
}

std::string computeEndCommentText(StringRef TokName, bool AddNewline) {
  std::string text = " //";
  if (!TokName.empty()) {
    text += ' ';
    text += TokName;
  }
  if (AddNewline)
    text += '\n';
  return text;
}

bool hasEndComment(const FormatToken *RBraceTok) {
  return RBraceTok->Next && RBraceTok->Next->is(tok::comment);
}

void addEndComment(const FormatToken *RBraceTok, StringRef EndCommentText,
                   const SourceManager &SourceMgr,
                   tooling::Replacements *Fixes) {
  auto EndLoc = RBraceTok->Tok.getEndLoc();
  auto Range = CharSourceRange::getCharRange(EndLoc, EndLoc);
  auto Err = Fixes->add(tooling::Replacement(SourceMgr, Range, EndCommentText));
  if (Err) {
    llvm::errs() << "Error while adding end comment: "
                 << llvm::toString(std::move(Err)) << "\n";
  }
}

void updateEndComment(const FormatToken *RBraceTok, StringRef EndCommentText,
                      const SourceManager &SourceMgr,
                      tooling::Replacements *Fixes) {
  assert(hasEndComment(RBraceTok));
  const FormatToken *Comment = RBraceTok->Next;
  auto Range = CharSourceRange::getCharRange(Comment->getStartOfNonWhitespace(),
                                             Comment->Tok.getEndLoc());
  auto Err = Fixes->add(tooling::Replacement(SourceMgr, Range, EndCommentText));
  if (Err) {
    llvm::errs() << "Error while updating long-construction end comment: "
                 << llvm::toString(std::move(Err)) << "\n";
  }
}

const FormatToken *
getToken(const AnnotatedLine *line,
                  const SmallVectorImpl<AnnotatedLine *> &AnnotatedLines, tok::TokenKind Kind) {
  if (!line->Affected || line->InPPDirective || !line->startsWith(tok::r_brace))
    return nullptr;
  size_t StartLineIndex = line->MatchingOpeningBlockLineIndex;
  if (StartLineIndex == UnwrappedLine::kInvalidIndex)
    return nullptr;
  assert(StartLineIndex < AnnotatedLines.size());
  const FormatToken *Tok = AnnotatedLines[StartLineIndex]->First;
  if (!Tok || Tok->isNot(Kind))
    return nullptr;
  return Tok;
}
} // namespace

LongConstructionEndCommentsFixer::LongConstructionEndCommentsFixer(const Environment &Env,
                                                     const FormatStyle &Style)
    : TokenAnalyzer(Env, Style) {}


tooling::Replacements LongConstructionEndCommentsFixer::analyze(
	TokenAnnotator &Annotator, SmallVectorImpl<AnnotatedLine *> &AnnotatedLines,
	FormatTokenLexer &Tokens) {
	if (Style.LongConstructionEndCommentsLenLimit == std::numeric_limits<unsigned>::max())
		return tooling::Replacements();

	tooling::Replacements Fixes;

	// TODO: it looks copy-paste
	analyze(Annotator, AnnotatedLines, Tokens, tok::kw_for, Fixes);
	analyze(Annotator, AnnotatedLines, Tokens, tok::kw_if, Fixes);
	analyze(Annotator, AnnotatedLines, Tokens, tok::kw_while, Fixes);
	analyze(Annotator, AnnotatedLines, Tokens, tok::kw_switch, Fixes);
	analyze(Annotator, AnnotatedLines, Tokens, tok::kw_case, Fixes);

	return Fixes;
}

tooling::Replacements LongConstructionEndCommentsFixer::analyze(
    TokenAnnotator &Annotator, SmallVectorImpl<AnnotatedLine *> &AnnotatedLines,
    FormatTokenLexer &Tokens, tok::TokenKind Kind, tooling::Replacements& Fixes) {

  const SourceManager &SourceMgr = Env.getSourceManager();
  AffectedRangeMgr.computeAffectedLines(AnnotatedLines.begin(),
                                        AnnotatedLines.end());
  
  size_t StartLineIndex = SIZE_MAX;
  for (size_t I = 0, E = AnnotatedLines.size(); I != E; ++I) {
    const AnnotatedLine *EndLine = AnnotatedLines[I];
    const FormatToken *Tok = getToken(EndLine, AnnotatedLines, Kind);
    if (!Tok)
      continue;
    FormatToken *RBraceTok = EndLine->First;
    if (RBraceTok->Finalized)
      continue;
    RBraceTok->Finalized = true;
    const FormatToken *EndCommentPrevTok = RBraceTok;
    if (StartLineIndex == SIZE_MAX)
      StartLineIndex = EndLine->MatchingOpeningBlockLineIndex;
    std::string TokName = computeName(Tok, Kind, Style);

    // The next token in the token stream after the place where the end comment
    // token must be. This is either the next token on the current line or the
    // first token on the next line.
    const FormatToken *EndCommentNextTok = EndCommentPrevTok->Next;
    if (EndCommentNextTok && EndCommentNextTok->is(tok::comment))
      EndCommentNextTok = EndCommentNextTok->Next;
    if (!EndCommentNextTok && I + 1 < E)
      EndCommentNextTok = AnnotatedLines[I + 1]->First;
    bool AddNewline = EndCommentNextTok &&
                      EndCommentNextTok->NewlinesBefore == 0 &&
                      EndCommentNextTok->isNot(tok::eof);
    const std::string EndCommentText =
        computeEndCommentText(TokName, AddNewline);
    if (!hasEndComment(EndCommentPrevTok)) {
      bool isShort = I - StartLineIndex  <= Style.LongConstructionEndCommentsLenLimit;
      if (!isShort)
        addEndComment(EndCommentPrevTok, EndCommentText, SourceMgr, &Fixes);
    } else { //if (!validEndComment(EndCommentPrevTok, TokName, kCommentPattern)) {
      updateEndComment(EndCommentPrevTok, EndCommentText, SourceMgr, &Fixes);
    }
    StartLineIndex = SIZE_MAX;
  }
  return Fixes;
}

} // namespace format
} // namespace clang
