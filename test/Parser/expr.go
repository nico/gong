// RUN: %gong_cc1 -verify %s

package p

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
type t ['4'.(int]int;  //expected-diag {{expected ')'}} expected-diag{{expected ']'}}


// The tests below depend on ExpressionStmt parsing.
type mytype struct{ foo int }
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
  //FIXME:
  //[...]int{1, 2, 3}
  //[]int{1, 2, 3}
  //map[string]int{"foo": 2, "bar": 3}
  mytype{}
  //mytype{foo: 4}; mytype{4}

  // PrimaryExpr, Operand, Literal, FunctionLit
  //FIXME
  //func(int) int{}

  // PrimaryExpr, Operand, OperandName
  foo
  importedpackage.foo

  // PrimaryExpr, Operand, MethodExpr
  mytype.mymethod
  //FIXME
  //(*mytype).mymethod

  // PrimaryExpr, Operand, '(' Expression ')'
  //FIXME
  //(4 + 4)

  // PrimaryExpr, Conversion
  //FIXME: Conversions for named types:
  int(4.5)
  // FIXME: *type(expr) vs (*type)(expr)
  // FIXME: <-chan int(expr) vs (<-chan int)(expr)
  interface{}(4)
  chan int(4)
  chan int()  // FIXME: should-diag {{expected expression}}
  chan int(3, 4)  // expected-diag{{expected ')'}}
  func(int)int(4)
  []int(4)
  [4]int(4)
  [...]int(4)  // expected-diag {{expected '{'}}
  struct{foo int}(4)
  map[string]int(4)

  // PrimaryExpr, BuiltinCall
  //FIXME
  //make([]int, 6)
  //println(4)

  // PrimaryExpr Selector
  "asdf".foo

  // PrimaryExpr Index
  "asdf"[4]

  // PrimaryExpr Slice
  "asdf"[:]
  "asdf"[4:]
  "asdf"[:4]
  "asdf"[3:4]

  // PrimaryExpr TypeAssertion
  foo.(int)

  // PrimaryExpr Call
  foo()
  foo(,)  // FIXME: should-diag
  foo(4,)
  foo(...)  // FIXME: should-diag
  foo(4...)
  foo(4...,)
  foo(4,...)  // FIXME: should-diag
  //foo(...,4)  // FIXME: should-diag
  foo(4 + 5)
  foo(4 + 5, 6 - 7)
}
