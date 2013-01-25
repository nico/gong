// RUN: %gong_cc1 -verify %s

package p

func }  // expected-diag {{expected identifier or '('}}

func (4) foo() {}  // expected-diag {{expected identifier}}

func (foo) foo() {}
func (foo bar) foo() {}
func (foo *bar) foo() {}

func (foo, bar) foo() {}  // expected-diag {{expected ')'}} expected-note {{to match this '('}}

func foo(foo bar) {}
func foo(foo, baz bar) {}
func foo(... bar) {}
func foo(bar...) {}  // expected-diag {{expected type}}
func foo(int ...int) {}  // valid!
func foo(foo... bar) {}
func foo(foo, baz... bar) {}
func foo(foo, baz... []int) {}
func foo(foo bar, baz quux) {}

func foo(foo bar) (foo, baz... bar) {}
func foo(foo bar) foo.bar {}

func foo(foo bar) foo . 4 {}  // expected-diag {{expected identifier}}
// .4 is lexed as numeric literal here:
func foo(foo bar) foo.4 {}  // expected-diag {{expected ';'}}

func foo(a int) (int) {}
func foo(a int) (int, int) {}
func foo(foo bar) (bar... foo.bar) {}

func foo(a.foo, b.foo) {}
func foo(a, b) {}
func foo(a.foo, b c) {}  // expected-diag {{unexpected type}}
func foo(a, b c) {}
func foo(a.foo, b.foo...) {}  // expected-diag {{unexpected '...'}}
func foo(a, b...) {}  // expected-diag {{expected type}}
func foo(a.foo, b ...c) {}  // expected-diag {{expected only identifiers before '...'}}
func foo(a, b ...c) {}

func foo(a, interface{}) {}
func foo(...interface{}, ...interface{}) {}
func foo(a, b ...interface{}) {}
func foo(int, ...interface{}) {}

// Function and method bodies can be omitted.
func a()
func a(int, int)
func a(p.int, int)
func a(int, p.int)

func a(int.foo, []int...)  // expected-diag {{unexpected '...'}}

func a(p1 int, p2 int)
func a(p1 p.int, p2 int)
func a(p1 int, p2 p.int)

func (foo) a()
func (foo) a() int

func a {}  // expected-diag {{missing parameter list}}
func (foo) a {}  // expected-diag {{missing parameter list}}

func f() {
  func(func(){myprint("yo")})  // expected-diag {{expected ')'}} expected-note {{to match this '('}} expected-diag {{expected '{' or '('}}
}

func a()
