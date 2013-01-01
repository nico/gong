//===--- ParseExpr.cpp - Expression Parsing -------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Provides the Expression parsing implementation.
///
//===----------------------------------------------------------------------===//

#include "gong/Parse/Parser.h"
#include "llvm/Support/ErrorHandling.h"
#include "RAIIObjectsForParser.h"
//#include "llvm/ADT/SmallVector.h"
//#include "llvm/ADT/SmallString.h"
using namespace gong;

/// \brief Return the precedence of the specified binary operator token.
static prec::Level getBinOpPrecedence(tok::TokenKind Kind) {
  switch (Kind) {
  default:                        return prec::Unknown;
  case tok::pipepipe:             return prec::LogicalOr;
  case tok::ampamp:               return prec::LogicalAnd;

  case tok::equalequal:
  case tok::exclaimequal:
  case tok::less:
  case tok::lessequal:
  case tok::greater:
  case tok::greaterequal:         return prec::Equality;

  case tok::plus:
  case tok::minus:
  case tok::pipe:
  case tok::caret:                return prec::Additive;

  case tok::star:
  case tok::slash:
  case tok::percent:
  case tok::lessless:
  case tok::greatergreater:
  case tok::amp:
  case tok::ampcaret:             return prec::Multiplicative;
  }
}

/// Expression = UnaryExpr | Expression binary_op UnaryExpr .
/// binary_op  = "||" | "&&" | rel_op | add_op | mul_op .
/// rel_op     = "==" | "!=" | "<" | "<=" | ">" | ">=" .
/// add_op     = "+" | "-" | "|" | "^" .
/// mul_op     = "*" | "/" | "%" | "<<" | ">>" | "&" | "&^" .
Parser::OwningExprResult
Parser::ParseExpression(TypeSwitchGuardParam *TSGOpt, TypeParam *TOpt,
                        bool *SawIdentifierOnly) {
  OwningExprResult LHS = ParseUnaryExpr(TSGOpt, TOpt, SawIdentifierOnly);
  return ParseRHSOfBinaryExpression(move(LHS), prec::Lowest, TSGOpt,
                                    SawIdentifierOnly);
}

/// This is called for expressions that start with an identifier, after the
/// initial identifier has been read.
Parser::OwningExprResult
Parser::ParseExpressionTail(IdentifierInfo *II, TypeSwitchGuardParam *TSGOpt,
                            bool *SawIdentifierOnly) {
  OwningExprResult LHS = ParsePrimaryExprTail(II, SawIdentifierOnly);
  LHS = ParsePrimaryExprSuffix(move(LHS), TSGOpt, SawIdentifierOnly);
  return ParseRHSOfBinaryExpression(move(LHS), prec::Lowest, TSGOpt,
                                    SawIdentifierOnly);
}

