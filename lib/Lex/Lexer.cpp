//===--- Lexer.cpp - Go Lexer ---------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the Lexer interface.
//
//===----------------------------------------------------------------------===//

#include "gong/Lex/Lexer.h"

#include "gong/Basic/DiagnosticIDs.h"

#include "utf/utf.h"

#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include <cstring>
using namespace gong;

//===----------------------------------------------------------------------===//
// Character information.
//===----------------------------------------------------------------------===//

enum {
  CHAR_HORZ_WS  = 0x01,  // ' ', '\t', '\f', '\v'.  Note, no '\0'
  CHAR_VERT_WS  = 0x02,  // '\r', '\n'
  CHAR_LETTER   = 0x04,  // a-z,A-Z
  CHAR_NUMBER   = 0x08,  // 0-9
  CHAR_UNDER    = 0x10,  // _
  CHAR_PERIOD   = 0x20,  // .
  CHAR_RAWDEL   = 0x40   // {}[]#<>%:;?*+-/^&|~!=,"'
};

// Statically initialize CharInfo table based on ASCII character set
// Reference: FreeBSD 7.2 /usr/share/misc/ascii
static const unsigned char CharInfo[256] =
{
// 0 NUL         1 SOH         2 STX         3 ETX
// 4 EOT         5 ENQ         6 ACK         7 BEL
   0           , 0           , 0           , 0           ,
   0           , 0           , 0           , 0           ,
// 8 BS          9 HT         10 NL         11 VT
//12 NP         13 CR         14 SO         15 SI
   0           , CHAR_HORZ_WS, CHAR_VERT_WS, CHAR_HORZ_WS,
   CHAR_HORZ_WS, CHAR_VERT_WS, 0           , 0           ,
//16 DLE        17 DC1        18 DC2        19 DC3
//20 DC4        21 NAK        22 SYN        23 ETB
   0           , 0           , 0           , 0           ,
   0           , 0           , 0           , 0           ,
//24 CAN        25 EM         26 SUB        27 ESC
//28 FS         29 GS         30 RS         31 US
   0           , 0           , 0           , 0           ,
   0           , 0           , 0           , 0           ,
//32 SP         33  !         34  "         35  #
//36  $         37  %         38  &         39  '
   CHAR_HORZ_WS, CHAR_RAWDEL , CHAR_RAWDEL , CHAR_RAWDEL ,
   0           , CHAR_RAWDEL , CHAR_RAWDEL , CHAR_RAWDEL ,
//40  (         41  )         42  *         43  +
//44  ,         45  -         46  .         47  /
   0           , 0           , CHAR_RAWDEL , CHAR_RAWDEL ,
   CHAR_RAWDEL , CHAR_RAWDEL , CHAR_PERIOD , CHAR_RAWDEL ,
//48  0         49  1         50  2         51  3
//52  4         53  5         54  6         55  7
   CHAR_NUMBER , CHAR_NUMBER , CHAR_NUMBER , CHAR_NUMBER ,
   CHAR_NUMBER , CHAR_NUMBER , CHAR_NUMBER , CHAR_NUMBER ,
//56  8         57  9         58  :         59  ;
//60  <         61  =         62  >         63  ?
   CHAR_NUMBER , CHAR_NUMBER , CHAR_RAWDEL , CHAR_RAWDEL ,
   CHAR_RAWDEL , CHAR_RAWDEL , CHAR_RAWDEL , CHAR_RAWDEL ,
//64  @         65  A         66  B         67  C
//68  D         69  E         70  F         71  G
   0           , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
//72  H         73  I         74  J         75  K
//76  L         77  M         78  N         79  O
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
//80  P         81  Q         82  R         83  S
//84  T         85  U         86  V         87  W
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
//88  X         89  Y         90  Z         91  [
//92  \         93  ]         94  ^         95  _
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_RAWDEL ,
   0           , CHAR_RAWDEL , CHAR_RAWDEL , CHAR_UNDER  ,
//96  `         97  a         98  b         99  c
//100  d       101  e        102  f        103  g
   0           , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
//104  h       105  i        106  j        107  k
//108  l       109  m        110  n        111  o
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
//112  p       113  q        114  r        115  s
//116  t       117  u        118  v        119  w
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_LETTER ,
//120  x       121  y        122  z        123  {
//124  |       125  }        126  ~        127 DEL
   CHAR_LETTER , CHAR_LETTER , CHAR_LETTER , CHAR_RAWDEL ,
   CHAR_RAWDEL , CHAR_RAWDEL , CHAR_RAWDEL , 0
};

static void InitCharacterInfo() {
  static bool isInited = false;
  if (isInited) return;
  // check the statically-initialized CharInfo table
  assert(CHAR_HORZ_WS == CharInfo[(int)' ']);
  assert(CHAR_HORZ_WS == CharInfo[(int)'\t']);
  assert(CHAR_HORZ_WS == CharInfo[(int)'\f']);
  assert(CHAR_HORZ_WS == CharInfo[(int)'\v']);
  assert(CHAR_VERT_WS == CharInfo[(int)'\n']);
  assert(CHAR_VERT_WS == CharInfo[(int)'\r']);
  assert(CHAR_UNDER   == CharInfo[(int)'_']);
  assert(CHAR_PERIOD  == CharInfo[(int)'.']);
  for (unsigned i = 'a'; i <= 'z'; ++i) {
    assert(CHAR_LETTER == CharInfo[i]);
    assert(CHAR_LETTER == CharInfo[i+'A'-'a']);
  }
  for (unsigned i = '0'; i <= '9'; ++i)
    assert(CHAR_NUMBER == CharInfo[i]);
    
  isInited = true;
}

/// isIdentifierBody - Return true if this is the body character of an
/// identifier, which is [a-zA-Z0-9_].
static inline bool isIdentifierBody(unsigned char c) {
  return (CharInfo[c] & (CHAR_LETTER|CHAR_NUMBER|CHAR_UNDER)) ? true : false;
}

/// isHorizontalWhitespace - Return true if this character is horizontal
/// whitespace: ' ', '\\t', '\\f', '\\v'.  Note that this returns false for
/// '\\0'.
static inline bool isHorizontalWhitespace(unsigned char c) {
  return (CharInfo[c] & CHAR_HORZ_WS) ? true : false;
}

/// isWhitespace - Return true if this character is horizontal or vertical
/// whitespace: ' ', '\\t', '\\f', '\\v', '\\n', '\\r'.  Note that this returns
/// false for '\\0'.
static inline bool isWhitespace(unsigned char c) {
  return (CharInfo[c] & (CHAR_HORZ_WS|CHAR_VERT_WS)) ? true : false;
}

/// isNumberBody - Return true if this is the body character of an
/// preprocessing number, which is [a-zA-Z0-9_.].
static inline bool isNumberBody(unsigned char c) {
  return (CharInfo[c] & (CHAR_LETTER|CHAR_NUMBER)) ?  true : false;
}


//===----------------------------------------------------------------------===//
// Lexer Class Implementation
//===----------------------------------------------------------------------===//

