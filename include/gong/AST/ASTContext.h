//===--- ASTContext.h - Context to hold long-lived AST nodes ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines the gong::ASTContext interface.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_AST_ASTCONTEXT_H
#define LLVM_GONG_AST_ASTCONTEXT_H

#include "gong/AST/Decl.h"
#include "gong/Basic/LLVM.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Support/Allocator.h"

#if 0
#include "gong/AST/CanonicalType.h"
#include "gong/AST/CommentCommandTraits.h"
#include "gong/AST/LambdaMangleContext.h"
#include "gong/AST/NestedNameSpecifier.h"
#include "gong/AST/PrettyPrinter.h"
#include "gong/AST/RawCommentList.h"
#include "gong/AST/TemplateName.h"
#include "gong/AST/Type.h"
#include "gong/Basic/AddressSpaces.h"
#include "gong/Basic/LangOptions.h"
#include "gong/Basic/OperatorKinds.h"
#include "gong/Basic/PartialDiagnostic.h"
#include "gong/Basic/VersionTuple.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/TinyPtrVector.h"
#include <vector>

namespace llvm {
  struct fltSemantics;
}

#endif
namespace gong {
class IdentifierTable;
#if 0
  class FileManager;
  class ASTRecordLayout;
  class BlockExpr;
  class CharUnits;
  class DiagnosticsEngine;
  class Expr;
  class ExternalASTSource;
  class ASTMutationListener;
  class SelectorTable;
  class TargetInfo;
  class CXXABI;
  // Decls
  class DeclContext;
  class CXXConversionDecl;
  class CXXMethodDecl;
  class CXXRecordDecl;
  class Decl;
  class FieldDecl;
  class MangleContext;
  class ObjCIvarDecl;
  class ObjCIvarRefExpr;
  class ObjCPropertyDecl;
  class ParmVarDecl;
  class RecordDecl;
  class StoredDeclsMap;
  class TagDecl;
  class TemplateTemplateParmDecl;
  class TemplateTypeParmDecl;
  class TranslationUnitDecl;
  class TypeDecl;
  class TypedefNameDecl;
  class UsingDecl;
  class UsingShadowDecl;
  class UnresolvedSetIterator;

  namespace Builtin { class Context; }

  namespace comments {
    class FullComment;
  }

#endif
/// \brief Holds long-lived AST nodes (such as types and decls) that can be
/// referred to throughout the semantic analysis of a file.
class ASTContext : public RefCountedBase<ASTContext> {
  ASTContext &this_() { return *this; }

  mutable std::vector<Type*> Types;

#if 0
  mutable llvm::FoldingSet<ExtQuals> ExtQualNodes;
  mutable llvm::FoldingSet<ComplexType> ComplexTypes;
#endif
  mutable llvm::FoldingSet<PointerType> PointerTypes;
  mutable llvm::ContextualFoldingSet<StructType, ASTContext&> CanStructTypes;
#if 0
  mutable llvm::FoldingSet<BlockPointerType> BlockPointerTypes;
  mutable llvm::FoldingSet<LValueReferenceType> LValueReferenceTypes;
  mutable llvm::FoldingSet<RValueReferenceType> RValueReferenceTypes;
  mutable llvm::FoldingSet<MemberPointerType> MemberPointerTypes;
  mutable llvm::FoldingSet<ConstantArrayType> ConstantArrayTypes;
  mutable llvm::FoldingSet<IncompleteArrayType> IncompleteArrayTypes;
  mutable std::vector<VariableArrayType*> VariableArrayTypes;
  mutable llvm::FoldingSet<DependentSizedArrayType> DependentSizedArrayTypes;
  mutable llvm::FoldingSet<DependentSizedExtVectorType>
    DependentSizedExtVectorTypes;
  mutable llvm::FoldingSet<VectorType> VectorTypes;
  mutable llvm::FoldingSet<FunctionNoProtoType> FunctionNoProtoTypes;
  mutable llvm::ContextualFoldingSet<FunctionProtoType, ASTContext&>
    FunctionProtoTypes;
  mutable llvm::FoldingSet<DependentTypeOfExprType> DependentTypeOfExprTypes;
  mutable llvm::FoldingSet<DependentDecltypeType> DependentDecltypeTypes;
  mutable llvm::FoldingSet<TemplateTypeParmType> TemplateTypeParmTypes;
  mutable llvm::FoldingSet<SubstTemplateTypeParmType>
    SubstTemplateTypeParmTypes;
  mutable llvm::FoldingSet<SubstTemplateTypeParmPackType>
    SubstTemplateTypeParmPackTypes;
  mutable llvm::ContextualFoldingSet<TemplateSpecializationType, ASTContext&>
    TemplateSpecializationTypes;
  mutable llvm::FoldingSet<ParenType> ParenTypes;
  mutable llvm::FoldingSet<ElaboratedType> ElaboratedTypes;
  mutable llvm::FoldingSet<DependentNameType> DependentNameTypes;
  mutable llvm::ContextualFoldingSet<DependentTemplateSpecializationType,
                                     ASTContext&>
    DependentTemplateSpecializationTypes;
  llvm::FoldingSet<PackExpansionType> PackExpansionTypes;
  mutable llvm::FoldingSet<ObjCObjectTypeImpl> ObjCObjectTypes;
  mutable llvm::FoldingSet<ObjCObjectPointerType> ObjCObjectPointerTypes;
  mutable llvm::FoldingSet<AutoType> AutoTypes;
  mutable llvm::FoldingSet<AtomicType> AtomicTypes;
  llvm::FoldingSet<AttributedType> AttributedTypes;

  mutable llvm::FoldingSet<QualifiedTemplateName> QualifiedTemplateNames;
  mutable llvm::FoldingSet<DependentTemplateName> DependentTemplateNames;
  mutable llvm::FoldingSet<SubstTemplateTemplateParmStorage> 
    SubstTemplateTemplateParms;
  mutable llvm::ContextualFoldingSet<SubstTemplateTemplateParmPackStorage,
                                     ASTContext&> 
    SubstTemplateTemplateParmPacks;
  
  /// \brief The set of nested name specifiers.
  ///
  /// This set is managed by the NestedNameSpecifier class.
  mutable llvm::FoldingSet<NestedNameSpecifier> NestedNameSpecifiers;
  mutable NestedNameSpecifier *GlobalNestedNameSpecifier;
  friend class NestedNameSpecifier;

  /// \brief A cache mapping from RecordDecls to ASTRecordLayouts.
  ///
  /// This is lazily created.  This is intentionally not serialized.
  mutable llvm::DenseMap<const RecordDecl*, const ASTRecordLayout*>
    ASTRecordLayouts;

  /// \brief A cache from types to size and alignment information.
  typedef llvm::DenseMap<const Type*,
                         std::pair<uint64_t, unsigned> > TypeInfoMap;
  mutable TypeInfoMap MemoizedTypeInfo;

  /// \brief Mapping from __block VarDecls to their copy initialization expr.
  llvm::DenseMap<const VarDecl*, Expr*> BlockVarCopyInits;
    
  /// \brief Mapping from class scope functions specialization to their
  /// template patterns.
  llvm::DenseMap<const FunctionDecl*, FunctionDecl*>
    ClassScopeSpecializationPattern;

  /// \brief The typedef for the __int128_t type.
  mutable TypedefDecl *Int128Decl;

  /// \brief The typedef for the __uint128_t type.
  mutable TypedefDecl *UInt128Decl;
#endif

  /// \brief The int type.
  mutable TypeSpecDecl *IntDecl;

  /// \brief The bool type.
  mutable TypeSpecDecl *BoolDecl;
  
  /// \brief The float32 type.
  mutable TypeSpecDecl *Float32Decl;

#if 0
  /// \brief The typedef for the target specific predefined
  /// __builtin_va_list type.
  mutable TypedefDecl *BuiltinVaListDecl;

  /// \brief The typedef for the predefined \c id type.
  mutable TypedefDecl *ObjCIdDecl;
  
  /// \brief The typedef for the predefined \c SEL type.
  mutable TypedefDecl *ObjCSelDecl;

  /// \brief The typedef for the predefined \c Class type.
  mutable TypedefDecl *ObjCClassDecl;

  /// \brief The typedef for the predefined \c Protocol class in Objective-C.
  mutable ObjCInterfaceDecl *ObjCProtocolClassDecl;
  
