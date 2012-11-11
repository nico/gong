//===--- Lexer.h - Go Lexer -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Lexer interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_LEXER_H
#define LLVM_GONG_LEXER_H

#include "gong/Lex/Token.h"
#include "llvm/ADT/SmallVector.h"
#include <string>
#include <cassert>

namespace llvm {
  class MemoryBuffer;
}

namespace gong {

/// ConflictMarkerKind - Kinds of conflict marker which the lexer might be
/// recovering from.
enum ConflictMarkerKind {
  /// Not within a conflict marker.
  CMK_None,
  /// A normal or diff3 conflict marker, initiated by at least 7 "<"s,
  /// separated by at least 7 "="s or "|"s, and terminated by at least 7 ">"s.
  CMK_Normal,
  /// A Perforce-style conflict marker, initiated by 4 ">"s,
  /// separated by 4 "="s, and terminated by 4 "<"s.
  CMK_Perforce
};

/// Lexer - This provides a simple interface that turns a text buffer into a
/// stream of tokens.  This provides no support for file reading or buffering,
/// or buffering/seeking of tokens, only forward lexing is supported.
class Lexer {
  //===--------------------------------------------------------------------===//
  // Constant configuration values for this lexer.
  const char *BufferStart;       // Start of the buffer.
  const char *BufferEnd;         // End of the buffer.
  
  //===--------------------------------------------------------------------===//
  // Context that changes as the file is lexed.

  // BufferPtr - Current pointer into the buffer.  This is the next character
  // to be lexed.
  const char *BufferPtr;

  // CurrentConflictMarkerState - The kind of conflict marker we are handling.
  ConflictMarkerKind CurrentConflictMarkerState;

  // IsAtStartOfLine - True if the next lexed token should get the "start of
  // line" flag set on it.
  bool IsAtStartOfLine;

  Lexer(const Lexer &) LLVM_DELETED_FUNCTION;
  void operator=(const Lexer &) LLVM_DELETED_FUNCTION;

  void InitLexer(const char *BufStart, const char *BufPtr, const char *BufEnd);
public:

  /// Lexer constructor - Create a new lexer object for the specified buffer.
  /// This lexer assumes that the associated file buffer objects will outlive
  /// it, so it doesn't take ownership of it.
  Lexer(const llvm::MemoryBuffer *InputBuffer);

  /// Lex - Return the next token in the file.  If this is the end of file, it
  /// return the tok::eof token.  This implicitly involves the preprocessor.
  void Lex(Token &Result) {
    // Start a new token.
    Result.startToken();

    // NOTE, any changes here should also change code after calls to
    // Preprocessor::HandleDirective
    if (IsAtStartOfLine) {
      Result.setFlag(Token::StartOfLine);
      IsAtStartOfLine = false;
    }

    // Get a token.  Note that this may delete the current lexer if the end of
    // file is reached.
    LexTokenInternal(Result);
  }

  const char *getBufferStart() const { return BufferStart; }

  /// ReadToEndOfLine - Read the rest of the current preprocessor line as an
  /// uninterpreted string.  This switches the lexer out of directive mode.
  void ReadToEndOfLine(SmallVectorImpl<char> *Result = 0);


  /// Diag - Forwarding function for diagnostics.  This translate a source
  /// position in the current buffer into a SourceLocation object for rendering.
  //DiagnosticBuilder Diag(const char *Loc, unsigned DiagID) const;

  /// getSourceLocation - Return a source location identifier for the specified
  /// offset in the current file.
  SourceLocation getSourceLocation(const char *Loc) const;

  /// \brief Return the current location in the buffer.
  const char *getBufferLocation() const { return BufferPtr; }
  
  /// AdvanceToTokenCharacter - If the current SourceLocation specifies a
  /// location at the start of a token, return a new location that specifies a
  /// character within the token.  This handles escaped newlines.
  static SourceLocation AdvanceToTokenCharacter(SourceLocation TokStart,
                                                unsigned Character/*,
                                                const SourceManager &SM,
                                                const LangOptions &LangOpts*/);
  
  /// \brief Computes the source location just past the end of the
  /// token at this source location.
  ///
  /// This routine can be used to produce a source location that
  /// points just past the end of the token referenced by \p Loc, and
  /// is generally used when a diagnostic needs to point just after a
  /// token where it expected something different that it received. If
  /// the returned source location would not be meaningful (e.g., if
  /// it points into a macro), this routine returns an invalid
  /// source location.
  ///
  /// \param Offset an offset from the end of the token, where the source
  /// location should refer to. The default offset (0) produces a source
  /// location pointing just past the end of the token; an offset of 1 produces
  /// a source location pointing to the last character in the token, etc.
  //static SourceLocation getLocForEndOfToken(SourceLocation Loc, unsigned Offset,
                                            //const SourceManager &SM,
                                            //const LangOptions &LangOpts);


