// RUN: %gong_cc1 -verify %s -sema
package p

func f0(a) {}  // expected-diag {{use of undeclared identifier 'a'}}
func f1(int) {}

func f2(b a) {}  // expected-diag {{use of undeclared identifier 'a'}}
func f3(b int) {}

func f4(int, a) {}  // expected-diag {{use of undeclared identifier 'a'}}
func f5(int, int) {}

func f6(int, b a) {}  // expected-diag {{use of undeclared identifier 'a'}}
func f7(int, b int) {}