  /// \brief The typedef for the predefined 'BOOL' type.
  mutable TypedefDecl *BOOLDecl;

  // Typedefs which may be provided defining the structure of Objective-C
  // pseudo-builtins
  QualType ObjCIdRedefinitionType;
  QualType ObjCClassRedefinitionType;
  QualType ObjCSelRedefinitionType;

  QualType ObjCConstantStringType;
  mutable RecordDecl *CFConstantStringTypeDecl;
  
  QualType ObjCNSStringType;

  /// \brief The typedef declaration for the Objective-C "instancetype" type.
  TypedefDecl *ObjCInstanceTypeDecl;
  
  /// \brief The type for the C FILE type.
  TypeDecl *FILEDecl;

  /// \brief The type for the C jmp_buf type.
  TypeDecl *jmp_bufDecl;

  /// \brief The type for the C sigjmp_buf type.
  TypeDecl *sigjmp_bufDecl;

  /// \brief The type for the C ucontext_t type.
  TypeDecl *ucontext_tDecl;

  /// \brief Type for the Block descriptor for Blocks CodeGen.
  ///
  /// Since this is only used for generation of debug info, it is not
  /// serialized.
  mutable RecordDecl *BlockDescriptorType;

  /// \brief Type for the Block descriptor for Blocks CodeGen.
  ///
  /// Since this is only used for generation of debug info, it is not
  /// serialized.
  mutable RecordDecl *BlockDescriptorExtendedType;

  TypeSourceInfo NullTypeSourceInfo;

  /// \brief Keeps track of all declaration attributes.
  ///
  /// Since so few decls have attrs, we keep them in a hash map instead of
  /// wasting space in the Decl class.
  llvm::DenseMap<const Decl*, AttrVec*> DeclAttrs;

  /// \brief Mapping that stores the methods overridden by a given C++
  /// member function.
  ///
  /// Since most C++ member functions aren't virtual and therefore
  /// don't override anything, we store the overridden functions in
  /// this map on the side rather than within the CXXMethodDecl structure.
  typedef llvm::TinyPtrVector<const CXXMethodDecl*> CXXMethodVector;
  llvm::DenseMap<const CXXMethodDecl *, CXXMethodVector> OverriddenMethods;

  /// \brief Mapping from each declaration context to its corresponding lambda 
  /// mangling context.
  llvm::DenseMap<const DeclContext *, LambdaMangleContext> LambdaMangleContexts;

  llvm::DenseMap<const DeclContext *, unsigned> UnnamedMangleContexts;
  llvm::DenseMap<const TagDecl *, unsigned> UnnamedMangleNumbers;

  /// \brief Mapping that stores parameterIndex values for ParmVarDecls when
  /// that value exceeds the bitfield size of ParmVarDeclBits.ParameterIndex.
  typedef llvm::DenseMap<const VarDecl *, unsigned> ParameterIndexTable;
  ParameterIndexTable ParamIndices;  
  
  ImportDecl *FirstLocalImport;
  ImportDecl *LastLocalImport;
#endif
  
  TranslationUnitDecl *TUDecl;

  /// \brief The associated SourceManager object.a
  SourceManager &SourceMgr;

#if 0
  /// \brief The language options used to create the AST associated with
  ///  this ASTContext object.
  LangOptions &LangOpts;
#endif

  /// \brief The allocator used to create AST objects.
  ///
  /// AST objects are never destructed; rather, all memory associated with the
  /// AST objects will be released when the ASTContext itself is destroyed.
  mutable llvm::BumpPtrAllocator BumpAlloc;

#if 0
  /// \brief Allocator for partial diagnostics.
  PartialDiagnostic::StorageAllocator DiagAllocator;

  /// \brief The current C++ ABI.
  std::unique_ptr<CXXABI> ABI;
  CXXABI *createCXXABI(const TargetInfo &T);

  /// \brief The logical -> physical address space map.
  const LangAS::Map *AddrSpaceMap;

  friend class ASTDeclReader;
  friend class ASTReader;
  friend class ASTWriter;
  friend class CXXRecordDecl;

  const TargetInfo *Target;
  gong::PrintingPolicy PrintingPolicy;
#endif
  
public:
  IdentifierTable &Idents;
#if 0
  SelectorTable &Selectors;
  Builtin::Context &BuiltinInfo;
  mutable DeclarationNameTable DeclarationNames;
  std::unique_ptr<ExternalASTSource> ExternalSource;
  ASTMutationListener *Listener;

  const gong::PrintingPolicy &getPrintingPolicy() const {
    return PrintingPolicy;
  }

  void setPrintingPolicy(const gong::PrintingPolicy &Policy) {
    PrintingPolicy = Policy;
  }
#endif
  
  SourceManager& getSourceManager() { return SourceMgr; }
  const SourceManager& getSourceManager() const { return SourceMgr; }

  llvm::BumpPtrAllocator &getAllocator() const {
    return BumpAlloc;
  }

  void *Allocate(unsigned Size, unsigned Align = 8) const {
    return BumpAlloc.Allocate(Size, Align);
  }
  void Deallocate(void *Ptr) const { }
  
#if 0
  /// Return the total amount of physical memory allocated for representing
  /// AST nodes and type information.
  size_t getASTAllocatedMemory() const {
    return BumpAlloc.getTotalMemory();
  }
  /// Return the total memory used for various side tables.
  size_t getSideTableAllocatedMemory() const;
  
  PartialDiagnostic::StorageAllocator &getDiagAllocator() {
    return DiagAllocator;
  }

  const TargetInfo &getTargetInfo() const { return *Target; }
  
  const LangOptions& getLangOpts() const { return LangOpts; }

  DiagnosticsEngine &getDiagnostics() const;

  FullSourceLoc getFullLoc(SourceLocation Loc) const {
    return FullSourceLoc(Loc,SourceMgr);
  }

  /// \brief All comments in this translation unit.
  RawCommentList Comments;

  /// \brief True if comments are already loaded from ExternalASTSource.
  mutable bool CommentsLoaded;

  class RawCommentAndCacheFlags {
  public:
    enum Kind {
      /// We searched for a comment attached to the particular declaration, but
      /// didn't find any.
      ///
      /// getRaw() == 0.
      NoCommentInDecl = 0,

      /// We have found a comment attached to this particular declaration.
      ///
      /// getRaw() != 0.
      FromDecl,

      /// This declaration does not have an attached comment, and we have
      /// searched the redeclaration chain.
      ///
      /// If getRaw() == 0, the whole redeclaration chain does not have any
      /// comments.
      ///
      /// If getRaw() != 0, it is a comment propagated from other
      /// redeclaration.
      FromRedecl
    };

    Kind getKind() const LLVM_READONLY {
      return Data.getInt();
    }

    void setKind(Kind K) {
      Data.setInt(K);
    }

    const RawComment *getRaw() const LLVM_READONLY {
      return Data.getPointer();
    }

    void setRaw(const RawComment *RC) {
      Data.setPointer(RC);
    }

    const Decl *getOriginalDecl() const LLVM_READONLY {
      return OriginalDecl;
    }

    void setOriginalDecl(const Decl *Orig) {
      OriginalDecl = Orig;
    }

  private:
    llvm::PointerIntPair<const RawComment *, 2, Kind> Data;
    const Decl *OriginalDecl;
  };

  /// \brief Mapping from declarations to comments attached to any
  /// redeclaration.
  ///
  /// Raw comments are owned by Comments list.  This mapping is populated
  /// lazily.
  mutable llvm::DenseMap<const Decl *, RawCommentAndCacheFlags> RedeclComments;

  /// \brief Mapping from declarations to parsed comments attached to any
  /// redeclaration.
  mutable llvm::DenseMap<const Decl *, comments::FullComment *> ParsedComments;

  /// \brief Return the documentation comment attached to a given declaration,
  /// without looking into cache.
  RawComment *getRawCommentForDeclNoCache(const Decl *D) const;

public:
  RawCommentList &getRawCommentList() {
    return Comments;
  }

  void addComment(const RawComment &RC) {
    assert(LangOpts.RetainCommentsFromSystemHeaders ||
           !SourceMgr.isInSystemHeader(RC.getSourceRange().getBegin()));
    Comments.addComment(RC, BumpAlloc);
  }

