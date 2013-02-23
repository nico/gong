//===--------------------- SemaLookup.cpp - Name Lookup  ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements name lookup for Go.
//
//===----------------------------------------------------------------------===//

#include "gong/Sema/Sema.h"

#include "gong/AST/Decl.h"
#include "gong/Parse/Scope.h"
#include "gong/Sema/Lookup.h"
using namespace gong;
using namespace sema;

#if 0
#include "gong/AST/ASTContext.h"
#include "gong/AST/CXXInheritance.h"
#include "gong/AST/DeclCXX.h"
#include "gong/AST/DeclLookups.h"
#include "gong/AST/DeclObjC.h"
#include "gong/AST/DeclTemplate.h"
#include "gong/AST/Expr.h"
#include "gong/AST/ExprCXX.h"
#include "gong/Basic/Builtins.h"
#include "gong/Basic/LangOptions.h"
#include "gong/Sema/DeclSpec.h"
#include "gong/Sema/ExternalSemaSource.h"
#include "gong/Sema/Overload.h"
#include "gong/Sema/ScopeInfo.h"
#include "gong/Sema/SemaInternal.h"
#include "gong/Sema/TemplateDeduction.h"
#include "gong/Sema/TypoCorrection.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/TinyPtrVector.h"
#include "llvm/ADT/edit_distance.h"
#include "llvm/Support/ErrorHandling.h"
#include <algorithm>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace {
  class UnqualUsingEntry {
    const DeclContext *Nominated;
    const DeclContext *CommonAncestor;

  public:
    UnqualUsingEntry(const DeclContext *Nominated,
                     const DeclContext *CommonAncestor)
      : Nominated(Nominated), CommonAncestor(CommonAncestor) {
    }

    const DeclContext *getCommonAncestor() const {
      return CommonAncestor;
    }

    const DeclContext *getNominatedNamespace() const {
      return Nominated;
    }

    // Sort by the pointer value of the common ancestor.
    struct Comparator {
      bool operator()(const UnqualUsingEntry &L, const UnqualUsingEntry &R) {
        return L.getCommonAncestor() < R.getCommonAncestor();
      }

      bool operator()(const UnqualUsingEntry &E, const DeclContext *DC) {
        return E.getCommonAncestor() < DC;
      }

      bool operator()(const DeclContext *DC, const UnqualUsingEntry &E) {
        return DC < E.getCommonAncestor();
      }
    };
  };

  /// A collection of using directives, as used by C++ unqualified
  /// lookup.
  class UnqualUsingDirectiveSet {
    typedef SmallVector<UnqualUsingEntry, 8> ListTy;

    ListTy list;
    llvm::SmallPtrSet<DeclContext*, 8> visited;

  public:
    UnqualUsingDirectiveSet() {}

    void visitScopeChain(Scope *S, Scope *InnermostFileScope) {
      // C++ [namespace.udir]p1:
      //   During unqualified name lookup, the names appear as if they
      //   were declared in the nearest enclosing namespace which contains
      //   both the using-directive and the nominated namespace.
      DeclContext *InnermostFileDC
        = static_cast<DeclContext*>(InnermostFileScope->getEntity());
      assert(InnermostFileDC && InnermostFileDC->isFileContext());

      for (; S; S = S->getParent()) {
        // C++ [namespace.udir]p1:
        //   A using-directive shall not appear in class scope, but may
        //   appear in namespace scope or in block scope.
        DeclContext *Ctx = static_cast<DeclContext*>(S->getEntity());
        if (Ctx && Ctx->isFileContext()) {
          visit(Ctx, Ctx);
        } else if (!Ctx || Ctx->isFunctionOrMethod()) {
          Scope::udir_iterator I = S->using_directives_begin(),
                             End = S->using_directives_end();
          for (; I != End; ++I)
            visit(*I, InnermostFileDC);
        }
      }
    }

    // Visits a context and collect all of its using directives
    // recursively.  Treats all using directives as if they were
    // declared in the context.
    //
    // A given context is only every visited once, so it is important
    // that contexts be visited from the inside out in order to get
    // the effective DCs right.
    void visit(DeclContext *DC, DeclContext *EffectiveDC) {
      if (!visited.insert(DC))
        return;

      addUsingDirectives(DC, EffectiveDC);
    }

    // Visits a using directive and collects all of its using
    // directives recursively.  Treats all using directives as if they
    // were declared in the effective DC.
    void visit(UsingDirectiveDecl *UD, DeclContext *EffectiveDC) {
      DeclContext *NS = UD->getNominatedNamespace();
      if (!visited.insert(NS))
        return;

      addUsingDirective(UD, EffectiveDC);
      addUsingDirectives(NS, EffectiveDC);
    }

    // Adds all the using directives in a context (and those nominated
    // by its using directives, transitively) as if they appeared in
    // the given effective context.
    void addUsingDirectives(DeclContext *DC, DeclContext *EffectiveDC) {
      SmallVector<DeclContext*,4> queue;
      while (true) {
        DeclContext::udir_iterator I, End;
        for (llvm::tie(I, End) = DC->getUsingDirectives(); I != End; ++I) {
          UsingDirectiveDecl *UD = *I;
          DeclContext *NS = UD->getNominatedNamespace();
          if (visited.insert(NS)) {
            addUsingDirective(UD, EffectiveDC);
            queue.push_back(NS);
          }
        }

        if (queue.empty())
          return;

        DC = queue.back();
        queue.pop_back();
      }
    }

    // Add a using directive as if it had been declared in the given
    // context.  This helps implement C++ [namespace.udir]p3:
    //   The using-directive is transitive: if a scope contains a
    //   using-directive that nominates a second namespace that itself
    //   contains using-directives, the effect is as if the
    //   using-directives from the second namespace also appeared in
    //   the first.
    void addUsingDirective(UsingDirectiveDecl *UD, DeclContext *EffectiveDC) {
      // Find the common ancestor between the effective context and
      // the nominated namespace.
      DeclContext *Common = UD->getNominatedNamespace();
      while (!Common->Encloses(EffectiveDC))
        Common = Common->getParent();
      Common = Common->getPrimaryContext();

      list.push_back(UnqualUsingEntry(UD->getNominatedNamespace(), Common));
    }

    void done() {
      std::sort(list.begin(), list.end(), UnqualUsingEntry::Comparator());
    }

    typedef ListTy::const_iterator const_iterator;

    const_iterator begin() const { return list.begin(); }
    const_iterator end() const { return list.end(); }

    std::pair<const_iterator,const_iterator>
    getNamespacesFor(DeclContext *DC) const {
      return std::equal_range(begin(), end(), DC->getPrimaryContext(),
                              UnqualUsingEntry::Comparator());
    }
  };
}

// Retrieve the set of identifier namespaces that correspond to a
// specific kind of name lookup.
static inline unsigned getIDNS(Sema::LookupNameKind NameKind,
                               bool CPlusPlus,
                               bool Redeclaration) {
  unsigned IDNS = 0;
  switch (NameKind) {
  case Sema::LookupObjCImplicitSelfParam:
  case Sema::LookupOrdinaryName:
  case Sema::LookupRedeclarationWithLinkage:
    IDNS = Decl::IDNS_Ordinary;
    if (CPlusPlus) {
      IDNS |= Decl::IDNS_Tag | Decl::IDNS_Member | Decl::IDNS_Namespace;
      if (Redeclaration)
        IDNS |= Decl::IDNS_TagFriend | Decl::IDNS_OrdinaryFriend;
    }
    break;

  case Sema::LookupOperatorName:
    // Operator lookup is its own crazy thing;  it is not the same
    // as (e.g.) looking up an operator name for redeclaration.
    assert(!Redeclaration && "cannot do redeclaration operator lookup");
    IDNS = Decl::IDNS_NonMemberOperator;
    break;

  case Sema::LookupTagName:
    if (CPlusPlus) {
      IDNS = Decl::IDNS_Type;

      // When looking for a redeclaration of a tag name, we add:
      // 1) TagFriend to find undeclared friend decls
      // 2) Namespace because they can't "overload" with tag decls.
      // 3) Tag because it includes class templates, which can't
      //    "overload" with tag decls.
      if (Redeclaration)
        IDNS |= Decl::IDNS_Tag | Decl::IDNS_TagFriend | Decl::IDNS_Namespace;
    } else {
      IDNS = Decl::IDNS_Tag;
    }
    break;
  case Sema::LookupLabel:
    IDNS = Decl::IDNS_Label;
    break;
      
  case Sema::LookupMemberName:
    IDNS = Decl::IDNS_Member;
    if (CPlusPlus)
      IDNS |= Decl::IDNS_Tag | Decl::IDNS_Ordinary;
    break;

  case Sema::LookupNestedNameSpecifierName:
    IDNS = Decl::IDNS_Type | Decl::IDNS_Namespace;
    break;

  case Sema::LookupNamespaceName:
    IDNS = Decl::IDNS_Namespace;
    break;

  case Sema::LookupUsingDeclName:
    IDNS = Decl::IDNS_Ordinary | Decl::IDNS_Tag
         | Decl::IDNS_Member | Decl::IDNS_Using;
    break;

  case Sema::LookupObjCProtocolName:
    IDNS = Decl::IDNS_ObjCProtocol;
    break;

  case Sema::LookupAnyName:
    IDNS = Decl::IDNS_Ordinary | Decl::IDNS_Tag | Decl::IDNS_Member
      | Decl::IDNS_Using | Decl::IDNS_Namespace | Decl::IDNS_ObjCProtocol
      | Decl::IDNS_Type;
    break;
  }
  return IDNS;
}
#endif

void LookupResult::configure() {
}

void LookupResult::sanityImpl() const {
  // Note that this function is never called by NDEBUG builds. See
  // LookupResult::sanity().
  assert(ResultKind != NotFound || Decls.size() == 0);
  assert(ResultKind != Found || Decls.size() == 1);
  assert(ResultKind != FoundOverloaded || Decls.size() > 1);
  //assert(ResultKind != FoundUnresolvedValue || sanityCheckUnresolved());
  assert(ResultKind != Ambiguous || Decls.size() > 1 ||
         (Decls.size() == 1 && (Ambiguity == AmbiguousBaseSubobjects ||
                                Ambiguity == AmbiguousBaseSubobjectTypes)));
  //assert((Paths != NULL) == (ResultKind == Ambiguous &&
  //                           (Ambiguity == AmbiguousBaseSubobjectTypes ||
  //                            Ambiguity == AmbiguousBaseSubobjects)));
}

#if 0
// Necessary because CXXBasePaths is not complete in Sema.h
void LookupResult::deletePaths(CXXBasePaths *Paths) {
  delete Paths;
}

static NamedDecl *getVisibleDecl(NamedDecl *D);

NamedDecl *LookupResult::getAcceptableDeclSlow(NamedDecl *D) const {
  return getVisibleDecl(D);
}
#endif

