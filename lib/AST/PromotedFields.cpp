//===------ CXXInheritance.cpp - C++ Inheritance ----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides routines that help analyzing C++ inheritance hierarchies.
//
//===----------------------------------------------------------------------===//

#include "gong/AST/PromotedFields.h"

#include "gong/AST/Decl.h"

using namespace gong;

#if 0
#include "gong/AST/ASTContext.h"
#include "gong/AST/RecordLayout.h"
#include "llvm/ADT/SetVector.h"
#include <algorithm>
#include <set>


/// \brief Computes the set of declarations referenced by these base
/// paths.
void PromotedFieldPaths::ComputeDeclsFound() {
  assert(NumDeclsFound == 0 && !DeclsFound &&
         "Already computed the set of declarations");

  llvm::SetVector<NamedDecl *, SmallVector<NamedDecl *, 8> > Decls;
  for (paths_iterator Path = begin(), PathEnd = end(); Path != PathEnd; ++Path)
    Decls.insert(Path->Decls.front());

  NumDeclsFound = Decls.size();
  DeclsFound = new NamedDecl * [NumDeclsFound];
  std::copy(Decls.begin(), Decls.end(), DeclsFound);
}

PromotedFieldPaths::decl_iterator PromotedFieldPaths::found_decls_begin() {
  if (NumDeclsFound == 0)
    ComputeDeclsFound();
  return DeclsFound;
}

PromotedFieldPaths::decl_iterator PromotedFieldPaths::found_decls_end() {
  if (NumDeclsFound == 0)
    ComputeDeclsFound();
  return DeclsFound + NumDeclsFound;
}

/// isAmbiguous - Determines whether the set of paths provided is
/// ambiguous, i.e., there are two or more paths that refer to
/// different base class subobjects of the same type. BaseType must be
/// an unqualified, canonical class type.
bool PromotedFieldPaths::isAmbiguous(CanQualType BaseType) {
  BaseType = BaseType.getUnqualifiedType();
  std::pair<bool, unsigned>& Subobjects = ClassSubobjects[BaseType];
  return Subobjects.second + (Subobjects.first? 1 : 0) > 1;
}

/// clear - Clear out all prior path information.
void PromotedFieldPaths::clear() {
  Paths.clear();
  ClassSubobjects.clear();
  ScratchPath.clear();
}

/// @brief Swaps the contents of this PromotedFieldPaths structure with the
/// contents of Other.
void PromotedFieldPaths::swap(PromotedFieldPaths &Other) {
  std::swap(Origin, Other.Origin);
  Paths.swap(Other.Paths);
  ClassSubobjects.swap(Other.ClassSubobjects);
  std::swap(FindAmbiguities, Other.FindAmbiguities);
  std::swap(RecordPaths, Other.RecordPaths);
}

bool CXXRecordDecl::isDerivedFrom(const CXXRecordDecl *Base) const {
  PromotedFieldPaths Paths(/*FindAmbiguities=*/false, /*RecordPaths=*/false);
  return isDerivedFrom(Base, Paths);
}

bool CXXRecordDecl::isDerivedFrom(const CXXRecordDecl *Base,
                                  PromotedFieldPaths &Paths) const {
  if (getCanonicalDecl() == Base->getCanonicalDecl())
    return false;
  
  Paths.setOrigin(const_cast<CXXRecordDecl*>(this));
  return lookupInBases(&FindBaseClass,
                       const_cast<CXXRecordDecl*>(Base->getCanonicalDecl()),
                       Paths);
}

static bool BaseIsNot(const CXXRecordDecl *Base, void *OpaqueTarget) {
  // OpaqueTarget is a CXXRecordDecl*.
  return Base->getCanonicalDecl() != (const CXXRecordDecl*) OpaqueTarget;
}

bool CXXRecordDecl::isProvablyNotDerivedFrom(const CXXRecordDecl *Base) const {
  return forallBases(BaseIsNot,
                     const_cast<CXXRecordDecl *>(Base->getCanonicalDecl()));
}

bool
CXXRecordDecl::isCurrentInstantiation(const DeclContext *CurContext) const {
  assert(isDependentContext());

  for (; !CurContext->isFileContext(); CurContext = CurContext->getParent())
    if (CurContext->Equals(this))
      return true;

  return false;
}

