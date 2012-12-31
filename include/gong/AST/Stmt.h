//===--- Stmt.h - Classes for representing statements -----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Stmt interface and subclasses.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_AST_STMT_H
#define LLVM_GONG_AST_STMT_H

#include "gong/Basic/SourceLocation.h"
#include "llvm/Support/ErrorHandling.h"

#if 0

#include "gong/AST/DeclGroup.h"
#include "gong/AST/StmtIterator.h"
#include "gong/Basic/IdentifierTable.h"
#include "gong/Basic/LLVM.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/Support/Compiler.h"
#include <string>

namespace llvm {
  class FoldingSetNodeID;
}
#endif

namespace gong {
class ASTContext;

#if 0
  class Attr;
  class Decl;
  class Expr;
  class IdentifierInfo;
  class LabelDecl;
  class ParmVarDecl;
  class PrinterHelper;
  struct PrintingPolicy;
  class QualType;
  class SourceManager;
  class StringLiteral;
  class SwitchStmt;
  class Token;
  class VarDecl;

  //===--------------------------------------------------------------------===//
  // ExprIterator - Iterators for iterating over Stmt* arrays that contain
  //  only Expr*.  This is needed because AST nodes use Stmt* arrays to store
  //  references to children (to be compatible with StmtIterator).
  //===--------------------------------------------------------------------===//

  class Stmt;
  class Expr;

  class ExprIterator {
    Stmt** I;
  public:
    ExprIterator(Stmt** i) : I(i) {}
    ExprIterator() : I(0) {}
    ExprIterator& operator++() { ++I; return *this; }
    ExprIterator operator-(size_t i) { return I-i; }
    ExprIterator operator+(size_t i) { return I+i; }
    Expr* operator[](size_t idx);
    // FIXME: Verify that this will correctly return a signed distance.
    signed operator-(const ExprIterator& R) const { return I - R.I; }
    Expr* operator*() const;
    Expr* operator->() const;
    bool operator==(const ExprIterator& R) const { return I == R.I; }
    bool operator!=(const ExprIterator& R) const { return I != R.I; }
    bool operator>(const ExprIterator& R) const { return I > R.I; }
    bool operator>=(const ExprIterator& R) const { return I >= R.I; }
  };

  class ConstExprIterator {
    const Stmt * const *I;
  public:
    ConstExprIterator(const Stmt * const *i) : I(i) {}
    ConstExprIterator() : I(0) {}
    ConstExprIterator& operator++() { ++I; return *this; }
    ConstExprIterator operator+(size_t i) const { return I+i; }
    ConstExprIterator operator-(size_t i) const { return I-i; }
    const Expr * operator[](size_t idx) const;
    signed operator-(const ConstExprIterator& R) const { return I - R.I; }
    const Expr * operator*() const;
    const Expr * operator->() const;
    bool operator==(const ConstExprIterator& R) const { return I == R.I; }
    bool operator!=(const ConstExprIterator& R) const { return I != R.I; }
    bool operator>(const ConstExprIterator& R) const { return I > R.I; }
    bool operator>=(const ConstExprIterator& R) const { return I >= R.I; }
  };
#endif

//===----------------------------------------------------------------------===//
// AST classes for statements.
//===----------------------------------------------------------------------===//

/// This represents one statement.
///
class Stmt {
public:
  enum StmtClass {
    NoStmtClass = 0,
#define STMT(CLASS, PARENT) CLASS##Class,
#define FIRST_STMT(CLASS) firstStmtConstant = CLASS##Class,
#define LAST_STMT(CLASS) lastStmtConstant = CLASS##Class,
#define FIRST_EXPR(CLASS) firstExprConstant = CLASS##Class,
#define LAST_EXPR(CLASS) lastExprConstant = CLASS##Class
#define ABSTRACT_EXPR(CLASS, PARENT)
#include "gong/AST/StmtNodes.def"
  };

  // Make vanilla 'new' and 'delete' illegal for Stmts.
protected:
  void* operator new(size_t bytes) throw() {
    llvm_unreachable("Stmts cannot be allocated with regular 'new'.");
  }
  void operator delete(void* data) throw() {
    llvm_unreachable("Stmts cannot be released with regular 'delete'.");
  }

  class StmtBitfields {
    friend class Stmt;

    /// \brief The statement class.
    unsigned sClass : 8;
  };
  enum { NumStmtBits = 8 };

  class BlockStmtBitfields {
    friend class BlockStmt;
    unsigned : NumStmtBits;

    unsigned NumStmts : 32 - NumStmtBits;
  };

#if 0
  class ExprBitfields {
    friend class Expr;
    friend class DeclRefExpr; // computeDependence
    friend class InitListExpr; // ctor
    friend class DesignatedInitExpr; // ctor
    friend class BlockDeclRefExpr; // ctor
    friend class ASTStmtReader; // deserialization
    friend class CXXNewExpr; // ctor
    friend class DependentScopeDeclRefExpr; // ctor
    friend class CXXConstructExpr; // ctor
    friend class CallExpr; // ctor
    friend class OffsetOfExpr; // ctor
    friend class ObjCMessageExpr; // ctor
    friend class ObjCArrayLiteral; // ctor
    friend class ObjCDictionaryLiteral; // ctor
    friend class ShuffleVectorExpr; // ctor
    friend class ParenListExpr; // ctor
    friend class CXXUnresolvedConstructExpr; // ctor
    friend class CXXDependentScopeMemberExpr; // ctor
    friend class OverloadExpr; // ctor
    friend class PseudoObjectExpr; // ctor
    friend class AtomicExpr; // ctor
    unsigned : NumStmtBits;

