//===--- LongConstructionEndCommentsFixer.h ----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief This file declares LongConstructionEndCommentsFixer, a TokenAnalyzer that
/// fixes long construction end comments.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_FORMAT_LONGCONSTRUCTIONENDCOMMENTSFIXER_H
#define LLVM_CLANG_LIB_FORMAT_LONGCONSTRUCTIONENDCOMMENTSFIXER_H

#include "TokenAnalyzer.h"

namespace clang {
namespace format {

class LongConstructionEndCommentsFixer : public TokenAnalyzer {
public:
  LongConstructionEndCommentsFixer(const Environment &Env, const FormatStyle &Style);

  tooling::Replacements
  analyze(TokenAnnotator &Annotator,
          SmallVectorImpl<AnnotatedLine *> &AnnotatedLines,
          FormatTokenLexer &Tokens) override;

  tooling::Replacements
	  analyze(TokenAnnotator &Annotator,
		  SmallVectorImpl<AnnotatedLine *> &AnnotatedLines,
		  FormatTokenLexer &Tokens, tok::TokenKind Kind, tooling::Replacements& Fixes);
};

} // end namespace format
} // end namespace clang

#endif
