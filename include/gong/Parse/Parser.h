//===--- Parser.h - C Language Parser ---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Parser interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_PARSE_PARSER_H
#define LLVM_GONG_PARSE_PARSER_H

#include "gong/Basic/OperatorPrecedence.h"
//#include "gong/Basic/Specifiers.h"
#include "gong/Lex/Lexer.h"
#include "gong/Parse/Action.h"
#include "gong/Parse/IdentifierList.h"
//#include "llvm/ADT/OwningPtr.h"
//#include "llvm/ADT/SmallVector.h"
//#include "llvm/Support/Compiler.h"
#include "llvm/Support/PrettyStackTrace.h"
//#include <stack>

namespace gong {
  class Scope;
  class BalancedDelimiterTracker;
  //class CorrectionCandidateCallback;
  //class DiagnosticBuilder;
  class Parser;
  //class ParsingDeclRAIIObject;
  //class ParsingDeclSpec;
  //class ParsingDeclarator;
  //class ParsingFieldDeclarator;
  class CompositeTypeNameLitNeedsParensRAIIObject;

/// PrettyStackTraceParserEntry - If a crash happens while the parser is active,
/// an entry is printed for it.
class PrettyStackTraceParserEntry : public llvm::PrettyStackTraceEntry {
  const Parser &P;
public:
  PrettyStackTraceParserEntry(const Parser &p) : P(p) {}
  virtual void print(raw_ostream &OS) const;
};

/// Parser - This implements a parser for the C family of languages.  After
/// parsing units of the grammar, productions are invoked to handle whatever has
/// been read.
///
class Parser /*: public CodeCompletionHandler */ {
  friend class CompositeTypeNameLitNeedsParensRAIIObject;
  friend class ParenBraceBracketBalancer;
  friend class BalancedDelimiterTracker;
  PrettyStackTraceParserEntry CrashInfo;

  Lexer &L;

  /// Tok - The current token we are peeking ahead.  All parsing methods assume
  /// that this is valid.
  Token Tok;

  // PrevTokLocation - The location of the token we previously
  // consumed. This token is used for diagnostics where we expected to
  // see a token following another token (e.g., the ';' at the end of
  // a statement).
  SourceLocation PrevTokLocation;

  unsigned short ParenCount, BracketCount, BraceCount;
  
  /// Actions - These are the callbacks we invoke as we parse various constructs
  /// in the file.
  Action &Actions;

  DiagnosticsEngine &Diags;

  /// ScopeCache - Cache scopes to reduce malloc traffic.
  enum { ScopeCacheSize = 16 };
  unsigned NumCachedScopes;
  Scope *ScopeCache[ScopeCacheSize];

  /// \brief Identifier for "while".
  IdentifierInfo *Ident_while;

  //bool SkipFunctionBodies;

  // This is set while parsing the text between an if/switch/for and the '{'
  // that starts the body.  This is set to true through
  // CompositeTypeNameLitNeedsParensRAIIObjects in ParseIf/Switch/For, and then
  // back to false in parenthesized expressions by BalancedDelimiterTracker.
  bool CompositeTypeNameLitNeedsParens;

public:
  Parser(Lexer &L, Action &Actions/*, bool SkipFunctionBodies*/);
  ~Parser();

  //const LangOptions &getLangOpts() const { return PP.getLangOpts(); }
  //const TargetInfo &getTargetInfo() const { return PP.getTargetInfo(); }
  Lexer &getLexer() const { return L; }
  //Sema &getActions() const { return Actions; }

  const Token &getCurToken() const { return Tok; }
  Scope *getCurScope() const { return Actions.getCurScope(); }

  // Type forwarding.  All of these are statically 'void*', but they may all be
  // different actual classes based on the actions in place.
  //typedef OpaquePtr<DeclGroupRef> DeclGroupPtrTy;

  //typedef SmallVector<TemplateParameterList *, 4> TemplateParameterLists;

  typedef Action::ExprResult        ExprResult;
  typedef Action::StmtResult        StmtResult;
  typedef Action::BaseResult        BaseResult;
  typedef Action::MemInitResult     MemInitResult;
  typedef Action::TypeResult        TypeResult;

