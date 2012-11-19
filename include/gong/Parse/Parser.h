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

//#include "gong/Basic/Specifiers.h"
#include "gong/Lex/Lexer.h"
#include "gong/Parse/Action.h"
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
  class ColonProtectionRAIIObject;
  class VersionTuple;

/// PrettyStackTraceParserEntry - If a crash happens while the parser is active,
/// an entry is printed for it.
class PrettyStackTraceParserEntry : public llvm::PrettyStackTraceEntry {
  const Parser &P;
public:
  PrettyStackTraceParserEntry(const Parser &p) : P(p) {}
  virtual void print(raw_ostream &OS) const;
};

/// These are precedences for the binary operators in the Go grammar.
/// Low precedences numbers bind more weakly than high numbers.
/// http://golang.org/ref/spec#Operator_precedence
namespace prec {
  enum Level {
    Unknown         = 0,    // Not binary operator.
    LogicalOr       = 1,    // ||
    LogicalAnd      = 2,    // &&
    Equality        = 3,    // ==, !=, <, <=, >, >=
    Additive        = 4,    // +, -, |, ^
    Multiplicative  = 5     // *, /, %, <<, >>, &, , &^
  };
}

/// Parser - This implements a parser for the C family of languages.  After
/// parsing units of the grammar, productions are invoked to handle whatever has
/// been read.
///
class Parser /*: public CodeCompletionHandler */ {
  friend class PragmaUnusedHandler;
  friend class ColonProtectionRAIIObject;
  friend class BalancedDelimiterTracker;

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
  //enum { ScopeCacheSize = 16 };
  //unsigned NumCachedScopes;
  //Scope *ScopeCache[ScopeCacheSize];

  /// \brief Identifier for "message".
  IdentifierInfo *Ident_message;

  /// ColonIsSacred - When this is false, we aggressively try to recover from
  /// code like "foo : bar" as if it were a typo for "foo :: bar".  This is not
  /// safe in case statements and a few other things.  This is managed by the
  /// ColonProtectionRAIIObject RAII object.
  //bool ColonIsSacred;

  //bool SkipFunctionBodies;

public:
  Parser(Lexer &L, Action &Actions/*, bool SkipFunctionBodies*/);
  ~Parser();

  //const LangOptions &getLangOpts() const { return PP.getLangOpts(); }
  //const TargetInfo &getTargetInfo() const { return PP.getTargetInfo(); }
  Lexer &geLexer() const { return L; }
  //Sema &getActions() const { return Actions; }

  const Token &getCurToken() const { return Tok; }
  //Scope *getCurScope() const { return Actions.getCurScope(); }

  // Type forwarding.  All of these are statically 'void*', but they may all be
  // different actual classes based on the actions in place.
  //typedef OpaquePtr<DeclGroupRef> DeclGroupPtrTy;
  //typedef OpaquePtr<TemplateName> TemplateTy;

  //typedef SmallVector<TemplateParameterList *, 4> TemplateParameterLists;

  typedef Action::ExprResult        ExprResult;
  typedef Action::StmtResult        StmtResult;
  typedef Action::BaseResult        BaseResult;
  typedef Action::MemInitResult     MemInitResult;
  typedef Action::TypeResult        TypeResult;

  //typedef Expr *ExprArg;
  //typedef llvm::MutableArrayRef<Stmt*> MultiStmtArg;
  //typedef Sema::FullExprArg FullExprArg;

  /// Adorns a ExprResult with Actions to make it an ExprResult
  ExprResult Owned(ExprResult res) {
    return ExprResult(res);
  }
  /// Adorns a StmtResult with Actions to make it an StmtResult
  StmtResult Owned(StmtResult res) {
    return StmtResult(res);
  }

  ExprResult ExprError() { return ExprResult(true); }
  StmtResult StmtError() { return StmtResult(true); }

  ExprResult ExprError(const DiagnosticBuilder &) { return ExprError(); }
  StmtResult StmtError(const DiagnosticBuilder &) { return StmtError(); }

  ExprResult ExprEmpty() { return ExprResult(false); }

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
  bool ParseFunctionDecl();
  bool ParseMethodDecl();

  /// Parses a function or method signature.
  bool ParseSignature();
  bool ParseResult();
  bool ParseParameters();
  bool IsParameterList();
  bool ParseParameterList();
  bool ParseParameterDecl();
  bool ParseReceiver();

