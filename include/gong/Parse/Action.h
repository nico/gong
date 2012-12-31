//===--- Action.h - Parser Action Interface ---------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Action interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_PARSE_ACTION_H
#define LLVM_GONG_PARSE_ACTION_H

#include "gong/Basic/IdentifierTable.h"
#include "gong/Basic/SourceLocation.h"
#include "gong/Basic/Specifiers.h"
//#include "gong/Basic/TemplateKinds.h"
//#include "gong/Basic/TypeTraits.h"
//#include "gong/Parse/DeclSpec.h"
#include "gong/Parse/Ownership.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/ADT/PointerUnion.h"

namespace gong {
  class UnqualifiedId;
  // Semantic.
  class DeclSpec;
  class ObjCDeclSpec;
  class CXXScopeSpec;
  class Declarator;
  class AttributeList;
  struct FieldDeclarator;
  // Parse.
  class IdentifierList;
  class Scope;
  class Action;
  class Selector;
  class Designation;
  class InitListDesignations;
  // Lex.
  class Lexer;
  class Token;

  // We can re-use the low bit of expression, statement, base, and
  // member-initializer pointers for the "invalid" flag of
  // ActionResult.
  template<> struct IsResultPtrLowBitFree<0> { static const bool value = true;};
  template<> struct IsResultPtrLowBitFree<1> { static const bool value = true;};
  template<> struct IsResultPtrLowBitFree<3> { static const bool value = true;};
  template<> struct IsResultPtrLowBitFree<4> { static const bool value = true;};
  template<> struct IsResultPtrLowBitFree<5> { static const bool value = true;};

/// As the parser reads the input file and recognizes the productions of the
/// grammar, it invokes methods on this class to turn the parsed input into
/// something useful: e.g. a parse tree.
///
/// The callback methods that this class provides are phrased as actions that
/// the parser has just done or is about to do when the method is called.  They
/// are not requests that the actions module do the specified action.
///
/// All of the methods here are optional except classifyIdentifier()  which
/// must be specified in order for the parse to complete accurately.  The
/// MinimalAction class does this bare-minimum of tracking to implement this
/// functionality.
class Action : public ActionBase {
  /// \brief The parser's current scope.
  ///
  /// The parser maintains this state here so that is accessible to \c Action 
  /// subclasses via \c getCurScope().
  Scope *CurScope;
  
protected:
  friend class Parser;
  
  /// \brief Retrieve the parser's current scope.
  Scope *getCurScope() const { return CurScope; }
  
public:
  Action() : CurScope(0) { }
  
  /// Out-of-line virtual destructor to provide home for this class.
  virtual ~Action();

  // Types - Though these don't actually enforce strong typing, they document
  // what types are required to be identical for the actions.
  typedef ActionBase::ExprTy ExprTy;
  typedef ActionBase::StmtTy StmtTy;

  /// Expr/Stmt/Type/BaseResult - Provide a unique type to wrap
  /// ExprTy/StmtTy/TypeTy/BaseTy, providing strong typing and
  /// allowing for failure.
  typedef ActionResult<0> ExprResult;
  typedef ActionResult<1> StmtResult;
  typedef ActionResult<2> TypeResult;
  typedef ActionResult<3> BaseResult;
  typedef ActionResult<4> MemInitResult;
  typedef ActionResult<5, DeclPtrTy> DeclResult;

  /// Same, but with ownership.
  typedef ASTOwningResult<&ActionBase::DeleteExpr> OwningExprResult;
  typedef ASTOwningResult<&ActionBase::DeleteStmt> OwningStmtResult;
  // Note that these will replace ExprResult and StmtResult when the transition
  // is complete.

  /// Single expressions or statements as arguments.
  typedef ASTOwningPtr<&ActionBase::DeleteExpr> ExprArg;
  typedef ASTOwningPtr<&ActionBase::DeleteStmt> StmtArg;

  /// Multiple expressions or statements as arguments.
  typedef ASTMultiPtr<&ActionBase::DeleteExpr> MultiExprArg;
  typedef ASTMultiPtr<&ActionBase::DeleteStmt> MultiStmtArg;
  typedef ASTMultiPtr<&ActionBase::DeleteTemplateParams> MultiTemplateParamsArg;

  class FullExprArg {
  public:
    FullExprArg(ActionBase &actions) : Expr(actions) { }
                
    // FIXME: The const_cast here is ugly. RValue references would make this
    // much nicer (or we could duplicate a bunch of the move semantics
    // emulation code from Ownership.h).
    FullExprArg(const FullExprArg& Other)
      : Expr(move(const_cast<FullExprArg&>(Other).Expr)) {}

    FullExprArg &operator=(const FullExprArg& Other) {
      Expr.operator=(move(const_cast<FullExprArg&>(Other).Expr));
      return *this;
    }

    OwningExprResult release() {
      return move(Expr);
    }

    ExprArg* operator->() {
      return &Expr;
    }

  private:
    // FIXME: No need to make the entire Action class a friend when it's just
    // Action::FullExpr that needs access to the constructor below.
    friend class Action;

    explicit FullExprArg(ExprArg expr)
      : Expr(move(expr)) {}

    ExprArg Expr;
  };

  template<typename T>
  FullExprArg MakeFullExpr(T &Arg) {
      return FullExprArg(ActOnFinishFullExpr(move(Arg)));
  }

  // Utilities for Action implementations to return smart results.

  OwningExprResult ExprError() { return OwningExprResult(*this, true); }
  OwningStmtResult StmtError() { return OwningStmtResult(*this, true); }

  OwningExprResult ExprError(const DiagnosticBuilder&) { return ExprError(); }
  OwningStmtResult StmtError(const DiagnosticBuilder&) { return StmtError(); }

  OwningExprResult ExprEmpty() { return OwningExprResult(*this, false); }
  OwningStmtResult StmtEmpty() { return OwningStmtResult(*this, false); }

  /// Statistics.
  virtual void PrintStats() const {}

  /// getDeclName - Return a pretty name for the specified decl if possible, or
  /// an empty string if not.  This is used for pretty crash reporting.
  virtual std::string getDeclName(DeclPtrTy D) { return ""; }

  //===--------------------------------------------------------------------===//
  // Import Decl handling.
  //===--------------------------------------------------------------------===//

  /// Called after a single import spec has been parsed.
  /// in the current scope.
  ///
  /// \param ImportPath The path to the package to be imported. Always valid.
  ///
  /// \param LocalName For |import M "lib/math"|, this is the M. Else NULL.
  ///
  /// \param IsLocal True for |import . "lib/math"| imports. If this is true,
  /// LocalName will be NULL.
  //FIXME: Needs More SourceLocation params
  virtual void ActOnImportSpec(SourceLocation PathLoc, StringRef ImportPath,
                               IdentifierInfo *LocalName,
                               bool IsLocal) {}
  // FIXME: Need to distinguish between one-line and grouped ImportDecls
  // Maybe ActOnImportSpec could return a decl, which is then passed on to
  // ActOnSingleImportDecl() / ActOnMultiImportDecl(). Or there's
  // ActOnBeginSingle/MultiImportDecl / ActOnFinishSingle/MultiImportDecl.

  //===--------------------------------------------------------------------===//
  // Declaration Tracking Callbacks.
  //===--------------------------------------------------------------------===//

  /// Describes the type of an identifier. Go has a shared namespace for
  /// package and type names and identifiers (i.e. a local variable |int|
  /// shadows the type |int|).
  enum IdentifierInfoType {
    IIT_Package,
    IIT_Type,
    IIT_Const,
    IIT_Var,
    IIT_Func,
    IIT_BuiltinFunc,
    IIT_Unknown
  };

  /// Determines if |II| names a package, a type, or an identifier (variable or
  /// function name).
  ///
  /// \param II the identifier for which we are performing name lookup
  ///
  /// \param S the scope in which this name lookup occurs
  virtual IdentifierInfoType classifyIdentifier(const IdentifierInfo &II,
                                                const Scope* S) = 0;

  /// Called after the 'type' in a TypeDecl without parentheses has been
  /// parsed.  ActOnTypeSpec() will be called once after.
  virtual DeclPtrTy ActOnSingleDecl(SourceLocation TypeLoc,
                                    DeclGroupKind DeclKind) {
    return DeclPtrTy();
  }

  /// Called after 'type (' in a TypeDecl with parentheses has been parsed.
  /// ActOnTypeSpec() will be called for each TypeSpec in this TypeDecl.
  virtual DeclPtrTy ActOnStartMultiDecl(SourceLocation TypeLoc,
                                        SourceLocation LParenLoc,
                                        DeclGroupKind DeclKind) {
    return DeclPtrTy();
  }
  /// Called after the closing ')' of a TypeDecl has been parsed.
  virtual void ActOnFinishMultiDecl(DeclPtrTy Decl,
                                    SourceLocation RParenLoc) {
  }

  /// Called after a const spec has been parsed.
  ///
  /// \param Decl The Decl returned by either ActOnSingleTypeDecl() or
  ///             ActOnStartMultiDecl().
  // FIXME: Pass rhs
  virtual void ActOnConstSpec(DeclPtrTy Decl, IdentifierList &Idents,
                              Scope *S) {}

  /// Called after a type spec has been parsed.
  ///
  /// \param Decl The Decl returned by either ActOnSingleTypeDecl() or
  ///             ActOnStartMultiDecl().
  virtual void ActOnTypeSpec(DeclPtrTy Decl, SourceLocation IILoc,
                             IdentifierInfo &II, Scope *S) {}

  /// Called after a var spec has been parsed.
  ///
  /// \param Decl The Decl returned by either ActOnSingleTypeDecl() or
  ///             ActOnStartMultiDecl().
  // FIXME: Pass rhs
  virtual void ActOnVarSpec(DeclPtrTy Decl, IdentifierList &Idents, Scope *S) {}


  /// Registers an identifier as function name.
  ///
  /// \param II The name of the new function.
  virtual DeclPtrTy ActOnFunctionDecl(SourceLocation FuncLoc,
                                      SourceLocation NameLoc,
                                      IdentifierInfo &II, Scope *S) {
    return DeclPtrTy();
  }

  /// This is called at the start of a function definition, after the
  /// FunctionDecl has already been created.
  virtual void ActOnStartOfFunctionDef(DeclPtrTy Fun, Scope *FnBodyScope) { }

  /// This is called when a function body has completed parsing.  Decl is
  /// returned by ParseStartOfFunctionDef.
  virtual void ActOnFinishFunctionBody(DeclPtrTy Decl, StmtArg Body) { }


  //===--------------------------------------------------------------------===//
  // Type Parsing Callbacks.
  //===--------------------------------------------------------------------===//

  //===--------------------------------------------------------------------===//
  // Statement Parsing Callbacks.
  //===--------------------------------------------------------------------===//

  /// \brief Parsed a block.
  ///
  /// \param \L the location of the opening '{'.
  ///
  /// \param \R the location of the closing '}'.
  ///
  /// \param Elts the statements in the block.
  virtual OwningStmtResult ActOnBlockStmt(SourceLocation L, SourceLocation R,
                                          MultiStmtArg Elts) {
    return StmtEmpty();
  }

  /// \brief Parsed a labeled statement.
  ///
  /// \param IILoc the location of the label.
  ///
  /// \param II the label.
  ///
  /// \param ColonLoc the location of the colon following the label.
  ///
  /// \param SubStmt the statement following the label.
  virtual OwningStmtResult ActOnLabeledStmt(SourceLocation IILoc,
                                            IdentifierInfo *II,
                                            SourceLocation ColonLoc,
                                            StmtArg SubStmt) {
    return StmtEmpty();
  }

  /// \brief Parsed an empty statement.
  virtual OwningStmtResult ActOnEmptyStmt(SourceLocation SemiLoc) {
    return StmtEmpty();
  }

  /// \brief Parsed a SendStmt such as "a <- b".
  ///
  /// \param LHS the left-hand side of the send statement.
  ///
  /// \param OpLoc the location of the "<-" operator.
  ///
  /// \param RHS the right-hand side of the send statement.
  virtual OwningStmtResult ActOnSendStmt(ExprArg LHS,
                                         SourceLocation OpLoc,
                                         ExprArg RHS) {
    return StmtEmpty();
  }
  /// \brief Parsed an IncDecStmt, for example "a++" or "a[i]--".
  ///
  /// \param Base the expression in front of the inc or dec operator.
  ///
  /// \param OpLoc the location of the "++" or "--" operator.
  ///
  /// \param OpKind the kind of operator, either tok::plusplus ("++") or
  /// tok::minusminus ("--").
  virtual OwningStmtResult ActOnIncDecStmt(ExprArg Base,
                                           SourceLocation OpLoc,
                                           tok::TokenKind OpKind) {
    return StmtEmpty();
  }

  /// \brief Parsed a "go" statement.
  ///
  /// \param GoLoc the location of the "go" keyword.
  ///
  /// \param Exp the go expression.
  virtual OwningStmtResult ActOnGoStmt(SourceLocation GoLoc, ExprArg Exp) {
    return StmtEmpty();
  }

  /// \brief Parsed a "return" statement.
  ///
  /// \param ReturnLoc the location of the "return" keyword.
  ///
  /// \param Exp the optional return expression.
  //FIXME: comma locs. Have an ExprList, similar to IdentList?
  virtual OwningStmtResult ActOnReturnStmt(SourceLocation ReturnLoc,
                                           MultiExprArg RetValExps) {
    return StmtEmpty();
  }

  /// \brief Parsed a "break" statement.
  ///
  /// \param BreakLoc the location of the "break" keyword.
  ///
  /// \param LabelLoc the location of the optional break label.
  ///
  /// \param Label the optional break label.
  ///
  /// \param CurScope the current scope.
  virtual OwningStmtResult ActOnBreakStmt(SourceLocation BreakLoc,
                                          SourceLocation LabelLoc,
                                          IdentifierInfo *Label,
                                          Scope *CurScope) {
    return StmtEmpty();
  }

  /// \brief Parsed a "continue" statement.
  ///
  /// \param ContinueLoc the location of the "continue" keyword.
  ///
  /// \param LabelLoc the location of the optional continue label.
  ///
  /// \param Label the optional continue label.
  ///
  /// \param CurScope the current scope.
  virtual OwningStmtResult ActOnContinueStmt(SourceLocation ContinueLoc,
                                             SourceLocation LabelLoc,
                                             IdentifierInfo *Label,
                                             Scope *CurScope) {
    return StmtEmpty();
  }

  /// \brief Parsed a "goto" statement.
  ///
  /// \param GotoLoc the location of the "goto" keyword.
  ///
  /// \param LabelLoc the location of the goto label.
  ///
  /// \param Label the goto label.
  virtual OwningStmtResult ActOnGotoStmt(SourceLocation GotoLoc,
                                         SourceLocation LabelLoc,
                                         IdentifierInfo *Label) {
    return StmtEmpty();
  }

  /// \brief Parsed a "fallthrough" statement.
  ///
  /// \param GotoLoc the location of the "fallthrough" keyword.
  virtual OwningStmtResult ActOnFallthroughStmt(SourceLocation FallthroughLoc) {
    return StmtEmpty();
  }