  /// \brief Return the documentation comment attached to a given declaration.
  /// Returns NULL if no comment is attached.
  ///
  /// \param OriginalDecl if not NULL, is set to declaration AST node that had
  /// the comment, if the comment we found comes from a redeclaration.
  const RawComment *getRawCommentForAnyRedecl(
                                      const Decl *D,
                                      const Decl **OriginalDecl = NULL) const;

  /// Return parsed documentation comment attached to a given declaration.
  /// Returns NULL if no comment is attached.
  ///
  /// \param PP the Preprocessor used with this TU.  Could be NULL if
  /// preprocessor is not available.
  comments::FullComment *getCommentForDecl(const Decl *D,
                                           const Preprocessor *PP) const;
  
  comments::FullComment *cloneFullComment(comments::FullComment *FC,
                                         const Decl *D) const;

private:
  mutable comments::CommandTraits CommentCommandTraits;

public:
  comments::CommandTraits &getCommentCommandTraits() const {
    return CommentCommandTraits;
  }

  /// \brief Retrieve the attributes for the given declaration.
  AttrVec& getDeclAttrs(const Decl *D);

  /// \brief Erase the attributes corresponding to the given declaration.
  void eraseDeclAttrs(const Decl *D);

  /// \brief Return \c true if \p FD is a zero-length bitfield which follows
  /// the non-bitfield \p LastFD.
  bool ZeroBitfieldFollowsNonBitfield(const FieldDecl *FD, 
                                      const FieldDecl *LastFD) const;

  /// \brief Return \c true if \p FD is a zero-length bitfield which follows
  /// the bitfield \p LastFD.
  bool ZeroBitfieldFollowsBitfield(const FieldDecl *FD,
                                   const FieldDecl *LastFD) const;
  
  /// \brief Return \c true if \p FD is a bitfield which follows the bitfield
  /// \p LastFD.
  bool BitfieldFollowsBitfield(const FieldDecl *FD,
                               const FieldDecl *LastFD) const;
  
  /// \brief Return \c true if \p FD is not a bitfield which follows the
  /// bitfield \p LastFD.
  bool NonBitfieldFollowsBitfield(const FieldDecl *FD,
                                  const FieldDecl *LastFD) const;
  
  /// \brief Return \c true if \p FD is a bitfield which follows the
  /// non-bitfield \p LastFD.
  bool BitfieldFollowsNonBitfield(const FieldDecl *FD,
                                  const FieldDecl *LastFD) const;

  // Access to the set of methods overridden by the given C++ method.
  typedef CXXMethodVector::const_iterator overridden_cxx_method_iterator;
  overridden_cxx_method_iterator
  overridden_methods_begin(const CXXMethodDecl *Method) const;

  overridden_cxx_method_iterator
  overridden_methods_end(const CXXMethodDecl *Method) const;

  unsigned overridden_methods_size(const CXXMethodDecl *Method) const;

  /// \brief Note that the given C++ \p Method overrides the given \p
  /// Overridden method.
  void addOverriddenMethod(const CXXMethodDecl *Method, 
                           const CXXMethodDecl *Overridden);

  /// \brief Return C++ or ObjC overridden methods for the given \p Method.
  ///
  /// An ObjC method is considered to override any method in the class's
  /// base classes, its protocols, or its categories' protocols, that has
  /// the same selector and is of the same kind (class or instance).
  /// A method in an implementation is not considered as overriding the same
  /// method in the interface or its categories.
  void getOverriddenMethods(
                        const NamedDecl *Method,
                        SmallVectorImpl<const NamedDecl *> &Overridden) const;
  
  /// \brief Notify the AST context that a new import declaration has been
  /// parsed or implicitly created within this translation unit.
  void addedLocalImportDecl(ImportDecl *Import);

  static ImportDecl *getNextLocalImport(ImportDecl *Import) {
    return Import->NextLocalImport;
  }
  
  /// \brief Iterator that visits import declarations.
  class import_iterator {
    ImportDecl *Import;
    
  public:
    typedef ImportDecl               *value_type;
    typedef ImportDecl               *reference;
    typedef ImportDecl               *pointer;
    typedef int                       difference_type;
    typedef std::forward_iterator_tag iterator_category;
    
    import_iterator() : Import() { }
    explicit import_iterator(ImportDecl *Import) : Import(Import) { }
    
    reference operator*() const { return Import; }
    pointer operator->() const { return Import; }
    
    import_iterator &operator++() {
      Import = ASTContext::getNextLocalImport(Import);
      return *this;
    }

    import_iterator operator++(int) {
      import_iterator Other(*this);
      ++(*this);
      return Other;
    }
    
    friend bool operator==(import_iterator X, import_iterator Y) {
      return X.Import == Y.Import;
    }

    friend bool operator!=(import_iterator X, import_iterator Y) {
      return X.Import != Y.Import;
    }
  };
  
  import_iterator local_import_begin() const { 
    return import_iterator(FirstLocalImport); 
  }
  import_iterator local_import_end() const { return import_iterator(); }
#endif
  
  TranslationUnitDecl *getTranslationUnitDecl() const { return TUDecl; }

  Type *BoolTy;
  Type *IntTy;
  Type *Int8Ty, *Int16Ty, *Int32Ty, *Int64Ty;
  Type *UInt8Ty, *UInt16Ty, *UInt32Ty, *UInt64Ty;
  Type *Float32Ty, *Float64Ty;
  Type *UnknownAnyTy;
#if 0
  // Builtin Types.
  CanQualType VoidTy;
  CanQualType BoolTy;
  CanQualType CharTy;
  CanQualType WCharTy;  // [C++ 3.9.1p5], integer type in C99.
  CanQualType WIntTy;   // [C99 7.24.1], integer type unchanged by default promotions.
  CanQualType Char16Ty; // [C++0x 3.9.1p5], integer type in C99.
  CanQualType Char32Ty; // [C++0x 3.9.1p5], integer type in C99.
  CanQualType SignedCharTy, ShortTy, IntTy, LongTy, LongLongTy, Int128Ty;
  CanQualType UnsignedCharTy, UnsignedShortTy, UnsignedIntTy, UnsignedLongTy;
  CanQualType UnsignedLongLongTy, UnsignedInt128Ty;
  CanQualType Float32Ty, Float64Ty;
  CanQualType FloatComplexTy, DoubleComplexTy, LongDoubleComplexTy;
  CanQualType VoidPtrTy, NullPtrTy;
  CanQualType BuiltinFnTy;

  // Types for deductions in C++0x [stmt.ranged]'s desugaring. Built on demand.
  mutable QualType AutoDeductTy;     // Deduction against 'auto'.
  mutable QualType AutoRRefDeductTy; // Deduction against 'auto &&'.

  // Type used to help define __builtin_va_list for some targets.
  // The type is built when constructing 'BuiltinVaListDecl'.
  mutable QualType VaListTagTy;
#endif

  ASTContext(/*LangOptions& LOpts,*/ SourceManager &SM, /*const TargetInfo *t,*/
             IdentifierTable &idents/*, SelectorTable &sels,
             Builtin::Context &builtins,
             unsigned size_reserve,
             bool DelayInitialization = false*/);

  ~ASTContext();

#if 0
  /// \brief Attach an external AST source to the AST context.
  ///
  /// The external AST source provides the ability to load parts of
  /// the abstract syntax tree as needed from some external storage,
  /// e.g., a precompiled header.
  void setExternalSource(std::unique_ptr<ExternalASTSource> &Source);

  /// \brief Retrieve a pointer to the external AST source associated
  /// with this AST context, if any.
  ExternalASTSource *getExternalSource() const { return ExternalSource.get(); }

  /// \brief Attach an AST mutation listener to the AST context.
  ///
  /// The AST mutation listener provides the ability to track modifications to
  /// the abstract syntax tree entities committed after they were initially
  /// created.
  void setASTMutationListener(ASTMutationListener *Listener) {
    this->Listener = Listener;
  }

  /// \brief Retrieve a pointer to the AST mutation listener associated
  /// with this AST context, if any.
  ASTMutationListener *getASTMutationListener() const { return Listener; }
#endif
  void PrintStats() const;
#if 0
  const std::vector<Type*>& getTypes() const { return Types; }

  /// \brief Retrieve the declaration for the 128-bit signed integer type.
  TypedefDecl *getInt128Decl() const;