    unsigned ValueKind : 2;
    unsigned ObjectKind : 2;
    unsigned TypeDependent : 1;
    unsigned ValueDependent : 1;
    unsigned InstantiationDependent : 1;
    unsigned ContainsUnexpandedParameterPack : 1;
  };
  enum { NumExprBits = 16 };

  class CharacterLiteralBitfields {
    friend class CharacterLiteral;
    unsigned : NumExprBits;

    unsigned Kind : 2;
  };

  class FloatingLiteralBitfields {
    friend class FloatingLiteral;
    unsigned : NumExprBits;

    unsigned IsIEEE : 1; // Distinguishes between PPC128 and IEEE128.
    unsigned IsExact : 1;
  };

  class UnaryExprOrTypeTraitExprBitfields {
    friend class UnaryExprOrTypeTraitExpr;
    unsigned : NumExprBits;

    unsigned Kind : 2;
    unsigned IsType : 1; // true if operand is a type, false if an expression.
  };

  class DeclRefExprBitfields {
    friend class DeclRefExpr;
    friend class ASTStmtReader; // deserialization
    unsigned : NumExprBits;

    unsigned HasQualifier : 1;
    unsigned HasTemplateKWAndArgsInfo : 1;
    unsigned HasFoundDecl : 1;
    unsigned HadMultipleCandidates : 1;
    unsigned RefersToEnclosingLocal : 1;
  };

  class CastExprBitfields {
    friend class CastExpr;
    unsigned : NumExprBits;

    unsigned Kind : 6;
    unsigned BasePathSize : 32 - 6 - NumExprBits;
  };

  class CallExprBitfields {
    friend class CallExpr;
    unsigned : NumExprBits;

    unsigned NumPreArgs : 1;
  };

  class ExprWithCleanupsBitfields {
    friend class ExprWithCleanups;
    friend class ASTStmtReader; // deserialization

    unsigned : NumExprBits;

    unsigned NumObjects : 32 - NumExprBits;
  };

  class PseudoObjectExprBitfields {
    friend class PseudoObjectExpr;
    friend class ASTStmtReader; // deserialization

    unsigned : NumExprBits;

    // These don't need to be particularly wide, because they're
    // strictly limited by the forms of expressions we permit.
    unsigned NumSubExprs : 8;
    unsigned ResultIndex : 32 - 8 - NumExprBits;
  };

  class ObjCIndirectCopyRestoreExprBitfields {
    friend class ObjCIndirectCopyRestoreExpr;
    unsigned : NumExprBits;

    unsigned ShouldCopy : 1;
  };

  class InitListExprBitfields {
    friend class InitListExpr;

    unsigned : NumExprBits;

    /// Whether this initializer list originally had a GNU array-range
    /// designator in it. This is a temporary marker used by CodeGen.
    unsigned HadArrayRangeDesignator : 1;

    /// Whether this initializer list initializes a std::initializer_list
    /// object.
    unsigned InitializesStdInitializerList : 1;
  };

  class TypeTraitExprBitfields {
    friend class TypeTraitExpr;
    friend class ASTStmtReader;
    friend class ASTStmtWriter;
    
    unsigned : NumExprBits;
    
    /// \brief The kind of type trait, which is a value of a TypeTrait enumerator.
    unsigned Kind : 8;
    
    /// \brief If this expression is not value-dependent, this indicates whether
    /// the trait evaluated true or false.
    unsigned Value : 1;

    /// \brief The number of arguments to this type trait.
    unsigned NumArgs : 32 - 8 - 1 - NumExprBits;
  };
#endif
  
  union {
    // FIXME: this is wasteful on 64-bit platforms.
    void *Aligner;

    StmtBitfields StmtBits;
    BlockStmtBitfields BlockStmtBits;
#if 0
    ExprBitfields ExprBits;
    CharacterLiteralBitfields CharacterLiteralBits;
    FloatingLiteralBitfields FloatingLiteralBits;
    UnaryExprOrTypeTraitExprBitfields UnaryExprOrTypeTraitExprBits;
    DeclRefExprBitfields DeclRefExprBits;
    CastExprBitfields CastExprBits;
    CallExprBitfields CallExprBits;
    ExprWithCleanupsBitfields ExprWithCleanupsBits;
    PseudoObjectExprBitfields PseudoObjectExprBits;
    ObjCIndirectCopyRestoreExprBitfields ObjCIndirectCopyRestoreExprBits;
    InitListExprBitfields InitListExprBits;
    TypeTraitExprBitfields TypeTraitExprBits;
#endif
  };

  friend class ASTStmtReader;
  friend class ASTStmtWriter;

public:
  // Only allow allocation of Stmts using the allocator in ASTContext
  // or by doing a placement new.
  void* operator new(size_t bytes, ASTContext& C,
                     unsigned alignment = 8) throw();

  void* operator new(size_t bytes, ASTContext* C,
                     unsigned alignment = 8) throw();

  void* operator new(size_t bytes, void* mem) throw() {
    return mem;
  }

