//===--- IdentifierTable.h - Hash table for identifier lookup ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines the gong::IdentifierInfo, gong::IdentifierTable, and
/// gong::Selector interfaces.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_BASIC_IDENTIFIERTABLE_H
#define LLVM_GONG_BASIC_IDENTIFIERTABLE_H

#include "gong/Basic/TokenKinds.h"
#include "gong/Basic/LLVM.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/PointerLikeTypeTraits.h"
#include <cassert>
#include <string>

namespace llvm {
  template <typename T> struct DenseMapInfo;
}

namespace gong {
  class IdentifierInfo;
  class IdentifierTable;

/// One of these records is kept for each identifier that
/// is lexed.  This contains information about whether the token was \#define'd,
/// is a language keyword, or if it is a front-end token of some sort (e.g. a
/// variable or function name).  The preprocessor keeps this information in a
/// set, and all tok::identifier tokens have a pointer to one of these.
class IdentifierInfo {
  unsigned TokenID            : 9; // Front-end token ID or tok::identifier.
  unsigned BuiltinID          :11;
  bool IsExtension            : 1; // True if identifier is a lang extension.
  bool IsFromAST              : 1; // True if identifier was loaded (at least 
                                   // partially) from an AST file.
  bool ChangedAfterLoad       : 1; // True if identifier has changed from the
                                   // definition loaded from an AST file.
  bool RevertedTokenID        : 1; // True if RevertTokenIDToIdentifier was
                                   // called.
  bool OutOfDate              : 1; // True if there may be additional
                                   // information about this identifier
                                   // stored externally.

  void *FETokenInfo;               // Managed by the language front-end.
  llvm::StringMapEntry<IdentifierInfo*> *Entry;

  IdentifierInfo(const IdentifierInfo&) = delete;
  void operator=(const IdentifierInfo&) = delete;

  friend class IdentifierTable;
  
public:
  IdentifierInfo();


  /// \brief Return true if this is the identifier for the specified string.
  ///
  /// This is intended to be used for string literals only: II->isStr("foo").
  template <std::size_t StrLen>
  bool isStr(const char (&Str)[StrLen]) const {
    return getLength() == StrLen-1 && !memcmp(getNameStart(), Str, StrLen-1);
  }

  /// \brief Return the beginning of the actual null-terminated string for this
  /// identifier.
  ///
  const char *getNameStart() const {
    return Entry->getKeyData();
  }

  /// \brief Efficiently return the length of this identifier info.
  ///
  unsigned getLength() const {
    return Entry->getKeyLength();
  }

  /// \brief Return the actual identifier string.
  StringRef getName() const {
    return StringRef(getNameStart(), getLength());
  }

  /// getTokenID - If this is a source-language token (e.g. 'for'), this API
  /// can be used to cause the lexer to map identifiers to source-language
  /// tokens.
  tok::TokenKind getTokenID() const { return (tok::TokenKind)TokenID; }

  /// \brief True if RevertTokenIDToIdentifier() was called.
  bool hasRevertedTokenIDToIdentifier() const { return RevertedTokenID; }

  /// \brief Revert TokenID to tok::identifier; used for GNU libstdc++ 4.2
  /// compatibility.
  ///
  /// TokenID is normally read-only but there are 2 instances where we revert it
  /// to tok::identifier for libstdc++ 4.2. Keep track of when this happens
  /// using this method so we can inform serialization about it.
  void RevertTokenIDToIdentifier() {
    assert(TokenID != tok::identifier && "Already at tok::identifier");
    TokenID = tok::identifier;
    RevertedTokenID = true;
  }

  /// getBuiltinID - Return a value indicating whether this is a builtin
  /// function.  0 is not-built-in.  1 is builtin-for-some-nonprimary-target.
  /// 2+ are specific builtin functions.
  unsigned getBuiltinID() const {
    return BuiltinID;
  }
  void setBuiltinID(unsigned ID) {
    BuiltinID = ID;
    assert(BuiltinID == ID && "ID too large for field!");
  }