  /// \brief Retrieve the declaration for the 128-bit unsigned integer type.
  TypedefDecl *getUInt128Decl() const;
#endif
  TypeSpecDecl *getIntDecl() const;
  TypeSpecDecl *getBoolDecl() const;
  TypeSpecDecl *getFloat32Decl() const;
  
  //===--------------------------------------------------------------------===//
  //                           Type Constructors
  //===--------------------------------------------------------------------===//

private:
  /// \brief Return a type with extended qualifiers.
  //QualType getExtQualType(const Type *Base, Qualifiers Quals) const;

  const Type *getTypeDeclTypeSlow(const TypeDecl *Decl) const;

public:
#if 0
  /// \brief Return the uniqued reference to the type for an address space
  /// qualified type with the specified type and address space.
  ///
  /// The resulting type has a union of the qualifiers from T and the address
  /// space. If T already has an address space specifier, it is silently
  /// replaced.
  QualType getAddrSpaceQualType(QualType T, unsigned AddressSpace) const;

  /// \brief Return the uniqued reference to the type for a \c restrict
  /// qualified type.
  ///
  /// The resulting type has a union of the qualifiers from \p T and
  /// \c restrict.
  QualType getRestrictType(QualType T) const {
    return T.withFastQualifiers(Qualifiers::Restrict);
  }

  /// \brief Return the uniqued reference to the type for a \c volatile
  /// qualified type.
  ///
  /// The resulting type has a union of the qualifiers from \p T and
  /// \c volatile.
  QualType getVolatileType(QualType T) const {
    return T.withFastQualifiers(Qualifiers::Volatile);
  }

  /// \brief Return the uniqued reference to the type for a \c const
  /// qualified type.
  ///
  /// The resulting type has a union of the qualifiers from \p T and \c const.
  ///
  /// It can be reasonably expected that this will always be equivalent to
  /// calling T.withConst().
  QualType getConstType(QualType T) const { return T.withConst(); }

  /// \brief Change the ExtInfo on a function type.
  const FunctionType *adjustFunctionType(const FunctionType *Fn,
                                         FunctionType::ExtInfo EInfo);

  /// \brief Return the uniqued reference to the type for a complex
  /// number with the specified element type.
  QualType getComplexType(QualType T) const;
  CanQualType getComplexType(CanQualType T) const {
    return CanQualType::CreateUnsafe(getComplexType((QualType) T));
  }
#endif

  /// \brief Return the uniqued reference to the type for a pointer to
  /// the specified type.
  const Type *getPointerType(const Type *) const;
#if 0
  CanQualType getPointerType(CanQualType T) const {
    return CanQualType::CreateUnsafe(getPointerType((QualType) T));
  }

  /// \brief Return the uniqued reference to the atomic type for the specified
  /// type.
  QualType getAtomicType(QualType T) const;

  /// \brief Return the uniqued reference to the type for a block of the
  /// specified type.
  QualType getBlockPointerType(QualType T) const;

  /// Gets the struct used to keep track of the descriptor for pointer to
  /// blocks.
  QualType getBlockDescriptorType() const;

  /// Gets the struct used to keep track of the extended descriptor for
  /// pointer to blocks.
  QualType getBlockDescriptorExtendedType() const;

  /// Returns true iff we need copy/dispose helpers for the given type.
  bool BlockRequiresCopying(QualType Ty, const VarDecl *D);
  
  
  /// Returns true, if given type has a known lifetime. HasByrefExtendedLayout is set
  /// to false in this case. If HasByrefExtendedLayout returns true, byref variable
  /// has extended lifetime. 
  bool getByrefLifetime(QualType Ty,
                        Qualifiers::ObjCLifetime &Lifetime,
                        bool &HasByrefExtendedLayout) const;
  
  /// \brief Return the uniqued reference to the type for an lvalue reference
  /// to the specified type.
  QualType getLValueReferenceType(QualType T, bool SpelledAsLValue = true)
    const;

  /// \brief Return the uniqued reference to the type for an rvalue reference
  /// to the specified type.
  QualType getRValueReferenceType(QualType T) const;

  /// \brief Return the uniqued reference to the type for a member pointer to
  /// the specified type in the specified class.
  ///
  /// The class \p Cls is a \c Type because it could be a dependent name.
  QualType getMemberPointerType(QualType T, const Type *Cls) const;

  /// \brief Return a non-unique reference to the type for a variable array of
  /// the specified element type.
  QualType getVariableArrayType(QualType EltTy, Expr *NumElts,
                                ArrayType::ArraySizeModifier ASM,
                                unsigned IndexTypeQuals,
                                SourceRange Brackets) const;

  /// \brief Return a non-unique reference to the type for a dependently-sized
  /// array of the specified element type.
  ///
  /// FIXME: We will need these to be uniqued, or at least comparable, at some
  /// point.
  QualType getDependentSizedArrayType(QualType EltTy, Expr *NumElts,
                                      ArrayType::ArraySizeModifier ASM,
                                      unsigned IndexTypeQuals,
                                      SourceRange Brackets) const;

  /// \brief Return a unique reference to the type for an incomplete array of
  /// the specified element type.
  QualType getIncompleteArrayType(QualType EltTy,
                                  ArrayType::ArraySizeModifier ASM,
                                  unsigned IndexTypeQuals) const;

  /// \brief Return the unique reference to the type for a constant array of
  /// the specified element type.
  QualType getConstantArrayType(QualType EltTy, const llvm::APInt &ArySize,
                                ArrayType::ArraySizeModifier ASM,
                                unsigned IndexTypeQuals) const;
  
  /// \brief Returns a vla type where known sizes are replaced with [*].
  QualType getVariableArrayDecayedType(QualType Ty) const;

  /// \brief Return the unique reference to a vector type of the specified
  /// element type and size.
  ///
  /// \pre \p VectorType must be a built-in type.
  QualType getVectorType(QualType VectorType, unsigned NumElts,
                         VectorType::VectorKind VecKind) const;

  /// \brief Return the unique reference to an extended vector type
  /// of the specified element type and size.
  ///
  /// \pre \p VectorType must be a built-in type.
  QualType getExtVectorType(QualType VectorType, unsigned NumElts) const;

  /// \pre Return a non-unique reference to the type for a dependently-sized
  /// vector of the specified element type.
  ///
  /// FIXME: We will need these to be uniqued, or at least comparable, at some
  /// point.
  QualType getDependentSizedExtVectorType(QualType VectorType,
                                          Expr *SizeExpr,
                                          SourceLocation AttrLoc) const;

  /// \brief Return a K&R style C function type like 'int()'.
  QualType getFunctionNoProtoType(QualType ResultTy,
                                  const FunctionType::ExtInfo &Info) const;

  QualType getFunctionNoProtoType(QualType ResultTy) const {
    return getFunctionNoProtoType(ResultTy, FunctionType::ExtInfo());
  }

  /// \brief Return a normal function type with a typed argument list.
  QualType getFunctionType(QualType ResultTy,
                           const QualType *Args, unsigned NumArgs,
                           const FunctionProtoType::ExtProtoInfo &EPI) const;
#endif

  /// \brief Return the unique reference to the type for the specified type
  /// declaration.
  const Type * getTypeDeclType(const TypeDecl *Decl/*,
                           const TypeDecl *PrevDecl = 0*/) const {
    assert(Decl && "Passed null for Decl param");
    if (!Decl) return NULL;  // FIXME
    if (Decl->TypeForDecl)
      return Decl->TypeForDecl;

    //if (PrevDecl) {
    //  assert(PrevDecl->TypeForDecl && "previous decl has no TypeForDecl");
    //  Decl->TypeForDecl = PrevDecl->TypeForDecl;
    //  return QualType(PrevDecl->TypeForDecl, 0);
    //}

    return getTypeDeclTypeSlow(Decl);
  }

  const Type *getNameType(const NameTypeDecl *Decl) const;
  const Type *getStructType(const StructTypeDecl *Decl) const;

#if 0
  QualType getEnumType(const EnumDecl *Decl) const;

  QualType getInjectedClassNameType(CXXRecordDecl *Decl, QualType TST) const;

  QualType getAttributedType(AttributedType::Kind attrKind,
                             QualType modifiedType,
                             QualType equivalentType);

  QualType getParenType(QualType NamedType) const;