  void operator delete(void*, ASTContext&, unsigned) throw() { }
  void operator delete(void*, ASTContext*, unsigned) throw() { }
  void operator delete(void*, std::size_t) throw() { }
  void operator delete(void*, void*) throw() { }

public:
  /// \brief A placeholder type used to construct an empty shell of a
  /// type, that will be filled in later (e.g., by some
  /// de-serialization).
  struct EmptyShell { };

private:
  /// \brief Whether statistic collection is enabled.
  static bool StatisticsEnabled;

protected:
  /// \brief Construct an empty statement.
  explicit Stmt(StmtClass SC, EmptyShell) {
    StmtBits.sClass = SC;
    //if (StatisticsEnabled) Stmt::addStmtClass(SC);
  }

public:
  Stmt(StmtClass SC) {
    StmtBits.sClass = SC;
    //if (StatisticsEnabled) Stmt::addStmtClass(SC);
  }

  StmtClass getStmtClass() const {
    return static_cast<StmtClass>(StmtBits.sClass);
  }
  const char *getStmtClassName() const;

#if 0
  /// SourceLocation tokens are not useful in isolation - they are low level
  /// value objects created/interpreted by SourceManager. We assume AST
  /// clients will have a pointer to the respective SourceManager.
  SourceRange getSourceRange() const LLVM_READONLY;
  SourceLocation getLocStart() const LLVM_READONLY;
  SourceLocation getLocEnd() const LLVM_READONLY;

  // global temp stats (until we have a per-module visitor)
  static void addStmtClass(const StmtClass s);
  static void EnableStatistics();
  static void PrintStats();

  /// \brief Dumps the specified AST fragment and all subtrees to
  /// \c llvm::errs().
  LLVM_ATTRIBUTE_USED void dump() const;
  LLVM_ATTRIBUTE_USED void dump(SourceManager &SM) const;
  void dump(raw_ostream &OS, SourceManager &SM) const;

  /// dumpPretty/printPretty - These two methods do a "pretty print" of the AST
  /// back to its original source language syntax.
  void dumpPretty(ASTContext &Context) const;
  void printPretty(raw_ostream &OS, PrinterHelper *Helper,
                   const PrintingPolicy &Policy,
                   unsigned Indentation = 0) const;

  /// viewAST - Visualize an AST rooted at this Stmt* using GraphViz.  Only
  ///   works on systems with GraphViz (Mac OS X) or dot+gv installed.
  void viewAST() const;

  /// Skip past any implicit AST nodes which might surround this
  /// statement, such as ExprWithCleanups or ImplicitCastExpr nodes.
  Stmt *IgnoreImplicit();

  const Stmt *stripLabelLikeStatements() const;
  Stmt *stripLabelLikeStatements() {
    return const_cast<Stmt*>(
      const_cast<const Stmt*>(this)->stripLabelLikeStatements());
  }

  /// hasImplicitControlFlow - Some statements (e.g. short circuited operations)
  ///  contain implicit control-flow in the order their subexpressions
  ///  are evaluated.  This predicate returns true if this statement has
  ///  such implicit control-flow.  Such statements are also specially handled
  ///  within CFGs.
  bool hasImplicitControlFlow() const;

  /// Child Iterators: All subclasses must implement 'children'
  /// to permit easy iteration over the substatements/subexpessions of an
  /// AST node.  This permits easy iteration over all nodes in the AST.
  typedef StmtIterator       child_iterator;
  typedef ConstStmtIterator  const_child_iterator;

  typedef StmtRange          child_range;
  typedef ConstStmtRange     const_child_range;

  child_range children();
  const_child_range children() const {
    return const_cast<Stmt*>(this)->children();
  }

  child_iterator child_begin() { return children().first; }
  child_iterator child_end() { return children().second; }

  const_child_iterator child_begin() const { return children().first; }
  const_child_iterator child_end() const { return children().second; }

  /// \brief Produce a unique representation of the given statement.
  ///
  /// \param ID once the profiling operation is complete, will contain
  /// the unique representation of the given statement.
  ///
  /// \param Context the AST context in which the statement resides
  ///
  /// \param Canonical whether the profile should be based on the canonical
  /// representation of this statement (e.g., where non-type template
  /// parameters are identified by index/level rather than their
  /// declaration pointers) or the exact representation of the statement as
  /// written in the source.
  void Profile(llvm::FoldingSetNodeID &ID, const ASTContext &Context,
               bool Canonical) const;
#endif
};

#if 0
/// DeclStmt - Adaptor class for mixing declarations with statements and
/// expressions. For example, BlockStmt mixes statements, expressions
/// and declarations (variables, types). Another example is ForStmt, where
/// the first statement can be an expression or a declaration.
///
class DeclStmt : public Stmt {
  DeclGroupRef DG;
  SourceLocation StartLoc, EndLoc;

public:
  DeclStmt(DeclGroupRef dg, SourceLocation startLoc,
           SourceLocation endLoc) : Stmt(DeclStmtClass), DG(dg),
                                    StartLoc(startLoc), EndLoc(endLoc) {}

  /// \brief Build an empty declaration statement.
  explicit DeclStmt(EmptyShell Empty) : Stmt(DeclStmtClass, Empty) { }

  /// isSingleDecl - This method returns true if this DeclStmt refers
  /// to a single Decl.
  bool isSingleDecl() const {
    return DG.isSingleDecl();
  }

  const Decl *getSingleDecl() const { return DG.getSingleDecl(); }
  Decl *getSingleDecl() { return DG.getSingleDecl(); }

  const DeclGroupRef getDeclGroup() const { return DG; }
  DeclGroupRef getDeclGroup() { return DG; }
  void setDeclGroup(DeclGroupRef DGR) { DG = DGR; }