  typedef Action::OwningExprResult OwningExprResult;
  typedef Action::OwningStmtResult OwningStmtResult;
  typedef Action::OwningDeclResult OwningDeclResult;

  //typedef Expr *ExprArg;
  //typedef llvm::MutableArrayRef<Stmt*> MultiStmtArg;
  //typedef Sema::FullExprArg FullExprArg;

  /// Adorns a ExprResult with Actions to make it an OwningExprResult
  OwningExprResult Owned(ExprResult res) {
    return OwningExprResult(Actions, res);
  }
  /// Adorns a StmtResult with Actions to make it an OwningStmtResult
  OwningStmtResult Owned(StmtResult res) {
    return OwningStmtResult(Actions, res);
  }

  OwningExprResult ExprError() { return OwningExprResult(Actions, true); }
  OwningStmtResult StmtError() { return OwningStmtResult(Actions, true); }
  OwningDeclResult DeclError() { return OwningDeclResult(Actions, true); }

  //OwningExprResult ExprError(const DiagnosticBuilder &) { return ExprError(); }
  //OwningStmtResult StmtError(const DiagnosticBuilder &) { return StmtError(); }

  OwningExprResult ExprEmpty() { return OwningExprResult(Actions, false); }

  // FIXME: Remove again when no longer needed.
  OwningDeclResult DeclEmpty() { return OwningDeclResult(Actions, false); }

  // Parsing methods.

  /// ParseSourceFile - All in one method that initializes parses, and
  /// shuts down the parser.
  void ParseSourceFile();

  /// Initialize - Warm up the parser.
  ///
  void Initialize();

  /// Parse the package clause.  Returns true on error.
  bool ParsePackageClause();

  /// Parse one import decl.  Returns true on error.
  bool ParseImportDecl();

  /// Parse one import spec.  Returns true on error.
  bool ParseImportSpec();

  /// Parse one top-level declaration.  Returns true if the EOF was encountered.
  bool ParseTopLevelDecl(/*DeclGroupPtrTy &Result*/);

  bool ParseFunctionOrMethodDecl();
  bool ParseFunctionDecl(SourceLocation FuncLoc);
  bool ParseMethodDecl();
  bool ParseBody() { return ParseBlock().isInvalid(); }

  /// Parses a function or method signature.
  bool ParseSignature();
  bool ParseResult();
  bool ParseParameters();
  bool IsParameterList();
  bool ParseParameterList();
  bool ParseParameterDecl();
  bool ParseReceiver();

  OwningDeclResult ParseType();
  OwningDeclResult ParseTypeName();
  OwningDeclResult ParseTypeNameTail(SourceLocation IILoc,
                                     IdentifierInfo *Head);
  OwningDeclResult ParseTypeLit();
  OwningDeclResult ParseArrayOrSliceType();
  OwningDeclResult ParseArrayType(BalancedDelimiterTracker &T);
  OwningDeclResult ParseSliceType(BalancedDelimiterTracker &T);
  OwningDeclResult ParseStructType();
  OwningDeclResult ParseFieldDecl();
  OwningDeclResult ParseAnonymousField();
  OwningDeclResult ParseAnonymousFieldTail(SourceLocation StarLoc,
                                           SourceLocation IILoc,
                                           IdentifierInfo *II);
  OwningDeclResult ParsePointerType();
  bool ParseFunctionType();
  OwningDeclResult ParseInterfaceType();
  OwningDeclResult ParseMethodSpec();
  OwningDeclResult ParseMapType();
  OwningDeclResult ParseChannelType();
  bool IsElementType() { return IsType(); }
  OwningDeclResult ParseElementType();
  bool ParseTypeList();
  // A ParameterDecl starting with an IdentifierList looks the same to the
  // parser as a ParameterList where each ParameterDecl is just a TypeName.
  // The token after the list differentiates the two cases:
  //   func f(a, b, c int   < "a, b, c" is an identifier list
  //   func f(a, b, c)      < "a, b, c" are TypeNames in a ParameterList
  // This class stores either an IdentifierList or a TypeList and provides a
  // function to convert an IdentifierList to a TypeList if needed.
  class IdentOrTypeList {
  public:
    // The kind of value this instance stores.
    enum ResultKind { RK_Type, RK_Ident };
  private:
    Action &Actions;
    Scope *S;
    IdentifierList Idents;  // Only valid if Kind == RK_Ident
    DeclVector Tys;  // Only valid if Kind == RK_Type
    ResultKind ResKind;

