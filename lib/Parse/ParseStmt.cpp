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

  case tok::identifier: {
    IdentifierInfo *II = Tok.getIdentifierInfo();
    ConsumeToken();
    if (Tok.is(tok::colon))
      return ParseLabeledStmtTail(II);
    return ParseSimpleStmtTail(II);
  }
  
  // FIXME: all other expr starts: literals, '(', 'struct', etc
  default: llvm_unreachable("unexpected token kind");
  }
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

/// SimpleStmt = EmptyStmt | ExpressionStmt | SendStmt | IncDecStmt |
///              Assignment | ShortVarDecl .
bool Parser::ParseSimpleStmt(bool *StmtWasExpression) {
  if (Tok.is(tok::semi))
    return ParseEmptyStmt();
  // FIXME: Not true, could be all the other valid expr prefixes too (literal,
  // 'struct', etc).
  if (Tok.isNot(tok::identifier)) {
    Diag(Tok, diag::expected_ident);
    // FIXME: recover?
    return true;
  }
  IdentifierInfo *II = Tok.getIdentifierInfo();
  ConsumeToken();
  return ParseSimpleStmtTail(II, StmtWasExpression);
}

static bool IsAssignmentOp(Token &Tok) {
  switch (Tok.getKind()) {
  default:
    return false;
  case tok::equal:
  case tok::plusequal:
  case tok::minusequal:
  case tok::pipeequal:
  case tok::caretequal:
  case tok::starequal:
  case tok::slashequal:
  case tok::percentequal:
  case tok::lesslessequal:
  case tok::greatergreaterequal:
  case tok::ampequal:
  case tok::ampcaretequal:
    return true;
  }
}

/// Called after the leading IdentifierInfo of a statement has been read.
bool Parser::ParseSimpleStmtTail(IdentifierInfo *II, bool *StmtWasExpression) {
  // FIXME: Tok could be '.'

  if (Tok.is(tok::comma)) {
    // It's an identifier list! (Which can be interpreted as expression list.)
    // If it's followed by ':=', this is a ShortVarDecl (the only statement
    // starting with an identifier list).
    // If it's followed by '=', this is an Assignment (the only statement
    // starting with an expression list).
    ParseIdentifierListTail(II);
    if (Tok.is(tok::colonequal))
      return ParseShortVarDeclTail();
    if (IsAssignmentOp(Tok))
      return ParseAssignmentTail();
    else {
      Diag(Tok, diag::expected_colonequal_or_equal);
      // FIXME: recover?
      return true;
      // FIXME: For bonus points, only suggest ':=' if at least one identifier
      //        is new.
    }
  }

  if (Tok.is(tok::colonequal))
    return ParseShortVarDeclTail();

  // FIXME: Or it could be a type!
  // Could be: ExpressionStmt, IncDecStmt, Assignment
  // FIXME: all the other expr prefixes
  ExprResult LHS = ParsePrimaryExprTail(II);
  LHS = ParsePrimaryExprSuffix(LHS);
  LHS = ParseRHSOfBinaryExpression(LHS, prec::Lowest);

  if (Tok.is(tok::comma)) {
    // Must be an expression list, and the simplestmt must be an assignment.
    ParseExpressionListTail(LHS);
    if (Tok.isNot(tok::equal)) {
      Diag(Tok, diag::expected_equal);
      return true;
    }
    return ParseAssignmentTail();
  }

  if (IsAssignmentOp(Tok))
    return ParseAssignmentTail();

  if (Tok.is(tok::plusplus) || Tok.is(tok::minusminus))
    return ParseIncDecStmtTail(LHS);

  if (Tok.is(tok::lessminus))
    return ParseSendStmtTail(LHS);

  if (StmtWasExpression)
    // See the above FIXME about types :-/
    *StmtWasExpression = true;

  return LHS.isInvalid();
}

/// This is called after the identifier list has been read.
/// ShortVarDecl = IdentifierList ":=" ExpressionList .
bool Parser::ParseShortVarDeclTail() {
  assert(Tok.is(tok::colonequal) && "expected ':='");
  ConsumeToken();
  return ParseExpressionList().isInvalid();
}

