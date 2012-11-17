//===--- Token.h - Token interface ------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Token interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_TOKEN_H
#define LLVM_GONG_TOKEN_H

#include "gong/Basic/TokenKinds.h"
#include "gong/Basic/SourceLocation.h"
#include <cstdlib>

namespace gong {

class IdentifierInfo;

/// Token - This structure provides full information about a lexed token.
/// It is not intended to be space efficient, it is intended to return as much
/// information as possible about each returned token.  This is expected to be
/// compressed into a smaller form if memory footprint is important.
class Token {
  /// The location of the token.
  SourceLocation Loc;

  // Conceptually these next two fields could be in a union.  However, this
  // causes gcc 4.2 to pessimize LexTokenInternal, a very performance critical
  // routine. Keeping as separate members with casts until a more beautiful fix
  // presents itself.

  /// UintData - This holds either the length of the token text, when
  /// a normal token, or the end of the SourceRange when an annotation
  /// token.
  unsigned UintData;

  /// PtrData - This is a union of four different pointer types, which depends
  /// on what type of token this is:
  ///  Identifiers, keywords, etc:
  ///    This is an IdentifierInfo*, which contains the uniqued identifier
  ///    spelling.
  ///  Literals:  isLiteral() returns true.
  ///    This is a pointer to the start of the token in a text buffer, which
  ///    may be dirty (have trigraphs / escaped newlines).
  ///  Other:
  ///    This is null.
  void *PtrData;

  /// Kind - The actual flavor of token this is.
  ///
  unsigned short Kind;

  /// Flags - Bits we track about this token, members of the TokenFlags enum.
  unsigned char Flags;
public:

  // Various flags set per token:
  enum TokenFlags {
    StartOfLine   = 0x01,  // At start of line or only after whitespace.
    LeadingSpace  = 0x02,  // Whitespace exists before this token.
    NeedsCleaning = 0x08,  // Contained an escaped newline or trigraph.
    LeadingEmptyMacro = 0x10, // Empty macro exists before this token.
    InsertedSemi  = 0x20   // For automatically inserted semicolons.
  };

  tok::TokenKind getKind() const { return (tok::TokenKind)Kind; }
  void setKind(tok::TokenKind K) { Kind = K; }

  /// is/isNot - Predicates to check if this token is a specific kind, as in
  /// "if (Tok.is(tok::l_brace)) {...}".
  bool is(tok::TokenKind K) const { return Kind == (unsigned) K; }
  bool isNot(tok::TokenKind K) const { return Kind != (unsigned) K; }

  /// \brief Return true if this is a raw identifier (when lexing
  /// in raw mode) or a non-keyword identifier (when lexing in non-raw mode).
  bool isIdentifier() const {
    return getKind() == tok::identifier;
  }

  /// \brief Return true if this is a "literal", like a numeric
  /// constant, string, etc.
  bool isLiteral() const {
    return tok::isLiteral(getKind());
  }

  /// \brief Return a source location identifier for the specified
  /// offset in the current file.
  SourceLocation getLocation() const { return Loc; }

  unsigned getLength() const {
    return UintData;
  }

  void setLocation(SourceLocation L) { Loc = L; }
  void setLength(unsigned Len) {
    UintData = Len;
  }

  const char *getName() const {
    return tok::getTokenName( (tok::TokenKind) Kind);
  }

  /// \brief Reset all flags to cleared.
  void startToken() {
    Kind = tok::unknown;
    Flags = 0;
    PtrData = 0;
    UintData = 0;
    Loc = SourceLocation();
  }

  IdentifierInfo *getIdentifierInfo() const {
    if (isLiteral()) return 0;
    return (IdentifierInfo*) PtrData;
  }
  void setIdentifierInfo(IdentifierInfo *II) {
    PtrData = (void*) II;
  }

  /// getLiteralData - For a literal token (numeric constant, string, etc), this
  /// returns a pointer to the start of it in the text buffer if known, null
  /// otherwise.
  const char *getLiteralData() const {
    assert(isLiteral() && "Cannot get literal data of non-literal");
    return reinterpret_cast<const char*>(PtrData);
  }
  void setLiteralData(const char *Ptr) {
    assert(isLiteral() && "Cannot set literal data of non-literal");
    PtrData = const_cast<char*>(Ptr);
  }

  /// \brief Set the specified flag.
  void setFlag(TokenFlags Flag) {
    Flags |= Flag;
  }

  /// \brief Unset the specified flag.
  void clearFlag(TokenFlags Flag) {
    Flags &= ~Flag;
  }

  /// \brief Return the internal represtation of the flags.
  ///
  /// This is only intended for low-level operations such as writing tokens to
  /// disk.
  unsigned getFlags() const {
    return Flags;
  }

  /// \brief Set a flag to either true or false.
  void setFlagValue(TokenFlags Flag, bool Val) {
    if (Val)
      setFlag(Flag);
    else
      clearFlag(Flag);
  }

  /// isAtStartOfLine - Return true if this token is at the start of a line.
  ///
  bool isAtStartOfLine() const { return (Flags & StartOfLine) ? true : false; }

  /// \brief Return true if this token has whitespace before it.
  ///
  bool hasLeadingSpace() const { return (Flags & LeadingSpace) ? true : false; }

  /// \brief Return true if this token has trigraphs or escaped newlines in it.
  bool needsCleaning() const { return (Flags & NeedsCleaning) ? true : false; }

  bool isInsertedSemi() const { return (Flags & InsertedSemi) ? true : false; }
};

}  // end namespace gong

namespace llvm {
  template <>
  struct isPodLike<gong::Token> { static const bool value = true; };
}  // end namespace llvm

#endif

