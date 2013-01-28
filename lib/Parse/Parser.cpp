//===--- Parser.cpp - C Language Family Parser ----------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the Parser interfaces.
//
//===----------------------------------------------------------------------===//

#include "gong/Parse/Parser.h"

#include "gong/Parse/IdentifierList.h"
#include "gong/Parse/Scope.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "RAIIObjectsForParser.h"
#if 0
#include "clang/Parse/ParseDiagnostic.h"
#include "clang/Sema/DeclSpec.h"
#include "clang/Sema/ParsedTemplate.h"
#include "ParsePragma.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/ASTConsumer.h"
#endif
using namespace gong;

Parser::Parser(Lexer &l, Action &actions/*, bool skipFunctionBodies*/)
  : CrashInfo(*this), L(l), Actions(actions), Diags(L.getDiagnostics()) {
  //SkipFunctionBodies = pp.isCodeCompletionEnabled() || skipFunctionBodies;
  Tok.startToken();
  Tok.setKind(tok::eof);
  Actions.CurScope = 0;
  NumCachedScopes = 0;
  ParenCount = BracketCount = BraceCount = 0;
  CompositeTypeNameLitNeedsParens = false;

  //PP.setCodeCompletionHandler(*this);
}

Parser::~Parser() {
  // If we still have scopes active, delete the scope tree.
  delete getCurScope();
  Actions.CurScope = 0;
  
  // Free the scope cache.
  for (unsigned i = 0, e = NumCachedScopes; i != e; ++i)
    delete ScopeCache[i];

  //PP.clearCodeCompletionHandler();
}

/// Initialize - Warm up the parser.
///
void Parser::Initialize() {
  // Create the translation unit scope.  Install it as the current scope.
  assert(getCurScope() == 0 && "A scope is already active?");
  EnterScope(Scope::DeclScope);
  Actions.ActOnTranslationUnitScope(getCurScope());

  Ident_while = &L.getIdentifierTable().get("while");

  //Actions.Initialize();

  // Prime the lexer look-ahead.
  ConsumeToken();
}

/// SourceFile     = PackageClause ";" { ImportDecl ";" } { TopLevelDecl ";" } .
void Parser::ParseSourceFile() {
  Initialize();

  if (Tok.isNot(tok::kw_package)) {
    // FIXME: fixit
    Diag(diag::expected_package);
    return;
  }
  // FIXME: return if this fails (?)
  ParsePackageClause();

  while (Tok.is(tok::kw_import)) {
    // FIXME: check if this succeeds
    ParseImportDecl();

    // FIXME: check if this succeeds
    // FIXME: fixit?
    //ExpectAndConsumeSemi(diag::expected_semi_import);
    if (Tok.isNot(tok::semi)) {
      Diag(diag::expected_semi_import);
      SkipUntil(tok::semi);
    } else
      ConsumeToken();
  }

  while (Tok.isNot(tok::eof)) {  // FIXME
    // FIXME: check if this succeeds
    ParseTopLevelDecl();

    // FIXME: check if this succeeds
    // FIXME: fixit?
    //ExpectAndConsumeSemi(diag::expected_semi_import);
    if (Tok.isNot(tok::semi)) {
      Diag(diag::expected_semi);
      SkipUntil(tok::semi);
    } else
      ConsumeToken();
  }

  ExitScope();
  assert(getCurScope() == 0 && "Scope imbalance!");
}

/// PackageClause  = "package" PackageName .
/// PackageName    = identifier .
bool Parser::ParsePackageClause() {
  assert(Tok.is(tok::kw_package) && "Not 'package'!");
  SourceLocation PackageLoc = ConsumeToken();
  if (Tok.isNot(tok::identifier)) {
    Diag(diag::expected_ident);
    SkipUntil(tok::semi);
    return true;
  }
  IdentifierInfo *II = Tok.getIdentifierInfo();
  SourceLocation IdentLoc = ConsumeToken();
  if (II->getName() == "_") {
    // FIXME: this check belongs in sema
    Diag(diag::invalid_package_name) << II;
    SkipUntil(tok::semi);  // FIXME: ?
    return true;
  }

  // FIXME: use
  (void)PackageLoc;
  (void)IdentLoc;

  return ExpectAndConsumeSemi(diag::expected_semi_package);
}

/// ImportDecl       = "import" ( ImportSpec | "(" { ImportSpec ";" } ")" ) .
bool Parser::ParseImportDecl() {
  assert(Tok.is(tok::kw_import) && "Not 'import'!");
  SourceLocation ImportLoc = ConsumeToken();

  // FIXME: use
  (void)ImportLoc;

  if (Tok.is(tok::l_paren)) {
    BalancedDelimiterTracker T(*this, tok::l_paren);
    T.consumeOpen();
    // FIXME (BalancedDelimiterTracker?)
    bool Fails = false;
    while (Tok.isNot(tok::r_paren) && Tok.isNot(tok::eof)) {
      bool Fail;
      if (Tok.isNot(tok::period) &&
          Tok.isNot(tok::identifier) &&
          Tok.isNot(tok::string_literal)) {
        Diag(Tok, diag::expected_period_or_ident_or_string);
        Fail = true;
      } else
        Fail = ParseImportSpec();

      if (Fail) {
        SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
        Fails = true;
      }

      // A semicolon may be omitted before a closing ')' or '}'.
      if (Tok.is(tok::r_paren))
        break;

      // FIXME: check if this succeeds
      // FIXME: fixit?
      //ExpectAndConsumeSemi(diag::expected_semi_import);
      if (Tok.isNot(tok::semi)) {
        Fails = true;
        Diag(diag::expected_semi_import);
        SkipUntil(tok::semi);
      } else
        ConsumeToken();
    }
    T.consumeClose();
    return Fails;
  } else {
    bool Fail = ParseImportSpec();
    if (Fail) {
      SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
      return true;
    }
  }

  return false;
}

