//===--- Type.h - C Language Family Type Representation ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the Type interface and subclasses.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_AST_TYPE_H
#define LLVM_GONG_AST_TYPE_H

#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Compiler.h"
#include "gong/Basic/LLVM.h"

#if 0
#include "gong/AST/NestedNameSpecifier.h"
#include "gong/AST/TemplateName.h"
#include "gong/Basic/Diagnostic.h"
#include "gong/Basic/ExceptionSpecificationType.h"
#include "gong/Basic/IdentifierTable.h"
#include "gong/Basic/Linkage.h"
#include "gong/Basic/PartialDiagnostic.h"
#include "gong/Basic/Specifiers.h"
#include "gong/Basic/Visibility.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/type_traits.h"
#endif

namespace gong {
  enum {
    TypeAlignmentInBits = 4,
    TypeAlignment = 1 << TypeAlignmentInBits
  };
  class Type;
}

namespace llvm {
  template <typename T>
  class PointerLikeTypeTraits;
  template<>
  class PointerLikeTypeTraits< ::gong::Type*> {
  public:
    static inline void *getAsVoidPointer(::gong::Type *P) { return P; }
    static inline ::gong::Type *getFromVoidPointer(void *P) {
      return static_cast< ::gong::Type*>(P);
    }
    enum { NumLowBitsAvailable = gong::TypeAlignmentInBits };
  };
}

namespace gong {
class StructTypeDecl;
#if 0
  class ASTContext;
  class TypedefNameDecl;
  class TemplateDecl;
  class NonTypeTemplateParmDecl;
  class TemplateTemplateParmDecl;
  class TagDecl;
  class RecordDecl;
  class CXXRecordDecl;
  class EnumDecl;
  class FieldDecl;
  class FunctionDecl;
  class Expr;
  class Stmt;
  class SourceLocation;
  class StmtIteratorBase;
  class TemplateArgument;
  class TemplateArgumentLoc;
  class TemplateArgumentListInfo;
  class ElaboratedType;
  class ExtQualsTypeCommonBase;
  struct PrintingPolicy;

  template <typename> class CanQual;
  typedef CanQual<Type> CanQualType;

#endif
  // Provide forward declarations for all of the *Type classes
#define TYPE(Class, Base) class Class##Type;
#include "gong/AST/TypeNodes.def"

/// \brief Base class that is common to both the \c ExtQuals and \c Type
/// classes, which allows \c QualType to access the common fields between the
/// two.
///
class ExtQualsTypeCommonBase {
  ExtQualsTypeCommonBase(const Type *baseType, const Type *canon)
    : BaseType(baseType), CanonicalType(canon) {}

  /// \brief A self-referential pointer (for \c Type).
  ///
  /// This pointer allows an efficient mapping from a QualType to its
  /// underlying type pointer.
  const Type *const BaseType;

  /// \brief The canonical type of this type.  A QualType.
  const Type *CanonicalType;

  friend class QualType;
  friend class Type;
};

/// Type - This is the base class of the type hierarchy.  A central concept
/// with types is that each type always has a canonical type.  A canonical type
/// is the type with any typedef names stripped out of it or the types it
/// references.  For example, consider:
///
///  typedef int  foo;
///  typedef foo* bar;
///    'int *'    'foo *'    'bar'
///
/// There will be a Type object created for 'int'.  Since int is canonical, its
/// canonicaltype pointer points to itself.  There is also a Type for 'foo' (a
/// TypedefType).  Its CanonicalType pointer points to the 'int' Type.  Next
/// there is a PointerType that represents 'int*', which, like 'int', is
/// canonical.  Finally, there is a PointerType type for 'foo*' whose canonical
/// type is 'int*', and there is a TypedefType for 'bar', whose canonical type
/// is also 'int*'.
///
/// Non-canonical types are useful for emitting diagnostics, without losing
/// information about typedefs being used.  Canonical types are useful for type
/// comparisons (they allow by-pointer equality tests) and useful for reasoning
/// about whether something has a particular form (e.g. is a function type),
/// because they implicitly, recursively, strip all typedefs out of a type.
///
/// Types, once created, are immutable.
///
class Type : public ExtQualsTypeCommonBase {
public:
  enum TypeClass {
#define TYPE(Class, Base) Class,
#define LAST_TYPE(Class) TypeLast = Class,
#define ABSTRACT_TYPE(Class, Base)
#include "gong/AST/TypeNodes.def"
    TagFirst = Record, TagLast = Enum
  };

private:
  Type(const Type &) LLVM_DELETED_FUNCTION;
  void operator=(const Type &) LLVM_DELETED_FUNCTION;

  /// Bitfields required by the Type class.
  class TypeBitfields {
    friend class Type;
    template <class T> friend class TypePropertyCache;

    /// TypeClass bitfield - Enum that specifies what subclass this belongs to.
    unsigned TC : 8;

    /// \brief Nonzero if the cache (i.e. the bitfields here starting
    /// with 'Cache') is valid.  If so, then this is a
    /// LangOptions::VisibilityMode+1.
    mutable unsigned CacheValidAndVisibility : 2;

    /// \brief True if the visibility was set explicitly in the source code.
    mutable unsigned CachedExplicitVisibility : 1;

    /// \brief Linkage of this type.
    mutable unsigned CachedLinkage : 2;

    /// \brief Whether this type involves and local or unnamed types.
    mutable unsigned CachedLocalOrUnnamed : 1;

    /// \brief FromAST - Whether this type comes from an AST file.
    mutable unsigned FromAST : 1;

    //bool isCacheValid() const {
    //  return (CacheValidAndVisibility != 0);
    //}
    //Visibility getVisibility() const {
    //  assert(isCacheValid() && "getting linkage from invalid cache");
    //  return static_cast<Visibility>(CacheValidAndVisibility-1);
    //}
    //bool isVisibilityExplicit() const {
    //  assert(isCacheValid() && "getting linkage from invalid cache");
    //  return CachedExplicitVisibility;
    //}
    //Linkage getLinkage() const {
    //  assert(isCacheValid() && "getting linkage from invalid cache");
    //  return static_cast<Linkage>(CachedLinkage);
    //}
    //bool hasLocalOrUnnamedType() const {
    //  assert(isCacheValid() && "getting linkage from invalid cache");
    //  return CachedLocalOrUnnamed;
    //}
  };
  enum { NumTypeBits = 19 };

protected:
  // These classes allow subclasses to somewhat cleanly pack bitfields
  // into Type.

  class ArrayTypeBitfields {
    friend class ArrayType;

    unsigned : NumTypeBits;

    /// IndexTypeQuals - CVR qualifiers from declarations like
    /// 'int X[static restrict 4]'. For function parameters only.
    unsigned IndexTypeQuals : 3;

    /// SizeModifier - storage class qualifiers from declarations like
    /// 'int X[static restrict 4]'. For function parameters only.
    /// Actually an ArrayType::ArraySizeModifier.
    unsigned SizeModifier : 3;
  };

  class BuiltinTypeBitfields {
    friend class BuiltinType;

    unsigned : NumTypeBits;

    /// The kind (BuiltinType::Kind) of builtin type this is.
    unsigned Kind : 8;
  };

  class FunctionTypeBitfields {
    friend class FunctionType;

    unsigned : NumTypeBits;

    /// Extra information which affects how the function is called, like
    /// regparm and the calling convention.
    unsigned ExtInfo : 9;

    /// TypeQuals - Used only by FunctionProtoType, put here to pack with the
    /// other bitfields.
    /// The qualifiers are part of FunctionProtoType because...
    ///
    /// C++ 8.3.5p4: The return type, the parameter type list and the
    /// cv-qualifier-seq, [...], are part of the function type.
    unsigned TypeQuals : 3;

    /// \brief The ref-qualifier associated with a \c FunctionProtoType.
    ///
    /// This is a value of type \c RefQualifierKind.
    unsigned RefQualifier : 2;
  };

  union {
    TypeBitfields TypeBits;
    ArrayTypeBitfields ArrayTypeBits;
    BuiltinTypeBitfields BuiltinTypeBits;
    FunctionTypeBitfields FunctionTypeBits;
  };

private:
  /// \brief Set whether this type comes from an AST file.
  void setFromAST(bool V = true) const {
    TypeBits.FromAST = V;
  }

  template <class T> friend class TypePropertyCache;

protected:
  // silence VC++ warning C4355: 'this' : used in base member initializer list
  Type *this_() { return this; }
  Type(TypeClass tc, const Type *canon)
    : ExtQualsTypeCommonBase(this, canon) {
    TypeBits.TC = tc;
    //TypeBits.VariablyModified = VariablyModified;
    //TypeBits.CacheValidAndVisibility = 0;
    //TypeBits.CachedExplicitVisibility = false;
    //TypeBits.CachedLocalOrUnnamed = false;
    //TypeBits.CachedLinkage = NoLinkage;
    TypeBits.FromAST = false;
  }
  friend class ASTContext;

public:
  TypeClass getTypeClass() const { return static_cast<TypeClass>(TypeBits.TC); }

  /// \brief Whether this type comes from an AST file.
  bool isFromAST() const { return TypeBits.FromAST; }

#if 0
  /// Determines if this type would be canonical if it had no further
  /// qualification.
  bool isCanonicalUnqualified() const {
    return CanonicalType == QualType(this, 0);
  }

  /// Pull a single level of sugar off of this locally-unqualified type.
  /// Users should generally prefer SplitQualType::getSingleStepDesugaredType()
  /// or QualType::getSingleStepDesugaredType(const ASTContext&).
  QualType getLocallyUnqualifiedSingleStepDesugaredType() const;

  /// Types are partitioned into 3 broad categories (C99 6.2.5p1):
  /// object types, function types, and incomplete types.

  /// isIncompleteType - Return true if this is an incomplete type.
  /// A type that can describe objects, but which lacks information needed to
  /// determine its size (e.g. void, or a fwd declared struct). Clients of this
  /// routine will need to determine if the size is actually required.
  ///
  /// \brief Def If non-NULL, and the type refers to some kind of declaration
  /// that can be completed (such as a C struct, C++ class, or Objective-C
  /// class), will be set to the declaration.
  bool isIncompleteType(NamedDecl **Def = 0) const;

  /// isIncompleteOrObjectType - Return true if this is an incomplete or object
  /// type, in other words, not a function type.
  bool isIncompleteOrObjectType() const {
    return !isFunctionType();
  }

  /// \brief Determine whether this type is an object type.
  bool isObjectType() const {
    // C++ [basic.types]p8:
    //   An object type is a (possibly cv-qualified) type that is not a
    //   function type, not a reference type, and not a void type.
    return !isReferenceType() && !isFunctionType() && !isVoidType();
  }

  /// isLiteralType - Return true if this is a literal type
  /// (C++0x [basic.types]p10)
  bool isLiteralType() const;

  /// \brief Test if this type is a standard-layout type.
  /// (C++0x [basic.type]p9)
  bool isStandardLayoutType() const;

  /// Helper methods to distinguish type categories. All type predicates
  /// operate on the canonical type, ignoring typedefs and qualifiers.

  /// isBuiltinType - returns true if the type is a builtin type.
  bool isBuiltinType() const;

  /// isSpecificBuiltinType - Test for a particular builtin type.
  bool isSpecificBuiltinType(unsigned K) const;