  bool ParseType();
  bool ParseTypeName();
  bool ParseTypeNameTail(IdentifierInfo *Head);
  bool ParseTypeLit();
  bool ParseArrayOrSliceType();
  bool ParseArrayType();
  bool ParseSliceType();
  bool ParseStructType();
  bool ParsePointerType();
  bool ParseFunctionType();
  bool ParseInterfaceType();
  bool ParseMapType();
  bool ParseChannelType();
  bool IsElementType() { return IsType(); }
  bool ParseElementType();

  bool ParseIdentifierList();
  bool ParseIdentifierListTail(IdentifierInfo *Head);

  bool ParseDeclaration();
  bool ParseConstDecl();
  bool ParseTypeDecl();
  bool ParseTypeSpec();
  bool ParseVarDecl();

  bool IsType();

  // Statements
  bool ParseStatement();
  bool ParseGoStmnt();
  bool ParseReturnStmnt();
  bool ParseBreakStmnt();
  bool ParseContinueStmnt();
  bool ParseGotoStmnt();
  bool ParseFallthroughStmnt();
  bool ParseIfStmt();
  bool ParseSwitchStmt();
  bool ParseSelectStmt();
  bool ParseForStmt();
  bool ParseDeferStmt();
  bool ParseEmptyStmt();
  bool ParseBlock();

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

  /// \brief Returns true if the current token is '=' or is a type of '='.
  /// For typos, give a fixit to '='
  bool isTokenEqualOrEqualTypo();

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

#if 0
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
    ParseScope(const ParseScope &) LLVM_DELETED_FUNCTION;
    void operator=(const ParseScope &) LLVM_DELETED_FUNCTION;

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

private:
  /// \brief RAII object used to modify the scope flags for the current scope.
  class ParseScopeFlags {
    Scope *CurScope;
    unsigned OldFlags;
    ParseScopeFlags(const ParseScopeFlags &) LLVM_DELETED_FUNCTION;
    void operator=(const ParseScopeFlags &) LLVM_DELETED_FUNCTION;

  public:
    ParseScopeFlags(Parser *Self, unsigned ScopeFlags, bool ManageFlags = true);
    ~ParseScopeFlags();
#endif

  //===--------------------------------------------------------------------===//
  // Diagnostic Emission and Error recovery.

public:
  DiagnosticBuilder Diag(SourceLocation Loc, unsigned DiagID);
  DiagnosticBuilder Diag(const Token &Tok, unsigned DiagID);
  DiagnosticBuilder Diag(unsigned DiagID) {
    return Diag(Tok, DiagID);
  }

private:
  void SuggestParentheses(SourceLocation Loc, unsigned DK,
                          SourceRange ParenRange);

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
private:

  Decl *ParseFunctionDefinition(ParsingDeclarator &D,
                 const ParsedTemplateInfo &TemplateInfo = ParsedTemplateInfo(),
                 LateParsedAttrList *LateParsedAttrs = 0);


public:
  //===--------------------------------------------------------------------===//
  // C99 6.5: Expressions.

  /// TypeCastState - State whether an expression is or may be a type cast.
  enum TypeCastState {
    NotTypeCast = 0,
    MaybeTypeCast,
    IsTypeCast
  };

  ExprResult ParseExpression(TypeCastState isTypeCast = NotTypeCast);
  // Expr that doesn't include commas.
  ExprResult ParseAssignmentExpression(TypeCastState isTypeCast = NotTypeCast);

private:
  ExprResult ParseRHSOfBinaryExpression(ExprResult LHS,
                                        prec::Level MinPrec);
  ExprResult ParseCastExpression(bool isUnaryExpression,
                                 bool isAddressOfOperand,
                                 bool &NotCastExpr,
                                 TypeCastState isTypeCast);
  ExprResult ParseCastExpression(bool isUnaryExpression,
                                 bool isAddressOfOperand = false,
                                 TypeCastState isTypeCast = NotTypeCast);

  /// Returns true if the next token cannot start an expression.
  bool isNotExpressionStart();

  /// Returns true if the next token would start a postfix-expression
  /// suffix.
  bool isPostfixExpressionSuffixStart() {
    tok::TokenKind K = Tok.getKind();
    return (K == tok::l_square || K == tok::l_paren ||
            K == tok::period || K == tok::arrow ||
            K == tok::plusplus || K == tok::minusminus);
  }

  ExprResult ParsePostfixExpressionSuffix(ExprResult LHS);
  ExprResult ParseUnaryExprOrTypeTraitExpression();
  ExprResult ParseBuiltinPrimaryExpression();