    // Converts an IdentifierList IdentOrTypeList to a TypeList.
    void ToTypes() {
      if (ResKind == RK_Type)
        return;
      ResKind = RK_Type;
      ArrayRef<IdentifierInfo*> IdentsRef = Idents.getIdents();
      ArrayRef<SourceLocation> IILocs = Idents.getIdentLocs();
      for (unsigned i = 0; i < IdentsRef.size(); ++i) {
        OwningDeclResult R(
            Actions.ActOnTypeName(IILocs[i], *IdentsRef[i], S));
        Tys.push_back(R.release());
      }
    }

  public:
    // Constructs an empty IdentOrTypeList.
    IdentOrTypeList(Action &Actions, Scope *S)
      : Actions(Actions), S(S), Tys(Actions), ResKind(RK_Ident) {}

    void AddType(OwningDeclResult Type) {
      if (ResKind != RK_Type)
        ToTypes();
      Tys.push_back(Type.release());
    }

    void AddFirstIdent(SourceLocation IILoc, IdentifierInfo *II) {
      assert(Tys.size() == 0);
      Idents.initialize(IILoc, II);
    }

    void AddIdent(SourceLocation CommaLoc, SourceLocation IILoc,
                  IdentifierInfo *II) {
      if (ResKind == RK_Ident)
        Idents.add(CommaLoc, IILoc, II);
      else {
        OwningDeclResult R(Actions.ActOnTypeName(IILoc, *II, S));
        Tys.push_back(R.release());
      }
    }

    ResultKind Kind() {
      return ResKind;
    }
    DeclVector &Types() {
      ToTypes();
      return Tys;
    }
    IdentifierList &Identifiers() {
      assert(ResKind == RK_Ident);
      return Idents;
    }
  };
  bool ParseTypeListTail(IdentOrTypeList &R, bool AcceptEllipsis = false);

  // This is called after an identifier has been read. It returns if the next
  // token could follow an identifier list in an IdentifierList production.
  bool IsPossiblyIdentifierList() {
    // The only place where IdentifierLists can be confused with
    // ExpressionLists is at the start of a ShortVarDecl.  At this point, a
    // single identifier has been parsed.  If the next token is not a ',' or
    // ':=', then it is certain that the identifier was meant to be the start
    // of an expression.
    return Tok.is(tok::comma) || Tok.is(tok::colonequal);
  }
  bool ParseIdentifierList(IdentifierList &IdentList);
  bool ParseIdentifierListTail(IdentifierList &IdentList);

  // FIXME: These should probably return OwningDeclResult too.
  Action::DeclPtrTy ParseDeclaration();
  Action::DeclPtrTy ParseConstDecl();
  bool ParseConstSpec(Action::DeclPtrTy ConstDecl);
  Action::DeclPtrTy ParseTypeDecl();
  bool ParseTypeSpec(Action::DeclPtrTy TypeDecl);
  Action::DeclPtrTy ParseVarDecl();
  bool ParseVarSpec(Action::DeclPtrTy VarDecl);
  Action::DeclPtrTy ParseDeclGroup(DeclGroupKind Kind, SourceLocation KWLoc);

  bool IsType();
  bool IsExpression();

  // Expressions

  /// If \a TOpt points to a TypeParam, then the expression parser
  /// will allow types in addition to expressions.
  /// TOpt.Kind will be set to EK_Type if a type was parsed.
  /// This is needed to parse the first parenthesized tokens in an expression
  /// like |([]int)(4)| and |((interface{}))(4)|.
  struct TypeParam {
    enum {
      EK_Expr,
      //EK_Identifier,
      EK_Type
      //EK_TypeName,
      //EK_StarTypeName
    } Kind;
    IdentifierInfo *II;

    TypeParam() : Kind(EK_Expr) {}
  };

  /// If \a TSGOpt points to a TypeSwitchGuardParam, then the expression parser
  /// will allow a trailing '.(type)' if a PrimaryExpr was parsed.
  /// TSGOpt.Result will be set to Parsed if that happend or to NotParsed in all
  /// other cases.
  struct TypeSwitchGuardParam {
    enum  { NotParsed, Parsed } Result;
    SourceLocation TSGLoc;  ///< Points at the '.'.

