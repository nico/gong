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
#include "gong/Parse/Ownership.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/ADT/PointerUnion.h"

namespace gong {
  class UnqualifiedId;
  // Semantic.
  class DeclSpec;
  class CXXScopeSpec;
  class Declarator;
  class AttributeList;
  // Parse.
  class IdentifierList;
  class Scope;
  class Action;
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
  typedef ASTOwningResult<&ActionBase::DeleteDecl> OwningDeclResult;
  // Note that these will replace ExprResult and StmtResult when the transition
  // is complete.

  /// Single expressions or statements as arguments.
  typedef ASTOwningPtr<&ActionBase::DeleteExpr> ExprArg;
  typedef ASTOwningPtr<&ActionBase::DeleteStmt> StmtArg;

  /// Multiple expressions or statements as arguments.
  typedef ASTMultiPtr<&ActionBase::DeleteExpr> MultiExprArg;
  typedef ASTMultiPtr<&ActionBase::DeleteStmt> MultiStmtArg;

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
  OwningDeclResult DeclError() { return OwningDeclResult(*this, true); }

  OwningExprResult ExprError(const DiagnosticBuilder&) { return ExprError(); }
  OwningStmtResult StmtError(const DiagnosticBuilder&) { return StmtError(); }

  OwningExprResult ExprEmpty() { return OwningExprResult(*this, false); }
  OwningStmtResult StmtEmpty() { return OwningStmtResult(*this, false); }

  /// Statistics.
  virtual void PrintStats() const {}

  /// getDeclName - Return a pretty name for the specified decl if possible, or
  /// an empty string if not.  This is used for pretty crash reporting.
  //virtual std::string getDeclName(DeclPtrTy D) { return ""; }

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
  // FIXME: Pass rhs, type once types are figured out
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
  // FIXME: Pass rhs, type once types are figured out
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

  /// A TypeName was parsed.
  virtual OwningDeclResult ActOnTypeName(SourceLocation IILoc,
                                         IdentifierInfo &II, Scope *CurScope) {
    return DeclError();
  }

  //===--------------------------------------------------------------------===//
  // Statement Parsing Callbacks.
  //===--------------------------------------------------------------------===//

