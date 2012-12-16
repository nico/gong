// RUN: %gong_cc1 %s -verify -fsyntax-only
package p
// FIXME: These diags are pretty bad.
vara= ' '  // expected-diag {{expected identifier or '('}}
varb= ' '  // expected-diag {{expected identifier or '('}}