  ExprResult ParseExprAfterUnaryExprOrTypeTrait(const Token &OpTok,
                                                     bool &isCastExpr,
                                                     ParsedType &CastTy,
                                                     SourceRange &CastRange);

  typedef SmallVector<Expr*, 20> ExprListTy;
  typedef SmallVector<SourceLocation, 20> CommaLocsTy;

  /// ParseExpressionList - Used for C/C++ (argument-)expression-list.
  bool ParseExpressionList(SmallVectorImpl<Expr*> &Exprs,
                           SmallVectorImpl<SourceLocation> &CommaLocs,
                           void (Sema::*Completer)(Scope *S,
                                                   Expr *Data,
                                             llvm::ArrayRef<Expr *> Args) = 0,
                           Expr *Data = 0);

  /// ParenParseOption - Control what ParseParenExpression will parse.
  enum ParenParseOption {
    SimpleExpr,      // Only parse '(' expression ')'
    CompoundStmt,    // Also allow '(' compound-statement ')'
    CompoundLiteral, // Also allow '(' type-name ')' '{' ... '}'
    CastExpr         // Also allow '(' type-name ')' <anything>
  };
  ExprResult ParseParenExpression(ParenParseOption &ExprType,
                                        bool stopIfCastExpr,
                                        bool isTypeCast,
                                        ParsedType &CastTy,
                                        SourceLocation &RParenLoc);

  ExprResult ParseCXXAmbiguousParenExpression(ParenParseOption &ExprType,
                                            ParsedType &CastTy,
                                            BalancedDelimiterTracker &Tracker);
  ExprResult ParseCompoundLiteralExpression(ParsedType Ty,
                                                  SourceLocation LParenLoc,
                                                  SourceLocation RParenLoc);

  ExprResult ParseStringLiteralExpression(bool AllowUserDefinedLiteral = false);

  ExprResult ParseGenericSelectionExpression();
  
  ExprResult ParseObjCBoolLiteral();

  //===--------------------------------------------------------------------===//
  // C++ Expressions
  ExprResult ParseCXXIdExpression(bool isAddressOfOperand = false);

  bool areTokensAdjacent(const Token &A, const Token &B);

  void CheckForTemplateAndDigraph(Token &Next, ParsedType ObjectTypePtr,
                                  bool EnteringContext, IdentifierInfo &II,
                                  CXXScopeSpec &SS);

  bool ParseOptionalCXXScopeSpecifier(CXXScopeSpec &SS,
                                      ParsedType ObjectType,
                                      bool EnteringContext,
                                      bool *MayBePseudoDestructor = 0,
                                      bool IsTypename = false);

  void CheckForLParenAfterColonColon();

  //===--------------------------------------------------------------------===//
  // C++ 9.3.2: C++ 'this' pointer
  ExprResult ParseCXXThis();

  /// ParseCXXSimpleTypeSpecifier - [C++ 7.1.5.2] Simple type specifiers.
  /// This should only be called when the current token is known to be part of
  /// simple-type-specifier.
  void ParseCXXSimpleTypeSpecifier(DeclSpec &DS);

  bool ParseCXXTypeSpecifierSeq(DeclSpec &DS);

  //===--------------------------------------------------------------------===//
  // C++ if/switch/while condition expression.
  bool ParseCXXCondition(ExprResult &ExprResult, Decl *&DeclResult,
                         SourceLocation Loc, bool ConvertToBoolean);

  //===--------------------------------------------------------------------===//
  // C++ types

  //===--------------------------------------------------------------------===//
  // gong Expressions

  ExprResult ParseBlockLiteralExpression();  // ^{...}

  //===--------------------------------------------------------------------===//
  // C99 6.8: Statements and Blocks.

  /// A SmallVector of statements, with stack size 32 (as that is the only one
  /// used.)
  typedef SmallVector<Stmt*, 32> StmtVector;
  /// A SmallVector of expressions, with stack size 12 (the maximum used.)
  typedef SmallVector<Expr*, 12> ExprVector;
  /// A SmallVector of types.
  typedef SmallVector<ParsedType, 12> TypeVector;

