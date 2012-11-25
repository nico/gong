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
#include "gong/Parse/Scope.h"
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
    case tok::kw_go:          return ParseGoStmt();
    case tok::kw_return:      return ParseReturnStmt();
    case tok::kw_break:       return ParseBreakStmt();
    case tok::kw_continue:    return ParseContinueStmt();
    case tok::kw_goto:        return ParseGotoStmt();
    case tok::kw_fallthrough: return ParseFallthroughStmt();
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
bool Parser::ParseGoStmt() {
  assert(Tok.is(tok::kw_go) && "expected 'go'");
  ConsumeToken();
  return ParseExpression().isInvalid();
}

/// ReturnStmt = "return" [ ExpressionList ] .
bool Parser::ParseReturnStmt() {
  // FIXME
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

/// BreakStmt = "break" [ Label ] .
bool Parser::ParseBreakStmt() {
  assert(Tok.is(tok::kw_break) && "expected 'break'");
  ConsumeToken();
  if (Tok.is(tok::identifier))
    ConsumeToken();
  return false;
}

/// ContinueStmt = "continue" [ Label ] .
bool Parser::ParseContinueStmt() {
  assert(Tok.is(tok::kw_continue) && "expected 'continue'");
  ConsumeToken();
  if (Tok.is(tok::identifier))
    ConsumeToken();
  return true;
}

/// GotoStmt = "goto" Label .
bool Parser::ParseGotoStmt() {
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

/// FallthroughStmt = "fallthrough" .
bool Parser::ParseFallthroughStmt() {
  assert(Tok.is(tok::kw_fallthrough) && "expected 'fallthrough'");
  ConsumeToken();
  return false;
}

/// IfStmt = "if" [ SimpleStmt ";" ] Expression Block
///          [ "else" ( IfStmt | Block ) ] .
bool Parser::ParseIfStmt() {
  // FIXME
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

/// SwitchStmt = ExprSwitchStmt | TypeSwitchStmt .
/// 
/// ExprSwitchStmt = "switch" [ SimpleStmt ";" ] [ Expression ]
///                  "{" { ExprCaseClause } "}" .
/// ExprCaseClause = ExprSwitchCase ":" { Statement ";" } .
/// ExprSwitchCase = "case" ExpressionList | "default" .
/// 
/// TypeSwitchStmt  = "switch" [ SimpleStmt ";" ] TypeSwitchGuard
///                   "{" { TypeCaseClause } "}" .
/// TypeSwitchGuard = [ identifier ":=" ] PrimaryExpr "." "(" "type" ")" .
/// TypeCaseClause  = TypeSwitchCase ":" { Statement ";" } .
/// TypeSwitchCase  = "case" TypeList | "default" .
/// TypeList        = Type { "," Type } .
bool Parser::ParseSwitchStmt() {
  // FIXME
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

/// SelectStmt = "select" "{" { CommClause } "}" .
/// CommClause = CommCase ":" { Statement ";" } .
/// CommCase   = "case" ( SendStmt | RecvStmt ) | "default" .
/// RecvStmt   = [ Expression [ "," Expression ] ( "=" | ":=" ) ] RecvExpr .
/// RecvExpr   = Expression .
bool Parser::ParseSelectStmt() {
  // FIXME
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

/// ForStmt = "for" [ Condition | ForClause | RangeClause ] Block .
/// Condition = Expression .
/// 
/// ForClause = [ InitStmt ] ";" [ Condition ] ";" [ PostStmt ] .
/// InitStmt = SimpleStmt .
/// PostStmt = SimpleStmt .
/// 
/// RangeClause = Expression [ "," Expression ] ( "=" | ":=" )
///               "range" Expression .
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

/// EmptyStmt = .
bool Parser::ParseEmptyStmt() {
  return false;
}

/// Block = "{" { Statement ";" } "}" .
bool Parser::ParseBlock() {
  assert(Tok.is(tok::l_brace) && "Expected '{'");

  // Enter a scope to hold everything within the block.
  ParseScope CompoundScope(this, Scope::DeclScope);

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
