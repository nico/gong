// RUN: %gong_cc1 -verify %s

package p

// ArrayType
//FIXME

// StructType
//FIXME

// PointerType
type t *int
type t *4  // expected-diag{{expected type}}

// FunctionType
type t func()
type t func a()  // expected-diag{{expected '('}}
type t func() int

// InterfaceType
//FIXME

// SliceType
type t []int
type t []  // expected-diag{{expected element type}}

// MapType
//FIXME

// ChannelType
//FIXME
