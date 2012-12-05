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

// Function and method bodies can be omitted.
func a()
func a(int, int)
func (foo) a()
func (foo) a() int

func a {}  // expected-diag {{expected '('}}

func f() {
  func(func(){myprint("yo")})  // expected-diag {{expected ')'}} expected-note {{to match this '('}} expected-diag {{expected '{' or '('}}
}

func a()