  StmtResult ParseStatement(SourceLocation *TrailingElseLoc = 0) {
    StmtVector Stmts;
    return ParseStatementOrDeclaration(Stmts, true, TrailingElseLoc);
  }
  StmtResult ParseStatementOrDeclaration(StmtVector &Stmts,
                                         bool OnlyStatement,
                                         SourceLocation *TrailingElseLoc = 0);
  StmtResult ParseStatementOrDeclarationAfterAttributes(
                                         StmtVector &Stmts,
                                         bool OnlyStatement,
                                         SourceLocation *TrailingElseLoc,
                                         ParsedAttributesWithRange &Attrs);
  StmtResult ParseExprStatement();
  StmtResult ParseLabeledStatement(ParsedAttributesWithRange &attrs);
  StmtResult ParseCaseStatement(bool MissingCase = false,
                                ExprResult Expr = ExprResult());
  StmtResult ParseDefaultStatement();
  StmtResult ParseCompoundStatement(bool isStmtExpr = false);
  StmtResult ParseCompoundStatement(bool isStmtExpr,
                                    unsigned ScopeFlags);
  void ParseCompoundStatementLeadingPragmas();
  StmtResult ParseCompoundStatementBody(bool isStmtExpr = false);
  bool ParseParenExprOrCondition(ExprResult &ExprResult,
                                 Decl *&DeclResult,
                                 SourceLocation Loc,
                                 bool ConvertToBoolean);
  StmtResult ParseIfStatement(SourceLocation *TrailingElseLoc);
  StmtResult ParseSwitchStatement(SourceLocation *TrailingElseLoc);
  StmtResult ParseWhileStatement(SourceLocation *TrailingElseLoc);
  StmtResult ParseDoStatement();
  StmtResult ParseForStatement(SourceLocation *TrailingElseLoc);
  StmtResult ParseGotoStatement();
  StmtResult ParseContinueStatement();
  StmtResult ParseBreakStatement();
  StmtResult ParseReturnStatement();
  StmtResult ParseAsmStatement(bool &msAsm);
  StmtResult ParseMicrosoftAsmStatement(SourceLocation AsmLoc);

  //===--------------------------------------------------------------------===//
  // C99 6.7: Declarations.

  /// A context for parsing declaration specifiers.  TODO: flesh this
  /// out, there are other significant restrictions on specifiers than
  /// would be best implemented in the parser.
  enum DeclSpecContext {
    DSC_normal, // normal context
    DSC_class,  // class context, enables 'friend'
    DSC_type_specifier, // C++ type-specifier-seq or C specifier-qualifier-list
    DSC_trailing, // C++11 trailing-type-specifier in a trailing return type
    DSC_top_level // top-level/namespace declaration context
  };

  DeclGroupPtrTy ParseDeclaration(StmtVector &Stmts,
                                  unsigned Context, SourceLocation &DeclEnd,
                                  ParsedAttributesWithRange &attrs);
  DeclGroupPtrTy ParseSimpleDeclaration(StmtVector &Stmts,
                                        unsigned Context,
                                        SourceLocation &DeclEnd,
                                        ParsedAttributesWithRange &attrs,
                                        bool RequireSemi);
  bool MightBeDeclarator(unsigned Context);
  DeclGroupPtrTy ParseDeclGroup(ParsingDeclSpec &DS, unsigned Context,
                                bool AllowFunctionDefinitions,
                                SourceLocation *DeclEnd = 0);
  Decl *ParseDeclarationAfterDeclarator(Declarator &D,
               const ParsedTemplateInfo &TemplateInfo = ParsedTemplateInfo());
  Decl *ParseDeclarationAfterDeclaratorAndAttributes(Declarator &D,
               const ParsedTemplateInfo &TemplateInfo = ParsedTemplateInfo());
  Decl *ParseFunctionStatementBody(Decl *Decl, ParseScope &BodyScope);

  struct FieldCallback {
    virtual void invoke(ParsingFieldDeclarator &Field) = 0;
    virtual ~FieldCallback() {}

  private:
    virtual void _anchor();
  };

  void ParseStructDeclaration(ParsingDeclSpec &DS, FieldCallback &Callback);

  bool isDeclarationSpecifier(bool DisambiguatingWithExpression = false);
  bool isTypeSpecifierQualifier();
  bool isTypeQualifier() const;

  /// isKnownToBeTypeSpecifier - Return true if we know that the specified token
  /// is definitely a type-specifier.  Return false if it isn't part of a type
  /// specifier or if we're not sure.
  bool isKnownToBeTypeSpecifier(const Token &Tok) const;

public:
  TypeResult ParseTypeName(SourceRange *Range = 0,
                           Declarator::TheContext Context
                             = Declarator::TypeNameContext,
                           AccessSpecifier AS = AS_none,
                           Decl **OwnedType = 0);

private:
  void ParseBlockId(SourceLocation CaretLoc);