Parser::OwningExprResult
Parser::ParseRHSOfBinaryExpression(OwningExprResult LHS, prec::Level MinPrec,
                                   TypeSwitchGuardParam *TSGOpt,
                                   bool *SawIdentifierOnly) {
  prec::Level NextTokPrec = getBinOpPrecedence(Tok.getKind());

  while (1) {
    // If this token has a lower precedence than we are allowed to parse (e.g.
    // because we are called recursively, or because the token is not a binop),
    // then we are done!
    if (NextTokPrec < MinPrec)
      return LHS;

    if (SawIdentifierOnly)
      *SawIdentifierOnly = false;
    if (TSGOpt)
      TSGOpt->Reset(*this);

    // Consume the operator, saving the operator token for error reporting.
    Token OpToken = Tok;
    ConsumeToken();

    // Parse another leaf here for the RHS of the operator.
    OwningExprResult RHS = ParseUnaryExpr();

    if (RHS.isInvalid())
      LHS = ExprError();
    
    // Remember the precedence of this operator and get the precedence of the
    // operator immediately to the right of the RHS.
    prec::Level ThisPrec = NextTokPrec;
    NextTokPrec = getBinOpPrecedence(Tok.getKind());

    // Get the precedence of the operator to the right of the RHS.  If it binds
    // more tightly with RHS than we do, evaluate it completely first.
    if (ThisPrec < NextTokPrec) {
      // Only parse things on the RHS that bind more tightly than the current
      // operator.  It is okay to bind exactly as tightly.  For example,
      // compile A+B+C+D as A+(B+(C+D)), where each paren is a level of
      // recursion here.  The function takes ownership of the RHS.
      RHS = ParseRHSOfBinaryExpression(
          move(RHS), static_cast<prec::Level>(ThisPrec + 1), NULL, NULL);

      if (RHS.isInvalid())
        LHS = ExprError();

      NextTokPrec = getBinOpPrecedence(Tok.getKind());
    }
    assert(NextTokPrec <= ThisPrec && "Recursion didn't work!");

    if (!LHS.isInvalid()) {
      // Combine the LHS and RHS into the LHS (e.g. build AST).
      LHS = Actions.ActOnBinaryOp(move(LHS), OpToken.getLocation(),
                                  OpToken.getKind(), move(RHS));
    }
  }
}

bool Parser::IsUnaryOp(tok::TokenKind Kind) {
  switch (Kind) {
  default:
    return false;
  case tok::plus:
  case tok::minus:
  case tok::exclaim:
  case tok::caret:
  case tok::star:
  case tok::amp:
  case tok::lessminus:
    return true;
  }
}

/// UnaryExpr  = PrimaryExpr | unary_op UnaryExpr .
/// unary_op   = "+" | "-" | "!" | "^" | "*" | "&" | "<-" .
Action::OwningExprResult
Parser::ParseUnaryExpr(TypeSwitchGuardParam *TSGOpt, TypeParam *TOpt,
                       bool *SawIdentifierOnly) {
  tok::TokenKind OpKind = Tok.getKind();
  
  // FIXME: * and <- if TOpt is set.
  if (IsUnaryOp(OpKind)) {
    if (SawIdentifierOnly)
      *SawIdentifierOnly = false;
    SourceLocation OpLoc = ConsumeToken();

    OwningExprResult Res = ParseUnaryExpr(NULL, TOpt, NULL);
    if (!Res.isInvalid())
      Res = Actions.ActOnUnaryOp(OpLoc, OpKind, move(Res));
    return Res;
  }
  return ParsePrimaryExpr(TSGOpt, TOpt, SawIdentifierOnly);
}

