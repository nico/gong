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
    Action::IdentifierInfoType Type;

    TypeNameInfo(Action::IdentifierInfoType type, TypeNameInfo *prev) {
      Type = type;
      Prev = prev;
    }
  };

  struct TypeNameInfoTable {
    llvm::RecyclingAllocator<llvm::BumpPtrAllocator, TypeNameInfo> Allocator;

    void AddEntry(Action::IdentifierInfoType Type, IdentifierInfo *II) {
//fprintf(stderr, "pushing %d for %s\n", Type, II->getName().str().c_str());
      TypeNameInfo *TI = Allocator.Allocate<TypeNameInfo>();
      new (TI) TypeNameInfo(Type, II->getFETokenInfo<TypeNameInfo>());
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

void MinimalAction::ActOnImportSpec(StringRef ImportPath,
                                    IdentifierInfo *LocalName,
                                    bool IsLocal) {
  if (IsLocal)
    // FIXME: This should insert all exported symbols in ImportPath into the
    // current file's file scope.
    return;

  TypeNameInfoTable &TNIT = *getTable(TypeNameInfoTablePtr);

  if (LocalName) {
    // FIXME: This should insert all exported symbols in ImportPath into
    // LocalName's scope.
    TNIT.AddEntry(Action::IIT_Package, LocalName);
    TUScope->AddDecl(DeclPtrTy::make(LocalName));
    return;
  }

  // FIXME: The right thing to do here is to load the package file pointed to
  // by ImportPath and get the `package` identifier used therein. As an
  // approximation, just grab the last path component off ImportPath.
  ImportPath = ImportPath.slice(1, ImportPath.size() - 1);  // Strip ""
  size_t SlashPos = ImportPath.rfind('/');
  if (SlashPos != StringRef::npos)
    ImportPath = ImportPath.substr(SlashPos + 1);

  TNIT.AddEntry(Action::IIT_Package, &Idents.get(ImportPath));
  TUScope->AddDecl(DeclPtrTy::make(&Idents.get(ImportPath)));
}

/// This looks at the IdentifierInfo::FETokenInfo field to determine whether
/// the name is a package name, type name, or not in this scope.
Action::IdentifierInfoType MinimalAction::classifyIdentifier(
    const IdentifierInfo &II, const Scope* S) {
  if (TypeNameInfo *TI = II.getFETokenInfo<TypeNameInfo>())
    return TI->Type;
  return IIT_Unknown;
}

/// Registers an identifier as type package name.
void MinimalAction::ActOnTypeSpec(IdentifierInfo &II, Scope* S) {
  getTable(TypeNameInfoTablePtr)->AddEntry(Action::IIT_Type, &II);

  // Remember that this needs to be removed when the scope is popped.
  S->AddDecl(DeclPtrTy::make(&II));
}

void MinimalAction::ActOnFunctionDecl(IdentifierInfo &II, Scope* S) {
  getTable(TypeNameInfoTablePtr)->AddEntry(Action::IIT_Func, &II);

  // Remember that this needs to be removed when the scope is popped.
  S->AddDecl(DeclPtrTy::make(&II));
}

void MinimalAction::ActOnTranslationUnitScope(Scope *S) {
  TUScope = S;

  TypeNameInfoTable &TNIT = *getTable(TypeNameInfoTablePtr);

  // http://golang.org/ref/spec#Predeclared_identifiers
  const char* BuiltinTypes[] = {
    "bool", "byte", "complex64", "complex128", "error", "float32", "float64",
    "int", "int8", "int16", "int32", "int64", "rune", "string",
    "uint", "uint8", "uint16", "uint32", "uint64", "uintptr"
  };
  for (size_t i = 0; i < llvm::array_lengthof(BuiltinTypes); ++i) {
    TNIT.AddEntry(Action::IIT_Type, &Idents.get(BuiltinTypes[i]));
    TUScope->AddDecl(DeclPtrTy::make(&Idents.get(BuiltinTypes[i])));
  }
  // FIXME: Why does the spec list "nil" in a separate category?
  const char* BuiltinConstants[] = { "true", "false", "iota", "nil" };
  for (size_t i = 0; i < llvm::array_lengthof(BuiltinConstants); ++i) {
    TNIT.AddEntry(Action::IIT_Const, &Idents.get(BuiltinConstants[i]));
    TUScope->AddDecl(DeclPtrTy::make(&Idents.get(BuiltinConstants[i])));
  }
  const char* BuiltinFunctions[] = {
    "append", "cap", "close", "complex", "copy", "delete", "imag", "len",
    "make", "new", "panic", "print", "println", "real", "recover",
  };
  for (size_t i = 0; i < llvm::array_lengthof(BuiltinFunctions); ++i) {
    TNIT.AddEntry(Action::IIT_BuiltinFunc, &Idents.get(BuiltinFunctions[i]));
    TUScope->AddDecl(DeclPtrTy::make(&Idents.get(BuiltinFunctions[i])));
  }
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

//fprintf(stderr, "popping %s\n", II.getName().str().c_str());
      II.setFETokenInfo(Next);
    }
  }
}
