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
    CharSourceRange Result;
    Result.Range = R;
    Result.IsTokenRange = true;
    return Result;
  }

  static CharSourceRange getCharRange(SourceRange R) {
    CharSourceRange Result;
    Result.Range = R;
    Result.IsTokenRange = false;
    return Result;
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

}  // end namespace gong

#endif