  // Check for the start of a C++11 attribute-specifier-seq in a context where
  // an attribute is not allowed.
  bool CheckProhibitedCXX11Attribute() {
    assert(Tok.is(tok::l_square));
    if (!getLangOpts().CPlusPlus0x || NextToken().isNot(tok::l_square))
      return false;
    return DiagnoseProhibitedCXX11Attribute();
  }
  bool DiagnoseProhibitedCXX11Attribute();

  void ProhibitAttributes(ParsedAttributesWithRange &attrs) {
    if (!attrs.Range.isValid()) return;
    DiagnoseProhibitedAttributes(attrs);
    attrs.clear();
  }
  void DiagnoseProhibitedAttributes(ParsedAttributesWithRange &attrs);

  // Forbid C++11 attributes that appear on certain syntactic 
  // locations which standard permits but we don't supported yet, 
  // for example, attributes appertain to decl specifiers.
  void ProhibitCXX11Attributes(ParsedAttributesWithRange &attrs);

  void MaybeParseGNUAttributes(Declarator &D,
                               LateParsedAttrList *LateAttrs = 0) {
    if (Tok.is(tok::kw___attribute)) {
      ParsedAttributes attrs(AttrFactory);
      SourceLocation endLoc;
      ParseGNUAttributes(attrs, &endLoc, LateAttrs);
      D.takeAttributes(attrs, endLoc);
    }
  }
  void MaybeParseGNUAttributes(ParsedAttributes &attrs,
                               SourceLocation *endLoc = 0,
                               LateParsedAttrList *LateAttrs = 0) {
    if (Tok.is(tok::kw___attribute))
      ParseGNUAttributes(attrs, endLoc, LateAttrs);
  }
  void ParseGNUAttributes(ParsedAttributes &attrs,
                          SourceLocation *endLoc = 0,
                          LateParsedAttrList *LateAttrs = 0);
  void ParseGNUAttributeArgs(IdentifierInfo *AttrName,
                             SourceLocation AttrNameLoc,
                             ParsedAttributes &Attrs,
                             SourceLocation *EndLoc,
                             IdentifierInfo *ScopeName,
                             SourceLocation ScopeLoc,
                             AttributeList::Syntax Syntax);

  void MaybeParseCXX0XAttributes(Declarator &D) {
    if (getLangOpts().CPlusPlus0x && isCXX11AttributeSpecifier()) {
      ParsedAttributesWithRange attrs(AttrFactory);
      SourceLocation endLoc;
      ParseCXX11Attributes(attrs, &endLoc);
      D.takeAttributes(attrs, endLoc);
    }
  }
  void MaybeParseCXX0XAttributes(ParsedAttributes &attrs,
                                 SourceLocation *endLoc = 0) {
    if (getLangOpts().CPlusPlus0x && isCXX11AttributeSpecifier()) {
      ParsedAttributesWithRange attrsWithRange(AttrFactory);
      ParseCXX11Attributes(attrsWithRange, endLoc);
      attrs.takeAllFrom(attrsWithRange);
    }
  }
  void MaybeParseCXX0XAttributes(ParsedAttributesWithRange &attrs,
                                 SourceLocation *endLoc = 0,
                                 bool OuterMightBeMessageSend = false) {
    if (getLangOpts().CPlusPlus0x &&
        isCXX11AttributeSpecifier(false, OuterMightBeMessageSend))
      ParseCXX11Attributes(attrs, endLoc);
  }

  void ParseCXX11AttributeSpecifier(ParsedAttributes &attrs,
                                    SourceLocation *EndLoc = 0);
  void ParseCXX11Attributes(ParsedAttributesWithRange &attrs,
                            SourceLocation *EndLoc = 0);

  IdentifierInfo *TryParseCXX11AttributeIdentifier(SourceLocation &Loc);

  void MaybeParseMicrosoftAttributes(ParsedAttributes &attrs,
                                     SourceLocation *endLoc = 0) {
    if (getLangOpts().MicrosoftExt && Tok.is(tok::l_square))
      ParseMicrosoftAttributes(attrs, endLoc);
  }
  void ParseMicrosoftAttributes(ParsedAttributes &attrs,
                                SourceLocation *endLoc = 0);
  void ParseMicrosoftDeclSpec(ParsedAttributes &Attrs);
  bool IsSimpleMicrosoftDeclSpec(IdentifierInfo *Ident);
  void ParseComplexMicrosoftDeclSpec(IdentifierInfo *Ident, 
                                     SourceLocation Loc,
                                     ParsedAttributes &Attrs);
  void ParseMicrosoftDeclSpecWithSingleArg(IdentifierInfo *AttrName, 
                                           SourceLocation AttrNameLoc, 
                                           ParsedAttributes &Attrs);
  void ParseMicrosoftTypeAttributes(ParsedAttributes &attrs);
  void ParseMicrosoftInheritanceClassAttributes(ParsedAttributes &attrs);
  void ParseBorlandTypeAttributes(ParsedAttributes &attrs);
  void ParseOpenCLAttributes(ParsedAttributes &attrs);
  void ParseOpenCLQualifiers(DeclSpec &DS);

