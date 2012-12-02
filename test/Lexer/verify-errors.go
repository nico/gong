// RUN: %gong_cc1 -verify %s

package p;

var a = '';  // expected-diag {{empty rune literal}} expected-diag {{expected ';'}}
