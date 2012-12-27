//===--- Scope.h - Scope interface ------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the IdentifierList interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_PARSE_IDENTIFIER_LIST_H
#define LLVM_GONG_PARSE_IDENTIFIER_LIST_H

#include "gong/Basic/SourceLocation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"

namespace gong {

/// A list of IdentifierInfos with SourceLocations.
class IdentifierList {
  llvm::SmallVector<IdentifierInfo*, 2> Idents;
  llvm::SmallVector<SourceLocation, 2> Locs;
  llvm::SmallVector<SourceLocation, 1> CommaLocs;
  
public:
  IdentifierList(SourceLocation IILoc, IdentifierInfo *II) {
    Locs.push_back(IILoc);
    Idents.push_back(II);
  }

  /// Add an identifier to the end of this list.
  void add(SourceLocation CommaLoc, SourceLocation IILoc, IdentifierInfo *II) {
    CommaLocs.push_back(CommaLoc);
    Locs.push_back(IILoc);
    Idents.push_back(II);
  }

  ArrayRef<IdentifierInfo*> getIdents() { return Idents; }
  ArrayRef<SourceLocation> getIdentLocs() { return Locs; }
  ArrayRef<SourceLocation> getCommaLocs() { return CommaLocs; }
};

}  // end namespace gong

#endif