  VersionTuple ParseVersionTuple(SourceRange &Range);
  void ParseAvailabilityAttribute(IdentifierInfo &Availability,
                                  SourceLocation AvailabilityLoc,
                                  ParsedAttributes &attrs,
                                  SourceLocation *endLoc);

  bool IsThreadSafetyAttribute(llvm::StringRef AttrName);
  void ParseThreadSafetyAttribute(IdentifierInfo &AttrName,
                                  SourceLocation AttrNameLoc,
                                  ParsedAttributes &Attrs,
                                  SourceLocation *EndLoc);

  void ParseTypeTagForDatatypeAttribute(IdentifierInfo &AttrName,
                                        SourceLocation AttrNameLoc,
                                        ParsedAttributes &Attrs,
                                        SourceLocation *EndLoc);

  void ParseTypeofSpecifier(DeclSpec &DS);
  SourceLocation ParseDecltypeSpecifier(DeclSpec &DS);
  void AnnotateExistingDecltypeSpecifier(const DeclSpec &DS,
                                         SourceLocation StartLoc,
                                         SourceLocation EndLoc);
  void ParseUnderlyingTypeSpecifier(DeclSpec &DS);
  void ParseAtomicSpecifier(DeclSpec &DS);

  ExprResult ParseAlignArgument(SourceLocation Start,
                                SourceLocation &EllipsisLoc);
  void ParseAlignmentSpecifier(ParsedAttributes &Attrs,
                               SourceLocation *endLoc = 0);

  VirtSpecifiers::Specifier isCXX0XVirtSpecifier(const Token &Tok) const;
  VirtSpecifiers::Specifier isCXX0XVirtSpecifier() const {
    return isCXX0XVirtSpecifier(Tok);
  }
  void ParseOptionalCXX0XVirtSpecifierSeq(VirtSpecifiers &VS, bool IsInterface);

  bool isCXX0XFinalKeyword() const;

  /// DeclaratorScopeObj - RAII object used in Parser::ParseDirectDeclarator to
  /// enter a new C++ declarator scope and exit it when the function is
  /// finished.
  class DeclaratorScopeObj {
    Parser &P;
    CXXScopeSpec &SS;
    bool EnteredScope;
    bool CreatedScope;
  public:
    DeclaratorScopeObj(Parser &p, CXXScopeSpec &ss)
      : P(p), SS(ss), EnteredScope(false), CreatedScope(false) {}

    void EnterDeclaratorScope() {
      assert(!EnteredScope && "Already entered the scope!");
      assert(SS.isSet() && "C++ scope was not set!");

      CreatedScope = true;
      P.EnterScope(0); // Not a decl scope.

      if (!P.Actions.ActOnCXXEnterDeclaratorScope(P.getCurScope(), SS))
        EnteredScope = true;
    }

    ~DeclaratorScopeObj() {
      if (EnteredScope) {
        assert(SS.isSet() && "C++ scope was cleared ?");
        P.Actions.ActOnCXXExitDeclaratorScope(P.getCurScope(), SS);
      }
      if (CreatedScope)
        P.ExitScope();
    }
  };

  /// ParseDeclarator - Parse and verify a newly-initialized declarator.
  void ParseDeclarator(Declarator &D);
  /// A function that parses a variant of direct-declarator.
  typedef void (Parser::*DirectDeclParseFunction)(Declarator&);
  void ParseDeclaratorInternal(Declarator &D,
                               DirectDeclParseFunction DirectDeclParser);

  void ParseTypeQualifierListOpt(DeclSpec &DS, bool GNUAttributesAllowed = true,
                                 bool CXX0XAttributesAllowed = true);
  void ParseDirectDeclarator(Declarator &D);
  void ParseParenDeclarator(Declarator &D);
  void ParseFunctionDeclarator(Declarator &D,
                               ParsedAttributes &attrs,
                               BalancedDelimiterTracker &Tracker,
                               bool IsAmbiguous,
                               bool RequiresArg = false);
  bool isFunctionDeclaratorIdentifierList();
  void ParseFunctionDeclaratorIdentifierList(
         Declarator &D,
         SmallVector<DeclaratorChunk::ParamInfo, 16> &ParamInfo);
  void ParseParameterDeclarationClause(
         Declarator &D,
         ParsedAttributes &attrs,
         SmallVector<DeclaratorChunk::ParamInfo, 16> &ParamInfo,
         SourceLocation &EllipsisLoc);
  void ParseBracketDeclarator(Declarator &D);

