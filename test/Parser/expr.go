// RUN: %gong_cc1 -verify %s

package p

import "mypackage"

// These look a bit funny, but array types are a fairly obvious place that
// contains an expression, so these tests were useful when bootstrapping
// expressions before statement parsing was in place.
type t [4]int;
type t ['4']int;
type t [""]int;

type t [func() {}]int;
type t [func a() {}]int;  // expected-diag{{expected '('}}

type t [struct{}{}]int;
type t [[]int{}]int;
type t [[4]int{}]int;
type t [[...]int{}]int;
type t [map[string]int{}]int;


// suffix
type t ['4'.foo]int;
type t ['4'.([]int)]int;

type t ['4'[4]]int;
type t ['4'[:]]int;
type t ['4'[4:]]int;
type t ['4'[:4]]int;
type t ['4'[4:4]]int;

type t ["4"[:][0].(int)]int;

type t ['4'[]]int;  //expected-diag {{expected expression}}

type t ['4'.]int;  //expected-diag {{expected identifier or '('}}

type t ['4'.()]int;  //expected-diag {{expected type}}

// FIXME: The 2nd diag shouldn't be emitted.
type t ['4'.(int]int;  //expected-diag {{expected ')'}} expected-note {{to match this '('}} expected-diag {{expected ']'}} expected-note {{to match this '['}}


// The tests below depend on ExpressionStmt parsing.
type mytype struct{ foo int }
type mynestedtype struct{ foo struct{bar int} }
func (mytype) mymethod() { }
func f() {
  // unary ops
  +-!^*&<-4

  // binary ops
  4 || 4
  4 && 4
  4 == 4
  4 != 4
  4 < 4
  4 <= 4
  4 > 4
  4 >= 4
  4 + 4
  4 - 4
  4 | 4
  4 ^ 4
  4 * 4
  4 / 4
  4 % 4
  4 << 4
  4 >> 4
  4 & 4
  4 &^ 4

  // PrimaryExpr, Operand, Literal, BasicLit
  4
  4.5
  56.0i
  "asdf"
  `asdf`
  'a'

  // PrimaryExpr, Operand, Literal, CompositeLit
  struct{a int}{}
  [4]int{}
  [...]int{1, 2, 3}
  []int{1, 2, 3, 4+5*6, 4+5*6: 8}
  map[string]int{"foo": 2, "bar": 3}
  mytype{}
  mytype{foo: 4}
  mytype{4}
  mynestedtype{ { 4 } }
  mynestedtype{foo: { 4 } }
  mynestedtype{ {bar: 4 } }
  mynestedtype{foo: {bar: 4 } }
  mynestedtype{foo: {bar: 4 }  // expected-diag {{expected '}'}} expected-note {{to match this '{'}}

  // This has to work for unknown types too: They could be defined later at
  // file scope, or in a different file of the same package.
  unknowntype{}

  // PrimaryExpr, Operand, Literal, FunctionLit
  func(int) int{}

  // PrimaryExpr, Operand, OperandName
  foo
  importedpackage.foo

  // PrimaryExpr, Operand, MethodExpr
  mytype.mymethod
  //FIXME
  (*mytype).mymethod

  // PrimaryExpr, Operand, '(' Expression ')'
  (4 + 4)
  (+4)
  (4 + 4  // expected-diag {{expected ')'}} expected-note {{to match this '('}}
  ((+4))
  (+(4))
  (-4)
  (!4)
  (^4)
  (&4)
  ('a')
  (4 + foo)
  (foo + 4)
  ((4 + 4) * 5)
  (5 * (4 + 4))
  ((+4))
  (([...]int{1,2,3} + 4))
  (([]int))([...]int{1,2,3})
  (([3]int))([...]int{1,2,3})
  ((map[int]string))(map[int]string{})
  (interface{})(4).foo
  ((((interface{}))))(4).foo
  (interface{}(4))
  (interface{}(4)).foo()
  (chan int)(4).foo
  (chan<- int)(4)
  (chan int)(4).foo()
  (chan<- int)  // expected-diag {{expected '('}}
  (<-chan int)(4)
  (chan int).foo()  // expected-diag {{expected '('}}
  (func())(func(){ myprint("yo") })()
  func()(func(){myprint("yo")})()  // expected-diag{{expected ')'}} expected-note {{to match this '('}}
  (+int)(4)
  (*int)(&a)

  // PrimaryExpr, Conversion
  int(4.5)
  interface{}(4)
  chan int(4)
  chan int()  // FIXME: should-diag {{expected expression in conversion}}
  chan int(3, 4)  // expected-diag{{expected ')'}} expected-note {{to match this '('}}
  []int(4)
  [4]int(4)
  [...]int(4)  // expected-diag {{expected '{'}}
  struct{foo int}(4)
  map[string]int(4)
  // FIXME: this should diag, see
  // https://code.google.com/p/go/issues/detail?id=4605 :
  func(int)int(4)
  // This is a conversion (and a call) and ok:
  (func())(func(){})()
  // This goes down the Result parsing path and is _not_ a conversion and it
  // should diag. (FIXME: The diag could be a lot more helpful and suggest
  // adding parens around "func()".
  // See https://code.google.com/p/go/issues/detail?id=4478 and
  // https://code.google.com/p/go/issues/detail?id=4109
  func()(func(){})()  // expected-diag {{expected ')'}} expected-note {{to match this '('}}

  // PrimaryExpr, BuiltinCall
  make([]int, 6)
  println(4)
  println(4  // expected-diag {{expected ')'}} expected-note {{to match this '('}}
  s3 := append(s2, s0...)

  // FIXME: See the comment in ParsePrimaryExprTail for why this is disabled:
  //println  // should-diag {{expected '(', builtins can only be used in calls}}
  {
    len := 4
    a := len + a
  }

  // PrimaryExpr Selector
  "asdf".foo
  mypackage.foo

  // PrimaryExpr Index
  "asdf"[4]

  "asdf"[4  // expected-diag {{expected ']' or ':'}}

  "asdf"[]  // expected-diag {{expected expression or ':'}}

  // PrimaryExpr Slice
  "asdf"[:]
  "asdf"[4:]
  "asdf"[:4]
  "asdf"[3:4]

  "asdf"[:  // expected-note {{to match this '['}}
  4  // expected-diag {{expected ']'}}

  // PrimaryExpr TypeAssertion
  foo.(int)
  foo.(type)  // expected-diag {{unexpected '.(type)'}}

  // PrimaryExpr Call
  foo()
  foo(;  // expected-diag {{expected ')'}} expected-note {{to match this '('}}
  foo(,)  // FIXME: should-diag
  foo(4,)
  foo(...)  // FIXME: should-diag
  foo(4...)  // Note: This is a floating point number.
  foo('a'...)
  foo(4...,)  // Note: This is a floating point number.
  foo('a'...,)
  foo(4,...)  // FIXME: should-diag
  foo(...,4) // expected-diag {{expected ')'}} expected-note {{to match this '('}}
  foo(4 + 5)
  foo(4 + 5, 6 - 7)
  func(int)int{}()
}