  // FIXME: actions for:
  // declarationstmt
  // expressionstmt
  // assignment
  // shortvardecl
  // switchstmt
  // selectstmt
  // forstmt

  /// \brief Parsed an "if" statement.
  ///
  /// Example: "if b := f(); b {} else {}"
  ///
  /// \param IfLoc the location of the "if" keyword.
  ///
  /// \param InitStmt the optional initial SimpleStmt.
  ///
  /// \param CondVal the if condition.
  ///
  /// \param ThenVal the "then" statement.
  ///
  /// \param ElseLoc the location of the "else" keyword.
  ///
  /// \param ElseVal the optional "else" statement.
  virtual OwningStmtResult ActOnIfStmt(SourceLocation IfLoc,
                                       StmtArg InitStmt,
                                       ExprArg CondVal, 
                                       StmtArg ThenVal,
                                       SourceLocation ElseLoc,
                                       StmtArg ElseVal) {
    return StmtEmpty();
  }

  /// \brief Parsed a "defer" statement.
  ///
  /// \param DeferLoc the location of the "defer" keyword.
  ///
  /// \param Exp the deferred expression.
  virtual OwningStmtResult ActOnDeferStmt(SourceLocation DeferLoc,
                                          ExprArg Exp) {
    return StmtEmpty();
  }

  //===--------------------------------------------------------------------===//
  // Expression Parsing Callbacks.
  //===--------------------------------------------------------------------===//


















  typedef uintptr_t ParsingDeclStackState;

  /// PushParsingDeclaration - Notes that the parser has begun
  /// processing a declaration of some sort.  Guaranteed to be matched
  /// by a call to PopParsingDeclaration with the value returned by
  /// this method.
  virtual ParsingDeclStackState PushParsingDeclaration() {
    return ParsingDeclStackState();
  }

  /// PopParsingDeclaration - Notes that the parser has completed
  /// processing a declaration of some sort.  The decl will be empty
  /// if the declaration didn't correspond to a full declaration (or
  /// if the actions module returned an empty decl for it).
  virtual void PopParsingDeclaration(ParsingDeclStackState S, DeclPtrTy D) {
  }

  /// ConvertDeclToDeclGroup - If the parser has one decl in a context where it
  /// needs a decl group, it calls this to convert between the two
  /// representations.
  virtual DeclGroupPtrTy ConvertDeclToDeclGroup(DeclPtrTy Ptr) {
    return DeclGroupPtrTy();
  }

  virtual void DiagnoseUseOfUnimplementedSelectors() {}

  /// getTypeName - Return non-null if the specified identifier is a type name
  /// in the current scope.
  ///
  /// \param II the identifier for which we are performing name lookup
  ///
  /// \param NameLoc the location of the identifier
  ///
  /// \param S the scope in which this name lookup occurs
  ///
  /// \param SS if non-NULL, the C++ scope specifier that precedes the
  /// identifier
  ///
  /// \param isClassName whether this is a C++ class-name production, in
  /// which we can end up referring to a member of an unknown specialization
  /// that we know (from the grammar) is supposed to be a type. For example,
  /// this occurs when deriving from "std::vector<T>::allocator_type", where T
  /// is a template parameter.
  ///
  /// \param ObjectType if we're checking whether an identifier is a type
  /// within a C++ member access expression, this will be the type of the 
  /// 
  /// \returns the type referred to by this identifier, or NULL if the type
  /// does not name an identifier.
  //virtual TypeTy *getTypeName(IdentifierInfo &II, SourceLocation NameLoc,
  //                            Scope *S, CXXScopeSpec *SS = 0,
  //                            bool isClassName = false,
  //                            TypeTy *ObjectType = 0) = 0;

  /// isTagName() - This method is called *for error recovery purposes only*
  /// to determine if the specified name is a valid tag name ("struct foo").  If
  /// so, this returns the TST for the tag corresponding to it (TST_enum,
  /// TST_union, TST_struct, TST_class).  This is used to diagnose cases in C
  /// where the user forgot to specify the tag.
  //virtual DeclSpec::TST isTagName(IdentifierInfo &II, Scope *S) {
  //  return DeclSpec::TST_unspecified;
  //}

  /// \brief Action called as part of error recovery when the parser has 
  /// determined that the given name must refer to a type, but 
  /// \c getTypeName() did not return a result.
  ///
  /// This callback permits the action to give a detailed diagnostic when an
  /// unknown type name is encountered and, potentially, to try to recover
  /// by producing a new type in \p SuggestedType.
  ///
  /// \param II the name that should be a type.
  ///
  /// \param IILoc the location of the name in the source.
  ///
  /// \param S the scope in which name lookup was performed.
  ///
  /// \param SS if non-NULL, the C++ scope specifier that preceded the name.
  ///
  /// \param SuggestedType if the action sets this type to a non-NULL type,
  /// the parser will recovery by consuming the type name token and then 
  /// pretending that the given type was the type it parsed.
  ///
  /// \returns true if a diagnostic was emitted, false otherwise. When false,
  /// the parser itself will emit a generic "unknown type name" diagnostic.
  virtual bool DiagnoseUnknownTypeName(const IdentifierInfo &II, 
                                       SourceLocation IILoc,
                                       Scope *S,
                                       CXXScopeSpec *SS,
                                       TypeTy *&SuggestedType) {
    return false;
  }
                                       
  /// \brief Determine whether the given name refers to a template.
  ///
  /// This callback is used by the parser after it has seen a '<' to determine
  /// whether the given name refers to a template and, if so, what kind of 
  /// template.
  ///
  /// \param S the scope in which the name occurs.
  ///
  /// \param SS the C++ nested-name-specifier that precedes the template name,
  /// if any.
  ///
  /// \param Name the name that we are querying to determine whether it is
  /// a template.
  ///
  /// \param ObjectType if we are determining whether the given name is a 
  /// template name in the context of a member access expression (e.g., 
  /// \c p->X<int>), this is the type of the object referred to by the
  /// member access (e.g., \c p).
  ///
  /// \param EnteringContext whether we are potentially entering the context
  /// referred to by the nested-name-specifier \p SS, which allows semantic
  /// analysis to look into uninstantiated templates.
  ///
  /// \param Template if the name does refer to a template, the declaration
  /// of the template that the name refers to.
  ///
  /// \param MemberOfUnknownSpecialization Will be set true if the resulting 
  /// member would be a member of an unknown specialization, in which case this
  /// lookup cannot possibly pass at this time.
  ///
  /// \returns the kind of template that this name refers to.
  //virtual TemplateNameKind isTemplateName(Scope *S,
  //                                        CXXScopeSpec &SS,
  //                                        UnqualifiedId &Name,
  //                                        TypeTy *ObjectType,
  //                                        bool EnteringContext,
  //                                        TemplateTy &Template,
  //                                    bool &MemberOfUnknownSpecialization) = 0;

  /// \brief Action called as part of error recovery when the parser has 
  /// determined that the given name must refer to a template, but 
  /// \c isTemplateName() did not return a result.
  ///
  /// This callback permits the action to give a detailed diagnostic when an
  /// unknown template name is encountered and, potentially, to try to recover
  /// by producing a new template in \p SuggestedTemplate.
  ///
  /// \param II the name that should be a template.
  ///
  /// \param IILoc the location of the name in the source.
  ///
  /// \param S the scope in which name lookup was performed.
  ///
  /// \param SS the C++ scope specifier that preceded the name.
  ///
  /// \param SuggestedTemplate if the action sets this template to a non-NULL,
  /// template, the parser will recover by consuming the template name token
  /// and the template argument list that follows.
  ///
  /// \param SuggestedTemplateKind as input, the kind of template that we
  /// expect (e.g., \c TNK_Type_template or \c TNK_Function_template). If the
  /// action provides a suggested template, this should be set to the kind of
  /// template.
  ///
  /// \returns true if a diagnostic was emitted, false otherwise. When false,
  /// the parser itself will emit a generic "unknown template name" diagnostic.
  //virtual bool DiagnoseUnknownTemplateName(const IdentifierInfo &II, 
  //                                         SourceLocation IILoc,
  //                                         Scope *S,
  //                                         const CXXScopeSpec *SS,
  //                                         TemplateTy &SuggestedTemplate,
  //                                         TemplateNameKind &SuggestedKind) {
  //  return false;
  //}
  
  /// \brief Determine whether the given name refers to a non-type nested name
  /// specifier, e.g., the name of a namespace or namespace alias.
  ///
  /// This actual is used in the parsing of pseudo-destructor names to 
  /// distinguish a nested-name-specifier and a "type-name ::" when we
  /// see the token sequence "X :: ~".
  virtual bool isNonTypeNestedNameSpecifier(Scope *S, CXXScopeSpec &SS,
                                            SourceLocation IdLoc,
                                            IdentifierInfo &II,
                                            TypeTy *ObjectType) {
    return false;
  }
  
  /// ActOnCXXGlobalScopeSpecifier - Return the object that represents the
  /// global scope ('::').
  virtual CXXScopeTy *ActOnCXXGlobalScopeSpecifier(Scope *S,
                                                   SourceLocation CCLoc) {
    return 0;
  }
  
  /// \brief Parsed an identifier followed by '::' in a C++
  /// nested-name-specifier.
  ///
  /// \param S the scope in which the nested-name-specifier was parsed.
  ///
  /// \param SS the nested-name-specifier that precedes the identifier. For
  /// example, if we are parsing "foo::bar::", \p SS will describe the "foo::"
  /// that has already been parsed.
  ///
  /// \param IdLoc the location of the identifier we have just parsed (e.g.,
  /// the "bar" in "foo::bar::".
  ///
  /// \param CCLoc the location of the '::' at the end of the
  /// nested-name-specifier.
  ///
  /// \param II the identifier that represents the scope that this
  /// nested-name-specifier refers to, e.g., the "bar" in "foo::bar::".
  ///
  /// \param ObjectType if this nested-name-specifier occurs as part of a
  /// C++ member access expression such as "x->Base::f", the type of the base
  /// object (e.g., *x in the example, if "x" were a pointer).
  ///
  /// \param EnteringContext if true, then we intend to immediately enter the
  /// context of this nested-name-specifier, e.g., for an out-of-line
  /// definition of a class member.
  ///
  /// \returns a CXXScopeTy* object representing the C++ scope.
  virtual CXXScopeTy *ActOnCXXNestedNameSpecifier(Scope *S,
                                                  CXXScopeSpec &SS,
                                                  SourceLocation IdLoc,
                                                  SourceLocation CCLoc,
                                                  IdentifierInfo &II,
                                                  TypeTy *ObjectType,
                                                  bool EnteringContext) {
    return 0;
  }
  
  /// IsInvalidUnlessNestedName - This method is used for error recovery
  /// purposes to determine whether the specified identifier is only valid as
  /// a nested name specifier, for example a namespace name.  It is
  /// conservatively correct to always return false from this method.
  ///
  /// The arguments are the same as those passed to ActOnCXXNestedNameSpecifier.
  virtual bool IsInvalidUnlessNestedName(Scope *S,
                                         CXXScopeSpec &SS,
                                         IdentifierInfo &II,
                                         TypeTy *ObjectType,
                                         bool EnteringContext) {
    return false;
  }

  /// ActOnCXXNestedNameSpecifier - Called during parsing of a
  /// nested-name-specifier that involves a template-id, e.g.,
  /// "foo::bar<int, float>::", and now we need to build a scope
  /// specifier. \p SS is empty or the previously parsed nested-name
  /// part ("foo::"), \p Type is the already-parsed class template
  /// specialization (or other template-id that names a type), \p
  /// TypeRange is the source range where the type is located, and \p
  /// CCLoc is the location of the trailing '::'.
  virtual CXXScopeTy *ActOnCXXNestedNameSpecifier(Scope *S,
                                                  const CXXScopeSpec &SS,
                                                  TypeTy *Type,
                                                  SourceRange TypeRange,
                                                  SourceLocation CCLoc) {
    return 0;
  }

  /// ShouldEnterDeclaratorScope - Called when a C++ scope specifier
  /// is parsed as part of a declarator-id to determine whether a scope
  /// should be entered.
  ///
  /// \param S the current scope
  /// \param SS the scope being entered
  /// \param isFriendDeclaration whether this is a friend declaration
  virtual bool ShouldEnterDeclaratorScope(Scope *S, const CXXScopeSpec &SS) {
    return false;
  }

  /// ActOnCXXEnterDeclaratorScope - Called when a C++ scope specifier (global
  /// scope or nested-name-specifier) is parsed as part of a declarator-id.
  /// After this method is called, according to [C++ 3.4.3p3], names should be
  /// looked up in the declarator-id's scope, until the declarator is parsed and
  /// ActOnCXXExitDeclaratorScope is called.
  /// The 'SS' should be a non-empty valid CXXScopeSpec.
  /// \returns true if an error occurred, false otherwise.
  virtual bool ActOnCXXEnterDeclaratorScope(Scope *S, CXXScopeSpec &SS) {
    return false;
  }

  /// ActOnCXXExitDeclaratorScope - Called when a declarator that previously
  /// invoked ActOnCXXEnterDeclaratorScope(), is finished. 'SS' is the same
  /// CXXScopeSpec that was passed to ActOnCXXEnterDeclaratorScope as well.
  /// Used to indicate that names should revert to being looked up in the
  /// defining scope.
  virtual void ActOnCXXExitDeclaratorScope(Scope *S, const CXXScopeSpec &SS) {
  }

  /// ActOnCXXEnterDeclInitializer - Invoked when we are about to parse an
  /// initializer for the declaration 'Dcl'.
  /// After this method is called, according to [C++ 3.4.1p13], if 'Dcl' is a
  /// static data member of class X, names should be looked up in the scope of
  /// class X.
  virtual void ActOnCXXEnterDeclInitializer(Scope *S, DeclPtrTy Dcl) {
  }

  /// ActOnCXXExitDeclInitializer - Invoked after we are finished parsing an
  /// initializer for the declaration 'Dcl'.
  virtual void ActOnCXXExitDeclInitializer(Scope *S, DeclPtrTy Dcl) {
  }

  /// ActOnDeclarator - This callback is invoked when a declarator is parsed and
  /// 'Init' specifies the initializer if any.  This is for things like:
  /// "int X = 4" or "typedef int foo".
  ///
  virtual DeclPtrTy ActOnDeclarator(Scope *S, Declarator &D) {
    return DeclPtrTy();
  }

  /// ActOnParamDeclarator - This callback is invoked when a parameter
  /// declarator is parsed. This callback only occurs for functions
  /// with prototypes. S is the function prototype scope for the
  /// parameters (C++ [basic.scope.proto]).
  virtual DeclPtrTy ActOnParamDeclarator(Scope *S, Declarator &D) {
    return DeclPtrTy();
  }

  /// \brief Parsed an exception object declaration within an Objective-C
  /// @catch statement.
  virtual DeclPtrTy ActOnObjCExceptionDecl(Scope *S, Declarator &D) {
    return DeclPtrTy();
  }