  SourceLocation getStartLoc() const { return StartLoc; }
  void setStartLoc(SourceLocation L) { StartLoc = L; }
  SourceLocation getEndLoc() const { return EndLoc; }
  void setEndLoc(SourceLocation L) { EndLoc = L; }

  SourceLocation getLocStart() const LLVM_READONLY { return StartLoc; }
  SourceLocation getLocEnd() const LLVM_READONLY { return EndLoc; }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == DeclStmtClass;
  }

  // Iterators over subexpressions.
  child_range children() {
    return child_range(child_iterator(DG.begin(), DG.end()),
                       child_iterator(DG.end(), DG.end()));
  }

  typedef DeclGroupRef::iterator decl_iterator;
  typedef DeclGroupRef::const_iterator const_decl_iterator;

  decl_iterator decl_begin() { return DG.begin(); }
  decl_iterator decl_end() { return DG.end(); }
  const_decl_iterator decl_begin() const { return DG.begin(); }
  const_decl_iterator decl_end() const { return DG.end(); }

  typedef std::reverse_iterator<decl_iterator> reverse_decl_iterator;
  reverse_decl_iterator decl_rbegin() {
    return reverse_decl_iterator(decl_end());
  }
  reverse_decl_iterator decl_rend() {
    return reverse_decl_iterator(decl_begin());
  }
};

/// NullStmt - This is the null statement ";": C99 6.8.3p3.
///
class NullStmt : public Stmt {
  SourceLocation SemiLoc;

  /// \brief True if the null statement was preceded by an empty macro, e.g:
  /// @code
  ///   #define CALL(x)
  ///   CALL(0);
  /// @endcode
  bool HasLeadingEmptyMacro;
public:
  NullStmt(SourceLocation L, bool hasLeadingEmptyMacro = false)
    : Stmt(NullStmtClass), SemiLoc(L),
      HasLeadingEmptyMacro(hasLeadingEmptyMacro) {}

  /// \brief Build an empty null statement.
  explicit NullStmt(EmptyShell Empty) : Stmt(NullStmtClass, Empty),
      HasLeadingEmptyMacro(false) { }

  SourceLocation getSemiLoc() const { return SemiLoc; }
  void setSemiLoc(SourceLocation L) { SemiLoc = L; }

  bool hasLeadingEmptyMacro() const { return HasLeadingEmptyMacro; }

  SourceLocation getLocStart() const LLVM_READONLY { return SemiLoc; }
  SourceLocation getLocEnd() const LLVM_READONLY { return SemiLoc; }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == NullStmtClass;
  }

  child_range children() { return child_range(); }

  friend class ASTStmtReader;
  friend class ASTStmtWriter;
};
#endif

/// This represents a block of statements like { stmt stmt }.
class BlockStmt : public Stmt {
  Stmt** Body;
  SourceLocation LBracLoc, RBracLoc;
public:
  BlockStmt(ASTContext &C, ArrayRef<Stmt*> StmtStart,
            SourceLocation LB, SourceLocation RB);

  // \brief Build an empty compound statment with a location.
  explicit BlockStmt(SourceLocation Loc)
    : Stmt(BlockStmtClass), Body(0), LBracLoc(Loc), RBracLoc(Loc) {
    BlockStmtBits.NumStmts = 0;
  }

  // \brief Build an empty compound statement.
  explicit BlockStmt(EmptyShell Empty)
    : Stmt(BlockStmtClass, Empty), Body(0) {
    BlockStmtBits.NumStmts = 0;
  }

  void setStmts(ASTContext &C, Stmt **Stmts, unsigned NumStmts);

  bool body_empty() const { return BlockStmtBits.NumStmts == 0; }
  unsigned size() const { return BlockStmtBits.NumStmts; }

  typedef Stmt** body_iterator;
  body_iterator body_begin() { return Body; }
  body_iterator body_end() { return Body + size(); }
  Stmt *body_back() { return !body_empty() ? Body[size()-1] : 0; }

  void setLastStmt(Stmt *S) {
    assert(!body_empty() && "setLastStmt");
    Body[size()-1] = S;
  }

  typedef Stmt* const * const_body_iterator;
  const_body_iterator body_begin() const { return Body; }
  const_body_iterator body_end() const { return Body + size(); }
  const Stmt *body_back() const { return !body_empty() ? Body[size()-1] : 0; }

  typedef std::reverse_iterator<body_iterator> reverse_body_iterator;
  reverse_body_iterator body_rbegin() {
    return reverse_body_iterator(body_end());
  }
  reverse_body_iterator body_rend() {
    return reverse_body_iterator(body_begin());
  }

  typedef std::reverse_iterator<const_body_iterator>
          const_reverse_body_iterator;

  const_reverse_body_iterator body_rbegin() const {
    return const_reverse_body_iterator(body_end());
  }

  const_reverse_body_iterator body_rend() const {
    return const_reverse_body_iterator(body_begin());
  }

  SourceLocation getLocStart() const LLVM_READONLY { return LBracLoc; }
  SourceLocation getLocEnd() const LLVM_READONLY { return RBracLoc; }

  SourceLocation getLBracLoc() const { return LBracLoc; }
  void setLBracLoc(SourceLocation L) { LBracLoc = L; }
  SourceLocation getRBracLoc() const { return RBracLoc; }
  void setRBracLoc(SourceLocation L) { RBracLoc = L; }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == BlockStmtClass;
  }

  // Iterators
  //child_range children() {
  //  return child_range(&Body[0], &Body[0]+BlockStmtBits.NumStmts);
  //}

  //const_child_range children() const {
  //  return child_range(&Body[0], &Body[0]+BlockStmtBits.NumStmts);
  //}
};