  QualType getElaboratedType(ElaboratedTypeKeyword Keyword,
                             NestedNameSpecifier *NNS,
                             QualType NamedType) const;
  QualType getDependentNameType(ElaboratedTypeKeyword Keyword,
                                NestedNameSpecifier *NNS,
                                const IdentifierInfo *Name,
                                QualType Canon = QualType()) const;

  /// \brief GCC extension.
  QualType getTypeOfExprType(Expr *e) const;
  QualType getTypeOfType(QualType t) const;

  /// \brief C++11 decltype.
  QualType getDecltypeType(Expr *e, QualType UnderlyingType) const;

  /// \brief Unary type transforms
  QualType getUnaryTransformType(QualType BaseType, QualType UnderlyingType,
                                 UnaryTransformType::UTTKind UKind) const;

  /// \brief C++11 deduced auto type.
  QualType getAutoType(QualType DeducedType) const;

  /// \brief C++11 deduction pattern for 'auto' type.
  QualType getAutoDeductType() const;

  /// \brief C++11 deduction pattern for 'auto &&' type.
  QualType getAutoRRefDeductType() const;

  /// \brief Return the unique reference to the type for the specified TagDecl
  /// (struct/union/class/enum) decl.
  QualType getTagDeclType(const TagDecl *Decl) const;

  /// \brief Return the unique type for "size_t" (C99 7.17), defined in
  /// <stddef.h>.
  ///
  /// The sizeof operator requires this (C99 6.5.3.4p4).
  CanQualType getSizeType() const;

  /// \brief Return the unique type for "intmax_t" (C99 7.18.1.5), defined in
  /// <stdint.h>.
  CanQualType getIntMaxType() const;

  /// \brief Return the unique type for "uintmax_t" (C99 7.18.1.5), defined in
  /// <stdint.h>.
  CanQualType getUIntMaxType() const;

  /// \brief In C++, this returns the unique wchar_t type.  In C99, this
  /// returns a type compatible with the type defined in <stddef.h> as defined
  /// by the target.
  QualType getWCharType() const { return WCharTy; }

  /// \brief Return the type of "signed wchar_t".
  ///
  /// Used when in C++, as a GCC extension.
  QualType getSignedWCharType() const;

  /// \brief Return the type of "unsigned wchar_t".
  ///
  /// Used when in C++, as a GCC extension.
  QualType getUnsignedWCharType() const;

  /// \brief In C99, this returns a type compatible with the type
  /// defined in <stddef.h> as defined by the target.
  QualType getWIntType() const { return WIntTy; }

  /// \brief Return the unique type for "ptrdiff_t" (C99 7.17) defined in
  /// <stddef.h>. Pointer - pointer requires this (C99 6.5.6p9).
  QualType getPointerDiffType() const;

  /// \brief Return the unique type for "pid_t" defined in
  /// <sys/types.h>. We need this to compute the correct type for vfork().
  QualType getProcessIDType() const;

  /// \brief Set the type for the C FILE type.
  void setFILEDecl(TypeDecl *FILEDecl) { this->FILEDecl = FILEDecl; }

  /// \brief Retrieve the C FILE type.
  QualType getFILEType() const {
    if (FILEDecl)
      return getTypeDeclType(FILEDecl);
    return QualType();
  }

  /// \brief Set the type for the C jmp_buf type.
  void setjmp_bufDecl(TypeDecl *jmp_bufDecl) {
    this->jmp_bufDecl = jmp_bufDecl;
  }

  /// \brief Retrieve the C jmp_buf type.
  QualType getjmp_bufType() const {
    if (jmp_bufDecl)
      return getTypeDeclType(jmp_bufDecl);
    return QualType();
  }

  /// \brief Set the type for the C sigjmp_buf type.
  void setsigjmp_bufDecl(TypeDecl *sigjmp_bufDecl) {
    this->sigjmp_bufDecl = sigjmp_bufDecl;
  }

  /// \brief Retrieve the C sigjmp_buf type.
  QualType getsigjmp_bufType() const {
    if (sigjmp_bufDecl)
      return getTypeDeclType(sigjmp_bufDecl);
    return QualType();
  }

  /// \brief Set the type for the C ucontext_t type.
  void setucontext_tDecl(TypeDecl *ucontext_tDecl) {
    this->ucontext_tDecl = ucontext_tDecl;
  }

  /// \brief Retrieve the C ucontext_t type.
  QualType getucontext_tType() const {
    if (ucontext_tDecl)
      return getTypeDeclType(ucontext_tDecl);
    return QualType();
  }

  /// \brief The result type of logical operations, '<', '>', '!=', etc.
  QualType getLogicalOperationType() const {
    return getLangOpts().CPlusPlus ? BoolTy : IntTy;
  }

  /// \brief Retrieve the C type declaration corresponding to the predefined
  /// \c __builtin_va_list type.
  TypedefDecl *getBuiltinVaListDecl() const;

  /// \brief Retrieve the type of the \c __builtin_va_list type.
  QualType getBuiltinVaListType() const {
    return getTypeDeclType(getBuiltinVaListDecl());
  }

  /// \brief Retrieve the C type declaration corresponding to the predefined
  /// \c __va_list_tag type used to help define the \c __builtin_va_list type
  /// for some targets.
  QualType getVaListTagType() const;

  /// \brief Return a type with additional \c const, \c volatile, or
  /// \c restrict qualifiers.
  QualType getCVRQualifiedType(QualType T, unsigned CVR) const {
    return getQualifiedType(T, Qualifiers::fromCVRMask(CVR));
  }

  /// \brief Un-split a SplitQualType.
  QualType getQualifiedType(SplitQualType split) const {
    return getQualifiedType(split.Ty, split.Quals);
  }

  /// \brief Return a type with additional qualifiers.
  QualType getQualifiedType(QualType T, Qualifiers Qs) const {
    if (!Qs.hasNonFastQualifiers())
      return T.withFastQualifiers(Qs.getFastQualifiers());
    QualifierCollector Qc(Qs);
    const Type *Ptr = Qc.strip(T);
    return getExtQualType(Ptr, Qc);
  }

  /// \brief Return a type with additional qualifiers.
  QualType getQualifiedType(const Type *T, Qualifiers Qs) const {
    if (!Qs.hasNonFastQualifiers())
      return QualType(T, Qs.getFastQualifiers());
    return getExtQualType(T, Qs);
  }

  /// \brief Return a type with the given lifetime qualifier.
  ///
  /// \pre Neither type.ObjCLifetime() nor \p lifetime may be \c OCL_None.
  QualType getLifetimeQualifiedType(QualType type,
                                    Qualifiers::ObjCLifetime lifetime) {
    assert(type.getObjCLifetime() == Qualifiers::OCL_None);
    assert(lifetime != Qualifiers::OCL_None);

    Qualifiers qs;
    qs.addObjCLifetime(lifetime);
    return getQualifiedType(type, qs);
  }

  DeclarationNameInfo getNameForTemplate(TemplateName Name,
                                         SourceLocation NameLoc) const;

  enum GetBuiltinTypeError {
    GE_None,              ///< No error
    GE_Missing_stdio,     ///< Missing a type from <stdio.h>
    GE_Missing_setjmp,    ///< Missing a type from <setjmp.h>
    GE_Missing_ucontext   ///< Missing a type from <ucontext.h>
  };

  /// \brief Return the type for the specified builtin.
  ///
  /// If \p IntegerConstantArgs is non-null, it is filled in with a bitmask of
  /// arguments to the builtin that are required to be integer constant
  /// expressions.
  QualType GetBuiltinType(unsigned ID, GetBuiltinTypeError &Error,
                          unsigned *IntegerConstantArgs = 0) const;

private:
  CanQualType getFromTargetType(unsigned Type) const;
  std::pair<uint64_t, unsigned> getTypeInfoImpl(const Type *T) const;

  //===--------------------------------------------------------------------===//
  //                         Type Predicates.
  //===--------------------------------------------------------------------===//

public:
  /// \brief Return true if the given vector types are of the same unqualified
  /// type or if they are equivalent to the same GCC vector type.
  ///
  /// \note This ignores whether they are target-specific (AltiVec or Neon)
  /// types.
  bool areCompatibleVectorTypes(QualType FirstVec, QualType SecondVec);

  //===--------------------------------------------------------------------===//
  //                         Type Sizing and Analysis
  //===--------------------------------------------------------------------===//

