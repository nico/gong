//===--- ParseStmt.cpp - Statement and Block Parser -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Statement and Block portions of the Parser
// interface.
//
//===----------------------------------------------------------------------===//

#include "gong/Parse/Parser.h"
#include "llvm/Support/ErrorHandling.h"
//#include "RAIIObjectsForParser.h"
//#include "gong/Basic/Diagnostic.h"
//#include "gong/Basic/SourceManager.h"
//#include "llvm/ADT/SmallString.h"
using namespace gong;

/// Statement =
///   Declaration | LabeledStmt | SimpleStmt |
///   GoStmt | ReturnStmt | BreakStmt | ContinueStmt | GotoStmt |
///   FallthroughStmt | Block | IfStmt | SwitchStmt | SelectStmt | ForStmt |
///   DeferStmt .
/// SimpleStmt = EmptyStmt | ExpressionStmt | SendStmt | IncDecStmt |
///              Assignment | ShortVarDecl .
bool Parser::ParseStatement() {
  switch (Tok.getKind()) {
    case tok::kw_const:
    case tok::kw_type:
    case tok::kw_var:         return ParseDeclaration();
    case tok::kw_go:          return ParseGoStmnt();
    case tok::kw_return:      return ParseReturnStmnt();
    case tok::kw_break:       return ParseBreakStmnt();
    case tok::kw_continue:    return ParseContinueStmnt();
    case tok::kw_goto:        return ParseGotoStmnt();
    case tok::kw_fallthrough: return ParseFallthroughStmnt();
    case tok::l_brace:        return ParseBlock();
    case tok::kw_if:          return ParseIfStmt();
    case tok::kw_switch:      return ParseSwitchStmt();
    case tok::kw_select:      return ParseSelectStmt();
    case tok::kw_for:         return ParseForStmt();
    case tok::kw_defer:       return ParseDeferStmt();

    // SimpleStmts
    case tok::semi:           return ParseEmptyStmt();

    case tok::identifier:
      // Could be: Label, ExpressionStmt, IncDecStmt, Assignment, ShortVarDecl
      //FIXME
      ;
    default: llvm_unreachable("unexpected token kind");
  }
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

/// GoStmt = "go" Expression .
bool Parser::ParseGoStmnt() {
  assert(Tok.is(tok::kw_go) && "expected 'go'");
  ConsumeToken();
  return ParseExpression().isInvalid();
}

/// ReturnStmt = "return" [ ExpressionList ] .
bool Parser::ParseReturnStmnt() {
  // FIXME
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

bool Parser::ParseBreakStmnt() {
  assert(Tok.is(tok::kw_break) && "expected 'break'");
  ConsumeToken();
  if (Tok.is(tok::identifier))
    ConsumeToken();
  return false;
}

bool Parser::ParseContinueStmnt() {
  assert(Tok.is(tok::kw_continue) && "expected 'continue'");
  ConsumeToken();
  if (Tok.is(tok::identifier))
    ConsumeToken();
  return true;
}

bool Parser::ParseGotoStmnt() {
  assert(Tok.is(tok::kw_goto) && "expected 'goto'");
  ConsumeToken();
  if (Tok.is(tok::identifier)) {
    ConsumeToken();
    return false;
  }
  Diag(Tok, diag::expected_ident);
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

bool Parser::ParseFallthroughStmnt() {
  assert(Tok.is(tok::kw_fallthrough) && "expected 'fallthrough'");
  ConsumeToken();
  return false;
}

bool Parser::ParseIfStmt() {
  // FIXME
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

bool Parser::ParseSwitchStmt() {
  // FIXME
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

bool Parser::ParseSelectStmt() {
  // FIXME
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

bool Parser::ParseForStmt() {
  // FIXME
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

/// DeferStmt = "defer" Expression .
bool Parser::ParseDeferStmt() {
  assert(Tok.is(tok::kw_defer) && "expected 'defer'");
  ConsumeToken();
  return ParseExpression().isInvalid();
}

bool Parser::ParseEmptyStmt() {
  return false;
}

/// Block = "{" { Statement ";" } "}" .
bool Parser::ParseBlock() {
  assert(Tok.is(tok::l_brace) && "Expected '{'");
  ConsumeBrace();
  // FIXME: scoping, better recovery, check IsStatment() first,
  //        semicolon insertion after last statement
  // See Parser::ParseCompoundStatementBody() in clang.
  while (Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
    ParseStatement();
    ExpectAndConsumeSemi(diag::expected_semi_import);
  }
  return ExpectAndConsume(tok::r_brace, diag::expected_r_brace);
}