  /// isPlaceholderType - Test for a type which does not represent an
  /// actual type-system type but is instead used as a placeholder for
  /// various convenient purposes within gong.  All such types are
  /// BuiltinTypes.
  bool isPlaceholderType() const;
  const BuiltinType *getAsPlaceholderType() const;

  /// isSpecificPlaceholderType - Test for a specific placeholder type.
  bool isSpecificPlaceholderType(unsigned K) const;

  /// isNonOverloadPlaceholderType - Test for a placeholder type
  /// other than Overload;  see BuiltinType::isNonOverloadPlaceholderType.
  bool isNonOverloadPlaceholderType() const;

  /// isIntegerType() does *not* include complex integers (a GCC extension).
  /// isComplexIntegerType() can be used to test for complex integers.
  bool isIntegerType() const;     // C99 6.2.5p17 (int, char, bool, enum)
  bool isEnumeralType() const;
  bool isBooleanType() const;
  bool isCharType() const;
  bool isWideCharType() const;
  bool isChar16Type() const;
  bool isChar32Type() const;
  bool isAnyCharacterType() const;
  bool isIntegralType(ASTContext &Ctx) const;

  /// \brief Determine whether this type is an integral or enumeration type.
  bool isIntegralOrEnumerationType() const;
  /// \brief Determine whether this type is an integral or unscoped enumeration
  /// type.
  bool isIntegralOrUnscopedEnumerationType() const;

  /// Floating point categories.
  bool isRealFloatingType() const; // C99 6.2.5p10 (float, double, long double)
  /// isComplexType() does *not* include complex integers (a GCC extension).
  /// isComplexIntegerType() can be used to test for complex integers.
  bool isComplexType() const;      // C99 6.2.5p11 (complex)
  bool isAnyComplexType() const;   // C99 6.2.5p11 (complex) + Complex Int.
  bool isFloatingType() const;     // C99 6.2.5p11 (real floating + complex)
  bool isHalfType() const;         // OpenCL 6.1.1.1, NEON (IEEE 754-2008 half)
  bool isRealType() const;         // C99 6.2.5p17 (real floating + integer)
  bool isArithmeticType() const;   // C99 6.2.5p18 (integer + floating)
  bool isVoidType() const;         // C99 6.2.5p19
  bool isDerivedType() const;      // C99 6.2.5p20
  bool isScalarType() const;       // C99 6.2.5p21 (arithmetic + pointers)
  bool isAggregateType() const;
  bool isFundamentalType() const;
  bool isCompoundType() const;

  // Type Predicates: Check to see if this type is structurally the specified
  // type, ignoring typedefs and qualifiers.
  bool isFunctionType() const;
  bool isFunctionNoProtoType() const { return getAs<FunctionNoProtoType>(); }
  bool isFunctionProtoType() const { return getAs<FunctionProtoType>(); }
  bool isPointerType() const;
  bool isAnyPointerType() const;   // Any C pointer
  bool isBlockPointerType() const;
  bool isVoidPointerType() const;
  bool isReferenceType() const;
  bool isLValueReferenceType() const;
  bool isRValueReferenceType() const;
  bool isFunctionPointerType() const;
  bool isMemberPointerType() const;
  bool isMemberFunctionPointerType() const;
  bool isMemberDataPointerType() const;
  bool isArrayType() const;
  bool isConstantArrayType() const;
  bool isIncompleteArrayType() const;
  bool isVariableArrayType() const;
  bool isRecordType() const;
  bool isClassType() const;
  bool isStructureType() const;
  bool isInterfaceType() const;
  bool isStructureOrClassType() const;
  bool isUnionType() const;
  bool isComplexIntegerType() const;            // GCC _Complex integer type.
  bool isVectorType() const;                    // GCC vector type.
  bool isExtVectorType() const;                 // Extended vector type.
  bool isCARCBridgableType() const;
  bool isNullPtrType() const;                   // C++0x nullptr_t
  bool isAtomicType() const;                    // C11 _Atomic()

  enum ScalarTypeKind {
    STK_CPointer,
    STK_BlockPointer,
    STK_MemberPointer,
    STK_Bool,
    STK_Integral,
    STK_Floating,
    STK_IntegralComplex,
    STK_FloatingComplex
  };
  /// getScalarTypeKind - Given that this is a scalar type, classify it.
  ScalarTypeKind getScalarTypeKind() const;

  /// isDependentType - Whether this type is a dependent type, meaning
  /// that its definition somehow depends on a template parameter
  /// (C++ [temp.dep.type]).
  bool isDependentType() const { return TypeBits.Dependent; }

  /// \brief Whether this type involves a variable-length array type
  /// with a definite size.
  bool hasSizedVLAType() const;

  /// \brief Whether this type is or contains a local or unnamed type.
  bool hasUnnamedOrLocalType() const;

  bool isOverloadableType() const;

  /// \brief Determine wither this type is a C++ elaborated-type-specifier.
  bool isElaboratedTypeSpecifier() const;

  bool canDecayToPointerType() const;

  /// hasPointerRepresentation - Whether this type is represented
  /// natively as a pointer; this includes pointers, references, block
  /// pointers, and Objective-C interface, qualified id, and qualified
  /// interface types, as well as nullptr_t.
  bool hasPointerRepresentation() const;

  /// \brief Determine whether this type has an integer representation
  /// of some sort, e.g., it is an integer type or a vector.
  bool hasIntegerRepresentation() const;

  /// \brief Determine whether this type has an signed integer representation
  /// of some sort, e.g., it is an signed integer type or a vector.
  bool hasSignedIntegerRepresentation() const;

  /// \brief Determine whether this type has an unsigned integer representation
  /// of some sort, e.g., it is an unsigned integer type or a vector.
  bool hasUnsignedIntegerRepresentation() const;

  /// \brief Determine whether this type has a floating-point representation
  /// of some sort, e.g., it is a floating-point type or a vector thereof.
  bool hasFloatingRepresentation() const;

  // Type Checking Functions: Check to see if this type is structurally the
  // specified type, ignoring typedefs and qualifiers, and return a pointer to
  // the best type we can.
  const RecordType *getAsStructureType() const;
  /// NOTE: getAs*ArrayType are methods on ASTContext.
  const RecordType *getAsUnionType() const;
  const ComplexType *getAsComplexIntegerType() const; // GCC complex int type.

  /// \brief Retrieves the CXXRecordDecl that this type refers to, either
  /// because the type is a RecordType or because it is the injected-class-name
  /// type of a class template or class template partial specialization.
  CXXRecordDecl *getAsCXXRecordDecl() const;

  /// If this is a pointer or reference to a RecordType, return the
  /// CXXRecordDecl that that type refers to.
  ///
  /// If this is not a pointer or reference, or the type being pointed to does
  /// not refer to a CXXRecordDecl, returns NULL.
  const CXXRecordDecl *getPointeeCXXRecordDecl() const;

  /// \brief Get the AutoType whose type will be deduced for a variable with
  /// an initializer of this type. This looks through declarators like pointer
  /// types, but not through decltype or typedefs.
  AutoType *getContainedAutoType() const;

  /// Member-template getAs<specific type>'.  Look through sugar for
  /// an instance of \<specific type>.   This scheme will eventually
  /// replace the specific getAsXXXX methods above.
  ///
  /// There are some specializations of this member template listed
  /// immediately following this class.
  template <typename T> const T *getAs() const;

  /// A variant of getAs<> for array types which silently discards
  /// qualifiers from the outermost type.
  const ArrayType *getAsArrayTypeUnsafe() const;

  /// Member-template castAs<specific type>.  Look through sugar for
  /// the underlying instance of \<specific type>.
  ///
  /// This method has the same relationship to getAs<T> as cast<T> has
  /// to dyn_cast<T>; which is to say, the underlying type *must*
  /// have the intended type, and this method will never return null.
  template <typename T> const T *castAs() const;

  /// A variant of castAs<> for array type which silently discards
  /// qualifiers from the outermost type.
  const ArrayType *castAsArrayTypeUnsafe() const;

  /// getBaseElementTypeUnsafe - Get the base element type of this
  /// type, potentially discarding type qualifiers.  This method
  /// should never be used when type qualifiers are meaningful.
  const Type *getBaseElementTypeUnsafe() const;

  /// getArrayElementTypeNoTypeQual - If this is an array type, return the
  /// element type of the array, potentially with type qualifiers missing.
  /// This method should never be used when type qualifiers are meaningful.
  const Type *getArrayElementTypeNoTypeQual() const;

  /// getPointeeType - If this is a pointer, or block
  /// pointer, this returns the respective pointee.
  QualType getPointeeType() const;

  /// getUnqualifiedDesugaredType() - Return the specified type with
  /// any "sugar" removed from the type, removing any typedefs,
  /// typeofs, etc., as well as any qualifiers.
  const Type *getUnqualifiedDesugaredType() const;

  /// More type predicates useful for type checking/promotion
  bool isPromotableIntegerType() const; // C99 6.3.1.1p2

  /// isSignedIntegerType - Return true if this is an integer type that is
  /// signed, according to C99 6.2.5p4 [char, signed char, short, int, long..],
  /// or an enum decl which has a signed representation.
  bool isSignedIntegerType() const;

  /// isUnsignedIntegerType - Return true if this is an integer type that is
  /// unsigned, according to C99 6.2.5p6 [which returns true for _Bool],
  /// or an enum decl which has an unsigned representation.
  bool isUnsignedIntegerType() const;

  /// Determines whether this is an integer type that is signed or an
  /// enumeration types whose underlying type is a signed integer type.
  bool isSignedIntegerOrEnumerationType() const;

  /// Determines whether this is an integer type that is unsigned or an
  /// enumeration types whose underlying type is a unsigned integer type.
  bool isUnsignedIntegerOrEnumerationType() const;

  /// isConstantSizeType - Return true if this is not a variable sized type,
  /// according to the rules of C99 6.7.5p3.  It is not legal to call this on
  /// incomplete types.
  bool isConstantSizeType() const;

  /// isSpecifierType - Returns true if this type can be represented by some
  /// set of type specifiers.
  bool isSpecifierType() const;

  /// \brief Determine the linkage of this type.
  Linkage getLinkage() const;

  /// \brief Determine the visibility of this type.
  Visibility getVisibility() const;

  /// \brief Return true if the visibility was explicitly set is the code.
  bool isVisibilityExplicit() const;

  /// \brief Determine the linkage and visibility of this type.
  std::pair<Linkage,Visibility> getLinkageAndVisibility() const;

  /// \brief Note that the linkage is no longer known.
  void ClearLinkageCache();

  const char *getTypeClassName() const;

  QualType getCanonicalTypeInternal() const {
    return CanonicalType;
  }
  CanQualType getCanonicalTypeUnqualified() const; // in CanonicalType.h
  LLVM_ATTRIBUTE_USED void dump() const;

  friend class ASTReader;
  friend class ASTWriter;
#endif
};
#if 0

/// \brief This will check for a TypedefType by removing any existing sugar
/// until it reaches a TypedefType or a non-sugared type.
template <> const TypedefType *Type::getAs() const;

// We can do canonical leaf types faster, because we don't have to
// worry about preserving child type decoration.
#define TYPE(Class, Base)
#define LEAF_TYPE(Class) \
template <> inline const Class##Type *Type::getAs() const { \
  return dyn_cast<Class##Type>(CanonicalType); \
} \
template <> inline const Class##Type *Type::castAs() const { \
  return cast<Class##Type>(CanonicalType); \
}
#include "gong/AST/TypeNodes.def"

