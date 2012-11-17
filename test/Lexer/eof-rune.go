// RUN: %gong_cc1 %s -verify -fsyntax-only
// vim: set binary noeol:

// This file intentionally ends without a \n on the last line.  Make sure your
// editor doesn't add one.

package p

// expected-diag@+1{{missing terminating ' character}} expected-diag@+1{{expected ';'}}
var a = '\