/// This is called after the lhs expression list has been read.
/// Assignment = ExpressionList assign_op ExpressionList .
/// 
/// assign_op = [ add_op | mul_op ] "=" .
/// The op= construct is a single token.
bool Parser::ParseAssignmentTail() {
  assert(IsAssignmentOp(Tok) && "expected assignment op");
  ConsumeToken();
  return ParseExpressionList().isInvalid();
}

/// This is called after the lhs expression has been read.
/// IncDecStmt = Expression ( "++" | "--" ) .
bool Parser::ParseIncDecStmtTail(ExprResult &LHS) {
  assert((Tok.is(tok::plusplus) || Tok.is(tok::minusminus)) &&
         "expected '++' or '--'");
  ConsumeToken();
  return LHS.isInvalid();
}

/// This is called after the channel has been read.
/// SendStmt = Channel "<-" Expression .
/// Channel  = Expression .
bool Parser::ParseSendStmtTail(ExprResult &LHS) {
  assert(Tok.is(tok::lessminus) && "expected '<-'");
  ConsumeToken();
  return ParseExpression().isInvalid();
}

/// This is called after the label identifier has been read.
/// LabeledStmt = Label ":" Statement .
/// Label       = identifier .
bool Parser::ParseLabeledStmtTail(IdentifierInfo *II) {
  assert(Tok.is(tok::colon) && "expected ':'");
  ConsumeToken();
  return ParseStatement();
}

/// GoStmt = "go" Expression .
bool Parser::ParseGoStmt() {
  assert(Tok.is(tok::kw_go) && "expected 'go'");
  ConsumeToken();
  return ParseExpression().isInvalid();
}

/// ReturnStmt = "return" [ ExpressionList ] .
bool Parser::ParseReturnStmt() {
  assert(Tok.is(tok::kw_return) && "expected 'return'");
  ConsumeToken();

  if (Tok.isNot(tok::semi))
    ParseExpressionList();
  return false;
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
  assert(Tok.is(tok::kw_if) && "expected 'if'");
  ConsumeToken();

  SourceLocation StmtLoc = Tok.getLocation();
  bool StmtWasExpression = false;
  if (ParseSimpleStmt(&StmtWasExpression))
    return true;

  if (Tok.is(tok::semi)) {
    ConsumeToken();
    ParseExpression();
  } else if (!StmtWasExpression) {
    Diag(StmtLoc, diag::expected_expr);
    return true;
  }

  if (Tok.isNot(tok::l_brace)) {
    Diag(Tok, diag::expected_l_brace);
    return true;
  }
  bool Failed = ParseBlock();

  if (Tok.isNot(tok::kw_else))
    return Failed;

  ConsumeToken();
  if (Tok.is(tok::kw_if))
    return ParseIfStmt();
  if (Tok.is(tok::l_brace))
    return ParseBlock();
  Diag(Tok, diag::expected_if_or_l_brace);
  return true;
}