/// ImportSpec       = [ "." | PackageName ] ImportPath .
/// ImportPath       = string_lit .
bool Parser::ParseImportSpec() {
  assert((Tok.is(tok::period) || Tok.is(tok::identifier) ||
          Tok.is(tok::string_literal)) && "Invalid ParseImportDecl start");

  bool IsDot = false;
  IdentifierInfo *II = NULL;
  if (Tok.is(tok::period)) {
    IsDot = true;
    ConsumeToken();
  } else if (Tok.is(tok::identifier)) {
    II = Tok.getIdentifierInfo();
    ConsumeToken();
  }

  if (Tok.isNot(tok::string_literal)) {
    Diag(diag::expected_string_literal);
    return true;
  }
  StringRef Import(Tok.getLiteralData(), Tok.getLength());
  SourceLocation ImportLoc = ConsumeStringToken();

  Actions.ActOnImportSpec(ImportLoc, Import, II, IsDot);

  return false;
}

/// TopLevelDecl  = Declaration | FunctionDecl | MethodDecl .
bool Parser::ParseTopLevelDecl(/*DeclGroupPtrTy &Result*/) {
  if (Tok.is(tok::kw_func)) {
    return ParseFunctionOrMethodDecl();
  } else if (Tok.is(tok::kw_const) || Tok.is(tok::kw_type) ||
             Tok.is(tok::kw_var)) {
    return ParseDeclaration() == Action::DeclPtrTy();
  } else {
    Diag(Tok, diag::expected_topleveldecl);
    SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
    return true;
  }
}

bool Parser::ParseFunctionOrMethodDecl() {
  assert(Tok.is(tok::kw_func) && "Expected 'func'");
  SourceLocation FuncLoc = ConsumeToken();

  if (Tok.is(tok::identifier))
    return ParseFunctionDecl(FuncLoc);
  else if (Tok.is(tok::l_paren))
    return ParseMethodDecl();
  else {
    Diag(FuncLoc, diag::expected_ident_or_l_paren);
    SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
    return true;
  }
}

/// The current token points at FunctionName when this is called.
/// FunctionDecl = "func" FunctionName Signature [ Body ] .
/// FunctionName = identifier .
/// Body         = Block .
bool Parser::ParseFunctionDecl(SourceLocation FuncLoc) {
  assert(Tok.is(tok::identifier) && "Expected identifier");
  IdentifierInfo *FunctionName = Tok.getIdentifierInfo();
  SourceLocation NameLoc = ConsumeToken();

  if (Tok.is(tok::l_paren)) {
    ParseSignature();
  } else {
    if (Tok.is(tok::l_brace)) {
      SourceLocation EndLoc = Lexer::getLocForEndOfToken(PrevTokLocation, 0,
                                                         L.getSourceManager());
      Diag(EndLoc, diag::missing_parameter_list)
          << FixItHint::CreateInsertion(EndLoc, "()");
    } else
      // FIXME: SkipUntil(tok::l_brace)?
      Diag(Tok, diag::expected_l_paren);
  }

  Action::DeclPtrTy Fun =
      Actions.ActOnFunctionDecl(FuncLoc, NameLoc, *FunctionName, getCurScope());

  if (Tok.is(tok::l_brace)) {
    // Enter a scope for the function body.
    ParseScope BodyScope(this, Scope::FnScope | Scope::DeclScope);
    
    Actions.ActOnStartOfFunctionDef(Fun, getCurScope());
    OwningStmtResult FnBody(ParseBlockBody());
    Actions.ActOnFinishFunctionBody(Fun, move(FnBody));
  }

  return false;
}

/// The current token points at Receiver when this is called.
/// MethodDecl   = "func" Receiver MethodName Signature [ Body ] .
/// BaseTypeName = identifier .
bool Parser::ParseMethodDecl() {
  ParseReceiver();

  IdentifierInfo *MethodName = NULL;;
  SourceLocation MethodLoc;
  if (Tok.is(tok::identifier)) {
    MethodName = Tok.getIdentifierInfo();
    MethodLoc = ConsumeToken();
  } else {
    Diag(Tok, diag::expected_ident);
  }
  (void)MethodName;  // FIXME
  (void)MethodLoc;  // FIXME

  if (Tok.is(tok::l_paren)) {
    ParseSignature();
  } else {
    if (Tok.is(tok::l_brace)) {
      SourceLocation EndLoc = Lexer::getLocForEndOfToken(PrevTokLocation, 0,
                                                         L.getSourceManager());
      Diag(EndLoc, diag::missing_parameter_list)
          << FixItHint::CreateInsertion(EndLoc, "()");
    } else
      // FIXME: SkipUntil(tok::l_brace)?
      Diag(Tok, diag::expected_l_paren);
  }

  if (Tok.is(tok::l_brace)) {
    // FIXME
    ParseBody();
  }

  return false;
}

/// Signature      = Parameters [ Result ] .
bool Parser::ParseSignature() {
  assert(Tok.is(tok::l_paren) && "Expected '('");

  ParseParameters();

  if (Tok.is(tok::l_paren) || IsType()) {
    ParseResult();
  }

  return false;
}

/// Result         = Parameters | Type .
bool Parser::ParseResult() {
  assert(Tok.is(tok::l_paren) | IsType());

  // Note: '(' could also be the start of a type, but ParseParameters() accepts
  // a superset of the type productions starting with '(', so it's ok to always
  // go down ParseParameters() when a '(' is found.
  if (Tok.is(tok::l_paren))
    return ParseParameters();
  // FIXME: check IsType()
  return ParseType().isInvalid();
}

/// Parameters     = "(" [ ParameterList [ "," ] ] ")" .
bool Parser::ParseParameters() {
  assert(Tok.is(tok::l_paren) && "Expected '('");
  BalancedDelimiterTracker T(*this, tok::l_paren);
  T.consumeOpen();

  if (IsParameterList()) {
    ParseParameterList();
  }

  if (Tok.is(tok::comma))
    ConsumeToken();

  return T.consumeClose();
}

bool Parser::IsParameterList() {
  return Tok.is(tok::identifier) || Tok.is(tok::ellipsis) || IsType();
}

/// ParameterList  = ParameterDecl { "," ParameterDecl } .
bool Parser::ParseParameterList() {
  assert(IsParameterList());

  ParseParameterDecl();
  while (Tok.is(tok::comma)) {
    ConsumeToken();
    ParseParameterDecl();
  }
  return false;
}

