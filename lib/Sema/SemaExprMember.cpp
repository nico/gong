//===--- SemaExprMember.cpp - Semantic Analysis for Expressions -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements semantic analysis member access expressions.
//
//===----------------------------------------------------------------------===//

#if 0
#include "gong/Sema/SemaInternal.h"
#include "gong/AST/DeclCXX.h"
#include "gong/AST/DeclTemplate.h"
#include "gong/AST/ExprCXX.h"
#include "gong/Lex/Preprocessor.h"
#include "gong/Sema/Lookup.h"
#include "gong/Sema/Scope.h"
#include "gong/Sema/ScopeInfo.h"

using namespace gong;
using namespace sema;

typedef llvm::SmallPtrSet<const CXXRecordDecl*, 4> BaseSet;
static bool BaseIsNotInSet(const CXXRecordDecl *Base, void *BasesPtr) {
  const BaseSet &Bases = *reinterpret_cast<const BaseSet*>(BasesPtr);
  return !Bases.count(Base->getCanonicalDecl());
}

/// Determines if the given class is provably not derived from all of
/// the prospective base classes.
static bool isProvablyNotDerivedFrom(Sema &SemaRef, CXXRecordDecl *Record,
                                     const BaseSet &Bases) {
  void *BasesPtr = const_cast<void*>(reinterpret_cast<const void*>(&Bases));
  return BaseIsNotInSet(Record, BasesPtr) &&
         Record->forallBases(BaseIsNotInSet, BasesPtr);
}

/// Diagnose a reference to a field with no object available.
static void diagnoseInstanceReference(Sema &SemaRef,
                                      const CXXScopeSpec &SS,
                                      NamedDecl *Rep,
                                      const DeclarationNameInfo &nameInfo) {
  SourceLocation Loc = nameInfo.getLoc();
  SourceRange Range(Loc);
  if (SS.isSet()) Range.setBegin(SS.getRange().getBegin());

  DeclContext *FunctionLevelDC = SemaRef.getFunctionLevelDeclContext();
  CXXMethodDecl *Method = dyn_cast<CXXMethodDecl>(FunctionLevelDC);
  CXXRecordDecl *ContextClass = Method ? Method->getParent() : 0;
  CXXRecordDecl *RepClass = dyn_cast<CXXRecordDecl>(Rep->getDeclContext());

  bool InStaticMethod = Method && Method->isStatic();
  bool IsField = isa<FieldDecl>(Rep) || isa<IndirectFieldDecl>(Rep);

  if (IsField && InStaticMethod)
    // "invalid use of member 'x' in static member function"
    SemaRef.Diag(Loc, diag::err_invalid_member_use_in_static_method)
        << Range << nameInfo.getName();
  else if (ContextClass && RepClass && SS.isEmpty() && !InStaticMethod &&
           !RepClass->Equals(ContextClass) && RepClass->Encloses(ContextClass))
    // Unqualified lookup in a non-static member function found a member of an
    // enclosing class.
    SemaRef.Diag(Loc, diag::err_nested_non_static_member_use)
      << IsField << RepClass << nameInfo.getName() << ContextClass << Range;
  else if (IsField)
    SemaRef.Diag(Loc, diag::err_invalid_non_static_member_use)
      << nameInfo.getName() << Range;
  else
    SemaRef.Diag(Loc, diag::err_member_call_without_object)
      << Range;
}

/// We know that the given qualified member reference points only to
/// declarations which do not belong to the static type of the base
/// expression.  Diagnose the problem.
static void DiagnoseQualifiedMemberReference(Sema &SemaRef,
                                             Expr *BaseExpr,
                                             QualType BaseType,
                                             const CXXScopeSpec &SS,
                                             NamedDecl *rep,
                                       const DeclarationNameInfo &nameInfo) {
  assert(BaseType && "member access is always explicit in go");
  SemaRef.Diag(nameInfo.getLoc(), diag::err_qualified_member_of_unrelated)
    << SS.getRange() << rep << BaseType;
}