void Lexer::InitLexer(const char *BufStart, const char *BufPtr,
                      const char *BufEnd) {
  InitCharacterInfo();

  BufferStart = BufStart;
  BufferPtr = BufPtr;
  BufferEnd = BufEnd;

  assert(BufEnd[0] == 0 &&
         "We assume that the input buffer has a null character at the end"
         " to simplify lexing!");

  // Check whether we have a BOM in the beginning of the buffer. If yes - act
  // accordingly. Right now we support only UTF-8 with and without BOM, so, just
  // skip the UTF-8 BOM if it's present.
  if (BufferStart == BufferPtr) {
    // Determine the size of the BOM.
    StringRef Buf(BufferStart, BufferEnd - BufferStart);
    size_t BOMLength = llvm::StringSwitch<size_t>(Buf)
      .StartsWith("\xEF\xBB\xBF", 3) // UTF-8 BOM
      .Default(0);

    // Skip the BOM.
    BufferPtr += BOMLength;
  }

  CurrentConflictMarkerState = CMK_None;

  // Start of the file is a start of line.
  IsAtStartOfLine = true;

  LastTokenKind = tok::eof;
}

/// Lexer constructor - Create a new lexer object for the specified buffer
/// with the specified preprocessor managing the lexing process.  This lexer
/// assumes that the associated file buffer and Preprocessor objects will
/// outlive it, so it doesn't take ownership of either of them.
Lexer::Lexer(DiagnosticsEngine &Diags, llvm::SourceMgr& SM,
             const llvm::MemoryBuffer *InputFile)
    : Diags(Diags), SM(SM) {
  InitLexer(InputFile->getBufferStart(), InputFile->getBufferStart(),
            InputFile->getBufferEnd());

  // Default to keeping comments if the preprocessor wants them.
  //SetCommentRetentionState(PP.getCommentRetentionState());
}

/// CurPtr points to the end of this file.  Handle this
/// condition, reporting diagnostics and handling other edge cases as required.
/// This returns true if Result contains a token, false if PP.Lex should be
/// called again.
bool Lexer::LexEndOfFile(Token &Result, const char *CurPtr) {
  Result.startToken();
  BufferPtr = BufferEnd;
  FormTokenWithChars(Result, BufferEnd, tok::eof);
  return true;
}

void Lexer::Diag(const char *Loc, unsigned DiagID) const {
  Diags.Report(llvm::SMLoc::getFromPointer(Loc), DiagID);
}

/// getSourceLocation - Return a source location identifier for the specified
/// offset in the current file.
SourceLocation Lexer::getSourceLocation(const char *Loc) const {
  assert(Loc >= BufferStart && Loc <= BufferEnd &&
         "Location out of range for this buffer!");
  return SourceLocation::getFromPointer(Loc);
}

StringRef Lexer::getSpelling(const Token &Tok) const {
  SourceLocation Loc = Tok.getLocation();
  return StringRef(Loc.getPointer(), Tok.getLength());
}

void Lexer::DumpToken(const Token &Tok, bool DumpFlags) const {
  llvm::errs() << tok::getTokenName(Tok.getKind()) << " '"
               << getSpelling(Tok) << "'";

  if (!DumpFlags) return;

  llvm::errs() << "\t";
  if (Tok.isAtStartOfLine())
    llvm::errs() << " [StartOfLine]";
  if (Tok.hasLeadingSpace())
    llvm::errs() << " [LeadingSpace]";
  if (Tok.isInsertedSemi())
    llvm::errs() << " [InsertedSemi]";

  int BufID = SM.FindBufferContainingLoc(Tok.getLocation());
  std::pair<unsigned, unsigned> Pos = SM.getLineAndColumn(Tok.getLocation());
  llvm::errs() << "\tLoc=<" <<
    SM.getMemoryBuffer(BufID)->getBufferIdentifier()
    << ":" << Pos.first << ":" << Pos.second << ">";
}


// SkipLineComment - We have just read the // characters from input.  Skip until
// we find the newline character thats terminate the comment.  Then update
/// BufferPtr and return.
///
/// If we're in KeepCommentMode or any CommentHandler has inserted
/// some tokens, this will store the first token and return true.
bool Lexer::SkipLineComment(Token &Result, const char *CurPtr) {
  // Scan over the body of the comment.  The common case, when scanning, is that
  // the comment contains normal ascii characters with nothing interesting in
  // them.  As such, optimize for this case with the inner loop.
  char C;
  do {
    C = *CurPtr;
    // Skip over characters in the fast loop.
    while (C != 0 &&                // Potentially EOF.
           C != '\n' && C != '\r')  // Newline or DOS-style newline.
      C = *++CurPtr;

    const char *NextLine = CurPtr;
    if (C != 0) {
      // We found a newline, see if it's escaped.
      const char *EscapePtr = CurPtr-1;
      while (isHorizontalWhitespace(*EscapePtr)) // Skip whitespace.
        --EscapePtr;

      if (*EscapePtr == '\\') // Escaped newline.
        CurPtr = EscapePtr;
      else if (EscapePtr[0] == '/' && EscapePtr[-1] == '?' &&
               EscapePtr[-2] == '?') // Trigraph-escaped newline.
        CurPtr = EscapePtr-2;
      else
        break; // This is a newline, we're done.
    }

    // Otherwise, this is a hard case.  Fall back on getAndAdvanceChar to
    // properly decode the character.  Read it in raw mode to avoid emitting
    // diagnostics about things like trigraphs.  If we see an escaped newline,
    // we'll handle it below.
    const char *OldPtr = CurPtr;
    C = getAndAdvanceChar(CurPtr, Result);

    // If we only read only one character, then no special handling is needed.
    // We're done and can skip forward to the newline.
    if (C != 0 && CurPtr == OldPtr+1) {
      CurPtr = NextLine;
      break;
    }

    if (CurPtr == BufferEnd+1) { 
      --CurPtr; 
      break; 
    }
  } while (C != '\n' && C != '\r');

  handleComment(Result, SourceRange(getSourceLocation(BufferPtr),
                                    getSourceLocation(CurPtr)));

  // If we are inside a line comment and we see the end of line,
  // return immediately, so that the lexer can return this as an EOD token.
  if (CurPtr == BufferEnd) {
    BufferPtr = CurPtr;
    return false;
  }

  // Otherwise, eat the \n character.  We don't care if this is a \n\r or
  // \r\n sequence.  This is an efficiency hack (because we know the \n can't
  // contribute to another token), it isn't needed for correctness.  Note that
  // this is ok even in KeepWhitespaceMode, because we would have returned the
  /// comment above in that mode.
  ++CurPtr;

  // The next returned token is at the start of the line.
  if (InsertSemi(Result, CurPtr))
    return true;
  Result.setFlag(Token::StartOfLine);
  // No leading whitespace seen so far.
  Result.clearFlag(Token::LeadingSpace);
  BufferPtr = CurPtr;
  return false;
}