  /// AddInitializerToDecl - This action is called immediately after
  /// ActOnDeclarator (when an initializer is present). The code is factored
  /// this way to make sure we are able to handle the following:
  ///   void func() { int xx = xx; }
  /// This allows ActOnDeclarator to register "xx" prior to parsing the
  /// initializer. The declaration above should still result in a warning,
  /// since the reference to "xx" is uninitialized.
  virtual void AddInitializerToDecl(DeclPtrTy Dcl, ExprArg Init) {
    return;
  }

  /// SetDeclDeleted - This action is called immediately after ActOnDeclarator
  /// if =delete is parsed. C++0x [dcl.fct.def]p10
  /// Note that this can be called even for variable declarations. It's the
  /// action's job to reject it.
  virtual void SetDeclDeleted(DeclPtrTy Dcl, SourceLocation DelLoc) {
    return;
  }

  /// ActOnUninitializedDecl - This action is called immediately after
  /// ActOnDeclarator (when an initializer is *not* present).
  /// If TypeContainsUndeducedAuto is true, then the type of the declarator
  /// has an undeduced 'auto' type somewhere.
  virtual void ActOnUninitializedDecl(DeclPtrTy Dcl,
                                      bool TypeContainsUndeducedAuto) {
    return;
  }

  /// \brief Note that the given declaration had an initializer that could not
  /// be parsed.
  virtual void ActOnInitializerError(DeclPtrTy Dcl) {
    return;
  }
  
  /// FinalizeDeclaratorGroup - After a sequence of declarators are parsed, this
  /// gives the actions implementation a chance to process the group as a whole.
  virtual DeclGroupPtrTy FinalizeDeclaratorGroup(Scope *S, const DeclSpec& DS,
                                                 DeclPtrTy *Group,
                                                 unsigned NumDecls) {
    return DeclGroupPtrTy();
  }


  /// @brief Indicates that all K&R-style parameter declarations have
  /// been parsed prior to a function definition.
  /// @param S  The function prototype scope.
  /// @param D  The function declarator.
  virtual void ActOnFinishKNRParamDeclarations(Scope *S, Declarator &D,
                                               SourceLocation LocAfterDecls) {
  }

  virtual DeclPtrTy ActOnFileScopeAsmDecl(SourceLocation Loc,
                                          ExprArg AsmString) {
    return DeclPtrTy();
  }

  /// ActOnPopScope - This callback is called immediately before the specified
  /// scope is popped and deleted.
  virtual void ActOnPopScope(SourceLocation Loc, Scope *S) {}

  // FIXME: Rename to FileScope (there's also Universe and Package scope)
  /// ActOnTranslationUnitScope - This callback is called once, immediately
  /// after creating the translation unit scope (in Parser::Initialize).
  virtual void ActOnTranslationUnitScope(Scope *S) {}

  /// ParsedFreeStandingDeclSpec - This method is invoked when a declspec with
  /// no declarator (e.g. "struct foo;") is parsed.
  virtual DeclPtrTy ParsedFreeStandingDeclSpec(Scope *S,
                                               //AccessSpecifier Access,
                                               DeclSpec &DS) {
    return DeclPtrTy();
  }

  /// ActOnStartLinkageSpecification - Parsed the beginning of a C++
  /// linkage specification, including the language and (if present)
  /// the '{'. ExternLoc is the location of the 'extern', LangLoc is
  /// the location of the language string literal, which is provided
  /// by Lang/StrSize. LBraceLoc, if valid, provides the location of
  /// the '{' brace. Otherwise, this linkage specification does not
  /// have any braces.
  virtual DeclPtrTy ActOnStartLinkageSpecification(Scope *S,
                                                   SourceLocation ExternLoc,
                                                   SourceLocation LangLoc,
                                                   llvm::StringRef Lang,
                                                   SourceLocation LBraceLoc) {
    return DeclPtrTy();
  }

  /// ActOnFinishLinkageSpecification - Completely the definition of
  /// the C++ linkage specification LinkageSpec. If RBraceLoc is
  /// valid, it's the position of the closing '}' brace in a linkage
  /// specification that uses braces.
  virtual DeclPtrTy ActOnFinishLinkageSpecification(Scope *S,
                                                    DeclPtrTy LinkageSpec,
                                                    SourceLocation RBraceLoc) {
    return LinkageSpec;
  }

  /// ActOnEndOfTranslationUnit - This is called at the very end of the
  /// translation unit when EOF is reached and all but the top-level scope is
  /// popped.
  virtual void ActOnEndOfTranslationUnit() {}

  //===--------------------------------------------------------------------===//
  // Type Parsing Callbacks.
  //===--------------------------------------------------------------------===//

  /// ActOnTypeName - A type-name (type-id in C++) was parsed.
  virtual TypeResult ActOnTypeName(Scope *S, Declarator &D) {
    return TypeResult();
  }

  enum TagUseKind {
    TUK_Reference,   // Reference to a tag:  'struct foo *X;'
    TUK_Declaration, // Fwd decl of a tag:   'struct foo;'
    TUK_Definition,  // Definition of a tag: 'struct foo { int X; } Y;'
    TUK_Friend       // Friend declaration:  'friend struct foo;'
  };

  /// \brief The parser has encountered a tag (e.g., "class X") that should be
  /// turned into a declaration by the action module.
  ///
  /// \param S the scope in which this tag occurs.
  ///
  /// \param TagSpec an instance of DeclSpec::TST, indicating what kind of tag
  /// this is (struct/union/enum/class).
  ///
  /// \param TUK how the tag we have encountered is being used, which
  /// can be a reference to a (possibly pre-existing) tag, a
  /// declaration of that tag, or the beginning of a definition of
  /// that tag.
  ///
  /// \param KWLoc the location of the "struct", "class", "union", or "enum"
  /// keyword.
  ///
  /// \param SS C++ scope specifier that precedes the name of the tag, e.g.,
  /// the "std::" in "class std::type_info".
  ///
  /// \param Name the name of the tag, e.g., "X" in "struct X". This parameter
  /// may be NULL, to indicate an anonymous class/struct/union/enum type.
  ///
  /// \param NameLoc the location of the name of the tag.
  ///
  /// \param Attr the set of attributes that appertain to the tag.
  ///
  /// \param AS when this tag occurs within a C++ class, provides the
  /// current access specifier (AS_public, AS_private, AS_protected).
  /// Otherwise, it will be AS_none.
  ///
  /// \param TemplateParameterLists the set of C++ template parameter lists
  /// that apply to this tag, if the tag is a declaration or definition (see
  /// the \p TK parameter). The action module is responsible for determining,
  /// based on the template parameter lists and the scope specifier, whether
  /// the declared tag is a class template or not.
  ///
  /// \param OwnedDecl the callee should set this flag true when the returned
  /// declaration is "owned" by this reference. Ownership is handled entirely
  /// by the action module.
  ///
  /// \returns the declaration to which this tag refers.
  virtual DeclPtrTy ActOnTag(Scope *S, unsigned TagSpec, TagUseKind TUK,
                             SourceLocation KWLoc, CXXScopeSpec &SS,
                             IdentifierInfo *Name, SourceLocation NameLoc,
                             AttributeList *Attr, //AccessSpecifier AS,
                             MultiTemplateParamsArg TemplateParameterLists,
                             bool &OwnedDecl, bool &IsDependent) {
    return DeclPtrTy();
  }

  /// Acts on a reference to a dependent tag name.  This arises in
  /// cases like:
  ///
  ///    template <class T> class A;
  ///    template <class T> class B {
  ///      friend class A<T>::M;  // here
  ///    };
  ///
  /// \param TagSpec an instance of DeclSpec::TST corresponding to the
  /// tag specifier.
  ///
  /// \param TUK the tag use kind (either TUK_Friend or TUK_Reference)
  ///
  /// \param SS the scope specifier (always defined)
  virtual TypeResult ActOnDependentTag(Scope *S,
                                       unsigned TagSpec,
                                       TagUseKind TUK,
                                       const CXXScopeSpec &SS,
                                       IdentifierInfo *Name,
                                       SourceLocation KWLoc,
                                       SourceLocation NameLoc) {
    return TypeResult();
  }

  virtual void ActOnFields(Scope* S, SourceLocation RecLoc, DeclPtrTy TagDecl,
                           DeclPtrTy *Fields, unsigned NumFields,
                           SourceLocation LBrac, SourceLocation RBrac,
                           AttributeList *AttrList) {}

  /// ActOnTagStartDefinition - Invoked when we have entered the
  /// scope of a tag's definition (e.g., for an enumeration, class,
  /// struct, or union).
  virtual void ActOnTagStartDefinition(Scope *S, DeclPtrTy TagDecl) { }

  /// ActOnStartCXXMemberDeclarations - Invoked when we have parsed a
  /// C++ record definition's base-specifiers clause and are starting its
  /// member declarations.
  virtual void ActOnStartCXXMemberDeclarations(Scope *S, DeclPtrTy TagDecl,
                                               SourceLocation LBraceLoc) { }

  /// ActOnTagFinishDefinition - Invoked once we have finished parsing
  /// the definition of a tag (enumeration, class, struct, or union).
  ///
  /// The scope is the scope of the tag definition.
  virtual void ActOnTagFinishDefinition(Scope *S, DeclPtrTy TagDecl,
                                        SourceLocation RBraceLoc) { }

  /// ActOnTagDefinitionError - Invoked if there's an unrecoverable
  /// error parsing the definition of a tag.
  ///
  /// The scope is the scope of the tag definition.
  virtual void ActOnTagDefinitionError(Scope *S, DeclPtrTy TagDecl) { }

  virtual DeclPtrTy ActOnEnumConstant(Scope *S, DeclPtrTy EnumDecl,
                                      DeclPtrTy LastEnumConstant,
                                      SourceLocation IdLoc, IdentifierInfo *Id,
                                      SourceLocation EqualLoc, ExprTy *Val) {
    return DeclPtrTy();
  }
  virtual void ActOnEnumBody(SourceLocation EnumLoc, SourceLocation LBraceLoc,
                             SourceLocation RBraceLoc, DeclPtrTy EnumDecl,
                             DeclPtrTy *Elements, unsigned NumElements,
                             Scope *S, AttributeList *AttrList) {}

  //===--------------------------------------------------------------------===//
  // Statement Parsing Callbacks.
  //===--------------------------------------------------------------------===//

  virtual OwningStmtResult ActOnNullStmt(SourceLocation SemiLoc) {
    return StmtEmpty();
  }

  virtual OwningStmtResult ActOnCompoundStmt(SourceLocation L, SourceLocation R,
                                             MultiStmtArg Elts,
                                             bool isStmtExpr) {
    return StmtEmpty();
  }
  virtual OwningStmtResult ActOnDeclStmt(DeclGroupPtrTy Decl,
                                         SourceLocation StartLoc,
                                         SourceLocation EndLoc) {
    return StmtEmpty();
  }

  virtual void ActOnForEachDeclStmt(DeclGroupPtrTy Decl) {
  }

  virtual OwningStmtResult ActOnExprStmt(FullExprArg Expr) {
    return OwningStmtResult(*this, Expr->release());
  }

  /// ActOnCaseStmt - Note that this handles the GNU 'case 1 ... 4' extension,
  /// which can specify an RHS value.  The sub-statement of the case is
  /// specified in a separate action.
  virtual OwningStmtResult ActOnCaseStmt(SourceLocation CaseLoc, ExprArg LHSVal,
                                         SourceLocation DotDotDotLoc,
                                         ExprArg RHSVal,
                                         SourceLocation ColonLoc) {
    return StmtEmpty();
  }

  /// ActOnCaseStmtBody - This installs a statement as the body of a case.
  virtual void ActOnCaseStmtBody(StmtTy *CaseStmt, StmtArg SubStmt) {}

  virtual OwningStmtResult ActOnDefaultStmt(SourceLocation DefaultLoc,
                                            SourceLocation ColonLoc,
                                            StmtArg SubStmt, Scope *CurScope){
    return StmtEmpty();
  }

  /// \brief Parsed the start of a "switch" statement.
  ///
  /// \param SwitchLoc The location of the "switch" keyword.
  ///
  /// \param Cond if the "switch" condition was parsed as an expression, 
  /// the expression itself.
  ///
  /// \param CondVar if the "switch" condition was parsed as a condition 
  /// variable, the condition variable itself.
  virtual OwningStmtResult ActOnStartOfSwitchStmt(SourceLocation SwitchLoc,
                                                  ExprArg Cond,
                                                  DeclPtrTy CondVar) {
    return StmtEmpty();
  }

  virtual OwningStmtResult ActOnFinishSwitchStmt(SourceLocation SwitchLoc,
                                                 StmtArg Switch, StmtArg Body) {
    return StmtEmpty();
  }

  /// \brief Parsed a "while" statement.
  ///
  /// \param Cond if the "while" condition was parsed as an expression, 
  /// the expression itself.
  ///
  /// \param CondVar if the "while" condition was parsed as a condition 
  /// variable, the condition variable itself.
  ///
  /// \param Body the body of the "while" loop.
  virtual OwningStmtResult ActOnWhileStmt(SourceLocation WhileLoc,
                                          FullExprArg Cond, DeclPtrTy CondVar,
                                          StmtArg Body) {
    return StmtEmpty();
  }
  virtual OwningStmtResult ActOnDoStmt(SourceLocation DoLoc, StmtArg Body,
                                       SourceLocation WhileLoc,
                                       SourceLocation CondLParen,
                                       ExprArg Cond,
                                       SourceLocation CondRParen) {
    return StmtEmpty();
  }

  /// \brief Parsed a "for" statement.
  ///
  /// \param ForLoc the location of the "for" keyword.
  ///
  /// \param LParenLoc the location of the left parentheses.
  ///
  /// \param First the statement used to initialize the for loop.
  ///
  /// \param Second the condition to be checked during each iteration, if
  /// that condition was parsed as an expression.
  ///
  /// \param SecondArg the condition variable to be checked during each 
  /// iterator, if that condition was parsed as a variable declaration.
  ///
  /// \param Third the expression that will be evaluated to "increment" any
  /// values prior to the next iteration.
  ///
  /// \param RParenLoc the location of the right parentheses.
  ///
  /// \param Body the body of the "body" loop.
  virtual OwningStmtResult ActOnForStmt(SourceLocation ForLoc,
                                        SourceLocation LParenLoc,
                                        StmtArg First, FullExprArg Second,
                                        DeclPtrTy SecondVar, FullExprArg Third, 
                                        SourceLocation RParenLoc,
                                        StmtArg Body) {
    return StmtEmpty();
  }
  
  virtual OwningStmtResult ActOnObjCForCollectionStmt(SourceLocation ForColLoc,
                                       SourceLocation LParenLoc,
                                       StmtArg First, ExprArg Second,
                                       SourceLocation RParenLoc, StmtArg Body) {
    return StmtEmpty();
  }
  virtual OwningStmtResult ActOnIndirectGotoStmt(SourceLocation GotoLoc,
                                                 SourceLocation StarLoc,
                                                 ExprArg DestExp) {
    return StmtEmpty();
  }
  virtual OwningStmtResult ActOnAsmStmt(SourceLocation AsmLoc,
                                        bool IsSimple,
                                        bool IsVolatile,
                                        unsigned NumOutputs,
                                        unsigned NumInputs,
                                        IdentifierInfo **Names,
                                        MultiExprArg Constraints,
                                        MultiExprArg Exprs,
                                        ExprArg AsmString,
                                        MultiExprArg Clobbers,
                                        SourceLocation RParenLoc,
                                        bool MSAsm = false) {
    return StmtEmpty();
  }