    TypeSwitchGuardParam() : Result(NotParsed) {}
    void Set(SourceLocation Loc) { Result = Parsed; TSGLoc = Loc; }
    void Reset(Parser& Self) {
      if (Result != Parsed)
        return;
      Self.Diag(TSGLoc, diag::unexpected_kw_type);
      Result = NotParsed;
    }
  };

  //FIXME: Also, now that this accepts types, ExprResult doesn't make much sense
  OwningExprResult ParseExpression(TypeSwitchGuardParam *TSGOpt = NULL,
                                   TypeParam *TOpt = NULL);
  OwningExprResult ParseExpressionTail(SourceLocation IILoc, IdentifierInfo *II,
                                       TypeSwitchGuardParam *TSGOpt = NULL);
  OwningExprResult ParseRHSOfBinaryExpression(OwningExprResult LHS,
                                              prec::Level MinPrec,
                                              TypeSwitchGuardParam *TSGOpt);
  bool IsUnaryOp(tok::TokenKind Kind);
  OwningExprResult ParseUnaryExpr(TypeSwitchGuardParam *TSGOpt = NULL,
                                  TypeParam *TOpt = NULL);
  OwningExprResult ParsePrimaryExpr(TypeSwitchGuardParam *TSGOpt,
                                    TypeParam *TOpt);
  OwningExprResult ParsePrimaryExprTail(SourceLocation IILoc,
                                        IdentifierInfo *II);
  OwningExprResult ParseConversion(TypeParam *TOpt);
  OwningExprResult ParseConversionTail();
  OwningExprResult ParsePrimaryExprSuffix(OwningExprResult LHS,
                                          TypeSwitchGuardParam *TSGOpt);
  OwningExprResult ParseSelectorOrTypeAssertionOrTypeSwitchGuardSuffix(
      OwningExprResult LHS, TypeSwitchGuardParam *TSGOpt);
  OwningExprResult ParseIndexOrSliceSuffix(OwningExprResult LHS);
  OwningExprResult ParseCallSuffix(OwningExprResult LHS);
  OwningExprResult ParseBasicLit();
  OwningExprResult ParseCompositeLitOrConversion(TypeParam *TOpt);
  OwningExprResult ParseLiteralValue();
  OwningExprResult ParseElementList();
  OwningExprResult ParseElement();
  OwningExprResult ParseFunctionLitOrConversion(TypeParam *TOpt);

  // FIXME: create ExprList class
  typedef Action::ExprTy ExprTy;
  static const unsigned ExprListSize = 12;
  typedef llvm::SmallVector<ExprTy*, ExprListSize> ExprListTy;
  //typedef llvm::SmallVector<SourceLocation, ExprListSize> CommaLocsTy;

  bool ParseExpressionList(ExprListTy &Exprs,
                           TypeSwitchGuardParam *TSGOpt = NULL);

  // A ShortVarDecl starts with an IdentifierList.  Other SimpleStmts start
  // with for example ExpressionLists or expressions starting with a single
  // identifier.  Hence, ParseExpressionListTail() needs to communicate if it
  // has parsed an IdentifierList or an ExpressionList, and if it's an
  // IdentifierList it may need to be converted to an ExpressionList.
  // This class stores either an IdentifierList or an ExpressionList and
  // provides a function to convert an IdentifierList to an ExpressionList if
  // needed.
  class IdentOrExprList {
  public:
    // The kind of value this instance stores.
    enum ResultKind { RK_Expr, RK_Ident };
  private:
    Action &Actions;
    Scope *S;
    IdentifierList Idents;  // Only valid if Kind == RK_Ident
    ExprVector Exprs;  // Only valid if Kind == RK_Expr
    ResultKind ResKind;

    // Converts an IdentifierList IdentOrExprList to an ExpressionList.
    void ToExprs() {
      if (ResKind == RK_Expr)
        return;
      ResKind = RK_Expr;
      ArrayRef<IdentifierInfo*> IdentsRef = Idents.getIdents();
      ArrayRef<SourceLocation> IILocs = Idents.getIdentLocs();
      for (unsigned i = 0; i < IdentsRef.size(); ++i) {
        OwningExprResult R(
            Actions.ActOnOperandName(IILocs[i], IdentsRef[i], S));
        Exprs.push_back(R.release());
      }
    }

