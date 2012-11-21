// RUN: %gong_cc1 -verify %s

package p

// ArrayType
type t [4]int;
type t [+4]int;
type t [-4]int;
type t [4 int;  // expected-diag{{expected ']'}}

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
type t interface{}
type t interface{
  4 // expected-diag{{expected identifier}}
}
type t interface{
  foo
  foo.bar
  baz(foo, bar quux)
}
// FIXME
//type t interface{
  //foo.  // exected-diag{{expected identifier}}
//}

// SliceType
type t []int
type t []  // expected-diag{{expected element type}}

// MapType
type t map[string]int
type t map;  // expected-diag{{expected '['}}
type t map[;  // expected-diag{{expected type}}
type t map[string  // expected-diag{{expected ']'}}
type t map[string]  // expected-diag{{expected type}}
type t map[string]map[foo]int

// ChannelType
type t chan int
type t chan<- int
type t <-chan int
type t <-int  // expected-diag{{expected 'chan'}}
type t <-chan<-chan int