// Check whether the declarations we found through a nested-name
// specifier in a member expression are actually members of the base
// type.  The restriction here is:
//
//   C++ [expr.ref]p2:
//     ... In these cases, the id-expression shall name a
//     member of the class or of one of its base classes.
//
// So it's perfectly legitimate for the nested-name specifier to name
// an unrelated class, and for us to find an overload set including
// decls from classes which are not superclasses, as long as the decl
// we actually pick through overload resolution is from a superclass.
bool Sema::CheckQualifiedMemberReference(Expr *BaseExpr,
                                         QualType BaseType,
                                         const CXXScopeSpec &SS,
                                         const LookupResult &R) {
  CXXRecordDecl *BaseRecord =
    cast_or_null<CXXRecordDecl>(computeDeclContext(BaseType));
  if (!BaseRecord) {
    // We can't check this yet because the base type is still
    // dependent.
    assert(BaseType->isDependentType());
    return false;
  }

  for (LookupResult::iterator I = R.begin(), E = R.end(); I != E; ++I) {
    // Note that we use the DC of the decl, not the underlying decl.
    DeclContext *DC = (*I)->getDeclContext();
    while (DC->isTransparentContext())
      DC = DC->getParent();

    if (!DC->isRecord())
      continue;

    CXXRecordDecl *MemberRecord = cast<CXXRecordDecl>(DC)->getCanonicalDecl();
    if (BaseRecord->getCanonicalDecl() == MemberRecord ||
        !BaseRecord->isProvablyNotDerivedFrom(MemberRecord))
      return false;
  }

  DiagnoseQualifiedMemberReference(*this, BaseExpr, BaseType, SS,
                                   R.getRepresentativeDecl(),
                                   R.getLookupNameInfo());
  return true;
}

namespace {

// Callback to only accept typo corrections that are either a ValueDecl or a
// FunctionTemplateDecl.
class RecordMemberExprValidatorCCC : public CorrectionCandidateCallback {
 public:
  virtual bool ValidateCandidate(const TypoCorrection &candidate) {
    NamedDecl *ND = candidate.getCorrectionDecl();
    return ND && (isa<ValueDecl>(ND) || isa<FunctionTemplateDecl>(ND));
  }
};

}

static bool
LookupMemberExprInRecord(Sema &SemaRef, LookupResult &R, 
                         SourceRange BaseRange, const RecordType *RTy,
                         SourceLocation OpLoc, CXXScopeSpec &SS,
                         bool HasTemplateArgs) {
  RecordDecl *RDecl = RTy->getDecl();
  if (!SemaRef.isThisOutsideMemberFunctionBody(QualType(RTy, 0)) &&
      SemaRef.RequireCompleteType(OpLoc, QualType(RTy, 0),
                                  diag::err_typecheck_incomplete_tag,
                                  BaseRange))
    return true;

  if (HasTemplateArgs) {
    // LookupTemplateName doesn't expect these both to exist simultaneously.
    QualType ObjectType = SS.isSet() ? QualType() : QualType(RTy, 0);

    bool MOUS;
    SemaRef.LookupTemplateName(R, 0, SS, ObjectType, false, MOUS);
    return false;
  }

  DeclContext *DC = RDecl;
  if (SS.isSet()) {
    // If the member name was a qualified-id, look into the
    // nested-name-specifier.
    DC = SemaRef.computeDeclContext(SS, false);

    if (SemaRef.RequireCompleteDeclContext(SS, DC)) {
      SemaRef.Diag(SS.getRange().getEnd(), diag::err_typecheck_incomplete_tag)
        << SS.getRange() << DC;
      return true;
    }

    assert(DC && "Cannot handle non-computable dependent contexts in lookup");

    if (!isa<TypeDecl>(DC)) {
      SemaRef.Diag(R.getNameLoc(), diag::err_qualified_member_nonclass)
        << DC << SS.getRange();
      return true;
    }
  }

  // The record definition is complete, now look up the member.
  SemaRef.LookupQualifiedName(R, DC);

  if (!R.empty())
    return false;

  // We didn't find anything with the given name, so try to correct
  // for typos.
  DeclarationName Name = R.getLookupName();
  RecordMemberExprValidatorCCC Validator;
  TypoCorrection Corrected = SemaRef.CorrectTypo(R.getLookupNameInfo(),
                                                 R.getLookupKind(), NULL,
                                                 &SS, Validator, DC);
  R.clear();
  if (NamedDecl *ND = Corrected.getCorrectionDecl()) {
    std::string CorrectedStr(
        Corrected.getAsString(SemaRef.getLangOpts()));
    std::string CorrectedQuotedStr(
        Corrected.getQuoted(SemaRef.getLangOpts()));
    R.setLookupName(Corrected.getCorrection());
    R.addDecl(ND);
    SemaRef.Diag(R.getNameLoc(), diag::err_no_member_suggest)
      << Name << DC << CorrectedQuotedStr << SS.getRange()
      << FixItHint::CreateReplacement(Corrected.getCorrectionRange(),
                                      CorrectedStr);
    SemaRef.Diag(ND->getLocation(), diag::note_previous_decl)
      << ND->getDeclName();
  }

  return false;
}

