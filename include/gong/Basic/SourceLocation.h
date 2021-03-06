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

namespace llvm {
class SourceMgr;
}

namespace gong {

typedef llvm::SourceMgr SourceManager;

typedef llvm::SMLoc SourceLocation;
typedef llvm::SMRange SourceRange;

/// \brief Represents a character-granular source range.
///
/// The underlying SourceRange can either specify the starting/ending character
/// of the range, or it can specify the start of the range and the start of the
/// last token of the range (a "token range").  In the token range case, the
/// size of the last token must be measured to determine the actual end of the
/// range.
class CharSourceRange { 
  SourceRange Range;
  bool IsTokenRange;
public:
  CharSourceRange() : IsTokenRange(false) {}
  CharSourceRange(SourceRange R, bool ITR) : Range(R), IsTokenRange(ITR) {}

  static CharSourceRange getTokenRange(SourceRange R) {
    return CharSourceRange(R, true);
  }

  static CharSourceRange getCharRange(SourceRange R) {
    return CharSourceRange(R, false);
  }
    
  static CharSourceRange getTokenRange(SourceLocation B, SourceLocation E) {
    return getTokenRange(SourceRange(B, E));
  }
  static CharSourceRange getCharRange(SourceLocation B, SourceLocation E) {
    return getCharRange(SourceRange(B, E));
  }
  
  /// \brief Return true if the end of this range specifies the start of
  /// the last token.  Return false if the end of this range specifies the last
  /// character in the range.
  bool isTokenRange() const { return IsTokenRange; }
  bool isCharRange() const { return !IsTokenRange; }
  
  SourceLocation getBegin() const { return Range.Start; }
  SourceLocation getEnd() const { return Range.End; }
  const SourceRange &getAsRange() const { return Range; }
 
  void setBegin(SourceLocation b) { Range.Start = b; }
  void setEnd(SourceLocation e) { Range.End = e; }
  
  bool isValid() const { return Range.isValid(); }
  bool isInvalid() const { return !isValid(); }
};

/// \brief Represents an unpacked "presumed" location which can be presented
/// to the user.
///
/// A 'presumed' location can be modified by \#line and GNU line marker
/// directives and is always the expansion point of a normal location.
///
/// You can get a PresumedLoc from a SourceLocation with SourceMgr.
class PresumedLoc {
  const char *Filename;
  unsigned Line, Col;
  SourceLocation IncludeLoc;
public:
  PresumedLoc() : Filename(0) {}
  PresumedLoc(const char *FN, unsigned Ln, unsigned Co, SourceLocation IL)
    : Filename(FN), Line(Ln), Col(Co), IncludeLoc(IL) {
  }

  /// \brief builds a PresumedLoc given a SourceManager and a SourceLocation
  /// that belongs to that manager.
  static PresumedLoc build(const SourceManager &SM, SourceLocation Loc);

  /// \brief Return true if this object is invalid or uninitialized.
  ///
  /// This occurs when created with invalid source locations or when walking
  /// off the top of a \#include stack.
  bool isInvalid() const { return Filename == 0; }
  bool isValid() const { return Filename != 0; }

  /// \brief Return the presumed filename of this location.
  ///
  /// This can be affected by \#line etc.
  const char *getFilename() const { return Filename; }

  /// \brief Return the presumed line number of this location.
  ///
  /// This can be affected by \#line etc.
  unsigned getLine() const { return Line; }

  /// \brief Return the presumed column number of this location.
  ///
  /// This cannot be affected by \#line, but is packaged here for convenience.
  unsigned getColumn() const { return Col; }

  /// \brief Return the presumed include location of this location.
  ///
  /// This can be affected by GNU linemarker directives.
  SourceLocation getIncludeLoc() const { return IncludeLoc; }
};

}  // end namespace gong

#endif