  public:
    // Constructs an IdentOrExprList that contains an expression.
    IdentOrExprList(Action &Actions, Scope *S, OwningExprResult Head)
      : Actions(Actions), S(S), Exprs(Actions), ResKind(RK_Expr) {
      Exprs.push_back(Head.release());
    }

    // Constructs an IdentOrExprList that contains an identifier.
    IdentOrExprList(Action &Actions, Scope *S,
                    SourceLocation IILoc, IdentifierInfo *II)
      : Actions(Actions), S(S), Exprs(Actions), ResKind(RK_Ident) {
      Idents.initialize(IILoc, II);
    }

    void AddExpr(OwningExprResult Expr) {
      if (ResKind != RK_Expr)
        ToExprs();
      Exprs.push_back(Expr.release());
    }

    void AddIdent(SourceLocation CommaLoc, SourceLocation IILoc,
                  IdentifierInfo *II) {
      if (ResKind == RK_Ident)
        Idents.add(CommaLoc, IILoc, II);
      else {
        OwningExprResult R(Actions.ActOnOperandName(IILoc, II, S));
        Exprs.push_back(R.release());
      }
    }

    ResultKind Kind() {
      return ResKind;
    }
    ExprVector &Expressions() {
      ToExprs();
      return Exprs;
    }
    IdentifierList &Identifiers() {
      assert(ResKind == RK_Ident);
      return Idents;
    }
  };
  bool ParseExpressionListTail(IdentOrExprList &Exprs);


  // Statements
  OwningStmtResult ParseStatement();

  /// This can be passed to ParseSimpleStmt() to tell it to accept additional
  /// constructs.
  enum SimpleStmtExts {
    /// Accept only SimpleStmts.
    SSE_None,

    // In addition to SimpleStmts, also accept RangeClause.
    SSE_RangeClause,

    // In addition to SimpleStmts, also accept TypeSwitchGuard.
    SSE_TypeSwitchGuard,

    // Only accept SendStmts or RecvStmts, for use in a CommCase.
    SSE_SendRecvStmt,
  };
  /// This describes what ParseSimpleStmt() parsed.
  enum SimpleStmtKind {
    /// An unremarkable SimpleStmt.
    SSK_Normal,

    /// A SimpleStmt consisting of a single expression.
    SSK_Expression,

    /// A RangeClause. This will only be set if SSE_RangeClause was passed as
    /// permitted option.
    SSK_RangeClause,

    /// A TypeSwitchGuard. This will only be set if SSE_TypeSwitchGuard was
    /// passed as permitted option.
    SSK_TypeSwitchGuard
  };
  OwningStmtResult ParseSimpleStmt(SimpleStmtKind *OutKind = NULL,
                                   SimpleStmtExts Ext = SSE_None);
  OwningStmtResult ParseSimpleStmtTail(SourceLocation IILoc, IdentifierInfo *II,
                                       SimpleStmtKind *OutKind = NULL,
                                       SimpleStmtExts Ext = SSE_None);
  OwningStmtResult ParseSimpleStmtTailAfterExpression(IdentOrExprList &LHS,
                                                      SourceLocation StartLoc,
                                                   TypeSwitchGuardParam *TSGOpt,
                                                      SimpleStmtKind *OutKind,
                                                      SimpleStmtExts Ext);

