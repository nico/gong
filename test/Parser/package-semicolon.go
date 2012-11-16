// RUN: %gong_cc1 -verify %s

package p var a = 4 // expected-diag {{expected ';' after package name}}