  // Objective-c statements
  
  /// \brief Parsed an Objective-C @catch statement.
  ///
  /// \param AtLoc The location of the '@' starting the '@catch'.
  ///
  /// \param RParen The location of the right parentheses ')' after the
  /// exception variable.
  ///
  /// \param Parm The variable that will catch the exception. Will be NULL if 
  /// this is a @catch(...) block.
  ///
  /// \param Body The body of the @catch block.
  virtual OwningStmtResult ActOnObjCAtCatchStmt(SourceLocation AtLoc,
                                                SourceLocation RParen,
                                                DeclPtrTy Parm, StmtArg Body) {
    return StmtEmpty();
  }

  /// \brief Parsed an Objective-C @finally statement.
  ///
  /// \param AtLoc The location of the '@' starting the '@finally'.
  ///
  /// \param Body The body of the @finally block.
  virtual OwningStmtResult ActOnObjCAtFinallyStmt(SourceLocation AtLoc,
                                                  StmtArg Body) {
    return StmtEmpty();
  }

  /// \brief Parsed an Objective-C @try-@catch-@finally statement.
  ///
  /// \param AtLoc The location of the '@' starting '@try'.
  ///
  /// \param Try The body of the '@try' statement.
  ///
  /// \param CatchStmts The @catch statements.
  ///
  /// \param Finally The @finally statement.
  virtual OwningStmtResult ActOnObjCAtTryStmt(SourceLocation AtLoc,
                                              StmtArg Try, 
                                              MultiStmtArg CatchStmts,
                                              StmtArg Finally) {
    return StmtEmpty();
  }

  virtual OwningStmtResult ActOnObjCAtThrowStmt(SourceLocation AtLoc,
                                                ExprArg Throw,
                                                Scope *CurScope) {
    return StmtEmpty();
  }

  virtual OwningStmtResult ActOnObjCAtSynchronizedStmt(SourceLocation AtLoc,
                                                       ExprArg SynchExpr,
                                                       StmtArg SynchBody) {
    return StmtEmpty();
  }

  // C++ Statements
  virtual DeclPtrTy ActOnExceptionDeclarator(Scope *S, Declarator &D) {
    return DeclPtrTy();
  }

  virtual OwningStmtResult ActOnCXXCatchBlock(SourceLocation CatchLoc,
                                              DeclPtrTy ExceptionDecl,
                                              StmtArg HandlerBlock) {
    return StmtEmpty();
  }

  virtual OwningStmtResult ActOnCXXTryBlock(SourceLocation TryLoc,
                                            StmtArg TryBlock,
                                            MultiStmtArg Handlers) {
    return StmtEmpty();
  }

  //===--------------------------------------------------------------------===//
  // Expression Parsing Callbacks.
  //===--------------------------------------------------------------------===//

  /// \brief Describes how the expressions currently being parsed are
  /// evaluated at run-time, if at all.
  enum ExpressionEvaluationContext {
    /// \brief The current expression and its subexpressions occur within an
    /// unevaluated operand (C++0x [expr]p8), such as a constant expression
    /// or the subexpression of \c sizeof, where the type or the value of the
    /// expression may be significant but no code will be generated to evaluate
    /// the value of the expression at run time.
    Unevaluated,

    /// \brief The current expression is potentially evaluated at run time,
    /// which means that code may be generated to evaluate the value of the
    /// expression at run time.
    PotentiallyEvaluated,

    /// \brief The current expression may be potentially evaluated or it may
    /// be unevaluated, but it is impossible to tell from the lexical context.
    /// This evaluation context is used primary for the operand of the C++
    /// \c typeid expression, whose argument is potentially evaluated only when
    /// it is an lvalue of polymorphic class type (C++ [basic.def.odr]p2).
    PotentiallyPotentiallyEvaluated
  };

  /// \brief The parser is entering a new expression evaluation context.
  ///
  /// \param NewContext is the new expression evaluation context.
  virtual void
  PushExpressionEvaluationContext(ExpressionEvaluationContext NewContext) { }

  /// \brief The parser is exiting an expression evaluation context.
  virtual void
  PopExpressionEvaluationContext() { }

  // Primary Expressions.

  /// \brief Retrieve the source range that corresponds to the given
  /// expression.
  virtual SourceRange getExprRange(ExprTy *E) const {
    return SourceRange();
  }
  
  /// \brief Parsed an id-expression (C++) or identifier (C) in expression
  /// context, e.g., the expression "x" that refers to a variable named "x".
  ///
  /// \param S the scope in which this id-expression or identifier occurs.
  ///
  /// \param SS the C++ nested-name-specifier that qualifies the name of the
  /// value, e.g., "std::" in "std::sort".
  ///
  /// \param Name the name to which the id-expression refers. In C, this will
  /// always be an identifier. In C++, it may also be an overloaded operator,
  /// destructor name (if there is a nested-name-specifier), or template-id.
  ///
  /// \param HasTrailingLParen whether the next token following the 
  /// id-expression or identifier is a left parentheses ('(').
  ///
  /// \param IsAddressOfOperand whether the token that precedes this 
  /// id-expression or identifier was an ampersand ('&'), indicating that 
  /// we will be taking the address of this expression.
  virtual OwningExprResult ActOnIdExpression(Scope *S,
                                             CXXScopeSpec &SS,
                                             UnqualifiedId &Name,
                                             bool HasTrailingLParen,
                                             bool IsAddressOfOperand) {
    return ExprEmpty();
  }
  
  virtual OwningExprResult ActOnPredefinedExpr(SourceLocation Loc,
                                               tok::TokenKind Kind) {
    return ExprEmpty();
  }
  virtual OwningExprResult ActOnCharacterConstant(const Token &) {
    return ExprEmpty();
  }
  virtual OwningExprResult ActOnNumericConstant(const Token &) {
    return ExprEmpty();
  }

  /// ActOnStringLiteral - The specified tokens were lexed as pasted string
  /// fragments (e.g. "foo" "bar" L"baz").
  virtual OwningExprResult ActOnStringLiteral(const Token *Toks,
                                              unsigned NumToks) {
    return ExprEmpty();
  }

  virtual OwningExprResult ActOnParenExpr(SourceLocation L, SourceLocation R,
                                          ExprArg Val) {
    return move(Val);  // Default impl returns operand.
  }

  virtual OwningExprResult ActOnParenOrParenListExpr(SourceLocation L,
                                              SourceLocation R,
                                              MultiExprArg Val,
                                              TypeTy *TypeOfCast=0) {
    return ExprEmpty();
  }

  // Postfix Expressions.
  virtual OwningExprResult ActOnPostfixUnaryOp(Scope *S, SourceLocation OpLoc,
                                               tok::TokenKind Kind,
                                               ExprArg Input) {
    return ExprEmpty();
  }
  virtual OwningExprResult ActOnArraySubscriptExpr(Scope *S, ExprArg Base,
                                                   SourceLocation LLoc,
                                                   ExprArg Idx,
                                                   SourceLocation RLoc) {
    return ExprEmpty();
  }

  /// \brief Parsed a member access expresion (C99 6.5.2.3, C++ [expr.ref])
  /// of the form \c x.m or \c p->m.
  ///
  /// \param S the scope in which the member access expression occurs.
  ///
  /// \param Base the class or pointer to class into which this member
  /// access expression refers, e.g., \c x in \c x.m.
  ///
  /// \param OpLoc the location of the "." or "->" operator.
  ///
  /// \param OpKind the kind of member access operator, which will be either
  /// tok::arrow ("->") or tok::period (".").
  ///
  /// \param SS in C++, the nested-name-specifier that precedes the member
  /// name, if any.
  ///
  /// \param Member the name of the member that we are referring to. In C,
  /// this will always store an identifier; in C++, we may also have operator
  /// names, conversion function names, destructors, and template names.
  ///
  /// \param ObjCImpDecl the Objective-C implementation declaration.
  /// FIXME: Do we really need this?
  ///
  /// \param HasTrailingLParen whether this member name is immediately followed
  /// by a left parentheses ('(').
  virtual OwningExprResult ActOnMemberAccessExpr(Scope *S, ExprArg Base,
                                                 SourceLocation OpLoc,
                                                 tok::TokenKind OpKind,
                                                 CXXScopeSpec &SS,
                                                 UnqualifiedId &Member,
                                                 DeclPtrTy ObjCImpDecl,
                                                 bool HasTrailingLParen) {
    return ExprEmpty();
  }
                                                 
  /// ActOnCallExpr - Handle a call to Fn with the specified array of arguments.
  /// This provides the location of the left/right parens and a list of comma
  /// locations.  There are guaranteed to be one fewer commas than arguments,
  /// unless there are zero arguments.
  virtual OwningExprResult ActOnCallExpr(Scope *S, ExprArg Fn,
                                         SourceLocation LParenLoc,
                                         MultiExprArg Args,
                                         SourceLocation *CommaLocs,
                                         SourceLocation RParenLoc) {
    return ExprEmpty();
  }

  // Unary Operators.  'Tok' is the token for the operator.
  virtual OwningExprResult ActOnUnaryOp(Scope *S, SourceLocation OpLoc,
                                        tok::TokenKind Op, ExprArg Input) {
    return ExprEmpty();
  }
  virtual OwningExprResult
    ActOnSizeOfAlignOfExpr(SourceLocation OpLoc, bool isSizeof, bool isType,
                           void *TyOrEx, const SourceRange &ArgRange) {
    return ExprEmpty();
  }

  virtual OwningExprResult ActOnCompoundLiteral(SourceLocation LParen,
                                                TypeTy *Ty,
                                                SourceLocation RParen,
                                                ExprArg Op) {
    return ExprEmpty();
  }
  virtual OwningExprResult ActOnInitList(SourceLocation LParenLoc,
                                         MultiExprArg InitList,
                                         SourceLocation RParenLoc) {
    return ExprEmpty();
  }
  /// @brief Parsed a C99 designated initializer.
  ///
  /// @param Desig Contains the designation with one or more designators.
  ///
  /// @param Loc The location of the '=' or ':' prior to the
  /// initialization expression.
  ///
  /// @param GNUSyntax If true, then this designated initializer used
  /// the deprecated GNU syntax @c fieldname:foo or @c [expr]foo rather
  /// than the C99 syntax @c .fieldname=foo or @c [expr]=foo.
  ///
  /// @param Init The value that the entity (or entities) described by
  /// the designation will be initialized with.
  virtual OwningExprResult ActOnDesignatedInitializer(Designation &Desig,
                                                      SourceLocation Loc,
                                                      bool GNUSyntax,
                                                      OwningExprResult Init) {
    return ExprEmpty();
  }

  virtual OwningExprResult ActOnCastExpr(Scope *S, SourceLocation LParenLoc,
                                         TypeTy *Ty, SourceLocation RParenLoc,
                                         ExprArg Op) {
    return ExprEmpty();
  }

  virtual bool TypeIsVectorType(TypeTy *Ty) {
    return false;
  }

  virtual OwningExprResult ActOnBinOp(Scope *S, SourceLocation TokLoc,
                                      tok::TokenKind Kind,
                                      ExprArg LHS, ExprArg RHS) {
    return ExprEmpty();
  }

  /// ActOnConditionalOp - Parse a ?: operation.  Note that 'LHS' may be null
  /// in the case of a the GNU conditional expr extension.
  virtual OwningExprResult ActOnConditionalOp(SourceLocation QuestionLoc,
                                              SourceLocation ColonLoc,
                                              ExprArg Cond, ExprArg LHS,
                                              ExprArg RHS) {
    return ExprEmpty();
  }

  //===---------------------- GNU Extension Expressions -------------------===//

  virtual OwningExprResult ActOnAddrLabel(SourceLocation OpLoc,
                                          SourceLocation LabLoc,
                                          IdentifierInfo *LabelII) { // "&&foo"
    return ExprEmpty();
  }

  virtual OwningExprResult ActOnStmtExpr(SourceLocation LPLoc, StmtArg SubStmt,
                                         SourceLocation RPLoc) { // "({..})"
    return ExprEmpty();
  }

  // __builtin_offsetof(type, identifier(.identifier|[expr])*)
  struct OffsetOfComponent {
    SourceLocation LocStart, LocEnd;
    bool isBrackets;  // true if [expr], false if .ident
    union {
      IdentifierInfo *IdentInfo;
      ExprTy *E;
    } U;
  };

  virtual OwningExprResult ActOnBuiltinOffsetOf(Scope *S,
                                                SourceLocation BuiltinLoc,
                                                SourceLocation TypeLoc,
                                                TypeTy *Arg1,
                                                OffsetOfComponent *CompPtr,
                                                unsigned NumComponents,
                                                SourceLocation RParenLoc) {
    return ExprEmpty();
  }

  // __builtin_types_compatible_p(type1, type2)
  virtual OwningExprResult ActOnTypesCompatibleExpr(SourceLocation BuiltinLoc,
                                                    TypeTy *arg1, TypeTy *arg2,
                                                    SourceLocation RPLoc) {
    return ExprEmpty();
  }
  // __builtin_choose_expr(constExpr, expr1, expr2)
  virtual OwningExprResult ActOnChooseExpr(SourceLocation BuiltinLoc,
                                           ExprArg cond, ExprArg expr1,
                                           ExprArg expr2, SourceLocation RPLoc){
    return ExprEmpty();
  }

  // __builtin_va_arg(expr, type)
  virtual OwningExprResult ActOnVAArg(SourceLocation BuiltinLoc,
                                      ExprArg expr, TypeTy *type,
                                      SourceLocation RPLoc) {
    return ExprEmpty();
  }

  /// ActOnGNUNullExpr - Parsed the GNU __null expression, the token
  /// for which is at position TokenLoc.
  virtual OwningExprResult ActOnGNUNullExpr(SourceLocation TokenLoc) {
    return ExprEmpty();
  }

  //===------------------------- "Block" Extension ------------------------===//

  /// ActOnBlockStart - This callback is invoked when a block literal is
  /// started.  The result pointer is passed into the block finalizers.
  //virtual void ActOnBlockStart(SourceLocation CaretLoc, Scope *CurScope) {}

  /// ActOnBlockArguments - This callback allows processing of block arguments.
  /// If there are no arguments, this is still invoked.
  //virtual void ActOnBlockArguments(Declarator &ParamInfo, Scope *CurScope) {}

  /// ActOnBlockError - If there is an error parsing a block, this callback
  /// is invoked to pop the information about the block from the action impl.
  //virtual void ActOnBlockError(SourceLocation CaretLoc, Scope *CurScope) {}

