// RUN: %gong_cc1 -verify %s

package p

func }  // expected-diag {{expected identifier or '(' after 'func'}}

func (4) foo() {}  // expected-diag {{expected identifier}}

func (foo) foo() {}
func (foo bar) foo() {}
func (foo *bar) foo() {}

func (foo, bar) foo() {}  // expected-diag {{expected ')'}}

func foo(foo bar) {}
func foo(foo, baz bar) {}
func foo(... bar) {}
func foo(foo... bar) {}
func foo(foo, baz... bar) {}
func foo(foo, baz... []int) {}
func foo(foo bar, baz quux) {}

func foo(foo bar) (foo, baz... bar) {}
func foo(foo bar) foo.bar {}

func foo(foo bar) foo . 4 {}  // expected-diag {{expected identifier}}
// .4 is lexed as numeric literal here:
func foo(foo bar) foo.4 {}  // expected-diag {{expected ';'}}


func foo(foo bar) (bar... foo.bar) {}

func a {}  // expected-diag {{expected '('}}

// FIXME: `func a()` without {} is valid, test once signature parsing is done