/// SwitchStmt = ExprSwitchStmt | TypeSwitchStmt .
/// 
/// ExprSwitchStmt = "switch" [ SimpleStmt ";" ] [ Expression ]
///                  "{" { ExprCaseClause } "}" .
/// 
/// TypeSwitchStmt  = "switch" [ SimpleStmt ";" ] TypeSwitchGuard
///                   "{" { TypeCaseClause } "}" .
/// TypeSwitchGuard = [ identifier ":=" ] PrimaryExpr "." "(" "type" ")" .
/// TypeCaseClause  = TypeSwitchCase ":" { Statement ";" } .
/// TypeSwitchCase  = "case" TypeList | "default" .
/// TypeList        = Type { "," Type } .
bool Parser::ParseSwitchStmt() {
  // FIXME: TypeSwitchStmts. Detecting those, especially with the optinal
  // SimpleStmt is annoying. Either this requires arbitrary lookahead to look
  // for '.' '(' 'type' (until an unbalanced '{' is hit), or ParseSimpleStmt
  // needs to learn to parse TypeSwitchGuards (and needs to retroactively verify
  // that the lhs expressionlist was just a single identifier etc)
  // For now, just assume we always have an expression switch statement.

  assert(Tok.is(tok::kw_switch) && "expected 'switch'");
  ConsumeToken();

  // For ExprSwitchStmts, everything between 'switch and '{' is optional.
  // (This is not true for TypeSwitchStmts.)
  if (Tok.isNot(tok::l_brace)) {
    // FIXME: This is fairly similar to the code in ParseIfStmt. If that's still
    // the case once TypeSwitchStmts work, refactor.
    SourceLocation StmtLoc = Tok.getLocation();
    bool StmtWasExpression = false;
    if (ParseSimpleStmt(&StmtWasExpression)) {
      SkipUntil(tok::l_brace, /*StopAtSemi=*/false, /*DontConsume=*/true);
    }

    if (Tok.is(tok::semi)) {
      ConsumeToken();
      ParseExpression();
    } else if (!StmtWasExpression) {
      Diag(StmtLoc, diag::expected_expr);
    }
  }

  if (Tok.isNot(tok::l_brace)) {
    Diag(Tok, diag::expected_l_brace);
    return true;
  }
  ConsumeBrace();

  // FIXME: This is fairly similar to the {} code in ParseSelectStmt().
  while (Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
    if (Tok.isNot(tok::kw_case) && Tok.isNot(tok::kw_default)) {
      Diag(Tok, diag::expected_case_or_default);
      SkipUntil(tok::r_brace, /*StopAtSemi=*/false);
      return true;
    }
    if (ParseExprCaseClause()) {
      SkipUntil(tok::r_brace, /*StopAtSemi=*/false);
      return true;
    }
  }
  // FIXME: diag on missing r_brace
  if (Tok.is(tok::r_brace))
    ConsumeBrace();

  return false;
}

/// ExprCaseClause = ExprSwitchCase ":" { Statement ";" } .
bool Parser::ParseExprCaseClause() {
  ParseExprSwitchCase();

  // FIXME: This is _really_ similar to ParseCommClause.
  if (Tok.isNot(tok::colon)) {
    // FIXME: just add fixit (at least for default and simple CommCases).
    Diag(Tok, diag::expected_colon);
  } else {
    ConsumeToken();
  }

  // ExprCaseClauses are always in ExprSwitchStmts, where they can only be
  // followed by other ExprCaseClauses, or the end of the ExprSwitchStmt.
  while (Tok.isNot(tok::kw_case) && Tok.isNot(tok::kw_default) &&
         Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
    ParseStatement();

    // A semicolon may be omitted before a closing ')' or '}'.
    if (Tok.is(tok::r_brace))
      break;

    if (Tok.isNot(tok::semi)) {
      Diag(Tok, diag::expected_semi);
      SkipUntil(tok::semi, tok::r_brace,
                /*ConsumeSemi=*/false, /*DontConsume=*/true);
    } else {
      ConsumeToken();
    }
  }
  return false;
}

/// ExprSwitchCase = "case" ExpressionList | "default" .
bool Parser::ParseExprSwitchCase() {
  assert((Tok.is(tok::kw_case) || Tok.is(tok::kw_default)) &&
         "expected 'case' or 'default'");
  if (Tok.is(tok::kw_default)) {
    ConsumeToken();
    return false;
  }
  ConsumeToken();
  return ParseExpressionList().isInvalid();
}

/// SelectStmt = "select" "{" { CommClause } "}" .
bool Parser::ParseSelectStmt() {
  assert(Tok.is(tok::kw_select) && "expected 'select'");
  ConsumeToken();

  // FIXME: This is somewhat similar to ParseInterfaceType
  if (Tok.isNot(tok::l_brace)) {
    // FIXME: ...after 'select'
    Diag(Tok, diag::expected_l_brace);
    // FIXME: recover?
    return true;
  }
  ConsumeBrace();

  // FIXME: might be better to run this loop only while (IsCommClause)?
  while (Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
    if (Tok.isNot(tok::kw_case) && Tok.isNot(tok::kw_default)) {
      Diag(Tok, diag::expected_case_or_default);
      SkipUntil(tok::r_brace, /*StopAtSemi=*/false);
      return true;
    }
    if (ParseCommClause()) {
      SkipUntil(tok::r_brace, /*StopAtSemi=*/false);
      return true;
    }
  }
  // FIXME: diag on missing r_brace
  if (Tok.is(tok::r_brace))
    ConsumeBrace();

  return false;
}