ExprResult
Sema::BuildMemberReferenceExpr(Expr *Base, QualType BaseType,
                               SourceLocation OpLoc, bool IsArrow,
                               CXXScopeSpec &SS,
                               SourceLocation TemplateKWLoc,
                               NamedDecl *FirstQualifierInScope,
                               const DeclarationNameInfo &NameInfo,
                               const TemplateArgumentListInfo *TemplateArgs) {
  if (BaseType->isDependentType() ||
      (SS.isSet() && isDependentScopeSpecifier(SS)))
    return ActOnDependentMemberExpr(Base, BaseType,
                                    IsArrow, OpLoc,
                                    SS, TemplateKWLoc, FirstQualifierInScope,
                                    NameInfo, TemplateArgs);

  LookupResult R(*this, NameInfo, LookupMemberName);

  // Explicit member accesses.
  ExprResult BaseResult = Owned(Base);
  ExprResult Result =
    LookupMemberExpr(R, BaseResult, IsArrow, OpLoc,
                     SS, /*ObjCImpDecl*/ 0, TemplateArgs != 0);

  if (BaseResult.isInvalid())
    return ExprError();
  Base = BaseResult.take();

  if (Result.isInvalid()) {
    Owned(Base);
    return ExprError();
  }

  if (Result.get())
    return Result;

  // LookupMemberExpr can modify Base, and thus change BaseType
  BaseType = Base->getType();

  return BuildMemberReferenceExpr(Base, BaseType,
                                  OpLoc, IsArrow, SS, TemplateKWLoc,
                                  FirstQualifierInScope, R, TemplateArgs);
}

static ExprResult
BuildFieldReferenceExpr(Sema &S, Expr *BaseExpr, bool IsArrow,
                        const CXXScopeSpec &SS, FieldDecl *Field,
                        DeclAccessPair FoundDecl,
                        const DeclarationNameInfo &MemberNameInfo);

ExprResult
Sema::BuildAnonymousStructUnionMemberReference(const CXXScopeSpec &SS,
                                               SourceLocation loc,
                                               IndirectFieldDecl *indirectField,
                                               Expr *baseObjectExpr,
                                               SourceLocation opLoc) {
  // First, build the expression that refers to the base object.
  
  bool baseObjectIsPointer = false;
  Qualifiers baseQuals;
  
  // Case 1:  the base of the indirect field is not a field.
  VarDecl *baseVariable = indirectField->getVarDecl();
  CXXScopeSpec EmptySS;
  if (baseVariable) {
    assert(baseVariable->getType()->isRecordType());
    
    // In principle we could have a member access expression that
    // accesses an anonymous struct/union that's a static member of
    // the base object's class.  However, under the current standard,
    // static data members cannot be anonymous structs or unions.
    // Supporting this is as easy as building a MemberExpr here.
    assert(!baseObjectExpr && "anonymous struct/union is static data member?");
    
    DeclarationNameInfo baseNameInfo(DeclarationName(), loc);
    
    ExprResult result 
      = BuildDeclarationNameExpr(EmptySS, baseNameInfo, baseVariable);
    if (result.isInvalid()) return ExprError();
    
    baseObjectExpr = result.take();    
    baseObjectIsPointer = false;
    baseQuals = baseObjectExpr->getType().getQualifiers();
    
    // Case 2: the base of the indirect field is a field and the user
    // wrote a member expression.
  } else if (baseObjectExpr) {
    // The caller provided the base object expression. Determine
    // whether its a pointer and whether it adds any qualifiers to the
    // anonymous struct/union fields we're looking into.
    QualType objectType = baseObjectExpr->getType();
    
    if (const PointerType *ptr = objectType->getAs<PointerType>()) {
      baseObjectIsPointer = true;
      objectType = ptr->getPointeeType();
    } else {
      baseObjectIsPointer = false;
    }
    baseQuals = objectType.getQualifiers();
    
    // Case 3: the base of the indirect field is a field and we should
    // build an implicit member access.
  } else {
    // We've found a member of an anonymous struct/union that is
    // inside a non-anonymous struct/union, so in a well-formed
    // program our base object expression is "this".
    QualType ThisTy = getCurrentThisType();
    if (ThisTy.isNull()) {
      Diag(loc, diag::err_invalid_member_use_in_static_method)
        << indirectField->getDeclName();
      return ExprError();
    }
    
    // Our base object expression is "this".
    CheckCXXThisCapture(loc);
    baseObjectExpr 
      = new (Context) CXXThisExpr(loc, ThisTy, /*isImplicit=*/ true);
    baseObjectIsPointer = true;
    baseQuals = ThisTy->castAs<PointerType>()->getPointeeType().getQualifiers();
  }
  
  // Build the implicit member references to the field of the
  // anonymous struct/union.
  Expr *result = baseObjectExpr;
  IndirectFieldDecl::chain_iterator
  FI = indirectField->chain_begin(), FEnd = indirectField->chain_end();
  
  // Build the first member access in the chain with full information.
  if (!baseVariable) {
    FieldDecl *field = cast<FieldDecl>(*FI);
    
    // FIXME: use the real found-decl info!
    DeclAccessPair foundDecl = DeclAccessPair::make(field, field->getAccess());
    
    // Make a nameInfo that properly uses the anonymous name.
    DeclarationNameInfo memberNameInfo(field->getDeclName(), loc);
    
    result = BuildFieldReferenceExpr(*this, result, baseObjectIsPointer,
                                     EmptySS, field, foundDecl,
                                     memberNameInfo).take();
    baseObjectIsPointer = false;
    
    // FIXME: check qualified member access
  }
  
  // In all cases, we should now skip the first declaration in the chain.
  ++FI;
  
  while (FI != FEnd) {
    FieldDecl *field = cast<FieldDecl>(*FI++);
    
    // FIXME: these are somewhat meaningless
    DeclarationNameInfo memberNameInfo(field->getDeclName(), loc);
    DeclAccessPair foundDecl = DeclAccessPair::make(field, field->getAccess());
    
    result = BuildFieldReferenceExpr(*this, result, /*isarrow*/ false,
                                     (FI == FEnd? SS : EmptySS), field, 
                                     foundDecl, memberNameInfo).take();
  }
  
  return Owned(result);
}

