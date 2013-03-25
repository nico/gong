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

#include "gong/Sema/Sema.h"

#include "gong/AST/Expr.h"
#include "gong/Sema/Lookup.h"

using namespace gong;
using namespace sema;

#if 0
#include "gong/Sema/SemaInternal.h"
#include "gong/AST/DeclCXX.h"
#include "gong/AST/ExprCXX.h"
#include "gong/Lex/Preprocessor.h"
#include "gong/Sema/Lookup.h"
#include "gong/Sema/Scope.h"
#include "gong/Sema/ScopeInfo.h"


namespace {

// Callback to only accept typo corrections that are a ValueDecl
class RecordMemberExprValidatorCCC : public CorrectionCandidateCallback {
 public:
  virtual bool ValidateCandidate(const TypoCorrection &candidate) {
    NamedDecl *ND = candidate.getCorrectionDecl();
    return ND && (isa<ValueDecl>(ND));
  }
};

}
#endif

// Returns true on failure.
static bool
LookupMemberExprInRecord(Sema &SemaRef, LookupResult &R, 
                         //SourceRange BaseRange,
                         const StructType *STy,
                         SourceLocation OpLoc) {
  StructTypeDecl *SDecl = STy->getDecl();
  DeclContext *DC = SDecl;

  // The record definition is complete, now look up the member.
  SemaRef.LookupQualifiedName(R, DC);

  if (!R.empty())
    return false;

#if 0
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
#endif

  // FIXME: Is this right? (also in clang)
  return false;
}

#if 0
static ExprResult
BuildFieldReferenceExpr(Sema &S, Expr *BaseExpr, bool IsArrow,
                        FieldDecl *Field, DeclAccessPair FoundDecl,
                        const DeclarationNameInfo &MemberNameInfo);

ExprResult
Sema::BuildAnonymousStructUnionMemberReference(SourceLocation loc,
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
                                     EmptySS, field, 
                                     foundDecl, memberNameInfo).take();
  }
  
  return Owned(result);
}
#endif

/// \brief Build a MemberExpr AST node.
static MemberExpr *BuildMemberExpr(Sema &SemaRef,
                                   ASTContext &C, Expr *Base,
                                   NamedDecl *Member,
                                   SourceLocation loc,
                                   const Type *Ty) {
  MemberExpr *E = MemberExpr::Create(C, Base, Member, loc, Ty);
  //SemaRef.MarkMemberReferenced(E);
  return E;
}

