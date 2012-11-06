//===--- TokenKinds.h - Enum values for C Token Kinds -----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines the gong::TokenKind enum and support functions.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_TOKENKINDS_H
#define LLVM_GONG_TOKENKINDS_H

namespace gong {

namespace tok {

/// \brief Provides a simple uniform namespace for tokens.
enum TokenKind {
#define TOK(X) X,
#include "gong/Basic/TokenKinds.def"
  NUM_TOKENS
};

/// \brief Determines the name of a token as used within the front end.
///
/// The name of a token will be an internal name (such as "l_square")
/// and should not be used as part of diagnostic messages.
const char *getTokenName(enum TokenKind Kind);

/// \brief Determines the spelling of simple punctuation tokens like
/// '!' or '%', and returns NULL for literal tokens.
///
/// This routine only retrieves the "simple" spelling of the token.
/// For the actual spelling of a given Token, use Lexer::getSpelling().
const char *getTokenSimpleSpelling(enum TokenKind Kind);

/// \brief Return true if this is a "literal" kind, like a numeric
/// constant, string, etc.
inline bool isLiteral(TokenKind K) {
  return (K == tok::numeric_literal) || (K == tok::rune_literal) ||
         (K == tok::string_literal) || (K == tok::raw_string_literal);
}

}  // end namespace tok
}  // end namespace gong

#endif