  OwningStmtResult ParseShortVarDeclTail(IdentifierList &LHSs,
                                         SourceLocation OpLoc,
                                         TypeSwitchGuardParam *TSGOpt = NULL);
  OwningStmtResult ParseAssignmentTail(SourceLocation OpLoc, tok::TokenKind Op,
                                       ExprVector &LHSs);
  OwningStmtResult ParseIncDecStmtTail(OwningExprResult LHS);
  OwningStmtResult ParseSendStmtTail(OwningExprResult LHS);
  OwningStmtResult ParseLabeledStmtTail(SourceLocation IILoc,
                                        IdentifierInfo *II);
  OwningStmtResult ParseDeclarationStmt();
  OwningStmtResult ParseGoStmt();
  OwningStmtResult ParseReturnStmt();
  OwningStmtResult ParseBreakStmt();
  OwningStmtResult ParseContinueStmt();
  OwningStmtResult ParseGotoStmt();
  OwningStmtResult ParseFallthroughStmt();
  OwningStmtResult ParseIfStmt();
  OwningStmtResult ParseSwitchStmt();
  enum CaseClauseType { ExprCaseClause, TypeCaseClause };
  bool ParseCaseClause(CaseClauseType Type);
  bool ParseSwitchCase(CaseClauseType Type);
  OwningStmtResult ParseSelectStmt();
  OwningStmtResult ParseCommClause();
  bool ParseCommCase();
  OwningStmtResult ParseForStmt();
  OwningStmtResult ParseWhileAsForStmt(SourceLocation WhileLoc);
  OwningStmtResult ParseForStmtTail(SourceLocation ForLoc);
  bool ParseRangeClauseTail(SourceLocation StartLoc, IdentOrExprList &LHS,
                            SourceLocation OpLoc, tok::TokenKind Op,
                            SimpleStmtKind *OutKind, SimpleStmtExts Exts);
  OwningStmtResult ParseDeferStmt();
  OwningStmtResult ParseEmptyStmt();
  OwningStmtResult ParseBlock();
  OwningStmtResult ParseBlockBody();

  /// ConsumeToken - Consume the current 'peek token' and lex the next one.
  /// This does not work with all kinds of tokens: strings and specific other
  /// tokens must be consumed with custom methods below.  This returns the
  /// location of the consumed token.
  SourceLocation ConsumeToken() {
    assert(!isTokenStringLiteral() && !isTokenParen() && !isTokenBracket() &&
           !isTokenBrace() &&
           "Should consume special tokens with Consume*Token");

    //if (Tok.is(tok::code_completion))
      //return handleUnexpectedCodeCompletionToken();

    PrevTokLocation = Tok.getLocation();
    L.Lex(Tok);
    return PrevTokLocation;
  }

private:
  //===--------------------------------------------------------------------===//
  // Low-Level token peeking and consumption methods.
  //

  /// isTokenParen - Return true if the cur token is '(' or ')'.
  bool isTokenParen() const {
    return Tok.getKind() == tok::l_paren || Tok.getKind() == tok::r_paren;
  }
  /// isTokenBracket - Return true if the cur token is '[' or ']'.
  bool isTokenBracket() const {
    return Tok.getKind() == tok::l_square || Tok.getKind() == tok::r_square;
  }
  /// isTokenBrace - Return true if the cur token is '{' or '}'.
  bool isTokenBrace() const {
    return Tok.getKind() == tok::l_brace || Tok.getKind() == tok::r_brace;
  }

  /// isTokenStringLiteral - True if this token is a string-literal.
  ///
  bool isTokenStringLiteral() const {
    return Tok.getKind() == tok::string_literal;
  }

  /// ConsumeAnyToken - Dispatch to the right Consume* method based on the
  /// current token type.  This should only be used in cases where the type of
  /// the token really isn't known, e.g. in error recovery.
  SourceLocation ConsumeAnyToken() {
    if (isTokenParen())
      return ConsumeParen();
    else if (isTokenBracket())
      return ConsumeBracket();
    else if (isTokenBrace())
      return ConsumeBrace();
    else if (isTokenStringLiteral())
      return ConsumeStringToken();
    else
      return ConsumeToken();
  }

  /// ConsumeParen - This consume method keeps the paren count up-to-date.
  ///
  SourceLocation ConsumeParen() {
    assert(isTokenParen() && "wrong consume method");
    if (Tok.getKind() == tok::l_paren)
      ++ParenCount;
    else if (ParenCount)
      --ParenCount;       // Don't let unbalanced )'s drive the count negative.
    PrevTokLocation = Tok.getLocation();
    L.Lex(Tok);
    return PrevTokLocation;
  }

  /// ConsumeBracket - This consume method keeps the bracket count up-to-date.
  ///
  SourceLocation ConsumeBracket() {
    assert(isTokenBracket() && "wrong consume method");
    if (Tok.getKind() == tok::l_square)
      ++BracketCount;
    else if (BracketCount)
      --BracketCount;     // Don't let unbalanced ]'s drive the count negative.

    PrevTokLocation = Tok.getLocation();
    L.Lex(Tok);
    return PrevTokLocation;
  }