bool CXXRecordDecl::forallBases(ForallBasesCallback *BaseMatches,
                                void *OpaqueData,
                                bool AllowShortCircuit) const {
  SmallVector<const CXXRecordDecl*, 8> Queue;

  const CXXRecordDecl *Record = this;
  bool AllMatches = true;
  while (true) {
    for (CXXRecordDecl::base_class_const_iterator
           I = Record->bases_begin(), E = Record->bases_end(); I != E; ++I) {
      const RecordType *Ty = I->getType()->getAs<RecordType>();
      if (!Ty) {
        if (AllowShortCircuit) return false;
        AllMatches = false;
        continue;
      }

      CXXRecordDecl *Base = 
            cast_or_null<CXXRecordDecl>(Ty->getDecl()->getDefinition());
      if (!Base ||
          (Base->isDependentContext() &&
           !Base->isCurrentInstantiation(Record))) {
        if (AllowShortCircuit) return false;
        AllMatches = false;
        continue;
      }
      
      Queue.push_back(Base);
      if (!BaseMatches(Base, OpaqueData)) {
        if (AllowShortCircuit) return false;
        AllMatches = false;
        continue;
      }
    }

    if (Queue.empty()) break;
    Record = Queue.back(); // not actually a queue.
    Queue.pop_back();
  }

  return AllMatches;
}
#endif

bool PromotedFieldPaths::lookupInBases(ASTContext &Context,
                                 const StructTypeDecl *Struct/*,
                                CXXRecordDecl::BaseMatchesCallback *BaseMatches,
                                 void *UserData*/) {
  bool FoundPath = false;

  bool IsFirstStep = ScratchPath.empty();

  for (StructTypeDecl::anon_field_iterator
           AnonField = Struct->anon_field_begin(),
           AnonFieldEnd = Struct->anon_field_end();
       AnonField != AnonFieldEnd; ++AnonField) {
#if 0
    // Find the record of the base class subobjects for this type.
    QualType BaseType = Context.getCanonicalType(BaseSpec->getType())
                                                          .getUnqualifiedType();
    
    // Determine whether we need to visit this base class at all,
    // updating the count of subobjects appropriately.
    std::pair<bool, unsigned>& Subobjects = ClassSubobjects[BaseType];
    ++Subobjects.second;
#endif
    
    if (isRecordingPaths()) {
      // Add this base specifier to the current path.
      PromotedFieldPathElement Element;
      //Element.Base = &*BaseSpec;
      //Element.Class = Record;
      //Element.SubobjectNumber = Subobjects.second;
      ScratchPath.push_back(Element);
    }
#if 0
    // Track whether there's a path involving this specific base.
    bool FoundPathThroughBase = false;
    
    if (BaseMatches(BaseSpec, ScratchPath, UserData)) {
      // We've found a path that terminates at this base.
      FoundPath = FoundPathThroughBase = true;
      if (isRecordingPaths()) {
        // We have a path. Make a copy of it before moving on.
        Paths.push_back(ScratchPath);
      } else if (!isFindingAmbiguities()) {
        // We found a path and we don't care about ambiguities;
        // return immediately.
        return FoundPath;
      }
    } else {
      CXXRecordDecl *BaseRecord
        = cast<CXXRecordDecl>(BaseSpec->getType()->castAs<RecordType>()
                                ->getDecl());
      if (lookupInBases(Context, BaseRecord, BaseMatches, UserData)) {
        // C++ [class.member.lookup]p2:
        //   A member name f in one sub-object B hides a member name f in
        //   a sub-object A if A is a base class sub-object of B. Any
        //   declarations that are so hidden are eliminated from
        //   consideration.
        
        // There is a path to a base class that meets the criteria. If we're 
        // not collecting paths or finding ambiguities, we're done.
        FoundPath = FoundPathThroughBase = true;
        if (!isFindingAmbiguities())
          return FoundPath;
      }
    }
    
    // Pop this base specifier off the current path (if we're
    // collecting paths).
    if (isRecordingPaths()) {
      ScratchPath.pop_back();
    }
#endif
  }

  return FoundPath;
}

bool StructTypeDecl::lookupInBases(//BaseMatchesCallback *BaseMatches,
                                   //void *UserData,
                                   PromotedFieldPaths &Paths) const {
  // If we didn't find anything, report that.
  return Paths.lookupInBases(getASTContext(), this/*, BaseMatches, UserData*/);
}

#if 0
bool CXXRecordDecl::FindBaseClass(const CXXBaseSpecifier *Specifier, 
                                  PromotedFieldPath &Path,
                                  void *BaseRecord) {
  assert(((Decl *)BaseRecord)->getCanonicalDecl() == BaseRecord &&
         "User data for FindBaseClass is not canonical!");
  return Specifier->getType()->castAs<RecordType>()->getDecl()
            ->getCanonicalDecl() == BaseRecord;
}

