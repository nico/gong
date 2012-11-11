//===--- SourceLocation.h - Compact identifier for Source Files -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines the gong::SourceLocation class and associated facilities.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_SOURCELOCATION_H
#define LLVM_GONG_SOURCELOCATION_H

#include "gong/Basic/LLVM.h"
#include "llvm/Support/SMLoc.h"

namespace gong {

typedef llvm::SMLoc SourceLocation;
typedef llvm::SMRange SourceRange;

}  // end namespace gong

#endif