#endif

/// BuiltinType - This class is used for builtin types like 'int'.  Builtin
/// types are always canonical and have a literal name field.
class BuiltinType : public Type {
public:
  enum Kind {
#define BUILTIN_TYPE(Id, SingletonId) Id,
#define LAST_BUILTIN_TYPE(Id) LastKind = Id
#include "gong/AST/BuiltinTypes.def"
  };

public:
  BuiltinType(Kind K)
    : Type(Builtin, /*canon=*/0 /* FIXME: why not |this|?*/) {
    BuiltinTypeBits.Kind = K;
  }

  Kind getKind() const { return static_cast<Kind>(BuiltinTypeBits.Kind); }
  StringRef getName() const;
  const char *getNameAsCString() const {
    // The StringRef is null-terminated.
    StringRef str = getName();
    assert(!str.empty() && str.data()[str.size()] == '\0');
    return str.data();
  }

  bool isSugared() const { return false; }
  //QualType desugar() const { return QualType(this, 0); }

  bool isInteger() const {
    return getKind() >= UInt8 && getKind() <= Int64;
  }

  bool isSignedInteger() const {
    return getKind() >= Int8 && getKind() <= Int64;
  }

  bool isUnsignedInteger() const {
    return getKind() >= UInt8 && getKind() <= UInt64;
  }

  bool isFloatingPoint() const {
    return getKind() >= Float32 && getKind() <= Float64;
  }

  static bool classof(const Type *T) { return T->getTypeClass() == Builtin; }
};

#if 0
/// ComplexType - C99 6.2.5p11 - Complex values.  This supports the C99 complex
/// types (_Complex float etc) as well as the GCC integer complex extensions.
///
class ComplexType : public Type, public llvm::FoldingSetNode {
  QualType ElementType;
  ComplexType(QualType Element, QualType CanonicalPtr) :
    Type(Complex, CanonicalPtr, Element->isDependentType(),
         Element->isInstantiationDependentType(),
         Element->isVariablyModifiedType(),
         Element->containsUnexpandedParameterPack()),
    ElementType(Element) {
  }
  friend class ASTContext;  // ASTContext creates these.

public:
  QualType getElementType() const { return ElementType; }

  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getElementType());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, QualType Element) {
    ID.AddPointer(Element.getAsOpaquePtr());
  }

  static bool classof(const Type *T) { return T->getTypeClass() == Complex; }
};

/// ParenType - Sugar for parentheses used when specifying types.
///
class ParenType : public Type, public llvm::FoldingSetNode {
  QualType Inner;

  ParenType(QualType InnerType, QualType CanonType) :
    Type(Paren, CanonType, InnerType->isDependentType(),
         InnerType->isInstantiationDependentType(),
         InnerType->isVariablyModifiedType(),
         InnerType->containsUnexpandedParameterPack()),
    Inner(InnerType) {
  }
  friend class ASTContext;  // ASTContext creates these.

public:

  QualType getInnerType() const { return Inner; }

  bool isSugared() const { return true; }
  QualType desugar() const { return getInnerType(); }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getInnerType());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, QualType Inner) {
    Inner.Profile(ID);
  }

  static bool classof(const Type *T) { return T->getTypeClass() == Paren; }
};
#endif

/// PointerType - C99 6.7.5.1 - Pointer Declarators.
///
class PointerType : public Type, public llvm::FoldingSetNode {
  Type *PointeeType;

  PointerType(Type *Pointee, Type *CanonicalPtr)
    : Type(Pointer, CanonicalPtr), PointeeType(Pointee) {}
  friend class ASTContext;  // ASTContext creates these.

public:

  Type *getPointeeType() const { return PointeeType; }

  bool isSugared() const { return false; }
  //QualType desugar() const { return QualType(this, 0); }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getPointeeType());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, Type *Pointee) {
    ID.AddPointer(Pointee);
  }

  static bool classof(const Type *T) { return T->getTypeClass() == Pointer; }
};

#if 0
/// BlockPointerType - pointer to a block type.
/// This type is to represent types syntactically represented as
/// "void (^)(int)", etc. Pointee is required to always be a function type.
///
class BlockPointerType : public Type, public llvm::FoldingSetNode {
  QualType PointeeType;  // Block is some kind of pointer type
  BlockPointerType(QualType Pointee, QualType CanonicalCls) :
    Type(BlockPointer, CanonicalCls, Pointee->isDependentType(),
         Pointee->isInstantiationDependentType(),
         Pointee->isVariablyModifiedType(),
         Pointee->containsUnexpandedParameterPack()),
    PointeeType(Pointee) {
  }
  friend class ASTContext;  // ASTContext creates these.

public:

  // Get the pointee type. Pointee is required to always be a function type.
  QualType getPointeeType() const { return PointeeType; }

  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  void Profile(llvm::FoldingSetNodeID &ID) {
      Profile(ID, getPointeeType());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, QualType Pointee) {
      ID.AddPointer(Pointee.getAsOpaquePtr());
  }

  static bool classof(const Type *T) {
    return T->getTypeClass() == BlockPointer;
  }
};

/// ReferenceType - Base for LValueReferenceType and RValueReferenceType
///
class ReferenceType : public Type, public llvm::FoldingSetNode {
  QualType PointeeType;

protected:
  ReferenceType(TypeClass tc, QualType Referencee, QualType CanonicalRef,
                bool SpelledAsLValue) :
    Type(tc, CanonicalRef, Referencee->isDependentType(),
         Referencee->isInstantiationDependentType(),
         Referencee->isVariablyModifiedType(),
         Referencee->containsUnexpandedParameterPack()),
    PointeeType(Referencee)
  {
    ReferenceTypeBits.SpelledAsLValue = SpelledAsLValue;
    ReferenceTypeBits.InnerRef = Referencee->isReferenceType();
  }

public:
  bool isSpelledAsLValue() const { return ReferenceTypeBits.SpelledAsLValue; }
  bool isInnerRef() const { return ReferenceTypeBits.InnerRef; }

  QualType getPointeeTypeAsWritten() const { return PointeeType; }
  QualType getPointeeType() const {
    // FIXME: this might strip inner qualifiers; okay?
    const ReferenceType *T = this;
    while (T->isInnerRef())
      T = T->PointeeType->castAs<ReferenceType>();
    return T->PointeeType;
  }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, PointeeType, isSpelledAsLValue());
  }
  static void Profile(llvm::FoldingSetNodeID &ID,
                      QualType Referencee,
                      bool SpelledAsLValue) {
    ID.AddPointer(Referencee.getAsOpaquePtr());
    ID.AddBoolean(SpelledAsLValue);
  }

  static bool classof(const Type *T) {
    return T->getTypeClass() == LValueReference ||
           T->getTypeClass() == RValueReference;
  }
};

/// LValueReferenceType - C++ [dcl.ref] - Lvalue reference
///
class LValueReferenceType : public ReferenceType {
  LValueReferenceType(QualType Referencee, QualType CanonicalRef,
                      bool SpelledAsLValue) :
    ReferenceType(LValueReference, Referencee, CanonicalRef, SpelledAsLValue)
  {}
  friend class ASTContext; // ASTContext creates these
public:
  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  static bool classof(const Type *T) {
    return T->getTypeClass() == LValueReference;
  }
};

/// RValueReferenceType - C++0x [dcl.ref] - Rvalue reference
///
class RValueReferenceType : public ReferenceType {
  RValueReferenceType(QualType Referencee, QualType CanonicalRef) :
    ReferenceType(RValueReference, Referencee, CanonicalRef, false) {
  }
  friend class ASTContext; // ASTContext creates these
public:
  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  static bool classof(const Type *T) {
    return T->getTypeClass() == RValueReference;
  }
};

/// MemberPointerType - C++ 8.3.3 - Pointers to members
///
class MemberPointerType : public Type, public llvm::FoldingSetNode {
  QualType PointeeType;
  /// The class of which the pointee is a member. Must ultimately be a
  /// RecordType, but could be a typedef or a template parameter too.
  const Type *Class;

  MemberPointerType(QualType Pointee, const Type *Cls, QualType CanonicalPtr) :
    Type(MemberPointer, CanonicalPtr,
         Cls->isDependentType() || Pointee->isDependentType(),
         (Cls->isInstantiationDependentType() ||
          Pointee->isInstantiationDependentType()),
         Pointee->isVariablyModifiedType(),
         (Cls->containsUnexpandedParameterPack() ||
          Pointee->containsUnexpandedParameterPack())),
    PointeeType(Pointee), Class(Cls) {
  }
  friend class ASTContext; // ASTContext creates these.

public:
  QualType getPointeeType() const { return PointeeType; }

  /// Returns true if the member type (i.e. the pointee type) is a
  /// function type rather than a data-member type.
  bool isMemberFunctionPointer() const {
    return PointeeType->isFunctionProtoType();
  }

  /// Returns true if the member type (i.e. the pointee type) is a
  /// data type rather than a function type.
  bool isMemberDataPointer() const {
    return !PointeeType->isFunctionProtoType();
  }

  const Type *getClass() const { return Class; }

  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getPointeeType(), getClass());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, QualType Pointee,
                      const Type *Class) {
    ID.AddPointer(Pointee.getAsOpaquePtr());
    ID.AddPointer(Class);
  }

  static bool classof(const Type *T) {
    return T->getTypeClass() == MemberPointer;
  }
};

/// ArrayType - C99 6.7.5.2 - Array Declarators.
///
class ArrayType : public Type, public llvm::FoldingSetNode {
public:
  /// ArraySizeModifier - Capture whether this is a normal array (e.g. int X[4])
  /// an array with a static size (e.g. int X[static 4]), or an array
  /// with a star size (e.g. int X[*]).
  /// 'static' is only allowed on function parameters.
  enum ArraySizeModifier {
    Normal, Static, Star
  };
private:
  /// ElementType - The element type of the array.
  QualType ElementType;

protected:
  // C++ [temp.dep.type]p1:
  //   A type is dependent if it is...
  //     - an array type constructed from any dependent type or whose
  //       size is specified by a constant expression that is
  //       value-dependent,
  ArrayType(TypeClass tc, QualType et, QualType can,
            ArraySizeModifier sm, unsigned tq,
            bool ContainsUnexpandedParameterPack)
    : Type(tc, can, et->isDependentType(),
           et->isInstantiationDependentType(),
           (tc == VariableArray || et->isVariablyModifiedType()),
           ContainsUnexpandedParameterPack),
      ElementType(et) {
    ArrayTypeBits.IndexTypeQuals = tq;
    ArrayTypeBits.SizeModifier = sm;
  }

  friend class ASTContext;  // ASTContext creates these.

public:
  QualType getElementType() const { return ElementType; }
  ArraySizeModifier getSizeModifier() const {
    return ArraySizeModifier(ArrayTypeBits.SizeModifier);
  }
  Qualifiers getIndexTypeQualifiers() const {
    return Qualifiers::fromCVRMask(getIndexTypeCVRQualifiers());
  }
  unsigned getIndexTypeCVRQualifiers() const {
    return ArrayTypeBits.IndexTypeQuals;
  }