  /// \brief Return the APFloat 'semantics' for the specified scalar floating
  /// point type.
  const llvm::fltSemantics &getFloatTypeSemantics(QualType T) const;

  /// \brief Get the size and alignment of the specified complete type in bits.
  std::pair<uint64_t, unsigned> getTypeInfo(const Type *T) const;
  std::pair<uint64_t, unsigned> getTypeInfo(QualType T) const {
    return getTypeInfo(T.getTypePtr());
  }

  /// \brief Return the size of the specified (complete) type \p T, in bits.
  uint64_t getTypeSize(QualType T) const {
    return getTypeInfo(T).first;
  }
  uint64_t getTypeSize(const Type *T) const {
    return getTypeInfo(T).first;
  }

  /// \brief Return the size of the character type, in bits.
  uint64_t getCharWidth() const {
    return getTypeSize(CharTy);
  }
  
  /// \brief Convert a size in bits to a size in characters.
  CharUnits toCharUnitsFromBits(int64_t BitSize) const;

  /// \brief Convert a size in characters to a size in bits.
  int64_t toBits(CharUnits CharSize) const;

  /// \brief Return the size of the specified (complete) type \p T, in
  /// characters.
  CharUnits getTypeSizeInChars(QualType T) const;
  CharUnits getTypeSizeInChars(const Type *T) const;

  /// \brief Return the ABI-specified alignment of a (complete) type \p T, in
  /// bits.
  unsigned getTypeAlign(QualType T) const {
    return getTypeInfo(T).second;
  }
  unsigned getTypeAlign(const Type *T) const {
    return getTypeInfo(T).second;
  }

  /// \brief Return the ABI-specified alignment of a (complete) type \p T, in 
  /// characters.
  CharUnits getTypeAlignInChars(QualType T) const;
  CharUnits getTypeAlignInChars(const Type *T) const;
  
  // getTypeInfoDataSizeInChars - Return the size of a type, in chars. If the
  // type is a record, its data size is returned.
  std::pair<CharUnits, CharUnits> getTypeInfoDataSizeInChars(QualType T) const;

  std::pair<CharUnits, CharUnits> getTypeInfoInChars(const Type *T) const;
  std::pair<CharUnits, CharUnits> getTypeInfoInChars(QualType T) const;

  /// \brief Return the "preferred" alignment of the specified type \p T for
  /// the current target, in bits.
  ///
  /// This can be different than the ABI alignment in cases where it is
  /// beneficial for performance to overalign a data type.
  unsigned getPreferredTypeAlign(const Type *T) const;

  /// \brief Return a conservative estimate of the alignment of the specified
  /// decl \p D.
  ///
  /// \pre \p D must not be a bitfield type, as bitfields do not have a valid
  /// alignment.
  ///
  /// If \p RefAsPointee, references are treated like their underlying type
  /// (for alignof), else they're treated like pointers (for CodeGen).
  CharUnits getDeclAlign(const Decl *D, bool RefAsPointee = false) const;

  /// \brief Get or compute information about the layout of the specified
  /// record (struct/union/class) \p D, which indicates its size and field
  /// position information.
  const ASTRecordLayout &getASTRecordLayout(const RecordDecl *D) const;

  void DumpRecordLayout(const RecordDecl *RD, raw_ostream &OS,
                        bool Simple = false) const;

  /// \brief Get the key function for the given record decl, or NULL if there
  /// isn't one.
  ///
  /// The key function is, according to the Itanium C++ ABI section 5.2.3:
  ///
  /// ...the first non-pure virtual function that is not inline at the point
  /// of class definition.
  const CXXMethodDecl *getKeyFunction(const CXXRecordDecl *RD);

  /// Get the offset of a FieldDecl or IndirectFieldDecl, in bits.
  uint64_t getFieldOffset(const ValueDecl *FD) const;

  bool isNearlyEmpty(const CXXRecordDecl *RD) const;

  MangleContext *createMangleContext();
  
  //===--------------------------------------------------------------------===//
  //                            Type Operators
  //===--------------------------------------------------------------------===//

  /// \brief Return the canonical (structural) type corresponding to the
  /// specified potentially non-canonical type \p T.
  ///
  /// The non-canonical version of a type may have many "decorated" versions of
  /// types.  Decorators can include typedefs, 'typeof' operators, etc. The
  /// returned type is guaranteed to be free of any of these, allowing two
  /// canonical types to be compared for exact equality with a simple pointer
  /// comparison.
  CanQualType getCanonicalType(QualType T) const {
    return CanQualType::CreateUnsafe(T.getCanonicalType());
  }

  const Type *getCanonicalType(const Type *T) const {
    return T->getCanonicalTypeInternal().getTypePtr();
  }

  /// \brief Return the canonical parameter type corresponding to the specific
  /// potentially non-canonical one.
  ///
  /// Qualifiers are stripped off, functions are turned into function
  /// pointers, and arrays decay one level into pointers.
  CanQualType getCanonicalParamType(QualType T) const;

  /// \brief Determine whether the given types \p T1 and \p T2 are equivalent.
  bool hasSameType(QualType T1, QualType T2) const {
    return getCanonicalType(T1) == getCanonicalType(T2);
  }
#endif

  /// \brief Determine whether the given types \p T1 and \p T2 are identical.
  bool isIdenticalType(const Type *T1, const Type *T2) const {
    return T1->getCanonicalType() == T2->getCanonicalType();
  }

#if 0

  /// \brief Return this type as a completely-unqualified array type,
  /// capturing the qualifiers in \p Quals.
  ///
  /// This will remove the minimal amount of sugaring from the types, similar
  /// to the behavior of QualType::getUnqualifiedType().
  ///
  /// \param T is the qualified type, which may be an ArrayType
  ///
  /// \param Quals will receive the full set of qualifiers that were
  /// applied to the array.
  ///
  /// \returns if this is an array type, the completely unqualified array type
  /// that corresponds to it. Otherwise, returns T.getUnqualifiedType().
  QualType getUnqualifiedArrayType(QualType T, Qualifiers &Quals);

  /// \brief Determine whether the given types are equivalent after
  /// cvr-qualifiers have been removed.
  bool hasSameUnqualifiedType(QualType T1, QualType T2) const {
    return getCanonicalType(T1).getTypePtr() ==
           getCanonicalType(T2).getTypePtr();
  }

  bool UnwrapSimilarPointerTypes(QualType &T1, QualType &T2);
  
  /// \brief Retrieves the "canonical" nested name specifier for a
  /// given nested name specifier.
  ///
  /// The canonical nested name specifier is a nested name specifier
  /// that uniquely identifies a type or namespace within the type
  /// system. For example, given:
  ///
  /// \code
  /// namespace N {
  ///   struct S {
  ///     template<typename T> struct X { typename T* type; };
  ///   };
  /// }
  ///
  /// template<typename T> struct Y {
  ///   typename N::S::X<T>::type member;
  /// };
  /// \endcode
  ///
  /// Here, the nested-name-specifier for N::S::X<T>:: will be
  /// S::X<template-param-0-0>, since 'S' and 'X' are uniquely defined
  /// by declarations in the type system and the canonical type for
  /// the template type parameter 'T' is template-param-0-0.
  NestedNameSpecifier *
  getCanonicalNestedNameSpecifier(NestedNameSpecifier *NNS) const;

  /// \brief Retrieves the default calling convention to use for
  /// C++ instance methods.
  CallingConv getDefaultCXXMethodCallConv(bool isVariadic);

  /// \brief Retrieves the canonical representation of the given
  /// calling convention.
  CallingConv getCanonicalCallConv(CallingConv CC) const;

  /// \brief Determines whether two calling conventions name the same
  /// calling convention.
  bool isSameCallConv(CallingConv lcc, CallingConv rcc) {
    return (getCanonicalCallConv(lcc) == getCanonicalCallConv(rcc));
  }