/// Resolves the result kind of this lookup.
void LookupResult::resolveKind() {
  unsigned N = Decls.size();

  // Fast case: no possible ambiguity.
  if (N == 0) {
    assert(ResultKind == NotFound);
    return;
  }

  // If there's a single decl, we need to examine it to decide what
  // kind of lookup this is.
  //if (N == 1) {  // FIXME
  //  NamedDecl *D = (*Decls.begin()); //->getUnderlyingDecl();
  //  if (isa<FunctionTemplateDecl>(D))
  //    ResultKind = FoundOverloaded;
  //  else if (isa<UnresolvedUsingValueDecl>(D))
  //    ResultKind = FoundUnresolvedValue;
  //  return;
  //}

  // Don't do any extra resolution if we've already resolved as ambiguous.
  if (ResultKind == Ambiguous) return;

  llvm::SmallPtrSet<NamedDecl*, 16> Unique;
  llvm::SmallPtrSet<const Type*, 16> UniqueTypes;

  bool Ambiguous = false;
  bool HasTag = false, HasFunction = false, HasNonFunction = false;
  bool HasFunctionTemplate = false, HasUnresolved = false;

  unsigned UniqueTagIndex = 0;

  unsigned I = 0;
  while (I < N) {
    NamedDecl *D = Decls[I]; //->getUnderlyingDecl();
    D = cast<NamedDecl>(D); //->getCanonicalDecl());

    // Redeclarations of types via typedef can occur both within a scope
    // and, through using declarations and directives, across scopes. There is
    // no ambiguity if they all refer to the same type, so unique based on the
    // canonical type.
    //if (TypeDecl *TD = dyn_cast<TypeDecl>(D)) {
    //  if (!TD->getDeclContext()->isRecord()) {
    //    QualType T = SemaRef.Context.getTypeDeclType(TD);
    //    if (!UniqueTypes.insert(SemaRef.Context.getCanonicalType(T))) {
    //      // The type is not unique; pull something off the back and continue
    //      // at this index.
    //      Decls[I] = Decls[--N];
    //      continue;
    //    }
    //  }
    //}

    if (!Unique.insert(D)) {
      // If it's not unique, pull something off the back (and
      // continue at this index).
      Decls[I] = Decls[--N];
      continue;
    }

    // Otherwise, do some decl type analysis and then continue.

    //if (isa<UnresolvedUsingValueDecl>(D)) {
    //  HasUnresolved = true;
    //} else if (isa<TagDecl>(D)) {
    //  if (HasTag)
    //    Ambiguous = true;
    //  UniqueTagIndex = I;
    //  HasTag = true;
    //} else if (isa<FunctionTemplateDecl>(D)) {
    //  HasFunction = true;
    //  HasFunctionTemplate = true;
    //} else if (isa<FunctionDecl>(D)) {
    //  HasFunction = true;
    //} else {
      if (HasNonFunction)
        Ambiguous = true;
      HasNonFunction = true;
    //}
    I++;
  }

  // C++ [basic.scope.hiding]p2:
  //   A class name or enumeration name can be hidden by the name of
  //   an object, function, or enumerator declared in the same
  //   scope. If a class or enumeration name and an object, function,
  //   or enumerator are declared in the same scope (in any order)
  //   with the same name, the class or enumeration name is hidden
  //   wherever the object, function, or enumerator name is visible.
  // But it's still an error if there are distinct tag types found,
  // even if they're not visible. (ref?)
  if (HideTags && HasTag && !Ambiguous &&
      (HasFunction || HasNonFunction || HasUnresolved)) {
    if (Decls[UniqueTagIndex]->getDeclContext()->getRedeclContext()->Equals(
         Decls[UniqueTagIndex? 0 : N-1]->getDeclContext()->getRedeclContext()))
      Decls[UniqueTagIndex] = Decls[--N];
    else
      Ambiguous = true;
  }

  Decls.set_size(N);

  if (HasNonFunction && (HasFunction || HasUnresolved))
    Ambiguous = true;

  if (Ambiguous)
    setAmbiguous(LookupResult::AmbiguousReference);
  //else if (HasUnresolved)
    //ResultKind = LookupResult::FoundUnresolvedValue;
  else if (N > 1 || HasFunctionTemplate)
    ResultKind = LookupResult::FoundOverloaded;
  else
    ResultKind = LookupResult::Found;
}

#if 0
void LookupResult::addDeclsFromBasePaths(const CXXBasePaths &P) {
  CXXBasePaths::const_paths_iterator I, E;
  for (I = P.begin(), E = P.end(); I != E; ++I)
    for (DeclContext::lookup_iterator DI = I->Decls.begin(),
         DE = I->Decls.end(); DI != DE; ++DI)
      addDecl(*DI);
}

void LookupResult::setAmbiguousBaseSubobjects(CXXBasePaths &P) {
  Paths = new CXXBasePaths;
  Paths->swap(P);
  addDeclsFromBasePaths(*Paths);
  resolveKind();
  setAmbiguous(AmbiguousBaseSubobjects);
}

void LookupResult::setAmbiguousBaseSubobjectTypes(CXXBasePaths &P) {
  Paths = new CXXBasePaths;
  Paths->swap(P);
  addDeclsFromBasePaths(*Paths);
  resolveKind();
  setAmbiguous(AmbiguousBaseSubobjectTypes);
}

void LookupResult::print(raw_ostream &Out) {
  Out << Decls.size() << " result(s)";
  if (isAmbiguous()) Out << ", ambiguous";
  if (Paths) Out << ", base paths present";

  for (iterator I = begin(), E = end(); I != E; ++I) {
    Out << "\n";
    (*I)->print(Out, 2);
  }
}

/// \brief Lookup a builtin function, when name lookup would otherwise
/// fail.
static bool LookupBuiltin(Sema &S, LookupResult &R) {
  Sema::LookupNameKind NameKind = R.getLookupKind();

  // If we didn't find a use of this identifier, and if the identifier
  // corresponds to a compiler builtin, create the decl object for the builtin
  // now, injecting it into translation unit scope, and return it.
  if (NameKind == Sema::LookupOrdinaryName ||
      NameKind == Sema::LookupRedeclarationWithLinkage) {
    IdentifierInfo *II = R.getLookupName().getAsIdentifierInfo();
    if (II) {
      // If this is a builtin on this (or all) targets, create the decl.
      if (unsigned BuiltinID = II->getBuiltinID()) {
        // In C++, we don't have any predefined library functions like
        // 'malloc'. Instead, we'll just error.
        if (S.getLangOpts().CPlusPlus &&
            S.Context.BuiltinInfo.isPredefinedLibFunction(BuiltinID))
          return false;

        if (NamedDecl *D = S.LazilyCreateBuiltin((IdentifierInfo *)II,
                                                 BuiltinID, S.TUScope,
                                                 R.isForRedeclaration(),
                                                 R.getNameLoc())) {
          R.addDecl(D);
          return true;
        }

        if (R.isForRedeclaration()) {
          // If we're redeclaring this function anyway, forget that
          // this was a builtin at all.
          S.Context.BuiltinInfo.ForgetBuiltin(BuiltinID, S.Context.Idents);
        }

        return false;
      }
    }
  }

  return false;
}

/// \brief Determine whether we can declare a special member function within
/// the class at this point.
static bool CanDeclareSpecialMemberFunction(const CXXRecordDecl *Class) {
  // We need to have a definition for the class.
  if (!Class->getDefinition() || Class->isDependentContext())
    return false;

  // We can't be in the middle of defining the class.
  return !Class->isBeingDefined();
}

// Adds all qualifying matches for a name within a decl context to the
// given lookup result.  Returns true if any matches were found.
static bool LookupDirect(Sema &S, LookupResult &R, const DeclContext *DC) {
  bool Found = false;

  // Perform lookup into this declaration context.
  DeclContext::lookup_const_result DR = DC->lookup(R.getLookupName());
  for (DeclContext::lookup_const_iterator I = DR.begin(), E = DR.end(); I != E;
       ++I) {
    NamedDecl *D = *I;
    if ((D = R.getAcceptableDecl(D))) {
      R.addDecl(D);
      Found = true;
    }
  }

  if (!Found && DC->isTranslationUnit() && LookupBuiltin(S, R))
    return true;

  if (R.getLookupName().getNameKind()
        != DeclarationName::CXXConversionFunctionName ||
      R.getLookupName().getCXXNameType()->isDependentType() ||
      !isa<CXXRecordDecl>(DC))
    return Found;

  // C++ [temp.mem]p6:
  //   A specialization of a conversion function template is not found by
  //   name lookup. Instead, any conversion function templates visible in the
  //   context of the use are considered. [...]
  const CXXRecordDecl *Record = cast<CXXRecordDecl>(DC);
  if (!Record->isCompleteDefinition())
    return Found;

  for (CXXRecordDecl::conversion_iterator U = Record->conversion_begin(),
         UEnd = Record->conversion_end(); U != UEnd; ++U) {
    FunctionTemplateDecl *ConvTemplate = dyn_cast<FunctionTemplateDecl>(*U);
    if (!ConvTemplate)
      continue;

    // When we're performing lookup for the purposes of redeclaration, just
    // add the conversion function template. When we deduce template
    // arguments for specializations, we'll end up unifying the return
    // type of the new declaration with the type of the function template.
    if (R.isForRedeclaration()) {
      R.addDecl(ConvTemplate);
      Found = true;
      continue;
    }

    // C++ [temp.mem]p6:
    //   [...] For each such operator, if argument deduction succeeds
    //   (14.9.2.3), the resulting specialization is used as if found by
    //   name lookup.
    //
    // When referencing a conversion function for any purpose other than
    // a redeclaration (such that we'll be building an expression with the
    // result), perform template argument deduction and place the
    // specialization into the result set. We do this to avoid forcing all
    // callers to perform special deduction for conversion functions.
    TemplateDeductionInfo Info(R.getNameLoc());
    FunctionDecl *Specialization = 0;

    const FunctionProtoType *ConvProto
      = ConvTemplate->getTemplatedDecl()->getType()->getAs<FunctionProtoType>();
    assert(ConvProto && "Nonsensical conversion function template type");

    // Compute the type of the function that we would expect the conversion
    // function to have, if it were to match the name given.
    // FIXME: Calling convention!
    FunctionProtoType::ExtProtoInfo EPI = ConvProto->getExtProtoInfo();
    EPI.ExtInfo = EPI.ExtInfo.withCallingConv(CC_Default);
    EPI.ExceptionSpecType = EST_None;
    EPI.NumExceptions = 0;
    QualType ExpectedType
      = R.getSema().Context.getFunctionType(R.getLookupName().getCXXNameType(),
                                            0, 0, EPI);

    // Perform template argument deduction against the type that we would
    // expect the function to have.
    if (R.getSema().DeduceTemplateArguments(ConvTemplate, 0, ExpectedType,
                                            Specialization, Info)
          == Sema::TDK_Success) {
      R.addDecl(Specialization);
      Found = true;
    }
  }

  return Found;
}

static bool isNamespaceOrTranslationUnitScope(Scope *S) {
  if (DeclContext *Ctx = static_cast<DeclContext*>(S->getEntity()))
    return Ctx->isFileContext();
  return false;
}

// Find the next outer declaration context from this scope. This
// routine actually returns the semantic outer context, which may
// differ from the lexical context (encoded directly in the Scope
// stack) when we are parsing a member of a class template. In this
// case, the second element of the pair will be true, to indicate that
// name lookup should continue searching in this semantic context when
// it leaves the current template parameter scope.
static std::pair<DeclContext *, bool> findOuterContext(Scope *S) {
  DeclContext *DC = static_cast<DeclContext *>(S->getEntity());
  DeclContext *Lexical = 0;
  for (Scope *OuterS = S->getParent(); OuterS;
       OuterS = OuterS->getParent()) {
    if (OuterS->getEntity()) {
      Lexical = static_cast<DeclContext *>(OuterS->getEntity());
      break;
    }
  }

  // C++ [temp.local]p8:
  //   In the definition of a member of a class template that appears
  //   outside of the namespace containing the class template
  //   definition, the name of a template-parameter hides the name of
  //   a member of this namespace.
  //
  // Example:
  //
  //   namespace N {
  //     class C { };
  //
  //     template<class T> class B {
  //       void f(T);
  //     };
  //   }
  //
  //   template<class C> void N::B<C>::f(C) {
  //     C b;  // C is the template parameter, not N::C
  //   }
  //
  // In this example, the lexical context we return is the
  // TranslationUnit, while the semantic context is the namespace N.
  if (!Lexical || !DC || !S->getParent() ||
      !S->getParent()->isTemplateParamScope())
    return std::make_pair(Lexical, false);

  // Find the outermost template parameter scope.
  // For the example, this is the scope for the template parameters of
  // template<class C>.
  Scope *OutermostTemplateScope = S->getParent();
  while (OutermostTemplateScope->getParent() &&
         OutermostTemplateScope->getParent()->isTemplateParamScope())
    OutermostTemplateScope = OutermostTemplateScope->getParent();

  // Find the namespace context in which the original scope occurs. In
  // the example, this is namespace N.
  DeclContext *Semantic = DC;
  while (!Semantic->isFileContext())
    Semantic = Semantic->getParent();

  // Find the declaration context just outside of the template
  // parameter scope. This is the context in which the template is
  // being lexically declaration (a namespace context). In the
  // example, this is the global scope.
  if (Lexical->isFileContext() && !Lexical->Equals(Semantic) &&
      Lexical->Encloses(Semantic))
    return std::make_pair(Semantic, true);

  return std::make_pair(Lexical, false);
}