/// \brief Build a MemberExpr AST node.
static MemberExpr *BuildMemberExpr(Sema &SemaRef,
                                   ASTContext &C, Expr *Base, bool isArrow,
                                   const CXXScopeSpec &SS,
                                   SourceLocation TemplateKWLoc,
                                   ValueDecl *Member,
                                   DeclAccessPair FoundDecl,
                                   const DeclarationNameInfo &MemberNameInfo,
                                   QualType Ty,
                                   ExprValueKind VK, ExprObjectKind OK,
                                   const TemplateArgumentListInfo *TemplateArgs = 0) {
  assert((!isArrow || Base->isRValue()) && "-> base must be a pointer rvalue");
  MemberExpr *E =
      MemberExpr::Create(C, Base, isArrow, SS.getWithLocInContext(C),
                         TemplateKWLoc, Member, FoundDecl, MemberNameInfo,
                         TemplateArgs, Ty, VK, OK);
  SemaRef.MarkMemberReferenced(E);
  return E;
}

ExprResult
Sema::BuildMemberReferenceExpr(Expr *BaseExpr, QualType BaseExprType,
                               SourceLocation OpLoc, bool IsArrow,
                               const CXXScopeSpec &SS,
                               SourceLocation TemplateKWLoc,
                               NamedDecl *FirstQualifierInScope,
                               LookupResult &R,
                               const TemplateArgumentListInfo *TemplateArgs,
                               bool SuppressQualifierCheck,
                               ActOnMemberAccessExtraArgs *ExtraArgs) {
  QualType BaseType = BaseExprType;
  if (IsArrow) {
    assert(BaseType->isPointerType());
    BaseType = BaseType->castAs<PointerType>()->getPointeeType();
  }
  R.setBaseObjectType(BaseType);

  const DeclarationNameInfo &MemberNameInfo = R.getLookupNameInfo();
  DeclarationName MemberName = MemberNameInfo.getName();
  SourceLocation MemberLoc = MemberNameInfo.getLoc();

  if (R.isAmbiguous())
    return ExprError();

  if (R.empty()) {
    // Rederive where we looked up.
    DeclContext *DC = (SS.isSet()
                       ? computeDeclContext(SS, false)
                       : BaseType->getAs<RecordType>()->getDecl());

    if (ExtraArgs) {
      ExprResult RetryExpr;
      if (!IsArrow && BaseExpr) {
        SFINAETrap Trap(*this, true);
        ParsedType ObjectType;
        bool MayBePseudoDestructor = false;
        RetryExpr = ActOnStartCXXMemberReference(getCurScope(), BaseExpr,
                                                 OpLoc, tok::arrow, ObjectType,
                                                 MayBePseudoDestructor);
        if (RetryExpr.isUsable() && !Trap.hasErrorOccurred()) {
          CXXScopeSpec TempSS(SS);
          RetryExpr = ActOnMemberAccessExpr(
              ExtraArgs->S, RetryExpr.get(), OpLoc, tok::arrow, TempSS,
              TemplateKWLoc, ExtraArgs->Id, ExtraArgs->ObjCImpDecl,
              ExtraArgs->HasTrailingLParen);
        }
        if (Trap.hasErrorOccurred())
          RetryExpr = ExprError();
      }
      if (RetryExpr.isUsable()) {
        Diag(OpLoc, diag::err_no_member_overloaded_arrow)
          << MemberName << DC << FixItHint::CreateReplacement(OpLoc, "->");
        return RetryExpr;
      }
    }

    Diag(R.getNameLoc(), diag::err_no_member)
      << MemberName << DC
      << (BaseExpr ? BaseExpr->getSourceRange() : SourceRange());
    return ExprError();
  }

  // Diagnose lookups that find only declarations from a non-base
  // type.  This is possible for either qualified lookups (which may
  // have been qualified with an unrelated type) or implicit member
  // expressions (which were found with unqualified lookup and thus
  // may have come from an enclosing scope).  Note that it's okay for
  // lookup to find declarations from a non-base type as long as those
  // aren't the ones picked by overload resolution.
  if ((SS.isSet() || !BaseExpr ||
       (isa<CXXThisExpr>(BaseExpr) &&
        cast<CXXThisExpr>(BaseExpr)->isImplicit())) &&
      !SuppressQualifierCheck &&
      CheckQualifiedMemberReference(BaseExpr, BaseType, SS, R))
    return ExprError();
  
  // Construct an unresolved result if we in fact got an unresolved
  // result.
  if (R.isOverloadedResult() || R.isUnresolvableResult()) {
    // Suppress any lookup-related diagnostics; we'll do these when we
    // pick a member.
    R.suppressDiagnostics();

    UnresolvedMemberExpr *MemExpr
      = UnresolvedMemberExpr::Create(Context, R.isUnresolvableResult(),
                                     BaseExpr, BaseExprType,
                                     IsArrow, OpLoc,
                                     SS.getWithLocInContext(Context),
                                     TemplateKWLoc, MemberNameInfo,
                                     TemplateArgs, R.begin(), R.end());

    return Owned(MemExpr);
  }

  assert(R.isSingleResult());
  DeclAccessPair FoundDecl = R.begin().getPair();
  NamedDecl *MemberDecl = R.getFoundDecl();

  // FIXME: diagnose the presence of template arguments now.

  // If the decl being referenced had an error, return an error for this
  // sub-expr without emitting another error, in order to avoid cascading
  // error cases.
  if (MemberDecl->isInvalidDecl())
    return ExprError();

  bool ShouldCheckUse = true;
  if (CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(MemberDecl)) {
    // Don't diagnose the use of a virtual member function unless it's
    // explicitly qualified.
    if (MD->isVirtual() && !SS.isSet())
      ShouldCheckUse = false;
  }

  // Check the use of this member.
  if (ShouldCheckUse && DiagnoseUseOfDecl(MemberDecl, MemberLoc)) {
    Owned(BaseExpr);
    return ExprError();
  }

  if (FieldDecl *FD = dyn_cast<FieldDecl>(MemberDecl))
    return BuildFieldReferenceExpr(*this, BaseExpr, IsArrow,
                                   SS, FD, FoundDecl, MemberNameInfo);

  if (IndirectFieldDecl *FD = dyn_cast<IndirectFieldDecl>(MemberDecl))
    // We may have found a field within an anonymous union or struct
    // (C++ [class.union]).
    return BuildAnonymousStructUnionMemberReference(SS, MemberLoc, FD,
                                                    BaseExpr, OpLoc);

  if (VarDecl *Var = dyn_cast<VarDecl>(MemberDecl)) {
    return Owned(BuildMemberExpr(*this, Context, BaseExpr, IsArrow, SS,
                                 TemplateKWLoc, Var, FoundDecl, MemberNameInfo,
                                 Var->getType().getNonReferenceType(),
                                 VK_LValue, OK_Ordinary));
  }

  if (CXXMethodDecl *MemberFn = dyn_cast<CXXMethodDecl>(MemberDecl)) {
    ExprValueKind valueKind;
    QualType type;
    if (MemberFn->isInstance()) {
      valueKind = VK_RValue;
      type = Context.BoundMemberTy;
    } else {
      valueKind = VK_LValue;
      type = MemberFn->getType();
    }

    return Owned(BuildMemberExpr(*this, Context, BaseExpr, IsArrow, SS, 
                                 TemplateKWLoc, MemberFn, FoundDecl, 
                                 MemberNameInfo, type, valueKind,
                                 OK_Ordinary));
  }
  assert(!isa<FunctionDecl>(MemberDecl) && "member function not C++ method?");

  if (EnumConstantDecl *Enum = dyn_cast<EnumConstantDecl>(MemberDecl)) {
    return Owned(BuildMemberExpr(*this, Context, BaseExpr, IsArrow, SS,
                                 TemplateKWLoc, Enum, FoundDecl, MemberNameInfo,
                                 Enum->getType(), VK_RValue, OK_Ordinary));
  }

  Owned(BaseExpr);

  // We found something that we didn't expect. Complain.
  if (isa<TypeDecl>(MemberDecl))
    Diag(MemberLoc, diag::err_typecheck_member_reference_type)
      << MemberName << BaseType << int(IsArrow);
  else
    Diag(MemberLoc, diag::err_typecheck_member_reference_unknown)
      << MemberName << BaseType << int(IsArrow);

  Diag(MemberDecl->getLocation(), diag::note_member_declared_here)
    << MemberName;
  R.suppressDiagnostics();
  return ExprError();
}

