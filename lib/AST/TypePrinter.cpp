//===--- TypePrinter.cpp - Pretty-Print Gong Types ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This contains code to print types from Gong's type system.
//
//===----------------------------------------------------------------------===//

#include "gong/AST/ASTContext.h"
#include "gong/AST/Decl.h"
#include "gong/AST/Type.h"
#include "gong/Basic/IdentifierTable.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SaveAndRestore.h"
using namespace gong;

#if 0
#include "gong/AST/PrettyPrinter.h"
#include "gong/AST/DeclObjC.h"
#include "gong/AST/DeclTemplate.h"
#include "gong/AST/Expr.h"
#include "gong/Basic/LangOptions.h"
#include "gong/Basic/SourceManager.h"
#include "llvm/ADT/StringExtras.h"
#endif

namespace {
#if 0
  /// \brief RAII object that enables printing of the ARC __strong lifetime
  /// qualifier.
  class IncludeStrongLifetimeRAII {
    PrintingPolicy &Policy;
    bool Old;
    
  public:
    explicit IncludeStrongLifetimeRAII(PrintingPolicy &Policy) 
      : Policy(Policy), Old(Policy.SuppressStrongLifetime) {
      Policy.SuppressStrongLifetime = false;
    }
    
    ~IncludeStrongLifetimeRAII() {
      Policy.SuppressStrongLifetime = Old;
    }
  };

  class ParamPolicyRAII {
    PrintingPolicy &Policy;
    bool Old;
    
  public:
    explicit ParamPolicyRAII(PrintingPolicy &Policy) 
      : Policy(Policy), Old(Policy.SuppressSpecifiers) {
      Policy.SuppressSpecifiers = false;
    }
    
    ~ParamPolicyRAII() {
      Policy.SuppressSpecifiers = Old;
    }
  };

  class ElaboratedTypePolicyRAII {
    PrintingPolicy &Policy;
    bool SuppressTagKeyword;
    bool SuppressScope;
    
  public:
    explicit ElaboratedTypePolicyRAII(PrintingPolicy &Policy) : Policy(Policy) {
      SuppressTagKeyword = Policy.SuppressTagKeyword;
      SuppressScope = Policy.SuppressScope;
      Policy.SuppressTagKeyword = true;
      Policy.SuppressScope = true;
    }
    
    ~ElaboratedTypePolicyRAII() {
      Policy.SuppressTagKeyword = SuppressTagKeyword;
      Policy.SuppressScope = SuppressScope;
    }
  };
#endif
  
  class TypePrinter {
    //PrintingPolicy Policy;
    bool HasEmptyPlaceHolder;

  public:
    explicit TypePrinter(/*const PrintingPolicy &Policy*/)
      : /*Policy(Policy),*/ HasEmptyPlaceHolder(false) { }

    void print(const Type *ty, raw_ostream &OS, StringRef PlaceHolder);

    void spaceBeforePlaceHolder(raw_ostream &OS);
    void printTypeSpec(const NamedDecl *D, raw_ostream &OS);

    void printBefore(const Type *ty, raw_ostream &OS);
    void printAfter(const Type *ty, raw_ostream &OS);
    void AppendScope(DeclContext *DC, raw_ostream &OS);
#define ABSTRACT_TYPE(CLASS, PARENT)
#define TYPE(CLASS, PARENT) \
    void print##CLASS##Before(const CLASS##Type *T, raw_ostream &OS); \
    void print##CLASS##After(const CLASS##Type *T, raw_ostream &OS);
#include "gong/AST/TypeNodes.def"
  };
}

void TypePrinter::spaceBeforePlaceHolder(raw_ostream &OS) {
  if (!HasEmptyPlaceHolder)
    OS << ' ';
}

void TypePrinter::print(const Type *T, raw_ostream &OS, StringRef PlaceHolder) {
  if (!T) {
    OS << "NULL TYPE";
    return;
  }

  SaveAndRestore<bool> PHVal(HasEmptyPlaceHolder, PlaceHolder.empty());

  printBefore(T, OS);
  OS << PlaceHolder;
  printAfter(T, OS);
}

/// \brief Prints the part of the type string before an identifier, e.g. for
/// "int foo[10]" it prints "int ".
//FIXME: Probably not needed for Go, as that has a orderlier type syntax?
void TypePrinter::printBefore(const Type *T, raw_ostream &OS) {
  //if (Policy.SuppressSpecifiers && T->isSpecifierType())
    //return;

  SaveAndRestore<bool> PrevPHIsEmpty(HasEmptyPlaceHolder);

  switch (T->getTypeClass()) {
#define ABSTRACT_TYPE(CLASS, PARENT)
#define TYPE(CLASS, PARENT) case Type::CLASS: \
    print##CLASS##Before(cast<CLASS##Type>(T), OS); \
    break;
#include "gong/AST/TypeNodes.def"
  }
}

/// \brief Prints the part of the type string after an identifier, e.g. for
/// "int foo[10]" it prints "[10]".
void TypePrinter::printAfter(const Type *T, raw_ostream &OS) {
  switch (T->getTypeClass()) {
#define ABSTRACT_TYPE(CLASS, PARENT)
#define TYPE(CLASS, PARENT) case Type::CLASS: \
    print##CLASS##After(cast<CLASS##Type>(T), OS); \
    break;
#include "gong/AST/TypeNodes.def"
  }
}

