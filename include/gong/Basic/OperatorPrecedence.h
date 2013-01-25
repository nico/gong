//===--- OperatorPrecedence.h - Operator precedence levels ------*- C++ -*-===//
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

#ifndef LLVM_GONG_OPERATOR_PRECEDENCE_H
#define LLVM_GONG_OPERATOR_PRECEDENCE_H

#include "gong/Basic/TokenKinds.h"

namespace gong {

/// These are precedences for the binary operators in the Go grammar.
/// Low precedences numbers bind more weakly than high numbers.
/// http://golang.org/ref/spec#Operator_precedence
namespace prec {
  enum Level {
    Unknown         = 0,    // Not binary operator.
    Lowest          = 1,
    LogicalOr       = 1,    // ||
    LogicalAnd      = 2,    // &&
    Equality        = 3,    // ==, !=, <, <=, >, >=
    Additive        = 4,    // +, -, |, ^
    Multiplicative  = 5     // *, /, %, <<, >>, &, , &^
  };
}

/// \brief Return the precedence of the specified binary operator token.
prec::Level getBinOpPrecedence(tok::TokenKind Kind);

}  // end namespace gong

#endif