static bool isRecordType(QualType T) {
  return T->isRecordType();
}
static bool isPointerToRecordType(QualType T) {
  if (const PointerType *PT = T->getAs<PointerType>())
    return PT->getPointeeType()->isRecordType();
  return false;
}

/// Perform conversions on the LHS of a member access expression.
ExprResult
Sema::PerformMemberExprBaseConversion(Expr *Base, bool IsArrow) {
  if (IsArrow && !Base->getType()->isFunctionType())
    return DefaultFunctionArrayLvalueConversion(Base);

  return CheckPlaceholderExpr(Base);
}

/// Look up the given member of the given non-type-dependent
/// expression.  This can return in one of two ways:
///  * If it returns a sentinel null-but-valid result, the caller will
///    assume that lookup was performed and the results written into
///    the provided structure.  It will take over from there.
///  * Otherwise, the returned expression will be produced in place of
///    an ordinary member expression.
///
/// The ObjCImpDecl bit is a gross hack that will need to be properly
/// fixed for ObjC++.
ExprResult
Sema::LookupMemberExpr(LookupResult &R, ExprResult &BaseExpr,
                       bool &IsArrow, SourceLocation OpLoc,
                       CXXScopeSpec &SS,
                       Decl *ObjCImpDecl, bool HasTemplateArgs) {
  assert(BaseExpr.get() && "no base expression");

  // Perform default conversions.
  BaseExpr = PerformMemberExprBaseConversion(BaseExpr.take(), IsArrow);
  if (BaseExpr.isInvalid())
    return ExprError();

  QualType BaseType = BaseExpr.get()->getType();
  assert(!BaseType->isDependentType());

  DeclarationName MemberName = R.getLookupName();
  SourceLocation MemberLoc = R.getNameLoc();

  // For later type-checking purposes, turn arrow accesses into dot
  // accesses.  The only access type we support that doesn't follow
  // the C equivalence "a->b === (*a).b" is ObjC property accesses,
  // and those never use arrows, so this is unaffected.
  if (IsArrow) {
    if (const PointerType *Ptr = BaseType->getAs<PointerType>())
      BaseType = Ptr->getPointeeType();
    else if (BaseType->isRecordType()) {
      // Recover from arrow accesses to records, e.g.:
      //   struct MyRecord foo;
      //   foo->bar
      // This is actually well-formed in C++ if MyRecord has an
      // overloaded operator->, but that should have been dealt with
      // by now.
      Diag(OpLoc, diag::err_typecheck_member_reference_suggestion)
        << BaseType << int(IsArrow) << BaseExpr.get()->getSourceRange()
        << FixItHint::CreateReplacement(OpLoc, ".");
      IsArrow = false;
    } else if (BaseType->isFunctionType()) {
      goto fail;
    } else {
      Diag(MemberLoc, diag::err_typecheck_member_reference_arrow)
        << BaseType << BaseExpr.get()->getSourceRange();
      return ExprError();
    }
  }

  // Handle field access to simple records.
  if (const RecordType *RTy = BaseType->getAs<RecordType>()) {
    if (LookupMemberExprInRecord(*this, R, BaseExpr.get()->getSourceRange(),
                                 RTy, OpLoc, SS, HasTemplateArgs))
      return ExprError();

    // Returning valid-but-null is how we indicate to the caller that
    // the lookup result was filled in.
    return Owned((Expr*) 0);
  }

  // Failure cases.
 fail:

  // Recover from dot accesses to pointers, e.g.:
  //   type *foo;
  //   foo.bar
  // This is actually well-formed in two cases:
  //   - 'type' is an Objective C type
  //   - 'bar' is a pseudo-destructor name which happens to refer to
  //     the appropriate pointer type
  if (const PointerType *Ptr = BaseType->getAs<PointerType>()) {
    if (!IsArrow && Ptr->getPointeeType()->isRecordType() &&
        MemberName.getNameKind() != DeclarationName::CXXDestructorName) {
      Diag(OpLoc, diag::err_typecheck_member_reference_suggestion)
        << BaseType << int(IsArrow) << BaseExpr.get()->getSourceRange()
          << FixItHint::CreateReplacement(OpLoc, "->");

      // Recurse as an -> access.
      IsArrow = true;
      return LookupMemberExpr(R, BaseExpr, IsArrow, OpLoc, SS,
                              ObjCImpDecl, HasTemplateArgs);
    }
  }

  // If the user is trying to apply -> or . to a function name, it's probably
  // because they forgot parentheses to call that function.
  if (tryToRecoverWithCall(BaseExpr,
                           PDiag(diag::err_member_reference_needs_call),
                           /*complain*/ false,
                           IsArrow ? &isPointerToRecordType : &isRecordType)) {
    if (BaseExpr.isInvalid())
      return ExprError();
    BaseExpr = DefaultFunctionArrayConversion(BaseExpr.take());
    return LookupMemberExpr(R, BaseExpr, IsArrow, OpLoc, SS,
                            ObjCImpDecl, HasTemplateArgs);
  }

  Diag(OpLoc, diag::err_typecheck_member_reference_struct_union)
    << BaseType << BaseExpr.get()->getSourceRange() << MemberLoc;

  return ExprError();
}