#endif
/// \brief Retrieve the visible declaration corresponding to D, if any.
///
/// This routine determines whether the declaration D is visible in the current
/// module, with the current imports. If not, it checks whether any
/// redeclaration of D is visible, and if so, returns that declaration.
/// 
/// \returns D, or a visible previous declaration of D, whichever is more recent
/// and visible. If no declaration of D is visible, returns null.
static NamedDecl *getVisibleDecl(NamedDecl *D) {
  if (LookupResult::isVisible(D))
    return D;
  
  for (Decl::redecl_iterator RD = D->redecls_begin(), RDEnd = D->redecls_end();
       RD != RDEnd; ++RD) {
    if (NamedDecl *ND = dyn_cast<NamedDecl>(*RD)) {
      if (LookupResult::isVisible(ND))
        return ND;
    }
  }
  
  return 0;
}

#if 0
/// @brief Perform unqualified name lookup starting from a given
/// scope.
///
/// Unqualified name lookup (C++ [basic.lookup.unqual], C99 6.2.1) is
/// used to find names within the current scope. For example, 'x' in
/// @code
/// int x;
/// int f() {
///   return x; // unqualified name look finds 'x' in the global scope
/// }
/// @endcode
///
/// Different lookup criteria can find different names. For example, a
/// particular scope can have both a struct and a function of the same
/// name, and each can be found by certain lookup criteria. For more
/// information about lookup criteria, see the documentation for the
/// class LookupCriteria.
///
/// @param S        The scope from which unqualified name lookup will
/// begin. If the lookup criteria permits, name lookup may also search
/// in the parent scopes.
///
/// @param [in,out] R Specifies the lookup to perform (e.g., the name to
/// look up and the lookup kind), and is updated with the results of lookup
/// including zero or more declarations and possibly additional information
/// used to diagnose ambiguities.
///
/// @returns \c true if lookup succeeded and false otherwise.
#endif
bool Sema::LookupName(LookupResult &R, Scope *S, bool AllowBuiltinCreation) {
  IdentifierInfo *Name = R.getLookupName();
  if (!Name) return false;

  LookupNameKind NameKind = R.getLookupKind();

  // Unqualified name lookup in Go is purely lexical, so
  // search in the declarations attached to the name.
  if (NameKind == Sema::LookupRedeclarationWithLinkage) {
    // Find the nearest non-transparent declaration scope.
    while (!(S->getFlags() & Scope::DeclScope) ||
           (S->getEntity() &&
            static_cast<DeclContext *>(S->getEntity())
              ->isTransparentContext()))
      S = S->getParent();
  }

  // Scan up the scope chain looking for a decl that matches this
  // identifier that is in the appropriate namespace.  This search
  // should not take long, as shadowing of names is uncommon, and
  // deep shadowing is extremely uncommon.
  bool LeftStartingScope = false;

  for (IdentifierResolver::iterator I = IdResolver.begin(*Name),
                                    IEnd = IdResolver.end();
       I != IEnd; ++I) {
    if (NameKind == LookupRedeclarationWithLinkage) {
      // Determine whether this (or a previous) declaration is
      // out-of-scope.
      if (!LeftStartingScope && !S->isDeclScope(Action::DeclPtrTy::make(*I)))
        LeftStartingScope = true;

      // If we found something outside of our starting scope that
      // does not have linkage, skip it.
      //if (LeftStartingScope && !((*I)->hasLinkage()))
        //continue;
    }
    
    // If this declaration is module-private and it came from an AST
    // file, we can't see it.
    NamedDecl *D = R.isHiddenDeclarationVisible()? *I : getVisibleDecl(*I);
    if (!D)
      continue;

    R.addDecl(D);

    // Check whether there are any other declarations with the same name
    // and in the same scope.
    if (I != IEnd) {
      // Find the scope in which this declaration was declared (if it
      // actually exists in a Scope).
      while (S && !S->isDeclScope(Action::DeclPtrTy::make(D)))
        S = S->getParent();

      // If the scope containing the declaration is the translation unit,
      // then we'll need to perform our checks based on the matching
      // DeclContexts rather than matching scopes.
      //if (S && isNamespaceOrTranslationUnitScope(S))  // FIXME?
        //S = 0;

      // Compute the DeclContext, if we need it.
      DeclContext *DC = 0;
      if (!S)
        DC = (*I)->getDeclContext()->getRedeclContext();

      IdentifierResolver::iterator LastI = I;
      for (++LastI; LastI != IEnd; ++LastI) {
        if (S) {
          // Match based on scope.
          if (!S->isDeclScope(Action::DeclPtrTy::make(*LastI)))
            break;
        } else {
          // Match based on DeclContext.
          DeclContext *LastDC =
              (*LastI)->getDeclContext()->getRedeclContext();
          if (!LastDC->Equals(DC))
            break;
        }

        D = R.isHiddenDeclarationVisible()? *LastI : getVisibleDecl(*LastI);
        if (D)
          R.addDecl(D);
      }

      R.resolveKind();
    }
    return true;
  }

  // If we didn't find a use of this identifier, and if the identifier
  // corresponds to a compiler builtin, create the decl object for the builtin
  // now, injecting it into translation unit scope, and return it.
  //if (AllowBuiltinCreation && LookupBuiltin(*this, R))
    //return true;

  // If we didn't find a use of this identifier, the ExternalSource 
  // may be able to handle the situation. 
  // Note: some lookup failures are expected!
  // See e.g. R.isForRedeclaration().
  //return (ExternalSource && ExternalSource->LookupUnqualified(R, S));
  return false;
}

#if 0
/// @brief Perform qualified name lookup in the namespaces nominated by
/// using directives by the given context.
///
/// C++98 [namespace.qual]p2:
///   Given X::m (where X is a user-declared namespace), or given \::m
///   (where X is the global namespace), let S be the set of all
///   declarations of m in X and in the transitive closure of all
///   namespaces nominated by using-directives in X and its used
///   namespaces, except that using-directives are ignored in any
///   namespace, including X, directly containing one or more
///   declarations of m. No namespace is searched more than once in
///   the lookup of a name. If S is the empty set, the program is
///   ill-formed. Otherwise, if S has exactly one member, or if the
///   context of the reference is a using-declaration
///   (namespace.udecl), S is the required set of declarations of
///   m. Otherwise if the use of m is not one that allows a unique
///   declaration to be chosen from S, the program is ill-formed.
///
/// C++98 [namespace.qual]p5:
///   During the lookup of a qualified namespace member name, if the
///   lookup finds more than one declaration of the member, and if one
///   declaration introduces a class name or enumeration name and the
///   other declarations either introduce the same object, the same
///   enumerator or a set of functions, the non-type name hides the
///   class or enumeration name if and only if the declarations are
///   from the same namespace; otherwise (the declarations are from
///   different namespaces), the program is ill-formed.
static bool LookupQualifiedNameInUsingDirectives(Sema &S, LookupResult &R,
                                                 DeclContext *StartDC) {
  assert(StartDC->isFileContext() && "start context is not a file context");

  DeclContext::udir_iterator I = StartDC->using_directives_begin();
  DeclContext::udir_iterator E = StartDC->using_directives_end();

  if (I == E) return false;

  // We have at least added all these contexts to the queue.
  llvm::SmallPtrSet<DeclContext*, 8> Visited;
  Visited.insert(StartDC);

  // We have not yet looked into these namespaces, much less added
  // their "using-children" to the queue.
  SmallVector<NamespaceDecl*, 8> Queue;

  // We have already looked into the initial namespace; seed the queue
  // with its using-children.
  for (; I != E; ++I) {
    NamespaceDecl *ND = (*I)->getNominatedNamespace()->getOriginalNamespace();
    if (Visited.insert(ND))
      Queue.push_back(ND);
  }

  // The easiest way to implement the restriction in [namespace.qual]p5
  // is to check whether any of the individual results found a tag
  // and, if so, to declare an ambiguity if the final result is not
  // a tag.
  bool FoundTag = false;
  bool FoundNonTag = false;

  LookupResult LocalR(LookupResult::Temporary, R);

  bool Found = false;
  while (!Queue.empty()) {
    NamespaceDecl *ND = Queue.back();
    Queue.pop_back();

    // We go through some convolutions here to avoid copying results
    // between LookupResults.
    bool UseLocal = !R.empty();
    LookupResult &DirectR = UseLocal ? LocalR : R;
    bool FoundDirect = LookupDirect(S, DirectR, ND);

    if (FoundDirect) {
      // First do any local hiding.
      DirectR.resolveKind();

      // If the local result is a tag, remember that.
      if (DirectR.isSingleTagDecl())
        FoundTag = true;
      else
        FoundNonTag = true;

      // Append the local results to the total results if necessary.
      if (UseLocal) {
        R.addAllDecls(LocalR);
        LocalR.clear();
      }
    }

    // If we find names in this namespace, ignore its using directives.
    if (FoundDirect) {
      Found = true;
      continue;
    }

    for (llvm::tie(I,E) = ND->getUsingDirectives(); I != E; ++I) {
      NamespaceDecl *Nom = (*I)->getNominatedNamespace();
      if (Visited.insert(Nom))
        Queue.push_back(Nom);
    }
  }

  if (Found) {
    if (FoundTag && FoundNonTag)
      R.setAmbiguousQualifiedTagHiding();
    else
      R.resolveKind();
  }

  return Found;
}

/// \brief Callback that looks for any member of a class with the given name.
static bool LookupAnyMember(const CXXBaseSpecifier *Specifier,
                            CXXBasePath &Path,
                            void *Name) {
  RecordDecl *BaseRecord = Specifier->getType()->getAs<RecordType>()->getDecl();

  DeclarationName N = DeclarationName::getFromOpaquePtr(Name);
  Path.Decls = BaseRecord->lookup(N);
  return !Path.Decls.empty();
}

/// \brief Determine whether the given set of member declarations contains only
/// static members, nested types, and enumerators.
template<typename InputIterator>
static bool HasOnlyStaticMembers(InputIterator First, InputIterator Last) {
  Decl *D = (*First)->getUnderlyingDecl();
  if (isa<VarDecl>(D) || isa<TypeDecl>(D) || isa<EnumConstantDecl>(D))
    return true;

  if (isa<CXXMethodDecl>(D)) {
    // Determine whether all of the methods are static.
    bool AllMethodsAreStatic = true;
    for(; First != Last; ++First) {
      D = (*First)->getUnderlyingDecl();

      if (!isa<CXXMethodDecl>(D)) {
        assert(isa<TagDecl>(D) && "Non-function must be a tag decl");
        break;
      }

      if (!cast<CXXMethodDecl>(D)->isStatic()) {
        AllMethodsAreStatic = false;
        break;
      }
    }

    if (AllMethodsAreStatic)
      return true;
  }

  return false;
}