void TypePrinter::printBuiltinBefore(const BuiltinType *T, raw_ostream &OS) {
  OS << T->getName();
  spaceBeforePlaceHolder(OS);
}
void TypePrinter::printBuiltinAfter(const BuiltinType *T, raw_ostream &OS) { }

#if 0
void TypePrinter::printComplexBefore(const ComplexType *T, raw_ostream &OS) {
  OS << "_Complex ";
  printBefore(T->getElementType(), OS);
}
void TypePrinter::printComplexAfter(const ComplexType *T, raw_ostream &OS) {
  printAfter(T->getElementType(), OS);
}
#endif

void TypePrinter::printPointerBefore(const PointerType *T, raw_ostream &OS) {
  //SaveAndRestore<bool> NonEmptyPH(HasEmptyPlaceHolder, false);
  OS << '*';
  printBefore(T->getPointeeType(), OS);
  // Handle things like 'int (*A)[4];' correctly.
  //if (isa<ArrayType>(T->getPointeeType()))
    //OS << '(';
}

void TypePrinter::printPointerAfter(const PointerType *T, raw_ostream &OS) {
  SaveAndRestore<bool> NonEmptyPH(HasEmptyPlaceHolder, false);
  // Handle things like 'int (*A)[4];' correctly.
  //if (isa<ArrayType>(T->getPointeeType()))
    //OS << ')';
  printAfter(T->getPointeeType(), OS);
}

#if 0
void TypePrinter::printBlockPointerBefore(const BlockPointerType *T,
                                          raw_ostream &OS) {
  SaveAndRestore<bool> NonEmptyPH(HasEmptyPlaceHolder, false);
  printBefore(T->getPointeeType(), OS);
  OS << '^';
}
void TypePrinter::printBlockPointerAfter(const BlockPointerType *T,
                                          raw_ostream &OS) {
  SaveAndRestore<bool> NonEmptyPH(HasEmptyPlaceHolder, false);
  printAfter(T->getPointeeType(), OS);
}

void TypePrinter::printMemberPointerBefore(const MemberPointerType *T, 
                                           raw_ostream &OS) { 
  IncludeStrongLifetimeRAII Strong(Policy);
  SaveAndRestore<bool> NonEmptyPH(HasEmptyPlaceHolder, false);
  printBefore(T->getPointeeType(), OS);
  // Handle things like 'int (Cls::*A)[4];' correctly.
  // FIXME: this should include vectors, but vectors use attributes I guess.
  if (isa<ArrayType>(T->getPointeeType()))
    OS << '(';

  PrintingPolicy InnerPolicy(Policy);
  InnerPolicy.SuppressTag = false;
  TypePrinter(InnerPolicy).print(QualType(T->getClass(), 0), OS, StringRef());

  OS << "::*";
}
void TypePrinter::printMemberPointerAfter(const MemberPointerType *T, 
                                          raw_ostream &OS) { 
  IncludeStrongLifetimeRAII Strong(Policy);
  SaveAndRestore<bool> NonEmptyPH(HasEmptyPlaceHolder, false);
  // Handle things like 'int (Cls::*A)[4];' correctly.
  // FIXME: this should include vectors, but vectors use attributes I guess.
  if (isa<ArrayType>(T->getPointeeType()))
    OS << ')';
  printAfter(T->getPointeeType(), OS);
}

void TypePrinter::printConstantArrayBefore(const ConstantArrayType *T, 
                                           raw_ostream &OS) {
  IncludeStrongLifetimeRAII Strong(Policy);
  SaveAndRestore<bool> NonEmptyPH(HasEmptyPlaceHolder, false);
  printBefore(T->getElementType(), OS);
}
void TypePrinter::printConstantArrayAfter(const ConstantArrayType *T, 
                                          raw_ostream &OS) {
  OS << '[' << T->getSize().getZExtValue() << ']';
  printAfter(T->getElementType(), OS);
}

void TypePrinter::printIncompleteArrayBefore(const IncompleteArrayType *T, 
                                             raw_ostream &OS) {
  IncludeStrongLifetimeRAII Strong(Policy);
  SaveAndRestore<bool> NonEmptyPH(HasEmptyPlaceHolder, false);
  printBefore(T->getElementType(), OS);
}
void TypePrinter::printIncompleteArrayAfter(const IncompleteArrayType *T, 
                                            raw_ostream &OS) {
  OS << "[]";
  printAfter(T->getElementType(), OS);
}