/// SkipWhitespace - Efficiently skip over a series of whitespace characters.
/// Update BufferPtr to point to the next non-whitespace character and return.
///
/// This method forms a token and returns true if KeepWhitespaceMode is enabled.
///
bool Lexer::SkipWhitespace(Token &Result, const char *CurPtr) {
  // Whitespace - Skip it, then return the token after the whitespace.
  unsigned char Char = *CurPtr;  // Skip consequtive spaces efficiently.
  while (1) {
    // Skip horizontal whitespace very aggressively.
    while (isHorizontalWhitespace(Char))
      Char = *++CurPtr;

    // Otherwise if we have something other than whitespace, we're done.
    if (Char != '\n' && Char != '\r')
      break;

    // ok, but handle newline.
    if (InsertSemi(Result, CurPtr))
      return true;
    // The returned token is at the start of the line.
    Result.setFlag(Token::StartOfLine);
    // No leading whitespace seen so far.
    Result.clearFlag(Token::LeadingSpace);
    Char = *++CurPtr;
  }

  // If this isn't immediately after a newline, there is leading space.
  char PrevChar = CurPtr[-1];
  if (PrevChar != '\n' && PrevChar != '\r')
    Result.setFlag(Token::LeadingSpace);

  // If the client wants us to return whitespace, return it now.
  //if (isKeepWhitespaceMode()) {
    //FormTokenWithChars(Result, CurPtr, tok::unknown);
    //return true;
  //}

  BufferPtr = CurPtr;
  return false;
}

void Lexer::LexUtfIdentifier(Token &Result, const char *CurPtr) {
  Rune C;
  int len = chartorune(&C, CurPtr);
  CurPtr += len;

  while (isalpharune(C) || isdigitrune(C) || C == '_') {
    len = chartorune(&C, CurPtr);
    CurPtr += len;
  }

  if (C == Runeerror && len == 1)
    Diag(CurPtr, diag::invalid_utf_sequence);
  else
    CurPtr -= len;   // Back up over the skipped character.


  const char *IdStart = BufferPtr;

  FormTokenWithChars(Result, CurPtr, tok::identifier);

  // Update the token info (identifier info and appropriate token kind).
  IdentifierInfo *II = &Identifiers.get(StringRef(IdStart, Result.getLength()));
  Result.setIdentifierInfo(II);
  Result.setKind(II->getTokenID());
}

void Lexer::LexIdentifier(Token &Result, const char *CurPtr) {
  // Match [_A-Za-z0-9]*, we have already matched [_A-Za-z$]
  unsigned char C = *CurPtr++;
  while (isIdentifierBody(C))
    C = *CurPtr++;

  --CurPtr;   // Back up over the skipped character.

  if (C >= 128)
    return LexUtfIdentifier(Result, CurPtr);

  const char *IdStart = BufferPtr;

  FormTokenWithChars(Result, CurPtr, tok::identifier);

  // Update the token info (identifier info and appropriate token kind).
  IdentifierInfo *II = &Identifiers.get(StringRef(IdStart, Result.getLength()));
  Result.setIdentifierInfo(II);
  Result.setKind(II->getTokenID());
}

/// LexRuneLiteral - Lex the remainder of a character constant, after having
/// lexed '.
void Lexer::LexRuneLiteral(Token &Result, const char *CurPtr,
                           tok::TokenKind Kind) {
  const char *NulCharacter = 0; // Does this character contain the \0 character?

  char C = getAndAdvanceChar(CurPtr, Result);
  if (C == '\'') {
    Diag(BufferPtr, diag::empty_rune);
    FormTokenWithChars(Result, CurPtr, tok::unknown);
    return;
  }

  while (C != '\'') {
    // Skip escaped characters.
    if (C == '\\')
      C = getAndAdvanceChar(CurPtr, Result);

    if (C == '\n' || C == '\r' ||             // Newline.
        (C == 0 && CurPtr-1 == BufferEnd)) {  // End of file.
      Diag(BufferPtr, diag::unterminated_rune);
      FormTokenWithChars(Result, CurPtr-1, tok::unknown);
      return;
    }

    if (C == 0) {
      NulCharacter = CurPtr-1;
    }
    C = getAndAdvanceChar(CurPtr, Result);
  }

  // If a nul character existed in the character, warn about it.
  if (NulCharacter)
    Diag(NulCharacter, diag::null_in_rune);

  // Update the location of token as well as BufferPtr.
  const char *TokStart = BufferPtr;
  FormTokenWithChars(Result, CurPtr, Kind);
  Result.setLiteralData(TokStart);
}

/// LexStringLiteral - Lex the remainder of a string literal, after having lexed
/// a ".
void Lexer::LexStringLiteral(Token &Result, const char *CurPtr,
                             tok::TokenKind Kind) {
  const char *NulCharacter = 0; // Does this string contain the \0 character?

  char C = getAndAdvanceChar(CurPtr, Result);
  while (C != '"') {
    // Skip escaped characters.
    if (C == '\\')
      C = getAndAdvanceChar(CurPtr, Result);
    
    if (C == '\n' || C == '\r' ||             // Newline.
        (C == 0 && CurPtr-1 == BufferEnd)) {  // End of file.
      Diag(BufferPtr, diag::unterminated_string);
      FormTokenWithChars(Result, CurPtr-1, tok::unknown);
      return;
    }
    
    if (C == 0) {
      NulCharacter = CurPtr-1;
    }
    C = getAndAdvanceChar(CurPtr, Result);
  }

  // If a nul character existed in the string, warn about it.
  if (NulCharacter)
    Diag(NulCharacter, diag::null_in_string);

  // Update the location of the token as well as the BufferPtr instance var.
  const char *TokStart = BufferPtr;
  FormTokenWithChars(Result, CurPtr, Kind);
  Result.setLiteralData(TokStart);
}

/// LexStringLiteral - Lex the remainder of a raw string literal, after having
/// lexed a `.
void Lexer::LexRawStringLiteral(Token &Result, const char *CurPtr,
                                tok::TokenKind Kind) {
  const char *NulCharacter = 0; // Does this string contain the \0 character?

  char C = getAndAdvanceChar(CurPtr, Result);
  while (C != '`') {
    if (C == 0 && CurPtr-1 == BufferEnd) {  // End of file.
      Diag(BufferPtr, diag::unterminated_raw_string);
      FormTokenWithChars(Result, CurPtr-1, tok::unknown);
      return;
    }
    
    if (C == 0) {
      NulCharacter = CurPtr-1;
    }
    C = getAndAdvanceChar(CurPtr, Result);
  }

  // If a nul character existed in the string, warn about it.
  if (NulCharacter)
    Diag(NulCharacter, diag::null_in_string);

  // Update the location of the token as well as the BufferPtr instance var.
  const char *TokStart = BufferPtr;
  FormTokenWithChars(Result, CurPtr, Kind);
  Result.setLiteralData(TokStart);
}

#ifdef __SSE2__
#include <emmintrin.h>
#elif __ALTIVEC__
#include <altivec.h>
#undef bool
#endif