  /// get/setExtension - Initialize information about whether or not this
  /// language token is an extension.  This controls extension warnings, and is
  /// only valid if a custom token ID is set.
  bool isExtensionToken() const { return IsExtension; }
  void setIsExtensionToken(bool Val) {
    IsExtension = Val;
  }

  /// getFETokenInfo/setFETokenInfo - The language front-end is allowed to
  /// associate arbitrary metadata with this token.
  template<typename T>
  T *getFETokenInfo() const { return static_cast<T*>(FETokenInfo); }
  void setFETokenInfo(void *T) { FETokenInfo = T; }

  /// isFromAST - Return true if the identifier in its current state was loaded
  /// from an AST file.
  bool isFromAST() const { return IsFromAST; }

  void setIsFromAST() { IsFromAST = true; }

  /// \brief Determine whether this identifier has changed since it was loaded
  /// from an AST file.
  bool hasChangedSinceDeserialization() const {
    return ChangedAfterLoad;
  }
  
  /// \brief Note that this identifier has changed since it was loaded from
  /// an AST file.
  void setChangedSinceDeserialization() {
    ChangedAfterLoad = true;
  }

  /// \brief Determine whether the information for this identifier is out of
  /// date with respect to the external source.
  bool isOutOfDate() const { return OutOfDate; }
  
  /// \brief Set whether the information for this identifier is out of
  /// date with respect to the external source.
  void setOutOfDate(bool OOD) {
    OutOfDate = OOD;
  }
};

/// \brief An iterator that walks over all of the known identifiers
/// in the lookup table.
///
/// Since this iterator uses an abstract interface via virtual
/// functions, it uses an object-oriented interface rather than the
/// more standard C++ STL iterator interface. In this OO-style
/// iteration, the single function \c Next() provides dereference,
/// advance, and end-of-sequence checking in a single
/// operation. Subclasses of this iterator type will provide the
/// actual functionality.
class IdentifierIterator {
private:
  IdentifierIterator(const IdentifierIterator &) = delete;
  void operator=(const IdentifierIterator &) = delete;

protected:
  IdentifierIterator() { }
  
public:
  virtual ~IdentifierIterator();

  /// \brief Retrieve the next string in the identifier table and
  /// advances the iterator for the following string.
  ///
  /// \returns The next string in the identifier table. If there is
  /// no such string, returns an empty \c StringRef.
  virtual StringRef Next() = 0;
};

/// IdentifierInfoLookup - An abstract class used by IdentifierTable that
///  provides an interface for performing lookups from strings
/// (const char *) to IdentiferInfo objects.
class IdentifierInfoLookup {
public:
  virtual ~IdentifierInfoLookup();

  /// get - Return the identifier token info for the specified named identifier.
  ///  Unlike the version in IdentifierTable, this returns a pointer instead
  ///  of a reference.  If the pointer is NULL then the IdentifierInfo cannot
  ///  be found.
  virtual IdentifierInfo* get(StringRef Name) = 0;

  /// \brief Retrieve an iterator into the set of all identifiers
  /// known to this identifier lookup source.
  ///
  /// This routine provides access to all of the identifiers known to
  /// the identifier lookup, allowing access to the contents of the
  /// identifiers without introducing the overhead of constructing
  /// IdentifierInfo objects for each.
  ///
  /// \returns A new iterator into the set of known identifiers. The
  /// caller is responsible for deleting this iterator.
  virtual IdentifierIterator *getIdentifiers() const;
};

/// \brief Implements an efficient mapping from strings to IdentifierInfo nodes.
///
/// This has no other purpose, but this is an extremely performance-critical
/// piece of the code, as each occurrence of every identifier goes through
/// here when lexed.
class IdentifierTable {
  // Shark shows that using MallocAllocator is *much* slower than using this
  // BumpPtrAllocator!
  typedef llvm::StringMap<IdentifierInfo*, llvm::BumpPtrAllocator> HashTableTy;
  HashTableTy HashTable;

