//===--- OperatorPrecedence.cpp ---------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines and computes precedence levels for binary operators.
///
//===----------------------------------------------------------------------===//

#include "gong/Basic/OperatorPrecedence.h"

namespace gong {

/// \brief Return the precedence of the specified binary operator token.
prec::Level getBinOpPrecedence(tok::TokenKind Kind) {
  switch (Kind) {
  default:                        return prec::Unknown;
  case tok::pipepipe:             return prec::LogicalOr;
  case tok::ampamp:               return prec::LogicalAnd;

  case tok::equalequal:
  case tok::exclaimequal:
  case tok::less:
  case tok::lessequal:
  case tok::greater:
  case tok::greaterequal:         return prec::Equality;

  case tok::plus:
  case tok::minus:
  case tok::pipe:
  case tok::caret:                return prec::Additive;

  case tok::star:
  case tok::slash:
  case tok::percent:
  case tok::lessless:
  case tok::greatergreater:
  case tok::amp:
  case tok::ampcaret:             return prec::Multiplicative;
  }
}

} // namespace gong