  /// ConsumeBrace - This consume method keeps the brace count up-to-date.
  ///
  SourceLocation ConsumeBrace() {
    assert(isTokenBrace() && "wrong consume method");
    if (Tok.getKind() == tok::l_brace)
      ++BraceCount;
    else if (BraceCount)
      --BraceCount;     // Don't let unbalanced }'s drive the count negative.

    PrevTokLocation = Tok.getLocation();
    L.Lex(Tok);
    return PrevTokLocation;
  }

  /// ConsumeStringToken - Consume the current 'peek token', lexing a new one
  /// and returning the token kind.  This method is specific to strings, as it
  /// handles string literal concatenation, as per C99 5.1.1.2, translation
  /// phase #6.
  SourceLocation ConsumeStringToken() {
    assert(isTokenStringLiteral() &&
           "Should only consume string literals with this method");
    PrevTokLocation = Tok.getLocation();
    L.Lex(Tok);
    return PrevTokLocation;
  }

  ///// \brief Consume the current code-completion token.
  /////
  ///// This routine should be called to consume the code-completion token once
  ///// a code-completion action has already been invoked.
  //SourceLocation ConsumeCodeCompletionToken() {
  //  assert(Tok.is(tok::code_completion));
  //  PrevTokLocation = Tok.getLocation();
  //  PP.Lex(Tok);
  //  return PrevTokLocation;
  //}

  /////\ brief When we are consuming a code-completion token without having
  ///// matched specific position in the grammar, provide code-completion results
  ///// based on context.
  /////
  ///// \returns the source location of the code-completion token.
  //SourceLocation handleUnexpectedCodeCompletionToken();

  ///// \brief Abruptly cut off parsing; mainly used when we have reached the
  ///// code-completion point.
  //void cutOffParsing() {
  //  PP.setCodeCompletionReached();
  //  // Cut off parsing by acting as if we reached the end-of-file.
  //  Tok.setKind(tok::eof);
  //}

  /// GetLookAheadToken - This peeks ahead N tokens and returns that token
  /// without consuming any tokens.  LookAhead(0) returns 'Tok', LookAhead(1)
  /// returns the token after Tok, etc.
  ///
  /// Note that this differs from the Preprocessor's LookAhead method, because
  /// the Parser always has one token lexed that the preprocessor doesn't.
  ///
  //const Token &GetLookAheadToken(unsigned N) {
    //if (N == 0 || Tok.is(tok::eof)) return Tok;
    //return PP.LookAhead(N-1);
  //}

public:
  /// NextToken - This peeks ahead one token and returns it without
  /// consuming it.
  //const Token &NextToken() {
    //return PP.LookAhead(0);
  //}

private:
  /// ExpectAndConsume - The parser expects that 'ExpectedTok' is next in the
  /// input.  If so, it is consumed and false is returned.
  ///
  /// If the input is malformed, this emits the specified diagnostic.  Next, if
  /// SkipToTok is specified, it calls SkipUntil(SkipToTok).  Finally, true is
  /// returned.
  bool ExpectAndConsume(tok::TokenKind ExpectedTok, unsigned Diag,
                        const char *DiagMsg = "",
                        tok::TokenKind SkipToTok = tok::unknown);

  /// \brief The parser expects a semicolon and, if present, will consume it.
  ///
  /// If the next token is not a semicolon, this emits the specified diagnostic,
  /// or, if there's just some closing-delimiter noise (e.g., ')' or ']') prior
  /// to the semicolon, consumes that extra token.
  bool ExpectAndConsumeSemi(unsigned DiagID);

  /// \brief The kind of extra semi diagnostic to emit.
  enum ExtraSemiKind {
    OutsideFunction = 0,
    InsideStruct = 1,
    InstanceVariableList = 2,
    AfterMemberFunctionDefinition = 3
  };

  /// \brief Consume any extra semi-colons until the end of the line.
  void ConsumeExtraSemi(ExtraSemiKind Kind/*, unsigned TST = TST_unspecified*/);

public:
  //===--------------------------------------------------------------------===//
  // Scope manipulation