/// \brief Perform qualified name lookup into a given context.
///
/// Qualified name lookup (C++ [basic.lookup.qual]) is used to find
/// names when the context of those names is explicit specified, e.g.,
/// "std::vector" or "x->member", or as part of unqualified name lookup.
///
/// Different lookup criteria can find different names. For example, a
/// particular scope can have both a struct and a function of the same
/// name, and each can be found by certain lookup criteria. For more
/// information about lookup criteria, see the documentation for the
/// class LookupCriteria.
///
/// \param R captures both the lookup criteria and any lookup results found.
///
/// \param LookupCtx The context in which qualified name lookup will
/// search. If the lookup criteria permits, name lookup may also search
/// in the parent contexts or (for C++ classes) base classes.
///
/// \param InUnqualifiedLookup true if this is qualified name lookup that
/// occurs as part of unqualified name lookup.
///
/// \returns true if lookup succeeded, false if it failed.
bool Sema::LookupQualifiedName(LookupResult &R, DeclContext *LookupCtx,
                               bool InUnqualifiedLookup) {
  assert(LookupCtx && "Sema::LookupQualifiedName requires a lookup context");

  if (!R.getLookupName())
    return false;

  // Make sure that the declaration context is complete.
  assert((!isa<TagDecl>(LookupCtx) ||
          LookupCtx->isDependentContext() ||
          cast<TagDecl>(LookupCtx)->isCompleteDefinition() ||
          cast<TagDecl>(LookupCtx)->isBeingDefined()) &&
         "Declaration context must already be complete!");

  // Perform qualified name lookup into the LookupCtx.
  if (LookupDirect(*this, R, LookupCtx)) {
    R.resolveKind();
    if (isa<CXXRecordDecl>(LookupCtx))
      R.setNamingClass(cast<CXXRecordDecl>(LookupCtx));
    return true;
  }

  // Don't descend into implied contexts for redeclarations.
  // C++98 [namespace.qual]p6:
  //   In a declaration for a namespace member in which the
  //   declarator-id is a qualified-id, given that the qualified-id
  //   for the namespace member has the form
  //     nested-name-specifier unqualified-id
  //   the unqualified-id shall name a member of the namespace
  //   designated by the nested-name-specifier.
  // See also [class.mfct]p5 and [class.static.data]p2.
  if (R.isForRedeclaration())
    return false;

  // If this is a namespace, look it up in the implied namespaces.
  if (LookupCtx->isFileContext())
    return LookupQualifiedNameInUsingDirectives(*this, R, LookupCtx);

  // If this isn't a C++ class, we aren't allowed to look into base
  // classes, we're done.
  CXXRecordDecl *LookupRec = dyn_cast<CXXRecordDecl>(LookupCtx);
  if (!LookupRec || !LookupRec->getDefinition())
    return false;

  // If we're performing qualified name lookup into a dependent class,
  // then we are actually looking into a current instantiation. If we have any
  // dependent base classes, then we either have to delay lookup until
  // template instantiation time (at which point all bases will be available)
  // or we have to fail.
  if (!InUnqualifiedLookup && LookupRec->isDependentContext() &&
      LookupRec->hasAnyDependentBases()) {
    R.setNotFoundInCurrentInstantiation();
    return false;
  }

  // Perform lookup into our base classes.
  CXXBasePaths Paths;
  Paths.setOrigin(LookupRec);

  // Look for this member in our base classes
  CXXRecordDecl::BaseMatchesCallback *BaseCallback = 0;
  switch (R.getLookupKind()) {
    case LookupObjCImplicitSelfParam:
    case LookupOrdinaryName:
    case LookupMemberName:
    case LookupRedeclarationWithLinkage:
      BaseCallback = &CXXRecordDecl::FindOrdinaryMember;
      break;

    case LookupTagName:
      BaseCallback = &CXXRecordDecl::FindTagMember;
      break;

    case LookupAnyName:
      BaseCallback = &LookupAnyMember;
      break;

    case LookupUsingDeclName:
      // This lookup is for redeclarations only.

    case LookupOperatorName:
    case LookupNamespaceName:
    case LookupObjCProtocolName:
    case LookupLabel:
      // These lookups will never find a member in a C++ class (or base class).
      return false;

    case LookupNestedNameSpecifierName:
      BaseCallback = &CXXRecordDecl::FindNestedNameSpecifierMember;
      break;
  }

  if (!LookupRec->lookupInBases(BaseCallback,
                                R.getLookupName().getAsOpaquePtr(), Paths))
    return false;

  R.setNamingClass(LookupRec);

  // C++ [class.member.lookup]p2:
  //   [...] If the resulting set of declarations are not all from
  //   sub-objects of the same type, or the set has a nonstatic member
  //   and includes members from distinct sub-objects, there is an
  //   ambiguity and the program is ill-formed. Otherwise that set is
  //   the result of the lookup.
  QualType SubobjectType;
  int SubobjectNumber = 0;
  AccessSpecifier SubobjectAccess = AS_none;

  for (CXXBasePaths::paths_iterator Path = Paths.begin(), PathEnd = Paths.end();
       Path != PathEnd; ++Path) {
    const CXXBasePathElement &PathElement = Path->back();

    // Pick the best (i.e. most permissive i.e. numerically lowest) access
    // across all paths.
    SubobjectAccess = std::min(SubobjectAccess, Path->Access);

    // Determine whether we're looking at a distinct sub-object or not.
    if (SubobjectType.isNull()) {
      // This is the first subobject we've looked at. Record its type.
      SubobjectType = Context.getCanonicalType(PathElement.Base->getType());
      SubobjectNumber = PathElement.SubobjectNumber;
      continue;
    }

    if (SubobjectType
                 != Context.getCanonicalType(PathElement.Base->getType())) {
      // We found members of the given name in two subobjects of
      // different types. If the declaration sets aren't the same, this
      // this lookup is ambiguous.
      if (HasOnlyStaticMembers(Path->Decls.begin(), Path->Decls.end())) {
        CXXBasePaths::paths_iterator FirstPath = Paths.begin();
        DeclContext::lookup_iterator FirstD = FirstPath->Decls.begin();
        DeclContext::lookup_iterator CurrentD = Path->Decls.begin();

        while (FirstD != FirstPath->Decls.end() &&
               CurrentD != Path->Decls.end()) {
         if ((*FirstD)->getUnderlyingDecl()->getCanonicalDecl() !=
             (*CurrentD)->getUnderlyingDecl()->getCanonicalDecl())
           break;

          ++FirstD;
          ++CurrentD;
        }

        if (FirstD == FirstPath->Decls.end() &&
            CurrentD == Path->Decls.end())
          continue;
      }

      R.setAmbiguousBaseSubobjectTypes(Paths);
      return true;
    }

    if (SubobjectNumber != PathElement.SubobjectNumber) {
      // We have a different subobject of the same type.

      // C++ [class.member.lookup]p5:
      //   A static member, a nested type or an enumerator defined in
      //   a base class T can unambiguously be found even if an object
      //   has more than one base class subobject of type T.
      if (HasOnlyStaticMembers(Path->Decls.begin(), Path->Decls.end()))
        continue;

      // We have found a nonstatic member name in multiple, distinct
      // subobjects. Name lookup is ambiguous.
      R.setAmbiguousBaseSubobjects(Paths);
      return true;
    }
  }

  // Lookup in a base class succeeded; return these results.

  DeclContext::lookup_result DR = Paths.front().Decls;
  for (DeclContext::lookup_iterator I = DR.begin(), E = DR.end(); I != E; ++I) {
    NamedDecl *D = *I;
    AccessSpecifier AS = CXXRecordDecl::MergeAccess(SubobjectAccess,
                                                    D->getAccess());
    R.addDecl(D, AS);
  }
  R.resolveKind();
  return true;
}

/// @brief Performs name lookup for a name that was parsed in the
/// source code, and may contain a C++ scope specifier.
///
/// This routine is a convenience routine meant to be called from
/// contexts that receive a name and an optional C++ scope specifier
/// (e.g., "N::M::x"). It will then perform either qualified or
/// unqualified name lookup (with LookupQualifiedName or LookupName,
/// respectively) on the given name and return those results.
///
/// @param S        The scope from which unqualified name lookup will
/// begin.
///
/// @param SS       An optional C++ scope-specifier, e.g., "::N::M".
///
/// @param EnteringContext Indicates whether we are going to enter the
/// context of the scope-specifier SS (if present).
///
/// @returns True if any decls were found (but possibly ambiguous)
bool Sema::LookupParsedName(LookupResult &R, Scope *S, CXXScopeSpec *SS,
                            bool AllowBuiltinCreation, bool EnteringContext) {
  if (SS && SS->isInvalid()) {
    // When the scope specifier is invalid, don't even look for
    // anything.
    return false;
  }

  if (SS && SS->isSet()) {
    if (DeclContext *DC = computeDeclContext(*SS, EnteringContext)) {
      // We have resolved the scope specifier to a particular declaration
      // contex, and will perform name lookup in that context.
      if (!DC->isDependentContext() && RequireCompleteDeclContext(*SS, DC))
        return false;

      R.setContextRange(SS->getRange());
      return LookupQualifiedName(R, DC);
    }

    // We could not resolve the scope specified to a specific declaration
    // context, which means that SS refers to an unknown specialization.
    // Name lookup can't find anything in this case.
    R.setNotFoundInCurrentInstantiation();
    R.setContextRange(SS->getRange());
    return false;
  }

  // Perform unqualified name lookup starting in the given scope.
  return LookupName(R, S, AllowBuiltinCreation);
}


/// \brief Produce a diagnostic describing the ambiguity that resulted
/// from name lookup.
///
/// \param Result The result of the ambiguous lookup to be diagnosed.
///
/// \returns true
bool Sema::DiagnoseAmbiguousLookup(LookupResult &Result) {
  assert(Result.isAmbiguous() && "Lookup result must be ambiguous");

  DeclarationName Name = Result.getLookupName();
  SourceLocation NameLoc = Result.getNameLoc();
  SourceRange LookupRange = Result.getContextRange();

  switch (Result.getAmbiguityKind()) {
  case LookupResult::AmbiguousBaseSubobjects: {
    CXXBasePaths *Paths = Result.getBasePaths();
    QualType SubobjectType = Paths->front().back().Base->getType();
    Diag(NameLoc, diag::err_ambiguous_member_multiple_subobjects)
      << Name << SubobjectType << getAmbiguousPathsDisplayString(*Paths)
      << LookupRange;

    DeclContext::lookup_iterator Found = Paths->front().Decls.begin();
    while (isa<CXXMethodDecl>(*Found) &&
           cast<CXXMethodDecl>(*Found)->isStatic())
      ++Found;

    Diag((*Found)->getLocation(), diag::note_ambiguous_member_found);

    return true;
  }

  case LookupResult::AmbiguousBaseSubobjectTypes: {
    Diag(NameLoc, diag::err_ambiguous_member_multiple_subobject_types)
      << Name << LookupRange;

    CXXBasePaths *Paths = Result.getBasePaths();
    std::set<Decl *> DeclsPrinted;
    for (CXXBasePaths::paths_iterator Path = Paths->begin(),
                                      PathEnd = Paths->end();
         Path != PathEnd; ++Path) {
      Decl *D = Path->Decls.front();
      if (DeclsPrinted.insert(D).second)
        Diag(D->getLocation(), diag::note_ambiguous_member_found);
    }

    return true;
  }

  case LookupResult::AmbiguousTagHiding: {
    Diag(NameLoc, diag::err_ambiguous_tag_hiding) << Name << LookupRange;

    llvm::SmallPtrSet<NamedDecl*,8> TagDecls;

    LookupResult::iterator DI, DE = Result.end();
    for (DI = Result.begin(); DI != DE; ++DI)
      if (TagDecl *TD = dyn_cast<TagDecl>(*DI)) {
        TagDecls.insert(TD);
        Diag(TD->getLocation(), diag::note_hidden_tag);
      }

    for (DI = Result.begin(); DI != DE; ++DI)
      if (!isa<TagDecl>(*DI))
        Diag((*DI)->getLocation(), diag::note_hiding_object);

    // For recovery purposes, go ahead and implement the hiding.
    LookupResult::Filter F = Result.makeFilter();
    while (F.hasNext()) {
      if (TagDecls.count(F.next()))
        F.erase();
    }
    F.done();

    return true;
  }

  case LookupResult::AmbiguousReference: {
    Diag(NameLoc, diag::err_ambiguous_reference) << Name << LookupRange;

    LookupResult::iterator DI = Result.begin(), DE = Result.end();
    for (; DI != DE; ++DI)
      Diag((*DI)->getLocation(), diag::note_ambiguous_candidate) << *DI;

    return true;
  }
  }

  llvm_unreachable("unknown ambiguity kind");
}

NamedDecl *Sema::LookupSingleName(Scope *S, DeclarationName Name,
                                  SourceLocation Loc,
                                  LookupNameKind NameKind,
                                  RedeclarationKind Redecl) {
  LookupResult R(*this, Name, Loc, NameKind, Redecl);
  LookupName(R, S);
  return R.getAsSingle<NamedDecl>();
}

//----------------------------------------------------------------------------
// Search for all visible declarations.
//----------------------------------------------------------------------------
VisibleDeclConsumer::~VisibleDeclConsumer() { }

namespace {

class ShadowContextRAII;

class VisibleDeclsRecord {
public:
  /// \brief An entry in the shadow map, which is optimized to store a
  /// single declaration (the common case) but can also store a list
  /// of declarations.
  typedef llvm::TinyPtrVector<NamedDecl*> ShadowMapEntry;

private:
  /// \brief A mapping from declaration names to the declarations that have
  /// this name within a particular scope.
  typedef llvm::DenseMap<DeclarationName, ShadowMapEntry> ShadowMap;

  /// \brief A list of shadow maps, which is used to model name hiding.
  std::list<ShadowMap> ShadowMaps;

  /// \brief The declaration contexts we have already visited.
  llvm::SmallPtrSet<DeclContext *, 8> VisitedContexts;

  friend class ShadowContextRAII;

public:
  /// \brief Determine whether we have already visited this context
  /// (and, if not, note that we are going to visit that context now).
  bool visitedContext(DeclContext *Ctx) {
    return !VisitedContexts.insert(Ctx);
  }

