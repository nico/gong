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
Parser::ExprResult
Parser::ParseExpression(TypeSwitchGuardParam *TSGOpt, TypeParam *TOpt) {
  ExprResult LHS = ParseUnaryExpr(TSGOpt, TOpt);
  return ParseRHSOfBinaryExpression(LHS, prec::Lowest, TSGOpt);
}

/// This is called for expressions that start with an identifier, after the
/// initial identifier has been read.
Parser::ExprResult
Parser::ParseExpressionTail(IdentifierInfo *II, TypeSwitchGuardParam *TSGOpt) {
  ExprResult LHS = ParsePrimaryExprTail(II);
  LHS = ParsePrimaryExprSuffix(LHS, TSGOpt);
  return ParseRHSOfBinaryExpression(LHS, prec::Lowest, TSGOpt);
}

Parser::ExprResult
Parser::ParseRHSOfBinaryExpression(ExprResult LHS, prec::Level MinPrec,
                                   TypeSwitchGuardParam *TSGOpt) {
  prec::Level NextTokPrec = getBinOpPrecedence(Tok.getKind());
  SourceLocation ColonLoc;

  while (1) {
    // If this token has a lower precedence than we are allowed to parse (e.g.
    // because we are called recursively, or because the token is not a binop),
    // then we are done!
    if (NextTokPrec < MinPrec)
      return LHS;

    if (TSGOpt)
      TSGOpt->Reset(*this);

    // Consume the operator, saving the operator token for error reporting.
    //Token OpToken = Tok;  // FIXME
    ConsumeToken();

    // Parse another leaf here for the RHS of the operator.
    ExprResult RHS = ParseUnaryExpr();

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
          RHS, static_cast<prec::Level>(ThisPrec + 1), NULL);

      if (RHS.isInvalid())
        LHS = ExprError();

      NextTokPrec = getBinOpPrecedence(Tok.getKind());
    }
    assert(NextTokPrec <= ThisPrec && "Recursion didn't work!");

    if (!LHS.isInvalid()) {
      // Combine the LHS and RHS into the LHS (e.g. build AST).
      //LHS = Actions.ActOnBinOp(getCurScope(), OpToken.getLocation(),
                               //OpToken.getKind(), LHS.take(), RHS.take());
    }
  }
}

