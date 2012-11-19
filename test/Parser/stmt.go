// RUN: %gong_cc1 -verify %s

package p

func f() {
  break
  break foo
  continue
  continue foo
  goto foo
  goto 4  // expected-diag{{expected identifier}}
  fallthrough
}