#if 0
// SwitchCase is the base class for CaseStmt and DefaultStmt,
class SwitchCase : public Stmt {
protected:
  // A pointer to the following CaseStmt or DefaultStmt class,
  // used by SwitchStmt.
  SwitchCase *NextSwitchCase;

  SwitchCase(StmtClass SC) : Stmt(SC), NextSwitchCase(0) {}

public:
  const SwitchCase *getNextSwitchCase() const { return NextSwitchCase; }

  SwitchCase *getNextSwitchCase() { return NextSwitchCase; }

  void setNextSwitchCase(SwitchCase *SC) { NextSwitchCase = SC; }

  Stmt *getSubStmt();
  const Stmt *getSubStmt() const {
    return const_cast<SwitchCase*>(this)->getSubStmt();
  }

  SourceLocation getLocStart() const LLVM_READONLY { return SourceLocation(); }
  SourceLocation getLocEnd() const LLVM_READONLY { return SourceLocation(); }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == CaseStmtClass ||
           T->getStmtClass() == DefaultStmtClass;
  }
};

class CaseStmt : public SwitchCase {
  enum { LHS, RHS, SUBSTMT, END_EXPR };
  Stmt* SubExprs[END_EXPR];  // The expression for the RHS is Non-null for
                             // GNU "case 1 ... 4" extension
  SourceLocation CaseLoc;
  SourceLocation EllipsisLoc;
  SourceLocation ColonLoc;
public:
  CaseStmt(Expr *lhs, Expr *rhs, SourceLocation caseLoc,
           SourceLocation ellipsisLoc, SourceLocation colonLoc)
    : SwitchCase(CaseStmtClass) {
    SubExprs[SUBSTMT] = 0;
    SubExprs[LHS] = reinterpret_cast<Stmt*>(lhs);
    SubExprs[RHS] = reinterpret_cast<Stmt*>(rhs);
    CaseLoc = caseLoc;
    EllipsisLoc = ellipsisLoc;
    ColonLoc = colonLoc;
  }

  /// \brief Build an empty switch case statement.
  explicit CaseStmt(EmptyShell Empty) : SwitchCase(CaseStmtClass) { }

  SourceLocation getCaseLoc() const { return CaseLoc; }
  void setCaseLoc(SourceLocation L) { CaseLoc = L; }
  SourceLocation getEllipsisLoc() const { return EllipsisLoc; }
  void setEllipsisLoc(SourceLocation L) { EllipsisLoc = L; }
  SourceLocation getColonLoc() const { return ColonLoc; }
  void setColonLoc(SourceLocation L) { ColonLoc = L; }

  Expr *getLHS() { return reinterpret_cast<Expr*>(SubExprs[LHS]); }
  Expr *getRHS() { return reinterpret_cast<Expr*>(SubExprs[RHS]); }
  Stmt *getSubStmt() { return SubExprs[SUBSTMT]; }

  const Expr *getLHS() const {
    return reinterpret_cast<const Expr*>(SubExprs[LHS]);
  }
  const Expr *getRHS() const {
    return reinterpret_cast<const Expr*>(SubExprs[RHS]);
  }
  const Stmt *getSubStmt() const { return SubExprs[SUBSTMT]; }

  void setSubStmt(Stmt *S) { SubExprs[SUBSTMT] = S; }
  void setLHS(Expr *Val) { SubExprs[LHS] = reinterpret_cast<Stmt*>(Val); }
  void setRHS(Expr *Val) { SubExprs[RHS] = reinterpret_cast<Stmt*>(Val); }

  SourceLocation getLocStart() const LLVM_READONLY { return CaseLoc; }
  SourceLocation getLocEnd() const LLVM_READONLY {
    // Handle deeply nested case statements with iteration instead of recursion.
    const CaseStmt *CS = this;
    while (const CaseStmt *CS2 = dyn_cast<CaseStmt>(CS->getSubStmt()))
      CS = CS2;

    return CS->getSubStmt()->getLocEnd();
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == CaseStmtClass;
  }

  // Iterators
  child_range children() {
    return child_range(&SubExprs[0], &SubExprs[END_EXPR]);
  }
};

class DefaultStmt : public SwitchCase {
  Stmt* SubStmt;
  SourceLocation DefaultLoc;
  SourceLocation ColonLoc;
public:
  DefaultStmt(SourceLocation DL, SourceLocation CL, Stmt *substmt) :
    SwitchCase(DefaultStmtClass), SubStmt(substmt), DefaultLoc(DL),
    ColonLoc(CL) {}

  /// \brief Build an empty default statement.
  explicit DefaultStmt(EmptyShell) : SwitchCase(DefaultStmtClass) { }

  Stmt *getSubStmt() { return SubStmt; }
  const Stmt *getSubStmt() const { return SubStmt; }
  void setSubStmt(Stmt *S) { SubStmt = S; }

  SourceLocation getDefaultLoc() const { return DefaultLoc; }
  void setDefaultLoc(SourceLocation L) { DefaultLoc = L; }
  SourceLocation getColonLoc() const { return ColonLoc; }
  void setColonLoc(SourceLocation L) { ColonLoc = L; }

  SourceLocation getLocStart() const LLVM_READONLY { return DefaultLoc; }
  SourceLocation getLocEnd() const LLVM_READONLY { return SubStmt->getLocEnd();}

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == DefaultStmtClass;
  }

  // Iterators
  child_range children() { return child_range(&SubStmt, &SubStmt+1); }
};