/// We have just read from input the / and * characters that started a comment.
/// Read until we find the * and / characters that terminate the comment.
///
/// If we're in KeepCommentMode or any CommentHandler has inserted
/// some tokens, this will store the first token and return true.
bool Lexer::SkipBlockComment(Token &Result, const char *CurPtr) {
  // Scan one character past where we should, looking for a '/' character.  Once
  // we find it, check to see if it was preceded by a *.  This common
  // optimization helps people who like to put a lot of * characters in their
  // comments.

  const char* CommentStart = CurPtr;

  // The first character we get with newlines and trigraphs skipped to handle
  // the degenerate /*/ case below correctly if the * has an escaped newline
  // after it.
  unsigned CharSize;
  unsigned char C = getCharAndSize(CurPtr, CharSize);
  CurPtr += CharSize;
  if (C == 0 && CurPtr == BufferEnd+1) {
    Diag(BufferPtr, diag::unterminated_block_comment);
    --CurPtr;

    BufferPtr = CurPtr;
    return false;
  }

  // Check to see if the first character after the '/*' is another /.  If so,
  // then this slash does not end the block comment, it is part of it.
  if (C == '/')
    C = *CurPtr++;

  while (1) {
    // Skip over all non-interesting characters until we find end of buffer or a
    // (probably ending) '/' character.
    if (CurPtr + 24 < BufferEnd) {
      // While not aligned to a 16-byte boundary.
      while (C != '/' && ((intptr_t)CurPtr & 0x0F) != 0)
        C = *CurPtr++;

      if (C == '/') goto FoundSlash;

#ifdef __SSE2__
      __m128i Slashes = _mm_set1_epi8('/');
      while (CurPtr+16 <= BufferEnd) {
        int cmp = _mm_movemask_epi8(_mm_cmpeq_epi8(*(const __m128i*)CurPtr,
                                    Slashes));
        if (cmp != 0) {
          // Adjust the pointer to point directly after the first slash. It's
          // not necessary to set C here, it will be overwritten at the end of
          // the outer loop.
          CurPtr += llvm::CountTrailingZeros_32(cmp) + 1;
          goto FoundSlash;
        }
        CurPtr += 16;
      }
#elif __ALTIVEC__
      __vector unsigned char Slashes = {
        '/', '/', '/', '/',  '/', '/', '/', '/',
        '/', '/', '/', '/',  '/', '/', '/', '/'
      };
      while (CurPtr+16 <= BufferEnd &&
             !vec_any_eq(*(vector unsigned char*)CurPtr, Slashes))
        CurPtr += 16;
#else
      // Scan for '/' quickly.  Many block comments are very large.
      while (CurPtr[0] != '/' &&
             CurPtr[1] != '/' &&
             CurPtr[2] != '/' &&
             CurPtr[3] != '/' &&
             CurPtr+4 < BufferEnd) {
        CurPtr += 4;
      }
#endif

      // It has to be one of the bytes scanned, increment to it and read one.
      C = *CurPtr++;
    }

    // Loop to scan the remainder.
    while (C != '/' && C != '\0')
      C = *CurPtr++;

    if (C == '/') {
  FoundSlash:
      if (CurPtr[-2] == '*')  // We found the final */.  We're done!
        break;

      if (CurPtr[0] == '*' && CurPtr[1] != '/') {
        // If this is a /* inside of the comment, emit a warning.  Don't do this
        // if this is a /*/, which will end the comment.
        Diag(CurPtr-1, diag::nested_block_comment);
      }
    } else if (C == 0 && CurPtr == BufferEnd+1) {
      Diag(BufferPtr, diag::unterminated_block_comment);
      // Note: the user probably forgot a */.  We could continue immediately
      // after the /*, but this would involve lexing a lot of what really is the
      // comment, which surely would confuse the parser.
      --CurPtr;

      BufferPtr = CurPtr;
      return false;
    }

    C = *CurPtr++;
  }

  handleComment(Result, SourceRange(getSourceLocation(BufferPtr),
                                    getSourceLocation(CurPtr)));

  // Check if there was a newline in the block comment.  Most lines are short,
  // so this should be fast.
  if (memchr(CommentStart, '\n', CurPtr - CommentStart))
    if (InsertSemi(Result, CurPtr))
      return true;

  // It is common for the tokens immediately after a /**/ comment to be
  // whitespace.  Instead of going through the big switch, handle it
  // efficiently now.  This is safe even in KeepWhitespaceMode because we would
  // have already returned above with the comment as a token.
  if (isHorizontalWhitespace(*CurPtr)) {
    Result.setFlag(Token::LeadingSpace);
    SkipWhitespace(Result, CurPtr+1);
    return false;
  }

  // Otherwise, just return so that the next character will be lexed as a token.
  BufferPtr = CurPtr;
  Result.setFlag(Token::LeadingSpace);
  return false;
}

/// isHexaLiteral - Return true if Start points to a hex constant.
/// in microsoft mode (where this is supposed to be several different tokens).
bool Lexer::isHexaLiteral(const char *Start) {
  unsigned Size;
  char C1 = getCharAndSize(Start, Size);
  if (C1 != '0')
    return false;
  char C2 = getCharAndSize(Start + Size, Size);
  return (C2 == 'x' || C2 == 'X');
}

/// LexNumericConstant - Lex the remainder of a integer or floating point
/// constant. From[-1] is the first character lexed.  Return the end of the
/// constant.
void Lexer::LexNumericConstant(Token &Result, const char *CurPtr) {
  unsigned Size;
  char C = getCharAndSize(CurPtr, Size);
  char PrevCh = 0;
  while (isNumberBody(C)) {
    CurPtr = ConsumeChar(CurPtr, Size, Result);
    PrevCh = C;
    C = getCharAndSize(CurPtr, Size);
  }

  if (C == '.') {
    // 0x1.2 is two tokens in Go.
    if (!isHexaLiteral(BufferPtr))
      return LexNumericConstant(Result, ConsumeChar(CurPtr, Size, Result));
  } else if ((C == '-' || C == '+') && (PrevCh == 'E' || PrevCh == 'e')) {
    // If we fell out, check for a sign, due to 1e+12. If we have one, continue.
    if (!isHexaLiteral(BufferPtr))
      return LexNumericConstant(Result, ConsumeChar(CurPtr, Size, Result));
  }

  // Update the location of token as well as BufferPtr.
  const char *TokStart = BufferPtr;
  FormTokenWithChars(Result, CurPtr, tok::numeric_literal);
  Result.setLiteralData(TokStart);
}


#if 0
/// Stringify - Convert the specified string into a C string, with surrounding
/// ""'s, and with escaped \ and " characters.
std::string Lexer::Stringify(const std::string &Str, bool Charify) {
  std::string Result = Str;
  char Quote = Charify ? '\'' : '"';
  for (unsigned i = 0, e = Result.size(); i != e; ++i) {
    if (Result[i] == '\\' || Result[i] == Quote) {
      Result.insert(Result.begin()+i, '\\');
      ++i; ++e;
    }
  }
  return Result;
}

/// Stringify - Convert the specified string into a C string by escaping '\'
/// and " characters.  This does not add surrounding ""'s to the string.
void Lexer::Stringify(SmallVectorImpl<char> &Str) {
  for (unsigned i = 0, e = Str.size(); i != e; ++i) {
    if (Str[i] == '\\' || Str[i] == '"') {
      Str.insert(Str.begin()+i, '\\');
      ++i; ++e;
    }
  }
}

