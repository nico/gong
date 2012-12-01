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

  // non-identifier ExpressionStmts
  case tok::amp:
  case tok::caret:
  case tok::exclaim:
  case tok::kw_chan:
  case tok::kw_func:
  case tok::kw_interface:
  case tok::kw_map:
  case tok::kw_struct:
  case tok::l_paren:
  case tok::l_square:
  case tok::lessminus:
  case tok::minus:
  case tok::numeric_literal:
  case tok::plus:
  case tok::rune_literal:
  case tok::star:
  case tok::string_literal:
    return ParseExpression().isInvalid();
  
  // FIXME: Check calls that won't come here (kw_package, kw_import, etc)?
  //        Silently ignore them?
  default: llvm_unreachable("unexpected token kind");
  }
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

/// SimpleStmt = EmptyStmt | ExpressionStmt | SendStmt | IncDecStmt |
///              Assignment | ShortVarDecl .
bool Parser::ParseSimpleStmt(SimpleStmtKind *OutKind, SimpleStmtExts Ext) {
  if (Tok.is(tok::semi))
    return ParseEmptyStmt();
  if (Tok.is(tok::identifier)) {
    IdentifierInfo *II = Tok.getIdentifierInfo();
    ConsumeToken();
    return ParseSimpleStmtTail(II, OutKind, Ext);
  }
  SourceLocation StartLoc = Tok.getLocation();
  TypeSwitchGuardParam Opt, *POpt = Ext == SSE_TypeSwitchGuard ? &Opt : NULL;
  // Could be: ExpressionStmt, SendStmt, IncDecStmt, Assignment. They all start
  // with an Expression.
  ExprResult LHS = ParseExpression(POpt);
  return ParseSimpleStmtTailAfterExpression(LHS, StartLoc, POpt, OutKind, Ext);
}