  bool alreadyVisitedContext(DeclContext *Ctx) {
    return VisitedContexts.count(Ctx);
  }

  /// \brief Determine whether the given declaration is hidden in the
  /// current scope.
  ///
  /// \returns the declaration that hides the given declaration, or
  /// NULL if no such declaration exists.
  NamedDecl *checkHidden(NamedDecl *ND);

  /// \brief Add a declaration to the current shadow map.
  void add(NamedDecl *ND) {
    ShadowMaps.back()[ND->getDeclName()].push_back(ND);
  }
};

/// \brief RAII object that records when we've entered a shadow context.
class ShadowContextRAII {
  VisibleDeclsRecord &Visible;

  typedef VisibleDeclsRecord::ShadowMap ShadowMap;

public:
  ShadowContextRAII(VisibleDeclsRecord &Visible) : Visible(Visible) {
    Visible.ShadowMaps.push_back(ShadowMap());
  }

  ~ShadowContextRAII() {
    Visible.ShadowMaps.pop_back();
  }
};

} // end anonymous namespace

NamedDecl *VisibleDeclsRecord::checkHidden(NamedDecl *ND) {
  // Look through using declarations.
  ND = ND->getUnderlyingDecl();

  unsigned IDNS = ND->getIdentifierNamespace();
  std::list<ShadowMap>::reverse_iterator SM = ShadowMaps.rbegin();
  for (std::list<ShadowMap>::reverse_iterator SMEnd = ShadowMaps.rend();
       SM != SMEnd; ++SM) {
    ShadowMap::iterator Pos = SM->find(ND->getDeclName());
    if (Pos == SM->end())
      continue;

    for (ShadowMapEntry::iterator I = Pos->second.begin(),
                               IEnd = Pos->second.end();
         I != IEnd; ++I) {
      // A tag declaration does not hide a non-tag declaration.
      if ((*I)->hasTagIdentifierNamespace() &&
          (IDNS & (Decl::IDNS_Member | Decl::IDNS_Ordinary |
                   Decl::IDNS_ObjCProtocol)))
        continue;

      // Protocols are in distinct namespaces from everything else.
      if ((((*I)->getIdentifierNamespace() & Decl::IDNS_ObjCProtocol)
           || (IDNS & Decl::IDNS_ObjCProtocol)) &&
          (*I)->getIdentifierNamespace() != IDNS)
        continue;

      // Functions and function templates in the same scope overload
      // rather than hide.  FIXME: Look for hiding based on function
      // signatures!
      if ((*I)->isFunctionOrFunctionTemplate() &&
          ND->isFunctionOrFunctionTemplate() &&
          SM == ShadowMaps.rbegin())
        continue;

      // We've found a declaration that hides this one.
      return *I;
    }
  }

  return 0;
}

static void LookupVisibleDecls(DeclContext *Ctx, LookupResult &Result,
                               bool QualifiedNameLookup,
                               bool InBaseClass,
                               VisibleDeclConsumer &Consumer,
                               VisibleDeclsRecord &Visited) {
  if (!Ctx)
    return;

  // Make sure we don't visit the same context twice.
  if (Visited.visitedContext(Ctx->getPrimaryContext()))
    return;

  // Enumerate all of the results in this context.
  for (DeclContext::all_lookups_iterator L = Ctx->lookups_begin(),
                                      LEnd = Ctx->lookups_end();
       L != LEnd; ++L) {
    DeclContext::lookup_result R = *L;
    for (DeclContext::lookup_iterator I = R.begin(), E = R.end(); I != E;
         ++I) {
      if (NamedDecl *ND = dyn_cast<NamedDecl>(*I)) {
        if ((ND = Result.getAcceptableDecl(ND))) {
          Consumer.FoundDecl(ND, Visited.checkHidden(ND), Ctx, InBaseClass);
          Visited.add(ND);
        }
      }
    }
  }

  // Traverse using directives for qualified name lookup.
  if (QualifiedNameLookup) {
    ShadowContextRAII Shadow(Visited);
    DeclContext::udir_iterator I, E;
    for (llvm::tie(I, E) = Ctx->getUsingDirectives(); I != E; ++I) {
      LookupVisibleDecls((*I)->getNominatedNamespace(), Result,
                         QualifiedNameLookup, InBaseClass, Consumer, Visited);
    }
  }

  // Traverse the contexts of inherited C++ classes.
  if (CXXRecordDecl *Record = dyn_cast<CXXRecordDecl>(Ctx)) {
    if (!Record->hasDefinition())
      return;

    for (CXXRecordDecl::base_class_iterator B = Record->bases_begin(),
                                         BEnd = Record->bases_end();
         B != BEnd; ++B) {
      QualType BaseType = B->getType();

      // Don't look into dependent bases, because name lookup can't look
      // there anyway.
      if (BaseType->isDependentType())
        continue;

      const RecordType *Record = BaseType->getAs<RecordType>();
      if (!Record)
        continue;

      // FIXME: It would be nice to be able to determine whether referencing
      // a particular member would be ambiguous. For example, given
      //
      //   struct A { int member; };
      //   struct B { int member; };
      //   struct C : A, B { };
      //
      //   void f(C *c) { c->### }
      //
      // accessing 'member' would result in an ambiguity. However, we
      // could be smart enough to qualify the member with the base
      // class, e.g.,
      //
      //   c->B::member
      //
      // or
      //
      //   c->A::member

      // Find results in this base class (and its bases).
      ShadowContextRAII Shadow(Visited);
      LookupVisibleDecls(Record->getDecl(), Result, QualifiedNameLookup,
                         true, Consumer, Visited);
    }
  }
}

static void LookupVisibleDecls(Scope *S, LookupResult &Result,
                               UnqualUsingDirectiveSet &UDirs,
                               VisibleDeclConsumer &Consumer,
                               VisibleDeclsRecord &Visited) {
  if (!S)
    return;

  if (!S->getEntity() ||
      (!S->getParent() &&
       !Visited.alreadyVisitedContext((DeclContext *)S->getEntity())) ||
      ((DeclContext *)S->getEntity())->isFunctionOrMethod()) {
    // Walk through the declarations in this Scope.
    for (Scope::decl_iterator D = S->decl_begin(), DEnd = S->decl_end();
         D != DEnd; ++D) {
      if (NamedDecl *ND = dyn_cast<NamedDecl>(*D))
        if ((ND = Result.getAcceptableDecl(ND))) {
          Consumer.FoundDecl(ND, Visited.checkHidden(ND), 0, false);
          Visited.add(ND);
        }
    }
  }

  // FIXME: C++ [temp.local]p8
  DeclContext *Entity = 0;
  if (S->getEntity()) {
    // Look into this scope's declaration context, along with any of its
    // parent lookup contexts (e.g., enclosing classes), up to the point
    // where we hit the context stored in the next outer scope.
    Entity = (DeclContext *)S->getEntity();
    DeclContext *OuterCtx = findOuterContext(S).first; // FIXME

    for (DeclContext *Ctx = Entity; Ctx && !Ctx->Equals(OuterCtx);
         Ctx = Ctx->getLookupParent()) {
      if (Ctx->isFunctionOrMethod())
        continue;

      LookupVisibleDecls(Ctx, Result, /*QualifiedNameLookup=*/false,
                         /*InBaseClass=*/false, Consumer, Visited);
    }
  } else if (!S->getParent()) {
    // Look into the translation unit scope. We walk through the translation
    // unit's declaration context, because the Scope itself won't have all of
    // the declarations if we loaded a precompiled header.
    // FIXME: We would like the translation unit's Scope object to point to the
    // translation unit, so we don't need this special "if" branch. However,
    // doing so would force the normal C++ name-lookup code to look into the
    // translation unit decl when the IdentifierInfo chains would suffice.
    // Once we fix that problem (which is part of a more general "don't look
    // in DeclContexts unless we have to" optimization), we can eliminate this.
    Entity = Result.getSema().Context.getTranslationUnitDecl();
    LookupVisibleDecls(Entity, Result, /*QualifiedNameLookup=*/false,
                       /*InBaseClass=*/false, Consumer, Visited);
  }

  if (Entity) {
    // Lookup visible declarations in any namespaces found by using
    // directives.
    UnqualUsingDirectiveSet::const_iterator UI, UEnd;
    llvm::tie(UI, UEnd) = UDirs.getNamespacesFor(Entity);
    for (; UI != UEnd; ++UI)
      LookupVisibleDecls(const_cast<DeclContext *>(UI->getNominatedNamespace()),
                         Result, /*QualifiedNameLookup=*/false,
                         /*InBaseClass=*/false, Consumer, Visited);
  }

  // Lookup names in the parent scope.
  ShadowContextRAII Shadow(Visited);
  LookupVisibleDecls(S->getParent(), Result, UDirs, Consumer, Visited);
}

void Sema::LookupVisibleDecls(Scope *S, LookupNameKind Kind,
                              VisibleDeclConsumer &Consumer,
                              bool IncludeGlobalScope) {
  // Determine the set of using directives available during
  // unqualified name lookup.
  Scope *Initial = S;
  UnqualUsingDirectiveSet UDirs;
  if (getLangOpts().CPlusPlus) {
    // Find the first namespace or translation-unit scope.
    while (S && !isNamespaceOrTranslationUnitScope(S))
      S = S->getParent();

    UDirs.visitScopeChain(Initial, S);
  }
  UDirs.done();

  // Look for visible declarations.
  LookupResult Result(*this, DeclarationName(), SourceLocation(), Kind);
  VisibleDeclsRecord Visited;
  if (!IncludeGlobalScope)
    Visited.visitedContext(Context.getTranslationUnitDecl());
  ShadowContextRAII Shadow(Visited);
  ::LookupVisibleDecls(Initial, Result, UDirs, Consumer, Visited);
}

void Sema::LookupVisibleDecls(DeclContext *Ctx, LookupNameKind Kind,
                              VisibleDeclConsumer &Consumer,
                              bool IncludeGlobalScope) {
  LookupResult Result(*this, DeclarationName(), SourceLocation(), Kind);
  VisibleDeclsRecord Visited;
  if (!IncludeGlobalScope)
    Visited.visitedContext(Context.getTranslationUnitDecl());
  ShadowContextRAII Shadow(Visited);
  ::LookupVisibleDecls(Ctx, Result, /*QualifiedNameLookup=*/true,
                       /*InBaseClass=*/false, Consumer, Visited);
}

/// LookupOrCreateLabel - Do a name lookup of a label with the specified name.
/// If GnuLabelLoc is a valid source location, then this is a definition
/// of an __label__ label name, otherwise it is a normal label definition
/// or use.
LabelDecl *Sema::LookupOrCreateLabel(IdentifierInfo *II, SourceLocation Loc,
                                     SourceLocation GnuLabelLoc) {
  // Do a lookup to see if we have a label with this name already.
  NamedDecl *Res = 0;

  if (GnuLabelLoc.isValid()) {
    // Local label definitions always shadow existing labels.
    Res = LabelDecl::Create(Context, CurContext, Loc, II, GnuLabelLoc);
    Scope *S = CurScope;
    PushOnScopeChains(Res, S, true);
    return cast<LabelDecl>(Res);
  }

  // Not a GNU local label.
  Res = LookupSingleName(CurScope, II, Loc, LookupLabel, NotForRedeclaration);
  // If we found a label, check to see if it is in the same context as us.
  // When in a Block, we don't want to reuse a label in an enclosing function.
  if (Res && Res->getDeclContext() != CurContext)
    Res = 0;
  if (Res == 0) {
    // If not forward referenced or defined already, create the backing decl.
    Res = LabelDecl::Create(Context, CurContext, Loc, II);
    Scope *S = CurScope->getFnParent();
    assert(S && "Not in a function?");
    PushOnScopeChains(Res, S, true);
  }
  return cast<LabelDecl>(Res);
}

//===----------------------------------------------------------------------===//
// Typo correction
//===----------------------------------------------------------------------===//

namespace {

typedef llvm::SmallVector<TypoCorrection, 1> TypoResultList;
typedef llvm::StringMap<TypoResultList, llvm::BumpPtrAllocator> TypoResultsMap;
typedef std::map<unsigned, TypoResultsMap> TypoEditDistanceMap;

static const unsigned MaxTypoDistanceResultSets = 5;

class TypoCorrectionConsumer : public VisibleDeclConsumer {
  /// \brief The name written that is a typo in the source.
  StringRef Typo;

