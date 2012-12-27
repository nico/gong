//===--- Specifiers.h - Declaration and Type Specifiers ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines various enumerations that describe declaration and
/// type specifiers.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_BASIC_SPECIFIERS_H
#define LLVM_GONG_BASIC_SPECIFIERS_H

namespace gong {

/// The kind of a declaration group such as "var ( a int )" or "type ( b int )".
enum DeclGroupKind {
  DGK_Const,
  DGK_Type,
  DGK_Var
};

} // end namespace gong

#endif // LLVM_GONG_BASIC_SPECIFIERS_H