namespace {

bool IsAssignmentOp(tok::TokenKind Kind) {
  switch (Kind) {
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

/// A simple RAII class that lets a TypeSwitchGuardParam emit a diagnostic when
/// the RAII object hasn't been disarmed. Otherwise, it sets a SimpleStmtKind
/// pointer to SSK_TypeSwitchGuard.
class TypeSwitchGuardParamRAII {
  Parser *Self;
  Parser::TypeSwitchGuardParam* TSG;
  bool Armed;
  Parser::SimpleStmtKind *Out;
public:
  TypeSwitchGuardParamRAII(
      Parser *Self, Parser::TypeSwitchGuardParam* TSG,
      Parser::SimpleStmtKind *Out)
      : Self(Self), TSG(TSG), Armed(true), Out(Out) {}
  ~TypeSwitchGuardParamRAII() {
    if (!TSG)
      return;
    if (Armed)
      TSG->Reset(*Self);
    else if (TSG->Result == Parser::TypeSwitchGuardParam::Parsed && Out)
      *Out = Parser::SSK_TypeSwitchGuard;
  }
  void disarm() { Armed = false; }
};

}  // namespace

/// Called after the leading IdentifierInfo of a simple statement has been read.
bool Parser::ParseSimpleStmtTail(IdentifierInfo *II, SimpleStmtKind *OutKind,
                                 SimpleStmtExts Ext) {
  SourceLocation StartLoc = PrevTokLocation;
  // FIXME: Tok could be '.'

  if (Tok.is(tok::comma)) {
    // It's an identifier list! (Which can be interpreted as expression list.)
    // If it's followed by ':=', this is a ShortVarDecl or a RangeClause (the
    // only statements starting with an identifier list).
    // If it's followed by '=', this is an Assignment or a RangeClause (the
    // only statement starting with an expression list).
    ParseIdentifierListTail(II);
    if (Tok.isNot(tok::colonequal) && Tok.isNot(tok::equal)) {
      Diag(Tok, diag::expected_colonequal_or_equal);
      SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
      return true;
      // FIXME: For bonus points, only suggest ':=' if at least one identifier
      //        is new.
    }
    tok::TokenKind Op = Tok.getKind();
    ConsumeToken();

    if (Tok.is(tok::kw_range))
      return ParseRangeClauseTail(Op, OutKind, Ext);
    if (Op == tok::colonequal)
      return ParseShortVarDeclTail();
    if (Op == tok::equal)
      // Since more than one element was parsed, only '=' is valid here.
      return ParseAssignmentTail(tok::equal);
  }

  TypeSwitchGuardParam Opt, *POpt = Ext == SSE_TypeSwitchGuard ? &Opt : NULL;

  if (Tok.is(tok::colonequal)) {
    ConsumeToken();
    if (Tok.is(tok::kw_range))
      return ParseRangeClauseTail(tok::colonequal, OutKind, Ext);
    bool Result = ParseShortVarDeclTail(POpt);
    if (POpt && POpt->Result == TypeSwitchGuardParam::Parsed && OutKind)
      *OutKind = SSK_TypeSwitchGuard;
    return Result;
  }
  // Could be: ExpressionStmt, SendStmt, IncDecStmt, Assignment. They all start
  // with an Expression.
  ExprResult LHS = ParseExpressionTail(II, POpt);
  return ParseSimpleStmtTailAfterExpression(LHS, StartLoc, POpt, OutKind, Ext);
}

/// Called after the leading Expression of a simple statement has been read.
bool
Parser::ParseSimpleStmtTailAfterExpression(ExprResult &LHS,
                                           SourceLocation StartLoc,
                                           TypeSwitchGuardParam *Opt,
                                           SimpleStmtKind *OutKind,
                                           SimpleStmtExts Ext) {
  // In a switch statement, ParseSimpleStmtTail() has to accept TypeSwitchGuards
  // which technically aren't SimpleStmts.  OptRAII will diag on all
  // TypeSwitchGuards when destructed unless it's explicitly disarm()ed.
  TypeSwitchGuardParamRAII OptRAII(this, Opt, OutKind);

  if (Tok.is(tok::comma)) {
    // Must be an expression list, and the simplestmt must be an assignment.
    ParseExpressionListTail(LHS);

    if (!IsAssignmentOp(Tok.getKind()) && Tok.isNot(tok::colonequal)) {
      Diag(Tok, diag::expected_assign_op);
      return true;
    }

    tok::TokenKind Op = Tok.getKind();
    SourceLocation OpLocation = ConsumeToken();
    if (Tok.is(tok::kw_range))
      return ParseRangeClauseTail(Op, OutKind, Ext);
    // "In assignment operations, both the left- and right-hand expression
    // lists must contain exactly one single-valued expression."
    // So expect a '=' for an assignment -- assignment operations (+= etc)
    // aren't permitted after an expression list.
    if (Op != tok::equal) {
      Diag(OpLocation, diag::expected_equal);
      SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
      return true;
    }
    return ParseAssignmentTail(tok::equal);
  }

  if (IsAssignmentOp(Tok.getKind())) {
    tok::TokenKind Op = Tok.getKind();
    ConsumeToken();
    if (Tok.is(tok::kw_range))
      return ParseRangeClauseTail(tok::colonequal, OutKind, Ext);
    // FIXME: if Op is '=' and the expression a TypeSwitchGuard, provide fixit
    // to turn '=' into ':='.
    return ParseAssignmentTail(Op);
  }

  if (Tok.is(tok::colonequal)) {
    ConsumeToken();
    if (Tok.is(tok::kw_range))
      return ParseRangeClauseTail(tok::colonequal, OutKind, Ext);
    // FIXME: fixit to change ':=' to '='
    Diag(StartLoc, diag::invalid_expr_left_of_colonequal);
    // FIXME: propagate error.
    return ParseShortVarDeclTail();
  }

  if (Tok.is(tok::plusplus) || Tok.is(tok::minusminus))
    return ParseIncDecStmtTail(LHS);

  if (Tok.is(tok::lessminus))
    return ParseSendStmtTail(LHS);

  if (OutKind)
    *OutKind = SSK_Expression;
  // Note: This can overwrite *OutKind.
  OptRAII.disarm();

  return LHS.isInvalid();
}

/// This is called after the ':=' has been read.
/// ShortVarDecl = IdentifierList ":=" ExpressionList .
bool Parser::ParseShortVarDeclTail(TypeSwitchGuardParam *Opt) {
  return ParseExpressionList(Opt).isInvalid();
}

/// This is called after the assign_op has been read.
/// Assignment = ExpressionList assign_op ExpressionList .
/// 
/// assign_op = [ add_op | mul_op ] "=" .
/// The op= construct is a single token.
bool Parser::ParseAssignmentTail(tok::TokenKind Op) {
  assert(IsAssignmentOp(Op) && "expected assignment op");
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
  SimpleStmtKind Kind = SSK_Normal;
  if (ParseSimpleStmt(&Kind))
    return true;

  if (Tok.is(tok::semi)) {
    ConsumeToken();
    ParseExpression();
  } else if (Kind != SSK_Expression) {
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
bool Parser::ParseSwitchStmt() {
  assert(Tok.is(tok::kw_switch) && "expected 'switch'");
  ConsumeToken();

  bool IsTypeSwitch = false;

  // For ExprSwitchStmts, everything between 'switch and '{' is optional.
  // (This is not true for TypeSwitchStmts.)
  if (Tok.isNot(tok::l_brace)) {
    // FIXME: This is fairly similar to the code in ParseIfStmt. If that's still
    // the case once TypeSwitchStmts work, refactor.
    SourceLocation StmtLoc = Tok.getLocation();
    SimpleStmtKind Kind = SSK_Normal;
    if (ParseSimpleStmt(&Kind, SSE_TypeSwitchGuard)) {
      SkipUntil(tok::l_brace, /*StopAtSemi=*/false, /*DontConsume=*/true);
    }

    if (Kind == SSK_TypeSwitchGuard) {
      IsTypeSwitch = true;
    } else {
      if (Tok.is(tok::semi)) {
        ConsumeToken();

        Kind = SSK_Normal;
        ParseSimpleStmt(&Kind, SSE_TypeSwitchGuard);
        if (Kind == SSK_TypeSwitchGuard)
          IsTypeSwitch = true;
        else if (Kind != SSK_Expression)
          Diag(StmtLoc, diag::expected_expr_or_typeswitchguard);
      } else if (Kind != SSK_Expression) {
        Diag(StmtLoc, diag::expected_expr_or_typeswitchguard);
      }
    }
  }

  if (Tok.isNot(tok::l_brace)) {
    Diag(Tok, diag::expected_l_brace);
    SkipUntil(tok::l_brace, /*StopAtSemi=*/false, /*DontConsume=*/true);
  }
  ConsumeBrace();

  // FIXME: This is fairly similar to the {} code in ParseSelectStmt().
  while (Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
    if (Tok.isNot(tok::kw_case) && Tok.isNot(tok::kw_default)) {
      Diag(Tok, diag::expected_case_or_default);
      SkipUntil(tok::r_brace, /*StopAtSemi=*/false);
      return true;
    }
    if (ParseCaseClause(IsTypeSwitch ? TypeCaseClause : ExprCaseClause)) {
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
/// TypeCaseClause  = TypeSwitchCase ":" { Statement ";" } .
bool Parser::ParseCaseClause(CaseClauseType Type) {
  ParseSwitchCase(Type);

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
/// TypeSwitchCase  = "case" TypeList | "default" .
bool Parser::ParseSwitchCase(CaseClauseType Type) {
  assert((Tok.is(tok::kw_case) || Tok.is(tok::kw_default)) &&
         "expected 'case' or 'default'");
  if (Tok.is(tok::kw_default)) {
    ConsumeToken();
    return false;
  }
  ConsumeToken();
  switch (Type) {
  case ExprCaseClause: return ParseExpressionList().isInvalid();
  case TypeCaseClause: return ParseTypeList();
  }
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
bool Parser::ParseForStmt() {
  assert(Tok.is(tok::kw_for) && "expected 'for'");
  ConsumeToken();

  if (Tok.is(tok::l_brace))
    return ParseBlock();

  SourceLocation StmtLoc = Tok.getLocation();
  SimpleStmtKind Kind = SSK_Normal;
  if (Tok.isNot(tok::semi))
    if (ParseSimpleStmt(&Kind, SSE_RangeClause))
      return true;

  if (Kind == SSK_RangeClause) {
    // Range clause, nothing more to do.
  } else if (Tok.is(tok::semi)) {
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
  } else if (Kind != SSK_Expression) {
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

/// This is called when Tok points at "range".
/// RangeClause = Expression [ "," Expression ] ( "=" | ":=" )
///               "range" Expression .
bool Parser::ParseRangeClauseTail(tok::TokenKind Op, SimpleStmtKind *OutKind,
                                  SimpleStmtExts Exts) {
  assert(Tok.is(tok::kw_range) && "expected 'range'");
  ConsumeToken();

  // FIXME: return an invalid statement in this case.
  bool Failed = false;
  if (Exts != SSE_RangeClause) {
    Diag(Tok, diag::range_only_valid_in_for);
    Failed = true;
  }
  if (Op != tok::equal && Op != tok::colonequal) {
    Diag(PrevTokLocation, diag::expected_colonequal_or_equal);
    Failed = true;
  }

  if (OutKind)
    *OutKind = SSK_RangeClause;

  return ParseExpression().isInvalid();
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