/// ParameterDecl  = [ IdentifierList ] [ "..." ] Type .
bool Parser::ParseParameterDecl() {
  // This tries to parse just a single ParameterDecl. However, it's not
  // always clear if a list of identifiers is an identifier list or a type list,
  // for example |int, int, int| are three ParameterDecls but
  // |int, int, int int| is just one. Hence, this slurps up type lists without
  // parameter lists too.

  if (Tok.is(tok::identifier)) {
    // FIXME: This would be marginally nicer if the lexer had 1 lookahead.
    IdentifierInfo *II = Tok.getIdentifierInfo();
    SourceLocation IILoc = ConsumeToken();

    bool SawIdentifiersOnly = true;
    ParseTypeNameTail(IILoc, II, &SawIdentifiersOnly);
    ParseTypeListTail(/*AcceptEllipsis=*/true, &SawIdentifiersOnly);

    bool HadEllipsis = false;
    SourceLocation EllipsisLoc;
    if (Tok.is(tok::ellipsis)) {
      EllipsisLoc = ConsumeToken();
      HadEllipsis = true;
    }

    bool HadTrailingType = false;
    SourceLocation TypeLoc;
    if (IsType()) {
      TypeLoc = Tok.getLocation();
      ParseType();
      HadTrailingType = true;
    }

    // ident only  ellipsis  type
    // 0           0         0       => ok
    // 1           0         0       => ok
    // 0           0         1       => unexpected type
    // 1           0         1       => ok
    // 0           1         0       => unexpected ...
    // 1           1         0       => expected type
    // 0           1         1       => expected only idents left of ...
    // 1           1         1       => ok
    if (!SawIdentifiersOnly && !HadEllipsis && HadTrailingType) {
      Diag(TypeLoc, diag::unexpected_type);
      return true;
    }
    if (!SawIdentifiersOnly && HadEllipsis && !HadTrailingType) {
      Diag(EllipsisLoc, diag::unexpected_ellipsis);
      return true;
    }
    if (SawIdentifiersOnly && HadEllipsis && !HadTrailingType) {
      Diag(Tok, diag::expected_type);
      return true;
    }
    if (!SawIdentifiersOnly && HadEllipsis && HadTrailingType) {
      Diag(EllipsisLoc, diag::expected_idents_only_before_ellipsis);
      return true;
    }
    return false;
  }

  if (Tok.is(tok::ellipsis))
    ConsumeToken();

  if (!IsType()) {
    Diag(Tok, diag::expected_type);
    return true;
  }
  return ParseType().isInvalid();
}

/// Receiver     = "(" [ identifier ] [ "*" ] BaseTypeName ")" .
bool Parser::ParseReceiver() {
  assert(Tok.is(tok::l_paren) && "Expected '('");
  BalancedDelimiterTracker T(*this, tok::l_paren);
  T.consumeOpen();

  IdentifierInfo *FirstII = NULL;
  if (Tok.is(tok::identifier)) {
    FirstII = Tok.getIdentifierInfo();
    ConsumeToken();
  }

  bool IsStar = false;
  if (Tok.is(tok::star)) {
    IsStar = true;
    ConsumeToken();
  }

  IdentifierInfo *TypeII = NULL;
  if (Tok.is(tok::identifier)) {
    TypeII = Tok.getIdentifierInfo();
    ConsumeToken();
  } else if (!IsStar) {
    TypeII = FirstII;
    FirstII = NULL;
  }

  if (!TypeII) {
    Diag(Tok.getLocation(), diag::expected_ident);
    SkipUntil(tok::r_paren, /*StopAtSemi=*/true, /*DontConsume=*/true);
  }

  T.consumeClose();

  // FIXME
  return true;
}

/// Type      = TypeName | TypeLit | "(" Type ")" .
Action::OwningDeclResult Parser::ParseType() {
  if (Tok.is(tok::identifier))
    return ParseTypeName();

  if (Tok.is(tok::l_paren)) {
    BalancedDelimiterTracker T(*this, tok::l_paren);
    T.consumeOpen();
    OwningDeclResult Res = ParseType();
    T.consumeClose();

    if (!Res.isInvalid())
      Res = Actions.ActOnParenType(T.getOpenLocation(), move(Res),
                                   T.getCloseLocation());
    return Res;
  }

  return ParseTypeLit() ? DeclError() : DeclEmpty();  // FIXME
}

/// TypeName  = identifier | QualifiedIdent .
/// QualifiedIdent = PackageName "." identifier .
Action::OwningDeclResult Parser::ParseTypeName() {
  assert(Tok.is(tok::identifier) && "Expected identifier");
  IdentifierInfo *TypeII = Tok.getIdentifierInfo();
  SourceLocation IILoc = ConsumeToken();
  return ParseTypeNameTail(IILoc, TypeII);
}

/// This is called for TypeName after the initial identifier has been read.
Action::OwningDeclResult
Parser::ParseTypeNameTail(SourceLocation IILoc, IdentifierInfo *Head,
                          bool *SawIdentifierOnly) {
  if (Tok.isNot(tok::period)) {
    // The type name was just the identifier.
    return Actions.ActOnTypeName(IILoc, *Head, getCurScope());
  }

  // It's a QualifiedIdent.
  ConsumeToken();

  if (SawIdentifierOnly)
    *SawIdentifierOnly = false;

  if (Tok.isNot(tok::identifier)) {
    Diag(Tok, diag::expected_ident);
    // FIXME: This doesn't recover well when called from ParseMethodSpec() for
    // interface{} types.
    SkipUntil(tok::l_brace, tok::semi,
              /*StopAtSemi=*/false, /*DontConsume=*/true);
    return DeclError();
  }

  IdentifierInfo *Qualified = Tok.getIdentifierInfo();
  (void)Qualified;
  ConsumeToken();
  // FIXME: call Actions.ActOnQualifiedTypeName() or some such.
  return DeclError();
}

/// TypeLit   = ArrayType | StructType | PointerType | FunctionType |
///             InterfaceType | SliceType | MapType | ChannelType .
bool Parser::ParseTypeLit() {
  switch (Tok.getKind()) {
  case tok::l_square:     return ParseArrayOrSliceType();
  case tok::kw_struct:    return ParseStructType();
  case tok::star:         return ParsePointerType().isInvalid();
  case tok::kw_func:      return ParseFunctionType();
  case tok::kw_interface: return ParseInterfaceType();
  case tok::kw_map:       return ParseMapType();
  case tok::kw_chan:
  case tok::lessminus:    return ParseChannelType().isInvalid();
  default: llvm_unreachable("unexpected token kind");
  }
}