/// LabelStmt - Represents a label, which has a substatement.  For example:
///    foo: return;
///
class LabelStmt : public Stmt {
  LabelDecl *TheDecl;
  Stmt *SubStmt;
  SourceLocation IdentLoc;
public:
  LabelStmt(SourceLocation IL, LabelDecl *D, Stmt *substmt)
    : Stmt(LabelStmtClass), TheDecl(D), SubStmt(substmt), IdentLoc(IL) {
  }

  // \brief Build an empty label statement.
  explicit LabelStmt(EmptyShell Empty) : Stmt(LabelStmtClass, Empty) { }

  SourceLocation getIdentLoc() const { return IdentLoc; }
  LabelDecl *getDecl() const { return TheDecl; }
  void setDecl(LabelDecl *D) { TheDecl = D; }
  const char *getName() const;
  Stmt *getSubStmt() { return SubStmt; }
  const Stmt *getSubStmt() const { return SubStmt; }
  void setIdentLoc(SourceLocation L) { IdentLoc = L; }
  void setSubStmt(Stmt *SS) { SubStmt = SS; }

  SourceLocation getLocStart() const LLVM_READONLY { return IdentLoc; }
  SourceLocation getLocEnd() const LLVM_READONLY { return SubStmt->getLocEnd();}

  child_range children() { return child_range(&SubStmt, &SubStmt+1); }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == LabelStmtClass;
  }
};

/// IfStmt - This represents an if/then/else.
///
class IfStmt : public Stmt {
  enum { VAR, COND, THEN, ELSE, END_EXPR };
  Stmt* SubExprs[END_EXPR];

  SourceLocation IfLoc;
  SourceLocation ElseLoc;

public:
  IfStmt(ASTContext &C, SourceLocation IL, VarDecl *var, Expr *cond,
         Stmt *then, SourceLocation EL = SourceLocation(), Stmt *elsev = 0);

  /// \brief Build an empty if/then/else statement
  explicit IfStmt(EmptyShell Empty) : Stmt(IfStmtClass, Empty) { }

  /// \brief Retrieve the variable declared in this "if" statement, if any.
  ///
  /// In the following example, "x" is the condition variable.
  /// \code
  /// if (int x = foo()) {
  ///   printf("x is %d", x);
  /// }
  /// \endcode
  VarDecl *getConditionVariable() const;
  void setConditionVariable(ASTContext &C, VarDecl *V);

  /// If this IfStmt has a condition variable, return the faux DeclStmt
  /// associated with the creation of that condition variable.
  const DeclStmt *getConditionVariableDeclStmt() const {
    return reinterpret_cast<DeclStmt*>(SubExprs[VAR]);
  }

  const Expr *getCond() const { return reinterpret_cast<Expr*>(SubExprs[COND]);}
  void setCond(Expr *E) { SubExprs[COND] = reinterpret_cast<Stmt *>(E); }
  const Stmt *getThen() const { return SubExprs[THEN]; }
  void setThen(Stmt *S) { SubExprs[THEN] = S; }
  const Stmt *getElse() const { return SubExprs[ELSE]; }
  void setElse(Stmt *S) { SubExprs[ELSE] = S; }

  Expr *getCond() { return reinterpret_cast<Expr*>(SubExprs[COND]); }
  Stmt *getThen() { return SubExprs[THEN]; }
  Stmt *getElse() { return SubExprs[ELSE]; }

  SourceLocation getIfLoc() const { return IfLoc; }
  void setIfLoc(SourceLocation L) { IfLoc = L; }
  SourceLocation getElseLoc() const { return ElseLoc; }
  void setElseLoc(SourceLocation L) { ElseLoc = L; }

  SourceLocation getLocStart() const LLVM_READONLY { return IfLoc; }
  SourceLocation getLocEnd() const LLVM_READONLY {
    if (SubExprs[ELSE])
      return SubExprs[ELSE]->getLocEnd();
    else
      return SubExprs[THEN]->getLocEnd();
  }

  // Iterators over subexpressions.  The iterators will include iterating
  // over the initialization expression referenced by the condition variable.
  child_range children() {
    return child_range(&SubExprs[0], &SubExprs[0]+END_EXPR);
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == IfStmtClass;
  }
};

/// SwitchStmt - This represents a 'switch' stmt.
///
class SwitchStmt : public Stmt {
  enum { VAR, COND, BODY, END_EXPR };
  Stmt* SubExprs[END_EXPR];
  // This points to a linked list of case and default statements.
  SwitchCase *FirstCase;
  SourceLocation SwitchLoc;

  /// If the SwitchStmt is a switch on an enum value, this records whether
  /// all the enum values were covered by CaseStmts.  This value is meant to
  /// be a hint for possible clients.
  unsigned AllEnumCasesCovered : 1;

public:
  SwitchStmt(ASTContext &C, VarDecl *Var, Expr *cond);

  /// \brief Build a empty switch statement.
  explicit SwitchStmt(EmptyShell Empty) : Stmt(SwitchStmtClass, Empty) { }

  /// \brief Retrieve the variable declared in this "switch" statement, if any.
  ///
  /// In the following example, "x" is the condition variable.
  /// \code
  /// switch (int x = foo()) {
  ///   case 0: break;
  ///   // ...
  /// }
  /// \endcode
  VarDecl *getConditionVariable() const;
  void setConditionVariable(ASTContext &C, VarDecl *V);

