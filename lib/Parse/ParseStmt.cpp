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
#include "RAIIObjectsForParser.h"
//#include "gong/Basic/Diagnostic.h"
//#include "gong/Basic/SourceManager.h"
//#include "llvm/ADT/SmallString.h"
using namespace gong;

/// Statement =
///   Declaration | LabeledStmt | SimpleStmt |
///   GoStmt | ReturnStmt | BreakStmt | ContinueStmt | GotoStmt |
///   FallthroughStmt | Block | IfStmt | SwitchStmt | SelectStmt | ForStmt |
///   DeferStmt .
Action::OwningStmtResult Parser::ParseStatement() {
  switch (Tok.getKind()) {
  case tok::kw_const:
  case tok::kw_type:
  case tok::kw_var:         return ParseDeclarationStmt();
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
    SourceLocation IILoc = ConsumeToken();

    // If a statement starts with "while", maybe the user meant "for" and is
    // just new to Go.  However, it is possible that "while" is just a variable
    // name and that it starts a valid expression.  If the token after "while"
    // could start a for loop and is not valid after "while" in a regular
    // expression, treat "while" as a typo for "for".
    if (II == Ident_while) {
      bool ValidForLoop = Tok.is(tok::l_brace) || IsExpression() ||
                          Tok.is(tok::semi);
      if (ValidForLoop && getBinOpPrecedence(Tok.getKind()) == prec::Unknown)
        return ParseWhileAsForStmt(IILoc);
    }

    if (Tok.is(tok::colon))
      return ParseLabeledStmtTail(IILoc, II);
    return ParseSimpleStmtTail(IILoc, II);
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

  // These two can't start a SimpleStmt, but ParseSimpleStmt() prints a nice
  // diagnostic for prefix inc/dec stmts.
  case tok::plusplus:
  case tok::minusminus:
    return ParseSimpleStmt();
  
  default:
    Diag(Tok, diag::expected_stmt) << L.getSpelling(Tok);
    SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
    return StmtError();
  }
}