/// ArrayType   = "[" ArrayLength "]" ElementType .
/// ArrayLength = Expression .
/// SliceType = "[" "]" ElementType .
bool Parser::ParseArrayOrSliceType() {
  assert(Tok.is(tok::l_square) && "Expected '['");
  BalancedDelimiterTracker T(*this, tok::l_square);
  T.consumeOpen();
  
  if (Tok.is(tok::r_square))
    return ParseSliceType(T);
  return ParseArrayType(T);
}

/// Tok points at ArrayLength when this is called.
bool Parser::ParseArrayType(BalancedDelimiterTracker &T) {
  OwningExprResult Expr = ParseExpression();
  (void)Expr;  // FIXME
  if (T.consumeClose()) {
    SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
    return true;
  }
  if (IsElementType())
    return ParseElementType().isInvalid();
  Diag(Tok, diag::expected_element_type);
  return true;
}

/// Tok points at the ']' when this is called.
bool Parser::ParseSliceType(BalancedDelimiterTracker &T) {
  assert(Tok.is(tok::r_square) && "Expected ']'");
  T.consumeClose();
  if (IsElementType())
    return ParseElementType().isInvalid();
  Diag(Tok, diag::expected_element_type);
  return false;
}

/// StructType     = "struct" "{" { FieldDecl ";" } "}" .
bool Parser::ParseStructType() {
  assert(Tok.is(tok::kw_struct) && "Expected 'struct'");
  ConsumeToken();

  // FIXME: This is very similar to ParseInterfaceType
  if (Tok.isNot(tok::l_brace)) {
    // FIXME: ...after 'struct'
    Diag(Tok, diag::expected_l_brace);
    SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
    return true;
  }
  BalancedDelimiterTracker T(*this, tok::l_brace);
  T.consumeOpen();

  while (Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
    if (Tok.isNot(tok::identifier) && Tok.isNot(tok::star)) {
      Diag(Tok, diag::expected_ident_or_star);
      T.skipToEnd();
      return true;
    }
    if (ParseFieldDecl()) {
      T.skipToEnd();
      return true;
    }

    if (Tok.isNot(tok::semi) && Tok.isNot(tok::r_brace)) {
      Diag(diag::expected_semi);  // FIXME "...in 'interface'"
      SkipUntil(tok::r_brace, /*StopAtSemi=*/true, /*DontConsume=*/true);
    }
    if (Tok.is(tok::semi))
      ConsumeToken();
  }
  T.consumeClose();
  return false;
}

/// FieldDecl      = (IdentifierList Type | AnonymousField) [ Tag ] .
/// Tag            = string_lit .
bool Parser::ParseFieldDecl() {
  assert((Tok.is(tok::identifier) || Tok.is(tok::star)) &&
      "Expected identifier or '*'");

  if (Tok.is(tok::star))
    ParseAnonymousField();
  else {
    // tok::identifier
    IdentifierInfo *II = Tok.getIdentifierInfo();
    SourceLocation IILoc = ConsumeToken();

    // If next is:
    // ',': IdentifierListTail Type
    // IsType(): Indentifier Type
    // else: AnonymousField
    if (Tok.is(tok::comma)) {
      IdentifierList IdentList(IILoc, II);
      ParseIdentifierListTail(IdentList);
      if (!IsType()) {
        Diag(Tok, diag::expected_type);
        return true;
      }
      ParseType();
    } else if (IsType()) {
      ParseType();
    } else {
      ParseAnonymousFieldTail(IILoc, II);
    }
  }

  if (Tok.is(tok::string_literal))
    ConsumeStringToken();
  return false;
}

/// AnonymousField = [ "*" ] TypeName .
bool Parser::ParseAnonymousField() {
  assert((Tok.is(tok::star) || Tok.is(tok::identifier)) &&
      "Expected '*' or identifier");
  if (Tok.is(tok::star))
    ConsumeToken();
  if (Tok.isNot(tok::identifier)) {
    Diag(Tok, diag::expected_ident);
    return true;
  }
  IdentifierInfo* II = Tok.getIdentifierInfo();
  SourceLocation IILoc = ConsumeToken();
  return ParseAnonymousFieldTail(IILoc, II);
}

bool Parser::ParseAnonymousFieldTail(SourceLocation IILoc, IdentifierInfo* II) {
  return ParseTypeNameTail(IILoc, II).isInvalid();
}

/// PointerType = "*" BaseType .
/// BaseType = Type .
Action::OwningDeclResult Parser::ParsePointerType() {
  assert(Tok.is(tok::star) && "Expected '*'");
  SourceLocation StarLoc = ConsumeToken();
  if (IsType()) {
    OwningDeclResult T = ParseType();
    if (T.isInvalid())
      return DeclError();

    return Actions.ActOnPointerType(StarLoc, move(T));
  }
  Diag(Tok, diag::expected_type);
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return DeclError();
}

/// FunctionType   = "func" Signature .
bool Parser::ParseFunctionType() {
  assert(Tok.is(tok::kw_func) && "Expected 'func'");
  ConsumeToken();
  if (Tok.is(tok::l_paren))
    return ParseSignature();
  Diag(Tok, diag::expected_l_paren);
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return true;
}

/// InterfaceType      = "interface" "{" { MethodSpec ";" } "}" .
bool Parser::ParseInterfaceType() {
  assert(Tok.is(tok::kw_interface) && "Expected 'interface'");
  ConsumeToken();
  if (Tok.isNot(tok::l_brace)) {
    // FIXME: ... after 'interface'
    Diag(Tok, diag::expected_l_brace);
    SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
    return true;
  }
  BalancedDelimiterTracker T(*this, tok::l_brace);
  T.consumeOpen();

  while (Tok.isNot(tok::r_brace) && Tok.isNot(tok::eof)) {
    if (Tok.isNot(tok::identifier)) {
      Diag(Tok, diag::expected_ident);
      T.skipToEnd();
      return true;
    }
    if (ParseMethodSpec()) {
      T.skipToEnd();
      return true;
    }

    if (Tok.isNot(tok::semi) && Tok.isNot(tok::r_brace)) {
      Diag(diag::expected_semi);  // FIXME "...in 'interface'"
      SkipUntil(tok::r_brace, /*StopAtSemi=*/true, /*DontConsume=*/true);
    }
    if (Tok.is(tok::semi))
      ConsumeToken();
  }
  T.consumeClose();
  return false;
}

