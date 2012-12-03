//===--- RAIIObjectsForParser.h - RAII helpers for the parser ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines and implements the some simple RAII objects that are used
// by the parser to manage bits in recursion.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_PARSE_RAII_OBJECTS_FOR_PARSER_H
#define LLVM_GONG_PARSE_RAII_OBJECTS_FOR_PARSER_H

//#include "gong/Parse/ParseDiagnostic.h"
#include "gong/Parse/Parser.h"
//#include "gong/Sema/DelayedDiagnostic.h"
//#include "gong/Sema/Sema.h"

namespace gong {
  /// CompositeTypeLitNameNeedsParensRAIIObject - This sets the
  /// Parser::CompositeTypeNameNeedsParens bool and restores it when destroyed.
  class CompositeTypeNameLitNeedsParensRAIIObject {
    Parser &P;
  public:
    CompositeTypeNameLitNeedsParensRAIIObject(Parser &p)
      : P(p) {
      assert(!P.CompositeTypeNameLitNeedsParens);
      P.CompositeTypeNameLitNeedsParens = true;
    }
    
    /// reset - This can be used to reset the state early, before the dtor
    /// is run.
    void reset() {
      P.CompositeTypeNameLitNeedsParens = false;
    }

    ~CompositeTypeNameLitNeedsParensRAIIObject() {
      reset();
    }
  };
  
  /// \brief RAII object that makes sure paren/bracket/brace count is correct
  /// after declaration/statement parsing, even when there's a parsing error.
  class ParenBraceBracketBalancer {
    Parser &P;
    unsigned short ParenCount, BracketCount, BraceCount;
  public:
    ParenBraceBracketBalancer(Parser &p)
      : P(p), ParenCount(p.ParenCount), BracketCount(p.BracketCount),
        BraceCount(p.BraceCount) { }
    
    ~ParenBraceBracketBalancer() {
      P.ParenCount = ParenCount;
      P.BracketCount = BracketCount;
      P.BraceCount = BraceCount;
    }
  };

  /// \brief RAII class that helps handle the parsing of an open/close delimiter
  /// pair, such as braces { ... } or parentheses ( ... ).
  class BalancedDelimiterTracker {
    Parser& P;
    tok::TokenKind Kind, Close;
    SourceLocation (Parser::*Consumer)();
    SourceLocation LOpen, LClose;
    
    unsigned short &getDepth() {
      switch (Kind) {
        case tok::l_brace: return P.BraceCount;
        case tok::l_square: return P.BracketCount;
        case tok::l_paren: return P.ParenCount;
        default: llvm_unreachable("Wrong token kind");
      }
    }
    
    enum { MaxDepth = 256 };
    
    bool diagnoseOverflow();
    bool diagnoseMissingClose();
    
  public:
    BalancedDelimiterTracker(Parser& p, tok::TokenKind k)
      : P(p), Kind(k)
    {
      switch (Kind) {
        default: llvm_unreachable("Unexpected balanced token");
        case tok::l_brace:
          Close = tok::r_brace; 
          Consumer = &Parser::ConsumeBrace;
          break;
        case tok::l_paren:
          Close = tok::r_paren; 
          Consumer = &Parser::ConsumeParen;
          break;
          
        case tok::l_square:
          Close = tok::r_square; 
          Consumer = &Parser::ConsumeBracket;
          break;
      }      
    }
    
    SourceLocation getOpenLocation() const { return LOpen; }
    SourceLocation getCloseLocation() const { return LClose; }
    SourceRange getRange() const { return SourceRange(LOpen, LClose); }
    
    bool consumeOpen() {
      if (!P.Tok.is(Kind))
        return true;
      
      if (getDepth() < MaxDepth) {
        LOpen = (P.*Consumer)();
        return false;
      }
      
      return diagnoseOverflow();
    }
    
    bool expectAndConsume(unsigned DiagID,
                          const char *Msg = "",
                          tok::TokenKind SkipToTok = tok::unknown);
    bool consumeClose() {
      if (P.Tok.is(Close)) {
        LClose = (P.*Consumer)();
        return false;
      } 
      
      return diagnoseMissingClose();
    }
    void skipToEnd();
  };

} // end namespace gong

#endif

