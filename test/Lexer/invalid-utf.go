// RUN: %gong_cc1 %s -verify -fsyntax-only
// This file contains invalid utf sequences. Edit it in a hex editor.
package p;

// expected-diag@+1 {{invalid utf8 sequence}}
var √ = a;

// expected-diag@+1 {{invalid utf8 sequence}}
var a√ = a;

// expected-diag@+1 {{invalid utf8 sequence}}
var √ü√ = a;
