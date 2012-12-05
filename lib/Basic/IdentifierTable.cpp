//===--- IdentifierTable.cpp - Hash table for identifier lookup -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the IdentifierInfo, IdentifierVisitor, and
// IdentifierTable interfaces.
//
//===----------------------------------------------------------------------===//

#include "gong/Basic/IdentifierTable.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/ErrorHandling.h"
#include <cctype>
#include <cstdio>

using namespace gong;

//===----------------------------------------------------------------------===//
// IdentifierInfo Implementation
//===----------------------------------------------------------------------===//

IdentifierInfo::IdentifierInfo() {
  TokenID = tok::identifier;
  BuiltinID = 0;
  IsExtension = false;
  IsFromAST = false;
  ChangedAfterLoad = false;
  RevertedTokenID = false;
  OutOfDate = false;
  FETokenInfo = 0;
  Entry = 0;
}

//===----------------------------------------------------------------------===//
// IdentifierTable Implementation
//===----------------------------------------------------------------------===//

IdentifierIterator::~IdentifierIterator() { }

IdentifierInfoLookup::~IdentifierInfoLookup() {}

namespace {
  /// \brief A simple identifier lookup iterator that represents an
  /// empty sequence of identifiers.
  class EmptyLookupIterator : public IdentifierIterator
  {
  public:
    virtual StringRef Next() { return StringRef(); }
  };
}

IdentifierIterator *IdentifierInfoLookup::getIdentifiers() const {
  return new EmptyLookupIterator();
}

IdentifierTable::IdentifierTable(IdentifierInfoLookup* externalLookup)
  : HashTable(8192), // Start with space for 8K identifiers.
    ExternalLookup(externalLookup) {

  // Populate the identifier table with info about keywords.
  AddKeywords();
}

//===----------------------------------------------------------------------===//
// Language Keyword Implementation
//===----------------------------------------------------------------------===//

/// AddKeyword - This method is used to associate a token ID with specific
/// identifiers because they are language keywords.  This causes the lexer to
/// automatically map matching identifiers to specialized token codes.
static void AddKeyword(StringRef Keyword,
                       tok::TokenKind TokenCode,
                       IdentifierTable &Table) {
  Table.get(Keyword, TokenCode);
}

/// AddKeywords - Add all keywords to the symbol table.
///
void IdentifierTable::AddKeywords() {
  // Add keywords and tokens for the current language.
#define KEYWORD(NAME) \
  AddKeyword(StringRef(#NAME), tok::kw_ ## NAME,  \
             *this);
#include "gong/Basic/TokenKinds.def"
#undef KEYWORD
}

//===----------------------------------------------------------------------===//
// Stats Implementation
//===----------------------------------------------------------------------===//

/// PrintStats - Print statistics about how well the identifier table is doing
/// at hashing identifiers.
void IdentifierTable::PrintStats() const {
  unsigned NumBuckets = HashTable.getNumBuckets();
  unsigned NumIdentifiers = HashTable.getNumItems();
  unsigned NumEmptyBuckets = NumBuckets-NumIdentifiers;
  unsigned AverageIdentifierSize = 0;
  unsigned MaxIdentifierLength = 0;

  // TODO: Figure out maximum times an identifier had to probe for -stats.
  for (llvm::StringMap<IdentifierInfo*, llvm::BumpPtrAllocator>::const_iterator
       I = HashTable.begin(), E = HashTable.end(); I != E; ++I) {
    unsigned IdLen = I->getKeyLength();
    AverageIdentifierSize += IdLen;
    if (MaxIdentifierLength < IdLen)
      MaxIdentifierLength = IdLen;
  }

  fprintf(stderr, "\n*** Identifier Table Stats:\n");
  fprintf(stderr, "# Identifiers:   %d\n", NumIdentifiers);
  fprintf(stderr, "# Empty Buckets: %d\n", NumEmptyBuckets);
  fprintf(stderr, "Hash density (#identifiers per bucket): %f\n",
          NumIdentifiers/(double)NumBuckets);
  fprintf(stderr, "Ave identifier length: %f\n",
          (AverageIdentifierSize/(double)NumIdentifiers));
  fprintf(stderr, "Max identifier length: %d\n", MaxIdentifierLength);

  // Compute statistics about the memory allocated for identifiers.
  HashTable.getAllocator().PrintStats();
}