/// The main callback when the parser finds something like
///   expression . [nested-name-specifier] identifier
///   expression -> [nested-name-specifier] identifier
/// where 'identifier' encompasses a fairly broad spectrum of
/// possibilities, including destructor and operator references.
///
/// \param OpKind either tok::arrow or tok::period
/// \param HasTrailingLParen whether the next token is '(', which
///   is used to diagnose mis-uses of special members that can
///   only be called
/// \param ObjCImpDecl the current Objective-C \@implementation
///   decl; this is an ugly hack around the fact that Objective-C
///   \@implementations aren't properly put in the context chain
ExprResult Sema::ActOnMemberAccessExpr(Scope *S, Expr *Base,
                                       SourceLocation OpLoc,
                                       tok::TokenKind OpKind,
                                       CXXScopeSpec &SS,
                                       SourceLocation TemplateKWLoc,
                                       UnqualifiedId &Id,
                                       Decl *ObjCImpDecl,
                                       bool HasTrailingLParen) {
  if (SS.isSet() && SS.isInvalid())
    return ExprError();

  // Warn about the explicit constructor calls Microsoft extension.
  if (getLangOpts().MicrosoftExt &&
      Id.getKind() == UnqualifiedId::IK_ConstructorName)
    Diag(Id.getSourceRange().getBegin(),
         diag::ext_ms_explicit_constructor_call);

  TemplateArgumentListInfo TemplateArgsBuffer;

  // Decompose the name into its component parts.
  DeclarationNameInfo NameInfo;
  const TemplateArgumentListInfo *TemplateArgs;
  DecomposeUnqualifiedId(Id, TemplateArgsBuffer,
                         NameInfo, TemplateArgs);

  DeclarationName Name = NameInfo.getName();
  bool IsArrow = (OpKind == tok::arrow);

  NamedDecl *FirstQualifierInScope
    = (!SS.isSet() ? 0 : FindFirstQualifierInScope(S,
                       static_cast<NestedNameSpecifier*>(SS.getScopeRep())));

  // This is a postfix expression, so get rid of ParenListExprs.
  ExprResult Result = MaybeConvertParenListExprToParenExpr(S, Base);
  if (Result.isInvalid()) return ExprError();
  Base = Result.take();

  if (Base->getType()->isDependentType() || Name.isDependentName() ||
      isDependentScopeSpecifier(SS)) {
    Result = ActOnDependentMemberExpr(Base, Base->getType(),
                                      IsArrow, OpLoc,
                                      SS, TemplateKWLoc, FirstQualifierInScope,
                                      NameInfo, TemplateArgs);
  } else {
    LookupResult R(*this, NameInfo, LookupMemberName);
    ExprResult BaseResult = Owned(Base);
    Result = LookupMemberExpr(R, BaseResult, IsArrow, OpLoc,
                              SS, ObjCImpDecl, TemplateArgs != 0);
    if (BaseResult.isInvalid())
      return ExprError();
    Base = BaseResult.take();

    if (Result.isInvalid()) {
      Owned(Base);
      return ExprError();
    }

    if (Result.get()) {
      // The only way a reference to a destructor can be used is to
      // immediately call it, which falls into this case.  If the
      // next token is not a '(', produce a diagnostic and build the
      // call now.
      if (!HasTrailingLParen &&
          Id.getKind() == UnqualifiedId::IK_DestructorName)
        return DiagnoseDtorReference(NameInfo.getLoc(), Result.get());

      return Result;
    }

    ActOnMemberAccessExtraArgs ExtraArgs = {S, Id, ObjCImpDecl, HasTrailingLParen};
    Result = BuildMemberReferenceExpr(Base, Base->getType(),
                                      OpLoc, IsArrow, SS, TemplateKWLoc,
                                      FirstQualifierInScope, R, TemplateArgs,
                                      false, &ExtraArgs);
  }

  return Result;
}