void TypePrinter::printVariableArrayBefore(const VariableArrayType *T, 
                                           raw_ostream &OS) {
  IncludeStrongLifetimeRAII Strong(Policy);
  SaveAndRestore<bool> NonEmptyPH(HasEmptyPlaceHolder, false);
  printBefore(T->getElementType(), OS);
}
void TypePrinter::printVariableArrayAfter(const VariableArrayType *T, 
                                          raw_ostream &OS) {
  OS << '[';
  if (T->getIndexTypeQualifiers().hasQualifiers()) {
    AppendTypeQualList(OS, T->getIndexTypeCVRQualifiers());
    OS << ' ';
  }

  if (T->getSizeModifier() == VariableArrayType::Static)
    OS << "static";
  else if (T->getSizeModifier() == VariableArrayType::Star)
    OS << '*';

  if (T->getSizeExpr())
    T->getSizeExpr()->printPretty(OS, 0, Policy);
  OS << ']';

  printAfter(T->getElementType(), OS);
}

void TypePrinter::printDependentSizedArrayBefore(
                                               const DependentSizedArrayType *T, 
                                               raw_ostream &OS) {
  IncludeStrongLifetimeRAII Strong(Policy);
  SaveAndRestore<bool> NonEmptyPH(HasEmptyPlaceHolder, false);
  printBefore(T->getElementType(), OS);
}
void TypePrinter::printDependentSizedArrayAfter(
                                               const DependentSizedArrayType *T, 
                                               raw_ostream &OS) {
  OS << '[';
  if (T->getSizeExpr())
    T->getSizeExpr()->printPretty(OS, 0, Policy);
  OS << ']';
  printAfter(T->getElementType(), OS);
}

void TypePrinter::printFunctionProtoBefore(const FunctionProtoType *T, 
                                           raw_ostream &OS) {
  if (T->hasTrailingReturn()) {
    OS << "auto ";
    if (!HasEmptyPlaceHolder)
      OS << '(';
  } else {
    // If needed for precedence reasons, wrap the inner part in grouping parens.
    SaveAndRestore<bool> PrevPHIsEmpty(HasEmptyPlaceHolder, false);
    printBefore(T->getResultType(), OS);
    if (!PrevPHIsEmpty.get())
      OS << '(';
  }
}

void TypePrinter::printFunctionProtoAfter(const FunctionProtoType *T, 
                                          raw_ostream &OS) { 
  // If needed for precedence reasons, wrap the inner part in grouping parens.
  if (!HasEmptyPlaceHolder)
    OS << ')';
  SaveAndRestore<bool> NonEmptyPH(HasEmptyPlaceHolder, false);

  OS << '(';
  {
    ParamPolicyRAII ParamPolicy(Policy);
    for (unsigned i = 0, e = T->getNumArgs(); i != e; ++i) {
      if (i) OS << ", ";
      print(T->getArgType(i), OS, StringRef());
    }
  }
  
  if (T->isVariadic()) {
    if (T->getNumArgs())
      OS << ", ";
    OS << "...";
  } else if (T->getNumArgs() == 0 && !Policy.LangOpts.CPlusPlus) {
    // Do not emit int() if we have a proto, emit 'int(void)'.
    OS << "void";
  }
  
  OS << ')';

  FunctionType::ExtInfo Info = T->getExtInfo();
  switch(Info.getCC()) {
  case CC_Default: break;
  case CC_C:
    OS << " __attribute__((cdecl))";
    break;
  case CC_X86StdCall:
    OS << " __attribute__((stdcall))";
    break;
  case CC_X86FastCall:
    OS << " __attribute__((fastcall))";
    break;
  case CC_X86ThisCall:
    OS << " __attribute__((thiscall))";
    break;
  case CC_X86Pascal:
    OS << " __attribute__((pascal))";
    break;
  case CC_AAPCS:
    OS << " __attribute__((pcs(\"aapcs\")))";
    break;
  case CC_AAPCS_VFP:
    OS << " __attribute__((pcs(\"aapcs-vfp\")))";
    break;
  case CC_PnaclCall:
    OS << " __attribute__((pnaclcall))";
    break;
  case CC_IntelOclBicc:
    OS << " __attribute__((intel_ocl_bicc))";
    break;
  }
  if (Info.getNoReturn())
    OS << " __attribute__((noreturn))";
  if (Info.getRegParm())
    OS << " __attribute__((regparm ("
       << Info.getRegParm() << ")))";

  if (unsigned quals = T->getTypeQuals()) {
    OS << ' ';
    AppendTypeQualList(OS, quals);
  }

  switch (T->getRefQualifier()) {
  case RQ_None:
    break;
    
  case RQ_LValue:
    OS << " &";
    break;
    
  case RQ_RValue:
    OS << " &&";
    break;
  }

  if (T->hasTrailingReturn()) {
    OS << " -> "; 
    print(T->getResultType(), OS, StringRef());
  } else
    printAfter(T->getResultType(), OS);
}

void TypePrinter::printFunctionNoProtoBefore(const FunctionNoProtoType *T, 
                                             raw_ostream &OS) { 
  // If needed for precedence reasons, wrap the inner part in grouping parens.
  SaveAndRestore<bool> PrevPHIsEmpty(HasEmptyPlaceHolder, false);
  printBefore(T->getResultType(), OS);
  if (!PrevPHIsEmpty.get())
    OS << '(';
}
void TypePrinter::printFunctionNoProtoAfter(const FunctionNoProtoType *T, 
                                            raw_ostream &OS) {
  // If needed for precedence reasons, wrap the inner part in grouping parens.
  if (!HasEmptyPlaceHolder)
    OS << ')';
  SaveAndRestore<bool> NonEmptyPH(HasEmptyPlaceHolder, false);
  
  OS << "()";
  if (T->getNoReturnAttr())
    OS << " __attribute__((noreturn))";
  printAfter(T->getResultType(), OS);
}
#endif