  static bool classof(const Type *T) {
    return T->getTypeClass() == ConstantArray ||
           T->getTypeClass() == VariableArray ||
           T->getTypeClass() == IncompleteArray
  }
};

/// ConstantArrayType - This class represents the canonical version of
/// C arrays with a specified constant size.  For example, the canonical
/// type for 'int A[4 + 4*100]' is a ConstantArrayType where the element
/// type is 'int' and the size is 404.
class ConstantArrayType : public ArrayType {
  llvm::APInt Size; // Allows us to unique the type.

  ConstantArrayType(QualType et, QualType can, const llvm::APInt &size,
                    ArraySizeModifier sm, unsigned tq)
    : ArrayType(ConstantArray, et, can, sm, tq,
                et->containsUnexpandedParameterPack()),
      Size(size) {}
protected:
  ConstantArrayType(TypeClass tc, QualType et, QualType can,
                    const llvm::APInt &size, ArraySizeModifier sm, unsigned tq)
    : ArrayType(tc, et, can, sm, tq, et->containsUnexpandedParameterPack()),
      Size(size) {}
  friend class ASTContext;  // ASTContext creates these.
public:
  const llvm::APInt &getSize() const { return Size; }
  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }


  /// \brief Determine the number of bits required to address a member of
  // an array with the given element type and number of elements.
  static unsigned getNumAddressingBits(ASTContext &Context,
                                       QualType ElementType,
                                       const llvm::APInt &NumElements);

  /// \brief Determine the maximum number of active bits that an array's size
  /// can require, which limits the maximum size of the array.
  static unsigned getMaxSizeBits(ASTContext &Context);

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getElementType(), getSize(),
            getSizeModifier(), getIndexTypeCVRQualifiers());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, QualType ET,
                      const llvm::APInt &ArraySize, ArraySizeModifier SizeMod,
                      unsigned TypeQuals) {
    ID.AddPointer(ET.getAsOpaquePtr());
    ID.AddInteger(ArraySize.getZExtValue());
    ID.AddInteger(SizeMod);
    ID.AddInteger(TypeQuals);
  }
  static bool classof(const Type *T) {
    return T->getTypeClass() == ConstantArray;
  }
};

/// IncompleteArrayType - This class represents C arrays with an unspecified
/// size.  For example 'int A[]' has an IncompleteArrayType where the element
/// type is 'int' and the size is unspecified.
class IncompleteArrayType : public ArrayType {

  IncompleteArrayType(QualType et, QualType can,
                      ArraySizeModifier sm, unsigned tq)
    : ArrayType(IncompleteArray, et, can, sm, tq,
                et->containsUnexpandedParameterPack()) {}
  friend class ASTContext;  // ASTContext creates these.
public:
  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  static bool classof(const Type *T) {
    return T->getTypeClass() == IncompleteArray;
  }

  friend class StmtIteratorBase;

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getElementType(), getSizeModifier(),
            getIndexTypeCVRQualifiers());
  }

  static void Profile(llvm::FoldingSetNodeID &ID, QualType ET,
                      ArraySizeModifier SizeMod, unsigned TypeQuals) {
    ID.AddPointer(ET.getAsOpaquePtr());
    ID.AddInteger(SizeMod);
    ID.AddInteger(TypeQuals);
  }
};

/// VariableArrayType - This class represents C arrays with a specified size
/// which is not an integer-constant-expression.  For example, 'int s[x+foo()]'.
/// Since the size expression is an arbitrary expression, we store it as such.
///
/// Note: VariableArrayType's aren't uniqued (since the expressions aren't) and
/// should not be: two lexically equivalent variable array types could mean
/// different things, for example, these variables do not have the same type
/// dynamically:
///
/// void foo(int x) {
///   int Y[x];
///   ++x;
///   int Z[x];
/// }
///
class VariableArrayType : public ArrayType {
  /// SizeExpr - An assignment expression. VLA's are only permitted within
  /// a function block.
  Stmt *SizeExpr;
  /// Brackets - The left and right array brackets.
  SourceRange Brackets;

  VariableArrayType(QualType et, QualType can, Expr *e,
                    ArraySizeModifier sm, unsigned tq,
                    SourceRange brackets)
    : ArrayType(VariableArray, et, can, sm, tq,
                et->containsUnexpandedParameterPack()),
      SizeExpr((Stmt*) e), Brackets(brackets) {}
  friend class ASTContext;  // ASTContext creates these.

public:
  Expr *getSizeExpr() const {
    // We use C-style casts instead of cast<> here because we do not wish
    // to have a dependency of Type.h on Stmt.h/Expr.h.
    return (Expr*) SizeExpr;
  }
  SourceRange getBracketsRange() const { return Brackets; }
  SourceLocation getLBracketLoc() const { return Brackets.getBegin(); }
  SourceLocation getRBracketLoc() const { return Brackets.getEnd(); }

  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  static bool classof(const Type *T) {
    return T->getTypeClass() == VariableArray;
  }

  friend class StmtIteratorBase;

  void Profile(llvm::FoldingSetNodeID &ID) {
    llvm_unreachable("Cannot unique VariableArrayTypes.");
  }
};

/// VectorType - GCC generic vector type. This type is created using
/// __attribute__((vector_size(n)), where "n" specifies the vector size in
/// bytes; or from an Altivec __vector or vector declaration.
/// Since the constructor takes the number of vector elements, the
/// client is responsible for converting the size into the number of elements.
class VectorType : public Type, public llvm::FoldingSetNode {
public:
  enum VectorKind {
    GenericVector,  // not a target-specific vector type
    AltiVecVector,  // is AltiVec vector
    AltiVecPixel,   // is AltiVec 'vector Pixel'
    AltiVecBool,    // is AltiVec 'vector bool ...'
    NeonVector,     // is ARM Neon vector
    NeonPolyVector  // is ARM Neon polynomial vector
  };
protected:
  /// ElementType - The element type of the vector.
  QualType ElementType;

  VectorType(QualType vecType, unsigned nElements, QualType canonType,
             VectorKind vecKind);

  VectorType(TypeClass tc, QualType vecType, unsigned nElements,
             QualType canonType, VectorKind vecKind);

  friend class ASTContext;  // ASTContext creates these.

public:

  QualType getElementType() const { return ElementType; }
  unsigned getNumElements() const { return VectorTypeBits.NumElements; }

  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  VectorKind getVectorKind() const {
    return VectorKind(VectorTypeBits.VecKind);
  }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getElementType(), getNumElements(),
            getTypeClass(), getVectorKind());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, QualType ElementType,
                      unsigned NumElements, TypeClass TypeClass,
                      VectorKind VecKind) {
    ID.AddPointer(ElementType.getAsOpaquePtr());
    ID.AddInteger(NumElements);
    ID.AddInteger(TypeClass);
    ID.AddInteger(VecKind);
  }

  static bool classof(const Type *T) {
    return T->getTypeClass() == Vector || T->getTypeClass() == ExtVector;
  }
};

/// ExtVectorType - Extended vector type. This type is created using
/// __attribute__((ext_vector_type(n)), where "n" is the number of elements.
/// Unlike vector_size, ext_vector_type is only allowed on typedef's. This
/// class enables syntactic extensions, like Vector Components for accessing
/// points, colors, and textures (modeled after OpenGL Shading Language).
class ExtVectorType : public VectorType {
  ExtVectorType(QualType vecType, unsigned nElements, QualType canonType) :
    VectorType(ExtVector, vecType, nElements, canonType, GenericVector) {}
  friend class ASTContext;  // ASTContext creates these.
public:
  static int getPointAccessorIdx(char c) {
    switch (c) {
    default: return -1;
    case 'x': return 0;
    case 'y': return 1;
    case 'z': return 2;
    case 'w': return 3;
    }
  }
  static int getNumericAccessorIdx(char c) {
    switch (c) {
      default: return -1;
      case '0': return 0;
      case '1': return 1;
      case '2': return 2;
      case '3': return 3;
      case '4': return 4;
      case '5': return 5;
      case '6': return 6;
      case '7': return 7;
      case '8': return 8;
      case '9': return 9;
      case 'A':
      case 'a': return 10;
      case 'B':
      case 'b': return 11;
      case 'C':
      case 'c': return 12;
      case 'D':
      case 'd': return 13;
      case 'E':
      case 'e': return 14;
      case 'F':
      case 'f': return 15;
    }
  }

  static int getAccessorIdx(char c) {
    if (int idx = getPointAccessorIdx(c)+1) return idx-1;
    return getNumericAccessorIdx(c);
  }

  bool isAccessorWithinNumElements(char c) const {
    if (int idx = getAccessorIdx(c)+1)
      return unsigned(idx-1) < getNumElements();
    return false;
  }
  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  static bool classof(const Type *T) {
    return T->getTypeClass() == ExtVector;
  }
};

/// FunctionType - C99 6.7.5.3 - Function Declarators.  This is the common base
/// class of FunctionNoProtoType and FunctionProtoType.
///
class FunctionType : public Type {
  // The type returned by the function.
  QualType ResultType;

 public:
  /// ExtInfo - A class which abstracts out some details necessary for
  /// making a call.
  ///
  /// It is not actually used directly for storing this information in
  /// a FunctionType, although FunctionType does currently use the
  /// same bit-pattern.
  ///
  // If you add a field (say Foo), other than the obvious places (both,
  // constructors, compile failures), what you need to update is
  // * Operator==
  // * getFoo
  // * withFoo
  // * functionType. Add Foo, getFoo.
  // * ASTContext::getFooType
  // * ASTContext::mergeFunctionTypes
  // * FunctionNoProtoType::Profile
  // * FunctionProtoType::Profile
  // * TypePrinter::PrintFunctionProto
  // * AST read and write
  // * Codegen
  class ExtInfo {
    // Feel free to rearrange or add bits, but if you go over 9,
    // you'll need to adjust both the Bits field below and
    // Type::FunctionTypeBitfields.

    //   |  CC  |noreturn|produces|regparm|
    //   |0 .. 3|   4    |    5   | 6 .. 8|
    //
    // regparm is either 0 (no regparm attribute) or the regparm value+1.
    enum { CallConvMask = 0xF };
    enum { NoReturnMask = 0x10 };
    enum { ProducesResultMask = 0x20 };
    enum { RegParmMask = ~(CallConvMask | NoReturnMask | ProducesResultMask),
           RegParmOffset = 6 }; // Assumed to be the last field

    uint16_t Bits;

    ExtInfo(unsigned Bits) : Bits(static_cast<uint16_t>(Bits)) {}

    friend class FunctionType;

   public:
    // Constructor with no defaults. Use this when you know that you
    // have all the elements (when reading an AST file for example).
    ExtInfo(bool noReturn, bool hasRegParm, unsigned regParm, CallingConv cc,
            bool producesResult) {
      assert((!hasRegParm || regParm < 7) && "Invalid regparm value");
      Bits = ((unsigned) cc) |
             (noReturn ? NoReturnMask : 0) |
             (producesResult ? ProducesResultMask : 0) |
             (hasRegParm ? ((regParm + 1) << RegParmOffset) : 0);
    }

    // Constructor with all defaults. Use when for example creating a
    // function know to use defaults.
    ExtInfo() : Bits(0) {}