  /// ActOnBlockStmtExpr - This is called when the body of a block statement
  /// literal was successfully completed.  ^(int x){...}
  //virtual OwningExprResult ActOnBlockStmtExpr(SourceLocation CaretLoc,
                                              //StmtArg Body,
                                              //Scope *CurScope) {
    //return ExprEmpty();
  //}

  //===------------------------- C++ Declarations -------------------------===//

  /// ActOnStartNamespaceDef - This is called at the start of a namespace
  /// definition.
  virtual DeclPtrTy ActOnStartNamespaceDef(Scope *S, SourceLocation IdentLoc,
                                           IdentifierInfo *Ident,
                                           SourceLocation LBrace,
                                           AttributeList *AttrList) {
    return DeclPtrTy();
  }

  /// ActOnFinishNamespaceDef - This callback is called after a namespace is
  /// exited. Decl is returned by ActOnStartNamespaceDef.
  virtual void ActOnFinishNamespaceDef(DeclPtrTy Dcl, SourceLocation RBrace) {
    return;
  }

  ///// ActOnUsingDirective - This is called when using-directive is parsed.
  //virtual DeclPtrTy ActOnUsingDirective(Scope *CurScope,
  //                                      SourceLocation UsingLoc,
  //                                      SourceLocation NamespcLoc,
  //                                      CXXScopeSpec &SS,
  //                                      SourceLocation IdentLoc,
  //                                      IdentifierInfo *NamespcName,
  //                                      AttributeList *AttrList);

  /// ActOnNamespaceAliasDef - This is called when a namespace alias definition
  /// is parsed.
  virtual DeclPtrTy ActOnNamespaceAliasDef(Scope *CurScope,
                                           SourceLocation NamespaceLoc,
                                           SourceLocation AliasLoc,
                                           IdentifierInfo *Alias,
                                           CXXScopeSpec &SS,
                                           SourceLocation IdentLoc,
                                           IdentifierInfo *Ident) {
    return DeclPtrTy();
  }

  /// \brief Parsed a C++ using-declaration.
  ///
  /// This callback will be invoked when the parser has parsed a C++
  /// using-declaration, e.g.,
  ///
  /// \code
  /// namespace std {
  ///   template<typename T, typename Alloc> class vector;
  /// }
  ///
  /// using std::vector; // using-declaration here
  /// \endcode
  ///
  /// \param CurScope the scope in which this using declaration was parsed.
  ///
  /// \param AS the currently-active access specifier.
  ///
  /// \param HasUsingKeyword true if this was declared with an
  ///   explicit 'using' keyword (i.e. if this is technically a using
  ///   declaration, not an access declaration)
  ///
  /// \param UsingLoc the location of the 'using' keyword.
  ///
  /// \param SS the nested-name-specifier that precedes the name.
  ///
  /// \param Name the name to which the using declaration refers.
  ///
  /// \param AttrList attributes applied to this using declaration, if any.
  ///
  /// \param IsTypeName whether this using declaration started with the 
  /// 'typename' keyword. FIXME: This will eventually be split into a 
  /// separate action.
  ///
  /// \param TypenameLoc the location of the 'typename' keyword, if present
  ///
  /// \returns a representation of the using declaration.
  //virtual DeclPtrTy ActOnUsingDeclaration(Scope *CurScope,
  //                                        //AccessSpecifier AS,
  //                                        bool HasUsingKeyword,
  //                                        SourceLocation UsingLoc,
  //                                        CXXScopeSpec &SS,
  //                                        UnqualifiedId &Name,
  //                                        AttributeList *AttrList,
  //                                        bool IsTypeName,
  //                                        SourceLocation TypenameLoc);

  /// ActOnParamDefaultArgument - Parse default argument for function parameter
  virtual void ActOnParamDefaultArgument(DeclPtrTy param,
                                         SourceLocation EqualLoc,
                                         ExprArg defarg) {
  }

  /// ActOnParamUnparsedDefaultArgument - We've seen a default
  /// argument for a function parameter, but we can't parse it yet
  /// because we're inside a class definition. Note that this default
  /// argument will be parsed later.
  virtual void ActOnParamUnparsedDefaultArgument(DeclPtrTy param,
                                                 SourceLocation EqualLoc,
                                                 SourceLocation ArgLoc) { }

  /// ActOnParamDefaultArgumentError - Parsing or semantic analysis of
  /// the default argument for the parameter param failed.
  virtual void ActOnParamDefaultArgumentError(DeclPtrTy param) { }

  /// AddCXXDirectInitializerToDecl - This action is called immediately after
  /// ActOnDeclarator, when a C++ direct initializer is present.
  /// e.g: "int x(1);"
  virtual void AddCXXDirectInitializerToDecl(DeclPtrTy Dcl,
                                             SourceLocation LParenLoc,
                                             MultiExprArg Exprs,
                                             SourceLocation *CommaLocs,
                                             SourceLocation RParenLoc) {
    return;
  }

  /// \brief Called when we re-enter a template parameter scope.
  ///
  /// This action occurs when we are going to parse an member
  /// function's default arguments or inline definition after the
  /// outermost class definition has been completed, and when one or
  /// more of the class definitions enclosing the member function is a
  /// template. The "entity" in the given scope will be set as it was
  /// when we entered the scope of the template initially, and should
  /// be used to, e.g., reintroduce the names of template parameters
  /// into the current scope so that they can be found by name lookup.
  ///
  /// \param S The (new) template parameter scope.
  ///
  /// \param Template the class template declaration whose template
  /// parameters should be reintroduced into the current scope.
  virtual void ActOnReenterTemplateScope(Scope *S, DeclPtrTy Template) {
  }

  /// ActOnStartDelayedMemberDeclarations - We have completed parsing
  /// a C++ class, and we are about to start parsing any parts of
  /// member declarations that could not be parsed earlier.  Enter
  /// the appropriate record scope.
  virtual void ActOnStartDelayedMemberDeclarations(Scope *S,
                                                   DeclPtrTy Record) {
  }

  /// ActOnStartDelayedCXXMethodDeclaration - We have completed
  /// parsing a top-level (non-nested) C++ class, and we are now
  /// parsing those parts of the given Method declaration that could
  /// not be parsed earlier (C++ [class.mem]p2), such as default
  /// arguments. This action should enter the scope of the given
  /// Method declaration as if we had just parsed the qualified method
  /// name. However, it should not bring the parameters into scope;
  /// that will be performed by ActOnDelayedCXXMethodParameter.
  virtual void ActOnStartDelayedCXXMethodDeclaration(Scope *S,
                                                     DeclPtrTy Method) {
  }

  /// ActOnDelayedCXXMethodParameter - We've already started a delayed
  /// C++ method declaration. We're (re-)introducing the given
  /// function parameter into scope for use in parsing later parts of
  /// the method declaration. For example, we could see an
  /// ActOnParamDefaultArgument event for this parameter.
  virtual void ActOnDelayedCXXMethodParameter(Scope *S, DeclPtrTy Param) {
  }

  /// ActOnFinishDelayedCXXMethodDeclaration - We have finished
  /// processing the delayed method declaration for Method. The method
  /// declaration is now considered finished. There may be a separate
  /// ActOnStartOfFunctionDef action later (not necessarily
  /// immediately!) for this method, if it was also defined inside the
  /// class body.
  virtual void ActOnFinishDelayedCXXMethodDeclaration(Scope *S,
                                                      DeclPtrTy Method) {
  }

  /// ActOnFinishDelayedMemberDeclarations - We have finished parsing
  /// a C++ class, and we are about to start parsing any parts of
  /// member declarations that could not be parsed earlier.  Enter the
  /// appropriate record scope.
  virtual void ActOnFinishDelayedMemberDeclarations(Scope *S,
                                                    DeclPtrTy Record) {
  }

  /// ActOnStaticAssertDeclaration - Parse a C++0x static_assert declaration.
  virtual DeclPtrTy ActOnStaticAssertDeclaration(SourceLocation AssertLoc,
                                                 ExprArg AssertExpr,
                                                 ExprArg AssertMessageExpr) {
    return DeclPtrTy();
  }

  /// ActOnFriendFunctionDecl - Parsed a friend function declarator.
  /// The name is actually a slight misnomer, because the declarator
  /// is not necessarily a function declarator.
  virtual DeclPtrTy ActOnFriendFunctionDecl(Scope *S,
                                            Declarator &D,
                                            bool IsDefinition,
                                            MultiTemplateParamsArg TParams) {
    return DeclPtrTy();
  }

  /// ActOnFriendTypeDecl - Parsed a friend type declaration.
  virtual DeclPtrTy ActOnFriendTypeDecl(Scope *S, const DeclSpec &DS,
                                        MultiTemplateParamsArg TParams) {
    return DeclPtrTy();
  }

  //===------------------------- C++ Expressions --------------------------===//

  /// \brief Parsed a destructor name or pseudo-destructor name. 
  ///
  /// \returns the type being destructed.
  //virtual TypeTy *getDestructorName(SourceLocation TildeLoc,
  //                                  IdentifierInfo &II, SourceLocation NameLoc,
  //                                  Scope *S, CXXScopeSpec &SS,
  //                                  TypeTy *ObjectType,
  //                                  bool EnteringContext) {
  //  return getTypeName(II, NameLoc, S, &SS, false, ObjectType);
  //}


  /// ActOnCXXNamedCast - Parse {dynamic,static,reinterpret,const}_cast's.
  virtual OwningExprResult ActOnCXXNamedCast(SourceLocation OpLoc,
                                             tok::TokenKind Kind,
                                             SourceLocation LAngleBracketLoc,
                                             TypeTy *Ty,
                                             SourceLocation RAngleBracketLoc,
                                             SourceLocation LParenLoc,
                                             ExprArg Op,
                                             SourceLocation RParenLoc) {
    return ExprEmpty();
  }

  /// ActOnCXXTypeidOfType - Parse typeid( type-id ).
  virtual OwningExprResult ActOnCXXTypeid(SourceLocation OpLoc,
                                          SourceLocation LParenLoc, bool isType,
                                          void *TyOrExpr,
                                          SourceLocation RParenLoc) {
    return ExprEmpty();
  }

  /// ActOnCXXThis - Parse the C++ 'this' pointer.
  virtual OwningExprResult ActOnCXXThis(SourceLocation ThisLoc) {
    return ExprEmpty();
  }

  /// ActOnCXXBoolLiteral - Parse {true,false} literals.
  virtual OwningExprResult ActOnCXXBoolLiteral(SourceLocation OpLoc,
                                               tok::TokenKind Kind) {
    return ExprEmpty();
  }

  /// ActOnCXXNullPtrLiteral - Parse 'nullptr'.
  virtual OwningExprResult ActOnCXXNullPtrLiteral(SourceLocation Loc) {
    return ExprEmpty();
  }

  /// ActOnCXXThrow - Parse throw expressions.
  virtual OwningExprResult ActOnCXXThrow(SourceLocation OpLoc, ExprArg Op) {
    return ExprEmpty();
  }

  /// ActOnCXXTypeConstructExpr - Parse construction of a specified type.
  /// Can be interpreted either as function-style casting ("int(x)")
  /// or class type construction ("ClassType(x,y,z)")
  /// or creation of a value-initialized type ("int()").
  virtual OwningExprResult ActOnCXXTypeConstructExpr(SourceRange TypeRange,
                                                     TypeTy *TypeRep,
                                                     SourceLocation LParenLoc,
                                                     MultiExprArg Exprs,
                                                     SourceLocation *CommaLocs,
                                                     SourceLocation RParenLoc) {
    return ExprEmpty();
  }

  /// \brief Parsed a condition declaration in a C++ if, switch, or while
  /// statement.
  /// 
  /// This callback will be invoked after parsing the declaration of "x" in
  ///
  /// \code
  /// if (int x = f()) {
  ///   // ...
  /// }
  /// \endcode
  ///
  /// \param S the scope of the if, switch, or while statement.
  ///
  /// \param D the declarator that that describes the variable being declared.
  virtual DeclResult ActOnCXXConditionDeclaration(Scope *S, Declarator &D) {
    return DeclResult();
  }

  /// \brief Parsed an expression that will be handled as the condition in
  /// an if/while/for statement. 
  ///
  /// This routine handles the conversion of the expression to 'bool'.
  ///
  /// \param S The scope in which the expression occurs.
  ///
  /// \param Loc The location of the construct that requires the conversion to
  /// a boolean value.
  ///
  /// \param SubExpr The expression that is being converted to bool.
  virtual OwningExprResult ActOnBooleanCondition(Scope *S, SourceLocation Loc,
                                                 ExprArg SubExpr) {
    return move(SubExpr);
  }
  
  /// \brief Parsed a C++ 'new' expression.
  ///
  /// \param StartLoc The start of the new expression, which is either the
  /// "new" keyword or the "::" preceding it, depending on \p UseGlobal.
  ///
  /// \param UseGlobal True if the "new" was qualified with "::".
  ///
  /// \param PlacementLParen The location of the opening parenthesis ('(') for
  /// the placement arguments, if any.
  /// 
  /// \param PlacementArgs The placement arguments, if any.
  ///
  /// \param PlacementRParen The location of the closing parenthesis (')') for
  /// the placement arguments, if any.
  ///
  /// \param TypeIdParens If the type was expressed as a type-id in parentheses,
  /// the source range covering the parenthesized type-id.
  ///
  /// \param D The parsed declarator, which may include an array size (for 
  /// array new) as the first declarator.
  ///
  /// \param ConstructorLParen The location of the opening parenthesis ('(') for
  /// the constructor arguments, if any.
  ///
  /// \param ConstructorArgs The constructor arguments, if any.
  ///
  /// \param ConstructorRParen The location of the closing parenthesis (')') for
  /// the constructor arguments, if any.
  virtual OwningExprResult ActOnCXXNew(SourceLocation StartLoc, bool UseGlobal,
                                       SourceLocation PlacementLParen,
                                       MultiExprArg PlacementArgs,
                                       SourceLocation PlacementRParen,
                                       SourceRange TypeIdParens, Declarator &D,
                                       SourceLocation ConstructorLParen,
                                       MultiExprArg ConstructorArgs,
                                       SourceLocation ConstructorRParen) {
    return ExprEmpty();
  }

  /// ActOnCXXDelete - Parsed a C++ 'delete' expression. UseGlobal is true if
  /// the delete was qualified (::delete). ArrayForm is true if the array form
  /// was used (delete[]).
  virtual OwningExprResult ActOnCXXDelete(SourceLocation StartLoc,
                                          bool UseGlobal, bool ArrayForm,
                                          ExprArg Operand) {
    return ExprEmpty();
  }

  //virtual OwningExprResult ActOnUnaryTypeTrait(UnaryTypeTrait OTT,
  //                                             SourceLocation KWLoc,
  //                                             SourceLocation LParen,
  //                                             TypeTy *Ty,
  //                                             SourceLocation RParen) {
  //  return ExprEmpty();
  //}