  //===--------------------------------------------------------------------===//
  // C++ 7: Declarations [dcl.dcl]

  /// The kind of attribute specifier we have found.
  enum CXX11AttributeKind {
    /// This is not an attribute specifier.
    CAK_NotAttributeSpecifier,
    /// This should be treated as an attribute-specifier.
    CAK_AttributeSpecifier,
    /// The next tokens are '[[', but this is not an attribute-specifier. This
    /// is ill-formed by C++11 [dcl.attr.grammar]p6.
    CAK_InvalidAttributeSpecifier
  };
  CXX11AttributeKind
  isCXX11AttributeSpecifier(bool Disambiguate = false,
                            bool OuterMightBeMessageSend = false);

  Decl *ParseNamespace(unsigned Context, SourceLocation &DeclEnd,
                       SourceLocation InlineLoc = SourceLocation());
  void ParseInnerNamespace(std::vector<SourceLocation>& IdentLoc,
                           std::vector<IdentifierInfo*>& Ident,
                           std::vector<SourceLocation>& NamespaceLoc,
                           unsigned int index, SourceLocation& InlineLoc,
                           ParsedAttributes& attrs,
                           BalancedDelimiterTracker &Tracker);
  Decl *ParseLinkage(ParsingDeclSpec &DS, unsigned Context);
  Decl *ParseUsingDirectiveOrDeclaration(unsigned Context,
                                         const ParsedTemplateInfo &TemplateInfo,
                                         SourceLocation &DeclEnd,
                                         ParsedAttributesWithRange &attrs,
                                         Decl **OwnedType = 0);
  Decl *ParseUsingDirective(unsigned Context,
                            SourceLocation UsingLoc,
                            SourceLocation &DeclEnd,
                            ParsedAttributes &attrs);
  Decl *ParseUsingDeclaration(unsigned Context,
                              const ParsedTemplateInfo &TemplateInfo,
                              SourceLocation UsingLoc,
                              SourceLocation &DeclEnd,
                              AccessSpecifier AS = AS_none,
                              Decl **OwnedType = 0);
  Decl *ParseStaticAssertDeclaration(SourceLocation &DeclEnd);
  Decl *ParseNamespaceAlias(SourceLocation NamespaceLoc,
                            SourceLocation AliasLoc, IdentifierInfo *Alias,
                            SourceLocation &DeclEnd);

  //===--------------------------------------------------------------------===//
  // C++ 9: classes [class] and C structs/unions.
  bool isValidAfterTypeSpecifier(bool CouldBeBitfield);
  void ParseClassSpecifier(tok::TokenKind TagTokKind, SourceLocation TagLoc,
                           DeclSpec &DS, const ParsedTemplateInfo &TemplateInfo,
                           AccessSpecifier AS, bool EnteringContext,
                           DeclSpecContext DSC);
  void ParseCXXMemberSpecification(SourceLocation StartLoc, unsigned TagType,
                                   Decl *TagDecl);
  ExprResult ParseCXXMemberInitializer(Decl *D, bool IsFunction,
                                       SourceLocation &EqualLoc);
  void ParseCXXClassMemberDeclaration(AccessSpecifier AS, AttributeList *Attr,
                const ParsedTemplateInfo &TemplateInfo = ParsedTemplateInfo(),
                                 ParsingDeclRAIIObject *DiagsFromTParams = 0);
  void ParseConstructorInitializer(Decl *ConstructorDecl);
  MemInitResult ParseMemInitializer(Decl *ConstructorDecl);
  void HandleMemberFunctionDeclDelays(Declarator& DeclaratorInfo,
                                      Decl *ThisDecl);

  //===--------------------------------------------------------------------===//
  // C++ 10: Derived classes [class.derived]
  TypeResult ParseBaseTypeSpecifier(SourceLocation &BaseLoc,
                                    SourceLocation &EndLocation);
  void ParseBaseClause(Decl *ClassDecl);
  BaseResult ParseBaseSpecifier(Decl *ClassDecl);
  AccessSpecifier getAccessSpecifierIfPresent() const;