/// SimpleStmt = EmptyStmt | ExpressionStmt | SendStmt | IncDecStmt |
///              Assignment | ShortVarDecl .
Action::OwningStmtResult Parser::ParseSimpleStmt(SimpleStmtKind *OutKind,
                                                 SimpleStmtExts Ext) {
  if (Tok.is(tok::semi))
    return ParseEmptyStmt();

  bool IsPreIncDec = false;
  tok::TokenKind IncDecKind;
  SourceLocation IncDecLoc;
  if (Tok.is(tok::plusplus) || Tok.is(tok::minusminus)) {
    // Go has "a++", but not "++a". Provide a nice fixit if a user uses the
    // latter.
    IsPreIncDec = true;
    IncDecKind = Tok.getKind();
    IncDecLoc = ConsumeToken();
  }

  // Could be: ExpressionStmt, SendStmt, IncDecStmt, Assignment. They all start
  // with an Expression.
  OwningStmtResult Result(Actions);
  SimpleStmtKind StmtKind = SSK_Normal;
  if (Tok.is(tok::identifier)) {
    IdentifierInfo *II = Tok.getIdentifierInfo();
    SourceLocation IILoc = ConsumeToken();
    Result = ParseSimpleStmtTail(IILoc, II, &StmtKind, Ext).take();
  } else {
    // Here: Statements starting with an unary operator, Conversions to
    // type literals that don't start with an operator (array, slice, map,
    // interface, chan), expressions in (), int / float / rune / string
    // literals, array / slice / map literals.
    SourceLocation StartLoc = Tok.getLocation();
    TypeSwitchGuardParam Opt, *POpt = Ext == SSE_TypeSwitchGuard ? &Opt : NULL;
    OwningExprResult LHSExpr = ParseExpression(POpt, NULL);
    IdentOrExprList LHS(Actions, getCurScope(), LHSExpr);
    Result = ParseSimpleStmtTailAfterExpression(LHS, StartLoc, POpt, &StmtKind,
                                                Ext).take();
  }

  if (IsPreIncDec) {
    if (StmtKind == SSK_Expression) {
      Diag(IncDecLoc, diag::no_prefix_op)
          << tok::getTokenSimpleSpelling(IncDecKind);

      // FIXME: add a fixit to insert ++ / -- in the right place, requires
      // getting the end loc of an OwningStmtResult. Then:
      // FIXME: Need to recover, but need some way to convert an SSK_Expression
      // OwningStmtResult into an OwningExprResult (also needed for for loops).
      //Result = Actions.ActOnIncDecStmt(move(Result), InsertLoc, IncDecKind);
      return StmtError(); // FIXME: test this happens before OutKind is written.
    } else {
      Diag(IncDecLoc, diag::unexpected_token)
          << tok::getTokenSimpleSpelling(IncDecKind);
      return StmtError(); // FIXME: test this happens before OutKind is written.
    }
  }

  if (OutKind)
    *OutKind = StmtKind;

  return Result;
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
Action::OwningStmtResult
Parser::ParseSimpleStmtTail(SourceLocation IILoc, IdentifierInfo *II,
                            SimpleStmtKind *OutKind, SimpleStmtExts Ext) {
  TypeSwitchGuardParam Opt, *POpt = Ext == SSE_TypeSwitchGuard ? &Opt : NULL;
  SourceLocation StartLoc = PrevTokLocation;
  if (IsPossiblyIdentifierList()) {
    IdentOrExprList LHS(Actions, getCurScope(), StartLoc, II);
    return ParseSimpleStmtTailAfterExpression(LHS, StartLoc, POpt, OutKind,
                                              Ext);
  }
  IdentOrExprList LHS(Actions, getCurScope(),
                      ParseExpressionTail(IILoc, II, POpt));
  return ParseSimpleStmtTailAfterExpression(LHS, StartLoc, POpt, OutKind, Ext);
}

/// Called after the leading Expression of a simple statement has been read.
Action::OwningStmtResult
Parser::ParseSimpleStmtTailAfterExpression(IdentOrExprList &LHS,
                                           SourceLocation StartLoc,
                                           TypeSwitchGuardParam *Opt,
                                           SimpleStmtKind *OutKind,
                                           SimpleStmtExts Ext) {
  // In a switch statement, ParseSimpleStmtTail() has to accept TypeSwitchGuards
  // which technically aren't SimpleStmts.  OptRAII will diag on all
  // TypeSwitchGuards when destructed unless it's explicitly disarm()ed.
  TypeSwitchGuardParamRAII OptRAII(this, Opt, OutKind);


  if (Tok.is(tok::comma)) {
    ParseExpressionListTail(LHS);

    // If it's followed by ':=', this is a ShortVarDecl (the
    // only statement starting with an identifier list) or a RangeClause.
    // If it's followed by '=', this is an Assignment or a RangeClause (the
    // only statements starting with an expression list).
    // TypeSwitchGuards can have only a single identifier, so they don't matter
    // here.
    if (!IsAssignmentOp(Tok.getKind()) && Tok.isNot(tok::colonequal)) {
      Diag(Tok, diag::expected_assign_op);
      SkipUntil(tok::semi, tok::l_brace,
                /*StopAtSemi=*/false, /*DontConsume=*/true);
      return StmtError();
      // FIXME: For bonus points, only suggest ':=' if at least one identifier
      //        is new.
    }

    tok::TokenKind Op = Tok.getKind();
    SourceLocation OpLoc = ConsumeToken();

    if (Tok.is(tok::kw_range))
      // FIXME: check that Exprs has at most two elements in it.
      return ParseRangeClauseTail(StartLoc, LHS, OpLoc, Op, OutKind, Ext)
                 ? StmtError()
                 : Actions.StmtEmpty(); // FIXME

    if (Op == tok::colonequal) {
      if (LHS.Kind() == IdentOrExprList::RK_Ident)
        return ParseShortVarDeclTail(LHS.Identifiers(), OpLoc);
      // FIXME: fixit
    }

    // "In assignment operations, both the left- and right-hand expression
    // lists must contain exactly one single-valued expression."
    // So expect a '=' for an assignment -- assignment operations (+= etc)
    // aren't permitted after an expression list.
    if (Op != tok::equal) {
      Diag(OpLoc, diag::expected_equal);
      SkipUntil(tok::semi, tok::l_brace,
                /*StopAtSemi=*/false, /*DontConsume=*/true);
      return StmtError();
    }
    return ParseAssignmentTail(OpLoc, tok::equal, LHS.Expressions());
  }

  if (IsAssignmentOp(Tok.getKind())) {
    tok::TokenKind Op = Tok.getKind();
    SourceLocation OpLoc = ConsumeToken();
    if (Tok.is(tok::kw_range))
      return ParseRangeClauseTail(StartLoc, LHS, OpLoc, Op, OutKind, Ext)
                 ? StmtError()
                 : Actions.StmtEmpty(); // FIXME
    // FIXME: if Op is '=' and the expression a TypeSwitchGuard, provide fixit
    // to turn '=' into ':='.
    ExprVector &LHSs = LHS.Expressions();
    return ParseAssignmentTail(OpLoc, Op, LHSs);
  }

  if (Tok.is(tok::colonequal)) {
    SourceLocation OpLoc = ConsumeToken();
    if (Tok.is(tok::kw_range))
      return ParseRangeClauseTail(StartLoc, LHS, OpLoc, tok::colonequal,
                                  OutKind, Ext)
                 ? StmtError()
                 : Actions.StmtEmpty(); // FIXME
    if (LHS.Kind() != IdentOrExprList::RK_Ident) {
      // FIXME: fixit to change ':=' to '=', but
      // only if Result != Parser::TypeSwitchGuardParam::Parsed 
      Diag(StartLoc, diag::invalid_expr_left_of_colonequal);

      // FIXME: Recover better?
      SkipUntil(tok::semi, tok::l_brace,
                /*StopAtSemi=*/false, /*DontConsume=*/true);
      OptRAII.disarm();
      return StmtError();
    }
    OwningStmtResult Result(ParseShortVarDeclTail(LHS.Identifiers(),
                                                  OpLoc, Opt));
    OptRAII.disarm();
    return Result;
  }

  // At this point, the statement starts with a single expression.
  assert(LHS.Expressions().size() == 1);
  OwningExprResult LHSExpr(Actions, *LHS.Expressions().take());

  if (Tok.is(tok::plusplus) || Tok.is(tok::minusminus))
    return ParseIncDecStmtTail(move(LHSExpr));

  if (Tok.is(tok::lessminus))
    return ParseSendStmtTail(move(LHSExpr));

  if (OutKind)
    *OutKind = SSK_Expression;
  // Note: This can overwrite *OutKind.
  OptRAII.disarm();

  return Actions.ActOnExprStmt(move(LHSExpr));
}

/// This is called after the ':=' has been read.
/// ShortVarDecl = IdentifierList ":=" ExpressionList .
Action::OwningStmtResult
Parser::ParseShortVarDeclTail(IdentifierList &LHSs, SourceLocation OpLoc,
                              TypeSwitchGuardParam *Opt) {
  ExprVector RHSs(Actions);
  ParseExpressionList(RHSs, Opt);
  return Actions.ActOnShortVarDeclStmt(LHSs, OpLoc, move_arg(RHSs));
}

/// This is called after the assign_op has been read.
/// Assignment = ExpressionList assign_op ExpressionList .
/// 
/// assign_op = [ add_op | mul_op ] "=" .
/// The op= construct is a single token.
Action::OwningStmtResult Parser::ParseAssignmentTail(SourceLocation OpLoc,
                                                     tok::TokenKind Op,
                                                     ExprVector &LHSs) {
  assert(IsAssignmentOp(Op) && "expected assignment op");
  ExprVector RHSs(Actions);
  ParseExpressionList(RHSs);
  return Actions.ActOnAssignmentStmt(move_arg(LHSs), OpLoc, Op, move_arg(RHSs));
}

/// This is called after the lhs expression has been read.
/// IncDecStmt = Expression ( "++" | "--" ) .
Action::OwningStmtResult Parser::ParseIncDecStmtTail(OwningExprResult LHS) {
  assert((Tok.is(tok::plusplus) || Tok.is(tok::minusminus)) &&
         "expected '++' or '--'");
  tok::TokenKind OpKind = Tok.getKind();
  SourceLocation OpLoc = ConsumeToken();
  return Actions.ActOnIncDecStmt(move(LHS), OpLoc, OpKind);
}

/// This is called after the channel has been read.
/// SendStmt = Channel "<-" Expression .
/// Channel  = Expression .
Action::OwningStmtResult Parser::ParseSendStmtTail(OwningExprResult LHS) {
  assert(Tok.is(tok::lessminus) && "expected '<-'");
  SourceLocation OpLoc = ConsumeToken();
  OwningExprResult RHS(ParseExpression());
  return Actions.ActOnSendStmt(move(LHS), OpLoc, move(RHS));
}

/// This is called after the label identifier has been read.
/// LabeledStmt = Label ":" Statement .
/// Label       = identifier .
Action::OwningStmtResult Parser::ParseLabeledStmtTail(SourceLocation IILoc,
                                                      IdentifierInfo *II) {
  assert(Tok.is(tok::colon) && "expected ':'");
  SourceLocation ColonLoc = ConsumeToken();
  OwningStmtResult SubStmt(ParseStatement());

  // Broken substmt shouldn't prevent the label from being added to the AST.
  if (SubStmt.isInvalid())
    SubStmt = Actions.ActOnEmptyStmt(ColonLoc);  // FIXME: Test!

  return Actions.ActOnLabeledStmt(IILoc, II, ColonLoc, move(SubStmt));
}

/// An adapter function to parse a Declaration in a statement context.
Action::OwningStmtResult Parser::ParseDeclarationStmt() {
  return Actions.ActOnDeclStmt(ParseDeclaration());
}

/// GoStmt = "go" Expression .
Action::OwningStmtResult Parser::ParseGoStmt() {
  assert(Tok.is(tok::kw_go) && "expected 'go'");
  SourceLocation GoLoc = ConsumeToken();
  OwningExprResult Exp(ParseExpression());
  return Actions.ActOnGoStmt(GoLoc, move(Exp));
}

/// ReturnStmt = "return" [ ExpressionList ] .
Action::OwningStmtResult Parser::ParseReturnStmt() {
  assert(Tok.is(tok::kw_return) && "expected 'return'");
  SourceLocation ReturnLoc = ConsumeToken();

  ExprVector Exprs(Actions);
  if (Tok.isNot(tok::semi))
    ParseExpressionList(Exprs);
  return Actions.ActOnReturnStmt(ReturnLoc, move_arg(Exprs));
}

/// BreakStmt = "break" [ Label ] .
Action::OwningStmtResult Parser::ParseBreakStmt() {
  assert(Tok.is(tok::kw_break) && "expected 'break'");
  SourceLocation BreakLoc = ConsumeToken();

  SourceLocation IILoc;
  IdentifierInfo *II = NULL;
  if (Tok.is(tok::identifier)) {
    II = Tok.getIdentifierInfo();
    IILoc = ConsumeToken();
  }
  return Actions.ActOnBreakStmt(BreakLoc, IILoc, II, getCurScope());
}

/// ContinueStmt = "continue" [ Label ] .
Action::OwningStmtResult Parser::ParseContinueStmt() {
  assert(Tok.is(tok::kw_continue) && "expected 'continue'");
  SourceLocation ContinueLoc = ConsumeToken();

  SourceLocation IILoc;
  IdentifierInfo *II = NULL;
  if (Tok.is(tok::identifier)) {
    II = Tok.getIdentifierInfo();
    IILoc = ConsumeToken();
  }
  return Actions.ActOnContinueStmt(ContinueLoc, IILoc, II, getCurScope());
}

/// GotoStmt = "goto" Label .
Action::OwningStmtResult Parser::ParseGotoStmt() {
  assert(Tok.is(tok::kw_goto) && "expected 'goto'");
  SourceLocation GotoLoc = ConsumeToken();
  if (Tok.isNot(tok::identifier)) {
    Diag(Tok, diag::expected_ident);
    SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
    return StmtError();
  }
  IdentifierInfo *II = Tok.getIdentifierInfo();
  SourceLocation IILoc = ConsumeToken();
  return Actions.ActOnGotoStmt(GotoLoc, IILoc, II);
}

/// FallthroughStmt = "fallthrough" .
Action::OwningStmtResult Parser::ParseFallthroughStmt() {
  assert(Tok.is(tok::kw_fallthrough) && "expected 'fallthrough'");
  SourceLocation FallthroughLoc = ConsumeToken();
  return Actions.ActOnFallthroughStmt(FallthroughLoc);
}

/// IfStmt = "if" [ SimpleStmt ";" ] Expression Block
///          [ "else" ( IfStmt | Block ) ] .
Parser::OwningStmtResult Parser::ParseIfStmt() {
  assert(Tok.is(tok::kw_if) && "expected 'if'");
  SourceLocation IfLoc = ConsumeToken();

  // http://tip.golang.org/ref/spec#Blocks
  // "Each if ... statement is considered to be in its own implicit block."
  ParseScope IfScope(this, Scope::DeclScope);

  CompositeTypeNameLitNeedsParensRAIIObject RequireParens(*this);

  SourceLocation StmtLoc = Tok.getLocation();
  SimpleStmtKind Kind = SSK_Normal;
  OwningStmtResult InitStmt(ParseSimpleStmt(&Kind));
  if (InitStmt.isInvalid())
    return StmtError();

  OwningExprResult CondExp(Actions);  // FIXME: set this
  if (Tok.is(tok::semi)) {
    ConsumeToken();
    ParseExpression();
  } else if (Kind != SSK_Expression) {
    Diag(StmtLoc, diag::expected_expr);
    return StmtError();
  }

  RequireParens.reset();

  // Read the 'then' stmt.
  if (Tok.isNot(tok::l_brace)) {
    Diag(Tok, diag::expected_l_brace);
    return StmtError();
  }
  OwningStmtResult ThenStmt(ParseBlock());

  // If it has an else, parse it.
  SourceLocation ElseLoc;
  OwningStmtResult ElseStmt(Actions);

  if (Tok.is(tok::kw_else)) {
    ElseLoc = ConsumeToken();
    if (Tok.is(tok::kw_if))
      ElseStmt = ParseIfStmt();
    else if (Tok.is(tok::l_brace))
      ElseStmt = ParseBlock();
    else {
      Diag(Tok, diag::expected_if_or_l_brace);
      ElseStmt = StmtError();
    }
  }

  // FIXME: clang has much nicer recovery code here (set invalid branches to
  // empty statements if the other branch and the condition are valid, etc)

  return Actions.ActOnIfStmt(IfLoc, InitStmt, CondExp, ThenStmt, ElseLoc,
                             ElseStmt);
}

/// SwitchStmt = ExprSwitchStmt | TypeSwitchStmt .
/// 
/// ExprSwitchStmt = "switch" [ SimpleStmt ";" ] [ Expression ]
///                  "{" { ExprCaseClause } "}" .
/// 
/// TypeSwitchStmt  = "switch" [ SimpleStmt ";" ] TypeSwitchGuard
///                   "{" { TypeCaseClause } "}" .
/// TypeSwitchGuard = [ identifier ":=" ] PrimaryExpr "." "(" "type" ")" .
Action::OwningStmtResult Parser::ParseSwitchStmt() {
  assert(Tok.is(tok::kw_switch) && "expected 'switch'");
  ConsumeToken();

  // http://tip.golang.org/ref/spec#Blocks
  // "Each ... switch statement is considered to be in its own implicit block."
  ParseScope SwitchScope(this, Scope::DeclScope);

  CompositeTypeNameLitNeedsParensRAIIObject RequireParens(*this);

  bool IsTypeSwitch = false;

  // For ExprSwitchStmts, everything between 'switch and '{' is optional.
  // (This is not true for TypeSwitchStmts.)
  if (Tok.isNot(tok::l_brace)) {
    // FIXME: This is fairly similar to the code in ParseIfStmt. If that's still
    // the case once TypeSwitchStmts work, refactor.
    SourceLocation StmtLoc = Tok.getLocation();
    SimpleStmtKind Kind = SSK_Normal;
    if (ParseSimpleStmt(&Kind, SSE_TypeSwitchGuard).isInvalid()) {
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

  RequireParens.reset();

  if (Tok.isNot(tok::l_brace)) {
    Diag(Tok, diag::expected_l_brace);
    SkipUntil(tok::l_brace, /*StopAtSemi=*/false, /*DontConsume=*/true);
  }

  BalancedDelimiterTracker T(*this, tok::l_brace);
  T.consumeOpen();

  // FIXME: This is fairly similar to the {} code in ParseSelectStmt().
  while (Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
    if (Tok.isNot(tok::kw_case) && Tok.isNot(tok::kw_default)) {
      Diag(Tok, diag::expected_case_or_default);
      SkipUntil(tok::r_brace, /*StopAtSemi=*/false);
      return StmtError();
    }
    ParseScope CaseScope(this, Scope::DeclScope);
    if (ParseCaseClause(IsTypeSwitch ? TypeCaseClause : ExprCaseClause)) {
      SkipUntil(tok::r_brace, /*StopAtSemi=*/false);
      return StmtError();
    }
  }
  T.consumeClose();
  return Actions.StmtEmpty();  // FIXME
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
  ExprVector Exprs(Actions);  // FIXME: use
  switch (Type) {
  case ExprCaseClause: return ParseExpressionList(Exprs);
  case TypeCaseClause: return ParseTypeList();
  }
}

/// SelectStmt = "select" "{" { CommClause } "}" .
Action::OwningStmtResult Parser::ParseSelectStmt() {
  assert(Tok.is(tok::kw_select) && "expected 'select'");
  SourceLocation SelectLoc = ConsumeToken();

  // FIXME: This is somewhat similar to ParseInterfaceType
  if (Tok.isNot(tok::l_brace)) {
    // FIXME: ...after 'select'
    Diag(Tok, diag::expected_l_brace);
    // FIXME: recover?
    return StmtError();
  }
  BalancedDelimiterTracker T(*this, tok::l_brace);
  T.consumeOpen();

  StmtVector Stmts(Actions);
  // FIXME: might be better to run this loop only while (IsCommClause)?
  while (Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
    if (Tok.isNot(tok::kw_case) && Tok.isNot(tok::kw_default)) {
      Diag(Tok, diag::expected_case_or_default);
      SkipUntil(tok::r_brace, /*StopAtSemi=*/false);
      return StmtError();
    }
    ParseScope CommScope(this, Scope::DeclScope);

    OwningStmtResult Stmt(ParseCommClause());
    if (Stmt.isInvalid()) {
      SkipUntil(tok::r_brace, /*StopAtSemi=*/false);
      return StmtError();
    }
    if (Stmt.isUsable())
      Stmts.push_back(Stmt.release());
  }
  T.consumeClose();
  return Actions.ActOnSelectStmt(SelectLoc, T.getOpenLocation(),
                                 T.getCloseLocation(), move_arg(Stmts));
}

/// CommClause = CommCase ":" { Statement ";" } .
Action::OwningStmtResult Parser::ParseCommClause() {
  tok::TokenKind Kind = Tok.getKind();
  SourceLocation KWLoc = Tok.getLocation();
  ParseCommCase();

  SourceLocation ColonLoc;
  if (Tok.isNot(tok::colon)) {
    // FIXME: just add fixit (at least for default and simple CommCases).
    Diag(Tok, diag::expected_colon);
  } else {
    ColonLoc = ConsumeToken();
  }

  // CommClauses are always in SelectStmts, where they can only be followed by
  // other CommClauses or the end of the SelectStmt.
  StmtVector Stmts(Actions);
  while (Tok.isNot(tok::kw_case) && Tok.isNot(tok::kw_default) &&
         Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
    OwningStmtResult Stmt(ParseStatement());
    if (Stmt.isUsable())
      Stmts.push_back(Stmt.release());

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

  if (Kind == tok::kw_default)
    return Actions.ActOnDefaultCommClause(KWLoc, ColonLoc,
                                          move_arg(Stmts));
  assert(Kind == tok::kw_case);
  OwningStmtResult CaseStmt(Actions);  // FIXME: fill in
  return Actions.ActOnCommClause(KWLoc, CaseStmt, ColonLoc, move_arg(Stmts));
}

/// CommCase   = "case" ( SendStmt | RecvStmt ) | "default" .
/// RecvStmt   = [ Expression [ "," Expression ] ( "=" | ":=" ) ] RecvExpr .
// FIXME: RecvStmt   = [ ExpressionList "=" | IdentifierList ":=" ] RecvExpr .
// after https://github.com/golang/go/commit/d3679726b4639c
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

  OwningExprResult LHS = ParseExpression();
  if (Tok.is(tok::lessminus))
    return ParseSendStmtTail(move(LHS)).isInvalid();

  // This is a RecvStmt.
  bool FoundAssignOp = false;
  if (Tok.is(tok::comma)) {
    IdentOrExprList Exprs(Actions, getCurScope(), move(LHS));
    // FIXME: RecvStmt allows just one optional further Expr, no full exprlist.
    ParseExpressionListTail(Exprs);

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
Action::OwningStmtResult Parser::ParseForStmt() {
  assert(Tok.is(tok::kw_for) && "expected 'for'");
  SourceLocation ForLoc = ConsumeToken();
  return ParseForStmtTail(ForLoc);
}

// This is called after the identifier "while" has been read if the token after
// it cannot possibly be valid as an expression.  In that case, try to recover
// by treating "while" as a typo for "for".
Action::OwningStmtResult Parser::ParseWhileAsForStmt(SourceLocation WhileLoc) {
  Diag(WhileLoc, diag::there_is_no_while)
      << FixItHint::CreateReplacement(SourceRange(WhileLoc, WhileLoc), "for");
  return ParseForStmtTail(WhileLoc);
}

// This is called after the 'for' keyword of a ForStmt has been read.
Action::OwningStmtResult Parser::ParseForStmtTail(SourceLocation ForLoc) {
  // http://tip.golang.org/ref/spec#Blocks
  // "Each for ... statement is considered to be in its own implicit block."
  ParseScope ForScope(this, Scope::DeclScope);

  CompositeTypeNameLitNeedsParensRAIIObject RequireParens(*this);

  if (Tok.is(tok::l_brace)) {
    RequireParens.reset();
    OwningExprResult Expr(Actions);
    return Actions.ActOnSimpleForStmt(ForLoc, Expr, ParseBlock());
  }

  OwningStmtResult Init(Actions);
  OwningExprResult Expr(Actions);  // FIXME: fill in

  SourceLocation StmtLoc = Tok.getLocation();
  SimpleStmtKind Kind = SSK_Normal;
  if (Tok.isNot(tok::semi)) {
    // FIXME: this could return an expression too.
    Init = ParseSimpleStmt(&Kind, SSE_RangeClause);
    if (Init.isInvalid())
      return StmtError();
  }

  OwningStmtResult Post(Actions);
  SourceLocation FirstSemiLoc, SecondSemiLoc, RangeLoc;

  if (Kind == SSK_RangeClause) {
    // Range clause, nothing more to do.
  } else if (Tok.is(tok::semi)) {
    // ForClause case
    FirstSemiLoc = ConsumeToken();  // Consume 1st ';'.
    if (Tok.isNot(tok::semi))
      ParseExpression();
    if (Tok.isNot(tok::semi)) {
      Diag(Tok, diag::expected_semi);
      // FIXME: recover?
      return StmtError();
    }
    SecondSemiLoc = ConsumeToken();  // Consume 2nd ';'.
    if (Tok.isNot(tok::l_brace)) {
      ParseSimpleStmt();
    }
    // FIXME: Collect errors above.
  } else if (Kind != SSK_Expression) {
    Diag(StmtLoc, diag::expected_expr);
    // FIXME: recover?
  } else {
    // Single expression
  }

  if (Tok.isNot(tok::l_brace)) {
    Diag(Tok, diag::expected_l_brace);
    return StmtError();
  }
  RequireParens.reset();
  OwningStmtResult Body(ParseBlock());

  // FIXME: error handling?
 
  if (Kind == SSK_Expression)
    return Actions.ActOnSimpleForStmt(ForLoc, Expr, Body);
  if (Kind == SSK_RangeClause) {
    // FIXME
    OwningExprResult OptLHSExpr(Actions);
    OwningExprResult RHSExpr(Actions);
    return Actions.ActOnRangeForStmt(ForLoc, Expr, SourceLocation(), OptLHSExpr,
                                     SourceLocation(), tok::equal, RangeLoc,
                                     RHSExpr, Body);
  }
  return Actions.ActOnForStmt(ForLoc, Init, FirstSemiLoc, Expr, SecondSemiLoc,
                              Post, Body);
}

/// This is called when Tok points at "range".
/// RangeClause = Expression [ "," Expression ] ( "=" | ":=" )
///               "range" Expression .
// FIXME: RangeClause = ( ExpressionList "=" | IdentifierList ":=" )
//                "range" Expression .
// after https://github.com/golang/go/commit/d3679726b4639c
bool Parser::ParseRangeClauseTail(SourceLocation StartLoc, IdentOrExprList &LHS,
                                  SourceLocation OpLoc, tok::TokenKind Op,
                                  SimpleStmtKind *OutKind,
                                  SimpleStmtExts Exts) {
  assert(Tok.is(tok::kw_range) && "expected 'range'");
  SourceLocation RangeLoc = ConsumeToken();

  // FIXME: return an invalid statement in this case.
  bool Failed = false;
  if (Exts != SSE_RangeClause) {
    Diag(RangeLoc, diag::range_only_valid_in_for);
    Failed = true;
  }
  if (Op != tok::equal && Op != tok::colonequal) {
    Diag(OpLoc, diag::expected_colonequal_or_equal);
    Failed = true;
  }

  if (OutKind)
    *OutKind = SSK_RangeClause;

  // Parse expression following `range`.
  OwningExprResult Exp(ParseExpression());

  // FIXME: wrap Exp in SelectExpr node.

  if (Op == tok::colonequal) {
    if (LHS.Kind() == IdentOrExprList::RK_Ident) {
      // FIXME: Use a dedicated ActOn method here.
      ExprVector Exprs(Actions);
      Exprs.push_back(Exp.release());
      return Actions
          .ActOnShortVarDeclStmt(LHS.Identifiers(), OpLoc, move_arg(Exprs))
          .isInvalid();
    } else {
      // FIXME: fixit to change ':=' to '='
      Diag(StartLoc, diag::invalid_expr_left_of_colonequal);
      Failed = true;
    }
  }

  return Exp.isInvalid();
}

/// DeferStmt = "defer" Expression .
Action::OwningStmtResult Parser::ParseDeferStmt() {
  assert(Tok.is(tok::kw_defer) && "expected 'defer'");
  SourceLocation DeferLoc = ConsumeToken();
  OwningExprResult Exp(ParseExpression());
  return Actions.ActOnDeferStmt(DeferLoc, move(Exp));
}

/// EmptyStmt = .
Action::OwningStmtResult Parser::ParseEmptyStmt() {
  assert(Tok.is(tok::semi) && "Expected ';'");
  return Actions.ActOnEmptyStmt(Tok.getLocation());
}

/// Block = "{" { Statement ";" } "}" .
Action::OwningStmtResult Parser::ParseBlock() {
  // Enter a scope to hold everything within the block.
  ParseScope CompoundScope(this, Scope::DeclScope);
  return ParseBlockBody();
}

Action::OwningStmtResult Parser::ParseBlockBody() {
  assert(Tok.is(tok::l_brace) && "Expected '{'");

  BalancedDelimiterTracker T(*this, tok::l_brace);
  T.consumeOpen();

  StmtVector Stmts(Actions);

  // FIXME: scoping, better recovery, check IsStatment() first,
  //        semicolon insertion after last statement
  // See Parser::ParseCompoundStatementBody() in clang.
  while (Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
    OwningStmtResult R(ParseStatement());
    if (R.isUsable())
      Stmts.push_back(R.release());
    // A semicolon may be omitted before a closing ')' or '}'.
    if (Tok.is(tok::r_brace))
      break;
    ExpectAndConsumeSemi(diag::expected_semi);
  }

  SourceLocation CloseLoc = Tok.getLocation();
  if (!T.consumeClose())
    // Recover by creating a compound statement with what we parsed so far,
    // instead of dropping everything and returning StmtError();
    CloseLoc = T.getCloseLocation();

  return Actions.ActOnBlockStmt(T.getOpenLocation(), CloseLoc, move_arg(Stmts));
}