  /// \brief The results found that have the smallest edit distance
  /// found (so far) with the typo name.
  ///
  /// The pointer value being set to the current DeclContext indicates
  /// whether there is a keyword with this name.
  TypoEditDistanceMap CorrectionResults;

  Sema &SemaRef;

public:
  explicit TypoCorrectionConsumer(Sema &SemaRef, IdentifierInfo *Typo)
    : Typo(Typo->getName()),
      SemaRef(SemaRef) { }

  virtual void FoundDecl(NamedDecl *ND, NamedDecl *Hiding, DeclContext *Ctx,
                         bool InBaseClass);
  void FoundName(StringRef Name);
  void addKeywordResult(StringRef Keyword);
  void addName(StringRef Name, NamedDecl *ND, unsigned Distance,
               NestedNameSpecifier *NNS=NULL, bool isKeyword=false);
  void addCorrection(TypoCorrection Correction);

  typedef TypoResultsMap::iterator result_iterator;
  typedef TypoEditDistanceMap::iterator distance_iterator;
  distance_iterator begin() { return CorrectionResults.begin(); }
  distance_iterator end()  { return CorrectionResults.end(); }
  void erase(distance_iterator I) { CorrectionResults.erase(I); }
  unsigned size() const { return CorrectionResults.size(); }
  bool empty() const { return CorrectionResults.empty(); }

  TypoResultList &operator[](StringRef Name) {
    return CorrectionResults.begin()->second[Name];
  }

  unsigned getBestEditDistance(bool Normalized) {
    if (CorrectionResults.empty())
      return (std::numeric_limits<unsigned>::max)();

    unsigned BestED = CorrectionResults.begin()->first;
    return Normalized ? TypoCorrection::NormalizeEditDistance(BestED) : BestED;
  }

  TypoResultsMap &getBestResults() {
    return CorrectionResults.begin()->second;
  }

};

}

void TypoCorrectionConsumer::FoundDecl(NamedDecl *ND, NamedDecl *Hiding,
                                       DeclContext *Ctx, bool InBaseClass) {
  // Don't consider hidden names for typo correction.
  if (Hiding)
    return;

  // Only consider entities with identifiers for names, ignoring
  // special names (constructors, overloaded operators, selectors,
  // etc.).
  IdentifierInfo *Name = ND->getIdentifier();
  if (!Name)
    return;

  FoundName(Name->getName());
}

void TypoCorrectionConsumer::FoundName(StringRef Name) {
  // Use a simple length-based heuristic to determine the minimum possible
  // edit distance. If the minimum isn't good enough, bail out early.
  unsigned MinED = abs((int)Name.size() - (int)Typo.size());
  if (MinED && Typo.size() / MinED < 3)
    return;

  // Compute an upper bound on the allowable edit distance, so that the
  // edit-distance algorithm can short-circuit.
  unsigned UpperBound = (Typo.size() + 2) / 3;

  // Compute the edit distance between the typo and the name of this
  // entity, and add the identifier to the list of results.
  addName(Name, NULL, Typo.edit_distance(Name, true, UpperBound));
}

void TypoCorrectionConsumer::addKeywordResult(StringRef Keyword) {
  // Compute the edit distance between the typo and this keyword,
  // and add the keyword to the list of results.
  addName(Keyword, NULL, Typo.edit_distance(Keyword), NULL, true);
}

void TypoCorrectionConsumer::addName(StringRef Name,
                                     NamedDecl *ND,
                                     unsigned Distance,
                                     NestedNameSpecifier *NNS,
                                     bool isKeyword) {
  TypoCorrection TC(&SemaRef.Context.Idents.get(Name), ND, NNS, Distance);
  if (isKeyword) TC.makeKeyword();
  addCorrection(TC);
}

void TypoCorrectionConsumer::addCorrection(TypoCorrection Correction) {
  StringRef Name = Correction.getCorrectionAsIdentifierInfo()->getName();
  TypoResultList &CList =
      CorrectionResults[Correction.getEditDistance(false)][Name];

  if (!CList.empty() && !CList.back().isResolved())
    CList.pop_back();
  if (NamedDecl *NewND = Correction.getCorrectionDecl()) {
    std::string CorrectionStr = Correction.getAsString(SemaRef.getLangOpts());
    for (TypoResultList::iterator RI = CList.begin(), RIEnd = CList.end();
         RI != RIEnd; ++RI) {
      // If the Correction refers to a decl already in the result list,
      // replace the existing result if the string representation of Correction
      // comes before the current result alphabetically, then stop as there is
      // nothing more to be done to add Correction to the candidate set.
      if (RI->getCorrectionDecl() == NewND) {
        if (CorrectionStr < RI->getAsString(SemaRef.getLangOpts()))
          *RI = Correction;
        return;
      }
    }
  }
  if (CList.empty() || Correction.isResolved())
    CList.push_back(Correction);

  while (CorrectionResults.size() > MaxTypoDistanceResultSets)
    erase(llvm::prior(CorrectionResults.end()));
}

// Fill the supplied vector with the IdentifierInfo pointers for each piece of
// the given NestedNameSpecifier (i.e. given a NestedNameSpecifier "foo::bar::",
// fill the vector with the IdentifierInfo pointers for "foo" and "bar").
static void getNestedNameSpecifierIdentifiers(
    NestedNameSpecifier *NNS,
    SmallVectorImpl<const IdentifierInfo*> &Identifiers) {
  if (NestedNameSpecifier *Prefix = NNS->getPrefix())
    getNestedNameSpecifierIdentifiers(Prefix, Identifiers);
  else
    Identifiers.clear();

  const IdentifierInfo *II = NULL;

  switch (NNS->getKind()) {
  case NestedNameSpecifier::Identifier:
    II = NNS->getAsIdentifier();
    break;

  case NestedNameSpecifier::Namespace:
    if (NNS->getAsNamespace()->isAnonymousNamespace())
      return;
    II = NNS->getAsNamespace()->getIdentifier();
    break;

  case NestedNameSpecifier::NamespaceAlias:
    II = NNS->getAsNamespaceAlias()->getIdentifier();
    break;

  case NestedNameSpecifier::TypeSpecWithTemplate:
  case NestedNameSpecifier::TypeSpec:
    II = QualType(NNS->getAsType(), 0).getBaseTypeIdentifier();
    break;

  case NestedNameSpecifier::Global:
    return;
  }

  if (II)
    Identifiers.push_back(II);
}

namespace {

class SpecifierInfo {
 public:
  DeclContext* DeclCtx;
  NestedNameSpecifier* NameSpecifier;
  unsigned EditDistance;

  SpecifierInfo(DeclContext *Ctx, NestedNameSpecifier *NNS, unsigned ED)
      : DeclCtx(Ctx), NameSpecifier(NNS), EditDistance(ED) {}
};

typedef SmallVector<DeclContext*, 4> DeclContextList;
typedef SmallVector<SpecifierInfo, 16> SpecifierInfoList;

class NamespaceSpecifierSet {
  ASTContext &Context;
  DeclContextList CurContextChain;
  SmallVector<const IdentifierInfo*, 4> CurContextIdentifiers;
  SmallVector<const IdentifierInfo*, 4> CurNameSpecifierIdentifiers;
  bool isSorted;

  SpecifierInfoList Specifiers;
  llvm::SmallSetVector<unsigned, 4> Distances;
  llvm::DenseMap<unsigned, SpecifierInfoList> DistanceMap;

  /// \brief Helper for building the list of DeclContexts between the current
  /// context and the top of the translation unit
  static DeclContextList BuildContextChain(DeclContext *Start);

  void SortNamespaces();

 public:
  NamespaceSpecifierSet(ASTContext &Context, DeclContext *CurContext,
                        CXXScopeSpec *CurScopeSpec)
      : Context(Context), CurContextChain(BuildContextChain(CurContext)),
        isSorted(true) {
    if (CurScopeSpec && CurScopeSpec->getScopeRep())
      getNestedNameSpecifierIdentifiers(CurScopeSpec->getScopeRep(),
                                        CurNameSpecifierIdentifiers);
    // Build the list of identifiers that would be used for an absolute
    // (from the global context) NestedNameSpecifier referring to the current
    // context.
    for (DeclContextList::reverse_iterator C = CurContextChain.rbegin(),
                                        CEnd = CurContextChain.rend();
         C != CEnd; ++C) {
      if (NamespaceDecl *ND = dyn_cast_or_null<NamespaceDecl>(*C))
        CurContextIdentifiers.push_back(ND->getIdentifier());
    }
  }

  /// \brief Add the namespace to the set, computing the corresponding
  /// NestedNameSpecifier and its distance in the process.
  void AddNamespace(NamespaceDecl *ND);

  typedef SpecifierInfoList::iterator iterator;
  iterator begin() {
    if (!isSorted) SortNamespaces();
    return Specifiers.begin();
  }
  iterator end() { return Specifiers.end(); }
};

}

DeclContextList NamespaceSpecifierSet::BuildContextChain(DeclContext *Start) {
  assert(Start && "Bulding a context chain from a null context");
  DeclContextList Chain;
  for (DeclContext *DC = Start->getPrimaryContext(); DC != NULL;
       DC = DC->getLookupParent()) {
    NamespaceDecl *ND = dyn_cast_or_null<NamespaceDecl>(DC);
    if (!DC->isInlineNamespace() && !DC->isTransparentContext() &&
        !(ND && ND->isAnonymousNamespace()))
      Chain.push_back(DC->getPrimaryContext());
  }
  return Chain;
}

void NamespaceSpecifierSet::SortNamespaces() {
  SmallVector<unsigned, 4> sortedDistances;
  sortedDistances.append(Distances.begin(), Distances.end());

  if (sortedDistances.size() > 1)
    std::sort(sortedDistances.begin(), sortedDistances.end());

  Specifiers.clear();
  for (SmallVector<unsigned, 4>::iterator DI = sortedDistances.begin(),
                                       DIEnd = sortedDistances.end();
       DI != DIEnd; ++DI) {
    SpecifierInfoList &SpecList = DistanceMap[*DI];
    Specifiers.append(SpecList.begin(), SpecList.end());
  }

  isSorted = true;
}

void NamespaceSpecifierSet::AddNamespace(NamespaceDecl *ND) {
  DeclContext *Ctx = cast<DeclContext>(ND);
  NestedNameSpecifier *NNS = NULL;
  unsigned NumSpecifiers = 0;
  DeclContextList NamespaceDeclChain(BuildContextChain(Ctx));
  DeclContextList FullNamespaceDeclChain(NamespaceDeclChain);

  // Eliminate common elements from the two DeclContext chains.
  for (DeclContextList::reverse_iterator C = CurContextChain.rbegin(),
                                      CEnd = CurContextChain.rend();
       C != CEnd && !NamespaceDeclChain.empty() &&
       NamespaceDeclChain.back() == *C; ++C) {
    NamespaceDeclChain.pop_back();
  }

  // Add an explicit leading '::' specifier if needed.
  if (NamespaceDecl *ND =
        NamespaceDeclChain.empty() ? NULL :
          dyn_cast_or_null<NamespaceDecl>(NamespaceDeclChain.back())) {
    IdentifierInfo *Name = ND->getIdentifier();
    if (std::find(CurContextIdentifiers.begin(), CurContextIdentifiers.end(),
                  Name) != CurContextIdentifiers.end() ||
        std::find(CurNameSpecifierIdentifiers.begin(),
                  CurNameSpecifierIdentifiers.end(),
                  Name) != CurNameSpecifierIdentifiers.end()) {
      NamespaceDeclChain = FullNamespaceDeclChain;
      NNS = NestedNameSpecifier::GlobalSpecifier(Context);
    }
  }

  // Build the NestedNameSpecifier from what is left of the NamespaceDeclChain
  for (DeclContextList::reverse_iterator C = NamespaceDeclChain.rbegin(),
                                      CEnd = NamespaceDeclChain.rend();
       C != CEnd; ++C) {
    NamespaceDecl *ND = dyn_cast_or_null<NamespaceDecl>(*C);
    if (ND) {
      NNS = NestedNameSpecifier::Create(Context, NNS, ND);
      ++NumSpecifiers;
    }
  }

  // If the built NestedNameSpecifier would be replacing an existing
  // NestedNameSpecifier, use the number of component identifiers that
  // would need to be changed as the edit distance instead of the number
  // of components in the built NestedNameSpecifier.
  if (NNS && !CurNameSpecifierIdentifiers.empty()) {
    SmallVector<const IdentifierInfo*, 4> NewNameSpecifierIdentifiers;
    getNestedNameSpecifierIdentifiers(NNS, NewNameSpecifierIdentifiers);
    NumSpecifiers = llvm::ComputeEditDistance(
      llvm::ArrayRef<const IdentifierInfo*>(CurNameSpecifierIdentifiers),
      llvm::ArrayRef<const IdentifierInfo*>(NewNameSpecifierIdentifiers));
  }

  isSorted = false;
  Distances.insert(NumSpecifiers);
  DistanceMap[NumSpecifiers].push_back(SpecifierInfo(Ctx, NNS, NumSpecifiers));
}