  /// \brief Invoked when the parser is starting to parse a C++ member access
  /// expression such as x.f or x->f.
  ///
  /// \param S the scope in which the member access expression occurs.
  ///
  /// \param Base the expression in which a member is being accessed, e.g., the
  /// "x" in "x.f".
  ///
  /// \param OpLoc the location of the member access operator ("." or "->")
  ///
  /// \param OpKind the kind of member access operator ("." or "->")
  ///
  /// \param ObjectType originally NULL. The action should fill in this type
  /// with the type into which name lookup should look to find the member in
  /// the member access expression.
  ///
  /// \param MayBePseudoDestructor Originally false. The action should
  /// set this true if the expression may end up being a
  /// pseudo-destructor expression, indicating to the parser that it
  /// shoudl be parsed as a pseudo-destructor rather than as a member
  /// access expression. Note that this should apply both when the
  /// object type is a scalar and when the object type is dependent.
  ///
  /// \returns the (possibly modified) \p Base expression
  virtual OwningExprResult ActOnStartCXXMemberReference(Scope *S,
                                                        ExprArg Base,
                                                        SourceLocation OpLoc,
                                                        tok::TokenKind OpKind,
                                                        TypeTy *&ObjectType,
                                                  bool &MayBePseudoDestructor) {
    return ExprEmpty();
  }

  /// \brief Parsed a C++ pseudo-destructor expression or a dependent
  /// member access expression that has the same syntactic form as a
  /// pseudo-destructor expression.
  ///
  /// \param S The scope in which the member access expression occurs.
  ///
  /// \param Base The expression in which a member is being accessed, e.g., the
  /// "x" in "x.f".
  ///
  /// \param OpLoc The location of the member access operator ("." or "->")
  ///
  /// \param OpKind The kind of member access operator ("." or "->")
  ///
  /// \param SS The nested-name-specifier that precedes the type names
  /// in the grammar. Note that this nested-name-specifier will not
  /// cover the last "type-name ::" in the grammar, because it isn't
  /// necessarily a nested-name-specifier.
  ///
  /// \param FirstTypeName The type name that follows the optional
  /// nested-name-specifier but precedes the '::', e.g., the first
  /// type-name in "type-name :: type-name". This type name may be
  /// empty. This will be either an identifier or a template-id.
  ///
  /// \param CCLoc The location of the '::' in "type-name ::
  /// typename". May be invalid, if there is no \p FirstTypeName.
  ///
  /// \param TildeLoc The location of the '~'.
  ///
  /// \param SecondTypeName The type-name following the '~', which is
  /// the name of the type being destroyed. This will be either an
  /// identifier or a template-id.
  ///
  /// \param HasTrailingLParen Whether the next token in the stream is
  /// a left parentheses.
  virtual OwningExprResult ActOnPseudoDestructorExpr(Scope *S, ExprArg Base,
                                                     SourceLocation OpLoc,
                                                     tok::TokenKind OpKind,
                                                     CXXScopeSpec &SS,
                                                  UnqualifiedId &FirstTypeName,
                                                     SourceLocation CCLoc,
                                                     SourceLocation TildeLoc,
                                                 UnqualifiedId &SecondTypeName,
                                                     bool HasTrailingLParen) {
    return ExprEmpty();
  }

  /// ActOnFinishFullExpr - Called whenever a full expression has been parsed.
  /// (C++ [intro.execution]p12).
  virtual OwningExprResult ActOnFinishFullExpr(ExprArg Expr) {
    return move(Expr);
  }

  //===---------------------------- C++ Classes ---------------------------===//
  /// ActOnBaseSpecifier - Parsed a base specifier
  virtual BaseResult ActOnBaseSpecifier(DeclPtrTy classdecl,
                                        SourceRange SpecifierRange,
                                        bool Virtual, //AccessSpecifier Access,
                                        TypeTy *basetype,
                                        SourceLocation BaseLoc) {
    return BaseResult();
  }

  virtual void ActOnBaseSpecifiers(DeclPtrTy ClassDecl, BaseTy **Bases,
                                   unsigned NumBases) {
  }

  /// ActOnAccessSpecifier - This is invoked when an access specifier
  /// (and the colon following it) is found during the parsing of a
  /// C++ class member declarator.
  //virtual DeclPtrTy ActOnAccessSpecifier(AccessSpecifier AS,
  //                                       SourceLocation ASLoc,
  //                                       SourceLocation ColonLoc) {
  //  return DeclPtrTy();
  //}

  /// ActOnCXXMemberDeclarator - This is invoked when a C++ class member
  /// declarator is parsed. 'AS' is the access specifier, 'BitfieldWidth'
  /// specifies the bitfield width if there is one and 'Init' specifies the
  /// initializer if any.  'Deleted' is true if there's a =delete
  /// specifier on the function.
  //virtual DeclPtrTy ActOnCXXMemberDeclarator(Scope *S, AccessSpecifier AS,
  //                                           Declarator &D,
  //                               MultiTemplateParamsArg TemplateParameterLists,
  //                                           ExprTy *BitfieldWidth,
  //                                           ExprTy *Init,
  //                                           bool IsDefinition,
  //                                           bool Deleted = false) {
  //  return DeclPtrTy();
  //}

  virtual MemInitResult ActOnMemInitializer(DeclPtrTy ConstructorDecl,
                                            Scope *S,
                                            CXXScopeSpec &SS,
                                            IdentifierInfo *MemberOrBase,
                                            TypeTy *TemplateTypeTy,
                                            SourceLocation IdLoc,
                                            SourceLocation LParenLoc,
                                            ExprTy **Args, unsigned NumArgs,
                                            SourceLocation *CommaLocs,
                                            SourceLocation RParenLoc) {
    return true;
  }

  /// ActOnMemInitializers - This is invoked when all of the member
  /// initializers of a constructor have been parsed. ConstructorDecl
  /// is the function declaration (which will be a C++ constructor in
  /// a well-formed program), ColonLoc is the location of the ':' that
  /// starts the constructor initializer, and MemInit/NumMemInits
  /// contains the individual member (and base) initializers.
  /// AnyErrors will be true if there were any invalid member initializers
  /// that are not represented in the list.
  virtual void ActOnMemInitializers(DeclPtrTy ConstructorDecl,
                                    SourceLocation ColonLoc,
                                    MemInitTy **MemInits, unsigned NumMemInits,
                                    bool AnyErrors){
  }

 virtual void ActOnDefaultCtorInitializers(DeclPtrTy CDtorDecl) {}

  /// ActOnFinishCXXMemberSpecification - Invoked after all member declarators
  /// are parsed but *before* parsing of inline method definitions.
  virtual void ActOnFinishCXXMemberSpecification(Scope* S, SourceLocation RLoc,
                                                 DeclPtrTy TagDecl,
                                                 SourceLocation LBrac,
                                                 SourceLocation RBrac,
                                                 AttributeList *AttrList) {
  }

  //===---------------------------C++ Templates----------------------------===//

  /// \brief Called when a C++ template type parameter(e.g., "typename T") has 
  /// been parsed. 
  ///
  /// Given
  ///
  /// \code
  /// template<typename T, typename U = T> struct pair;
  /// \endcode
  ///
  /// this callback will be invoked twice: once for the type parameter \c T
  /// with \p Depth=0 and \p Position=0, and once for the type parameter \c U
  /// with \p Depth=0 and \p Position=1.
  ///
  /// \param Typename Specifies whether the keyword "typename" was used to 
  /// declare the type parameter (otherwise, "class" was used).
  ///
  /// \param Ellipsis Specifies whether this is a C++0x parameter pack.
  ///
  /// \param EllipsisLoc Specifies the start of the ellipsis.
  ///
  /// \param KeyLoc The location of the "class" or "typename" keyword.
  ///
  /// \param ParamName The name of the parameter, where NULL indicates an 
  /// unnamed template parameter.
  ///
  /// \param ParamNameLoc The location of the parameter name (if any).
  ///
  /// \param Depth The depth of this template parameter, e.g., the number of
  /// template parameter lists that occurred outside the template parameter
  /// list in which this template type parameter occurs.
  ///
  /// \param Position The zero-based position of this template parameter within
  /// its template parameter list, which is also the number of template
  /// parameters that precede this parameter in the template parameter list.
  ///
  /// \param EqualLoc The location of the '=' sign for the default template
  /// argument, if any.
  ///
  /// \param DefaultArg The default argument, if provided.
  virtual DeclPtrTy ActOnTypeParameter(Scope *S, bool Typename, bool Ellipsis,
                                       SourceLocation EllipsisLoc,
                                       SourceLocation KeyLoc,
                                       IdentifierInfo *ParamName,
                                       SourceLocation ParamNameLoc,
                                       unsigned Depth, unsigned Position,
                                       SourceLocation EqualLoc,
                                       TypeTy *DefaultArg) {
    return DeclPtrTy();
  }

  /// \brief Called when a C++ non-type template parameter has been parsed.
  ///
  /// Given
  ///
  /// \code
  /// template<int Size> class Array;
  /// \endcode
  ///
  /// This callback will be invoked for the 'Size' non-type template parameter.
  ///
  /// \param S The current scope.
  ///
  /// \param D The parsed declarator.
  ///
  /// \param Depth The depth of this template parameter, e.g., the number of
  /// template parameter lists that occurred outside the template parameter
  /// list in which this template type parameter occurs.
  ///
  /// \param Position The zero-based position of this template parameter within
  /// its template parameter list, which is also the number of template
  /// parameters that precede this parameter in the template parameter list.
  ///
  /// \param EqualLoc The location of the '=' sign for the default template
  /// argument, if any.
  ///
  /// \param DefaultArg The default argument, if provided.
  virtual DeclPtrTy ActOnNonTypeTemplateParameter(Scope *S, Declarator &D,
                                                  unsigned Depth,
                                                  unsigned Position,
                                                  SourceLocation EqualLoc,
                                                  ExprArg DefaultArg) {
    return DeclPtrTy();
  }

  /// \brief Adds a default argument to the given non-type template
  /// parameter.
  virtual void ActOnNonTypeTemplateParameterDefault(DeclPtrTy TemplateParam,
                                                    SourceLocation EqualLoc,
                                                    ExprArg Default) {
  }

  /// \brief Called when a C++ template template parameter has been parsed. 
  ///
  /// Given
  ///
  /// \code
  /// template<template <typename> class T> class X;
  /// \endcode
  ///
  /// this callback will be invoked for the template template parameter \c T.
  ///
  /// \param S The scope in which this template template parameter occurs.
  ///
  /// \param TmpLoc The location of the "template" keyword.
  ///
  /// \param TemplateParams The template parameters required by the template.
  ///
  /// \param ParamName The name of the parameter, or NULL if unnamed.
  ///
  /// \param ParamNameLoc The source location of the parameter name (if given).
  ///
  /// \param Depth The depth of this template parameter, e.g., the number of
  /// template parameter lists that occurred outside the template parameter
  /// list in which this template parameter occurs.
  ///
  /// \param Position The zero-based position of this template parameter within
  /// its template parameter list, which is also the number of template
  /// parameters that precede this parameter in the template parameter list.
  ///
  /// \param EqualLoc The location of the '=' sign for the default template
  /// argument, if any.
  ///
  /// \param DefaultArg The default argument, if provided.
  virtual DeclPtrTy ActOnTemplateTemplateParameter(Scope *S,
                                                   SourceLocation TmpLoc,
                                                   TemplateParamsTy *Params,
                                                   IdentifierInfo *ParamName,
                                                   SourceLocation ParamNameLoc,
                                                   unsigned Depth,
                                                   unsigned Position,
                                                   SourceLocation EqualLoc,
                                    const ParsedTemplateArgument &DefaultArg) {
    return DeclPtrTy();
  }

  /// ActOnTemplateParameterList - Called when a complete template
  /// parameter list has been parsed, e.g.,
  ///
  /// @code
  /// export template<typename T, T Size>
  /// @endcode
  ///
  /// Depth is the number of enclosing template parameter lists. This
  /// value does not include templates from outer scopes. For example:
  ///
  /// @code
  /// template<typename T> // depth = 0
  ///   class A {
  ///     template<typename U> // depth = 0
  ///       class B;
  ///   };
  ///
  /// template<typename T> // depth = 0
  ///   template<typename U> // depth = 1
  ///     class A<T>::B { ... };
  /// @endcode
  ///
  /// ExportLoc, if valid, is the position of the "export"
  /// keyword. Otherwise, "export" was not specified.
  /// TemplateLoc is the position of the template keyword, LAngleLoc
  /// is the position of the left angle bracket, and RAngleLoc is the
  /// position of the corresponding right angle bracket.
  /// Params/NumParams provides the template parameters that were
  /// parsed as part of the template-parameter-list.
  virtual TemplateParamsTy *
  ActOnTemplateParameterList(unsigned Depth,
                             SourceLocation ExportLoc,
                             SourceLocation TemplateLoc,
                             SourceLocation LAngleLoc,
                             DeclPtrTy *Params, unsigned NumParams,
                             SourceLocation RAngleLoc) {
    return 0;
  }

  /// \brief Form a type from a template and a list of template
  /// arguments.
  ///
  /// This action merely forms the type for the template-id, possibly
  /// checking well-formedness of the template arguments. It does not
  /// imply the declaration of any entity.
  ///
  /// \param Template  A template whose specialization results in a
  /// type, e.g., a class template or template template parameter.
  virtual TypeResult ActOnTemplateIdType(TemplateTy Template,
                                         SourceLocation TemplateLoc,
                                         SourceLocation LAngleLoc,
                                         ASTTemplateArgsPtr TemplateArgs,
                                         SourceLocation RAngleLoc) {
    return TypeResult();
  }

  /// \brief Note that a template ID was used with a tag.
  ///
  /// \param Type The result of ActOnTemplateIdType.
  ///
  /// \param TUK Either TUK_Reference or TUK_Friend.  Declarations and
  /// definitions are interpreted as explicit instantiations or
  /// specializations.
  ///
  /// \param TagSpec The tag keyword that was provided as part of the
  /// elaborated-type-specifier;  either class, struct, union, or enum.
  ///
  /// \param TagLoc The location of the tag keyword.
  virtual TypeResult ActOnTagTemplateIdType(TypeResult Type,
                                            TagUseKind TUK,
                                            //DeclSpec::TST TagSpec,
                                            SourceLocation TagLoc) {
    return TypeResult();
  }