/// PrimaryExpr =
///   Operand |
///   Conversion |
///   BuiltinCall |
///   PrimaryExpr Selector |
///   PrimaryExpr Index |
///   PrimaryExpr Slice |
///   PrimaryExpr TypeAssertion |
///   PrimaryExpr Call .
/// Operand    = Literal | OperandName | MethodExpr | "(" Expression ")" .
/// Literal    = BasicLit | CompositeLit | FunctionLit .
/// OperandName = identifier | QualifiedIdent.
Action::OwningExprResult
Parser::ParsePrimaryExpr(TypeSwitchGuardParam *TSGOpt, TypeParam *TOpt,
                         bool *SawIdentifierOnly) {
  OwningExprResult Res(Actions);

  if (SawIdentifierOnly && Tok.isNot(tok::identifier))
    *SawIdentifierOnly = false;

  switch (Tok.getKind()) {
  default: return ExprError();
  case tok::numeric_literal:
  case tok::rune_literal:
  case tok::string_literal:
    Res = ParseBasicLit();
    break;
  case tok::kw_struct:
  case tok::l_square:
  case tok::kw_map:
    Res = ParseCompositeLitOrConversion(TOpt);
    break;
  case tok::kw_func:
    Res = ParseFunctionLitOrConversion(TOpt);
    break;
  case tok::identifier: {
    IdentifierInfo *II = Tok.getIdentifierInfo();
    ConsumeToken();
    Res = ParsePrimaryExprTail(II, SawIdentifierOnly);
    break;
  }
  case tok::kw_chan:
  case tok::kw_interface:
    Res = ParseConversion(TOpt);
    break;
  case tok::l_paren: {
    BalancedDelimiterTracker T(*this, tok::l_paren);
    T.consumeOpen();
    /// This handles PrimaryExprs that start with '('. This happens in these
    /// cases:
    ///   1. The '(' Expression ')' production in Operand.
    ///   2. The '(' Type ')' production in Type, at the beginning of a
    ///      Conversion.
    ///   3. The '(' '*' TypeName ')' production in Operand's MethodExpr.
    TypeParam TypeOpt;
    //assert(false);
    Res = ParseExpression(TSGOpt, &TypeOpt);
    T.consumeClose();

    if (!Res.isInvalid())
      Res = Actions.ActOnParenExpr(T.getOpenLocation(), move(Res),
                                   T.getCloseLocation());
    
    // If ParseExpression() parsed a type and this is not a context that accepts
    // types, check that the type is followed by a '(' to produce a Conversion
    // expression.
    if (!TOpt) {
      switch (TypeOpt.Kind) {
      case TypeParam::EK_Type:
        // FIXME: This is mostly duplicated from ParseConversion().
        if (Tok.isNot(tok::l_paren)) {
          Diag(Tok, diag::expected_l_paren);
          Res = ExprError();
        }
        break;
      default:; // FIXME FIXME
      }
    } else {
      TOpt->Kind = TypeOpt.Kind;
    }
    break;
  }
  }
  return ParsePrimaryExprSuffix(move(Res), TSGOpt, SawIdentifierOnly);
}