    bool getNoReturn() const { return Bits & NoReturnMask; }
    bool getProducesResult() const { return Bits & ProducesResultMask; }
    bool getHasRegParm() const { return (Bits >> RegParmOffset) != 0; }
    unsigned getRegParm() const {
      unsigned RegParm = Bits >> RegParmOffset;
      if (RegParm > 0)
        --RegParm;
      return RegParm;
    }
    CallingConv getCC() const { return CallingConv(Bits & CallConvMask); }

    bool operator==(ExtInfo Other) const {
      return Bits == Other.Bits;
    }
    bool operator!=(ExtInfo Other) const {
      return Bits != Other.Bits;
    }

    // Note that we don't have setters. That is by design, use
    // the following with methods instead of mutating these objects.

    ExtInfo withNoReturn(bool noReturn) const {
      if (noReturn)
        return ExtInfo(Bits | NoReturnMask);
      else
        return ExtInfo(Bits & ~NoReturnMask);
    }

    ExtInfo withProducesResult(bool producesResult) const {
      if (producesResult)
        return ExtInfo(Bits | ProducesResultMask);
      else
        return ExtInfo(Bits & ~ProducesResultMask);
    }

    ExtInfo withRegParm(unsigned RegParm) const {
      assert(RegParm < 7 && "Invalid regparm value");
      return ExtInfo((Bits & ~RegParmMask) |
                     ((RegParm + 1) << RegParmOffset));
    }

    ExtInfo withCallingConv(CallingConv cc) const {
      return ExtInfo((Bits & ~CallConvMask) | (unsigned) cc);
    }

    void Profile(llvm::FoldingSetNodeID &ID) const {
      ID.AddInteger(Bits);
    }
  };

protected:
  FunctionType(TypeClass tc, QualType res,
               unsigned typeQuals, RefQualifierKind RefQualifier,
               QualType Canonical, bool Dependent,
               bool InstantiationDependent,
               bool VariablyModified, bool ContainsUnexpandedParameterPack,
               ExtInfo Info)
    : Type(tc, Canonical, Dependent, InstantiationDependent, VariablyModified,
           ContainsUnexpandedParameterPack),
      ResultType(res) {
    FunctionTypeBits.ExtInfo = Info.Bits;
    FunctionTypeBits.TypeQuals = typeQuals;
    FunctionTypeBits.RefQualifier = static_cast<unsigned>(RefQualifier);
  }
  unsigned getTypeQuals() const { return FunctionTypeBits.TypeQuals; }

  RefQualifierKind getRefQualifier() const {
    return static_cast<RefQualifierKind>(FunctionTypeBits.RefQualifier);
  }

public:

  QualType getResultType() const { return ResultType; }

  bool getHasRegParm() const { return getExtInfo().getHasRegParm(); }
  unsigned getRegParmType() const { return getExtInfo().getRegParm(); }
  bool getNoReturnAttr() const { return getExtInfo().getNoReturn(); }
  CallingConv getCallConv() const { return getExtInfo().getCC(); }
  ExtInfo getExtInfo() const { return ExtInfo(FunctionTypeBits.ExtInfo); }
  bool isConst() const { return getTypeQuals() & Qualifiers::Const; }
  bool isVolatile() const { return getTypeQuals() & Qualifiers::Volatile; }
  bool isRestrict() const { return getTypeQuals() & Qualifiers::Restrict; }

  /// \brief Determine the type of an expression that calls a function of
  /// this type.
  QualType getCallResultType(ASTContext &Context) const {
    return getResultType().getNonLValueExprType(Context);
  }

  static StringRef getNameForCallConv(CallingConv CC);

  static bool classof(const Type *T) {
    return T->getTypeClass() == FunctionNoProto ||
           T->getTypeClass() == FunctionProto;
  }
};

/// FunctionNoProtoType - Represents a K&R-style 'int foo()' function, which has
/// no information available about its arguments.
class FunctionNoProtoType : public FunctionType, public llvm::FoldingSetNode {
  FunctionNoProtoType(QualType Result, QualType Canonical, ExtInfo Info)
    : FunctionType(FunctionNoProto, Result, 0, RQ_None, Canonical,
                   /*Dependent=*/false, /*InstantiationDependent=*/false,
                   Result->isVariablyModifiedType(),
                   /*ContainsUnexpandedParameterPack=*/false, Info) {}

  friend class ASTContext;  // ASTContext creates these.

public:
  // No additional state past what FunctionType provides.

  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getResultType(), getExtInfo());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, QualType ResultType,
                      ExtInfo Info) {
    Info.Profile(ID);
    ID.AddPointer(ResultType.getAsOpaquePtr());
  }

  static bool classof(const Type *T) {
    return T->getTypeClass() == FunctionNoProto;
  }
};

/// FunctionProtoType - Represents a prototype with argument type info, e.g.
/// 'int foo(int)' or 'int foo(void)'.  'void' is represented as having no
/// arguments, not as having a single void argument. Such a type can have an
/// exception specification, but this specification is not part of the canonical
/// type.
class FunctionProtoType : public FunctionType, public llvm::FoldingSetNode {
public:
  /// ExtProtoInfo - Extra information about a function prototype.
  struct ExtProtoInfo {
    ExtProtoInfo() :
      Variadic(false), HasTrailingReturn(false), TypeQuals(0),
      ExceptionSpecType(EST_None), RefQualifier(RQ_None),
      NumExceptions(0), Exceptions(0), NoexceptExpr(0),
      ExceptionSpecDecl(0), ExceptionSpecTemplate(0),
      ConsumedArguments(0) {}

    FunctionType::ExtInfo ExtInfo;
    bool Variadic : 1;
    bool HasTrailingReturn : 1;
    unsigned char TypeQuals;
    ExceptionSpecificationType ExceptionSpecType;
    RefQualifierKind RefQualifier;
    unsigned NumExceptions;
    const QualType *Exceptions;
    Expr *NoexceptExpr;
    FunctionDecl *ExceptionSpecDecl;
    FunctionDecl *ExceptionSpecTemplate;
    const bool *ConsumedArguments;
  };

private:
  /// \brief Determine whether there are any argument types that
  /// contain an unexpanded parameter pack.
  static bool containsAnyUnexpandedParameterPack(const QualType *ArgArray,
                                                 unsigned numArgs) {
    for (unsigned Idx = 0; Idx < numArgs; ++Idx)
      if (ArgArray[Idx]->containsUnexpandedParameterPack())
        return true;

    return false;
  }

  FunctionProtoType(QualType result, const QualType *args, unsigned numArgs,
                    QualType canonical, const ExtProtoInfo &epi);

  /// NumArgs - The number of arguments this function has, not counting '...'.
  unsigned NumArgs : 17;

  /// NumExceptions - The number of types in the exception spec, if any.
  unsigned NumExceptions : 9;

  /// ExceptionSpecType - The type of exception specification this function has.
  unsigned ExceptionSpecType : 3;

  /// HasAnyConsumedArgs - Whether this function has any consumed arguments.
  unsigned HasAnyConsumedArgs : 1;

  /// Variadic - Whether the function is variadic.
  unsigned Variadic : 1;

  /// HasTrailingReturn - Whether this function has a trailing return type.
  unsigned HasTrailingReturn : 1;

  // ArgInfo - There is an variable size array after the class in memory that
  // holds the argument types.

  // Exceptions - There is another variable size array after ArgInfo that
  // holds the exception types.

  // NoexceptExpr - Instead of Exceptions, there may be a single Expr* pointing
  // to the expression in the noexcept() specifier.

  // ExceptionSpecDecl, ExceptionSpecTemplate - Instead of Exceptions, there may
  // be a pair of FunctionDecl* pointing to the function which should be used to
  // instantiate this function type's exception specification, and the function
  // from which it should be instantiated.

  // ConsumedArgs - A variable size array, following Exceptions
  // and of length NumArgs, holding flags indicating which arguments
  // are consumed.  This only appears if HasAnyConsumedArgs is true.

  friend class ASTContext;  // ASTContext creates these.

  const bool *getConsumedArgsBuffer() const {
    assert(hasAnyConsumedArgs());

    // Find the end of the exceptions.
    Expr * const *eh_end = reinterpret_cast<Expr * const *>(arg_type_end());
    if (getExceptionSpecType() != EST_ComputedNoexcept)
      eh_end += NumExceptions;
    else
      eh_end += 1; // NoexceptExpr

    return reinterpret_cast<const bool*>(eh_end);
  }

public:
  unsigned getNumArgs() const { return NumArgs; }
  QualType getArgType(unsigned i) const {
    assert(i < NumArgs && "Invalid argument number!");
    return arg_type_begin()[i];
  }

  ExtProtoInfo getExtProtoInfo() const {
    ExtProtoInfo EPI;
    EPI.ExtInfo = getExtInfo();
    EPI.Variadic = isVariadic();
    EPI.HasTrailingReturn = hasTrailingReturn();
    EPI.ExceptionSpecType = getExceptionSpecType();
    EPI.TypeQuals = static_cast<unsigned char>(getTypeQuals());
    EPI.RefQualifier = getRefQualifier();
    if (EPI.ExceptionSpecType == EST_Dynamic) {
      EPI.NumExceptions = NumExceptions;
      EPI.Exceptions = exception_begin();
    } else if (EPI.ExceptionSpecType == EST_ComputedNoexcept) {
      EPI.NoexceptExpr = getNoexceptExpr();
    } else if (EPI.ExceptionSpecType == EST_Uninstantiated) {
      EPI.ExceptionSpecDecl = getExceptionSpecDecl();
      EPI.ExceptionSpecTemplate = getExceptionSpecTemplate();
    } else if (EPI.ExceptionSpecType == EST_Unevaluated) {
      EPI.ExceptionSpecDecl = getExceptionSpecDecl();
    }
    if (hasAnyConsumedArgs())
      EPI.ConsumedArguments = getConsumedArgsBuffer();
    return EPI;
  }

