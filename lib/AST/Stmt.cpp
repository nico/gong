//===--- Stmt.cpp - Statement AST Node Implementation ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Stmt class and statement subclasses.
//
//===----------------------------------------------------------------------===//

#include "gong/AST/Stmt.h"

#include "gong/AST/ASTContext.h"
using namespace gong;

#if 0
#include "gong/AST/ASTDiagnostic.h"
#include "gong/AST/ExprCXX.h"
#include "gong/AST/ExprObjC.h"
#include "gong/AST/StmtCXX.h"
#include "gong/AST/StmtObjC.h"
#include "gong/AST/Type.h"
#include "gong/Basic/TargetInfo.h"
#include "gong/Lex/Token.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/raw_ostream.h"

static struct StmtClassNameTable {
  const char *Name;
  unsigned Counter;
  unsigned Size;
} StmtClassInfo[Stmt::lastStmtConstant+1];

static StmtClassNameTable &getStmtInfoTableEntry(Stmt::StmtClass E) {
  static bool Initialized = false;
  if (Initialized)
    return StmtClassInfo[E];

  // Intialize the table on the first use.
  Initialized = true;
#define ABSTRACT_STMT(STMT)
#define STMT(CLASS, PARENT) \
  StmtClassInfo[(unsigned)Stmt::CLASS##Class].Name = #CLASS;    \
  StmtClassInfo[(unsigned)Stmt::CLASS##Class].Size = sizeof(CLASS);
#include "gong/AST/StmtNodes.inc"

  return StmtClassInfo[E];
}
#endif

void *Stmt::operator new(size_t bytes, ASTContext& C,
                         unsigned alignment) throw() {
  return ::operator new(bytes, C, alignment);
}

void *Stmt::operator new(size_t bytes, ASTContext* C,
                         unsigned alignment) throw() {
  return ::operator new(bytes, *C, alignment);
}

#if 0
const char *Stmt::getStmtClassName() const {
  return getStmtInfoTableEntry((StmtClass) StmtBits.sClass).Name;
}

void Stmt::PrintStats() {
  // Ensure the table is primed.
  getStmtInfoTableEntry(Stmt::NullStmtClass);

  unsigned sum = 0;
  llvm::errs() << "\n*** Stmt/Expr Stats:\n";
  for (int i = 0; i != Stmt::lastStmtConstant+1; i++) {
    if (StmtClassInfo[i].Name == 0) continue;
    sum += StmtClassInfo[i].Counter;
  }
  llvm::errs() << "  " << sum << " stmts/exprs total.\n";
  sum = 0;
  for (int i = 0; i != Stmt::lastStmtConstant+1; i++) {
    if (StmtClassInfo[i].Name == 0) continue;
    if (StmtClassInfo[i].Counter == 0) continue;
    llvm::errs() << "    " << StmtClassInfo[i].Counter << " "
                 << StmtClassInfo[i].Name << ", " << StmtClassInfo[i].Size
                 << " each (" << StmtClassInfo[i].Counter*StmtClassInfo[i].Size
                 << " bytes)\n";
    sum += StmtClassInfo[i].Counter*StmtClassInfo[i].Size;
  }

  llvm::errs() << "Total bytes = " << sum << "\n";
}

void Stmt::addStmtClass(StmtClass s) {
  ++getStmtInfoTableEntry(s).Counter;
}

bool Stmt::StatisticsEnabled = false;
void Stmt::EnableStatistics() {
  StatisticsEnabled = true;
}

Stmt *Stmt::IgnoreImplicit() {
  Stmt *s = this;

  if (ExprWithCleanups *ewc = dyn_cast<ExprWithCleanups>(s))
    s = ewc->getSubExpr();

  while (ImplicitCastExpr *ice = dyn_cast<ImplicitCastExpr>(s))
    s = ice->getSubExpr();

  return s;
}

/// \brief Strip off all label-like statements.
///
/// This will strip off label statements, case statements, attributed
/// statements and default statements recursively.
const Stmt *Stmt::stripLabelLikeStatements() const {
  const Stmt *S = this;
  while (true) {
    if (const LabelStmt *LS = dyn_cast<LabelStmt>(S))
      S = LS->getSubStmt();
    else if (const SwitchCase *SC = dyn_cast<SwitchCase>(S))
      S = SC->getSubStmt();
    else if (const AttributedStmt *AS = dyn_cast<AttributedStmt>(S))
      S = AS->getSubStmt();
    else
      return S;
  }
}

namespace {
  struct good {};
  struct bad {};

  // These silly little functions have to be static inline to suppress
  // unused warnings, and they have to be defined to suppress other
  // warnings.
  static inline good is_good(good) { return good(); }

  typedef Stmt::child_range children_t();
  template <class T> good implements_children(children_t T::*) {
    return good();
  }
  static inline bad implements_children(children_t Stmt::*) {
    return bad();
  }

  typedef SourceLocation getLocStart_t() const;
  template <class T> good implements_getLocStart(getLocStart_t T::*) {
    return good();
  }
  static inline bad implements_getLocStart(getLocStart_t Stmt::*) {
    return bad();
  }

  typedef SourceLocation getLocEnd_t() const;
  template <class T> good implements_getLocEnd(getLocEnd_t T::*) {
    return good();
  }
  static inline bad implements_getLocEnd(getLocEnd_t Stmt::*) {
    return bad();
  }

#define ASSERT_IMPLEMENTS_children(type) \
  (void) sizeof(is_good(implements_children(&type::children)))
#define ASSERT_IMPLEMENTS_getLocStart(type) \
  (void) sizeof(is_good(implements_getLocStart(&type::getLocStart)))
#define ASSERT_IMPLEMENTS_getLocEnd(type) \
  (void) sizeof(is_good(implements_getLocEnd(&type::getLocEnd)))
}

/// Check whether the various Stmt classes implement their member
/// functions.
static inline void check_implementations() {
#define ABSTRACT_STMT(type)
#define STMT(type, base) \
  ASSERT_IMPLEMENTS_children(type); \
  ASSERT_IMPLEMENTS_getLocStart(type); \
  ASSERT_IMPLEMENTS_getLocEnd(type);
#include "gong/AST/StmtNodes.inc"
}

Stmt::child_range Stmt::children() {
  switch (getStmtClass()) {
  case Stmt::NoStmtClass: llvm_unreachable("statement without class");
#define ABSTRACT_STMT(type)
#define STMT(type, base) \
  case Stmt::type##Class: \
    return static_cast<type*>(this)->children();
#include "gong/AST/StmtNodes.inc"
  }
  llvm_unreachable("unknown statement kind!");
}

// Amusing macro metaprogramming hack: check whether a class provides
// a more specific implementation of getSourceRange.
//
// See also Expr.cpp:getExprLoc().
namespace {
  /// This implementation is used when a class provides a custom
  /// implementation of getSourceRange.
  template <class S, class T>
  SourceRange getSourceRangeImpl(const Stmt *stmt,
                                 SourceRange (T::*v)() const) {
    return static_cast<const S*>(stmt)->getSourceRange();
  }

  /// This implementation is used when a class doesn't provide a custom
  /// implementation of getSourceRange.  Overload resolution should pick it over
  /// the implementation above because it's more specialized according to
  /// function template partial ordering.
  template <class S>
  SourceRange getSourceRangeImpl(const Stmt *stmt,
                                 SourceRange (Stmt::*v)() const) {
    return SourceRange(static_cast<const S*>(stmt)->getLocStart(),
                       static_cast<const S*>(stmt)->getLocEnd());
  }
}

SourceRange Stmt::getSourceRange() const {
  switch (getStmtClass()) {
  case Stmt::NoStmtClass: llvm_unreachable("statement without class");
#define ABSTRACT_STMT(type)
#define STMT(type, base) \
  case Stmt::type##Class: \
    return getSourceRangeImpl<type>(this, &type::getSourceRange);
#include "gong/AST/StmtNodes.inc"
  }
  llvm_unreachable("unknown statement kind!");
}

SourceLocation Stmt::getLocStart() const {
//  llvm::errs() << "getLocStart() for " << getStmtClassName() << "\n";
  switch (getStmtClass()) {
  case Stmt::NoStmtClass: llvm_unreachable("statement without class");
#define ABSTRACT_STMT(type)
#define STMT(type, base) \
  case Stmt::type##Class: \
    return static_cast<const type*>(this)->getLocStart();
#include "gong/AST/StmtNodes.inc"
  }
  llvm_unreachable("unknown statement kind");
}

SourceLocation Stmt::getLocEnd() const {
  switch (getStmtClass()) {
  case Stmt::NoStmtClass: llvm_unreachable("statement without class");
#define ABSTRACT_STMT(type)
#define STMT(type, base) \
  case Stmt::type##Class: \
    return static_cast<const type*>(this)->getLocEnd();
#include "gong/AST/StmtNodes.inc"
  }
  llvm_unreachable("unknown statement kind");
}
#endif

BlockStmt::BlockStmt(ASTContext &C, ArrayRef<Stmt *> Stmts,
                     SourceLocation LB, SourceLocation RB)
  : Stmt(BlockStmtClass), LBracLoc(LB), RBracLoc(RB) {
  BlockStmtBits.NumStmts = Stmts.size();
  assert(BlockStmtBits.NumStmts == Stmts.size() &&
         "NumStmts doesn't fit in bits of BlockStmtBits.NumStmts!");

  if (Stmts.size() == 0) {
    Body = 0;
    return;
  }

  Body = new (C) Stmt*[Stmts.size()];
  std::copy(Stmts.begin(), Stmts.end(), Body);
}

#if 0
CompoundStmt::CompoundStmt(ASTContext &C, Stmt **StmtStart, unsigned NumStmts,
                           SourceLocation LB, SourceLocation RB)
  : Stmt(CompoundStmtClass), LBracLoc(LB), RBracLoc(RB) {
  CompoundStmtBits.NumStmts = NumStmts;
  assert(CompoundStmtBits.NumStmts == NumStmts &&
         "NumStmts doesn't fit in bits of CompoundStmtBits.NumStmts!");

  if (NumStmts == 0) {
    Body = 0;
    return;
  }

  Body = new (C) Stmt*[NumStmts];
  memcpy(Body, StmtStart, NumStmts * sizeof(*Body));
}

void CompoundStmt::setStmts(ASTContext &C, Stmt **Stmts, unsigned NumStmts) {
  if (this->Body)
    C.Deallocate(Body);
  this->CompoundStmtBits.NumStmts = NumStmts;

  Body = new (C) Stmt*[NumStmts];
  memcpy(Body, Stmts, sizeof(Stmt *) * NumStmts);
}

const char *LabelStmt::getName() const {
  return getDecl()->getIdentifier()->getNameStart();
}

bool Stmt::hasImplicitControlFlow() const {
  switch (StmtBits.sClass) {
    default:
      return false;

    case CallExprClass:
    case ConditionalOperatorClass:
    case ChooseExprClass:
    case StmtExprClass:
    case DeclStmtClass:
      return true;

    case Stmt::BinaryOperatorClass: {
      const BinaryOperator* B = cast<BinaryOperator>(this);
      if (B->isLogicalOp() || B->getOpcode() == BO_Comma)
        return true;
      else
        return false;
    }
  }
}

CXXForRangeStmt::CXXForRangeStmt(DeclStmt *Range, DeclStmt *BeginEndStmt,
                                 Expr *Cond, Expr *Inc, DeclStmt *LoopVar,
                                 Stmt *Body, SourceLocation FL,
                                 SourceLocation CL, SourceLocation RPL)
  : Stmt(CXXForRangeStmtClass), ForLoc(FL), ColonLoc(CL), RParenLoc(RPL) {
  SubExprs[RANGE] = Range;
  SubExprs[BEGINEND] = BeginEndStmt;
  SubExprs[COND] = reinterpret_cast<Stmt*>(Cond);
  SubExprs[INC] = reinterpret_cast<Stmt*>(Inc);
  SubExprs[LOOPVAR] = LoopVar;
  SubExprs[BODY] = Body;
}

Expr *CXXForRangeStmt::getRangeInit() {
  DeclStmt *RangeStmt = getRangeStmt();
  VarDecl *RangeDecl = dyn_cast_or_null<VarDecl>(RangeStmt->getSingleDecl());
  assert(RangeDecl &&& "for-range should have a single var decl");
  return RangeDecl->getInit();
}

const Expr *CXXForRangeStmt::getRangeInit() const {
  return const_cast<CXXForRangeStmt*>(this)->getRangeInit();
}

VarDecl *CXXForRangeStmt::getLoopVariable() {
  Decl *LV = cast<DeclStmt>(getLoopVarStmt())->getSingleDecl();
  assert(LV && "No loop variable in CXXForRangeStmt");
  return cast<VarDecl>(LV);
}

const VarDecl *CXXForRangeStmt::getLoopVariable() const {
  return const_cast<CXXForRangeStmt*>(this)->getLoopVariable();
}

IfStmt::IfStmt(ASTContext &C, SourceLocation IL, VarDecl *var, Expr *cond,
               Stmt *then, SourceLocation EL, Stmt *elsev)
  : Stmt(IfStmtClass), IfLoc(IL), ElseLoc(EL)
{
  setConditionVariable(C, var);
  SubExprs[COND] = reinterpret_cast<Stmt*>(cond);
  SubExprs[THEN] = then;
  SubExprs[ELSE] = elsev;
}

VarDecl *IfStmt::getConditionVariable() const {
  if (!SubExprs[VAR])
    return 0;

  DeclStmt *DS = cast<DeclStmt>(SubExprs[VAR]);
  return cast<VarDecl>(DS->getSingleDecl());
}

void IfStmt::setConditionVariable(ASTContext &C, VarDecl *V) {
  if (!V) {
    SubExprs[VAR] = 0;
    return;
  }

  SourceRange VarRange = V->getSourceRange();
  SubExprs[VAR] = new (C) DeclStmt(DeclGroupRef(V), VarRange.getBegin(),
                                   VarRange.getEnd());
}

ForStmt::ForStmt(ASTContext &C, Stmt *Init, Expr *Cond, VarDecl *condVar,
                 Expr *Inc, Stmt *Body, SourceLocation FL, SourceLocation LP,
                 SourceLocation RP)
  : Stmt(ForStmtClass), ForLoc(FL), LParenLoc(LP), RParenLoc(RP)
{
  SubExprs[INIT] = Init;
  setConditionVariable(C, condVar);
  SubExprs[COND] = reinterpret_cast<Stmt*>(Cond);
  SubExprs[INC] = reinterpret_cast<Stmt*>(Inc);
  SubExprs[BODY] = Body;
}

VarDecl *ForStmt::getConditionVariable() const {
  if (!SubExprs[CONDVAR])
    return 0;

  DeclStmt *DS = cast<DeclStmt>(SubExprs[CONDVAR]);
  return cast<VarDecl>(DS->getSingleDecl());
}

void ForStmt::setConditionVariable(ASTContext &C, VarDecl *V) {
  if (!V) {
    SubExprs[CONDVAR] = 0;
    return;
  }

  SourceRange VarRange = V->getSourceRange();
  SubExprs[CONDVAR] = new (C) DeclStmt(DeclGroupRef(V), VarRange.getBegin(),
                                       VarRange.getEnd());
}

SwitchStmt::SwitchStmt(ASTContext &C, VarDecl *Var, Expr *cond)
  : Stmt(SwitchStmtClass), FirstCase(0), AllEnumCasesCovered(0)
{
  setConditionVariable(C, Var);
  SubExprs[COND] = reinterpret_cast<Stmt*>(cond);
  SubExprs[BODY] = NULL;
}

VarDecl *SwitchStmt::getConditionVariable() const {
  if (!SubExprs[VAR])
    return 0;

  DeclStmt *DS = cast<DeclStmt>(SubExprs[VAR]);
  return cast<VarDecl>(DS->getSingleDecl());
}

void SwitchStmt::setConditionVariable(ASTContext &C, VarDecl *V) {
  if (!V) {
    SubExprs[VAR] = 0;
    return;
  }

  SourceRange VarRange = V->getSourceRange();
  SubExprs[VAR] = new (C) DeclStmt(DeclGroupRef(V), VarRange.getBegin(),
                                   VarRange.getEnd());
}

Stmt *SwitchCase::getSubStmt() {
  if (isa<CaseStmt>(this))
    return cast<CaseStmt>(this)->getSubStmt();
  return cast<DefaultStmt>(this)->getSubStmt();
}

// ReturnStmt
const Expr* ReturnStmt::getRetValue() const {
  return cast_or_null<Expr>(RetExpr);
}
Expr* ReturnStmt::getRetValue() {
  return cast_or_null<Expr>(RetExpr);
}
#endif
