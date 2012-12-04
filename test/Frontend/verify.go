// RUN: %gong_cc1 -verify %s 2>&1 | FileCheck %s

package p;

''  // expected-diag {{empty run literal}}

;  // expected-note {{random note}}

// CHECK: <unknown>:0: error: 'error' diagnostic expected but not seen: 
// CHECK-NEXT:   Line 5: empty run literal
// CHECK-NEXT: <unknown>:0: error: 'error' diagnostic seen but not expected: 
// CHECK-NEXT:   Line 5: empty rune literal
// CHECK-NEXT:   Line 5: expected ';'
// CHECK-NEXT: <unknown>:0: error: 'note' diagnostic expected but not seen: 
// CHECK-NEXT:   Line 7: random note