  /// \brief Get the kind of exception specification on this function.
  ExceptionSpecificationType getExceptionSpecType() const {
    return static_cast<ExceptionSpecificationType>(ExceptionSpecType);
  }
  /// \brief Return whether this function has any kind of exception spec.
  bool hasExceptionSpec() const {
    return getExceptionSpecType() != EST_None;
  }
  /// \brief Return whether this function has a dynamic (throw) exception spec.
  bool hasDynamicExceptionSpec() const {
    return isDynamicExceptionSpec(getExceptionSpecType());
  }
  /// \brief Return whether this function has a noexcept exception spec.
  bool hasNoexceptExceptionSpec() const {
    return isNoexceptExceptionSpec(getExceptionSpecType());
  }
  /// \brief Result type of getNoexceptSpec().
  enum NoexceptResult {
    NR_NoNoexcept,  ///< There is no noexcept specifier.
    NR_BadNoexcept, ///< The noexcept specifier has a bad expression.
    NR_Dependent,   ///< The noexcept specifier is dependent.
    NR_Throw,       ///< The noexcept specifier evaluates to false.
    NR_Nothrow      ///< The noexcept specifier evaluates to true.
  };
  /// \brief Get the meaning of the noexcept spec on this function, if any.
  NoexceptResult getNoexceptSpec(ASTContext &Ctx) const;
  unsigned getNumExceptions() const { return NumExceptions; }
  QualType getExceptionType(unsigned i) const {
    assert(i < NumExceptions && "Invalid exception number!");
    return exception_begin()[i];
  }
  Expr *getNoexceptExpr() const {
    if (getExceptionSpecType() != EST_ComputedNoexcept)
      return 0;
    // NoexceptExpr sits where the arguments end.
    return *reinterpret_cast<Expr *const *>(arg_type_end());
  }
  /// \brief If this function type has an exception specification which hasn't
  /// been determined yet (either because it has not been evaluated or because
  /// it has not been instantiated), this is the function whose exception
  /// specification is represented by this type.
  FunctionDecl *getExceptionSpecDecl() const {
    if (getExceptionSpecType() != EST_Uninstantiated &&
        getExceptionSpecType() != EST_Unevaluated)
      return 0;
    return reinterpret_cast<FunctionDecl * const *>(arg_type_end())[0];
  }
  /// \brief If this function type has an uninstantiated exception
  /// specification, this is the function whose exception specification
  /// should be instantiated to find the exception specification for
  /// this type.
  FunctionDecl *getExceptionSpecTemplate() const {
    if (getExceptionSpecType() != EST_Uninstantiated)
      return 0;
    return reinterpret_cast<FunctionDecl * const *>(arg_type_end())[1];
  }
  bool isNothrow(ASTContext &Ctx) const {
    ExceptionSpecificationType EST = getExceptionSpecType();
    assert(EST != EST_Unevaluated && EST != EST_Uninstantiated);
    if (EST == EST_DynamicNone || EST == EST_BasicNoexcept)
      return true;
    if (EST != EST_ComputedNoexcept)
      return false;
    return getNoexceptSpec(Ctx) == NR_Nothrow;
  }

  bool isVariadic() const { return Variadic; }

  /// \brief Determines whether this function prototype contains a
  /// parameter pack at the end.
  ///
  /// A function template whose last parameter is a parameter pack can be
  /// called with an arbitrary number of arguments, much like a variadic
  /// function.
  bool isTemplateVariadic() const;

  bool hasTrailingReturn() const { return HasTrailingReturn; }

  unsigned getTypeQuals() const { return FunctionType::getTypeQuals(); }


  /// \brief Retrieve the ref-qualifier associated with this function type.
  RefQualifierKind getRefQualifier() const {
    return FunctionType::getRefQualifier();
  }

  typedef const QualType *arg_type_iterator;
  arg_type_iterator arg_type_begin() const {
    return reinterpret_cast<const QualType *>(this+1);
  }
  arg_type_iterator arg_type_end() const { return arg_type_begin()+NumArgs; }

  typedef const QualType *exception_iterator;
  exception_iterator exception_begin() const {
    // exceptions begin where arguments end
    return arg_type_end();
  }
  exception_iterator exception_end() const {
    if (getExceptionSpecType() != EST_Dynamic)
      return exception_begin();
    return exception_begin() + NumExceptions;
  }

  bool hasAnyConsumedArgs() const {
    return HasAnyConsumedArgs;
  }
  bool isArgConsumed(unsigned I) const {
    assert(I < getNumArgs() && "argument index out of range!");
    if (hasAnyConsumedArgs())
      return getConsumedArgsBuffer()[I];
    return false;
  }

  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  // FIXME: Remove the string version.
  void printExceptionSpecification(std::string &S, 
                                   const PrintingPolicy &Policy) const;
  void printExceptionSpecification(raw_ostream &OS, 
                                   const PrintingPolicy &Policy) const;

  static bool classof(const Type *T) {
    return T->getTypeClass() == FunctionProto;
  }

  void Profile(llvm::FoldingSetNodeID &ID, const ASTContext &Ctx);
  static void Profile(llvm::FoldingSetNodeID &ID, QualType Result,
                      arg_type_iterator ArgTys, unsigned NumArgs,
                      const ExtProtoInfo &EPI, const ASTContext &Context);
};

class TypedefType : public Type {
  TypedefNameDecl *Decl;
protected:
  TypedefType(TypeClass tc, const TypedefNameDecl *D, QualType can)
    : Type(tc, can, can->isDependentType(),
           can->isInstantiationDependentType(),
           can->isVariablyModifiedType(),
           /*ContainsUnexpandedParameterPack=*/false),
      Decl(const_cast<TypedefNameDecl*>(D)) {
    assert(!isa<TypedefType>(can) && "Invalid canonical type");
  }
  friend class ASTContext;  // ASTContext creates these.
public:

  TypedefNameDecl *getDecl() const { return Decl; }

  bool isSugared() const { return true; }
  QualType desugar() const;

  static bool classof(const Type *T) { return T->getTypeClass() == Typedef; }
};

/// TypeOfExprType (GCC extension).
class TypeOfExprType : public Type {
  Expr *TOExpr;

protected:
  TypeOfExprType(Expr *E, QualType can = QualType());
  friend class ASTContext;  // ASTContext creates these.
public:
  Expr *getUnderlyingExpr() const { return TOExpr; }

  /// \brief Remove a single level of sugar.
  QualType desugar() const;

  /// \brief Returns whether this type directly provides sugar.
  bool isSugared() const;

  static bool classof(const Type *T) { return T->getTypeClass() == TypeOfExpr; }
};

/// \brief Internal representation of canonical, dependent
/// typeof(expr) types.
///
/// This class is used internally by the ASTContext to manage
/// canonical, dependent types, only. Clients will only see instances
/// of this class via TypeOfExprType nodes.
class DependentTypeOfExprType
  : public TypeOfExprType, public llvm::FoldingSetNode {
  const ASTContext &Context;

public:
  DependentTypeOfExprType(const ASTContext &Context, Expr *E)
    : TypeOfExprType(E), Context(Context) { }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, Context, getUnderlyingExpr());
  }

  static void Profile(llvm::FoldingSetNodeID &ID, const ASTContext &Context,
                      Expr *E);
};

/// TypeOfType (GCC extension).
class TypeOfType : public Type {
  QualType TOType;
  TypeOfType(QualType T, QualType can)
    : Type(TypeOf, can, T->isDependentType(),
           T->isInstantiationDependentType(),
           T->isVariablyModifiedType(),
           T->containsUnexpandedParameterPack()),
      TOType(T) {
    assert(!isa<TypedefType>(can) && "Invalid canonical type");
  }
  friend class ASTContext;  // ASTContext creates these.
public:
  QualType getUnderlyingType() const { return TOType; }

  /// \brief Remove a single level of sugar.
  QualType desugar() const { return getUnderlyingType(); }

  /// \brief Returns whether this type directly provides sugar.
  bool isSugared() const { return true; }

  static bool classof(const Type *T) { return T->getTypeClass() == TypeOf; }
};

/// DecltypeType (C++0x)
class DecltypeType : public Type {
  Expr *E;
  QualType UnderlyingType;

protected:
  DecltypeType(Expr *E, QualType underlyingType, QualType can = QualType());
  friend class ASTContext;  // ASTContext creates these.
public:
  Expr *getUnderlyingExpr() const { return E; }
  QualType getUnderlyingType() const { return UnderlyingType; }

  /// \brief Remove a single level of sugar.
  QualType desugar() const;

  /// \brief Returns whether this type directly provides sugar.
  bool isSugared() const;

  static bool classof(const Type *T) { return T->getTypeClass() == Decltype; }
};

/// \brief Internal representation of canonical, dependent
/// decltype(expr) types.
///
/// This class is used internally by the ASTContext to manage
/// canonical, dependent types, only. Clients will only see instances
/// of this class via DecltypeType nodes.
class DependentDecltypeType : public DecltypeType, public llvm::FoldingSetNode {
  const ASTContext &Context;

public:
  DependentDecltypeType(const ASTContext &Context, Expr *E);

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, Context, getUnderlyingExpr());
  }

  static void Profile(llvm::FoldingSetNodeID &ID, const ASTContext &Context,
                      Expr *E);
};

/// \brief A unary type transform, which is a type constructed from another
class UnaryTransformType : public Type {
public:
  enum UTTKind {
    EnumUnderlyingType
  };

private:
  /// The untransformed type.
  QualType BaseType;
  /// The transformed type if not dependent, otherwise the same as BaseType.
  QualType UnderlyingType;

  UTTKind UKind;
protected:
  UnaryTransformType(QualType BaseTy, QualType UnderlyingTy, UTTKind UKind,
                     QualType CanonicalTy);
  friend class ASTContext;
public:
  bool isSugared() const { return !isDependentType(); }
  QualType desugar() const { return UnderlyingType; }

  QualType getUnderlyingType() const { return UnderlyingType; }
  QualType getBaseType() const { return BaseType; }

  UTTKind getUTTKind() const { return UKind; }

  static bool classof(const Type *T) {
    return T->getTypeClass() == UnaryTransform;
  }
};
#endif

class StructType : public Type {
  /// Stores the StructTypeDecl associated with this type.
  StructTypeDecl *decl;

  explicit StructType(const StructTypeDecl *D)
    : Type(Struct, /*CanonicalTy=*/0), decl(const_cast<StructTypeDecl*>(D)) {}
  friend class ASTContext;   // ASTContext creates these.

public:
  StructTypeDecl *getDecl() const { return decl; }

  static bool classof(const Type *T) { return T->getTypeClass() == Struct; }
};

#if 0
class TagType : public Type {
  /// Stores the TagDecl associated with this type. The decl may point to any
  /// TagDecl that declares the entity.
  TagDecl * decl;

  friend class ASTReader;
  
protected:
  TagType(TypeClass TC, const TagDecl *D, QualType can);

public:
  TagDecl *getDecl() const;

  /// @brief Determines whether this type is in the process of being
  /// defined.
  bool isBeingDefined() const;

  static bool classof(const Type *T) {
    return T->getTypeClass() >= TagFirst && T->getTypeClass() <= TagLast;
  }
};

/// RecordType - This is a helper class that allows the use of isa/cast/dyncast
/// to detect TagType objects of structs/unions/classes.
class RecordType : public TagType {
protected:
  explicit RecordType(const RecordDecl *D)
    : TagType(Record, reinterpret_cast<const TagDecl*>(D), QualType()) { }
  explicit RecordType(TypeClass TC, RecordDecl *D)
    : TagType(TC, reinterpret_cast<const TagDecl*>(D), QualType()) { }
  friend class ASTContext;   // ASTContext creates these.
public:

  RecordDecl *getDecl() const {
    return reinterpret_cast<RecordDecl*>(TagType::getDecl());
  }

  // FIXME: This predicate is a helper to QualType/Type. It needs to
  // recursively check all fields for const-ness. If any field is declared
  // const, it needs to return false.
  bool hasConstFields() const { return false; }

  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  static bool classof(const Type *T) { return T->getTypeClass() == Record; }
};

/// EnumType - This is a helper class that allows the use of isa/cast/dyncast
/// to detect TagType objects of enums.
class EnumType : public TagType {
  explicit EnumType(const EnumDecl *D)
    : TagType(Enum, reinterpret_cast<const TagDecl*>(D), QualType()) { }
  friend class ASTContext;   // ASTContext creates these.
public:

  EnumDecl *getDecl() const {
    return reinterpret_cast<EnumDecl*>(TagType::getDecl());
  }

  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  static bool classof(const Type *T) { return T->getTypeClass() == Enum; }
};