  /// \brief Compute the preamble of the given file.
  ///
  /// The preamble of a file contains the initial comments, include directives,
  /// and other preprocessor directives that occur before the code in this
  /// particular file actually begins. The preamble of the main source file is
  /// a potential prefix header.
  ///
  /// \param Buffer The memory buffer containing the file's contents.
  ///
  /// \param MaxLines If non-zero, restrict the length of the preamble
  /// to fewer than this number of lines.
  ///
  /// \returns The offset into the file where the preamble ends and the rest
  /// of the file begins along with a boolean value indicating whether 
  /// the preamble ends at the beginning of a new line.
  //static std::pair<unsigned, bool>
  //ComputePreamble(const llvm::MemoryBuffer *Buffer, const LangOptions &LangOpts,
                  //unsigned MaxLines = 0);

  //===--------------------------------------------------------------------===//
  // Internal implementation interfaces.
private:

  /// LexTokenInternal - Internal interface to lex a preprocessing token. Called
  /// by Lex.
  ///
  void LexTokenInternal(Token &Result);

  /// FormTokenWithChars - When we lex a token, we have identified a span
  /// starting at BufferPtr, going to TokEnd that forms the token.  This method
  /// takes that range and assigns it to the token as its location and size.  In
  /// addition, since tokens cannot overlap, this also updates BufferPtr to be
  /// TokEnd.
  void FormTokenWithChars(Token &Result, const char *TokEnd,
                          tok::TokenKind Kind) {
    unsigned TokLen = TokEnd-BufferPtr;
    Result.setLength(TokLen);
    Result.setLocation(getSourceLocation(BufferPtr));
    Result.setKind(Kind);
    BufferPtr = TokEnd;
  }

  //===--------------------------------------------------------------------===//
  // Lexer character reading interfaces.

  // This lexer is built on two interfaces for reading characters, both of which
  // automatically provide phase 1/2 translation.  getAndAdvanceChar is used
  // when we know that we will be reading a character from the input buffer and
  // that this character will be part of the result token. This occurs in (f.e.)
  // string processing, because we know we need to read until we find the
  // closing '"' character.
  //
  // The second interface is the combination of getCharAndSize with
  // ConsumeChar.  getCharAndSize reads a phase 1/2 translated character,
  // returning it and its size.  If the lexer decides that this character is
  // part of the current token, it calls ConsumeChar on it.  This two stage
  // approach allows us to emit diagnostics for characters (e.g. warnings about
  // trigraphs), knowing that they only are emitted if the character is
  // consumed.

  /// getAndAdvanceChar - Read a single 'character' from the specified buffer,
  /// advance over it, and return it.
  inline char getAndAdvanceChar(const char *&Ptr, Token &Tok) {
    // If this is not a trigraph and not a UCN or escaped newline, return
    // quickly.
    return *Ptr++;
  }

  /// ConsumeChar - When a character (identified by getCharAndSize) is consumed
  /// and added to a given token, check to see if there are diagnostics that
  /// need to be emitted or flags that need to be set on the token.  If so, do
  /// it.
  const char *ConsumeChar(const char *Ptr, unsigned Size, Token &Tok) {
    return Ptr+1;
  }

  /// getCharAndSize - Peek a single 'character' from the specified buffer,
  /// get its size, and return it.
  inline char getCharAndSize(const char *Ptr, unsigned &Size) {
    Size = 1;
    return *Ptr;
  }

  /// getEscapedNewLineSize - Return the size of the specified escaped newline,
  /// or 0 if it is not an escaped newline. P[-1] is known to be a "\" on entry
  /// to this function.
  static unsigned getEscapedNewLineSize(const char *P);

  /// SkipEscapedNewLines - If P points to an escaped newline (or a series of
  /// them), skip over them and return the first non-escaped-newline found,
  /// otherwise return P.
  static const char *SkipEscapedNewLines(const char *P);

  //===--------------------------------------------------------------------===//
  // Other lexer functions.

  void SkipBytes(unsigned Bytes, bool StartOfLine);

  const char *LexUDSuffix(Token &Result, const char *CurPtr);
  
  // Helper functions to lex the remainder of a token of the specific type.
  void LexIdentifier         (Token &Result, const char *CurPtr);
  void LexNumericConstant    (Token &Result, const char *CurPtr);
  void LexStringLiteral      (Token &Result, const char *CurPtr,
                              tok::TokenKind Kind);
  void LexRawStringLiteral   (Token &Result, const char *CurPtr,
                              tok::TokenKind Kind);
  void LexAngledStringLiteral(Token &Result, const char *CurPtr);
  void LexRuneLiteral        (Token &Result, const char *CurPtr,
                              tok::TokenKind Kind);
  bool LexEndOfFile          (Token &Result, const char *CurPtr);

  bool SkipWhitespace        (Token &Result, const char *CurPtr);
  bool SkipLineComment       (Token &Result, const char *CurPtr);
  bool SkipBlockComment      (Token &Result, const char *CurPtr);
  //bool SaveLineComment       (Token &Result, const char *CurPtr);
  
  bool IsStartOfConflictMarker(const char *CurPtr);
  bool HandleEndOfConflictMarker(const char *CurPtr);

  //bool isCodeCompletionPoint(const char *CurPtr) const;
  //void cutOffLexing() { BufferPtr = BufferEnd; }

  bool isHexaLiteral(const char *Start);
};


}  // end namespace gong

#endif