/// CommClause = CommCase ":" { Statement ";" } .
bool Parser::ParseCommClause() {
  ParseCommCase();

  if (Tok.isNot(tok::colon)) {
    // FIXME: just add fixit (at least for default and simple CommCases).
    Diag(Tok, diag::expected_colon);
  } else {
    ConsumeToken();
  }

  // CommClauses are always in SelectStmts, where they can only be followed by
  // other CommClauses, or the end of the SelectStmt.
  while (Tok.isNot(tok::kw_case) && Tok.isNot(tok::kw_default) &&
         Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
    ParseStatement();

    // A semicolon may be omitted before a closing ')' or '}'.
    if (Tok.is(tok::r_brace))
      break;

    if (Tok.isNot(tok::semi)) {
      Diag(Tok, diag::expected_semi);
      SkipUntil(tok::semi, tok::r_brace,
                /*ConsumeSemi=*/false, /*DontConsume=*/true);
    } else {
      ConsumeToken();
    }
  }
  return false;
}

/// CommCase   = "case" ( SendStmt | RecvStmt ) | "default" .
/// RecvStmt   = [ Expression [ "," Expression ] ( "=" | ":=" ) ] RecvExpr .
/// RecvExpr   = Expression .
/// SendStmt = Channel "<-" Expression .
/// Channel  = Expression .
bool Parser::ParseCommCase() {
  assert((Tok.is(tok::kw_case) || Tok.is(tok::kw_default)) &&
         "expected 'case' or 'default'");
  if (Tok.is(tok::kw_default)) {
    ConsumeToken();
    return false;
  }

  ConsumeToken();  // Consume 'case'.

  ExprResult LHS = ParseExpression();
  if (Tok.is(tok::lessminus))
    return ParseSendStmtTail(LHS);

  // This is a RecvStmt.
  bool FoundAssignOp = false;
  if (Tok.is(tok::comma)) {
    LHS = ParseExpressionListTail(LHS);

    if (Tok.isNot(tok::equal) && Tok.isNot(tok::colonequal)) {
      Diag(Tok, diag::expected_colonequal_or_equal);
      // FIXME: recover?
      return true;
    }
    ConsumeToken();  // Eat '=' or ':='.
    FoundAssignOp = true;
  } else if (Tok.is(tok::equal) || Tok.is(tok::colonequal)) {
    ConsumeToken();  // Eat '=' or ':='.
    FoundAssignOp = true;
  }

  if (FoundAssignOp) {
    // Parse RecvExpr after the assignment operator.
    ParseExpression();
  } else {
    // LHS was the RecvExpr, nothing to do.
  }
  return false;
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
  assert(Tok.is(tok::kw_for) && "expected 'for'");
  ConsumeToken();

  if (Tok.is(tok::l_brace))
    return ParseBlock();

  SourceLocation StmtLoc = Tok.getLocation();
  bool StmtWasExpression = false;
  // FIXME: Need to allow range clauses in the simplestmt too
  if (Tok.isNot(tok::semi))
    if (ParseSimpleStmt(&StmtWasExpression))
      return true;

  if (Tok.is(tok::semi)) {
    // ForClause case
    ConsumeToken();  // Consume 1st ';'.
    if (Tok.isNot(tok::semi))
      ParseExpression();
    if (Tok.isNot(tok::semi)) {
      Diag(Tok, diag::expected_semi);
      // FIXME: recover?
      return true;
    }
    ConsumeToken();  // Consume 2nd ';'.
    if (Tok.isNot(tok::l_brace)) {
      ParseSimpleStmt();
    }
    // FIXME: Collect errors above.
  } else if (!StmtWasExpression) {
    Diag(StmtLoc, diag::expected_expr);
    // FIXME: recover?
  } else {
    // Single expression
    // (FIXME: or range expr, once that's done)
  }

  if (Tok.isNot(tok::l_brace)) {
    Diag(Tok, diag::expected_l_brace);
    return true;
  }
  return ParseBlock();
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
    // A semicolon may be omitted before a closing ')' or '}'.
    if (Tok.is(tok::r_brace))
      break;
    ExpectAndConsumeSemi(diag::expected_semi);
  }
  return ExpectAndConsume(tok::r_brace, diag::expected_r_brace);
}