  /// \brief Process the declaration or definition of an explicit
  /// class template specialization or a class template partial
  /// specialization.
  ///
  /// This routine is invoked when an explicit class template
  /// specialization or a class template partial specialization is
  /// declared or defined, to introduce the (partial) specialization
  /// and produce a declaration for it. In the following example,
  /// ActOnClassTemplateSpecialization will be invoked for the
  /// declarations at both A and B:
  ///
  /// \code
  /// template<typename T> class X;
  /// template<> class X<int> { }; // A: explicit specialization
  /// template<typename T> class X<T*> { }; // B: partial specialization
  /// \endcode
  ///
  /// Note that it is the job of semantic analysis to determine which
  /// of the two cases actually occurred in the source code, since
  /// they are parsed through the same path. The formulation of the
  /// template parameter lists describes which case we are in.
  ///
  /// \param S the current scope
  ///
  /// \param TagSpec whether this declares a class, struct, or union
  /// (template)
  ///
  /// \param TUK whether this is a declaration or a definition
  ///
  /// \param KWLoc the location of the 'class', 'struct', or 'union'
  /// keyword.
  ///
  /// \param SS the scope specifier preceding the template-id
  ///
  /// \param Template the declaration of the class template that we
  /// are specializing.
  ///
  /// \param Attr attributes on the specialization
  ///
  /// \param TemplateParameterLists the set of template parameter
  /// lists that apply to this declaration. In a well-formed program,
  /// the number of template parameter lists will be one more than the
  /// number of template-ids in the scope specifier. However, it is
  /// common for users to provide the wrong number of template
  /// parameter lists (such as a missing \c template<> prior to a
  /// specialization); the parser does not check this condition.
  virtual DeclResult
  ActOnClassTemplateSpecialization(Scope *S, unsigned TagSpec, TagUseKind TUK,
                                   SourceLocation KWLoc,
                                   CXXScopeSpec &SS,
                                   TemplateTy Template,
                                   SourceLocation TemplateNameLoc,
                                   SourceLocation LAngleLoc,
                                   ASTTemplateArgsPtr TemplateArgs,
                                   SourceLocation RAngleLoc,
                                   AttributeList *Attr,
                              MultiTemplateParamsArg TemplateParameterLists) {
    return DeclResult();
  }

  /// \brief Invoked when a declarator that has one or more template parameter
  /// lists has been parsed.
  ///
  /// This action is similar to ActOnDeclarator(), except that the declaration
  /// being created somehow involves a template, e.g., it is a template
  /// declaration or specialization.
  virtual DeclPtrTy ActOnTemplateDeclarator(Scope *S,
                              MultiTemplateParamsArg TemplateParameterLists,
                                            Declarator &D) {
    return DeclPtrTy();
  }

  /// \brief Invoked when the parser is beginning to parse a function template
  /// or function template specialization definition.
  virtual DeclPtrTy ActOnStartOfFunctionTemplateDef(Scope *FnBodyScope,
                                MultiTemplateParamsArg TemplateParameterLists,
                                                    Declarator &D) {
    return DeclPtrTy();
  }

  /// \brief Process the explicit instantiation of a class template
  /// specialization.
  ///
  /// This routine is invoked when an explicit instantiation of a
  /// class template specialization is encountered. In the following
  /// example, ActOnExplicitInstantiation will be invoked to force the
  /// instantiation of X<int>:
  ///
  /// \code
  /// template<typename T> class X { /* ... */ };
  /// template class X<int>; // explicit instantiation
  /// \endcode
  ///
  /// \param S the current scope
  ///
  /// \param ExternLoc the location of the 'extern' keyword that specifies that
  /// this is an extern template (if any).
  ///
  /// \param TemplateLoc the location of the 'template' keyword that
  /// specifies that this is an explicit instantiation.
  ///
  /// \param TagSpec whether this declares a class, struct, or union
  /// (template).
  ///
  /// \param KWLoc the location of the 'class', 'struct', or 'union'
  /// keyword.
  ///
  /// \param SS the scope specifier preceding the template-id.
  ///
  /// \param Template the declaration of the class template that we
  /// are instantiation.
  ///
  /// \param LAngleLoc the location of the '<' token in the template-id.
  ///
  /// \param TemplateArgs the template arguments used to form the
  /// template-id.
  ///
  /// \param TemplateArgLocs the locations of the template arguments.
  ///
  /// \param RAngleLoc the location of the '>' token in the template-id.
  ///
  /// \param Attr attributes that apply to this instantiation.
  virtual DeclResult
  ActOnExplicitInstantiation(Scope *S,
                             SourceLocation ExternLoc,
                             SourceLocation TemplateLoc,
                             unsigned TagSpec,
                             SourceLocation KWLoc,
                             const CXXScopeSpec &SS,
                             TemplateTy Template,
                             SourceLocation TemplateNameLoc,
                             SourceLocation LAngleLoc,
                             ASTTemplateArgsPtr TemplateArgs,
                             SourceLocation RAngleLoc,
                             AttributeList *Attr) {
    return DeclResult();
  }

  /// \brief Process the explicit instantiation of a member class of a
  /// class template specialization.
  ///
  /// This routine is invoked when an explicit instantiation of a
  /// member class of a class template specialization is
  /// encountered. In the following example,
  /// ActOnExplicitInstantiation will be invoked to force the
  /// instantiation of X<int>::Inner:
  ///
  /// \code
  /// template<typename T> class X { class Inner { /* ... */}; };
  /// template class X<int>::Inner; // explicit instantiation
  /// \endcode
  ///
  /// \param S the current scope
  ///
  /// \param ExternLoc the location of the 'extern' keyword that specifies that
  /// this is an extern template (if any).
  ///
  /// \param TemplateLoc the location of the 'template' keyword that
  /// specifies that this is an explicit instantiation.
  ///
  /// \param TagSpec whether this declares a class, struct, or union
  /// (template).
  ///
  /// \param KWLoc the location of the 'class', 'struct', or 'union'
  /// keyword.
  ///
  /// \param SS the scope specifier preceding the template-id.
  ///
  /// \param Template the declaration of the class template that we
  /// are instantiation.
  ///
  /// \param LAngleLoc the location of the '<' token in the template-id.
  ///
  /// \param TemplateArgs the template arguments used to form the
  /// template-id.
  ///
  /// \param TemplateArgLocs the locations of the template arguments.
  ///
  /// \param RAngleLoc the location of the '>' token in the template-id.
  ///
  /// \param Attr attributes that apply to this instantiation.
  virtual DeclResult
  ActOnExplicitInstantiation(Scope *S,
                             SourceLocation ExternLoc,
                             SourceLocation TemplateLoc,
                             unsigned TagSpec,
                             SourceLocation KWLoc,
                             CXXScopeSpec &SS,
                             IdentifierInfo *Name,
                             SourceLocation NameLoc,
                             AttributeList *Attr) {
    return DeclResult();
  }

  /// \brief Process the explicit instantiation of a function template or a
  /// member of a class template.
  ///
  /// This routine is invoked when an explicit instantiation of a
  /// function template or member function of a class template specialization 
  /// is encountered. In the following example,
  /// ActOnExplicitInstantiation will be invoked to force the
  /// instantiation of X<int>:
  ///
  /// \code
  /// template<typename T> void f(T);
  /// template void f(int); // explicit instantiation
  /// \endcode
  ///
  /// \param S the current scope
  ///
  /// \param ExternLoc the location of the 'extern' keyword that specifies that
  /// this is an extern template (if any).
  ///
  /// \param TemplateLoc the location of the 'template' keyword that
  /// specifies that this is an explicit instantiation.
  ///
  /// \param D the declarator describing the declaration to be implicitly
  /// instantiated.
  virtual DeclResult ActOnExplicitInstantiation(Scope *S,
                                                SourceLocation ExternLoc,
                                                SourceLocation TemplateLoc,
                                                Declarator &D) {
    return DeclResult();
  }
                             
                              
  /// \brief Called when the parser has parsed a C++ typename
  /// specifier that ends in an identifier, e.g., "typename T::type".
  ///
  /// \param TypenameLoc the location of the 'typename' keyword
  /// \param SS the nested-name-specifier following the typename (e.g., 'T::').
  /// \param II the identifier we're retrieving (e.g., 'type' in the example).
  /// \param IdLoc the location of the identifier.
  virtual TypeResult
  ActOnTypenameType(Scope *S, SourceLocation TypenameLoc, 
                    const CXXScopeSpec &SS, const IdentifierInfo &II, 
                    SourceLocation IdLoc) {
    return TypeResult();
  }

  /// \brief Called when the parser has parsed a C++ typename
  /// specifier that ends in a template-id, e.g.,
  /// "typename MetaFun::template apply<T1, T2>".
  ///
  /// \param TypenameLoc the location of the 'typename' keyword
  /// \param SS the nested-name-specifier following the typename (e.g., 'T::').
  /// \param TemplateLoc the location of the 'template' keyword, if any.
  /// \param Ty the type that the typename specifier refers to.
  virtual TypeResult
  ActOnTypenameType(Scope *S, SourceLocation TypenameLoc, 
                    const CXXScopeSpec &SS, SourceLocation TemplateLoc, 
                    TypeTy *Ty) {
    return TypeResult();
  }

  /// \brief Called when the parser begins parsing a construct which should not
  /// have access control applied to it.
  virtual void ActOnStartSuppressingAccessChecks() {
  }

  /// \brief Called when the parser finishes parsing a construct which should
  /// not have access control applied to it.
  virtual void ActOnStopSuppressingAccessChecks() {
  }

  //===---------------------------- Pragmas -------------------------------===//

  enum PragmaOptionsAlignKind {
    POAK_Native,  // #pragma options align=native
    POAK_Natural, // #pragma options align=natural
    POAK_Packed,  // #pragma options align=packed
    POAK_Power,   // #pragma options align=power
    POAK_Mac68k,  // #pragma options align=mac68k
    POAK_Reset    // #pragma options align=reset
  };

  /// ActOnPragmaOptionsAlign - Called on well formed #pragma options
  /// align={...}.
  virtual void ActOnPragmaOptionsAlign(PragmaOptionsAlignKind Kind,
                                       SourceLocation PragmaLoc,
                                       SourceLocation KindLoc) {
    return;
  }

  enum PragmaPackKind {
    PPK_Default, // #pragma pack([n])
    PPK_Show,    // #pragma pack(show), only supported by MSVC.
    PPK_Push,    // #pragma pack(push, [identifier], [n])
    PPK_Pop      // #pragma pack(pop, [identifier], [n])
  };

  /// ActOnPragmaPack - Called on well formed #pragma pack(...).
  virtual void ActOnPragmaPack(PragmaPackKind Kind,
                               IdentifierInfo *Name,
                               ExprTy *Alignment,
                               SourceLocation PragmaLoc,
                               SourceLocation LParenLoc,
                               SourceLocation RParenLoc) {
    return;
  }

  /// ActOnPragmaUnused - Called on well formed #pragma unused(...).
  virtual void ActOnPragmaUnused(const Token *Identifiers,
                                 unsigned NumIdentifiers, Scope *CurScope,
                                 SourceLocation PragmaLoc,
                                 SourceLocation LParenLoc,
                                 SourceLocation RParenLoc) {
    return;
  }

  /// ActOnPragmaWeakID - Called on well formed #pragma weak ident.
  virtual void ActOnPragmaWeakID(IdentifierInfo* WeakName,
                                 SourceLocation PragmaLoc,
                                 SourceLocation WeakNameLoc) {
    return;
  }

  /// ActOnPragmaWeakAlias - Called on well formed #pragma weak ident = ident.
  virtual void ActOnPragmaWeakAlias(IdentifierInfo* WeakName,
                                    IdentifierInfo* AliasName,
                                    SourceLocation PragmaLoc,
                                    SourceLocation WeakNameLoc,
                                    SourceLocation AliasNameLoc) {
    return;
  }
  
  /// \name Code completion actions
  ///
  /// These actions are used to signal that a code-completion token has been
  /// found at a point in the grammar where the Action implementation is
  /// likely to be able to provide a list of possible completions, e.g.,
  /// after the "." or "->" of a member access expression.
  /// 
  /// \todo Code completion for designated field initializers
  /// \todo Code completion for call arguments after a function template-id
  /// \todo Code completion within a call expression, object construction, etc.
  /// \todo Code completion within a template argument list.
  /// \todo Code completion for attributes.
  //@{
  
  /// \brief Describes the context in which code completion occurs.
  enum CodeCompletionContext {
    /// \brief Code completion occurs at top-level or namespace context.
    CCC_Namespace,
    /// \brief Code completion occurs within a class, struct, or union.
    CCC_Class,
    /// \brief Code completion occurs within an Objective-C interface, protocol,
    /// or category.
    CCC_ObjCInterface,
    /// \brief Code completion occurs within an Objective-C implementation or
    /// category implementation
    CCC_ObjCImplementation,
    /// \brief Code completion occurs within the list of instance variables
    /// in an Objective-C interface, protocol, category, or implementation.
    CCC_ObjCInstanceVariableList,
    /// \brief Code completion occurs following one or more template
    /// headers.
    CCC_Template,
    /// \brief Code completion occurs following one or more template
    /// headers within a class.
    CCC_MemberTemplate,
    /// \brief Code completion occurs within an expression.
    CCC_Expression,
    /// \brief Code completion occurs within a statement, which may
    /// also be an expression or a declaration.
    CCC_Statement,
    /// \brief Code completion occurs at the beginning of the
    /// initialization statement (or expression) in a for loop.
    CCC_ForInit,
    /// \brief Code completion occurs within the condition of an if,
    /// while, switch, or for statement.
    CCC_Condition,
    /// \brief Code completion occurs within the body of a function on a 
    /// recovery path, where we do not have a specific handle on our position
    /// in the grammar.
    CCC_RecoveryInFunction
  };
    
  /// \brief Code completion for an ordinary name that occurs within the given
  /// scope.
  ///
  /// \param S the scope in which the name occurs.
  ///
  /// \param CompletionContext the context in which code completion
  /// occurs.
  virtual void CodeCompleteOrdinaryName(Scope *S, 
                                    CodeCompletionContext CompletionContext) { }
  
  /// \brief Code completion for a member access expression.
  ///
  /// This code completion action is invoked when the code-completion token
  /// is found after the "." or "->" of a member access expression.
  ///
  /// \param S the scope in which the member access expression occurs.
  ///
  /// \param Base the base expression (e.g., the x in "x.foo") of the member
  /// access.
  ///
  /// \param OpLoc the location of the "." or "->" operator.
  ///
  /// \param IsArrow true when the operator is "->", false when it is ".".
  virtual void CodeCompleteMemberReferenceExpr(Scope *S, ExprTy *Base,
                                               SourceLocation OpLoc,
                                               bool IsArrow) { }
  
  /// \brief Code completion for a reference to a tag.
  ///
  /// This code completion action is invoked when the code-completion
  /// token is found after a tag keyword (struct, union, enum, or class).
  ///
  /// \param S the scope in which the tag reference occurs.
  ///
  /// \param TagSpec an instance of DeclSpec::TST, indicating what kind of tag
  /// this is (struct/union/enum/class).
  virtual void CodeCompleteTag(Scope *S, unsigned TagSpec) { }
  
  /// \brief Code completion for a case statement.
  ///
  /// \brief S the scope in which the case statement occurs.
  virtual void CodeCompleteCase(Scope *S) { }
  
  /// \brief Code completion for a call.
  ///
  /// \brief S the scope in which the call occurs.
  ///
  /// \param Fn the expression describing the function being called.
  ///
  /// \param Args the arguments to the function call (so far).
  ///
  /// \param NumArgs the number of arguments in \p Args.
  virtual void CodeCompleteCall(Scope *S, ExprTy *Fn,
                                ExprTy **Args, unsigned NumArgs) { }
                 
  /// \brief Code completion for the initializer of a variable declaration.
  ///
  /// \param S The scope in which the initializer occurs.
  ///
  /// \param D The declaration being initialized.
  virtual void CodeCompleteInitializer(Scope *S, DeclPtrTy D) { }
  