//===----------------------------------------------------------------------===//
// Token Spelling
//===----------------------------------------------------------------------===//

static bool isWhitespace(unsigned char c);

/// MeasureTokenLength - Relex the token at the specified location and return
/// its length in bytes in the input file.  If the token needs cleaning (e.g.
/// includes a trigraph or an escaped newline) then this count includes bytes
/// that are part of that.
unsigned Lexer::MeasureTokenLength(SourceLocation Loc,
                                   const SourceManager &SM,
                                   const LangOptions &LangOpts) {
  // TODO: this could be special cased for common tokens like identifiers, ')',
  // etc to make this faster, if it mattered.  Just look at StrData[0] to handle
  // all obviously single-char tokens.  This could use
  // Lexer::isObviouslySimpleCharacter for example to handle identifiers or
  // something.

  // If this comes from a macro expansion, we really do want the macro name, not
  // the token this macro expanded to.
  Loc = SM.getExpansionLoc(Loc);
  std::pair<FileID, unsigned> LocInfo = SM.getDecomposedLoc(Loc);
  bool Invalid = false;
  StringRef Buffer = SM.getBufferData(LocInfo.first, &Invalid);
  if (Invalid)
    return 0;

  const char *StrData = Buffer.data()+LocInfo.second;

  if (isWhitespace(StrData[0]))
    return 0;

  // Create a lexer starting at the beginning of this token.
  Lexer TheLexer(SM.getLocForStartOfFile(LocInfo.first), LangOpts,
                 Buffer.begin(), StrData, Buffer.end());
  TheLexer.SetCommentRetentionState(true);
  Token TheTok;
  TheLexer.LexFromRawLexer(TheTok);
  return TheTok.getLength();
}

static SourceLocation getBeginningOfFileToken(SourceLocation Loc,
                                              const SourceManager &SM,
                                              const LangOptions &LangOpts) {
  assert(Loc.isFileID());
  std::pair<FileID, unsigned> LocInfo = SM.getDecomposedLoc(Loc);
  if (LocInfo.first.isInvalid())
    return Loc;
  
  bool Invalid = false;
  StringRef Buffer = SM.getBufferData(LocInfo.first, &Invalid);
  if (Invalid)
    return Loc;

  // Back up from the current location until we hit the beginning of a line
  // (or the buffer). We'll relex from that point.
  const char *BufStart = Buffer.data();
  if (LocInfo.second >= Buffer.size())
    return Loc;
  
  const char *StrData = BufStart+LocInfo.second;
  if (StrData[0] == '\n' || StrData[0] == '\r')
    return Loc;

  const char *LexStart = StrData;
  while (LexStart != BufStart) {
    if (LexStart[0] == '\n' || LexStart[0] == '\r') {
      ++LexStart;
      break;
    }

    --LexStart;
  }
  
  // Create a lexer starting at the beginning of this token.
  SourceLocation LexerStartLoc = Loc.getLocWithOffset(-LocInfo.second);
  Lexer TheLexer(LexerStartLoc, LangOpts, BufStart, LexStart, Buffer.end());
  TheLexer.SetCommentRetentionState(true);
  
  // Lex tokens until we find the token that contains the source location.
  Token TheTok;
  do {
    TheLexer.LexFromRawLexer(TheTok);
    
    if (TheLexer.getBufferLocation() > StrData) {
      // Lexing this token has taken the lexer past the source location we're
      // looking for. If the current token encompasses our source location,
      // return the beginning of that token.
      if (TheLexer.getBufferLocation() - TheTok.getLength() <= StrData)
        return TheTok.getLocation();
      
      // We ended up skipping over the source location entirely, which means
      // that it points into whitespace. We're done here.
      break;
    }
  } while (TheTok.getKind() != tok::eof);
  
  // We've passed our source location; just return the original source location.
  return Loc;
}

SourceLocation Lexer::GetBeginningOfToken(SourceLocation Loc,
                                          const SourceManager &SM,
                                          const LangOptions &LangOpts) {
 if (Loc.isFileID())
   return getBeginningOfFileToken(Loc, SM, LangOpts);
 
 if (!SM.isMacroArgExpansion(Loc))
   return Loc;

 SourceLocation FileLoc = SM.getSpellingLoc(Loc);
 SourceLocation BeginFileLoc = getBeginningOfFileToken(FileLoc, SM, LangOpts);
 std::pair<FileID, unsigned> FileLocInfo = SM.getDecomposedLoc(FileLoc);
 std::pair<FileID, unsigned> BeginFileLocInfo
   = SM.getDecomposedLoc(BeginFileLoc);
 assert(FileLocInfo.first == BeginFileLocInfo.first &&
        FileLocInfo.second >= BeginFileLocInfo.second);
 return Loc.getLocWithOffset(BeginFileLocInfo.second - FileLocInfo.second);
}

/// AdvanceToTokenCharacter - Given a location that specifies the start of a
/// token, return a new location that specifies a character within the token.
SourceLocation Lexer::AdvanceToTokenCharacter(SourceLocation TokStart,
                                              unsigned CharNo,
                                              const SourceManager &SM,
                                              const LangOptions &LangOpts) {
  // Figure out how many physical characters away the specified expansion
  // character is.  This needs to take into consideration newlines and
  // trigraphs.
  bool Invalid = false;
  const char *TokPtr = SM.getCharacterData(TokStart, &Invalid);
  
  // If they request the first char of the token, we're trivially done.
  if (Invalid || (CharNo == 0))
    return TokStart;
  
  unsigned PhysOffset = 0;
  
  // The usual case is that tokens don't contain anything interesting.  Skip
  // over the uninteresting characters.  If a token only consists of simple
  // chars, this method is extremely fast.
  while (true) {
    if (CharNo == 0)
      return TokStart.getLocWithOffset(PhysOffset);
    ++TokPtr, --CharNo, ++PhysOffset;
  }
  
  // If we have a character that may be a trigraph or escaped newline, use a
  // lexer to parse it correctly.
  for (; CharNo; --CharNo) {
    unsigned Size;
    Lexer::getCharAndSize(TokPtr, Size, LangOpts);
    TokPtr += Size;
    PhysOffset += Size;
  }
  
  return TokStart.getLocWithOffset(PhysOffset);
}

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
SourceLocation Lexer::getLocForEndOfToken(SourceLocation Loc, unsigned Offset,
                                          const SourceManager &SM,
                                          const LangOptions &LangOpts) {
  if (Loc.isInvalid())
    return SourceLocation();

  if (Loc.isMacroID()) {
    if (Offset > 0 || !isAtEndOfMacroExpansion(Loc, SM, LangOpts, &Loc))
      return SourceLocation(); // Points inside the macro expansion.
  }

  unsigned Len = Lexer::MeasureTokenLength(Loc, SM, LangOpts);
  if (Len > Offset)
    Len = Len - Offset;
  else
    return Loc;
  
  return Loc.getLocWithOffset(Len);
}


//===----------------------------------------------------------------------===//
// Diagnostics forwarding code.
//===----------------------------------------------------------------------===//

