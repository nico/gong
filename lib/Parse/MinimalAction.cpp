//===--- MinimalAction.cpp - Implement the MinimalAction class ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements the MinimalAction interface.
//
//===----------------------------------------------------------------------===//

#include "gong/Parse/Parser.h"
//#include "gong/Parse/DeclSpec.h"
#include "gong/Parse/Scope.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/RecyclingAllocator.h"
#include "llvm/Support/raw_ostream.h"
using namespace gong;

///  Out-of-line virtual destructor to provide home for ActionBase class.
ActionBase::~ActionBase() {}

///  Out-of-line virtual destructor to provide home for Action class.
Action::~Action() {}

//void PrettyStackTraceActionsDecl::print(llvm::raw_ostream &OS) const {
//  if (Loc.isValid()) {
//    Loc.print(OS, SM);
//    OS << ": ";
//  }
//  OS << Message;
//
//  std::string Name = Actions.getDeclName(TheDecl);
//  if (!Name.empty())
//    OS << " '" << Name << '\'';
//
//  OS << '\n';
//}

/// TypeNameInfo - A link exists here for each scope that an identifier is
/// defined.
namespace {
  struct TypeNameInfo {
    TypeNameInfo *Prev;
    bool isTypeName;

    TypeNameInfo(bool istypename, TypeNameInfo *prev) {
      isTypeName = istypename;
      Prev = prev;
    }
  };

  struct TypeNameInfoTable {
    llvm::RecyclingAllocator<llvm::BumpPtrAllocator, TypeNameInfo> Allocator;

    void AddEntry(bool isTypename, IdentifierInfo *II) {
      TypeNameInfo *TI = Allocator.Allocate<TypeNameInfo>();
      new (TI) TypeNameInfo(isTypename, II->getFETokenInfo<TypeNameInfo>());
      II->setFETokenInfo(TI);
    }

    void DeleteEntry(TypeNameInfo *Entry) {
      Entry->~TypeNameInfo();
      Allocator.Deallocate(Entry);
    }
  };
}

static TypeNameInfoTable *getTable(void *TP) {
  return static_cast<TypeNameInfoTable*>(TP);
}

MinimalAction::MinimalAction(Lexer &l)
  : Idents(l.getIdentifierTable()), L(l) {
  TypeNameInfoTablePtr = new TypeNameInfoTable();
}

MinimalAction::~MinimalAction() {
  delete getTable(TypeNameInfoTablePtr);
}

void MinimalAction::ActOnTranslationUnitScope(Scope *S) {
  TUScope = S;

  TypeNameInfoTable &TNIT = *getTable(TypeNameInfoTablePtr);

  const char* BuiltinTypes[] = {
    "bool", "byte", "complex64", "complex128", "error", "float32", "float64",
    "int", "int8", "int16", "int32", "int64", "rune", "string",
    "uint", "uint8", "uint16", "uint32", "uint64", "uintptr"
  };
  for (size_t i = 0; i < llvm::array_lengthof(BuiltinTypes); ++i)
    TNIT.AddEntry(true, &Idents.get(BuiltinTypes[i]));
}

/// This looks at the IdentifierInfo::FETokenInfo field to determine whether
/// the name is a type name  or not in this scope.
Action::TypeTy *
MinimalAction::getTypeName(IdentifierInfo &II, SourceLocation Loc,
                           Scope *S, CXXScopeSpec *SS,
                           bool isClassName, TypeTy *ObjectType) {
  if (TypeNameInfo *TI = II.getFETokenInfo<TypeNameInfo>())
    if (TI->isTypeName)
      return TI;
  return 0;
}

/// isCurrentClassName - Always returns false, because MinimalAction
/// does not support C++ classes with constructors.
bool MinimalAction::isCurrentClassName(const IdentifierInfo &, Scope *,
                                       const CXXScopeSpec *) {
  return false;
}

/// ActOnDeclarator - If this is a typedef declarator, we modify the
/// IdentifierInfo::FETokenInfo field to keep track of this fact, until S is
/// popped.
Action::DeclPtrTy
MinimalAction::ActOnDeclarator(Scope *S, Declarator &D) {
  //IdentifierInfo *II = D.getIdentifier();

  //// If there is no identifier associated with this declarator, bail out.
  //if (II == 0) return DeclPtrTy();

  //TypeNameInfo *weCurrentlyHaveTypeInfo = II->getFETokenInfo<TypeNameInfo>();
  //bool isTypeName =
  //  D.getDeclSpec().getStorageClassSpec() == DeclSpec::SCS_typedef;

  //// this check avoids creating TypeNameInfo objects for the common case.
  //// It does need to handle the uncommon case of shadowing a typedef name with a
  //// non-typedef name. e.g. { typedef int a; a xx; { int a; } }
  //if (weCurrentlyHaveTypeInfo || isTypeName) {
  //  // Allocate and add the 'TypeNameInfo' "decl".
  //  getTable(TypeNameInfoTablePtr)->AddEntry(isTypeName, II);

  //  // Remember that this needs to be removed when the scope is popped.
  //  S->AddDecl(DeclPtrTy::make(II));
  //}
  return DeclPtrTy();
}

Action::DeclPtrTy
MinimalAction::ActOnStartClassInterface(SourceLocation AtInterfaceLoc,
                                        IdentifierInfo *ClassName,
                                        SourceLocation ClassLoc,
                                        IdentifierInfo *SuperName,
                                        SourceLocation SuperLoc,
                                        const DeclPtrTy *ProtoRefs,
                                        unsigned NumProtocols,
                                        const SourceLocation *ProtoLocs,
                                        SourceLocation EndProtoLoc,
                                        AttributeList *AttrList) {
  // Allocate and add the 'TypeNameInfo' "decl".
  getTable(TypeNameInfoTablePtr)->AddEntry(true, ClassName);
  return DeclPtrTy();
}

/// ActOnForwardClassDeclaration -
/// Scope will always be top level file scope.
Action::DeclPtrTy
MinimalAction::ActOnForwardClassDeclaration(SourceLocation AtClassLoc,
                                            IdentifierInfo **IdentList,
                                            SourceLocation *IdentLocs,
                                            unsigned NumElts) {
  for (unsigned i = 0; i != NumElts; ++i) {
    // Allocate and add the 'TypeNameInfo' "decl".
    getTable(TypeNameInfoTablePtr)->AddEntry(true, IdentList[i]);

    // Remember that this needs to be removed when the scope is popped.
    TUScope->AddDecl(DeclPtrTy::make(IdentList[i]));
  }
  return DeclPtrTy();
}

/// ActOnPopScope - When a scope is popped, if any typedefs are now
/// out-of-scope, they are removed from the IdentifierInfo::FETokenInfo field.
void MinimalAction::ActOnPopScope(SourceLocation Loc, Scope *S) {
  TypeNameInfoTable &Table = *getTable(TypeNameInfoTablePtr);

  for (Scope::decl_iterator I = S->decl_begin(), E = S->decl_end();
       I != E; ++I) {
    IdentifierInfo &II = *(*I).getAs<IdentifierInfo>();
    TypeNameInfo *TI = II.getFETokenInfo<TypeNameInfo>();
    assert(TI && "This decl didn't get pushed??");

    if (TI) {
      TypeNameInfo *Next = TI->Prev;
      Table.DeleteEntry(TI);

      II.setFETokenInfo(Next);
    }
  }
}