  /// \brief Code completion after the "return" keyword within a function.
  ///
  /// \param S The scope in which the return statement occurs.
  virtual void CodeCompleteReturn(Scope *S) { }
  
  /// \brief Code completion for the right-hand side of an assignment or
  /// compound assignment operator.
  ///
  /// \param S The scope in which the assignment occurs.
  ///
  /// \param LHS The left-hand side of the assignment expression.
  virtual void CodeCompleteAssignmentRHS(Scope *S, ExprTy *LHS) { }
  
  /// \brief Code completion for a C++ nested-name-specifier that precedes a
  /// qualified-id of some form.
  ///
  /// This code completion action is invoked when the code-completion token
  /// is found after the "::" of a nested-name-specifier.
  ///
  /// \param S the scope in which the nested-name-specifier occurs.
  /// 
  /// \param SS the scope specifier ending with "::".
  ///
  /// \parame EnteringContext whether we're entering the context of this
  /// scope specifier.
  virtual void CodeCompleteQualifiedId(Scope *S, CXXScopeSpec &SS,
                                       bool EnteringContext) { }
  
  /// \brief Code completion for a C++ "using" declaration or directive.
  ///
  /// This code completion action is invoked when the code-completion token is
  /// found after the "using" keyword.
  ///
  /// \param S the scope in which the "using" occurs.
  virtual void CodeCompleteUsing(Scope *S) { }
  
  /// \brief Code completion for a C++ using directive.
  ///
  /// This code completion action is invoked when the code-completion token is
  /// found after "using namespace".
  ///
  /// \param S the scope in which the "using namespace" occurs.
  virtual void CodeCompleteUsingDirective(Scope *S) { }
  
  /// \brief Code completion for a C++ namespace declaration or namespace
  /// alias declaration.
  ///
  /// This code completion action is invoked when the code-completion token is
  /// found after "namespace".
  ///
  /// \param S the scope in which the "namespace" token occurs.
  virtual void CodeCompleteNamespaceDecl(Scope *S) { }

  /// \brief Code completion for a C++ namespace alias declaration.
  ///
  /// This code completion action is invoked when the code-completion token is
  /// found after "namespace identifier = ".
  ///
  /// \param S the scope in which the namespace alias declaration occurs.
  virtual void CodeCompleteNamespaceAliasDecl(Scope *S) { }
  
  /// \brief Code completion for an operator name.
  ///
  /// This code completion action is invoked when the code-completion token is
  /// found after the keyword "operator".
  ///
  /// \param S the scope in which the operator keyword occurs.
  virtual void CodeCompleteOperatorName(Scope *S) { }

  /// \brief Code completion after the '@' at the top level.
  ///
  /// \param S the scope in which the '@' occurs.
  ///
  /// \param ObjCImpDecl the Objective-C implementation or category 
  /// implementation.
  ///
  /// \param InInterface whether we are in an Objective-C interface or
  /// protocol.
  virtual void CodeCompleteObjCAtDirective(Scope *S, DeclPtrTy ObjCImpDecl,
                                           bool InInterface) { }

  /// \brief Code completion after the '@' in the list of instance variables.
  virtual void CodeCompleteObjCAtVisibility(Scope *S) { }
  
  /// \brief Code completion after the '@' in a statement.
  virtual void CodeCompleteObjCAtStatement(Scope *S) { }

  /// \brief Code completion after the '@' in an expression.
  virtual void CodeCompleteObjCAtExpression(Scope *S) { }

  /// \brief Code completion for an ObjC property decl.
  ///
  /// This code completion action is invoked when the code-completion token is
  /// found after the left paren.
  ///
  /// \param S the scope in which the operator keyword occurs.  
  virtual void CodeCompleteObjCPropertyFlags(Scope *S, ObjCDeclSpec &ODS) { }

  /// \brief Code completion for the getter of an Objective-C property 
  /// declaration.  
  ///
  /// This code completion action is invoked when the code-completion
  /// token is found after the "getter = " in a property declaration.
  ///
  /// \param S the scope in which the property is being declared.
  ///
  /// \param ClassDecl the Objective-C class or category in which the property
  /// is being defined.
  ///
  /// \param Methods the set of methods declared thus far within \p ClassDecl.
  ///
  /// \param NumMethods the number of methods in \p Methods
  virtual void CodeCompleteObjCPropertyGetter(Scope *S, DeclPtrTy ClassDecl,
                                              DeclPtrTy *Methods,
                                              unsigned NumMethods) {
  }

  /// \brief Code completion for the setter of an Objective-C property 
  /// declaration.  
  ///
  /// This code completion action is invoked when the code-completion
  /// token is found after the "setter = " in a property declaration.
  ///
  /// \param S the scope in which the property is being declared.
  ///
  /// \param ClassDecl the Objective-C class or category in which the property
  /// is being defined.
  ///
  /// \param Methods the set of methods declared thus far within \p ClassDecl.
  ///
  /// \param NumMethods the number of methods in \p Methods
  virtual void CodeCompleteObjCPropertySetter(Scope *S, DeclPtrTy ClassDecl,
                                              DeclPtrTy *Methods,
                                              unsigned NumMethods) {
  }

  /// \brief Code completion for the receiver in an Objective-C message send.
  ///
  /// This code completion action is invoked when we see a '[' that indicates
  /// the start of an Objective-C message send.
  ///
  /// \param S The scope in which the Objective-C message send occurs.
  virtual void CodeCompleteObjCMessageReceiver(Scope *S) { }
  
  /// \brief Code completion for an ObjC message expression that sends
  /// a message to the superclass.
  ///
  /// This code completion action is invoked when the code-completion token is
  /// found after the class name and after each argument.
  ///
  /// \param S The scope in which the message expression occurs. 
  /// \param SuperLoc The location of the 'super' keyword.
  /// \param SelIdents The identifiers that describe the selector (thus far).
  /// \param NumSelIdents The number of identifiers in \p SelIdents.
  virtual void CodeCompleteObjCSuperMessage(Scope *S, SourceLocation SuperLoc,
                                            IdentifierInfo **SelIdents,
                                            unsigned NumSelIdents) { }

  /// \brief Code completion for an ObjC message expression that refers to
  /// a class method.
  ///
  /// This code completion action is invoked when the code-completion token is
  /// found after the class name and after each argument.
  ///
  /// \param S The scope in which the message expression occurs. 
  /// \param Receiver The type of the class that is receiving a message.
  /// \param SelIdents The identifiers that describe the selector (thus far).
  /// \param NumSelIdents The number of identifiers in \p SelIdents.
  virtual void CodeCompleteObjCClassMessage(Scope *S, TypeTy *Receiver,
                                            IdentifierInfo **SelIdents,
                                            unsigned NumSelIdents) { }
  
  /// \brief Code completion for an ObjC message expression that refers to
  /// an instance method.
  ///
  /// This code completion action is invoked when the code-completion token is
  /// found after the receiver expression and after each argument.
  ///
  /// \param S the scope in which the operator keyword occurs.  
  /// \param Receiver an expression for the receiver of the message. 
  /// \param SelIdents the identifiers that describe the selector (thus far).
  /// \param NumSelIdents the number of identifiers in \p SelIdents.
  virtual void CodeCompleteObjCInstanceMessage(Scope *S, ExprTy *Receiver,
                                               IdentifierInfo **SelIdents,
                                               unsigned NumSelIdents) { }

  /// \brief Code completion for a protocol declaration or definition, after
  /// the @protocol but before any identifier.
  ///
  /// \param S the scope in which the protocol declaration occurs.
  virtual void CodeCompleteObjCProtocolDecl(Scope *S) { }

  /// \brief Code completion for an Objective-C interface, after the
  /// @interface but before any identifier.
  virtual void CodeCompleteObjCInterfaceDecl(Scope *S) { }

  /// \brief Code completion for the superclass of an Objective-C
  /// interface, after the ':'.
  ///
  /// \param S the scope in which the interface declaration occurs.
  ///
  /// \param ClassName the name of the class being defined.
  virtual void CodeCompleteObjCSuperclass(Scope *S, 
                                          IdentifierInfo *ClassName,
                                          SourceLocation ClassNameLoc) {
  }

  /// \brief Code completion for an Objective-C implementation, after the
  /// @implementation but before any identifier.
  virtual void CodeCompleteObjCImplementationDecl(Scope *S) { }
  
  /// \brief Code completion for the category name in an Objective-C interface
  /// declaration.
  ///
  /// This code completion action is invoked after the '(' that indicates
  /// a category name within an Objective-C interface declaration.
  virtual void CodeCompleteObjCInterfaceCategory(Scope *S, 
                                                 IdentifierInfo *ClassName,
                                                 SourceLocation ClassNameLoc) {
  }

  /// \brief Code completion for the category name in an Objective-C category
  /// implementation.
  ///
  /// This code completion action is invoked after the '(' that indicates
  /// the category name within an Objective-C category implementation.
  virtual void CodeCompleteObjCImplementationCategory(Scope *S, 
                                                      IdentifierInfo *ClassName,
                                                  SourceLocation ClassNameLoc) {
  }
  
  /// \brief Code completion for the property names when defining an
  /// Objective-C property.
  ///
  /// This code completion action is invoked after @synthesize or @dynamic and
  /// after each "," within one of those definitions.
  virtual void CodeCompleteObjCPropertyDefinition(Scope *S, 
                                                  DeclPtrTy ObjCImpDecl) {
  }

  /// \brief Code completion for the instance variable name that should 
  /// follow an '=' when synthesizing an Objective-C property.
  ///
  /// This code completion action is invoked after each '=' that occurs within
  /// an @synthesized definition.
  virtual void CodeCompleteObjCPropertySynthesizeIvar(Scope *S, 
                                                   IdentifierInfo *PropertyName,
                                                  DeclPtrTy ObjCImpDecl) {
  }

  /// \brief Code completion for an Objective-C method declaration or
  /// definition, which may occur within an interface, category,
  /// extension, protocol, or implementation thereof (where applicable).
  ///
  /// This code completion action is invoked after the "-" or "+" that
  /// starts a method declaration or definition, and after the return
  /// type such a declaration (e.g., "- (id)").
  ///
  /// \param S The scope in which the completion occurs.
  ///
  /// \param IsInstanceMethod Whether this is an instance method
  /// (introduced with '-'); otherwise, it's a class method
  /// (introduced with '+').
  ///
  /// \param ReturnType If non-NULL, the specified return type of the method
  /// being declared or defined.
  ///
  /// \param IDecl The interface, category, protocol, or
  /// implementation, or category implementation in which this method
  /// declaration or definition occurs.
  virtual void CodeCompleteObjCMethodDecl(Scope *S, 
                                          bool IsInstanceMethod,
                                          TypeTy *ReturnType,
                                          DeclPtrTy IDecl) {
  }
  
  /// \brief Code completion for a selector identifier or argument name within
  /// an Objective-C method declaration.
  ///
  /// \param S The scope in which this code completion occurs.
  ///
  /// \param IsInstanceMethod Whether we are parsing an instance method (or, 
  /// if false, a class method).
  ///
  /// \param AtParameterName Whether the actual code completion point is at the
  /// argument name.
  ///
  /// \param ReturnType If non-NULL, the specified return type of the method
  /// being declared or defined.
  ///
  /// \param SelIdents The identifiers that occurred in the selector for the
  /// method declaration prior to the code completion point.
  ///
  /// \param NumSelIdents The number of identifiers provided by SelIdents.
  virtual void CodeCompleteObjCMethodDeclSelector(Scope *S, 
                                                  bool IsInstanceMethod,
                                                  bool AtParameterName,
                                                  TypeTy *ReturnType,
                                                  IdentifierInfo **SelIdents,
                                                  unsigned NumSelIdents) { }
  
  //@}
};

/// MinimalAction - Minimal actions are used by light-weight clients of the
/// parser that do not need name resolution or significant semantic analysis to
/// be performed.  The actions implemented here are in the form of unresolved
/// identifiers.  By using a simpler interface than the SemanticAction class,
/// the parser doesn't have to build complex data structures and thus runs more
/// quickly.
class MinimalAction : public Action {
  /// Translation Unit Scope - useful to Objective-C actions that need
  /// to lookup file scope declarations in the "ordinary" C decl namespace.
  /// For example, user-defined classes, built-in "id" type, etc.
  Scope *TUScope;
  IdentifierTable &Idents;
  Lexer &L;
  void *TypeNameInfoTablePtr;
public:
  MinimalAction(Lexer &l);
  ~MinimalAction();

  /// Registers an identifier as package name, unless \a IsLocal is set.
  virtual void ActOnImportSpec(SourceLocation PathLoc, StringRef ImportPath,
                               IdentifierInfo *LocalName,
                               bool IsLocal) LLVM_OVERRIDE;

  /// This looks at the IdentifierInfo::FETokenInfo field to determine whether
  /// the name is a package name, type name, or not in this scope.
  virtual IdentifierInfoType classifyIdentifier(const IdentifierInfo &II,
                                                const Scope* S);

  /// Registers an identifier as type name.
  virtual void ActOnTypeSpec(DeclPtrTy Decl, SourceLocation IILoc,
                             IdentifierInfo &II, Scope *S) LLVM_OVERRIDE;

  /// Registers an identifier as function name.
  virtual DeclPtrTy ActOnFunctionDecl(SourceLocation FuncLoc,
                                      SourceLocation NameLoc,
                                      IdentifierInfo &II,
                                      Scope *S) LLVM_OVERRIDE;

  /// ActOnPopScope - When a scope is popped, if any typedefs are now
  /// out-of-scope, they are removed from the IdentifierInfo::FETokenInfo field.
  virtual void ActOnPopScope(SourceLocation Loc, Scope *S) LLVM_OVERRIDE;
  virtual void ActOnTranslationUnitScope(Scope *S) LLVM_OVERRIDE;
};

///// PrettyStackTraceActionsDecl - If a crash occurs in the parser while parsing
///// something related to a virtualized decl, include that virtualized decl in
///// the stack trace.
//class PrettyStackTraceActionsDecl : public llvm::PrettyStackTraceEntry {
//  Action::DeclPtrTy TheDecl;
//  SourceLocation Loc;
//  Action &Actions;
//  SourceManager &SM;
//  const char *Message;
//public:
//  PrettyStackTraceActionsDecl(Action::DeclPtrTy Decl, SourceLocation L,
//                              Action &actions, SourceManager &sm,
//                              const char *Msg)
//  : TheDecl(Decl), Loc(L), Actions(actions), SM(sm), Message(Msg) {}
//
//  virtual void print(llvm::raw_ostream &OS) const;
//};
//
///// \brief RAII object that enters a new expression evaluation context.
//class EnterExpressionEvaluationContext {
//  /// \brief The action object.
//  Action &Actions;
//
//public:
//  EnterExpressionEvaluationContext(Action &Actions,
//                              Action::ExpressionEvaluationContext NewContext)
//    : Actions(Actions) {
//    Actions.PushExpressionEvaluationContext(NewContext);
//  }
//
//  ~EnterExpressionEvaluationContext() {
//    Actions.PopExpressionEvaluationContext();
//  }
//};

}  // end namespace gong

#endif