void TypePrinter::printTypeSpec(const NamedDecl *D, raw_ostream &OS) {
  IdentifierInfo &II = D->getDeclName();
  OS << II.getName();
  spaceBeforePlaceHolder(OS);
}

#if 0
void TypePrinter::printUnresolvedUsingBefore(const UnresolvedUsingType *T,
                                             raw_ostream &OS) {
  printTypeSpec(T->getDecl(), OS);
}
void TypePrinter::printUnresolvedUsingAfter(const UnresolvedUsingType *T,
                                             raw_ostream &OS) { }
#endif

void TypePrinter::printNameBefore(const NameType *T, raw_ostream &OS) { 
  printTypeSpec(T->getDecl(), OS);
}
void TypePrinter::printNameAfter(const NameType *T, raw_ostream &OS) { } 

#if 0
void TypePrinter::printTypeOfExprBefore(const TypeOfExprType *T,
                                        raw_ostream &OS) {
  OS << "typeof ";
  T->getUnderlyingExpr()->printPretty(OS, 0, Policy);
  spaceBeforePlaceHolder(OS);
}
void TypePrinter::printTypeOfExprAfter(const TypeOfExprType *T,
                                       raw_ostream &OS) { }

void TypePrinter::printTypeOfBefore(const TypeOfType *T, raw_ostream &OS) { 
  OS << "typeof(";
  print(T->getUnderlyingType(), OS, StringRef());
  OS << ')';
  spaceBeforePlaceHolder(OS);
}
void TypePrinter::printTypeOfAfter(const TypeOfType *T, raw_ostream &OS) { } 

void TypePrinter::printDecltypeBefore(const DecltypeType *T, raw_ostream &OS) { 
  OS << "decltype(";
  T->getUnderlyingExpr()->printPretty(OS, 0, Policy);
  OS << ')';
  spaceBeforePlaceHolder(OS);
}
void TypePrinter::printDecltypeAfter(const DecltypeType *T, raw_ostream &OS) { } 

void TypePrinter::printUnaryTransformBefore(const UnaryTransformType *T,
                                            raw_ostream &OS) {
  IncludeStrongLifetimeRAII Strong(Policy);

  switch (T->getUTTKind()) {
    case UnaryTransformType::EnumUnderlyingType:
      OS << "__underlying_type(";
      print(T->getBaseType(), OS, StringRef());
      OS << ')';
      spaceBeforePlaceHolder(OS);
      return;
  }

  printBefore(T->getBaseType(), OS);
}
void TypePrinter::printUnaryTransformAfter(const UnaryTransformType *T,
                                           raw_ostream &OS) {
  IncludeStrongLifetimeRAII Strong(Policy);

  switch (T->getUTTKind()) {
    case UnaryTransformType::EnumUnderlyingType:
      return;
  }

  printAfter(T->getBaseType(), OS);
}

void TypePrinter::printAutoBefore(const AutoType *T, raw_ostream &OS) { 
  // If the type has been deduced, do not print 'auto'.
  if (T->isDeduced()) {
    printBefore(T->getDeducedType(), OS);
  } else {
    OS << "auto";
    spaceBeforePlaceHolder(OS);
  }
}
void TypePrinter::printAutoAfter(const AutoType *T, raw_ostream &OS) { 
  // If the type has been deduced, do not print 'auto'.
  if (T->isDeduced())
    printAfter(T->getDeducedType(), OS);
}

void TypePrinter::printAtomicBefore(const AtomicType *T, raw_ostream &OS) {
  IncludeStrongLifetimeRAII Strong(Policy);

  OS << "_Atomic(";
  print(T->getValueType(), OS, StringRef());
  OS << ')';
  spaceBeforePlaceHolder(OS);
}
void TypePrinter::printAtomicAfter(const AtomicType *T, raw_ostream &OS) { }

/// Appends the given scope to the end of a string.
void TypePrinter::AppendScope(DeclContext *DC, raw_ostream &OS) {
  if (DC->isTranslationUnit()) return;
  if (DC->isFunctionOrMethod()) return;
  AppendScope(DC->getParent(), OS);

  if (NamespaceDecl *NS = dyn_cast<NamespaceDecl>(DC)) {
    if (Policy.SuppressUnwrittenScope && 
        (NS->isAnonymousNamespace() || NS->isInline()))
      return;
    if (NS->getIdentifier())
      OS << NS->getName() << "::";
    else
      OS << "<anonymous>::";
  } else if (ClassTemplateSpecializationDecl *Spec
               = dyn_cast<ClassTemplateSpecializationDecl>(DC)) {
    IncludeStrongLifetimeRAII Strong(Policy);
    OS << Spec->getIdentifier()->getName();
    const TemplateArgumentList &TemplateArgs = Spec->getTemplateArgs();
    TemplateSpecializationType::PrintTemplateArgumentList(OS,
                                            TemplateArgs.data(),
                                            TemplateArgs.size(),
                                            Policy);
    OS << "::";
  } else if (TagDecl *Tag = dyn_cast<TagDecl>(DC)) {
    if (TypedefNameDecl *Typedef = Tag->getTypedefNameForAnonDecl())
      OS << Typedef->getIdentifier()->getName() << "::";
    else if (Tag->getIdentifier())
      OS << Tag->getIdentifier()->getName() << "::";
    else
      return;
  }
}