/// This is called if the first token in a PrimaryExpression was an identifier,
/// after that identifier has been read.
Action::OwningExprResult
Parser::ParsePrimaryExprTail(IdentifierInfo *II, bool *SawIdentifierOnly) {
  // FIXME: Requiring this classification from the Action interface in the limit
  // means that MinimalAction needs to do module loading, which is probably
  // undesirable for most non-Sema clients.  Consider doing something like
  // ParseTypeOrExpr() instead which parses the superset of both, and hand the
  // result on to Action.
  // -> Since this is now done anyways, this could just always accept typelits
  //    and let Sema sort things out after the fact.


  // If the next token is a '{', then this is a CompositeLit starting with a
  // TypeName. (Expressions can't be followed by '{', so this can be done
  // unconditionally for all IIs.)
  // It's possible that II isn't known to be a type, for example if the
  // type declaration happens later at file scope, or is in a different file of
  // the same package.
  if (Tok.is(tok::l_brace) && !CompositeTypeNameLitNeedsParens) {
    if (SawIdentifierOnly)
      *SawIdentifierOnly = false;
    return ParseLiteralValue();
  }

  Action::IdentifierInfoType IIT =
      Actions.classifyIdentifier(*II, getCurScope());
  switch (IIT) {
  case Action::IIT_Package:
    // For things in packages, perfect parser-level handling requires loading 
    // the package binary and looking up if the identifier is a type.  Instead,
    // always allow type literals and clean them up in sema.
    // For statements that introduce new bindings, like `packagename := 4`, it
    // doesn't matter that a name refers to a package -- in this case don't
    // expect a period.
    if (Tok.is(tok::period)) {
      ConsumeToken();
      ExpectAndConsume(tok::identifier, diag::expected_ident);
    }
    if (Tok.is(tok::l_brace) && !CompositeTypeNameLitNeedsParens) {
      if (SawIdentifierOnly)
        *SawIdentifierOnly = false;
      return ParseLiteralValue();
    }
    break;
  case Action::IIT_Type:
    // If the next token is a '.', this is a MethodExpr. While semantically not
    // completey correct, ParsePrimaryExprSuffix() will handle this fine.

    // If the next token is a '(', then this is a Conversion. While semantically
    // not correct, ParsePrimaryExprSuffix()'s call parsing will parse this.
    // (It'll accept too much though: Conversions get exactly one expr, but
    // ParsePrimaryExprSuffix() will accept 0-n.)
    break;
  case Action::IIT_BuiltinFunc: {
    // FIXME: It looks like gc just always accepts types in calls and lets
    //        sema sort things out. With that change, builtins could be handled
    //        by the normal ParseCallSuffix() function. (Except that a trailing
    //        '...' isn't permitted in BuiltinCalls, but see golang bug 4479)
    /// BuiltinCall = identifier "(" [ BuiltinArgs [ "," ] ] ")" .
    /// BuiltinArgs = Type [ "," ExpressionList ] | ExpressionList .

    if (Tok.isNot(tok::l_paren)) {
      // FIXME: MinimalAction doesn't handle `len := 4` correctly and complains
      // about shadowed builtins too.  Until that's fixed (or until this whole
      // method is killed), just allow builtin names to be used in general
      // expressions -- it's possible they are shadowed and aren't a builtin
      // at the moment.
      //Diag(Tok, diag::expected_l_paren_after_builtin);
      //return true;
      break;
    }
    if (SawIdentifierOnly)
      *SawIdentifierOnly = false;
    BalancedDelimiterTracker T(*this, tok::l_paren);
    T.consumeOpen();
    if (Tok.isNot(tok::r_paren)) {
      TypeParam TypeOpt;
      // No builtin returns an interface type, so they can't be followed by
      // '.(type)'.
      // FIXME This is not true if the builtin is shadowed.
      OwningExprResult LHS = ParseExpression(NULL, &TypeOpt);
      ExprVector Exprs(Actions);
      ParseExpressionListTail(move(LHS), NULL, Exprs);
      if (Tok.is(tok::ellipsis))
        ConsumeToken();
      if (Tok.is(tok::comma))
        ConsumeToken();
    }
    if (T.consumeClose())
      return ExprError();
    break;
  }
  case Action::IIT_Const:
  case Action::IIT_Func:
  case Action::IIT_Unknown:
  case Action::IIT_Var:
    // Only "Var" and "Const" make much sense here, "Func" and "Unknown" are
    // even cause for a diagnostic. But that's a Sema-level thing really.
    // If the next token is a '.', this will be handled by the regular
    // suffix-parsing in ParsePrimaryExprSuffix().
    break;
  }
  return Actions.ExprEmpty();  // FIXME
}

/// Conversion = Type "(" Expression ")" .
Action::OwningExprResult
Parser::ParseConversion(TypeParam *TOpt) {
  ParseType();
  if (Tok.is(tok::l_paren)) {
    if (TOpt)
      TOpt->Kind = TypeParam::EK_Expr;
    return ParseConversionTail();
  }
  if (!TOpt) {
    Diag(Tok, diag::expected_l_paren);
    return ExprError();
  }
  // FIXME: TypeName, identifier, etc?
  TOpt->Kind = TypeParam::EK_Type;
  return ExprError();
}

/// This is called after the Type in a Conversion has been read.
Action::OwningExprResult
Parser::ParseConversionTail() {
  assert(Tok.is(tok::l_paren) && "expected '('");
  BalancedDelimiterTracker T(*this, tok::l_paren);
  T.consumeOpen();
  ParseExpression();
  if (T.consumeClose())
    return ExprError();
  return Actions.ExprEmpty();  // FIXME
}