/// MethodSpec         = MethodName Signature | InterfaceTypeName .
/// MethodName         = identifier .
/// InterfaceTypeName  = TypeName .
bool Parser::ParseMethodSpec() {
  assert(Tok.is(tok::identifier) && "Expected identifier");

  // If next is:
  // '(' identifier was MethodName, next is signature
  // '.' identifier was head of InterfaceTypeName as part of a QualifiedIdent
  // else: InterfaceTypeName as identifier
  IdentifierInfo *II = Tok.getIdentifierInfo();
  SourceLocation IILoc = ConsumeToken();

  if (Tok.is(tok::l_paren))
    return ParseSignature();
  else
    return ParseTypeNameTail(IILoc, II).isInvalid();
}

/// MapType     = "map" "[" KeyType "]" ElementType .
/// KeyType     = Type .
bool Parser::ParseMapType() {
  assert(Tok.is(tok::kw_map) && "Expected 'map'");
  ConsumeToken();

  BalancedDelimiterTracker T(*this, tok::l_square);
  if (T.expectAndConsume(diag::expected_l_square)) {
    SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
    return true;
  }

  if (!IsType()) {
    Diag(Tok, diag::expected_type);
    SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
    return true;
  }
  ParseType();

  if (T.consumeClose())
    return true;

  if (!IsElementType()) {
    Diag(Tok, diag::expected_type);
    SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
    return true;
  }
  return ParseElementType().isInvalid();
}

/// ChannelType = ( "chan" [ "<-" ] | "<-" "chan" ) ElementType .
Parser::OwningDeclResult Parser::ParseChannelType() {
  assert((Tok.is(tok::kw_chan) || Tok.is(tok::lessminus)) && "Expected 'map'");

  SourceLocation ChanLoc, ArrowLoc;
  bool IsRecv = Tok.is(tok::lessminus);
  if (!IsRecv) {
    // "chan" [ "<-" ]
    ChanLoc = ConsumeToken();
    if (Tok.is(tok::lessminus))
      ArrowLoc = ConsumeToken();
  } else {
    // "<-" "chan"
    ArrowLoc = ConsumeToken();

    if (ExpectAndConsume(tok::kw_chan, diag::expected_chan)) {
      // FIXME: recover better for "var a <- }", "(<-)(".
      SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
      return DeclError();
    }
    ChanLoc = PrevTokLocation;
  }

  if (!IsElementType()) {
    Diag(Tok, diag::expected_element_type);
    SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
    return DeclError();
  }
  OwningDeclResult Res = ParseElementType();
  if (!Res.isInvalid()) {
    if (IsRecv)
      Res = Actions.ActOnRecvChannelType(ArrowLoc, ChanLoc, move(Res));
    else if (ArrowLoc.isValid())
      Res = Actions.ActOnSendChannelType(ChanLoc, ArrowLoc, move(Res));
    else
      Res = Actions.ActOnBiChannelType(ChanLoc, move(Res));
  }
  return Res;
}

/// ElementType = Type .
Parser::OwningDeclResult Parser::ParseElementType() {
  return ParseType();
}

/// TypeList        = Type { "," Type } .
bool Parser::ParseTypeList() {
  ParseType();
  return ParseTypeListTail();
}

/// This is called after the first Type in a TypeList has been called.
/// If SawIdentifiersOnly is not NULL, it's set to false if not all types in
/// the list were single identifiers (else it's not written).
bool Parser::ParseTypeListTail(bool AcceptEllipsis, bool *SawIdentifiersOnly) {
  while (Tok.is(tok::comma)) {
    ConsumeToken();
    // FIXME: Diag if Tok doesn't start a type.

    if (Tok.is(tok::ellipsis)) {
      if (!AcceptEllipsis)
        Diag(Tok, diag::unexpected_ellipsis);
      ConsumeToken();
    }

    if (Tok.isNot(tok::identifier)) {
      if (SawIdentifiersOnly)
        *SawIdentifiersOnly = false;
      ParseType();
      continue;
    }

    IdentifierInfo *II = Tok.getIdentifierInfo();
    SourceLocation IILoc = ConsumeToken();
    ParseTypeNameTail(IILoc, II, SawIdentifiersOnly);
  }
  return false;
}

/// IdentifierList = identifier { "," identifier } .
bool Parser::ParseIdentifierList(IdentifierList &IdentList) {
  assert(Tok.is(tok::identifier) && "Expected identifier");
  IdentifierInfo *Ident = Tok.getIdentifierInfo();
  SourceLocation IdentLoc = ConsumeToken();
  IdentList.initialize(IdentLoc, Ident);
  return ParseIdentifierListTail(IdentList);
}

/// This is called for IdentifierInfo after the initial identifier has been read
bool Parser::ParseIdentifierListTail(IdentifierList &IdentList) {
  while (Tok.is(tok::comma)) {
    SourceLocation CommaLoc = ConsumeToken();

    if (Tok.isNot(tok::identifier)) {
      Diag(Tok, diag::expected_ident);
      return true;
    }
    IdentifierInfo *Ident = Tok.getIdentifierInfo();
    SourceLocation IdentLoc = ConsumeToken();
    IdentList.add(CommaLoc, IdentLoc, Ident);
  }
  return false;
}

Action::DeclPtrTy Parser::ParseDeclaration() {
  assert((Tok.is(tok::kw_const) || Tok.is(tok::kw_type) ||
          Tok.is(tok::kw_var)) && "Expected 'const', 'type', or 'var'");
  switch (Tok.getKind()) {
    case tok::kw_const: return ParseConstDecl();
    case tok::kw_type:  return ParseTypeDecl();
    case tok::kw_var:   return ParseVarDecl();
    default: llvm_unreachable("unexpected token kind");
  }
}

/// ConstDecl      = "const" ( ConstSpec | "(" { ConstSpec ";" } ")" ) .
Action::DeclPtrTy Parser::ParseConstDecl() {
  assert(Tok.is(tok::kw_const) && "Expected 'const'");
  SourceLocation ConstLoc = ConsumeToken();
  if (Tok.is(tok::identifier)) {
    Action::DeclPtrTy ConstDecl = Actions.ActOnSingleDecl(ConstLoc, DGK_Const);
    if (ParseConstSpec(ConstDecl))
      return Action::DeclPtrTy();
    return ConstDecl;
  }
  if (Tok.is(tok::l_paren))
    return ParseDeclGroup(DGK_Const, ConstLoc);
  Diag(Tok, diag::expected_ident_or_l_paren);
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return Action::DeclPtrTy();
}