#endif

void TypePrinter::printStructBefore(const StructType *T, raw_ostream &OS) {
  //if (Policy.SuppressTag)
    //return;

  //bool HasKindDecoration = false;

  // We don't print tags unless this is an elaborated type.
  // In C, we just assume every RecordType is an elaborated type.
  //if (!(Policy.LangOpts.CPlusPlus || Policy.SuppressTagKeyword ||
        //D->getTypedefNameForAnonDecl())) {
    //HasKindDecoration = true;
    //OS << D->getKindName();
    //OS << ' ';
  //}

  // Compute the full nested-name-specifier for this type.
  // In C, this will always be empty except when the type
  // being printed is anonymous within other Record.
  //if (!Policy.SuppressScope)
    //AppendScope(D->getDeclContext(), OS);

  //if (const IdentifierInfo *II = D->getIdentifier())
    //OS << II->getName();
  //else if (TypedefNameDecl *Typedef = D->getTypedefNameForAnonDecl()) {
    //assert(Typedef->getIdentifier() && "Typedef without identifier?");
    //OS << Typedef->getIdentifier()->getName();
  //} else {
    // Make an unambiguous representation for anonymous types, e.g.
    //   <anonymous enum at /usr/include/string.h:120:9>
    
    //if (isa<CXXRecordDecl>(D) && cast<CXXRecordDecl>(D)->isLambda()) {
      //OS << "<lambda";
      //HasKindDecoration = true;
    //} else {
      //OS << "<anonymous";
    //}
    
    //if (Policy.AnonymousTagLocations) {
      // Suppress the redundant tag keyword if we just printed one.
      // We don't have to worry about ElaboratedTypes here because you can't
      // refer to an anonymous type with one.
      //if (!HasKindDecoration)
        OS << "<struct";

      PresumedLoc PLoc =
          PresumedLoc::build(T->getDecl()->getASTContext().getSourceManager(),
                             T->getDecl()->getLocation());
      if (PLoc.isValid()) {
        OS << " at " << PLoc.getFilename()
           << ':' << PLoc.getLine()
           << ':' << PLoc.getColumn();
      }
    //}
    
    OS << '>';
  //}

  spaceBeforePlaceHolder(OS);
}
void TypePrinter::printStructAfter(const StructType *T, raw_ostream &OS) { }

#if 0
void TypePrinter::printEnumBefore(const EnumType *T, raw_ostream &OS) { 
  printTag(T->getDecl(), OS);
}
void TypePrinter::printEnumAfter(const EnumType *T, raw_ostream &OS) { }

void TypePrinter::printTemplateTypeParmBefore(const TemplateTypeParmType *T, 
                                              raw_ostream &OS) { 
  if (IdentifierInfo *Id = T->getIdentifier())
    OS << Id->getName();
  else
    OS << "type-parameter-" << T->getDepth() << '-' << T->getIndex();
  spaceBeforePlaceHolder(OS);
}
void TypePrinter::printTemplateTypeParmAfter(const TemplateTypeParmType *T, 
                                             raw_ostream &OS) { } 

void TypePrinter::printSubstTemplateTypeParmBefore(
                                             const SubstTemplateTypeParmType *T, 
                                             raw_ostream &OS) { 
  IncludeStrongLifetimeRAII Strong(Policy);
  printBefore(T->getReplacementType(), OS);
}
void TypePrinter::printSubstTemplateTypeParmAfter(
                                             const SubstTemplateTypeParmType *T, 
                                             raw_ostream &OS) { 
  IncludeStrongLifetimeRAII Strong(Policy);
  printAfter(T->getReplacementType(), OS);
}

void TypePrinter::printSubstTemplateTypeParmPackBefore(
                                        const SubstTemplateTypeParmPackType *T, 
                                        raw_ostream &OS) { 
  IncludeStrongLifetimeRAII Strong(Policy);
  printTemplateTypeParmBefore(T->getReplacedParameter(), OS);
}
void TypePrinter::printSubstTemplateTypeParmPackAfter(
                                        const SubstTemplateTypeParmPackType *T, 
                                        raw_ostream &OS) { 
  IncludeStrongLifetimeRAII Strong(Policy);
  printTemplateTypeParmAfter(T->getReplacedParameter(), OS);
}

void TypePrinter::printTemplateSpecializationBefore(
                                            const TemplateSpecializationType *T, 
                                            raw_ostream &OS) { 
  IncludeStrongLifetimeRAII Strong(Policy);
  T->getTemplateName().print(OS, Policy);
  
  TemplateSpecializationType::PrintTemplateArgumentList(OS,
                                                        T->getArgs(), 
                                                        T->getNumArgs(), 
                                                        Policy);
  spaceBeforePlaceHolder(OS);
}
void TypePrinter::printTemplateSpecializationAfter(
                                            const TemplateSpecializationType *T, 
                                            raw_ostream &OS) { } 