bool CXXRecordDecl::FindTagMember(const CXXBaseSpecifier *Specifier, 
                                  PromotedFieldPath &Path,
                                  void *Name) {
  RecordDecl *BaseRecord =
    Specifier->getType()->castAs<RecordType>()->getDecl();

  DeclarationName N = DeclarationName::getFromOpaquePtr(Name);
  for (Path.Decls = BaseRecord->lookup(N);
       !Path.Decls.empty();
       Path.Decls = Path.Decls.slice(1)) {
    if (Path.Decls.front()->isInIdentifierNamespace(IDNS_Tag))
      return true;
  }

  return false;
}

bool CXXRecordDecl::FindOrdinaryMember(const CXXBaseSpecifier *Specifier, 
                                       PromotedFieldPath &Path,
                                       void *Name) {
  RecordDecl *BaseRecord =
    Specifier->getType()->castAs<RecordType>()->getDecl();
  
  const unsigned IDNS = IDNS_Ordinary | IDNS_Tag | IDNS_Member;
  DeclarationName N = DeclarationName::getFromOpaquePtr(Name);
  for (Path.Decls = BaseRecord->lookup(N);
       !Path.Decls.empty();
       Path.Decls = Path.Decls.slice(1)) {
    if (Path.Decls.front()->isInIdentifierNamespace(IDNS))
      return true;
  }
  
  return false;
}

bool CXXRecordDecl::
FindNestedNameSpecifierMember(const CXXBaseSpecifier *Specifier, 
                              PromotedFieldPath &Path,
                              void *Name) {
  RecordDecl *BaseRecord =
    Specifier->getType()->castAs<RecordType>()->getDecl();
  
  DeclarationName N = DeclarationName::getFromOpaquePtr(Name);
  for (Path.Decls = BaseRecord->lookup(N);
       !Path.Decls.empty();
       Path.Decls = Path.Decls.slice(1)) {
    // FIXME: Refactor the "is it a nested-name-specifier?" check
    if (isa<TypedefNameDecl>(Path.Decls.front()) ||
        Path.Decls.front()->isInIdentifierNamespace(IDNS_Tag))
      return true;
  }
  
  return false;
}

void OverridingMethods::add(unsigned OverriddenSubobject, 
                            UniqueVirtualMethod Overriding) {
  SmallVector<UniqueVirtualMethod, 4> &SubobjectOverrides
    = Overrides[OverriddenSubobject];
  if (std::find(SubobjectOverrides.begin(), SubobjectOverrides.end(), 
                Overriding) == SubobjectOverrides.end())
    SubobjectOverrides.push_back(Overriding);
}

void OverridingMethods::add(const OverridingMethods &Other) {
  for (const_iterator I = Other.begin(), IE = Other.end(); I != IE; ++I) {
    for (overriding_const_iterator M = I->second.begin(), 
                                MEnd = I->second.end();
         M != MEnd; 
         ++M)
      add(I->first, *M);
  }
}

void OverridingMethods::replaceAll(UniqueVirtualMethod Overriding) {
  for (iterator I = begin(), IEnd = end(); I != IEnd; ++I) {
    I->second.clear();
    I->second.push_back(Overriding);
  }
}


namespace {
  class FinalOverriderCollector {
    /// \brief The number of subobjects of a given class type that
    /// occur within the class hierarchy.
    llvm::DenseMap<const CXXRecordDecl *, unsigned> SubobjectCount;

    /// \brief Overriders for each virtual base subobject.
    llvm::DenseMap<const CXXRecordDecl *, CXXFinalOverriderMap *> VirtualOverriders;

    CXXFinalOverriderMap FinalOverriders;

  public:
    ~FinalOverriderCollector();

    void Collect(const CXXRecordDecl *RD, bool VirtualBase,
                 const CXXRecordDecl *InVirtualSubobject,
                 CXXFinalOverriderMap &Overriders);
  };
}