/// GetMappedTokenLoc - If lexing out of a 'mapped buffer', where we pretend the
/// lexer buffer was all expanded at a single point, perform the mapping.
/// This is currently only used for _Pragma implementation, so it is the slow
/// path of the hot getSourceLocation method.  Do not allow it to be inlined.
static LLVM_ATTRIBUTE_NOINLINE SourceLocation GetMappedTokenLoc(
    Preprocessor &PP, SourceLocation FileLoc, unsigned CharNo, unsigned TokLen);
static SourceLocation GetMappedTokenLoc(Preprocessor &PP,
                                        SourceLocation FileLoc,
                                        unsigned CharNo, unsigned TokLen) {
  assert(FileLoc.isMacroID() && "Must be a macro expansion");

  // Otherwise, we're lexing "mapped tokens".  This is used for things like
  // _Pragma handling.  Combine the expansion location of FileLoc with the
  // spelling location.
  SourceManager &SM = PP.getSourceManager();

  // Create a new SLoc which is expanded from Expansion(FileLoc) but whose
  // characters come from spelling(FileLoc)+Offset.
  SourceLocation SpellingLoc = SM.getSpellingLoc(FileLoc);
  SpellingLoc = SpellingLoc.getLocWithOffset(CharNo);

  // Figure out the expansion loc range, which is the range covered by the
  // original _Pragma(...) sequence.
  std::pair<SourceLocation,SourceLocation> II =
    SM.getImmediateExpansionRange(FileLoc);

  return SM.createExpansionLoc(SpellingLoc, II.first, II.second, TokLen);
}

//===----------------------------------------------------------------------===//
// Helper methods for lexing.
//===----------------------------------------------------------------------===//


/// SaveLineComment - If in save-comment mode, package up this Line comment in
/// an appropriate way and return it.
bool Lexer::SaveLineComment(Token &Result, const char *CurPtr) {
  // If we're not in a preprocessor directive, just return the // comment
  // directly.
  FormTokenWithChars(Result, CurPtr, tok::comment);

  if (!ParsingPreprocessorDirective || LexingRawMode)
    return true;

  // If this Line-style comment is in a macro definition, transmogrify it into
  // a C-style block comment.
  bool Invalid = false;
  std::string Spelling = PP->getSpelling(Result, &Invalid);
  if (Invalid)
    return true;
  
  assert(Spelling[0] == '/' && Spelling[1] == '/' && "Not bcpl comment?");
  Spelling[1] = '*';   // Change prefix to "/*".
  Spelling += "*/";    // add suffix.

  Result.setKind(tok::comment);
  PP->CreateString(Spelling, Result,
                   Result.getLocation(), Result.getLocation());
  return true;
}

//===----------------------------------------------------------------------===//
// Primary Lexing Entry Points
//===----------------------------------------------------------------------===//

bool Lexer::isCodeCompletionPoint(const char *CurPtr) const {
  if (PP && PP->isCodeCompletionEnabled()) {
    SourceLocation Loc = FileLoc.getLocWithOffset(CurPtr-BufferStart);
    return Loc == PP->getCodeCompletionLoc();
  }

  return false;
}
#endif

/// \brief Find the end of a version control conflict marker.
static const char *FindConflictEnd(const char *CurPtr, const char *BufferEnd,
                                   ConflictMarkerKind CMK) {
  const char *Terminator = CMK == CMK_Perforce ? "<<<<\n" : ">>>>>>>";
  size_t TermLen = CMK == CMK_Perforce ? 5 : 7;
  StringRef RestOfBuffer(CurPtr+TermLen, BufferEnd-CurPtr-TermLen);
  size_t Pos = RestOfBuffer.find(Terminator);
  while (Pos != StringRef::npos) {
    // Must occur at start of line.
    if (RestOfBuffer[Pos-1] != '\r' &&
        RestOfBuffer[Pos-1] != '\n') {
      RestOfBuffer = RestOfBuffer.substr(Pos+TermLen);
      Pos = RestOfBuffer.find(Terminator);
      continue;
    }
    return RestOfBuffer.data()+Pos;
  }
  return 0;
}

/// IsStartOfConflictMarker - If the specified pointer is the start of a version
/// control conflict marker like '<<<<<<<', recognize it as such, emit an error
/// and recover nicely.  This returns true if it is a conflict marker and false
/// if not.
bool Lexer::IsStartOfConflictMarker(const char *CurPtr) {
  // Only a conflict marker if it starts at the beginning of a line.
  if (CurPtr != BufferStart &&
      CurPtr[-1] != '\n' && CurPtr[-1] != '\r')
    return false;
  
  // Check to see if we have <<<<<<< or >>>>.
  if ((BufferEnd-CurPtr < 8 || StringRef(CurPtr, 7) != "<<<<<<<") &&
      (BufferEnd-CurPtr < 6 || StringRef(CurPtr, 5) != ">>>> "))
    return false;

  // If we have a situation where we don't care about conflict markers, ignore
  // it.
  if (CurrentConflictMarkerState)
    return false;
  
  ConflictMarkerKind Kind = *CurPtr == '<' ? CMK_Normal : CMK_Perforce;

  // Check to see if there is an ending marker somewhere in the buffer at the
  // start of a line to terminate this conflict marker.
  if (FindConflictEnd(CurPtr, BufferEnd, Kind)) {
    // We found a match.  We are really in a conflict marker.
    // Diagnose this, and ignore to the end of line.
    Diag(CurPtr, diag::conflict_marker);
    CurrentConflictMarkerState = Kind;
    
    // Skip ahead to the end of line.  We know this exists because the
    // end-of-conflict marker starts with \r or \n.
    while (*CurPtr != '\r' && *CurPtr != '\n') {
      assert(CurPtr != BufferEnd && "Didn't find end of line");
      ++CurPtr;
    }
    BufferPtr = CurPtr;
    return true;
  }
  
  // No end of conflict marker found.
  return false;
}


/// HandleEndOfConflictMarker - If this is a '====' or '||||' or '>>>>', or if
/// it is '<<<<' and the conflict marker started with a '>>>>' marker, then it
/// is the end of a conflict marker.  Handle it by ignoring up until the end of
/// the line.  This returns true if it is a conflict marker and false if not.
bool Lexer::HandleEndOfConflictMarker(const char *CurPtr) {
  // Only a conflict marker if it starts at the beginning of a line.
  if (CurPtr != BufferStart &&
      CurPtr[-1] != '\n' && CurPtr[-1] != '\r')
    return false;
  
  // If we have a situation where we don't care about conflict markers, ignore
  // it.
  if (!CurrentConflictMarkerState)
    return false;
  
  // Check to see if we have the marker (4 characters in a row).
  for (unsigned i = 1; i != 4; ++i)
    if (CurPtr[i] != CurPtr[0])
      return false;
  
  // If we do have it, search for the end of the conflict marker.  This could
  // fail if it got skipped with a '#if 0' or something.  Note that CurPtr might
  // be the end of conflict marker.
  if (const char *End = FindConflictEnd(CurPtr, BufferEnd,
                                        CurrentConflictMarkerState)) {
    CurPtr = End;
    
    // Skip ahead to the end of line.
    while (CurPtr != BufferEnd && *CurPtr != '\r' && *CurPtr != '\n')
      ++CurPtr;
    
    BufferPtr = CurPtr;
    
    // No longer in the conflict marker.
    CurrentConflictMarkerState = CMK_None;
    return true;
  }
  
  return false;
}