  IdentifierInfoLookup* ExternalLookup;

public:
  /// \brief Create the identifier table, populating it with info about the
  /// language keywords.
  IdentifierTable(IdentifierInfoLookup* externalLookup = 0);

  /// \brief Set the external identifier lookup mechanism.
  void setExternalIdentifierLookup(IdentifierInfoLookup *IILookup) {
    ExternalLookup = IILookup;
  }

  /// \brief Retrieve the external identifier lookup object, if any.
  IdentifierInfoLookup *getExternalIdentifierLookup() const {
    return ExternalLookup;
  }
  
  llvm::BumpPtrAllocator& getAllocator() {
    return HashTable.getAllocator();
  }

  /// \brief Return the identifier token info for the specified named
  /// identifier.
  IdentifierInfo &get(StringRef Name) {
    auto &Entry = *HashTable.insert(std::make_pair(Name, nullptr)).first;

    IdentifierInfo *&II = Entry.second;
    if (II) return *II;

    // No entry; if we have an external lookup, look there first.
    if (ExternalLookup) {
      II = ExternalLookup->get(Name);
      if (II)
        return *II;
    }

    // Lookups failed, make a new IdentifierInfo.
    void *Mem = getAllocator().Allocate<IdentifierInfo>();
    II = new (Mem) IdentifierInfo();

    // Make sure getName() knows how to find the IdentifierInfo
    // contents.
    II->Entry = &Entry;

    return *II;
  }

  IdentifierInfo &get(StringRef Name, tok::TokenKind TokenCode) {
    IdentifierInfo &II = get(Name);
    II.TokenID = TokenCode;
    assert(II.TokenID == (unsigned) TokenCode && "TokenCode too large");
    return II;
  }

  /// \brief Gets an IdentifierInfo for the given name without consulting
  ///        external sources.
  ///
  /// This is a version of get() meant for external sources that want to
  /// introduce or modify an identifier. If they called get(), they would
  /// likely end up in a recursion.
  IdentifierInfo &getOwn(StringRef Name) {
    auto &Entry = *HashTable.insert(std::make_pair(Name, nullptr)).first;

    IdentifierInfo *&II = Entry.second;
    if (II)
      return *II;

    // Lookups failed, make a new IdentifierInfo.
    void *Mem = getAllocator().Allocate<IdentifierInfo>();
    II = new (Mem) IdentifierInfo();

    // Make sure getName() knows how to find the IdentifierInfo
    // contents.
    II->Entry = &Entry;

    return *II;
  }

  typedef HashTableTy::const_iterator iterator;
  typedef HashTableTy::const_iterator const_iterator;

  iterator begin() const { return HashTable.begin(); }
  iterator end() const   { return HashTable.end(); }
  unsigned size() const { return HashTable.size(); }

  /// \brief Print some statistics to stderr that indicate how well the
  /// hashing is doing.
  void PrintStats() const;

  void AddKeywords();
};

}  // end namespace gong

namespace llvm {

// Provide PointerLikeTypeTraits for IdentifierInfo pointers, which
// are not guaranteed to be 8-byte aligned.
template<>
struct PointerLikeTypeTraits<gong::IdentifierInfo*> {
public:
  static inline void *getAsVoidPointer(gong::IdentifierInfo* P) {
    return P;
  }
  static inline gong::IdentifierInfo *getFromVoidPointer(void *P) {
    return static_cast<gong::IdentifierInfo*>(P);
  }
  enum { NumLowBitsAvailable = 1 };
};

template<>
struct PointerLikeTypeTraits<const gong::IdentifierInfo*> {
public:
  static inline const void *getAsVoidPointer(const gong::IdentifierInfo* P) {
    return P;
  }
  static inline const gong::IdentifierInfo *getFromVoidPointer(const void *P) {
    return static_cast<const gong::IdentifierInfo*>(P);
  }
  enum { NumLowBitsAvailable = 1 };
};

}  // end namespace llvm
#endif
