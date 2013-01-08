//==--- SourceLocation.cpp - Compact identifier for Source Files -*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines accessor methods for the PresumedLoc class.
//
//===----------------------------------------------------------------------===//

#include "gong/Basic/SourceLocation.h"

#include "gong/Basic/SourceManager.h"
#include "llvm/Support/MemoryBuffer.h"
using namespace gong;

// static
PresumedLoc PresumedLoc::build(const SourceManager &SM, SourceLocation Loc) {
  int ID = SM.FindBufferContainingLoc(Loc);
  if (ID == -1)
    return PresumedLoc();
  std::pair<unsigned, unsigned> LC = SM.getLineAndColumn(Loc);
  return PresumedLoc(SM.getMemoryBuffer(ID)->getBufferIdentifier(), LC.first,
                     LC.second, SM.getBufferInfo(ID).IncludeLoc);
}