/// \brief Perform name lookup for a possible result for typo correction.
static void LookupPotentialTypoResult(Sema &SemaRef,
                                      LookupResult &Res,
                                      IdentifierInfo *Name,
                                      Scope *S, CXXScopeSpec *SS,
                                      DeclContext *MemberContext,
                                      bool EnteringContext,
                                      bool isObjCIvarLookup) {
  Res.suppressDiagnostics();
  Res.clear();
  Res.setLookupName(Name);
  if (MemberContext) {
    SemaRef.LookupQualifiedName(Res, MemberContext);
    return;
  }

  SemaRef.LookupParsedName(Res, S, SS, /*AllowBuiltinCreation=*/false,
                           EnteringContext);
}

/// \brief Add keywords to the consumer as possible typo corrections.
static void AddKeywordsToConsumer(Sema &SemaRef,
                                  TypoCorrectionConsumer &Consumer,
                                  Scope *S, CorrectionCandidateCallback &CCC,
                                  bool AfterNestedNameSpecifier) {
  if (AfterNestedNameSpecifier) {
    // For 'X::', we know exactly which keywords can appear next.
    Consumer.addKeywordResult("template");
    if (CCC.WantExpressionKeywords)
      Consumer.addKeywordResult("operator");
    return;
  }

  if (CCC.WantObjCSuper)
    Consumer.addKeywordResult("super");

  if (CCC.WantTypeSpecifiers) {
    // Add type-specifier keywords to the set of results.
    const char *CTypeSpecs[] = {
      "char", "const", "double", "enum", "float", "int", "long", "short",
      "signed", "struct", "union", "unsigned", "void", "volatile", 
      "_Complex", "_Imaginary",
      // storage-specifiers as well
      "extern", "inline", "static", "typedef"
    };

    const unsigned NumCTypeSpecs = sizeof(CTypeSpecs) / sizeof(CTypeSpecs[0]);
    for (unsigned I = 0; I != NumCTypeSpecs; ++I)
      Consumer.addKeywordResult(CTypeSpecs[I]);

    if (SemaRef.getLangOpts().C99)
      Consumer.addKeywordResult("restrict");
    if (SemaRef.getLangOpts().Bool || SemaRef.getLangOpts().CPlusPlus)
      Consumer.addKeywordResult("bool");
    else if (SemaRef.getLangOpts().C99)
      Consumer.addKeywordResult("_Bool");
    
    if (SemaRef.getLangOpts().CPlusPlus) {
      Consumer.addKeywordResult("class");
      Consumer.addKeywordResult("typename");
      Consumer.addKeywordResult("wchar_t");

      if (SemaRef.getLangOpts().CPlusPlus0x) {
        Consumer.addKeywordResult("char16_t");
        Consumer.addKeywordResult("char32_t");
        Consumer.addKeywordResult("constexpr");
        Consumer.addKeywordResult("decltype");
        Consumer.addKeywordResult("thread_local");
      }
    }

    if (SemaRef.getLangOpts().GNUMode)
      Consumer.addKeywordResult("typeof");
  }

  if (CCC.WantCXXNamedCasts && SemaRef.getLangOpts().CPlusPlus) {
    Consumer.addKeywordResult("const_cast");
    Consumer.addKeywordResult("dynamic_cast");
    Consumer.addKeywordResult("reinterpret_cast");
    Consumer.addKeywordResult("static_cast");
  }

  if (CCC.WantExpressionKeywords) {
    Consumer.addKeywordResult("sizeof");
    if (SemaRef.getLangOpts().Bool || SemaRef.getLangOpts().CPlusPlus) {
      Consumer.addKeywordResult("false");
      Consumer.addKeywordResult("true");
    }

    if (SemaRef.getLangOpts().CPlusPlus) {
      const char *CXXExprs[] = {
        "delete", "new", "operator", "throw", "typeid"
      };
      const unsigned NumCXXExprs = sizeof(CXXExprs) / sizeof(CXXExprs[0]);
      for (unsigned I = 0; I != NumCXXExprs; ++I)
        Consumer.addKeywordResult(CXXExprs[I]);

      if (isa<CXXMethodDecl>(SemaRef.CurContext) &&
          cast<CXXMethodDecl>(SemaRef.CurContext)->isInstance())
        Consumer.addKeywordResult("this");

      if (SemaRef.getLangOpts().CPlusPlus0x) {
        Consumer.addKeywordResult("alignof");
        Consumer.addKeywordResult("nullptr");
      }
    }

    if (SemaRef.getLangOpts().C11) {
      // FIXME: We should not suggest _Alignof if the alignof macro
      // is present.
      Consumer.addKeywordResult("_Alignof");
    }
  }

  if (CCC.WantRemainingKeywords) {
    if (SemaRef.getCurFunctionOrMethodDecl() || SemaRef.getCurBlock()) {
      // Statements.
      const char *CStmts[] = {
        "do", "else", "for", "goto", "if", "return", "switch", "while" };
      const unsigned NumCStmts = sizeof(CStmts) / sizeof(CStmts[0]);
      for (unsigned I = 0; I != NumCStmts; ++I)
        Consumer.addKeywordResult(CStmts[I]);

      if (SemaRef.getLangOpts().CPlusPlus) {
        Consumer.addKeywordResult("catch");
        Consumer.addKeywordResult("try");
      }

      if (S && S->getBreakParent())
        Consumer.addKeywordResult("break");

      if (S && S->getContinueParent())
        Consumer.addKeywordResult("continue");

      if (!SemaRef.getCurFunction()->SwitchStack.empty()) {
        Consumer.addKeywordResult("case");
        Consumer.addKeywordResult("default");
      }
    } else {
      if (SemaRef.getLangOpts().CPlusPlus) {
        Consumer.addKeywordResult("namespace");
        Consumer.addKeywordResult("template");
      }

      if (S && S->isClassScope()) {
        Consumer.addKeywordResult("explicit");
        Consumer.addKeywordResult("friend");
        Consumer.addKeywordResult("mutable");
        Consumer.addKeywordResult("private");
        Consumer.addKeywordResult("protected");
        Consumer.addKeywordResult("public");
        Consumer.addKeywordResult("virtual");
      }
    }

    if (SemaRef.getLangOpts().CPlusPlus) {
      Consumer.addKeywordResult("using");

      if (SemaRef.getLangOpts().CPlusPlus0x)
        Consumer.addKeywordResult("static_assert");
    }
  }
}

static bool isCandidateViable(CorrectionCandidateCallback &CCC,
                              TypoCorrection &Candidate) {
  Candidate.setCallbackDistance(CCC.RankCandidate(Candidate));
  return Candidate.getEditDistance(false) != TypoCorrection::InvalidDistance;
}