/// ConstSpec      = IdentifierList [ [ Type ] "=" ExpressionList ] .
bool Parser::ParseConstSpec(Action::DeclPtrTy ConstDecl) {
  assert(Tok.is(tok::identifier) && "Expected identifier");
  IdentifierList IdentList;
  ParseIdentifierList(IdentList);

  // FIXME: call this later; pass equalloc, rhs
  Actions.ActOnConstSpec(ConstDecl, IdentList, getCurScope());

  if (Tok.is(tok::semi) || Tok.is(tok::r_paren))
    return false;
  if (Tok.isNot(tok::equal) && !IsType()) {
    Diag(Tok, diag::expected_equal_or_type);
    SkipUntil(tok::semi, /*ConsumeSemi=*/false, /*DontConsume=*/true);
    return true;
  }
  if (Tok.isNot(tok::equal))
    ParseType();
  if (Tok.isNot(tok::equal))
    Diag(Tok, diag::expected_equal);
  else
    ConsumeToken();  // Eat '='.
  if (!IsExpression())
    return true;
  ExprVector Exprs(Actions);  // FIXME: use
  return ParseExpressionList(Exprs);
}

/// TypeDecl     = "type" ( TypeSpec | "(" { TypeSpec ";" } ")" ) .
Action::DeclPtrTy Parser::ParseTypeDecl() {
  assert(Tok.is(tok::kw_type) && "Expected 'type'");
  SourceLocation TypeLoc = ConsumeToken();
  if (Tok.is(tok::identifier)) {
    Action::DeclPtrTy TypeDecl = Actions.ActOnSingleDecl(TypeLoc, DGK_Type);
    if (ParseTypeSpec(TypeDecl))
      return Action::DeclPtrTy();
    return TypeDecl;
  }
  if (Tok.is(tok::l_paren))
    return ParseDeclGroup(DGK_Type, TypeLoc);
  Diag(Tok, diag::expected_ident_or_l_paren);
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return Action::DeclPtrTy();
}

/// TypeSpec     = identifier Type .
bool Parser::ParseTypeSpec(Action::DeclPtrTy TypeDecl) {
  assert(Tok.is(tok::identifier) && "Expected identifier");
  IdentifierInfo *TypeName = Tok.getIdentifierInfo();
  SourceLocation TypeNameLoc = ConsumeToken();
  if (!IsType()) {
    Diag(Tok, diag::expected_type);
    return true;
  }
  // FIXME: pass type to ActOnTypeSpec(), return result from ActOnTypeSpec().
  Actions.ActOnTypeSpec(TypeDecl, TypeNameLoc, *TypeName, getCurScope());
  return ParseType().isInvalid();
}

/// VarDecl     = "var" ( VarSpec | "(" { VarSpec ";" } ")" ) .
Action::DeclPtrTy Parser::ParseVarDecl() {
  assert(Tok.is(tok::kw_var) && "Expected 'var'");
  SourceLocation VarLoc = ConsumeToken();
  if (Tok.is(tok::identifier)) {
    Action::DeclPtrTy VarDecl = Actions.ActOnSingleDecl(VarLoc, DGK_Var);
    if (ParseVarSpec(VarDecl))
      return Action::DeclPtrTy();
    return VarDecl;
  }
  if (Tok.is(tok::l_paren))
    return ParseDeclGroup(DGK_Var, VarLoc);
  Diag(Tok, diag::expected_ident_or_l_paren);
  SkipUntil(tok::semi, /*StopAtSemi=*/false, /*DontConsume=*/true);
  return Action::DeclPtrTy();
}

/// VarSpec     = IdentifierList
///               ( Type [ "=" ExpressionList ] | "=" ExpressionList ) .
bool Parser::ParseVarSpec(Action::DeclPtrTy VarDecl) {
  assert(Tok.is(tok::identifier) && "Expected identifier");
  IdentifierList IdentList;
  ParseIdentifierList(IdentList);
  if (Tok.isNot(tok::equal) && !IsType()) {
    Diag(Tok, diag::expected_equal_or_type);
    SkipUntil(tok::semi, tok::r_paren, tok::r_brace,
              /*ConsumeSemi=*/false, /*DontConsume=*/true);
    return true;
  }
  if (Tok.isNot(tok::equal))
    ParseType();

  // FIXME: call this later; pass equalloc, rhs
  Actions.ActOnVarSpec(VarDecl, IdentList, getCurScope());

  if (Tok.is(tok::semi) || Tok.is(tok::r_paren) || Tok.is(tok::r_brace))
    return false;
  if (Tok.isNot(tok::equal))
    Diag(Tok, diag::expected_equal);
  else
    ConsumeToken();  // Eat '='.
  ExprVector Exprs(Actions);  // FIXME: use
  return ParseExpressionList(Exprs);
}

Action::DeclPtrTy Parser::ParseDeclGroup(DeclGroupKind Kind,
                                         SourceLocation KWLoc) {
  assert(Tok.is(tok::l_paren) && "Expected '('");
  BalancedDelimiterTracker T(*this, tok::l_paren);
  T.consumeOpen();

  // FIXME: Alternatively, this could collect all the Specs in a vector and
  //        pass them at once to ActOnMultiDecl?
  Action::DeclPtrTy DeclGroup =
      Actions.ActOnStartMultiDecl(KWLoc, T.getOpenLocation(), Kind);

  // FIXME: Similar to importspec block parsing
  while (Tok.isNot(tok::r_paren) && Tok.isNot(tok::eof)) {
    if (Tok.isNot(tok::identifier)) {
      Diag(Tok, diag::expected_ident);
      T.skipToEnd();
      return Action::DeclPtrTy();
    }
    bool Fail;
    switch (Kind) {
    case DGK_Const: Fail = ParseConstSpec(DeclGroup); break;
    case DGK_Type:  Fail = ParseTypeSpec(DeclGroup); break;
    case DGK_Var:   Fail = ParseVarSpec(DeclGroup); break;
    }
    if (Fail) {
      T.skipToEnd();
      // FIXME: This doesn't call ActOnFinishMulti(). Does it matter?
      return Action::DeclPtrTy();
    }

    if (Tok.isNot(tok::semi) && Tok.isNot(tok::r_paren)) {
      Diag(diag::expected_semi);
      SkipUntil(tok::r_paren, /*StopAtSemi=*/true, /*DontConsume=*/true);
    }
    if (Tok.is(tok::semi))
      ConsumeToken();
  }
  T.consumeClose();

  // FIXME: Do this for var and const too
  if (Kind == DGK_Type)
    Actions.ActOnFinishMultiDecl(DeclGroup, T.getCloseLocation());
  return DeclGroup;
}