  bool ParseUnqualifiedIdTemplateId(CXXScopeSpec &SS,
                                    SourceLocation TemplateKWLoc,
                                    IdentifierInfo *Name,
                                    SourceLocation NameLoc,
                                    bool EnteringContext,
                                    ParsedType ObjectType,
                                    UnqualifiedId &Id,
                                    bool AssumeTemplateId);
  bool ParseUnqualifiedIdOperator(CXXScopeSpec &SS, bool EnteringContext,
                                  ParsedType ObjectType,
                                  UnqualifiedId &Result);

public:
  bool ParseUnqualifiedId(CXXScopeSpec &SS, bool EnteringContext,
                          bool AllowDestructorName,
                          bool AllowConstructorName,
                          ParsedType ObjectType,
                          SourceLocation& TemplateKWLoc,
                          UnqualifiedId &Result);

private:
  //===--------------------------------------------------------------------===//
  // C++ 14: Templates [temp]

  // C++ 14.1: Template Parameters [temp.param]
  Decl *ParseDeclarationStartingWithTemplate(unsigned Context,
                                             SourceLocation &DeclEnd,
                                             AccessSpecifier AS = AS_none,
                                             AttributeList *AccessAttrs = 0);
  Decl *ParseTemplateDeclarationOrSpecialization(unsigned Context,
                                                 SourceLocation &DeclEnd,
                                                 AccessSpecifier AS,
                                                 AttributeList *AccessAttrs);
  Decl *ParseSingleDeclarationAfterTemplate(
                                       unsigned Context,
                                       const ParsedTemplateInfo &TemplateInfo,
                                       ParsingDeclRAIIObject &DiagsFromParams,
                                       SourceLocation &DeclEnd,
                                       AccessSpecifier AS=AS_none,
                                       AttributeList *AccessAttrs = 0);
  bool ParseTemplateParameters(unsigned Depth,
                               SmallVectorImpl<Decl*> &TemplateParams,
                               SourceLocation &LAngleLoc,
                               SourceLocation &RAngleLoc);
  bool ParseTemplateParameterList(unsigned Depth,
                                  SmallVectorImpl<Decl*> &TemplateParams);
  bool isStartOfTemplateTypeParameter();
  Decl *ParseTemplateParameter(unsigned Depth, unsigned Position);
  Decl *ParseTypeParameter(unsigned Depth, unsigned Position);
  Decl *ParseTemplateTemplateParameter(unsigned Depth, unsigned Position);
  Decl *ParseNonTypeTemplateParameter(unsigned Depth, unsigned Position);
  // C++ 14.3: Template arguments [temp.arg]
  typedef SmallVector<ParsedTemplateArgument, 16> TemplateArgList;

  bool ParseTemplateIdAfterTemplateName(TemplateTy Template,
                                        SourceLocation TemplateNameLoc,
                                        const CXXScopeSpec &SS,
                                        bool ConsumeLastToken,
                                        SourceLocation &LAngleLoc,
                                        TemplateArgList &TemplateArgs,
                                        SourceLocation &RAngleLoc);

  bool AnnotateTemplateIdToken(TemplateTy Template, TemplateNameKind TNK,
                               CXXScopeSpec &SS,
                               SourceLocation TemplateKWLoc,
                               UnqualifiedId &TemplateName,
                               bool AllowTypeAnnotation = true);
  void AnnotateTemplateIdTokenAsType();
  bool IsTemplateArgumentList(unsigned Skip = 0);
  bool ParseTemplateArgumentList(TemplateArgList &TemplateArgs);
  ParsedTemplateArgument ParseTemplateTemplateArgument();
  ParsedTemplateArgument ParseTemplateArgument();
  Decl *ParseExplicitInstantiation(unsigned Context,
                                   SourceLocation ExternLoc,
                                   SourceLocation TemplateLoc,
                                   SourceLocation &DeclEnd,
                                   AccessSpecifier AS = AS_none);

  //===--------------------------------------------------------------------===//
  // Modules
  DeclGroupPtrTy ParseModuleImport(SourceLocation AtLoc);

  //===--------------------------------------------------------------------===//
  // GNU G++: Type Traits [Type-Traits.html in the GCC manual]
  ExprResult ParseUnaryTypeTrait();
  ExprResult ParseBinaryTypeTrait();
  ExprResult ParseTypeTrait();
  
  //===--------------------------------------------------------------------===//
  // Embarcadero: Arary and Expression Traits
  ExprResult ParseArrayTypeTrait();
  ExprResult ParseExpressionTrait();

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