  /// If this SwitchStmt has a condition variable, return the faux DeclStmt
  /// associated with the creation of that condition variable.
  const DeclStmt *getConditionVariableDeclStmt() const {
    return reinterpret_cast<DeclStmt*>(SubExprs[VAR]);
  }

  const Expr *getCond() const { return reinterpret_cast<Expr*>(SubExprs[COND]);}
  const Stmt *getBody() const { return SubExprs[BODY]; }
  const SwitchCase *getSwitchCaseList() const { return FirstCase; }

  Expr *getCond() { return reinterpret_cast<Expr*>(SubExprs[COND]);}
  void setCond(Expr *E) { SubExprs[COND] = reinterpret_cast<Stmt *>(E); }
  Stmt *getBody() { return SubExprs[BODY]; }
  void setBody(Stmt *S) { SubExprs[BODY] = S; }
  SwitchCase *getSwitchCaseList() { return FirstCase; }

  /// \brief Set the case list for this switch statement.
  ///
  /// The caller is responsible for incrementing the retain counts on
  /// all of the SwitchCase statements in this list.
  void setSwitchCaseList(SwitchCase *SC) { FirstCase = SC; }

  SourceLocation getSwitchLoc() const { return SwitchLoc; }
  void setSwitchLoc(SourceLocation L) { SwitchLoc = L; }

  void setBody(Stmt *S, SourceLocation SL) {
    SubExprs[BODY] = S;
    SwitchLoc = SL;
  }
  void addSwitchCase(SwitchCase *SC) {
    assert(!SC->getNextSwitchCase()
           && "case/default already added to a switch");
    SC->setNextSwitchCase(FirstCase);
    FirstCase = SC;
  }

  /// Set a flag in the SwitchStmt indicating that if the 'switch (X)' is a
  /// switch over an enum value then all cases have been explicitly covered.
  void setAllEnumCasesCovered() {
    AllEnumCasesCovered = 1;
  }

  /// Returns true if the SwitchStmt is a switch of an enum value and all cases
  /// have been explicitly covered.
  bool isAllEnumCasesCovered() const {
    return (bool) AllEnumCasesCovered;
  }

  SourceLocation getLocStart() const LLVM_READONLY { return SwitchLoc; }
  SourceLocation getLocEnd() const LLVM_READONLY {
    return SubExprs[BODY]->getLocEnd();
  }

  // Iterators
  child_range children() {
    return child_range(&SubExprs[0], &SubExprs[0]+END_EXPR);
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == SwitchStmtClass;
  }
};


/// ForStmt - This represents a 'for (init;cond;inc)' stmt.  Note that any of
/// the init/cond/inc parts of the ForStmt will be null if they were not
/// specified in the source.
///
class ForStmt : public Stmt {
  enum { INIT, CONDVAR, COND, INC, BODY, END_EXPR };
  Stmt* SubExprs[END_EXPR]; // SubExprs[INIT] is an expression or declstmt.
  SourceLocation ForLoc;
  SourceLocation LParenLoc, RParenLoc;

public:
  ForStmt(ASTContext &C, Stmt *Init, Expr *Cond, VarDecl *condVar, Expr *Inc,
          Stmt *Body, SourceLocation FL, SourceLocation LP, SourceLocation RP);

  /// \brief Build an empty for statement.
  explicit ForStmt(EmptyShell Empty) : Stmt(ForStmtClass, Empty) { }

  Stmt *getInit() { return SubExprs[INIT]; }

  /// \brief Retrieve the variable declared in this "for" statement, if any.
  ///
  /// In the following example, "y" is the condition variable.
  /// \code
  /// for (int x = random(); int y = mangle(x); ++x) {
  ///   // ...
  /// }
  /// \endcode
  VarDecl *getConditionVariable() const;
  void setConditionVariable(ASTContext &C, VarDecl *V);

  /// If this ForStmt has a condition variable, return the faux DeclStmt
  /// associated with the creation of that condition variable.
  const DeclStmt *getConditionVariableDeclStmt() const {
    return reinterpret_cast<DeclStmt*>(SubExprs[CONDVAR]);
  }

  Expr *getCond() { return reinterpret_cast<Expr*>(SubExprs[COND]); }
  Expr *getInc()  { return reinterpret_cast<Expr*>(SubExprs[INC]); }
  Stmt *getBody() { return SubExprs[BODY]; }

  const Stmt *getInit() const { return SubExprs[INIT]; }
  const Expr *getCond() const { return reinterpret_cast<Expr*>(SubExprs[COND]);}
  const Expr *getInc()  const { return reinterpret_cast<Expr*>(SubExprs[INC]); }
  const Stmt *getBody() const { return SubExprs[BODY]; }

  void setInit(Stmt *S) { SubExprs[INIT] = S; }
  void setCond(Expr *E) { SubExprs[COND] = reinterpret_cast<Stmt*>(E); }
  void setInc(Expr *E) { SubExprs[INC] = reinterpret_cast<Stmt*>(E); }
  void setBody(Stmt *S) { SubExprs[BODY] = S; }

  SourceLocation getForLoc() const { return ForLoc; }
  void setForLoc(SourceLocation L) { ForLoc = L; }
  SourceLocation getLParenLoc() const { return LParenLoc; }
  void setLParenLoc(SourceLocation L) { LParenLoc = L; }
  SourceLocation getRParenLoc() const { return RParenLoc; }
  void setRParenLoc(SourceLocation L) { RParenLoc = L; }

