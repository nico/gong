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
  IdentifierList() { }
  IdentifierList(SourceLocation IILoc, IdentifierInfo *II) {
    initialize(IILoc, II);
  }

  /// Adds the initial identifier to the list.
  void initialize(SourceLocation IILoc, IdentifierInfo *II) {
    assert(Idents.size() == 0);
    Locs.push_back(IILoc);
    Idents.push_back(II);
  }

  /// Add an identifier to the end of this list when the list is not empty.
  void add(SourceLocation CommaLoc, SourceLocation IILoc, IdentifierInfo *II) {
    assert(Idents.size() > 0);
    CommaLocs.push_back(CommaLoc);
    Locs.push_back(IILoc);
    Idents.push_back(II);
  }

  /// Returns the identifiers in this list.
  ArrayRef<IdentifierInfo*> getIdents() { return Idents; }

  /// Returns the source location of the identifiers in this list.
  /// The result always has as many elements as the result of getIdents().
  ArrayRef<SourceLocation> getIdentLocs() { return Locs; }

  /// Returns the locations of the ',' tokens separating the identifiers.
  /// The result always has as one less element than the result of getIdents(),
  /// except when the list is empty.
  ArrayRef<SourceLocation> getCommaLocs() { return CommaLocs; }
};

}  // end namespace gong

#endif