bool Parser::IsUnaryOp() {
  switch (Tok.getKind()) {
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
Action::ExprResult
Parser::ParseUnaryExpr(TypeSwitchGuardParam *TSGOpt, TypeParam *TOpt) {
  // FIXME: * and <- if TOpt is set.
  if (IsUnaryOp()) {
    TSGOpt = NULL;
    ConsumeToken();  // FIXME: use
  }
  return ParsePrimaryExpr(TSGOpt, TOpt);
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
Action::ExprResult
Parser::ParsePrimaryExpr(TypeSwitchGuardParam *TSGOpt, TypeParam *TOpt) {
  ExprResult Res;

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
    Res = ParsePrimaryExprTail(II);
    break;
  }
  case tok::kw_chan:
  case tok::kw_interface:
    Res = ParseConversion(TOpt);
    break;
  case tok::l_paren: {
    // FIXME: here

    //FIXME:
    // *: type or deref or conversion or methodexpr
    // <-: type or conversion or expression
    // identifier: Tricky! ParsePrimaryExprTail(), but accept types too.

    ConsumeParen();
    /// This handles PrimaryExprs that start with '('. This happens in these
    /// cases:
    ///   1. The '(' Expression ')' production in Operand.
    ///   2. The '(' Type ')' production in Type, at the beginning of a
    ///      Conversion.
    ///   3. The '(' '*' TypeName ')' production in Operand's MethodExpr.
    TypeParam TypeOpt;
    //assert(false);
    Res = ParseExpression(TSGOpt, &TypeOpt);
    ExpectAndConsume(tok::r_paren, diag::expected_r_paren);
    
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
  return ParsePrimaryExprSuffix(Res, TSGOpt);
}

/// This is called if the first token in a PrimaryExpression was an identifier,
/// after that identifier has been read.
Action::ExprResult
Parser::ParsePrimaryExprTail(IdentifierInfo *II) {
  // FIXME: Requiring this classification from the Action interface in the limit
  // means that MinimalAction needs to do module loading, which is probably
  // undesirable for most non-Sema clients.  Consider doing something like
  // ParseTypeOrExpr() instead which parses the superset of both, and hand the
  // result on to Action.
  // -> Since this is now done anyways, this could just always accept typelits
  //    and let Sema sort things out after the fact.
  Action::IdentifierInfoType IIT =
      Actions.classifyIdentifier(*II, getCurScope());
  switch (IIT) {
  case Action::IIT_Package:
    // For things in packages, perfect parser-level handling requires loading 
    // the package binary and looking up if the identifier is a type.  Instead,
    // always allow type literals and clean them up in sema.
    ExpectAndConsume(tok::period, diag::expected_period);
    ExpectAndConsume(tok::identifier, diag::expected_ident);
    if (Tok.is(tok::l_brace) /*FIXME: && !CompositeTypeNameLitNeedsParens*/)
      return ParseLiteralValue();
    break;
  case Action::IIT_Type:
    // If the next token is a '.', this is a MethodExpr. While semantically not
    // completey correct, ParsePrimaryExprSuffix() will handle this fine.

    // If the next token is a '(', then this is a Conversion. While semantically
    // not correct, ParsePrimaryExprSuffix()'s call parsing will parse this.
    // (It'll accept too much though: Conversions get exactly one expr, but
    // ParsePrimaryExprSuffix() will accept 0-n.)

    // If the next token is a '{', the this is a CompositeLit starting with a
    // TypeName. (Expressions can't be followed by '{', so this could be done
    // unconditionally for all IIs.)
    if (Tok.is(tok::l_brace) /*FIXME: && !CompositeTypeNameLitNeedsParens*/)
      return ParseLiteralValue();
    break;
  case Action::IIT_BuiltinFunc: {
    // FIXME: It looks like gc just always accepts types in calls and lets
    //        sema sort things out. With that change, builtins could be handled
    //        by the normal ParseCallSuffix() function. (Except that a trailing
    //        '...' isn't permitted in BuiltinCalls, but see golang bug 4479)
    /// BuiltinCall = identifier "(" [ BuiltinArgs [ "," ] ] ")" .
    /// BuiltinArgs = Type [ "," ExpressionList ] | ExpressionList .

// FIXME: here

    if (Tok.isNot(tok::l_paren)) {
      Diag(Tok, diag::expected_l_paren_after_builtin);
      return true;
    }
    ConsumeParen();
    if (Tok.isNot(tok::r_paren)) {
      TypeParam TypeOpt;
      // No builtin returns an interface type, so they can't be followed by
      // '.(type)'.
      ExprResult LHS = ParseExpression(NULL, &TypeOpt);
      ParseExpressionListTail(LHS);
      if (Tok.is(tok::comma))
        ConsumeToken();
    }
    if (Tok.isNot(tok::r_paren)) {
      Diag(Tok, diag::expected_r_paren);
      SkipUntil(tok::r_paren);
      return ExprError();
    }
    ConsumeParen();
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
  return false;
}

/// Conversion = Type "(" Expression ")" .
Action::ExprResult
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
Action::ExprResult
Parser::ParseConversionTail() {
  assert(Tok.is(tok::l_paren) && "expected '('");
  ConsumeParen();
// FIXME: here
  ParseExpression();
  if (Tok.isNot(tok::r_paren)) {
    Diag(Tok, diag::expected_r_paren);
    SkipUntil(tok::r_paren);
    return ExprError();
  }
  ConsumeParen();
  return false;
}

Action::ExprResult
Parser::ParsePrimaryExprSuffix(ExprResult &LHS, TypeSwitchGuardParam *TSGOpt) {
  while (1) {
    switch (Tok.getKind()) {
    default:  // Not a postfix-expression suffix.
      return LHS;
    case tok::period: {  // Selector or TypeAssertion
      if (TSGOpt)
        TSGOpt->Reset(*this);
      LHS = ParseSelectorOrTypeAssertionOrTypeSwitchGuardSuffix(LHS, TSGOpt);
      break;
    }
    case tok::l_square: {  // Index or Slice
      if (TSGOpt)
        TSGOpt->Reset(*this);
      LHS = ParseIndexOrSliceSuffix(LHS);
      break;
    }
    case tok::l_paren: {  // Call
// FIXME: here
      if (TSGOpt)
        TSGOpt->Reset(*this);
      LHS = ParseCallSuffix(LHS);
      break;
    }
    }
  }
}

/// Selector       = "." identifier .
/// TypeAssertion  = "." "(" Type ")" .
Action::ExprResult
Parser::ParseSelectorOrTypeAssertionOrTypeSwitchGuardSuffix(
    ExprResult &LHS, TypeSwitchGuardParam *TSGOpt) {
  assert(Tok.is(tok::period) && "expected '.'");
  ConsumeToken();

  bool AllowTypeKeyword = TSGOpt != NULL;

  SourceLocation PrevLoc = PrevTokLocation;

  if (Tok.is(tok::identifier)) {
    ConsumeToken();
    return LHS;
  } else if(Tok.is(tok::l_paren)) {
// FIXME: here
    ConsumeParen();

    if (Tok.is(tok::kw_type)) {
      if (!AllowTypeKeyword)
        Diag(PrevLoc, diag::unexpected_kw_type);
      else if (TSGOpt)
        TSGOpt->Set(PrevLoc);
      ConsumeToken();
    } else {
      if (!IsType()) {
        Diag(Tok, diag::expected_type);
        SkipUntil(tok::r_paren);
        return ExprError();
      }
      ParseType();
    }

    if (Tok.is(tok::r_paren))
      ConsumeParen();
    else {
      Diag(Tok, diag::expected_r_paren);
      SkipUntil(tok::r_paren);
      return ExprError();
    }
    return LHS;
  } else {
    Diag(Tok, diag::expected_ident_or_l_paren);
    return ExprError();
  }
}

/// Index          = "[" Expression "]" .
/// Slice          = "[" [ Expression ] ":" [ Expression ] "]" .
Action::ExprResult
Parser::ParseIndexOrSliceSuffix(ExprResult &LHS) {
  assert(Tok.is(tok::l_square) && "expected '['");
  BalancedDelimiterTracker T(*this, tok::l_square);
  T.consumeOpen();

  // FIXME: here

  if (Tok.is(tok::r_square)) {
    Diag(Tok, diag::expected_expr);
    T.consumeClose();
    return ExprError();
  }

  if (Tok.isNot(tok::colon))
    ParseExpression();

  if (Tok.is(tok::colon)) {
    ConsumeToken();
    if (Tok.isNot(tok::r_square))
      ParseExpression();
  }

  // FIXME: This prints "expected ']'", but a ':' is sometimes ok too
  // (after "[1" for example).
  T.consumeClose();
  return LHS;
}

/// Call           = "(" [ ArgumentList [ "," ] ] ")" .
/// ArgumentList   = ExpressionList [ "..." ] .
Action::ExprResult
Parser::ParseCallSuffix(ExprResult &LHS) {
  assert(Tok.is(tok::l_paren) && "expected '('");
  ConsumeParen();
// FIXME: here
  if (Tok.isNot(tok::r_paren)) {
    ParseExpressionList();
    if (Tok.is(tok::ellipsis))
      ConsumeToken();
    if (Tok.is(tok::comma))
      ConsumeToken();
  }
  if (Tok.isNot(tok::r_paren)) {
    Diag(Tok, diag::expected_r_paren);
    SkipUntil(tok::r_paren);
    return ExprError();
  }
  ConsumeParen();
  return LHS;
}

/// BasicLit   = int_lit | float_lit | imaginary_lit | char_lit | string_lit .
Action::ExprResult
Parser::ParseBasicLit() {
  assert((Tok.is(tok::numeric_literal) || Tok.is(tok::rune_literal) ||
        Tok.is(tok::string_literal)) && "Unexpected basic literal start");
  ConsumeAnyToken();
  return false;
}

/// CompositeLit  = LiteralType LiteralValue .
/// LiteralType   = StructType | ArrayType | "[" "..." "]" ElementType |
///                 SliceType | MapType | TypeName .
Action::ExprResult
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
    // FIXME: here
    ConsumeBracket();
    if (Tok.is(tok::ellipsis)) {
      WasEllipsisArray = true;
      ConsumeToken();
      if (Tok.isNot(tok::r_square))
        Diag(Tok, diag::expected_r_square);
      else
        ParseSliceType();
    } else if (Tok.is(tok::r_square))
      ParseSliceType();
    else
      ParseArrayType();
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
    return false;
  }

  // FIXME: ...after 'literal type'
  Diag(Tok, WasEllipsisArray ? diag::expected_l_brace :
                               diag::expected_l_brace_or_l_paren);
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

/// LiteralValue  = "{" [ ElementList [ "," ] ] "}" .
Action::ExprResult
Parser::ParseLiteralValue() {
  assert(Tok.is(tok::l_brace) && "expected '{'");
  BalancedDelimiterTracker T(*this, tok::l_brace);
  T.consumeOpen();

  if (Tok.isNot(tok::r_brace)) {
    ParseElementList();
    if (Tok.is(tok::comma))
      ConsumeToken();
  }

  return T.consumeClose();
}

/// ElementList   = Element { "," Element } .
Action::ExprResult
Parser::ParseElementList() {
  ParseElement();
  while (Tok.is(tok::comma)) {
    ConsumeToken();
    ParseElement();
  }
  return true;
}

/// Element       = [ Key ":" ] Value .
/// Key           = FieldName | ElementIndex .
/// FieldName     = identifier .
/// ElementIndex  = Expression .
/// Value         = Expression | LiteralValue .
Action::ExprResult
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
  return false;
}

/// FunctionLit = FunctionType Body .
Action::ExprResult
Parser::ParseFunctionLitOrConversion(TypeParam *TOpt) {
  assert(Tok.is(tok::kw_func) && "expected 'func'");
  if (ParseFunctionType())
    return ExprError();

  if (Tok.is(tok::l_brace)) {
    // FunctionLit
    return ParseBody();
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
Action::ExprResult
Parser::ParseExpressionList(TypeSwitchGuardParam *TSGOpt) {
  ExprResult LHS = ParseExpression(TSGOpt);
  if (Tok.is(tok::comma) && TSGOpt)
    TSGOpt->Reset(*this);
  return ParseExpressionListTail(LHS);
}

/// This is called after the initial Expression in ExpressionList has been read.
Action::ExprResult
Parser::ParseExpressionListTail(ExprResult &LHS) {
  while (Tok.is(tok::comma)) {
    ConsumeToken();
    // FIXME: Diag if Tok doesn't start an expression.
    ParseExpression();
  }
  return LHS;
}