bool Parser::IsType() {
  switch (Tok.getKind()) {
  default:
    return false;
  case tok::identifier:
  case tok::l_paren:
  case tok::l_square:
  case tok::kw_struct:
  case tok::star:
  case tok::kw_func:
  case tok::kw_interface:
  case tok::kw_map:
  case tok::kw_chan:
  case tok::lessminus:
    return true;
  }
}

bool Parser::IsExpression() {
  // An expression can start with a type (for a conversion), so every
  // type prefix is also an expression prefix.
  return IsType() || IsUnaryOp(Tok.getKind()) || Tok.is(tok::numeric_literal) ||
         Tok.is(tok::rune_literal) || Tok.is(tok::string_literal);
}

DiagnosticBuilder Parser::Diag(SourceLocation Loc, unsigned DiagID) {
  return Diags.Report(Loc, DiagID);
}

DiagnosticBuilder Parser::Diag(const Token &Tok, unsigned DiagID) {
  return Diag(Tok.getLocation(), DiagID);
}

//===----------------------------------------------------------------------===//
// Error recovery.
//===----------------------------------------------------------------------===//

/// SkipUntil - Read tokens until we get to the specified token, then consume
/// it (unless DontConsume is true).  Because we cannot guarantee that the
/// token will ever occur, this skips to the next token, or to some likely
/// good stopping point.  If StopAtSemi is true, skipping will stop at a ';'
/// character.
///
/// If SkipUntil finds the specified token, it returns true, otherwise it
/// returns false.
bool Parser::SkipUntil(ArrayRef<tok::TokenKind> Toks, bool StopAtSemi,
                       bool DontConsume, bool StopAtCodeCompletion) {
  // We always want this function to skip at least one token if the first token
  // isn't T and if not at EOF.
  bool isFirstTokenSkipped = true;
  while (1) {
    // If we found one of the tokens, stop and return true.
    for (unsigned i = 0, NumToks = Toks.size(); i != NumToks; ++i) {
      if (Tok.is(Toks[i])) {
        if (DontConsume) {
          // Noop, don't consume the token.
        } else {
          ConsumeAnyToken();
        }
        return true;
      }
    }

    switch (Tok.getKind()) {
    case tok::eof:
      // Ran out of tokens.
      return false;
        
    case tok::code_completion:
      if (!StopAtCodeCompletion)
        ConsumeToken();
      return false;
        
    case tok::l_paren:
      // Recursively skip properly-nested parens.
      ConsumeParen();
      SkipUntil(tok::r_paren, false, false, StopAtCodeCompletion);
      break;
    case tok::l_square:
      // Recursively skip properly-nested square brackets.
      ConsumeBracket();
      SkipUntil(tok::r_square, false, false, StopAtCodeCompletion);
      break;
    case tok::l_brace:
      // Recursively skip properly-nested braces.
      ConsumeBrace();
      SkipUntil(tok::r_brace, false, false, StopAtCodeCompletion);
      break;

    // Okay, we found a ']' or '}' or ')', which we think should be balanced.
    // Since the user wasn't looking for this token (if they were, it would
    // already be handled), this isn't balanced.  If there is a LHS token at a
    // higher level, we will assume that this matches the unbalanced token
    // and return it.  Otherwise, this is a spurious RHS token, which we skip.
    case tok::r_paren:
      if (ParenCount && !isFirstTokenSkipped)
        return false;  // Matches something.
      ConsumeParen();
      break;
    case tok::r_square:
      if (BracketCount && !isFirstTokenSkipped)
        return false;  // Matches something.
      ConsumeBracket();
      break;
    case tok::r_brace:
      if (BraceCount && !isFirstTokenSkipped)
        return false;  // Matches something.
      ConsumeBrace();
      break;

    case tok::string_literal:
      ConsumeStringToken();
      break;
        
    case tok::semi:
      if (StopAtSemi)
        return false;
      // FALL THROUGH.
    default:
      // Skip this token.
      ConsumeToken();
      break;
    }
    isFirstTokenSkipped = false;
  }
}

/// If a crash happens while the parser is active, print out a line indicating
/// what the current token is.
void PrettyStackTraceParserEntry::print(raw_ostream &OS) const {
  const Token &Tok = P.getCurToken();
  if (Tok.is(tok::eof)) {
    OS << "<eof> parser at end of file\n";
    return;
  }

  if (!Tok.getLocation().isValid()) {
    OS << "<unknown> parser at unknown location\n";
    return;
  }

  const Lexer &L = P.getLexer();
  const llvm::SourceMgr &SM = L.getSourceManager();

  int BufID = SM.FindBufferContainingLoc(Tok.getLocation());
  std::pair<unsigned, unsigned> Pos = SM.getLineAndColumn(Tok.getLocation());
  OS << SM.getMemoryBuffer(BufID)->getBufferIdentifier()
     << ":" << Pos.first << ":" << Pos.second;
  OS << ": current parser token '" << L.getSpelling(Tok) << "'\n";
}

static bool IsCommonTypo(tok::TokenKind ExpectedTok, const Token &Tok) {
  switch (ExpectedTok) {
  case tok::semi:
    return Tok.is(tok::colon) || Tok.is(tok::comma); // : or , for ;
  default: return false;
  }
}