void TypePrinter::printInjectedClassNameBefore(const InjectedClassNameType *T,
                                               raw_ostream &OS) {
  printTemplateSpecializationBefore(T->getInjectedTST(), OS);
}
void TypePrinter::printInjectedClassNameAfter(const InjectedClassNameType *T,
                                               raw_ostream &OS) { }

void TypePrinter::printElaboratedBefore(const ElaboratedType *T,
                                        raw_ostream &OS) {
  OS << TypeWithKeyword::getKeywordName(T->getKeyword());
  if (T->getKeyword() != ETK_None)
    OS << " ";
  NestedNameSpecifier* Qualifier = T->getQualifier();
  if (Qualifier)
    Qualifier->print(OS, Policy);
  
  ElaboratedTypePolicyRAII PolicyRAII(Policy);
  printBefore(T->getNamedType(), OS);
}
void TypePrinter::printElaboratedAfter(const ElaboratedType *T,
                                        raw_ostream &OS) {
  ElaboratedTypePolicyRAII PolicyRAII(Policy);
  printAfter(T->getNamedType(), OS);
}

void TypePrinter::printParenBefore(const ParenType *T, raw_ostream &OS) {
  if (!HasEmptyPlaceHolder && !isa<FunctionType>(T->getInnerType())) {
    printBefore(T->getInnerType(), OS);
    OS << '(';
  } else
    printBefore(T->getInnerType(), OS);
}
void TypePrinter::printParenAfter(const ParenType *T, raw_ostream &OS) {
  if (!HasEmptyPlaceHolder && !isa<FunctionType>(T->getInnerType())) {
    OS << ')';
    printAfter(T->getInnerType(), OS);
  } else
    printAfter(T->getInnerType(), OS);
}

void TypePrinter::printDependentNameBefore(const DependentNameType *T,
                                           raw_ostream &OS) { 
  OS << TypeWithKeyword::getKeywordName(T->getKeyword());
  if (T->getKeyword() != ETK_None)
    OS << " ";
  
  T->getQualifier()->print(OS, Policy);
  
  OS << T->getIdentifier()->getName();
  spaceBeforePlaceHolder(OS);
}
void TypePrinter::printDependentNameAfter(const DependentNameType *T,
                                          raw_ostream &OS) { } 

void TypePrinter::printDependentTemplateSpecializationBefore(
        const DependentTemplateSpecializationType *T, raw_ostream &OS) { 
  IncludeStrongLifetimeRAII Strong(Policy);
  
  OS << TypeWithKeyword::getKeywordName(T->getKeyword());
  if (T->getKeyword() != ETK_None)
    OS << " ";
  
  if (T->getQualifier())
    T->getQualifier()->print(OS, Policy);    
  OS << T->getIdentifier()->getName();
  TemplateSpecializationType::PrintTemplateArgumentList(OS,
                                                        T->getArgs(),
                                                        T->getNumArgs(),
                                                        Policy);
  spaceBeforePlaceHolder(OS);
}
void TypePrinter::printDependentTemplateSpecializationAfter(
        const DependentTemplateSpecializationType *T, raw_ostream &OS) { } 

void TypePrinter::printPackExpansionBefore(const PackExpansionType *T, 
                                           raw_ostream &OS) {
  printBefore(T->getPattern(), OS);
}
void TypePrinter::printPackExpansionAfter(const PackExpansionType *T, 
                                          raw_ostream &OS) {
  printAfter(T->getPattern(), OS);
  OS << "...";
}

void TypePrinter::printAttributedBefore(const AttributedType *T,
                                        raw_ostream &OS) {
  // Prefer the macro forms of the GC and ownership qualifiers.
  if (T->getAttrKind() == AttributedType::attr_objc_gc ||
      T->getAttrKind() == AttributedType::attr_objc_ownership)
    return printBefore(T->getEquivalentType(), OS);

  printBefore(T->getModifiedType(), OS);
}

