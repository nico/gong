// RUN: %gong_cc1 -verify %s

package p

// TypeName
type t int
type t int.int

// TypeLit, ArrayType
type t [4]int;
type t [+4]int;
type t [-4]int;
type t [4 int;  // expected-diag{{expected ']'}}

// TypeLit, StructType
type t struct {}
type t struct { foo int }
type t struct {
  anontype
  *anontype2
  anontype3.qual
  *anontype4.qual
  anontype "stringlit"
  foo bar
  bar, baz *quux
  bar2, baz2 *quux2 "stringlit"

  bar, baz []int
  *anon5000 []int  // expected-diag{{expected ';'}}
}
type t struct {
  4  // expected-diag{{expected identifier}}
}

// TypeLit, PointerType
type t *int
type t *4  // expected-diag{{expected type}}

// TypeLit, FunctionType
type t func()
type t func a()  // expected-diag{{expected '('}}
type t func() int

// TypeLit, InterfaceType
type t interface{}
type t interface{ foo }
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

// TypeLit, SliceType
type t []int
type t []  // expected-diag{{expected element type}}

// TypeLit, MapType
type t map[string]int
type t map;  // expected-diag{{expected '['}}
type t map[;  // expected-diag{{expected type}}
type t map[string  // expected-diag{{expected ']'}}
type t map[string]  // expected-diag{{expected type}}
type t map[string]map[foo]int

// TypeLit, ChannelType
type t chan int
type t chan<- int
type t <-chan int
type t <-int  // expected-diag{{expected 'chan'}}
type t <-chan<-chan int

// '(' Type ')'
type t (int)
type t (*int)
type t ((((int))))
type t (map[(string)](int))
type t (int  // expected-diag{{expected ')'}}