/// AttributedType - An attributed type is a type to which a type
/// attribute has been applied.  The "modified type" is the
/// fully-sugared type to which the attributed type was applied;
/// generally it is not canonically equivalent to the attributed type.
/// The "equivalent type" is the minimally-desugared type which the
/// type is canonically equivalent to.
///
/// For example, in the following attributed type:
///     int32_t __attribute__((vector_size(16)))
///   - the modified type is the TypedefType for int32_t
///   - the equivalent type is VectorType(16, int32_t)
///   - the canonical type is VectorType(16, int)
class AttributedType : public Type, public llvm::FoldingSetNode {
public:
  // It is really silly to have yet another attribute-kind enum, but
  // gong::attr::Kind doesn't currently cover the pure type attrs.
  enum Kind {
    // Expression operand.
    attr_address_space,
    attr_regparm,
    attr_vector_size,
    attr_neon_vector_type,
    attr_neon_polyvector_type,

    FirstExprOperandKind = attr_address_space,
    LastExprOperandKind = attr_neon_polyvector_type,

    // Enumerated operand (string or keyword).
    attr_pcs,

    FirstEnumOperandKind = attr_pcs,
    LastEnumOperandKind = attr_pcs,

    // No operand.
    attr_noreturn,
    attr_cdecl,
    attr_fastcall,
    attr_stdcall,
    attr_thiscall,
    attr_pascal,
    attr_pnaclcall
  };

private:
  QualType ModifiedType;
  QualType EquivalentType;

  friend class ASTContext; // creates these

  AttributedType(QualType canon, Kind attrKind,
                 QualType modified, QualType equivalent)
    : Type(Attributed, canon, canon->isDependentType(),
           canon->isInstantiationDependentType(),
           canon->isVariablyModifiedType(),
           canon->containsUnexpandedParameterPack()),
      ModifiedType(modified), EquivalentType(equivalent) {
    AttributedTypeBits.AttrKind = attrKind;
  }

public:
  Kind getAttrKind() const {
    return static_cast<Kind>(AttributedTypeBits.AttrKind);
  }

  QualType getModifiedType() const { return ModifiedType; }
  QualType getEquivalentType() const { return EquivalentType; }

  bool isSugared() const { return true; }
  QualType desugar() const { return getEquivalentType(); }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getAttrKind(), ModifiedType, EquivalentType);
  }

  static void Profile(llvm::FoldingSetNodeID &ID, Kind attrKind,
                      QualType modified, QualType equivalent) {
    ID.AddInteger(attrKind);
    ID.AddPointer(modified.getAsOpaquePtr());
    ID.AddPointer(equivalent.getAsOpaquePtr());
  }

  static bool classof(const Type *T) {
    return T->getTypeClass() == Attributed;
  }
};

/// \brief Represents a C++0x auto type.
///
/// These types are usually a placeholder for a deduced type. However, within
/// templates and before the initializer is attached, there is no deduced type
/// and an auto type is type-dependent and canonical.
class AutoType : public Type, public llvm::FoldingSetNode {
  AutoType(QualType DeducedType)
    : Type(Auto, DeducedType.isNull() ? QualType(this, 0) : DeducedType,
           /*Dependent=*/DeducedType.isNull(),
           /*InstantiationDependent=*/DeducedType.isNull(),
           /*VariablyModified=*/false, /*ContainsParameterPack=*/false) {
    assert((DeducedType.isNull() || !DeducedType->isDependentType()) &&
           "deduced a dependent type for auto");
  }

  friend class ASTContext;  // ASTContext creates these

public:
  bool isSugared() const { return isDeduced(); }
  QualType desugar() const { return getCanonicalTypeInternal(); }

  QualType getDeducedType() const {
    return isDeduced() ? getCanonicalTypeInternal() : QualType();
  }
  bool isDeduced() const {
    return !isDependentType();
  }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getDeducedType());
  }

  static void Profile(llvm::FoldingSetNodeID &ID,
                      QualType Deduced) {
    ID.AddPointer(Deduced.getAsOpaquePtr());
  }

  static bool classof(const Type *T) {
    return T->getTypeClass() == Auto;
  }
};

/// \brief The kind of a tag type.
enum TagTypeKind {
  /// \brief The "struct" keyword.
  TTK_Struct,
  /// \brief The "__interface" keyword.
  TTK_Interface,
  /// \brief The "union" keyword.
  TTK_Union,
  /// \brief The "class" keyword.
  TTK_Class,
  /// \brief The "enum" keyword.
  TTK_Enum
};

/// \brief The elaboration keyword that precedes a qualified type name or
/// introduces an elaborated-type-specifier.
enum ElaboratedTypeKeyword {
  /// \brief The "struct" keyword introduces the elaborated-type-specifier.
  ETK_Struct,
  /// \brief The "__interface" keyword introduces the elaborated-type-specifier.
  ETK_Interface,
  /// \brief The "union" keyword introduces the elaborated-type-specifier.
  ETK_Union,
  /// \brief The "class" keyword introduces the elaborated-type-specifier.
  ETK_Class,
  /// \brief The "enum" keyword introduces the elaborated-type-specifier.
  ETK_Enum,
  /// \brief The "typename" keyword precedes the qualified type name, e.g.,
  /// \c typename T::type.
  ETK_Typename,
  /// \brief No keyword precedes the qualified type name.
  ETK_None
};

/// A helper class for Type nodes having an ElaboratedTypeKeyword.
/// The keyword in stored in the free bits of the base class.
/// Also provides a few static helpers for converting and printing
/// elaborated type keyword and tag type kind enumerations.
class TypeWithKeyword : public Type {
protected:
  TypeWithKeyword(ElaboratedTypeKeyword Keyword, TypeClass tc,
                  QualType Canonical, bool Dependent,
                  bool InstantiationDependent, bool VariablyModified,
                  bool ContainsUnexpandedParameterPack)
  : Type(tc, Canonical, Dependent, InstantiationDependent, VariablyModified,
         ContainsUnexpandedParameterPack) {
    TypeWithKeywordBits.Keyword = Keyword;
  }

public:
  ElaboratedTypeKeyword getKeyword() const {
    return static_cast<ElaboratedTypeKeyword>(TypeWithKeywordBits.Keyword);
  }

  /// getKeywordForTypeSpec - Converts a type specifier (DeclSpec::TST)
  /// into an elaborated type keyword.
  static ElaboratedTypeKeyword getKeywordForTypeSpec(unsigned TypeSpec);

  /// getTagTypeKindForTypeSpec - Converts a type specifier (DeclSpec::TST)
  /// into a tag type kind.  It is an error to provide a type specifier
  /// which *isn't* a tag kind here.
  static TagTypeKind getTagTypeKindForTypeSpec(unsigned TypeSpec);

  /// getKeywordForTagDeclKind - Converts a TagTypeKind into an
  /// elaborated type keyword.
  static ElaboratedTypeKeyword getKeywordForTagTypeKind(TagTypeKind Tag);

  /// getTagTypeKindForKeyword - Converts an elaborated type keyword into
  // a TagTypeKind. It is an error to provide an elaborated type keyword
  /// which *isn't* a tag kind here.
  static TagTypeKind getTagTypeKindForKeyword(ElaboratedTypeKeyword Keyword);

  static bool KeywordIsTagTypeKind(ElaboratedTypeKeyword Keyword);

  static const char *getKeywordName(ElaboratedTypeKeyword Keyword);

  static const char *getTagTypeKindName(TagTypeKind Kind) {
    return getKeywordName(getKeywordForTagTypeKind(Kind));
  }

  class CannotCastToThisType {};
  static CannotCastToThisType classof(const Type *);
};

/// \brief Represents a type that was referred to using an elaborated type
/// keyword, e.g., struct S, or via a qualified name, e.g., N::M::type,
/// or both.
///
/// This type is used to keep track of a type name as written in the
/// source code, including tag keywords and any nested-name-specifiers.
/// The type itself is always "sugar", used to express what was written
/// in the source code but containing no additional semantic information.
class ElaboratedType : public TypeWithKeyword, public llvm::FoldingSetNode {

  /// \brief The nested name specifier containing the qualifier.
  NestedNameSpecifier *NNS;

  /// \brief The type that this qualified name refers to.
  QualType NamedType;

  ElaboratedType(ElaboratedTypeKeyword Keyword, NestedNameSpecifier *NNS,
                 QualType NamedType, QualType CanonType)
    : TypeWithKeyword(Keyword, Elaborated, CanonType,
                      NamedType->isDependentType(),
                      NamedType->isInstantiationDependentType(),
                      NamedType->isVariablyModifiedType(),
                      NamedType->containsUnexpandedParameterPack()),
      NNS(NNS), NamedType(NamedType) {
    assert(!(Keyword == ETK_None && NNS == 0) &&
           "ElaboratedType cannot have elaborated type keyword "
           "and name qualifier both null.");
  }

  friend class ASTContext;  // ASTContext creates these

public:
  ~ElaboratedType();

  /// \brief Retrieve the qualification on this type.
  NestedNameSpecifier *getQualifier() const { return NNS; }

  /// \brief Retrieve the type named by the qualified-id.
  QualType getNamedType() const { return NamedType; }

  /// \brief Remove a single level of sugar.
  QualType desugar() const { return getNamedType(); }

  /// \brief Returns whether this type directly provides sugar.
  bool isSugared() const { return true; }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getKeyword(), NNS, NamedType);
  }

  static void Profile(llvm::FoldingSetNodeID &ID, ElaboratedTypeKeyword Keyword,
                      NestedNameSpecifier *NNS, QualType NamedType) {
    ID.AddInteger(Keyword);
    ID.AddPointer(NNS);
    NamedType.Profile(ID);
  }

  static bool classof(const Type *T) {
    return T->getTypeClass() == Elaborated;
  }
};

class AtomicType : public Type, public llvm::FoldingSetNode {
  QualType ValueType;

  AtomicType(QualType ValTy, QualType Canonical)
    : Type(Atomic, Canonical, ValTy->isDependentType(),
           ValTy->isInstantiationDependentType(),
           ValTy->isVariablyModifiedType(),
           ValTy->containsUnexpandedParameterPack()),
      ValueType(ValTy) {}
  friend class ASTContext;  // ASTContext creates these.

  public:
  /// getValueType - Gets the type contained by this atomic type, i.e.
  /// the type returned by performing an atomic load of this atomic type.
  QualType getValueType() const { return ValueType; }

  bool isSugared() const { return false; }
  QualType desugar() const { return QualType(this, 0); }

  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getValueType());
  }
  static void Profile(llvm::FoldingSetNodeID &ID, QualType T) {
    ID.AddPointer(T.getAsOpaquePtr());
  }
  static bool classof(const Type *T) {
    return T->getTypeClass() == Atomic;
  }
};

/// A qualifier set is used to build a set of qualifiers.
class QualifierCollector : public Qualifiers {
public:
  QualifierCollector(Qualifiers Qs = Qualifiers()) : Qualifiers(Qs) {}

  /// Collect any qualifiers on the given type and return an
  /// unqualified type.  The qualifiers are assumed to be consistent
  /// with those already in the type.
  const Type *strip(QualType type) {
    addFastQualifiers(type.getLocalFastQualifiers());
    if (!type.hasLocalNonFastQualifiers())
      return type.getTypePtrUnsafe();

    const ExtQuals *extQuals = type.getExtQualsUnsafe();
    addConsistentQualifiers(extQuals->getQualifiers());
    return extQuals->getBaseType();
  }