Action::OwningExprResult
Parser::ParsePrimaryExprSuffix(OwningExprResult LHS,
                               TypeSwitchGuardParam *TSGOpt,
                               bool *SawIdentifierOnly) {
  while (1) {
    switch (Tok.getKind()) {
    default:  // Not a postfix-expression suffix.
      return LHS;
    case tok::period: {  // Selector or TypeAssertion
      if (SawIdentifierOnly)
        *SawIdentifierOnly = false;
      if (TSGOpt)
        TSGOpt->Reset(*this);
      LHS = ParseSelectorOrTypeAssertionOrTypeSwitchGuardSuffix(move(LHS),
                                                                TSGOpt);
      break;
    }
    case tok::l_square: {  // Index or Slice
      if (SawIdentifierOnly)
        *SawIdentifierOnly = false;
      if (TSGOpt)
        TSGOpt->Reset(*this);
      LHS = ParseIndexOrSliceSuffix(move(LHS));
      break;
    }
    case tok::l_paren: {  // Call
      if (SawIdentifierOnly)
        *SawIdentifierOnly = false;
      if (TSGOpt)
        TSGOpt->Reset(*this);
      LHS = ParseCallSuffix(move(LHS));
      break;
    }
    }
  }
}

/// Selector       = "." identifier .
/// TypeAssertion  = "." "(" Type ")" .
Action::OwningExprResult
Parser::ParseSelectorOrTypeAssertionOrTypeSwitchGuardSuffix(
    OwningExprResult LHS, TypeSwitchGuardParam *TSGOpt) {
  assert(Tok.is(tok::period) && "expected '.'");
  ConsumeToken();

  bool AllowTypeKeyword = TSGOpt != NULL;

  SourceLocation PrevLoc = PrevTokLocation;

  // Selector
  if (Tok.is(tok::identifier)) {
    ConsumeToken();
    return LHS;
  }

  // TypeAssertion
  if(Tok.is(tok::l_paren)) {
    BalancedDelimiterTracker T(*this, tok::l_paren);
    T.consumeOpen();

    if (Tok.is(tok::kw_type)) {
      if (!AllowTypeKeyword)
        Diag(PrevLoc, diag::unexpected_kw_type);
      else if (TSGOpt)
        TSGOpt->Set(PrevLoc);
      ConsumeToken();
    } else {
      if (!IsType()) {
        Diag(Tok, diag::expected_type);
        T.skipToEnd();
        return ExprError();
      }
      ParseType();
    }

    if (T.consumeClose())
      return ExprError();
    return LHS;
  }

  Diag(Tok, diag::expected_ident_or_l_paren);
  return ExprError();
}

/// Index          = "[" Expression "]" .
/// Slice          = "[" [ Expression ] ":" [ Expression ] "]" .
Action::OwningExprResult
Parser::ParseIndexOrSliceSuffix(OwningExprResult LHS) {
  assert(Tok.is(tok::l_square) && "expected '['");
  BalancedDelimiterTracker T(*this, tok::l_square);
  T.consumeOpen();

  if (Tok.is(tok::r_square)) {
    Diag(Tok, diag::expected_expr_or_colon);
    T.consumeClose();
    return ExprError();
  }

  OwningExprResult FirstExpr(Actions);
  if (Tok.isNot(tok::colon)) {
    FirstExpr = ParseExpression();

    if (Tok.is(tok::r_square)) {
      // It's an Index expression.
      T.consumeClose();
      if (LHS.isInvalid() || FirstExpr.isInvalid())
        return ExprError();
      return Actions.ActOnIndexExpr(LHS, T.getOpenLocation(), FirstExpr,
                                    T.getCloseLocation());
    }
    if (Tok.isNot(tok::colon)) {
      Diag(Tok, diag::expected_r_square_or_colon);
      return ExprError();
    }
  }

  // It's a Slice expression, and the current token is ':'.
  assert(Tok.is(tok::colon));
  SourceLocation ColonLoc = ConsumeToken();

  OwningExprResult SecondExpr(Actions);
  if (Tok.isNot(tok::r_square))
    SecondExpr = ParseExpression();

  if (T.consumeClose() || LHS.isInvalid() || FirstExpr.isInvalid() ||
      SecondExpr.isInvalid())
    return ExprError();

  return Actions.ActOnSliceExpr(LHS, T.getOpenLocation(), FirstExpr, ColonLoc,
                                SecondExpr, T.getCloseLocation());
}

