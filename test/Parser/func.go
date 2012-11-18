// RUN: %gong_cc1 -verify %s

package p

func }  // expected-diag {{expected identifier or '(' after 'func'}}

func a {}