void TypePrinter::printAttributedAfter(const AttributedType *T,
                                       raw_ostream &OS) {
  // Prefer the macro forms of the GC and ownership qualifiers.
  if (T->getAttrKind() == AttributedType::attr_objc_gc ||
      T->getAttrKind() == AttributedType::attr_objc_ownership)
    return printAfter(T->getEquivalentType(), OS);

  // TODO: not all attributes are GCC-style attributes.
  OS << " __attribute__((";
  switch (T->getAttrKind()) {
  case AttributedType::attr_address_space:
    OS << "address_space(";
    OS << T->getEquivalentType().getAddressSpace();
    OS << ')';
    break;

  case AttributedType::attr_vector_size: {
    OS << "__vector_size__(";
    if (const VectorType *vector =T->getEquivalentType()->getAs<VectorType>()) {
      OS << vector->getNumElements();
      OS << " * sizeof(";
      print(vector->getElementType(), OS, StringRef());
      OS << ')';
    }
    OS << ')';
    break;
  }

  case AttributedType::attr_neon_vector_type:
  case AttributedType::attr_neon_polyvector_type: {
    if (T->getAttrKind() == AttributedType::attr_neon_vector_type)
      OS << "neon_vector_type(";
    else
      OS << "neon_polyvector_type(";
    const VectorType *vector = T->getEquivalentType()->getAs<VectorType>();
    OS << vector->getNumElements();
    OS << ')';
    break;
  }

  case AttributedType::attr_regparm: {
    OS << "regparm(";
    QualType t = T->getEquivalentType();
    while (!t->isFunctionType())
      t = t->getPointeeType();
    OS << t->getAs<FunctionType>()->getRegParmType();
    OS << ')';
    break;
  }

  case AttributedType::attr_objc_gc: {
    OS << "objc_gc(";

    QualType tmp = T->getEquivalentType();
    while (tmp.getObjCGCAttr() == Qualifiers::GCNone) {
      QualType next = tmp->getPointeeType();
      if (next == tmp) break;
      tmp = next;
    }

    if (tmp.isObjCGCWeak())
      OS << "weak";
    else
      OS << "strong";
    OS << ')';
    break;
  }

  case AttributedType::attr_objc_ownership:
    OS << "objc_ownership(";
    switch (T->getEquivalentType().getObjCLifetime()) {
    case Qualifiers::OCL_None: llvm_unreachable("no ownership!");
    case Qualifiers::OCL_ExplicitNone: OS << "none"; break;
    case Qualifiers::OCL_Strong: OS << "strong"; break;
    case Qualifiers::OCL_Weak: OS << "weak"; break;
    case Qualifiers::OCL_Autoreleasing: OS << "autoreleasing"; break;
    }
    OS << ')';
    break;

  case AttributedType::attr_noreturn: OS << "noreturn"; break;
  case AttributedType::attr_cdecl: OS << "cdecl"; break;
  case AttributedType::attr_fastcall: OS << "fastcall"; break;
  case AttributedType::attr_stdcall: OS << "stdcall"; break;
  case AttributedType::attr_thiscall: OS << "thiscall"; break;
  case AttributedType::attr_pascal: OS << "pascal"; break;
  case AttributedType::attr_pcs: {
    OS << "pcs(";
   QualType t = T->getEquivalentType();
   while (!t->isFunctionType())
     t = t->getPointeeType();
   OS << (t->getAs<FunctionType>()->getCallConv() == CC_AAPCS ?
         "\"aapcs\"" : "\"aapcs-vfp\"");
   OS << ')';
   break;
  }
  case AttributedType::attr_pnaclcall: OS << "pnaclcall"; break;
  case AttributedType::attr_inteloclbicc: OS << "inteloclbicc"; break;
  }
  OS << "))";
}

void TypePrinter::printObjCInterfaceBefore(const ObjCInterfaceType *T, 
                                           raw_ostream &OS) { 
  OS << T->getDecl()->getName();
  spaceBeforePlaceHolder(OS);
}
void TypePrinter::printObjCInterfaceAfter(const ObjCInterfaceType *T, 
                                          raw_ostream &OS) { } 

void TypePrinter::printObjCObjectBefore(const ObjCObjectType *T,
                                        raw_ostream &OS) {
  if (T->qual_empty())
    return printBefore(T->getBaseType(), OS);

  print(T->getBaseType(), OS, StringRef());
  OS << '<';
  bool isFirst = true;
  for (ObjCObjectType::qual_iterator
         I = T->qual_begin(), E = T->qual_end(); I != E; ++I) {
    if (isFirst)
      isFirst = false;
    else
      OS << ',';
    OS << (*I)->getName();
  }
  OS << '>';
  spaceBeforePlaceHolder(OS);
}
void TypePrinter::printObjCObjectAfter(const ObjCObjectType *T,
                                        raw_ostream &OS) {
  if (T->qual_empty())
    return printAfter(T->getBaseType(), OS);
}

void TypePrinter::printObjCObjectPointerBefore(const ObjCObjectPointerType *T, 
                                               raw_ostream &OS) {
  T->getPointeeType().getLocalQualifiers().print(OS, Policy,
                                                /*appendSpaceIfNonEmpty=*/true);

  if (T->isObjCIdType() || T->isObjCQualifiedIdType())
    OS << "id";
  else if (T->isObjCClassType() || T->isObjCQualifiedClassType())
    OS << "Class";
  else if (T->isObjCSelType())
    OS << "SEL";
  else
    OS << T->getInterfaceDecl()->getName();
  
  if (!T->qual_empty()) {
    OS << '<';
    for (ObjCObjectPointerType::qual_iterator I = T->qual_begin(), 
                                              E = T->qual_end();
         I != E; ++I) {
      OS << (*I)->getName();
      if (I+1 != E)
        OS << ',';
    }
    OS << '>';
  }
  
  if (!T->isObjCIdType() && !T->isObjCQualifiedIdType()) {
    OS << " *"; // Don't forget the implicit pointer.
  } else {
    spaceBeforePlaceHolder(OS);
  }
}
void TypePrinter::printObjCObjectPointerAfter(const ObjCObjectPointerType *T, 
                                              raw_ostream &OS) { }