static ExprResult
BuildFieldReferenceExpr(Sema &S, Expr *BaseExpr, bool IsArrow,
                        const CXXScopeSpec &SS, FieldDecl *Field,
                        DeclAccessPair FoundDecl,
                        const DeclarationNameInfo &MemberNameInfo) {
  // x.a is an l-value if 'a' has a reference type. Otherwise:
  // x.a is an l-value/x-value/pr-value if the base is (and note
  //   that *x is always an l-value), except that if the base isn't
  //   an ordinary object then we must have an rvalue.
  ExprValueKind VK = VK_LValue;
  ExprObjectKind OK = OK_Ordinary;
  if (!IsArrow) {
    if (BaseExpr->getObjectKind() == OK_Ordinary)
      VK = BaseExpr->getValueKind();
    else
      VK = VK_RValue;
  }
  if (VK != VK_RValue && Field->isBitField())
    OK = OK_BitField;
  
  // Figure out the type of the member; see C99 6.5.2.3p3, C++ [expr.ref]
  QualType MemberType = Field->getType();
  if (const ReferenceType *Ref = MemberType->getAs<ReferenceType>()) {
    MemberType = Ref->getPointeeType();
    VK = VK_LValue;
  } else {
    QualType BaseType = BaseExpr->getType();
    if (IsArrow) BaseType = BaseType->getAs<PointerType>()->getPointeeType();

    Qualifiers BaseQuals = BaseType.getQualifiers();

    // CVR attributes from the base are picked up by members,
    // except that 'mutable' members don't pick up 'const'.
    if (Field->isMutable()) BaseQuals.removeConst();

    Qualifiers MemberQuals
    = S.Context.getCanonicalType(MemberType).getQualifiers();

    assert(!MemberQuals.hasAddressSpace());


    Qualifiers Combined = BaseQuals + MemberQuals;
    if (Combined != MemberQuals)
      MemberType = S.Context.getQualifiedType(MemberType, Combined);
  }

  S.UnusedPrivateFields.remove(Field);

  ExprResult Base =
  S.PerformObjectMemberConversion(BaseExpr, SS.getScopeRep(),
                                  FoundDecl, Field);
  if (Base.isInvalid())
    return ExprError();
  return S.Owned(BuildMemberExpr(S, S.Context, Base.take(), IsArrow, SS,
                                 /*TemplateKWLoc=*/SourceLocation(),
                                 Field, FoundDecl, MemberNameInfo,
                                 MemberType, VK, OK));
}

#endif
