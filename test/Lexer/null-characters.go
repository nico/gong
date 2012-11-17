// RUN: %gong_cc1 %s -verify -fsyntax-only

// Test null characters in various places.

package p

var r = ' ';  // expected-diag{{null character(s) in rune literal}}

var s = " ";  // expected-diag{{null character(s) in string literal}}

var t = ` `;  // expected-diag{{null character(s) in string literal}}

   // expected-diag{{null character(s) in file}}