  /// \brief Parsed a declaration in a statement context.
  ///
  /// \param Decl the declaration.
  virtual OwningStmtResult ActOnDeclStmt(DeclPtrTy Decl) {
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

  /// \brief Parsed an expression in a statement context.
  ///
  /// \param Expr the expression.
  virtual OwningStmtResult ActOnExprStmt(ExprArg Expr) {
    return StmtEmpty();
  }

  /// \brief Parsed an Assignment, such as "a, b[i] = c(), d[j]".
  ///
  /// \param LHSs the left-hand sides of the assignment.
  ///
  /// \param OpLoc the location of the assignment operator.
  ///
  /// \param OpKind the kind of operator, one of +=, -=, |=, ^=, *=, /=, %=,
  //  <<=, >>=, &=, &^=, =.
  ///
  /// \param RHSs the right-hand sides of the assignment.
  //FIXME: comma locs. Have an ExprList, similar to IdentList?
  virtual OwningStmtResult ActOnAssignmentStmt(MultiExprArg LHSs,
                                               SourceLocation OpLoc,
                                               tok::TokenKind OpKind,
                                               MultiExprArg RHSs) {
    return StmtEmpty();
  }

  /// \brief Parsed an ShortVarDecl, such as "a, b := c(), d[j]".
  ///
  /// \param LHSs the left-hand sides of the assignment.
  ///
  /// \param OpLoc the location of the ':=' operator.
  ///
  /// \param RHSs the right-hand sides of the assignment.
  virtual OwningStmtResult ActOnShortVarDeclStmt(IdentifierList &Idents,
                                                 SourceLocation OpLoc,
                                                 MultiExprArg RHSs) {
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

  // FIXME: actions for:
  // switchstmt
  // CommClause for cases
  // forstmt remnants

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

  /// \brief Parsed a CommClause with a "case".
  ///
  /// \param CaseLoc the location of the "case" keyword.
  ///
  /// \param CaseStmt the statement following "case".
  ///
  /// \param ColonLoc the location of the colon ending the "case".
  ///
  /// \param Elts the statements following the "case".
  virtual OwningStmtResult ActOnCommClause(SourceLocation CaseLoc,
                                           StmtArg CaseStmt, SourceLocation
                                           ColonLoc, MultiStmtArg Elts) {
    return StmtEmpty();
  }

  /// \brief Parsed a CommClause with a "default".
  ///
  /// \param DefaultLoc the location of the "default" keyword.
  ///
  /// \param ColonLoc the location of the colon following the "default".
  ///
  /// \param Elts the statements following the "default".
  virtual OwningStmtResult ActOnDefaultCommClause(SourceLocation DefaultLoc,
                                                  SourceLocation ColonLoc,
                                                  MultiStmtArg Elts) {
    return StmtEmpty();
  }

  /// \brief Parsed a "select" statement.
  ///
  /// \param SelectLoc the location of the "select" keyword.
  ///
  /// \param \L the location of the opening '{'.
  ///
  /// \param \R the location of the closing '}'.
  ///
  /// \param Elts the statements in the block.  They will all be statements
  /// produced by ActOnCommClause() or ActOnDefaultCommClause().
  virtual OwningStmtResult ActOnSelectStmt(SourceLocation SelectLoc,
                                           SourceLocation L, SourceLocation R,
                                           MultiStmtArg Elts) {
    return StmtEmpty();
  }

  /// \brief Parsed a simple "for" statement, such as "for c() {}" or "for {}".
  ///
  /// \param ForLoc the location of the "for" keyword.
  ///
  /// \param Condition the optional condition expression.
  ///
  /// \param Body the look body.
  virtual OwningStmtResult ActOnSimpleForStmt(SourceLocation ForLoc,
                                              ExprArg Condition, StmtArg Body) {
    return StmtEmpty();
  }

  /// \brief Parsed a "for" statement, such as "for i := 0; i < n; i++ {}".
  ///
  /// \param ForLoc the location of the "for" keyword.
  ///
  /// \param InitStmt the optional init stmt.
  ///
  /// \param FirstSemiLoc the location of the first ';'
  ///
  /// \param Condition the optional condition expression.
  ///
  /// \param SecondSemiLoc the location of the second ';'
  ///
  /// \param PostStmt the optional update stmt.
  ///
  /// \param Body the look body.
  virtual OwningStmtResult ActOnForStmt(SourceLocation ForLoc, StmtArg InitStmt,
                                        SourceLocation FirstSemiLoc,
                                        ExprArg Condition,
                                        SourceLocation SecondSemiLoc,
                                        StmtArg PostStmt, StmtArg Body) {
    return StmtEmpty();
  }

  /// \brief Parsed a "for" range statement, such as "for _, v := range m {}".
  ///
  /// \param ForLoc the location of the "for" keyword.
  ///
  /// \param LHSExpr the expression on the lhs of the assignment operator.
  ///
  /// \param CommaLoc the optional comma after the first lhs expression.
  ///
  /// \param OptLHSExpr the optional second expresson on the lhs of the
  /// assignment operator.
  ///
  /// \param OpLoc the location of the assigment operator.
  ///
  /// \param OpKind the kind of assignment operator, '=' or ':='.
  ///
  /// \param RangeLoc the location of the 'range' keyword.
  ///
  /// \param RangeExpr the expression on the rhs of the assignment operator.
  ///
  /// \param Body the look body.
  virtual OwningStmtResult ActOnRangeForStmt(SourceLocation ForLoc,
                                             ExprArg LHSExpr,
                                             SourceLocation CommaLoc,
                                             ExprArg OptLHSExpr,
                                             SourceLocation OpLoc,
                                             tok::TokenKind OpKind,
                                             SourceLocation RangeLoc,
                                             ExprArg RangeExpr, StmtArg Body) {
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

  // FIXME: actions for:
  // composite literals
  // function literals
  // operand names
  // method exprs
  // typeassertion
  //   builtin calls

  /// \brief Parsed an unary operator.
  ///
  /// \param OpLoc the location of the unary operator.
  ///
  /// \param Op the operator kind, one of +, -, !, ^, *, &, <-
  ///
  /// \param Input the operand of the unary operator.
  virtual OwningExprResult ActOnUnaryOp(SourceLocation OpLoc, tok::TokenKind Op,
                                        ExprArg Input) {
    return ExprEmpty();
  }

  /// \brief Parsed a binary operator.
  ///
  /// \param LHS the left-hand side of the operator.
  ///
  /// \param OpLoc the location of the binary operator.
  ///
  /// \param Op the operator kind, one of ||, &&, ==, !=, <, <=, >, >=, +, -, |,
  //  ^, *, /, %, <<, >>, &, &^.
  ///
  /// \param RHS the right-hand side of the binary operator.
  virtual OwningExprResult ActOnBinaryOp(ExprArg LHS, SourceLocation OpLoc,
                                         tok::TokenKind Op, ExprArg RHS) {
    return ExprEmpty();
  }

  /// \brief Parsed a numeric literal, such as "13", "2.5", "-5i".
  ///
  /// \param Tok the token containing the numeric literal.
  virtual OwningExprResult ActOnNumericLiteral(const Token &Tok) {
    return ExprEmpty();
  }

  /// \brief Parsed a rune literal, such as "'a'", "'\007'", "'\xa0'", "'\n'".
  ///
  /// \param Tok the token containing the rune literal.
  virtual OwningExprResult ActOnRuneLiteral(const Token &Tok) {
    return ExprEmpty();
  }

  /// \brief Parsed a string literal, such as '"foo"', '`bar`'.
  ///
  /// \param Tok the token containing the string literal.
  virtual OwningExprResult ActOnStringLiteral(const Token &Tok) {
    return ExprEmpty();
  }

  /// \brief Parsed an operand name, such as 'foo'.
  ///
  /// Note that in expression contexts, this is also called for type names and
  /// the first part of a QualifiedIdent (i.e. package names), as the parser
  /// cannot tell different kinds of names apart.
  virtual OwningExprResult ActOnOperandName(SourceLocation IILoc,
                                            IdentifierInfo *II,
                                            Scope *CurScope) {
    return ExprEmpty();
  }

  /// \brief Parsed a parenthesized expression.
  ///
  /// This can also be called for parenthesized types, for example when parsing
  /// "a = (int)(4)".
  ///
  /// Currently, this is also called for MethodExpr starts, such as "(*f)" which
  /// may be followed by ".g(f)". FIXME: Decide if that's desired, or if the
  /// parser should use lookahead to give more detailed callbacks for
  /// MethodExprs.
  ///
  /// \param L location of the '('.
  ///
  /// \param Expr the expression in parentheses.
  ///
  /// \param R the location of the ')'.
  virtual OwningExprResult ActOnParenExpr(SourceLocation L, ExprArg Expr,
                                          SourceLocation R) {
    return move(Expr);  // Default impl returns operand.
  }

  /// \brief Parse a selector suffix expression, such as "a.foo".
  ///
  /// Note that this is also called for the '.' in QualifiedIdents.
  ///
  /// \param Base the expression that the selector is attached to.
  ///
  /// \param OpLoc the location of the '.'.
  ///
  /// \param IILoc the location of the selector identifier.
  ///
  /// \param II the selector identifier.
  virtual OwningExprResult ActOnSelectorExpr(ExprArg Base, SourceLocation OpLoc,
                                             SourceLocation IILoc,
                                             IdentifierInfo *II) {
    return ExprEmpty();
  }

  /// \brief Parse an index suffix expression, such as "a[foo]".
  ///
  /// \param Base the expression that is being indexed.
  ///
  /// \param L the location of the opening '['.
  ///
  /// \param Idx the expression in brackets.
  ///
  /// \param R the location of the closing ']'.
  virtual OwningExprResult ActOnIndexExpr(ExprArg Base, SourceLocation L,
                                          ExprArg Idx, SourceLocation R) {
    return ExprEmpty();
  }

  /// \brief Parse a slice suffix expression, such as "a[4:5]" or "b[:]".
  ///
  /// \param Base the expression that is being sliced.
  ///
  /// \param L the location of the opening '['.
  ///
  /// \param FirstExpr the optional expression before the ':'.
  ///
  /// \param ColonLoc the location of the ':'.
  ///
  /// \param SecondExpr the optional expression after the ':'.
  ///
  /// \param R the location of the closing ']'.
  virtual OwningExprResult ActOnSliceExpr(ExprArg Base, SourceLocation L,
                                          ExprArg FirstExpr,
                                          SourceLocation ColonLoc,
                                          ExprArg SecondExpr,
                                          SourceLocation R) {
    return ExprEmpty();
  }

  /// \brief Parsed a call suffix expression, such as "f(4, 5...,)".
  ///
  /// Note that this is also called for conversion expressions, such as
  /// "int(4.4)". FIXME: Rename to ActOnCallOrConversionExpr()?
  /// FIXME: Decide if this hsould also be called for BuiltinCalls (probably).
  ///
  /// \param Base the expression that is being called.
  ///
  /// \param L the location of the opening '('.
  ///
  /// \param Args the call arguments
  ///
  /// \param EllipsisLoc location of the optional trailing '...'.
  ///
  /// \param TrailingCommaLoc location of the optional trailing ','.
  ///
  /// \param R the location of the closing ')'.
  //FIXME: comma locs. Have an ExprList, similar to IdentList?
  virtual OwningExprResult ActOnCallExpr(ExprArg Base,
                                         SourceLocation L,
                                         MultiExprArg Args,
                                         SourceLocation EllipsisLoc,
                                         SourceLocation TrailingCommaLoc,
                                         SourceLocation R) {
    return ExprEmpty();
  }











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


  /// ActOnPopScope - This callback is called immediately before the specified
  /// scope is popped and deleted.
  virtual void ActOnPopScope(SourceLocation Loc, Scope *S) {}

  // FIXME: Rename to FileScope (there's also Universe and Package scope)
  /// ActOnTranslationUnitScope - This callback is called once, immediately
  /// after creating the translation unit scope (in Parser::Initialize).
  virtual void ActOnTranslationUnitScope(Scope *S) {}

  /// ActOnEndOfTranslationUnit - This is called at the very end of the
  /// translation unit when EOF is reached and all but the top-level scope is
  /// popped.
  virtual void ActOnEndOfTranslationUnit() {}

  //===--------------------------------------------------------------------===//
  // Type Parsing Callbacks.
  //===--------------------------------------------------------------------===//

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
  /// \param OwnedDecl the callee should set this flag true when the returned
  /// declaration is "owned" by this reference. Ownership is handled entirely
  /// by the action module.
  ///
  /// \returns the declaration to which this tag refers.
  virtual DeclPtrTy ActOnTag(Scope *S, unsigned TagSpec, TagUseKind TUK,
                             SourceLocation KWLoc, CXXScopeSpec &SS,
                             IdentifierInfo *Name, SourceLocation NameLoc,
                             AttributeList *Attr, //AccessSpecifier AS,
                             bool &OwnedDecl, bool &IsDependent) {
    return DeclPtrTy();
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

  virtual void ActOnForEachDeclStmt(DeclGroupPtrTy Decl) {
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

  // C++ Statements
  virtual OwningStmtResult ActOnCXXCatchBlock(SourceLocation CatchLoc,
                                              DeclPtrTy ExceptionDecl,
                                              StmtArg HandlerBlock) {
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
  
  // Postfix Expressions.

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
  /// \param HasTrailingLParen whether this member name is immediately followed
  /// by a left parentheses ('(').
  virtual OwningExprResult ActOnMemberAccessExpr(Scope *S, ExprArg Base,
                                                 SourceLocation OpLoc,
                                                 tok::TokenKind OpKind,
                                                 CXXScopeSpec &SS,
                                                 UnqualifiedId &Member,
                                                 bool HasTrailingLParen) {
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

  virtual OwningExprResult ActOnCastExpr(Scope *S, SourceLocation LParenLoc,
                                         TypeTy *Ty, SourceLocation RParenLoc,
                                         ExprArg Op) {
    return ExprEmpty();
  }

  virtual bool TypeIsVectorType(TypeTy *Ty) {
    return false;
  }

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

  /// ActOnFinishFullExpr - Called whenever a full expression has been parsed.
  /// (C++ [intro.execution]p12).
  virtual OwningExprResult ActOnFinishFullExpr(ExprArg Expr) {
    return move(Expr);
  }

  //===---------------------------- C++ Classes ---------------------------===//
  /// ActOnFinishCXXMemberSpecification - Invoked after all member declarators
  /// are parsed but *before* parsing of inline method definitions.
  virtual void ActOnFinishCXXMemberSpecification(Scope* S, SourceLocation RLoc,
                                                 DeclPtrTy TagDecl,
                                                 SourceLocation LBrac,
                                                 SourceLocation RBrac,
                                                 AttributeList *AttrList) {
  }
  
  /// \name Code completion actions
  ///
  /// These actions are used to signal that a code-completion token has been
  /// found at a point in the grammar where the Action implementation is
  /// likely to be able to provide a list of possible completions, e.g.,
  /// after the "." or "->" of a member access expression.
  /// 
  /// \todo Code completion for designated field initializers
  /// \todo Code completion within a call expression, object construction, etc.
  /// \todo Code completion for attributes.
  //@{
  
  /// \brief Describes the context in which code completion occurs.
  enum CodeCompletionContext {
    /// \brief Code completion occurs at top-level or namespace context.
    CCC_Namespace,
    /// \brief Code completion occurs within a class, struct, or union.
    CCC_Class,
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