/// Call           = "(" [ ArgumentList [ "," ] ] ")" .
/// ArgumentList   = ExpressionList [ "..." ] .
Action::OwningExprResult
Parser::ParseCallSuffix(OwningExprResult LHS) {
  assert(Tok.is(tok::l_paren) && "expected '('");
  BalancedDelimiterTracker T(*this, tok::l_paren);
  T.consumeOpen();

  ExprVector Args(Actions);
  SourceLocation EllipsisLoc;
  SourceLocation TrailingCommaLoc;
  if (Tok.isNot(tok::r_paren)) {
    ParseExpressionList(Args);
    // FIXME: fixit for "4..." that that's parsed as float, not as [4, ellipsis]
    if (Tok.is(tok::ellipsis))
      EllipsisLoc = ConsumeToken();
    if (Tok.is(tok::comma))
      TrailingCommaLoc = ConsumeToken();
  }
  if (T.consumeClose())
    return ExprError();
  return Actions.ActOnCallExpr(move(LHS), T.getOpenLocation(), move_arg(Args),
                               EllipsisLoc, TrailingCommaLoc,
                               T.getCloseLocation());
}

/// BasicLit   = int_lit | float_lit | imaginary_lit | char_lit | string_lit .
Action::OwningExprResult
Parser::ParseBasicLit() {
  assert((Tok.is(tok::numeric_literal) || Tok.is(tok::rune_literal) ||
        Tok.is(tok::string_literal)) && "Unexpected basic literal start");
  ConsumeAnyToken();
  return Actions.ExprEmpty();  // FIXME
}

/// CompositeLit  = LiteralType LiteralValue .
/// LiteralType   = StructType | ArrayType | "[" "..." "]" ElementType |
///                 SliceType | MapType | TypeName .
Action::OwningExprResult
Parser::ParseCompositeLitOrConversion(TypeParam *TOpt) {
  // FIXME: TypeName lits (Tok.is(tok::identifier)
  assert((Tok.is(tok::kw_struct) || Tok.is(tok::l_square) ||
        Tok.is(tok::kw_map)) && "Unexpected composite literal start");

  bool WasEllipsisArray = false;

  switch (Tok.getKind()) {
  default: llvm_unreachable("unexpected token kind");
  case tok::kw_struct:
    if (ParseStructType())
      return ExprError();
    break;
  case tok::l_square: {
    BalancedDelimiterTracker T(*this, tok::l_square);
    T.consumeOpen();
    if (Tok.is(tok::ellipsis)) {
      WasEllipsisArray = true;
      ConsumeToken();
      if (Tok.isNot(tok::r_square))
        Diag(Tok, diag::expected_r_square);
      else
        ParseSliceType(T);
    } else if (Tok.is(tok::r_square))
      ParseSliceType(T);
    else
      ParseArrayType(T);
    break;
  }
  case tok::kw_map:
    if (ParseMapType())
      return ExprError();
    break;
  }

  if (!WasEllipsisArray && Tok.is(tok::l_paren))
    return ParseConversionTail();
  if (Tok.is(tok::l_brace))
    return ParseLiteralValue();
  if (!WasEllipsisArray && TOpt) {
    TOpt->Kind = TypeParam::EK_Type;
    return Actions.ExprEmpty();  // FIXME
  }

  // FIXME: ...after 'literal type'
  Diag(Tok, WasEllipsisArray ? diag::expected_l_brace :
                               diag::expected_l_brace_or_l_paren);
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return ExprError();
}

/// LiteralValue  = "{" [ ElementList [ "," ] ] "}" .
Action::OwningExprResult
Parser::ParseLiteralValue() {
  assert(Tok.is(tok::l_brace) && "expected '{'");
  BalancedDelimiterTracker T(*this, tok::l_brace);
  T.consumeOpen();

  if (Tok.isNot(tok::r_brace)) {
    ParseElementList();
    if (Tok.is(tok::comma))
      ConsumeToken();
  }

  if (T.consumeClose())
    return ExprError();
  return Actions.ExprEmpty();  // FIXME
}