/// \brief Try to "correct" a typo in the source code by finding
/// visible declarations whose names are similar to the name that was
/// present in the source code.
///
/// \param TypoName the \c DeclarationNameInfo structure that contains
/// the name that was present in the source code along with its location.
///
/// \param LookupKind the name-lookup criteria used to search for the name.
///
/// \param S the scope in which name lookup occurs.
///
/// \param SS the nested-name-specifier that precedes the name we're
/// looking for, if present.
///
/// \param CCC A CorrectionCandidateCallback object that provides further
/// validation of typo correction candidates. It also provides flags for
/// determining the set of keywords permitted.
///
/// \param MemberContext if non-NULL, the context in which to look for
/// a member access expression.
///
/// \param EnteringContext whether we're entering the context described by
/// the nested-name-specifier SS.
///
/// \param OPT when non-NULL, the search for visible declarations will
/// also walk the protocols in the qualified interfaces of \p OPT.
///
/// \returns a \c TypoCorrection containing the corrected name if the typo
/// along with information such as the \c NamedDecl where the corrected name
/// was declared, and any additional \c NestedNameSpecifier needed to access
/// it (C++ only). The \c TypoCorrection is empty if there is no correction.
TypoCorrection Sema::CorrectTypo(const DeclarationNameInfo &TypoName,
                                 Sema::LookupNameKind LookupKind,
                                 Scope *S, CXXScopeSpec *SS,
                                 CorrectionCandidateCallback &CCC,
                                 DeclContext *MemberContext,
                                 bool EnteringContext,
                                 const ObjCObjectPointerType *OPT) {
  if (Diags.hasFatalErrorOccurred() || !getLangOpts().SpellChecking)
    return TypoCorrection();

  // In Microsoft mode, don't perform typo correction in a template member
  // function dependent context because it interferes with the "lookup into
  // dependent bases of class templates" feature.
  if (getLangOpts().MicrosoftMode && CurContext->isDependentContext() &&
      isa<CXXMethodDecl>(CurContext))
    return TypoCorrection();

  // We only attempt to correct typos for identifiers.
  IdentifierInfo *Typo = TypoName.getName().getAsIdentifierInfo();
  if (!Typo)
    return TypoCorrection();

  // If the scope specifier itself was invalid, don't try to correct
  // typos.
  if (SS && SS->isInvalid())
    return TypoCorrection();

  // Never try to correct typos during template deduction or
  // instantiation.
  if (!ActiveTemplateInstantiations.empty())
    return TypoCorrection();

  NamespaceSpecifierSet Namespaces(Context, CurContext, SS);

  TypoCorrectionConsumer Consumer(*this, Typo);

  // If a callback object considers an empty typo correction candidate to be
  // viable, assume it does not do any actual validation of the candidates.
  TypoCorrection EmptyCorrection;
  bool ValidatingCallback = !isCandidateViable(CCC, EmptyCorrection);

  // Perform name lookup to find visible, similarly-named entities.
  bool IsUnqualifiedLookup = false;
  DeclContext *QualifiedDC = MemberContext;
  if (MemberContext) {
    LookupVisibleDecls(MemberContext, LookupKind, Consumer);

    // Look in qualified interfaces.
    if (OPT) {
      for (ObjCObjectPointerType::qual_iterator
             I = OPT->qual_begin(), E = OPT->qual_end();
           I != E; ++I)
        LookupVisibleDecls(*I, LookupKind, Consumer);
    }
  } else if (SS && SS->isSet()) {
    QualifiedDC = computeDeclContext(*SS, EnteringContext);
    if (!QualifiedDC)
      return TypoCorrection();

    // Provide a stop gap for files that are just seriously broken.  Trying
    // to correct all typos can turn into a HUGE performance penalty, causing
    // some files to take minutes to get rejected by the parser.
    if (TyposCorrected + UnqualifiedTyposCorrected.size() >= 20)
      return TypoCorrection();
    ++TyposCorrected;

    LookupVisibleDecls(QualifiedDC, LookupKind, Consumer);
  } else {
    IsUnqualifiedLookup = true;
    UnqualifiedTyposCorrectedMap::iterator Cached
      = UnqualifiedTyposCorrected.find(Typo);
    if (Cached != UnqualifiedTyposCorrected.end()) {
      // Add the cached value, unless it's a keyword or fails validation. In the
      // keyword case, we'll end up adding the keyword below.
      if (Cached->second) {
        if (!Cached->second.isKeyword() &&
            isCandidateViable(CCC, Cached->second))
          Consumer.addCorrection(Cached->second);
      } else {
        // Only honor no-correction cache hits when a callback that will validate
        // correction candidates is not being used.
        if (!ValidatingCallback)
          return TypoCorrection();
      }
    }
    if (Cached == UnqualifiedTyposCorrected.end()) {
      // Provide a stop gap for files that are just seriously broken.  Trying
      // to correct all typos can turn into a HUGE performance penalty, causing
      // some files to take minutes to get rejected by the parser.
      if (TyposCorrected + UnqualifiedTyposCorrected.size() >= 20)
        return TypoCorrection();
    }
  }

  // Determine whether we are going to search in the various namespaces for
  // corrections.
  bool SearchNamespaces
    = getLangOpts().CPlusPlus &&
      (IsUnqualifiedLookup || (QualifiedDC && QualifiedDC->isNamespace()));
  // In a few cases we *only* want to search for corrections bases on just
  // adding or changing the nested name specifier.
  bool AllowOnlyNNSChanges = Typo->getName().size() < 3;
  
  if (IsUnqualifiedLookup || SearchNamespaces) {
    // For unqualified lookup, look through all of the names that we have
    // seen in this translation unit.
    // FIXME: Re-add the ability to skip very unlikely potential corrections.
    for (IdentifierTable::iterator I = Context.Idents.begin(),
                                IEnd = Context.Idents.end();
         I != IEnd; ++I)
      Consumer.FoundName(I->getKey());

    // Walk through identifiers in external identifier sources.
    // FIXME: Re-add the ability to skip very unlikely potential corrections.
    if (IdentifierInfoLookup *External
                            = Context.Idents.getExternalIdentifierLookup()) {
      OwningPtr<IdentifierIterator> Iter(External->getIdentifiers());
      do {
        StringRef Name = Iter->Next();
        if (Name.empty())
          break;

        Consumer.FoundName(Name);
      } while (true);
    }
  }

  AddKeywordsToConsumer(*this, Consumer, S, CCC, SS && SS->isNotEmpty());

  // If we haven't found anything, we're done.
  if (Consumer.empty()) {
    // If this was an unqualified lookup, note that no correction was found.
    if (IsUnqualifiedLookup)
      (void)UnqualifiedTyposCorrected[Typo];

    return TypoCorrection();
  }

  // Make sure the best edit distance (prior to adding any namespace qualifiers)
  // is not more that about a third of the length of the typo's identifier.
  unsigned ED = Consumer.getBestEditDistance(true);
  if (ED > 0 && Typo->getName().size() / ED < 3) {
    // If this was an unqualified lookup, note that no correction was found.
    if (IsUnqualifiedLookup)
      (void)UnqualifiedTyposCorrected[Typo];

    return TypoCorrection();
  }

  // Build the NestedNameSpecifiers for the KnownNamespaces, if we're going
  // to search those namespaces.
  if (SearchNamespaces) {
    // Load any externally-known namespaces.
    if (ExternalSource && !LoadedExternalKnownNamespaces) {
      SmallVector<NamespaceDecl *, 4> ExternalKnownNamespaces;
      LoadedExternalKnownNamespaces = true;
      ExternalSource->ReadKnownNamespaces(ExternalKnownNamespaces);
      for (unsigned I = 0, N = ExternalKnownNamespaces.size(); I != N; ++I)
        KnownNamespaces[ExternalKnownNamespaces[I]] = true;
    }
    
    for (llvm::DenseMap<NamespaceDecl*, bool>::iterator 
           KNI = KnownNamespaces.begin(),
           KNIEnd = KnownNamespaces.end();
         KNI != KNIEnd; ++KNI)
      Namespaces.AddNamespace(KNI->first);
  }

  // Weed out any names that could not be found by name lookup or, if a
  // CorrectionCandidateCallback object was provided, failed validation.
  llvm::SmallVector<TypoCorrection, 16> QualifiedResults;
  LookupResult TmpRes(*this, TypoName, LookupKind);
  TmpRes.suppressDiagnostics();
  while (!Consumer.empty()) {
    TypoCorrectionConsumer::distance_iterator DI = Consumer.begin();
    unsigned ED = DI->first;
    for (TypoCorrectionConsumer::result_iterator I = DI->second.begin(),
                                              IEnd = DI->second.end();
         I != IEnd; /* Increment in loop. */) {
      // If we only want nested name specifier corrections, ignore potential
      // corrections that have a different base identifier from the typo.
      if (AllowOnlyNNSChanges &&
          I->second.front().getCorrectionAsIdentifierInfo() != Typo) {
        TypoCorrectionConsumer::result_iterator Prev = I;
        ++I;
        DI->second.erase(Prev);
        continue;
      }

      // If the item already has been looked up or is a keyword, keep it.
      // If a validator callback object was given, drop the correction
      // unless it passes validation.
      bool Viable = false;
      for (TypoResultList::iterator RI = I->second.begin();
           RI != I->second.end(); /* Increment in loop. */) {
        TypoResultList::iterator Prev = RI;
        ++RI;
        if (Prev->isResolved()) {
          if (!isCandidateViable(CCC, *Prev))
            RI = I->second.erase(Prev);
          else
            Viable = true;
        }
      }
      if (Viable || I->second.empty()) {
        TypoCorrectionConsumer::result_iterator Prev = I;
        ++I;
        if (!Viable)
          DI->second.erase(Prev);
        continue;
      }
      assert(I->second.size() == 1 && "Expected a single unresolved candidate");

      // Perform name lookup on this name.
      TypoCorrection &Candidate = I->second.front();
      IdentifierInfo *Name = Candidate.getCorrectionAsIdentifierInfo();
      LookupPotentialTypoResult(*this, TmpRes, Name, S, SS, MemberContext,
                                EnteringContext, CCC.IsObjCIvarLookup);

      switch (TmpRes.getResultKind()) {
      case LookupResult::NotFound:
      case LookupResult::NotFoundInCurrentInstantiation:
      case LookupResult::FoundUnresolvedValue:
        QualifiedResults.push_back(Candidate);
        // We didn't find this name in our scope, or didn't like what we found;
        // ignore it.
        {
          TypoCorrectionConsumer::result_iterator Next = I;
          ++Next;
          DI->second.erase(I);
          I = Next;
        }
        break;

      case LookupResult::Ambiguous:
        // We don't deal with ambiguities.
        return TypoCorrection();

      case LookupResult::FoundOverloaded: {
        TypoCorrectionConsumer::result_iterator Prev = I;
        // Store all of the Decls for overloaded symbols
        for (LookupResult::iterator TRD = TmpRes.begin(),
                                 TRDEnd = TmpRes.end();
             TRD != TRDEnd; ++TRD)
          Candidate.addCorrectionDecl(*TRD);
        ++I;
        if (!isCandidateViable(CCC, Candidate))
          DI->second.erase(Prev);
        break;
      }

      case LookupResult::Found: {
        TypoCorrectionConsumer::result_iterator Prev = I;
        Candidate.setCorrectionDecl(TmpRes.getAsSingle<NamedDecl>());
        ++I;
        if (!isCandidateViable(CCC, Candidate))
          DI->second.erase(Prev);
        break;
      }

      }
    }

    if (DI->second.empty())
      Consumer.erase(DI);
    else if (!getLangOpts().CPlusPlus || QualifiedResults.empty() || !ED)
      // If there are results in the closest possible bucket, stop
      break;

    // Only perform the qualified lookups for C++
    if (SearchNamespaces) {
      TmpRes.suppressDiagnostics();
      for (llvm::SmallVector<TypoCorrection,
                             16>::iterator QRI = QualifiedResults.begin(),
                                        QRIEnd = QualifiedResults.end();
           QRI != QRIEnd; ++QRI) {
        for (NamespaceSpecifierSet::iterator NI = Namespaces.begin(),
                                          NIEnd = Namespaces.end();
             NI != NIEnd; ++NI) {
          DeclContext *Ctx = NI->DeclCtx;

          // FIXME: Stop searching once the namespaces are too far away to create
          // acceptable corrections for this identifier (since the namespaces
          // are sorted in ascending order by edit distance).

          TmpRes.clear();
          TmpRes.setLookupName(QRI->getCorrectionAsIdentifierInfo());
          if (!LookupQualifiedName(TmpRes, Ctx)) continue;

          // Any corrections added below will be validated in subsequent
          // iterations of the main while() loop over the Consumer's contents.
          switch (TmpRes.getResultKind()) {
          case LookupResult::Found: {
            TypoCorrection TC(*QRI);
            TC.setCorrectionDecl(TmpRes.getAsSingle<NamedDecl>());
            TC.setCorrectionSpecifier(NI->NameSpecifier);
            TC.setQualifierDistance(NI->EditDistance);
            Consumer.addCorrection(TC);
            break;
          }
          case LookupResult::FoundOverloaded: {
            TypoCorrection TC(*QRI);
            TC.setCorrectionSpecifier(NI->NameSpecifier);
            TC.setQualifierDistance(NI->EditDistance);
            for (LookupResult::iterator TRD = TmpRes.begin(),
                                     TRDEnd = TmpRes.end();
                 TRD != TRDEnd; ++TRD)
              TC.addCorrectionDecl(*TRD);
            Consumer.addCorrection(TC);
            break;
          }
          case LookupResult::NotFound:
          case LookupResult::NotFoundInCurrentInstantiation:
          case LookupResult::Ambiguous:
          case LookupResult::FoundUnresolvedValue:
            break;
          }
        }
      }
    }

    QualifiedResults.clear();
  }

  // No corrections remain...
  if (Consumer.empty()) return TypoCorrection();

  TypoResultsMap &BestResults = Consumer.getBestResults();
  ED = Consumer.getBestEditDistance(true);

  if (!AllowOnlyNNSChanges && ED > 0 && Typo->getName().size() / ED < 3) {
    // If this was an unqualified lookup and we believe the callback
    // object wouldn't have filtered out possible corrections, note
    // that no correction was found.
    if (IsUnqualifiedLookup && !ValidatingCallback)
      (void)UnqualifiedTyposCorrected[Typo];

    return TypoCorrection();
  }

  // If only a single name remains, return that result.
  if (BestResults.size() == 1) {
    const TypoResultList &CorrectionList = BestResults.begin()->second;
    const TypoCorrection &Result = CorrectionList.front();
    if (CorrectionList.size() != 1) return TypoCorrection();

    // Don't correct to a keyword that's the same as the typo; the keyword
    // wasn't actually in scope.
    if (ED == 0 && Result.isKeyword()) return TypoCorrection();

    // Record the correction for unqualified lookup.
    if (IsUnqualifiedLookup)
      UnqualifiedTyposCorrected[Typo] = Result;

    TypoCorrection TC = Result;
    TC.setCorrectionRange(SS, TypoName);
    return TC;
  }
  else if (BestResults.size() > 1
           // Ugly hack equivalent to CTC == CTC_ObjCMessageReceiver;
           // WantObjCSuper is only true for CTC_ObjCMessageReceiver and for
           // some instances of CTC_Unknown, while WantRemainingKeywords is true
           // for CTC_Unknown but not for CTC_ObjCMessageReceiver.
           && CCC.WantObjCSuper && !CCC.WantRemainingKeywords
           && BestResults["super"].front().isKeyword()) {
    // Prefer 'super' when we're completing in a message-receiver
    // context.

    // Don't correct to a keyword that's the same as the typo; the keyword
    // wasn't actually in scope.
    if (ED == 0) return TypoCorrection();

    // Record the correction for unqualified lookup.
    if (IsUnqualifiedLookup)
      UnqualifiedTyposCorrected[Typo] = BestResults["super"].front();

    TypoCorrection TC = BestResults["super"].front();
    TC.setCorrectionRange(SS, TypoName);
    return TC;
  }

  // If this was an unqualified lookup and we believe the callback object did
  // not filter out possible corrections, note that no correction was found.
  if (IsUnqualifiedLookup && !ValidatingCallback)
    (void)UnqualifiedTyposCorrected[Typo];

  return TypoCorrection();
}

void TypoCorrection::addCorrectionDecl(NamedDecl *CDecl) {
  if (!CDecl) return;

  if (isKeyword())
    CorrectionDecls.clear();

  CorrectionDecls.push_back(CDecl->getUnderlyingDecl());

  if (!CorrectionName)
    CorrectionName = CDecl->getDeclName();
}

std::string TypoCorrection::getAsString(const LangOptions &LO) const {
  if (CorrectionNameSpec) {
    std::string tmpBuffer;
    llvm::raw_string_ostream PrefixOStream(tmpBuffer);
    CorrectionNameSpec->print(PrefixOStream, PrintingPolicy(LO));
    CorrectionName.printName(PrefixOStream);
    return PrefixOStream.str();
  }

  return CorrectionName.getAsString();
}
#endif