  /// Apply the collected qualifiers to the given type.
  QualType apply(const ASTContext &Context, QualType QT) const;

  /// Apply the collected qualifiers to the given type.
  QualType apply(const ASTContext &Context, const Type* T) const;
};


// Inline function definitions.

inline FunctionType::ExtInfo getFunctionExtInfo(const Type &t) {
  if (const PointerType *PT = t.getAs<PointerType>()) {
    if (const FunctionType *FT = PT->getPointeeType()->getAs<FunctionType>())
      return FT->getExtInfo();
  } else if (const FunctionType *FT = t.getAs<FunctionType>())
    return FT->getExtInfo();

  return FunctionType::ExtInfo();
}

inline FunctionType::ExtInfo getFunctionExtInfo(QualType t) {
  return getFunctionExtInfo(*t);
}

/// \brief Tests whether the type is categorized as a fundamental type.
///
/// \returns True for types specified in C++0x [basic.fundamental].
inline bool Type::isFundamentalType() const {
  return isVoidType() ||
         // FIXME: It's really annoying that we don't have an
         // 'isArithmeticType()' which agrees with the standard definition.
         (isArithmeticType() && !isEnumeralType());
}

/// \brief Tests whether the type is categorized as a compound type.
///
/// \returns True for types specified in C++0x [basic.compound].
inline bool Type::isCompoundType() const {
  // C++0x [basic.compound]p1:
  //   Compound types can be constructed in the following ways:
  //    -- arrays of objects of a given type [...];
  return isArrayType() ||
  //    -- functions, which have parameters of given types [...];
         isFunctionType() ||
  //    -- pointers to void or objects or functions [...];
         isPointerType() ||
  //    -- references to objects or functions of a given type. [...]
         isReferenceType() ||
  //    -- classes containing a sequence of objects of various types, [...];
         isRecordType() ||
  //    -- unions, which are classes capable of containing objects of different
  //               types at different times;
         isUnionType() ||
  //    -- enumerations, which comprise a set of named constant values. [...];
         isEnumeralType() ||
  //    -- pointers to non-static class members, [...].
         isMemberPointerType();
}

inline bool Type::isFunctionType() const {
  return isa<FunctionType>(CanonicalType);
}
inline bool Type::isPointerType() const {
  return isa<PointerType>(CanonicalType);
}
inline bool Type::isAnyPointerType() const {
  return isPointerType();
}
inline bool Type::isBlockPointerType() const {
  return isa<BlockPointerType>(CanonicalType);
}
inline bool Type::isReferenceType() const {
  return isa<ReferenceType>(CanonicalType);
}
inline bool Type::isLValueReferenceType() const {
  return isa<LValueReferenceType>(CanonicalType);
}
inline bool Type::isRValueReferenceType() const {
  return isa<RValueReferenceType>(CanonicalType);
}
inline bool Type::isFunctionPointerType() const {
  if (const PointerType *T = getAs<PointerType>())
    return T->getPointeeType()->isFunctionType();
  else
    return false;
}
inline bool Type::isMemberPointerType() const {
  return isa<MemberPointerType>(CanonicalType);
}
inline bool Type::isMemberFunctionPointerType() const {
  if (const MemberPointerType* T = getAs<MemberPointerType>())
    return T->isMemberFunctionPointer();
  else
    return false;
}
inline bool Type::isMemberDataPointerType() const {
  if (const MemberPointerType* T = getAs<MemberPointerType>())
    return T->isMemberDataPointer();
  else
    return false;
}
inline bool Type::isArrayType() const {
  return isa<ArrayType>(CanonicalType);
}
inline bool Type::isConstantArrayType() const {
  return isa<ConstantArrayType>(CanonicalType);
}
inline bool Type::isIncompleteArrayType() const {
  return isa<IncompleteArrayType>(CanonicalType);
}
inline bool Type::isVariableArrayType() const {
  return isa<VariableArrayType>(CanonicalType);
}
inline bool Type::isBuiltinType() const {
  return isa<BuiltinType>(CanonicalType);
}
inline bool Type::isRecordType() const {
  return isa<RecordType>(CanonicalType);
}
inline bool Type::isEnumeralType() const {
  return isa<EnumType>(CanonicalType);
}
inline bool Type::isAnyComplexType() const {
  return isa<ComplexType>(CanonicalType);
}
inline bool Type::isVectorType() const {
  return isa<VectorType>(CanonicalType);
}
inline bool Type::isExtVectorType() const {
  return isa<ExtVectorType>(CanonicalType);
}
inline bool Type::isAtomicType() const {
  return isa<AtomicType>(CanonicalType);
}

inline bool Type::isSpecificBuiltinType(unsigned K) const {
  if (const BuiltinType *BT = getAs<BuiltinType>())
    if (BT->getKind() == (BuiltinType::Kind) K)
      return true;
  return false;
}

inline bool Type::isPlaceholderType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(this))
    return BT->isPlaceholderType();
  return false;
}

inline const BuiltinType *Type::getAsPlaceholderType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(this))
    if (BT->isPlaceholderType())
      return BT;
  return 0;
}

inline bool Type::isSpecificPlaceholderType(unsigned K) const {
  assert(BuiltinType::isPlaceholderTypeKind((BuiltinType::Kind) K));
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(this))
    return (BT->getKind() == (BuiltinType::Kind) K);
  return false;
}

inline bool Type::isNonOverloadPlaceholderType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(this))
    return BT->isNonOverloadPlaceholderType();
  return false;
}

inline bool Type::isVoidType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() == BuiltinType::Void;
  return false;
}

inline bool Type::isHalfType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() == BuiltinType::Half;
  // FIXME: Should we allow complex __fp16? Probably not.
  return false;
}

inline bool Type::isNullPtrType() const {
  if (const BuiltinType *BT = getAs<BuiltinType>())
    return BT->getKind() == BuiltinType::NullPtr;
  return false;
}

extern bool IsEnumDeclComplete(EnumDecl *);
extern bool IsEnumDeclScoped(EnumDecl *);

inline bool Type::isIntegerType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() >= BuiltinType::Bool &&
           BT->getKind() <= BuiltinType::Int128;
  if (const EnumType *ET = dyn_cast<EnumType>(CanonicalType)) {
    // Incomplete enum types are not treated as integer types.
    // FIXME: In C++, enum types are never integer types.
    return IsEnumDeclComplete(ET->getDecl()) &&
      !IsEnumDeclScoped(ET->getDecl());
  }
  return false;
}

inline bool Type::isScalarType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() > BuiltinType::Void &&
           BT->getKind() <= BuiltinType::NullPtr;
  if (const EnumType *ET = dyn_cast<EnumType>(CanonicalType))
    // Enums are scalar types, but only if they are defined.  Incomplete enums
    // are not treated as scalar types.
    return IsEnumDeclComplete(ET->getDecl());
  return isa<PointerType>(CanonicalType) ||
         isa<BlockPointerType>(CanonicalType) ||
         isa<MemberPointerType>(CanonicalType) ||
         isa<ComplexType>(CanonicalType);
}

inline bool Type::isIntegralOrEnumerationType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() >= BuiltinType::Bool &&
           BT->getKind() <= BuiltinType::Int128;

  // Check for a complete enum type; incomplete enum types are not properly an
  // enumeration type in the sense required here.
  if (const EnumType *ET = dyn_cast<EnumType>(CanonicalType))
    return IsEnumDeclComplete(ET->getDecl());

  return false;  
}

inline bool Type::isBooleanType() const {
  if (const BuiltinType *BT = dyn_cast<BuiltinType>(CanonicalType))
    return BT->getKind() == BuiltinType::Bool;
  return false;
}

/// \brief Determines whether this is a type for which one can define
/// an overloaded operator.
inline bool Type::isOverloadableType() const {
  return isDependentType() || isRecordType() || isEnumeralType();
}

/// \brief Determines whether this type can decay to a pointer type.
inline bool Type::canDecayToPointerType() const {
  return isFunctionType() || isArrayType();
}

inline bool Type::hasPointerRepresentation() const {
  return (isPointerType() || isReferenceType() || isBlockPointerType() ||
          isNullPtrType());
}

inline const Type *Type::getBaseElementTypeUnsafe() const {
  const Type *type = this;
  while (const ArrayType *arrayType = type->getAsArrayTypeUnsafe())
    type = arrayType->getElementType().getTypePtr();
  return type;
}

/// Insertion operator for diagnostics.  This allows sending QualType's into a
/// diagnostic with <<.
inline const DiagnosticBuilder &operator<<(const DiagnosticBuilder &DB,
                                           QualType T) {
  DB.AddTaggedVal(reinterpret_cast<intptr_t>(T.getAsOpaquePtr()),
                  DiagnosticsEngine::ak_qualtype);
  return DB;
}

/// Insertion operator for partial diagnostics.  This allows sending QualType's
/// into a diagnostic with <<.
inline const PartialDiagnostic &operator<<(const PartialDiagnostic &PD,
                                           QualType T) {
  PD.AddTaggedVal(reinterpret_cast<intptr_t>(T.getAsOpaquePtr()),
                  DiagnosticsEngine::ak_qualtype);
  return PD;
}

// Helper class template that is used by Type::getAs to ensure that one does
// not try to look through a qualified type to get to an array type.
template<typename T,
         bool isArrayType = (llvm::is_same<T, ArrayType>::value ||
                             llvm::is_base_of<ArrayType, T>::value)>
struct ArrayType_cannot_be_used_with_getAs { };

template<typename T>
struct ArrayType_cannot_be_used_with_getAs<T, true>;

// Member-template getAs<specific type>'.
template <typename T> const T *Type::getAs() const {
  ArrayType_cannot_be_used_with_getAs<T> at;
  (void)at;

  // If this is directly a T type, return it.
  if (const T *Ty = dyn_cast<T>(this))
    return Ty;

  // If the canonical form of this type isn't the right kind, reject it.
  if (!isa<T>(CanonicalType))
    return 0;

  // If this is a typedef for the type, strip the typedef off without
  // losing all typedef information.
  return cast<T>(getUnqualifiedDesugaredType());
}

inline const ArrayType *Type::getAsArrayTypeUnsafe() const {
  // If this is directly an array type, return it.
  if (const ArrayType *arr = dyn_cast<ArrayType>(this))
    return arr;

  // If the canonical form of this type isn't the right kind, reject it.
  if (!isa<ArrayType>(CanonicalType))
    return 0;

  // If this is a typedef for the type, strip the typedef off without
  // losing all typedef information.
  return cast<ArrayType>(getUnqualifiedDesugaredType());
}

template <typename T> const T *Type::castAs() const {
  ArrayType_cannot_be_used_with_getAs<T> at;
  (void) at;

  assert(isa<T>(CanonicalType));
  if (const T *ty = dyn_cast<T>(this)) return ty;
  return cast<T>(getUnqualifiedDesugaredType());
}

inline const ArrayType *Type::castAsArrayTypeUnsafe() const {
  assert(isa<ArrayType>(CanonicalType));
  if (const ArrayType *arr = dyn_cast<ArrayType>(this)) return arr;
  return cast<ArrayType>(getUnqualifiedDesugaredType());
}

#endif

}  // end namespace gong

#endif