/// This implements a simple Go lexer.  It is an extremely performance
/// critical piece of code.  This assumes that the buffer has a null
/// character at the end of the file.  It assumes that the Flags of result
/// have been cleared before calling this.
void Lexer::LexTokenInternal(Token &Result) {
LexNextToken:
  Result.setIdentifierInfo(0);

  // CurPtr - Cache BufferPtr in an automatic variable.
  const char *CurPtr = BufferPtr;

  // Small amounts of horizontal whitespace is very common between tokens.
  if ((*CurPtr == ' ') || (*CurPtr == '\t')) {
    ++CurPtr;
    while ((*CurPtr == ' ') || (*CurPtr == '\t'))
      ++CurPtr;

    BufferPtr = CurPtr;
    Result.setFlag(Token::LeadingSpace);
  }

  unsigned SizeTmp, SizeTmp2;   // Temporaries for use in cases below.

  // Read a character, advancing over it.
  unsigned char Char = getAndAdvanceChar(CurPtr, Result);
  tok::TokenKind Kind;

  switch (Char) {
  case 0:  // Null.
    // Found end of file?
    if (CurPtr-1 == BufferEnd) {
      // Read the PP instance variable into an automatic variable, because
      // LexEndOfFile will often delete 'this'.
      if (LexEndOfFile(Result, CurPtr-1))  // Retreat back into the file.
        return;   // Got a token to return.
      assert(false);
    }

    Diag(CurPtr-1, diag::null_in_file);
    Result.setFlag(Token::LeadingSpace);
    if (SkipWhitespace(Result, CurPtr))
      return; // KeepWhitespaceMode

    goto LexNextToken;   // GCC isn't tail call eliminating.
      
  case '\n':
  case '\r':
    // The returned token is at the start of the line.
    if (InsertSemi(Result, BufferPtr))
      return;
    Result.setFlag(Token::StartOfLine);
    // No leading whitespace seen so far.
    Result.clearFlag(Token::LeadingSpace);

    if (SkipWhitespace(Result, CurPtr))
      return; // KeepWhitespaceMode
    goto LexNextToken;   // GCC isn't tail call eliminating.
  case ' ':
  case '\t':
  case '\f':
  case '\v':
  SkipHorizontalWhitespace:
    Result.setFlag(Token::LeadingSpace);
    if (SkipWhitespace(Result, CurPtr))
      return; // KeepWhitespaceMode

  SkipIgnoredUnits:
    CurPtr = BufferPtr;

    // If the next token is obviously a // or /* */ comment, skip it efficiently
    // too (without going through the big switch stmt).
    if (CurPtr[0] == '/' && CurPtr[1] == '/') {
      if (SkipLineComment(Result, CurPtr+2))
        return; // There is a token to return.
      goto SkipIgnoredUnits;
    } else if (CurPtr[0] == '/' && CurPtr[1] == '*') {
      if (SkipBlockComment(Result, CurPtr+2))
        return; // There is a token to return.
      goto SkipIgnoredUnits;
    } else if (isHorizontalWhitespace(*CurPtr)) {
      goto SkipHorizontalWhitespace;
    }
    goto LexNextToken;   // GCC isn't tail call eliminating.
      
  // http://golang.org/ref/spec#Integer_literals
  // http://golang.org/ref/spec#Floating-point_literals
  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    return LexNumericConstant(Result, CurPtr);

  // http://golang.org/ref/spec#Identifiers
  case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
  case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
  case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
  case 'V': case 'W': case 'X': case 'Y': case 'Z':
  case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
  case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
  case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
  case 'v': case 'w': case 'x': case 'y': case 'z':
  case '_':
    return LexIdentifier(Result, CurPtr);

  // http://golang.org/ref/spec#Rune_literals
  case '\'':
    return LexRuneLiteral(Result, CurPtr, tok::rune_literal);

  // http://golang.org/ref/spec#String_literals
  case '"':
    return LexStringLiteral(Result, CurPtr, tok::string_literal);
  case '`':
    return LexRawStringLiteral(Result, CurPtr, tok::string_literal);

  // http://golang.org/ref/spec#Operators_and_Delimiters
  case '[':
    Kind = tok::l_square;
    break;
  case ']':
    Kind = tok::r_square;
    break;
  case '(':
    Kind = tok::l_paren;
    break;
  case ')':
    Kind = tok::r_paren;
    break;
  case '{':
    Kind = tok::l_brace;
    break;
  case '}':
    Kind = tok::r_brace;
    break;
  case '.':
    Char = getCharAndSize(CurPtr, SizeTmp);
    if (Char >= '0' && Char <= '9') {
      return LexNumericConstant(Result, ConsumeChar(CurPtr, SizeTmp, Result));
    } else if (Char == '.' &&
               getCharAndSize(CurPtr+SizeTmp, SizeTmp2) == '.') {
      Kind = tok::ellipsis;
      CurPtr = ConsumeChar(ConsumeChar(CurPtr, SizeTmp, Result),
                           SizeTmp2, Result);
    } else {
      Kind = tok::period;
    }
    break;
  case '&':
    Char = getCharAndSize(CurPtr, SizeTmp);
    if (Char == '&') {
      Kind = tok::ampamp;
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
    } else if (Char == '=') {
      Kind = tok::ampequal;
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
    } else if (Char == '^') {
      char After = getCharAndSize(CurPtr+SizeTmp, SizeTmp2);
      if (After == '=') {
        Kind = tok::ampcaretequal;
        CurPtr = ConsumeChar(ConsumeChar(CurPtr, SizeTmp, Result),
                             SizeTmp2, Result);
      } else {
        Kind = tok::ampcaret;
        CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
      }
    } else {
      Kind = tok::amp;
    }
    break;
  case '*':
    if (getCharAndSize(CurPtr, SizeTmp) == '=') {
      Kind = tok::starequal;
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
    } else {
      Kind = tok::star;
    }
    break;
  case '+':
    Char = getCharAndSize(CurPtr, SizeTmp);
    if (Char == '+') {
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
      Kind = tok::plusplus;
    } else if (Char == '=') {
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
      Kind = tok::plusequal;
    } else {
      Kind = tok::plus;
    }
    break;
  case '-':
    Char = getCharAndSize(CurPtr, SizeTmp);
    if (Char == '-') {      // --
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
      Kind = tok::minusminus;
    } else if (Char == '=') {   // -=
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
      Kind = tok::minusequal;
    } else {
      Kind = tok::minus;
    }
    break;
  case '!':
    if (getCharAndSize(CurPtr, SizeTmp) == '=') {
      Kind = tok::exclaimequal;
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
    } else {
      Kind = tok::exclaim;
    }
    break;
  case '/':
    // http://golang.org/ref/spec#Comments
    Char = getCharAndSize(CurPtr, SizeTmp);
    if (Char == '/') {         // Line comment.
      if (SkipLineComment(Result, ConsumeChar(CurPtr, SizeTmp, Result)))
        return; // There is a token to return.

      // It is common for the tokens immediately after a // comment to be
      // whitespace (indentation for the next line).  Instead of going through
      // the big switch, handle it efficiently now.
      goto SkipIgnoredUnits;
    }

    if (Char == '*') {  // /**/ comment.
      if (SkipBlockComment(Result, ConsumeChar(CurPtr, SizeTmp, Result)))
        return; // There is a token to return.
      goto LexNextToken;   // GCC isn't tail call eliminating.
    }

    if (Char == '=') {
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
      Kind = tok::slashequal;
    } else {
      Kind = tok::slash;
    }
    break;
  case '%':
    Char = getCharAndSize(CurPtr, SizeTmp);
    if (Char == '=') {
      Kind = tok::percentequal;
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
    } else {
      Kind = tok::percent;
    }
    break;
  case '<':
    Char = getCharAndSize(CurPtr, SizeTmp);
    if (Char == '<') {
      char After = getCharAndSize(CurPtr+SizeTmp, SizeTmp2);
      if (After == '=') {
        Kind = tok::lesslessequal;
        CurPtr = ConsumeChar(ConsumeChar(CurPtr, SizeTmp, Result),
                             SizeTmp2, Result);
      } else if (After == '<' && IsStartOfConflictMarker(CurPtr-1)) {
        // If this is actually a '<<<<<<<' version control conflict marker,
        // recognize it as such and recover nicely.
        goto LexNextToken;
      } else if (After == '<' && HandleEndOfConflictMarker(CurPtr-1)) {
        // If this is '<<<<' and we're in a Perforce-style conflict marker,
        // ignore it.
        goto LexNextToken;
      } else {
        CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
        Kind = tok::lessless;
      }
    } else if (Char == '=') {
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
      Kind = tok::lessequal;
    } else if (Char == '-') {
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
      Kind = tok::lessminus;
    } else {
      Kind = tok::less;
    }
    break;
  case '>':
    Char = getCharAndSize(CurPtr, SizeTmp);
    if (Char == '=') {
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
      Kind = tok::greaterequal;
    } else if (Char == '>') {
      char After = getCharAndSize(CurPtr+SizeTmp, SizeTmp2);
      if (After == '=') {
        CurPtr = ConsumeChar(ConsumeChar(CurPtr, SizeTmp, Result),
                             SizeTmp2, Result);
        Kind = tok::greatergreaterequal;
      } else if (After == '>' && IsStartOfConflictMarker(CurPtr-1)) {
        // If this is actually a '>>>>' conflict marker, recognize it as such
        // and recover nicely.
        goto LexNextToken;
      } else if (After == '>' && HandleEndOfConflictMarker(CurPtr-1)) {
        // If this is '>>>>>>>' and we're in a conflict marker, ignore it.
        goto LexNextToken;
      } else {
        CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
        Kind = tok::greatergreater;
      }
      
    } else {
      Kind = tok::greater;
    }
    break;
  case '^':
    Char = getCharAndSize(CurPtr, SizeTmp);
    if (Char == '=') {
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
      Kind = tok::caretequal;
    } else {
      Kind = tok::caret;
    }
    break;
  case '|':
    Char = getCharAndSize(CurPtr, SizeTmp);
    if (Char == '=') {
      Kind = tok::pipeequal;
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
    } else if (Char == '|') {
      // If this is '|||||||' and we're in a conflict marker, ignore it.
      if (CurPtr[1] == '|' && HandleEndOfConflictMarker(CurPtr-1))
        goto LexNextToken;
      Kind = tok::pipepipe;
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
    } else {
      Kind = tok::pipe;
    }
    break;
  case ':':
    Char = getCharAndSize(CurPtr, SizeTmp);
    if (Char == '=') {
      Kind = tok::colonequal;
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
    } else {
      Kind = tok::colon;
    }
    break;
  case ';':
    Kind = tok::semi;
    break;
  case '=':
    Char = getCharAndSize(CurPtr, SizeTmp);
    if (Char == '=') {
      // If this is '====' and we're in a conflict marker, ignore it.
      if (CurPtr[1] == '=' && HandleEndOfConflictMarker(CurPtr-1))
        goto LexNextToken;
      
      Kind = tok::equalequal;
      CurPtr = ConsumeChar(CurPtr, SizeTmp, Result);
    } else {
      Kind = tok::equal;
    }
    break;
  case ',':
    Kind = tok::comma;
    break;

  default:
    if (Char >= 128) {
      Rune r;
      int len = chartorune(&r, CurPtr - 1);
      CurPtr = CurPtr - 1 + len;
      if (isalpharune(r)) {
        return LexUtfIdentifier(Result, CurPtr);
      }
      if (r == Runeerror && len == 1) {
        Diag(CurPtr - 1, diag::invalid_utf_sequence);
      }
    }
    Kind = tok::unknown;
    break;
  }

  // Update the location of token as well as BufferPtr.
  FormTokenWithChars(Result, CurPtr, Kind);
}

