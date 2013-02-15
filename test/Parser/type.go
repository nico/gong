// RUN: %gong_cc1 -verify %s

package p

type ( t int )
type (
  t int
  t [4]int
)

// TypeName
type t int
type t int.int

// TypeLit, ArrayType
type t [4]int;
type t [+4]int;
type t [-4]int;
type t [4 int;  // expected-diag{{expected ']'}} expected-note{{to match this '['}}

// TypeLit, StructType
type t struct {}
type t struct { foo int }
type t struct {
  anontype
  *anontype2
  anontype3.qual
  *anontype4.qual
  anontype "stringlit"
  anontype `rawstringlit`
  *anontype2 "stringlit"
  anontype3.qual "stringlit"
  *anontype4.qual "stringlit"
  foo bar
  bar, baz *quux
  bar2, baz2 *quux2 "stringlit"

  bar, baz []int
  bar, baz, []int  // expected-diag{{expected identifier}}
  *anon5000 []int  // expected-diag{{expected ';' or '}'}}
}
type t struct {
  4  // expected-diag{{expected identifier, '*', or '}' in 'struct'}}
  foo int 4  // expected-diag{{expected ';' or '}'}}
}
type t struct {
  4;  // expected-diag{{expected identifier}}
  *;  // expected-diag{{expected identifier}}
  foo int 4  // expected-diag{{expected ';' or '}'}}
}
type t struct {
  foo, bar  // expected-diag{{expected type}}
  bar int 4  // expected-diag{{expected ';' or '}'}}
}
type t struct {
  foo, bar;  // expected-diag{{expected type}}
  bar int 4  // expected-diag{{expected ';' or '}'}}
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
type t interface { foo }
type t interface {
  4 // expected-diag{{expected identifier or '}'}}
}
type t interface {
  foo
  foo.bar
  baz(foo, bar quux)
  foo []int  // expected-diag{{expected ';' or '}'}}
}
type t interface{
  foo.
}  // expected-diag{{expected identifier}}
type t interface {
  4  // expected-diag{{expected identifier}}
  foo 4  // expected-diag{{expected ';' or '}'}}
}
type t interface {
  foo. 4  // expected-diag{{expected identifier}}
  bar 4  // expected-diag{{expected ';' or '}'}}
}

// TypeLit, SliceType
type t []int
type t []  // expected-diag{{expected element type}}

// TypeLit, MapType
type t map[string]int
type t map;  // expected-diag{{expected '['}}
type t map[;  // expected-diag{{expected type}}
type t map[string  // expected-diag{{expected ']'}}  expected-note{{to match this '['}}
type t map[string]  // expected-diag{{expected type}}
type t map[string]map[foo]int

// TypeLit, ChannelType
type t chan int
type t chan<- int
type t <-chan int
type t <-int  // expected-diag{{expected 'chan'}}
type t <-;  // expected-diag{{expected 'chan'}}
type t <-chan<-chan int
func f() { var t <- }  // expected-diag{{expected 'chan'}}
func f() { var t (<-) }  // expected-diag{{expected 'chan'}}
func f() { var t <-chan }  // expected-diag{{expected element type}}
func f() { var t (<-chan) }  // expected-diag{{expected element type}}

// '(' Type ')'
type t (int)
type t (*int)
type t ((((int))))
type t (map[(string)](int))
type t (int  // expected-diag{{expected ')'}} expected-note{{to match this '('}}
