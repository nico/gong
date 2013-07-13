// RUN: %gong_cc1 -verify %s

package p

func f() {
  for i := 0; i < 10; ++i { }  // expected-diag {{Go has no prefix ++ operator, use postfix instead}}
  for i := 0; i < 10; --i { }  // expected-diag {{Go has no prefix -- operator, use postfix instead}}

  i := 0
  ++i  // expected-diag {{Go has no prefix ++ operator, use postfix instead}}

  --j, j = 4, 5  // expected-diag {{unexpected --}}

}