void FinalOverriderCollector::Collect(const CXXRecordDecl *RD, 
                                      bool VirtualBase,
                                      const CXXRecordDecl *InVirtualSubobject,
                                      CXXFinalOverriderMap &Overriders) {
  unsigned SubobjectNumber = 0;
  if (!VirtualBase)
    SubobjectNumber
      = ++SubobjectCount[cast<CXXRecordDecl>(RD->getCanonicalDecl())];

  for (CXXRecordDecl::base_class_const_iterator Base = RD->bases_begin(),
         BaseEnd = RD->bases_end(); Base != BaseEnd; ++Base) {
    if (const RecordType *RT = Base->getType()->getAs<RecordType>()) {
      const CXXRecordDecl *BaseDecl = cast<CXXRecordDecl>(RT->getDecl());
      if (!BaseDecl->isPolymorphic())
        continue;

      if (Overriders.empty() && !Base->isVirtual()) {
        // There are no other overriders of virtual member functions,
        // so let the base class fill in our overriders for us.
        Collect(BaseDecl, false, InVirtualSubobject, Overriders);
        continue;
      }

      // Collect all of the overridders from the base class subobject
      // and merge them into the set of overridders for this class.
      // For virtual base classes, populate or use the cached virtual
      // overrides so that we do not walk the virtual base class (and
      // its base classes) more than once.
      CXXFinalOverriderMap ComputedBaseOverriders;
      CXXFinalOverriderMap *BaseOverriders = &ComputedBaseOverriders;
      if (Base->isVirtual()) {
        CXXFinalOverriderMap *&MyVirtualOverriders = VirtualOverriders[BaseDecl];
        BaseOverriders = MyVirtualOverriders;
        if (!MyVirtualOverriders) {
          MyVirtualOverriders = new CXXFinalOverriderMap;

          // Collect may cause VirtualOverriders to reallocate, invalidating the
          // MyVirtualOverriders reference. Set BaseOverriders to the right
          // value now.
          BaseOverriders = MyVirtualOverriders;

          Collect(BaseDecl, true, BaseDecl, *MyVirtualOverriders);
        }
      } else
        Collect(BaseDecl, false, InVirtualSubobject, ComputedBaseOverriders);

      // Merge the overriders from this base class into our own set of
      // overriders.
      for (CXXFinalOverriderMap::iterator OM = BaseOverriders->begin(), 
                               OMEnd = BaseOverriders->end();
           OM != OMEnd;
           ++OM) {
        const CXXMethodDecl *CanonOM
          = cast<CXXMethodDecl>(OM->first->getCanonicalDecl());
        Overriders[CanonOM].add(OM->second);
      }
    }
  }

  for (CXXRecordDecl::method_iterator M = RD->method_begin(), 
                                   MEnd = RD->method_end();
       M != MEnd;
       ++M) {
    // We only care about virtual methods.
    if (!M->isVirtual())
      continue;

    CXXMethodDecl *CanonM = cast<CXXMethodDecl>(M->getCanonicalDecl());

    if (CanonM->begin_overridden_methods()
                                       == CanonM->end_overridden_methods()) {
      // This is a new virtual function that does not override any
      // other virtual function. Add it to the map of virtual
      // functions for which we are tracking overridders. 

      // C++ [class.virtual]p2:
      //   For convenience we say that any virtual function overrides itself.
      Overriders[CanonM].add(SubobjectNumber,
                             UniqueVirtualMethod(CanonM, SubobjectNumber,
                                                 InVirtualSubobject));
      continue;
    }

    // This virtual method overrides other virtual methods, so it does
    // not add any new slots into the set of overriders. Instead, we
    // replace entries in the set of overriders with the new
    // overrider. To do so, we dig down to the original virtual
    // functions using data recursion and update all of the methods it
    // overrides.
    typedef std::pair<CXXMethodDecl::method_iterator, 
                      CXXMethodDecl::method_iterator> OverriddenMethods;
    SmallVector<OverriddenMethods, 4> Stack;
    Stack.push_back(std::make_pair(CanonM->begin_overridden_methods(),
                                   CanonM->end_overridden_methods()));
    while (!Stack.empty()) {
      OverriddenMethods OverMethods = Stack.back();
      Stack.pop_back();

      for (; OverMethods.first != OverMethods.second; ++OverMethods.first) {
        const CXXMethodDecl *CanonOM
          = cast<CXXMethodDecl>((*OverMethods.first)->getCanonicalDecl());

        // C++ [class.virtual]p2:
        //   A virtual member function C::vf of a class object S is
        //   a final overrider unless the most derived class (1.8)
        //   of which S is a base class subobject (if any) declares
        //   or inherits another member function that overrides vf.
        //
        // Treating this object like the most derived class, we
        // replace any overrides from base classes with this
        // overriding virtual function.
        Overriders[CanonOM].replaceAll(
                               UniqueVirtualMethod(CanonM, SubobjectNumber,
                                                   InVirtualSubobject));

        if (CanonOM->begin_overridden_methods()
                                       == CanonOM->end_overridden_methods())
          continue;

        // Continue recursion to the methods that this virtual method
        // overrides.
        Stack.push_back(std::make_pair(CanonOM->begin_overridden_methods(),
                                       CanonOM->end_overridden_methods()));
      }
    }

    // C++ [class.virtual]p2:
    //   For convenience we say that any virtual function overrides itself.
    Overriders[CanonM].add(SubobjectNumber,
                           UniqueVirtualMethod(CanonM, SubobjectNumber,
                                               InVirtualSubobject));
  }
}