/// ElementList   = Element { "," Element } .
Action::OwningExprResult
Parser::ParseElementList() {
  ParseElement();
  while (Tok.is(tok::comma)) {
    ConsumeToken();
    ParseElement();
  }
  return Actions.ExprEmpty();  // FIXME
}

/// Element       = [ Key ":" ] Value .
/// Key           = FieldName | ElementIndex .
/// FieldName     = identifier .
/// ElementIndex  = Expression .
/// Value         = Expression | LiteralValue .
Action::OwningExprResult
Parser::ParseElement() {
  IdentifierInfo *FieldName = NULL;
  if (Tok.is(tok::identifier)) {
    IdentifierInfo *II = Tok.getIdentifierInfo();
    ConsumeToken();
    if (Tok.isNot(tok::colon)) {
      ParseExpressionTail(II);
    } else {
      FieldName = II;
    }
  } else if (Tok.isNot(tok::l_brace)) {
    // FIXME: check that this is a valid expression start.
    ParseExpression();
  }

  if (Tok.is(tok::colon)) {
    // Key was present.
    ConsumeToken();
    // Need to parse Expression or LiteralValue for Value.
    if (Tok.is(tok::l_brace))
      ParseLiteralValue();
    else
      ParseExpression();
  } else {
    // No key. Need to use already-parsed Expression for Value, or parse
    // a LiteralValue.
    if (Tok.is(tok::l_brace))
      ParseLiteralValue();
  }
  return Actions.ExprEmpty();  // FIXME
}

/// FunctionLit = FunctionType Body .
Action::OwningExprResult
Parser::ParseFunctionLitOrConversion(TypeParam *TOpt) {
  assert(Tok.is(tok::kw_func) && "expected 'func'");
  if (ParseFunctionType())
    return ExprError();

  if (Tok.is(tok::l_brace)) {
    // FunctionLit
    return ParseBody() ? ExprError() : Actions.ExprEmpty();  // FIXME
  } else if (Tok.is(tok::l_paren)) {
    // Conversion
    return ParseConversionTail();
  } else if (TOpt) {
    TOpt->Kind = TypeParam::EK_Type;
    return ExprError();  // FIXME
  } else {
    Diag(Tok, diag::expected_l_brace_or_l_paren);
    return ExprError();
  }
}


/// MethodExpr    = ReceiverType "." MethodName .
/// ReceiverType  = TypeName | "(" "*" TypeName ")" .

/// ExpressionList = Expression { "," Expression } .
Action::OwningExprResult
Parser::ParseExpressionList(ExprListTy &Exprs, TypeSwitchGuardParam *TSGOpt) {
  OwningExprResult LHS = ParseExpression(TSGOpt);
  if (Tok.is(tok::comma) && TSGOpt)
    TSGOpt->Reset(*this);
  return ParseExpressionListTail(move(LHS), NULL, Exprs);
}

/// This is called after the initial Expression in ExpressionList has been read.
Action::OwningExprResult
Parser::ParseExpressionListTail(OwningExprResult LHS, bool *SawIdentifiersOnly,
                                ExprListTy &Exprs) {
  Exprs.push_back(LHS.release());

  while (Tok.is(tok::comma)) {
    ConsumeToken();

    // FIXME: Diag if Tok doesn't start an expression.

    if (Tok.isNot(tok::identifier)) {
      if (SawIdentifiersOnly)
        *SawIdentifiersOnly = false;
      ParseExpression();
      continue;
    }

    IdentifierInfo *II = Tok.getIdentifierInfo();
    ConsumeToken();
    ParseExpressionTail(II, NULL, SawIdentifiersOnly);
  }
  return Actions.ExprEmpty();  // FIXME?
}