void TemplateSpecializationType::
  PrintTemplateArgumentList(raw_ostream &OS,
                            const TemplateArgumentListInfo &Args,
                            const PrintingPolicy &Policy) {
  return PrintTemplateArgumentList(OS,
                                   Args.getArgumentArray(),
                                   Args.size(),
                                   Policy);
}

void
TemplateSpecializationType::PrintTemplateArgumentList(
                                                raw_ostream &OS,
                                                const TemplateArgument *Args,
                                                unsigned NumArgs,
                                                  const PrintingPolicy &Policy,
                                                      bool SkipBrackets) {
  if (!SkipBrackets)
    OS << '<';
  
  bool needSpace = false;
  for (unsigned Arg = 0; Arg < NumArgs; ++Arg) {
    if (Arg > 0)
      OS << ", ";
    
    // Print the argument into a string.
    SmallString<128> Buf;
    llvm::raw_svector_ostream ArgOS(Buf);
    if (Args[Arg].getKind() == TemplateArgument::Pack) {
      PrintTemplateArgumentList(ArgOS,
                                Args[Arg].pack_begin(), 
                                Args[Arg].pack_size(), 
                                Policy, true);
    } else {
      Args[Arg].print(Policy, ArgOS);
    }
    StringRef ArgString = ArgOS.str();

    // If this is the first argument and its string representation
    // begins with the global scope specifier ('::foo'), add a space
    // to avoid printing the diagraph '<:'.
    if (!Arg && !ArgString.empty() && ArgString[0] == ':')
      OS << ' ';

    OS << ArgString;

    needSpace = (!ArgString.empty() && ArgString.back() == '>');
  }

  // If the last character of our string is '>', add another space to
  // keep the two '>''s separate tokens. We don't *have* to do this in
  // C++0x, but it's still good hygiene.
  if (needSpace)
    OS << ' ';

  if (!SkipBrackets)
    OS << '>';
}

// Sadly, repeat all that with TemplateArgLoc.
void TemplateSpecializationType::
PrintTemplateArgumentList(raw_ostream &OS,
                          const TemplateArgumentLoc *Args, unsigned NumArgs,
                          const PrintingPolicy &Policy) {
  OS << '<';

  bool needSpace = false;
  for (unsigned Arg = 0; Arg < NumArgs; ++Arg) {
    if (Arg > 0)
      OS << ", ";
    
    // Print the argument into a string.
    SmallString<128> Buf;
    llvm::raw_svector_ostream ArgOS(Buf);
    if (Args[Arg].getArgument().getKind() == TemplateArgument::Pack) {
      PrintTemplateArgumentList(ArgOS,
                                Args[Arg].getArgument().pack_begin(), 
                                Args[Arg].getArgument().pack_size(), 
                                Policy, true);
    } else {
      Args[Arg].getArgument().print(Policy, ArgOS);
    }
    StringRef ArgString = ArgOS.str();
    
    // If this is the first argument and its string representation
    // begins with the global scope specifier ('::foo'), add a space
    // to avoid printing the diagraph '<:'.
    if (!Arg && !ArgString.empty() && ArgString[0] == ':')
      OS << ' ';

    OS << ArgString;

    needSpace = (!ArgString.empty() && ArgString.back() == '>');
  }
  
  // If the last character of our string is '>', add another space to
  // keep the two '>''s separate tokens. We don't *have* to do this in
  // C++0x, but it's still good hygiene.
  if (needSpace)
    OS << ' ';

  OS << '>';
}

void QualType::dump(const char *msg) const {
  if (msg)
    llvm::errs() << msg << ": ";
  LangOptions LO;
  print(llvm::errs(), PrintingPolicy(LO), "identifier");
  llvm::errs() << '\n';
}
void QualType::dump() const {
  dump(0);
}

void Type::dump() const {
  QualType(this, 0).dump();
}
#endif

std::string Type::getAsString(/*const PrintingPolicy &Policy*/) const {
  std::string S;
  getAsStringInternal(S/*, Policy*/);
  return S;
}

#if 0
std::string QualType::getAsString(const Type *ty, Qualifiers qs) {
  std::string buffer;
  LangOptions options;
  getAsStringInternal(ty, qs, buffer, PrintingPolicy(options));
  return buffer;
}

void QualType::print(const Type *ty, Qualifiers qs,
                     raw_ostream &OS, const PrintingPolicy &policy,
                     const Twine &PlaceHolder) {
  SmallString<128> PHBuf;
  StringRef PH = PlaceHolder.toStringRef(PHBuf);

  TypePrinter(policy).print(ty, qs, OS, PH);
}
#endif

void Type::getAsStringInternal(const Type *ty, std::string &buffer/*,
                               const PrintingPolicy &policy*/) {
  SmallString<256> Buf;
  llvm::raw_svector_ostream StrOS(Buf);
  TypePrinter(/*policy*/).print(ty, StrOS, buffer);
  std::string str = StrOS.str();
  buffer.swap(str);
}