FinalOverriderCollector::~FinalOverriderCollector() {
  for (llvm::DenseMap<const CXXRecordDecl *, CXXFinalOverriderMap *>::iterator
         VO = VirtualOverriders.begin(), VOEnd = VirtualOverriders.end();
       VO != VOEnd; 
       ++VO)
    delete VO->second;
}

void 
CXXRecordDecl::getFinalOverriders(CXXFinalOverriderMap &FinalOverriders) const {
  FinalOverriderCollector Collector;
  Collector.Collect(this, false, 0, FinalOverriders);

  // Weed out any final overriders that come from virtual base class
  // subobjects that were hidden by other subobjects along any path.
  // This is the final-overrider variant of C++ [class.member.lookup]p10.
  for (CXXFinalOverriderMap::iterator OM = FinalOverriders.begin(), 
                           OMEnd = FinalOverriders.end();
       OM != OMEnd;
       ++OM) {
    for (OverridingMethods::iterator SO = OM->second.begin(), 
                                  SOEnd = OM->second.end();
         SO != SOEnd; 
         ++SO) {
      SmallVector<UniqueVirtualMethod, 4> &Overriding = SO->second;
      if (Overriding.size() < 2)
        continue;

      for (SmallVector<UniqueVirtualMethod, 4>::iterator 
             Pos = Overriding.begin(), PosEnd = Overriding.end();
           Pos != PosEnd;
           /* increment in loop */) {
        if (!Pos->InVirtualSubobject) {
          ++Pos;
          continue;
        }

        // We have an overriding method in a virtual base class
        // subobject (or non-virtual base class subobject thereof);
        // determine whether there exists an other overriding method
        // in a base class subobject that hides the virtual base class
        // subobject.
        bool Hidden = false;
        for (SmallVector<UniqueVirtualMethod, 4>::iterator
               OP = Overriding.begin(), OPEnd = Overriding.end();
             OP != OPEnd && !Hidden; 
             ++OP) {
          if (Pos == OP)
            continue;

          if (OP->Method->getParent()->isVirtuallyDerivedFrom(
                         const_cast<CXXRecordDecl *>(Pos->InVirtualSubobject)))
            Hidden = true;
        }

        if (Hidden) {
          // The current overriding function is hidden by another
          // overriding function; remove this one.
          Pos = Overriding.erase(Pos);
          PosEnd = Overriding.end();
        } else {
          ++Pos;
        }
      }
    }
  }
}

static void 
AddIndirectPrimaryBases(const CXXRecordDecl *RD, ASTContext &Context,
                        CXXIndirectPrimaryBaseSet& Bases) {
  // If the record has a virtual primary base class, add it to our set.
  const ASTRecordLayout &Layout = Context.getASTRecordLayout(RD);
  if (Layout.isPrimaryBaseVirtual())
    Bases.insert(Layout.getPrimaryBase());

  for (CXXRecordDecl::base_class_const_iterator I = RD->bases_begin(),
       E = RD->bases_end(); I != E; ++I) {
    assert(!I->getType()->isDependentType() &&
           "Cannot get indirect primary bases for class with dependent bases.");

    const CXXRecordDecl *BaseDecl =
      cast<CXXRecordDecl>(I->getType()->castAs<RecordType>()->getDecl());

    // Only bases with virtual bases participate in computing the
    // indirect primary virtual base classes.
    if (BaseDecl->getNumVBases())
      AddIndirectPrimaryBases(BaseDecl, Context, Bases);
  }

}

void 
CXXRecordDecl::getIndirectPrimaryBases(CXXIndirectPrimaryBaseSet& Bases) const {
  ASTContext &Context = getASTContext();

  if (!getNumVBases())
    return;

  for (CXXRecordDecl::base_class_const_iterator I = bases_begin(),
       E = bases_end(); I != E; ++I) {
    assert(!I->getType()->isDependentType() &&
           "Cannot get indirect primary bases for class with dependent bases.");

    const CXXRecordDecl *BaseDecl =
      cast<CXXRecordDecl>(I->getType()->castAs<RecordType>()->getDecl());

    // Only bases with virtual bases participate in computing the
    // indirect primary virtual base classes.
    if (BaseDecl->getNumVBases())
      AddIndirectPrimaryBases(BaseDecl, Context, Bases);
  }
}
#endif