Action::OwningExprResult
Sema::BuildMemberReferenceExpr(Expr *BaseExpr, const Type *BaseExprType,
                               SourceLocation OpLoc, LookupResult &R) {
  const Type* BaseType = BaseExprType;

  if (const PointerType *P = dyn_cast<PointerType>(BaseType))
    BaseType = P->getPointeeType();
  //R.setBaseObjectType(BaseType);

  //const DeclarationNameInfo &MemberNameInfo = R.getLookupNameInfo();
  //DeclarationName MemberName = MemberNameInfo.getName();
  //SourceLocation MemberLoc = MemberNameInfo.getLoc();
  IdentifierInfo *II = R.getLookupName();

  if (R.isAmbiguous())
    return ExprError();

  // FIXME: embedded fields
  if (R.empty()) {
    // FIXME: make sure this prints the '*' for pointer-to-struct types (?)
    DeclContext *DC = BaseType->getAs<StructType>()->getDecl();
    Diag(R.getNameLoc(), diag::no_field) << II << DC;
    //Diag(R.getNameLoc(), diag::err_no_member)
      //<< MemberName << DC
      //<< (BaseExpr ? BaseExpr->getSourceRange() : SourceRange());
    return ExprError();
  }

  assert(R.isSingleResult());
  NamedDecl *MemberDecl = R.getFoundDecl();
#if 0
  DeclAccessPair FoundDecl = R.begin().getPair();

  // If the decl being referenced had an error, return an error for this
  // sub-expr without emitting another error, in order to avoid cascading
  // error cases.
  if (MemberDecl->isInvalidDecl())
    return ExprError();

  bool ShouldCheckUse = true;
  if (CXXMethodDecl *MD = dyn_cast<CXXMethodDecl>(MemberDecl)) {
    // Don't diagnose the use of a virtual member function unless it's
    // explicitly qualified.
    if (MD->isVirtual())
      ShouldCheckUse = false;
  }

  // Check the use of this member.
  if (ShouldCheckUse && DiagnoseUseOfDecl(MemberDecl, MemberLoc)) {
    Owned(BaseExpr);
    return ExprError();
  }
#endif

  if (FieldDecl *FD = dyn_cast<FieldDecl>(MemberDecl)) {
    //return BuildFieldReferenceExpr(*this, BaseExpr, IsArrow,
                                   //FD, FoundDecl, MemberNameInfo);
    return Owned(BuildMemberExpr(*this, Context, BaseExpr, FD,
                                 R.getNameLoc(), FD->getType()));
  }

#if 0
  if (IndirectFieldDecl *FD = dyn_cast<IndirectFieldDecl>(MemberDecl))
    // We may have found a field within an anonymous union or struct
    // (C++ [class.union]).
    return BuildAnonymousStructUnionMemberReference(MemberLoc, FD,
                                                    BaseExpr, OpLoc);

  if (VarDecl *Var = dyn_cast<VarDecl>(MemberDecl)) {
    return Owned(BuildMemberExpr(*this, Context, BaseExpr, IsArrow,
                                 Var, FoundDecl, MemberNameInfo,
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

    return Owned(BuildMemberExpr(*this, Context, BaseExpr, IsArrow,
                                 MemberFn, FoundDecl, 
                                 MemberNameInfo, type, valueKind,
                                 OK_Ordinary));
  }
  assert(!isa<FunctionDecl>(MemberDecl) && "member function not C++ method?");

  if (EnumConstantDecl *Enum = dyn_cast<EnumConstantDecl>(MemberDecl)) {
    return Owned(BuildMemberExpr(*this, Context, BaseExpr, IsArrow,
                                 Enum, FoundDecl, MemberNameInfo,
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
#endif
  return ExprError();
}

#if 0
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
#endif

/// Look up the given member of the given non-type-dependent
/// expression.  This can return in one of two ways:
///  * If it returns a sentinel null-but-valid result, the caller will
///    assume that lookup was performed and the results written into
///    the provided structure.  It will take over from there.
///  * Otherwise, the returned expression will be produced in place of
///    an ordinary member expression.
Action::OwningExprResult
Sema::LookupMemberExpr(LookupResult &R, Expr &BaseExpr,
                       SourceLocation OpLoc) {

  const Type *BaseType = BaseExpr.getType();
  assert(BaseType);

  SourceLocation MemberLoc = R.getNameLoc();
#if 0
  DeclarationName MemberName = R.getLookupName();

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
#endif
  // spec#Selector: "If x is a pointer to a struct, x.y is shorthand for (*x).y"
  if (const PointerType *P = dyn_cast<PointerType>(BaseType))
    BaseType = P->getPointeeType();

  // Handle field access to simple records.
  if (const StructType *STy = BaseType->getAs<StructType>()) {
    if (LookupMemberExprInRecord(*this, R, //BaseExpr.get()->getSourceRange(),
                                 STy, OpLoc))
      return ExprError();

    // Returning valid-but-null is how we indicate to the caller that
    // the lookup result was filled in.
    return Owned((Expr *)0);
  }

#if 0
  // Failure cases.
 fail:

  // If the user is trying to apply . to a function name, it's probably
  // because they forgot parentheses to call that function.
  if (tryToRecoverWithCall(BaseExpr,
                           PDiag(diag::err_member_reference_needs_call),
                           /*complain*/ false,
                           IsArrow ? &isPointerToRecordType : &isRecordType)) {
    if (BaseExpr.isInvalid())
      return ExprError();
    BaseExpr = DefaultFunctionArrayConversion(BaseExpr.take());
    return LookupMemberExpr(R, BaseExpr, IsArrow, OpLoc);
  }
#endif

  Diag(OpLoc, diag::typecheck_field_reference_struct)
    //<< BaseType << BaseExpr.get()->getSourceRange() << MemberLoc;
    << BaseType << SourceRange(MemberLoc, MemberLoc);

  return ExprError();
}

Action::OwningExprResult
Sema::ActOnSelectorExpr(ExprArg BaseExpr, SourceLocation OpLoc,
                        SourceLocation IILoc, IdentifierInfo *II) {
  Expr *Base = BaseExpr.takeAs<Expr>();
  // Note: This will fail for all ExprEmpty()s getting passed in (atm for all
  // numeric literals etc)
  assert(Base && "no base expression");

  // If base is a var, this is a lookup into its type (struct or interface)
  // If base is a type, this could be a MethodExpr.

  LookupResult R(*this, II, IILoc, LookupMemberName);
  OwningExprResult BaseResult = Owned(Base);
  OwningExprResult Result = LookupMemberExpr(R, *Base, OpLoc);
  if (BaseResult.isInvalid())
    return ExprError();
  Base = BaseResult.takeAs<Expr>();

  if (Result.isInvalid()) {
    Owned(Base);
    return ExprError();
  }

  if (Result.get()) {
    return Result;
  }

  Result = BuildMemberReferenceExpr(Base, Base->getType(), OpLoc, R);
  return Result;
}


#if 0
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
  return S.Owned(BuildMemberExpr(S, S.Context, Base.take(), IsArrow,
                                 Field, FoundDecl, MemberNameInfo,
                                 MemberType, VK, OK));
}

#endif