  /// Type Query functions.  If the type is an instance of the specified class,
  /// return the Type pointer for the underlying maximally pretty type.  This
  /// is a member of ASTContext because this may need to do some amount of
  /// canonicalization, e.g. to move type qualifiers into the element type.
  const ArrayType *getAsArrayType(QualType T) const;
  const ConstantArrayType *getAsConstantArrayType(QualType T) const {
    return dyn_cast_or_null<ConstantArrayType>(getAsArrayType(T));
  }
  const VariableArrayType *getAsVariableArrayType(QualType T) const {
    return dyn_cast_or_null<VariableArrayType>(getAsArrayType(T));
  }
  const IncompleteArrayType *getAsIncompleteArrayType(QualType T) const {
    return dyn_cast_or_null<IncompleteArrayType>(getAsArrayType(T));
  }
  const DependentSizedArrayType *getAsDependentSizedArrayType(QualType T)
    const {
    return dyn_cast_or_null<DependentSizedArrayType>(getAsArrayType(T));
  }
  
  /// \brief Return the innermost element type of an array type.
  ///
  /// For example, will return "int" for int[m][n]
  QualType getBaseElementType(const ArrayType *VAT) const;

  /// \brief Return the innermost element type of a type (which needn't
  /// actually be an array type).
  QualType getBaseElementType(QualType QT) const;

  /// \brief Return number of constant array elements.
  uint64_t getConstantArrayElementCount(const ConstantArrayType *CA) const;

  /// \brief Perform adjustment on the parameter type of a function.
  ///
  /// This routine adjusts the given parameter type @p T to the actual
  /// parameter type used by semantic analysis (C99 6.7.5.3p[7,8],
  /// C++ [dcl.fct]p3). The adjusted parameter type is returned.
  QualType getAdjustedParameterType(QualType T) const;
  
  /// \brief Retrieve the parameter type as adjusted for use in the signature
  /// of a function, decaying array and function types and removing top-level
  /// cv-qualifiers.
  QualType getSignatureParameterType(QualType T) const;
  
  /// \brief Return the properly qualified result of decaying the specified
  /// array type to a pointer.
  ///
  /// This operation is non-trivial when handling typedefs etc.  The canonical
  /// type of \p T must be an array type, this returns a pointer to a properly
  /// qualified element of the array.
  ///
  /// See C99 6.7.5.3p7 and C99 6.3.2.1p3.
  QualType getArrayDecayedType(QualType T) const;

  /// \brief Return the type that \p PromotableType will promote to: C99
  /// 6.3.1.1p2, assuming that \p PromotableType is a promotable integer type.
  QualType getPromotedIntegerType(QualType PromotableType) const;

  /// \brief Whether this is a promotable bitfield reference according
  /// to C99 6.3.1.1p2, bullet 2 (and GCC extensions).
  ///
  /// \returns the type this bit-field will promote to, or NULL if no
  /// promotion occurs.
  QualType isPromotableBitField(Expr *E) const;

  /// \brief Return the highest ranked integer type, see C99 6.3.1.8p1. 
  ///
  /// If \p LHS > \p RHS, returns 1.  If \p LHS == \p RHS, returns 0.  If
  /// \p LHS < \p RHS, return -1.
  int getIntegerTypeOrder(QualType LHS, QualType RHS) const;

  /// \brief Compare the rank of the two specified floating point types,
  /// ignoring the domain of the type (i.e. 'double' == '_Complex double').
  ///
  /// If \p LHS > \p RHS, returns 1.  If \p LHS == \p RHS, returns 0.  If
  /// \p LHS < \p RHS, return -1.
  int getFloatingTypeOrder(QualType LHS, QualType RHS) const;

  /// \brief Return a real floating point or a complex type (based on
  /// \p typeDomain/\p typeSize).
  ///
  /// \param typeDomain a real floating point or complex type.
  /// \param typeSize a real floating point or complex type.
  QualType getFloatingTypeOfSizeWithinDomain(QualType typeSize,
                                             QualType typeDomain) const;

  unsigned getTargetAddressSpace(QualType T) const {
    return getTargetAddressSpace(T.getQualifiers());
  }

  unsigned getTargetAddressSpace(Qualifiers Q) const {
    return getTargetAddressSpace(Q.getAddressSpace());
  }

  unsigned getTargetAddressSpace(unsigned AS) const {
    if (AS < LangAS::Offset || AS >= LangAS::Offset + LangAS::Count)
      return AS;
    else
      return (*AddrSpaceMap)[AS - LangAS::Offset];
  }

private:
  // Helper for integer ordering
  unsigned getIntegerRank(const Type *T) const;

public:

  //===--------------------------------------------------------------------===//
  //                    Type Compatibility Predicates
  //===--------------------------------------------------------------------===//

  /// Compatibility predicates used to check assignment expressions.
  bool typesAreCompatible(QualType T1, QualType T2, 
                          bool CompareUnqualified = false); // C99 6.2.7p1

  bool propertyTypesAreCompatible(QualType, QualType); 
  bool typesAreBlockPointerCompatible(QualType, QualType); 

  // Functions for calculating composite types
  QualType mergeTypes(QualType, QualType, bool OfBlockPointer=false,
                      bool Unqualified = false, bool BlockReturnType = false);
  QualType mergeFunctionTypes(QualType, QualType, bool OfBlockPointer=false,
                              bool Unqualified = false);
  QualType mergeFunctionArgumentTypes(QualType, QualType,
                                      bool OfBlockPointer=false,
                                      bool Unqualified = false);
  QualType mergeTransparentUnionType(QualType, QualType,
                                     bool OfBlockPointer=false,
                                     bool Unqualified = false);
  
  bool FunctionTypesMatchOnNSConsumedAttrs(
         const FunctionProtoType *FromFunctionType,
         const FunctionProtoType *ToFunctionType);

  //===--------------------------------------------------------------------===//
  //                    Integer Predicates
  //===--------------------------------------------------------------------===//

  // The width of an integer, as defined in C99 6.2.6.2. This is the number
  // of bits in an integer type excluding any padding bits.
  unsigned getIntWidth(QualType T) const;

  // Per C99 6.2.5p6, for every signed integer type, there is a corresponding
  // unsigned integer type.  This method takes a signed type, and returns the
  // corresponding unsigned integer type.
  QualType getCorrespondingUnsignedType(QualType T) const;

  //===--------------------------------------------------------------------===//
  //                    Type Iterators.
  //===--------------------------------------------------------------------===//

  typedef std::vector<Type*>::iterator       type_iterator;
  typedef std::vector<Type*>::const_iterator const_type_iterator;

  type_iterator types_begin() { return Types.begin(); }
  type_iterator types_end() { return Types.end(); }
  const_type_iterator types_begin() const { return Types.begin(); }
  const_type_iterator types_end() const { return Types.end(); }

  //===--------------------------------------------------------------------===//
  //                    Integer Values
  //===--------------------------------------------------------------------===//

  /// \brief Make an APSInt of the appropriate width and signedness for the
  /// given \p Value and integer \p Type.
  llvm::APSInt MakeIntValue(uint64_t Value, QualType Type) const {
    llvm::APSInt Res(getIntWidth(Type), 
                     !Type->isSignedIntegerOrEnumerationType());
    Res = Value;
    return Res;
  }

  bool isSentinelNullExpr(const Expr *E);
  
  /// \brief Set the copy inialization expression of a block var decl.
  void setBlockVarCopyInits(VarDecl*VD, Expr* Init);
  /// \brief Get the copy initialization expression of the VarDecl \p VD, or
  /// NULL if none exists.
  Expr *getBlockVarCopyInits(const VarDecl* VD);

  /// \brief Allocate an uninitialized TypeSourceInfo.
  ///
  /// The caller should initialize the memory held by TypeSourceInfo using
  /// the TypeLoc wrappers.
  ///
  /// \param T the type that will be the basis for type source info. This type
  /// should refer to how the declarator was written in source code, not to
  /// what type semantic analysis resolved the declarator to.
  ///
  /// \param Size the size of the type info to create, or 0 if the size
  /// should be calculated based on the type.
  TypeSourceInfo *CreateTypeSourceInfo(QualType T, unsigned Size = 0) const;

  /// \brief Allocate a TypeSourceInfo where all locations have been
  /// initialized to a given location, which defaults to the empty
  /// location.
  TypeSourceInfo *
  getTrivialTypeSourceInfo(QualType T, 
                           SourceLocation Loc = SourceLocation()) const;

  TypeSourceInfo *getNullTypeSourceInfo() { return &NullTypeSourceInfo; }

  /// \brief Add a deallocation callback that will be invoked when the 
  /// ASTContext is destroyed.
  ///
  /// \param Callback A callback function that will be invoked on destruction.
  ///
  /// \param Data Pointer data that will be provided to the callback function
  /// when it is called.
  void AddDeallocation(void (*Callback)(void*), void *Data);

