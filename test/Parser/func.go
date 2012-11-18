// RUN: %gong_cc1 -verify %s

package p

func }  // expected-diag {{expected identifier or '(' after 'func'}}

func (4) foo() {}  // expected-diag {{expected identifier}}

func (foo) foo() {}
func (foo bar) foo() {}
func (foo *bar) foo() {}

func (foo, bar) foo() {}  // expected-diag {{expected ')'}}

func a {}