  /// ParseScope - Introduces a new scope for parsing. The kind of
  /// scope is determined by ScopeFlags. Objects of this type should
  /// be created on the stack to coincide with the position where the
  /// parser enters the new scope, and this object's constructor will
  /// create that new scope. Similarly, once the object is destroyed
  /// the parser will exit the scope.
  class ParseScope {
    Parser *Self;
    ParseScope(const ParseScope &) = delete;
    void operator=(const ParseScope &) = delete;

  public:
    // ParseScope - Construct a new object to manage a scope in the
    // parser Self where the new Scope is created with the flags
    // ScopeFlags, but only when ManageScope is true (the default). If
    // ManageScope is false, this object does nothing.
    ParseScope(Parser *Self, unsigned ScopeFlags, bool ManageScope = true)
      : Self(Self) {
      if (ManageScope)
        Self->EnterScope(ScopeFlags);
      else
        this->Self = 0;
    }

    // Exit - Exit the scope associated with this object now, rather
    // than waiting until the object is destroyed.
    void Exit() {
      if (Self) {
        Self->ExitScope();
        Self = 0;
      }
    }

    ~ParseScope() {
      Exit();
    }
  };

  /// EnterScope - Start a new scope.
  void EnterScope(unsigned ScopeFlags);

  /// ExitScope - Pop a scope off the scope stack.
  void ExitScope();

  //===--------------------------------------------------------------------===//
  // Diagnostic Emission and Error recovery.

public:
  DiagnosticBuilder Diag(SourceLocation Loc, unsigned DiagID);
  DiagnosticBuilder Diag(const Token &Tok, unsigned DiagID);
  DiagnosticBuilder Diag(unsigned DiagID) {
    return Diag(Tok, DiagID);
  }

public:
  /// SkipUntil - Read tokens until we get to the specified token, then consume
  /// it (unless DontConsume is true).  Because we cannot guarantee that the
  /// token will ever occur, this skips to the next token, or to some likely
  /// good stopping point.  If StopAtSemi is true, skipping will stop at a ';'
  /// character.
  ///
  /// If SkipUntil finds the specified token, it returns true, otherwise it
  /// returns false.
  bool SkipUntil(tok::TokenKind T, bool StopAtSemi = true,
                 bool DontConsume = false, bool StopAtCodeCompletion = false) {
    return SkipUntil(llvm::makeArrayRef(T), StopAtSemi, DontConsume,
                     StopAtCodeCompletion);
  }
  bool SkipUntil(tok::TokenKind T1, tok::TokenKind T2, bool StopAtSemi = true,
                 bool DontConsume = false, bool StopAtCodeCompletion = false) {
    tok::TokenKind TokArray[] = {T1, T2};
    return SkipUntil(TokArray, StopAtSemi, DontConsume,StopAtCodeCompletion);
  }
  bool SkipUntil(tok::TokenKind T1, tok::TokenKind T2, tok::TokenKind T3,
                 bool StopAtSemi = true, bool DontConsume = false,
                 bool StopAtCodeCompletion = false) {
    tok::TokenKind TokArray[] = {T1, T2, T3};
    return SkipUntil(TokArray, StopAtSemi, DontConsume,StopAtCodeCompletion);
  }
  bool SkipUntil(ArrayRef<tok::TokenKind> Toks, bool StopAtSemi = true,
                 bool DontConsume = false, bool StopAtCodeCompletion = false);

  /// SkipMalformedDecl - Read tokens until we get to some likely good stopping
  /// point for skipping past a simple-declaration.
  void SkipMalformedDecl();

#if 0
public:
  //===--------------------------------------------------------------------===//
  // Preprocessor code-completion pass-through
  virtual void CodeCompleteDirective(bool InConditional);
  virtual void CodeCompleteInConditionalExclusion();
  virtual void CodeCompleteMacroName(bool IsDefinition);
  virtual void CodeCompletePreprocessorExpression();
  virtual void CodeCompleteMacroArgument(IdentifierInfo *Macro,
                                         MacroInfo *MacroInfo,
                                         unsigned ArgumentIndex);
  virtual void CodeCompleteNaturalLanguage();
#endif
};

}  // end namespace gong

#endif