  GVALinkage GetGVALinkageForFunction(const FunctionDecl *FD);
  GVALinkage GetGVALinkageForVariable(const VarDecl *VD);

  /// \brief Determines if the decl can be CodeGen'ed or deserialized from PCH
  /// lazily, only when used; this is only relevant for function or file scoped
  /// var definitions.
  ///
  /// \returns true if the function/var must be CodeGen'ed/deserialized even if
  /// it is not used.
  bool DeclMustBeEmitted(const Decl *D);

  void addUnnamedTag(const TagDecl *Tag);
  int getUnnamedTagManglingNumber(const TagDecl *Tag) const;

  /// \brief Retrieve the lambda mangling number for a lambda expression.
  unsigned getLambdaManglingNumber(CXXMethodDecl *CallOperator);
  
  /// \brief Used by ParmVarDecl to store on the side the
  /// index of the parameter when it exceeds the size of the normal bitfield.
  void setParameterIndex(const ParmVarDecl *D, unsigned index);

  /// \brief Used by ParmVarDecl to retrieve on the side the
  /// index of the parameter when it exceeds the size of the normal bitfield.
  unsigned getParameterIndex(const ParmVarDecl *D) const;
  
  //===--------------------------------------------------------------------===//
  //                    Statistics
  //===--------------------------------------------------------------------===//

  /// \brief The number of implicitly-declared default constructors.
  static unsigned NumImplicitDefaultConstructors;
  
  /// \brief The number of implicitly-declared default constructors for 
  /// which declarations were built.
  static unsigned NumImplicitDefaultConstructorsDeclared;

  /// \brief The number of implicitly-declared copy constructors.
  static unsigned NumImplicitCopyConstructors;
  
  /// \brief The number of implicitly-declared copy constructors for 
  /// which declarations were built.
  static unsigned NumImplicitCopyConstructorsDeclared;

  /// \brief The number of implicitly-declared move constructors.
  static unsigned NumImplicitMoveConstructors;

  /// \brief The number of implicitly-declared move constructors for
  /// which declarations were built.
  static unsigned NumImplicitMoveConstructorsDeclared;

  /// \brief The number of implicitly-declared copy assignment operators.
  static unsigned NumImplicitCopyAssignmentOperators;
  
  /// \brief The number of implicitly-declared copy assignment operators for 
  /// which declarations were built.
  static unsigned NumImplicitCopyAssignmentOperatorsDeclared;

  /// \brief The number of implicitly-declared move assignment operators.
  static unsigned NumImplicitMoveAssignmentOperators;
  
  /// \brief The number of implicitly-declared move assignment operators for 
  /// which declarations were built.
  static unsigned NumImplicitMoveAssignmentOperatorsDeclared;

  /// \brief The number of implicitly-declared destructors.
  static unsigned NumImplicitDestructors;
  
  /// \brief The number of implicitly-declared destructors for which 
  /// declarations were built.
  static unsigned NumImplicitDestructorsDeclared;
  
private:
  ASTContext(const ASTContext &) = delete;
  void operator=(const ASTContext &) = delete;
#endif

public:
  /// \brief Initialize built-in types.
  ///
  /// This routine may only be invoked once for a given ASTContext object.
  /// It is normally invoked by the ASTContext constructor. However, the
  /// constructor can be asked to delay initialization, which places the burden
  /// of calling this function on the user of that object.
  ///
  /// \param Target The target 
  //void InitBuiltinTypes(const TargetInfo &Target);
  void InitBuiltinTypes();
  
private:
  //void InitBuiltinType(CanQualType &R, BuiltinType::Kind K);
  void InitBuiltinType(Type *&R, BuiltinType::Kind K);

#if 0
private:
  /// \brief A set of deallocations that should be performed when the 
  /// ASTContext is destroyed.
  SmallVector<std::pair<void (*)(void*), void *>, 16> Deallocations;
#endif
                                       
  // FIXME: This currently contains the set of StoredDeclMaps used
  // by DeclContext objects.  This probably should not be in ASTContext,
  // but we include it here so that ASTContext can quickly deallocate them.
  llvm::PointerIntPair<StoredDeclsMap*,1> LastSDM;

#if 0
  /// \brief A counter used to uniquely identify "blocks".
  mutable unsigned int UniqueBlockByRefTypeID;
  
#endif
  friend class DeclContext;
  //friend class DeclarationNameTable;
  void ReleaseDeclContextMaps();
};
#if 0
  
/// \brief Utility function for constructing a nullary selector.
static inline Selector GetNullarySelector(StringRef name, ASTContext& Ctx) {
  IdentifierInfo* II = &Ctx.Idents.get(name);
  return Ctx.Selectors.getSelector(0, &II);
}

/// \brief Utility function for constructing an unary selector.
static inline Selector GetUnarySelector(StringRef name, ASTContext& Ctx) {
  IdentifierInfo* II = &Ctx.Idents.get(name);
  return Ctx.Selectors.getSelector(1, &II);
}

#endif
}  // end namespace gong

// operator new and delete aren't allowed inside namespaces.

/// @brief Placement new for using the ASTContext's allocator.
///
/// This placement form of operator new uses the ASTContext's allocator for
/// obtaining memory.
///
/// IMPORTANT: These are also declared in gong/AST/AttrIterator.h! Any changes
/// here need to also be made there.
///
/// We intentionally avoid using a nothrow specification here so that the calls
/// to this operator will not perform a null check on the result -- the
/// underlying allocator never returns null pointers.
///
/// Usage looks like this (assuming there's an ASTContext 'Context' in scope):
/// @code
/// // Default alignment (8)
/// IntegerLiteral *Ex = new (Context) IntegerLiteral(arguments);
/// // Specific alignment
/// IntegerLiteral *Ex2 = new (Context, 4) IntegerLiteral(arguments);
/// @endcode
/// Please note that you cannot use delete on the pointer; it must be
/// deallocated using an explicit destructor call followed by
/// @c Context.Deallocate(Ptr).
///
/// @param Bytes The number of bytes to allocate. Calculated by the compiler.
/// @param C The ASTContext that provides the allocator.
/// @param Alignment The alignment of the allocated memory (if the underlying
///                  allocator supports it).
/// @return The allocated memory. Could be NULL.
inline void *operator new(size_t Bytes, const gong::ASTContext &C,
                          size_t Alignment = 16) {
  return C.Allocate(Bytes, Alignment);
}
/// @brief Placement delete companion to the new above.
///
/// This operator is just a companion to the new above. There is no way of
/// invoking it directly; see the new operator for more details. This operator
/// is called implicitly by the compiler if a placement new expression using
/// the ASTContext throws in the object constructor.
inline void operator delete(void *Ptr, const gong::ASTContext &C, size_t) {
  C.Deallocate(Ptr);
}

/// This placement form of operator new[] uses the ASTContext's allocator for
/// obtaining memory.
///
/// We intentionally avoid using a nothrow specification here so that the calls
/// to this operator will not perform a null check on the result -- the
/// underlying allocator never returns null pointers.
///
/// Usage looks like this (assuming there's an ASTContext 'Context' in scope):
/// @code
/// // Default alignment (8)
/// char *data = new (Context) char[10];
/// // Specific alignment
/// char *data = new (Context, 4) char[10];
/// @endcode
/// Please note that you cannot use delete on the pointer; it must be
/// deallocated using an explicit destructor call followed by
/// @c Context.Deallocate(Ptr).
///
/// @param Bytes The number of bytes to allocate. Calculated by the compiler.
/// @param C The ASTContext that provides the allocator.
/// @param Alignment The alignment of the allocated memory (if the underlying
///                  allocator supports it).
/// @return The allocated memory. Could be NULL.
inline void *operator new[](size_t Bytes, const gong::ASTContext& C,
                            size_t Alignment = 8) {
  return C.Allocate(Bytes, Alignment);
}

/// @brief Placement delete[] companion to the new[] above.
///
/// This operator is just a companion to the new[] above. There is no way of
/// invoking it directly; see the new[] operator for more details. This operator
/// is called implicitly by the compiler if a placement new[] expression using
/// the ASTContext throws in the object constructor.
inline void operator delete[](void *Ptr, const gong::ASTContext &C, size_t) {
  C.Deallocate(Ptr);
}

#endif
