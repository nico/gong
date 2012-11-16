// RUN: %gong_cc1 %s -verify -fsyntax-only

// expected-diag@+1 {{invalid utf8 sequence}}
var √ = a;

// expected-diag@+1 {{invalid utf8 sequence}}
var a√ = a;

// expected-diag@+1 {{invalid utf8 sequence}}
var √ü√ = a;