bool Lexer::InsertSemi(Token &Result, const char *TokEnd) {
  // http://golang.org/ref/spec#Semicolons
  if (LastTokenKind == tok::identifier ||
      LastTokenKind == tok::numeric_literal ||
      LastTokenKind == tok::rune_literal ||
      LastTokenKind == tok::string_literal ||
      LastTokenKind == tok::kw_break ||
      LastTokenKind == tok::kw_continue ||
      LastTokenKind == tok::kw_fallthrough ||
      LastTokenKind == tok::kw_return ||
      LastTokenKind == tok::plusplus ||
      LastTokenKind == tok::minusminus ||
      LastTokenKind == tok::r_paren ||
      LastTokenKind == tok::r_square ||
      LastTokenKind == tok::r_brace) {
    // FormTokenWithChars() will set LastTokenKind to tok::semi, which will 
    // prevert insertion of more than one semicolon.
    Result.setFlag(Token::InsertedSemi);
    FormTokenWithChars(Result, TokEnd, tok::semi);
    return true;
  }
  return false;
}

void Lexer::addCommentHandler(CommentHandler *Handler) {
  assert(Handler && "NULL comment handler");
  assert(std::find(CommentHandlers.begin(), CommentHandlers.end(), Handler) ==
         CommentHandlers.end() && "Comment handler already registered");
  CommentHandlers.push_back(Handler);
}

void Lexer::removeCommentHandler(CommentHandler *Handler) {
  std::vector<CommentHandler *>::iterator Pos
      = std::find(CommentHandlers.begin(), CommentHandlers.end(), Handler);
  assert(Pos != CommentHandlers.end() && "Comment handler not registered");
  CommentHandlers.erase(Pos);
}

void Lexer::handleComment(Token &result, SourceRange Comment) {
  for (std::vector<CommentHandler *>::iterator H = CommentHandlers.begin(),
       HEnd = CommentHandlers.end();
       H != HEnd; ++H) {
    (*H)->handleComment(*this, Comment);
  }
}
CommentHandler::~CommentHandler() { }