  SourceLocation getLocStart() const LLVM_READONLY { return ForLoc; }
  SourceLocation getLocEnd() const LLVM_READONLY {
    return SubExprs[BODY]->getLocEnd();
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == ForStmtClass;
  }

  // Iterators
  child_range children() {
    return child_range(&SubExprs[0], &SubExprs[0]+END_EXPR);
  }
};

/// GotoStmt - This represents a direct goto.
///
class GotoStmt : public Stmt {
  LabelDecl *Label;
  SourceLocation GotoLoc;
  SourceLocation LabelLoc;
public:
  GotoStmt(LabelDecl *label, SourceLocation GL, SourceLocation LL)
    : Stmt(GotoStmtClass), Label(label), GotoLoc(GL), LabelLoc(LL) {}

  /// \brief Build an empty goto statement.
  explicit GotoStmt(EmptyShell Empty) : Stmt(GotoStmtClass, Empty) { }

  LabelDecl *getLabel() const { return Label; }
  void setLabel(LabelDecl *D) { Label = D; }

  SourceLocation getGotoLoc() const { return GotoLoc; }
  void setGotoLoc(SourceLocation L) { GotoLoc = L; }
  SourceLocation getLabelLoc() const { return LabelLoc; }
  void setLabelLoc(SourceLocation L) { LabelLoc = L; }

  SourceLocation getLocStart() const LLVM_READONLY { return GotoLoc; }
  SourceLocation getLocEnd() const LLVM_READONLY { return LabelLoc; }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == GotoStmtClass;
  }

  // Iterators
  child_range children() { return child_range(); }
};

/// ContinueStmt - This represents a continue.
///
class ContinueStmt : public Stmt {
  SourceLocation ContinueLoc;
public:
  ContinueStmt(SourceLocation CL) : Stmt(ContinueStmtClass), ContinueLoc(CL) {}

  /// \brief Build an empty continue statement.
  explicit ContinueStmt(EmptyShell Empty) : Stmt(ContinueStmtClass, Empty) { }

  SourceLocation getContinueLoc() const { return ContinueLoc; }
  void setContinueLoc(SourceLocation L) { ContinueLoc = L; }

  SourceLocation getLocStart() const LLVM_READONLY { return ContinueLoc; }
  SourceLocation getLocEnd() const LLVM_READONLY { return ContinueLoc; }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == ContinueStmtClass;
  }

  // Iterators
  child_range children() { return child_range(); }
};

/// BreakStmt - This represents a break.
///
class BreakStmt : public Stmt {
  SourceLocation BreakLoc;
public:
  BreakStmt(SourceLocation BL) : Stmt(BreakStmtClass), BreakLoc(BL) {}

  /// \brief Build an empty break statement.
  explicit BreakStmt(EmptyShell Empty) : Stmt(BreakStmtClass, Empty) { }

  SourceLocation getBreakLoc() const { return BreakLoc; }
  void setBreakLoc(SourceLocation L) { BreakLoc = L; }

  SourceLocation getLocStart() const LLVM_READONLY { return BreakLoc; }
  SourceLocation getLocEnd() const LLVM_READONLY { return BreakLoc; }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == BreakStmtClass;
  }

  // Iterators
  child_range children() { return child_range(); }
};


/// ReturnStmt - This represents a return, optionally of an expression:
///   return;
///   return 4;
///
/// Note that GCC allows return with no argument in a function declared to
/// return a value, and it allows returning a value in functions declared to
/// return void.  We explicitly model this in the AST, which means you can't
/// depend on the return type of the function and the presence of an argument.
///
class ReturnStmt : public Stmt {
  Stmt *RetExpr;
  SourceLocation RetLoc;
  const VarDecl *NRVOCandidate;

public:
  ReturnStmt(SourceLocation RL)
    : Stmt(ReturnStmtClass), RetExpr(0), RetLoc(RL), NRVOCandidate(0) { }

  ReturnStmt(SourceLocation RL, Expr *E, const VarDecl *NRVOCandidate)
    : Stmt(ReturnStmtClass), RetExpr((Stmt*) E), RetLoc(RL),
      NRVOCandidate(NRVOCandidate) {}

  /// \brief Build an empty return expression.
  explicit ReturnStmt(EmptyShell Empty) : Stmt(ReturnStmtClass, Empty) { }

  const Expr *getRetValue() const;
  Expr *getRetValue();
  void setRetValue(Expr *E) { RetExpr = reinterpret_cast<Stmt*>(E); }

  SourceLocation getReturnLoc() const { return RetLoc; }
  void setReturnLoc(SourceLocation L) { RetLoc = L; }

  /// \brief Retrieve the variable that might be used for the named return
  /// value optimization.
  ///
  /// The optimization itself can only be performed if the variable is
  /// also marked as an NRVO object.
  const VarDecl *getNRVOCandidate() const { return NRVOCandidate; }
  void setNRVOCandidate(const VarDecl *Var) { NRVOCandidate = Var; }

  SourceLocation getLocStart() const LLVM_READONLY { return RetLoc; }
  SourceLocation getLocEnd() const LLVM_READONLY {
    return RetExpr ? RetExpr->getLocEnd() : RetLoc;
  }

  static bool classof(const Stmt *T) {
    return T->getStmtClass() == ReturnStmtClass;
  }

  // Iterators
  child_range children() {
    if (RetExpr) return child_range(&RetExpr, &RetExpr+1);
    return child_range();
  }
};

#endif

}  // end namespace gong

#endif
