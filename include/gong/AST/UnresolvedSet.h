//===-- UnresolvedSet.h - Unresolved sets of declarations  ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines the UnresolvedSet class, which is used to store
//  collections of declarations in the AST.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_GONG_AST_UNRESOLVEDSET_H
#define LLVM_GONG_AST_UNRESOLVEDSET_H

#include "gong/Basic/LLVM.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include <iterator>

namespace gong {

/// The iterator over UnresolvedSets.  Serves as both the const and
/// non-const iterator.
class UnresolvedSetIterator {
private:
  typedef llvm::MutableArrayRef<NamedDecl*> DeclsTy;
  typedef DeclsTy::iterator IteratorTy;

  IteratorTy ir;

  friend class UnresolvedSetImpl;
  friend class ASTUnresolvedSet;
  friend class OverloadExpr;
  explicit UnresolvedSetIterator(DeclsTy::iterator ir) : ir(ir) {}
  explicit UnresolvedSetIterator(DeclsTy::const_iterator ir) :
    ir(const_cast<DeclsTy::iterator>(ir)) {}
  
  IteratorTy getIterator() const { return ir; }
  
public:
  UnresolvedSetIterator() {}

  typedef std::iterator_traits<IteratorTy>::difference_type difference_type;
  typedef NamedDecl *value_type;
  typedef NamedDecl **pointer;
  typedef NamedDecl *reference;
  typedef std::iterator_traits<IteratorTy>::iterator_category iterator_category;

  NamedDecl *getDecl() const { return *ir; }

  NamedDecl *operator*() const { return getDecl(); }
  
  UnresolvedSetIterator &operator++() { ++ir; return *this; }
  UnresolvedSetIterator operator++(int) { return UnresolvedSetIterator(ir++); }
  UnresolvedSetIterator &operator--() { --ir; return *this; }
  UnresolvedSetIterator operator--(int) { return UnresolvedSetIterator(ir--); }

  UnresolvedSetIterator &operator+=(difference_type d) {
    ir += d; return *this;
  }
  UnresolvedSetIterator operator+(difference_type d) const {
    return UnresolvedSetIterator(ir + d);
  }
  UnresolvedSetIterator &operator-=(difference_type d) {
    ir -= d; return *this;
  }
  UnresolvedSetIterator operator-(difference_type d) const {
    return UnresolvedSetIterator(ir - d);
  }
  value_type operator[](difference_type d) const { return *(*this + d); }

  difference_type operator-(const UnresolvedSetIterator &o) const {
    return ir - o.ir;
  }

  bool operator==(const UnresolvedSetIterator &o) const { return ir == o.ir; }
  bool operator!=(const UnresolvedSetIterator &o) const { return ir != o.ir; }
  bool operator<(const UnresolvedSetIterator &o) const { return ir < o.ir; }
  bool operator<=(const UnresolvedSetIterator &o) const { return ir <= o.ir; }
  bool operator>=(const UnresolvedSetIterator &o) const { return ir >= o.ir; }
  bool operator>(const UnresolvedSetIterator &o) const { return ir > o.ir; }
};

/// UnresolvedSet - A set of unresolved declarations.
class UnresolvedSetImpl {
  typedef SmallVectorImpl<NamedDecl*> DeclsTy;

  // Don't allow direct construction, and only permit subclassing by
  // UnresolvedSet.
private:
  template <unsigned N> friend class UnresolvedSet;
  UnresolvedSetImpl() {}
  UnresolvedSetImpl(const UnresolvedSetImpl &) LLVM_DELETED_FUNCTION;

public:
  // We don't currently support assignment through this iterator, so we might
  // as well use the same implementation twice.
  typedef UnresolvedSetIterator iterator;
  typedef UnresolvedSetIterator const_iterator;

  iterator begin() { return iterator(decls().begin()); }
  iterator end() { return iterator(decls().end()); }

  const_iterator begin() const { return const_iterator(decls().begin()); }
  const_iterator end() const { return const_iterator(decls().end()); }

  void addDecl(NamedDecl *D) {
    decls().push_back(D);
  }

  void replace(iterator I, NamedDecl *New) {
    *I.ir = New;
  }

  void erase(unsigned I) {
    decls()[I] = decls().back();
    decls().pop_back();
  }

  void erase(iterator I) {
    *I.ir = decls().back();
    decls().pop_back();
  }

  void clear() { decls().clear(); }
  void set_size(unsigned N) { decls().set_size(N); }

  bool empty() const { return decls().empty(); }
  unsigned size() const { return decls().size(); }

  void append(iterator I, iterator E) {
    decls().append(I.ir, E.ir);
  }

  NamedDecl *&operator[](unsigned I) { return decls()[I]; }
  NamedDecl * const&operator[](unsigned I) const { return decls()[I]; }

private:
  // These work because the only permitted subclass is UnresolvedSetImpl

  DeclsTy &decls() {
    return *reinterpret_cast<DeclsTy*>(this);
  }
  const DeclsTy &decls() const {
    return *reinterpret_cast<const DeclsTy*>(this);
  }
};

/// A set of unresolved declarations 
template <unsigned InlineCapacity> class UnresolvedSet :
    public UnresolvedSetImpl {
  SmallVector<NamedDecl*, InlineCapacity> Decls;
};

  
} // namespace gong

#endif