/// ExpectAndConsume - The parser expects that 'ExpectedTok' is next in the
/// input.  If so, it is consumed and false is returned.
///
/// If the input is malformed, this emits the specified diagnostic.  Next, if
/// SkipToTok is specified, it calls SkipUntil(SkipToTok).  Finally, true is
/// returned.
bool Parser::ExpectAndConsume(tok::TokenKind ExpectedTok, unsigned DiagID,
                              const char *Msg, tok::TokenKind SkipToTok) {
  if (Tok.is(ExpectedTok) || Tok.is(tok::code_completion)) {
    ConsumeAnyToken();
    return false;
  }

  // Detect common single-character typos and resume.
  if (IsCommonTypo(ExpectedTok, Tok)) {
    SourceLocation Loc = Tok.getLocation();
    Diag(Loc, DiagID)
      << Msg
      << FixItHint::CreateReplacement(SourceRange(Loc, Loc),
                                      getTokenSimpleSpelling(ExpectedTok));
    ConsumeAnyToken();

    // Pretend there wasn't a problem.
    return false;
  }

  const char *Spelling = 0;
  SourceLocation EndLoc = Lexer::getLocForEndOfToken(PrevTokLocation,
                                                     0, L.getSourceManager());
  if (EndLoc.isValid() &&
      (Spelling = tok::getTokenSimpleSpelling(ExpectedTok))) {
    // Show what code to insert to fix this problem.
    Diag(EndLoc, DiagID)
      << Msg
      << FixItHint::CreateInsertion(EndLoc, Spelling);
  } else
    Diag(Tok, DiagID) << Msg;

  if (SkipToTok != tok::unknown)
    SkipUntil(SkipToTok);
  return true;
}

bool Parser::ExpectAndConsumeSemi(unsigned DiagID) {
  if (Tok.is(tok::semi) || Tok.is(tok::code_completion)) {
    ConsumeToken();
    return false;
  }
  
  // FIXME
  //if ((Tok.is(tok::r_paren) || Tok.is(tok::r_square)) && 
  //    NextToken().is(tok::semi)) {
  //  Diag(Tok, diag::err_extraneous_token_before_semi)
  //    << PP.getSpelling(Tok)
  //    << FixItHint::CreateRemoval(Tok.getLocation());
  //  ConsumeAnyToken(); // The ')' or ']'.
  //  ConsumeToken(); // The ';'.
  //  return false;
  //}
  
  return ExpectAndConsume(tok::semi, DiagID);
}

void Parser::ConsumeExtraSemi(ExtraSemiKind Kind/*, unsigned TST*/) {
  if (!Tok.is(tok::semi)) return;

  bool HadMultipleSemis = false;
  //SourceLocation StartLoc = Tok.getLocation();
  SourceLocation EndLoc = Tok.getLocation();
  ConsumeToken();

  while ((Tok.is(tok::semi) && !Tok.isAtStartOfLine())) {
    HadMultipleSemis = true;
    EndLoc = Tok.getLocation();
    ConsumeToken();
  }

  //// C++11 allows extra semicolons at namespace scope, but not in any of the
  //// other contexts.
  //if (Kind == OutsideFunction && getLangOpts().CPlusPlus) {
  //  if (getLangOpts().CPlusPlus0x)
  //    Diag(StartLoc, diag::warn_cxx98_compat_top_level_semi)
  //        << FixItHint::CreateRemoval(SourceRange(StartLoc, EndLoc));
  //  else
  //    Diag(StartLoc, diag::ext_extra_semi_cxx11)
  //        << FixItHint::CreateRemoval(SourceRange(StartLoc, EndLoc));
  //  return;
  //}

  //if (Kind != AfterMemberFunctionDefinition || HadMultipleSemis)
  //  Diag(StartLoc, diag::ext_extra_semi)
  //      << Kind << DeclSpec::getSpecifierName((DeclSpec::TST)TST)
  //      << FixItHint::CreateRemoval(SourceRange(StartLoc, EndLoc));
  //else
  //  // A single semicolon is valid after a member function definition.
  //  Diag(StartLoc, diag::warn_extra_semi_after_mem_fn_def)
  //    << FixItHint::CreateRemoval(SourceRange(StartLoc, EndLoc));
}

//===----------------------------------------------------------------------===//
// Scope manipulation
//===----------------------------------------------------------------------===//

/// Scope - Start a new scope.
void Parser::EnterScope(unsigned ScopeFlags) {
  if (NumCachedScopes) {
    Scope *N = ScopeCache[--NumCachedScopes];
    N->Init(getCurScope(), ScopeFlags);
    Actions.CurScope = N;
  } else {
    Actions.CurScope = new Scope(getCurScope(), ScopeFlags);
  }
}

/// Pop a scope off the scope stack.
void Parser::ExitScope() {
  assert(getCurScope() && "Scope imbalance!");

  // Inform the actions module that this scope is going away if there are any
  // decls in it.
  if (!getCurScope()->decl_empty())
    Actions.ActOnPopScope(Tok.getLocation(), getCurScope());

  Scope *OldScope = getCurScope();
  Actions.CurScope = OldScope->getParent();

  if (NumCachedScopes == ScopeCacheSize)
    delete OldScope;
  else
    ScopeCache[NumCachedScopes++] = OldScope;
}

//===----------------------------------------------------------------------===//
// Delimiter tracking
//===----------------------------------------------------------------------===//

bool BalancedDelimiterTracker::diagnoseOverflow() {
  P.Diag(P.Tok, diag::parser_impl_limit_overflow);
  P.SkipUntil(tok::eof);
  return true;  
}

bool BalancedDelimiterTracker::expectAndConsume(unsigned DiagID,
                                            const char *Msg,
                                            tok::TokenKind SkipToToc ) {
  LOpen = P.Tok.getLocation();
  if (P.ExpectAndConsume(Kind, DiagID, Msg, SkipToToc))
    return true;
  
  if (getDepth() < MaxDepth)
    return false;
    
  return diagnoseOverflow();
}

bool BalancedDelimiterTracker::diagnoseMissingClose() {
  assert(!P.Tok.is(Close) && "Should have consumed closing delimiter");
  
  const char *LHSName = "unknown";
  diag::kind DID;
  switch (Close) {
  default: llvm_unreachable("Unexpected balanced token");
  case tok::r_paren : LHSName = "("; DID = diag::expected_r_paren; break;
  case tok::r_brace : LHSName = "{"; DID = diag::expected_r_brace; break;
  case tok::r_square: LHSName = "["; DID = diag::expected_r_square; break;
  }
  P.Diag(P.Tok, DID);
  P.Diag(LOpen, diag::note_matching) << LHSName;
  if (P.SkipUntil(Close, /*StopAtSemi*/ true, /*DontConsume*/ true))
    LClose = P.ConsumeAnyToken();
  return true;
}

void BalancedDelimiterTracker::skipToEnd() {
  P.SkipUntil(Close, false);
}
