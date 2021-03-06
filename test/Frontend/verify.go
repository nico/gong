// RUN: not %gong_cc1 -verify %s 2>&1 | FileCheck %s

package p;
func g() {
''  // expected-diag {{empty run literal}}

;  // expected-note {{random note}}
}
// CHECK: error: 'error' diagnostic expected but not seen: 
// CHECK-NEXT:   Line 5: empty run literal
// CHECK-NEXT: error: 'error' diagnostic seen but not expected: 
// CHECK-NEXT:   Line 5: empty rune literal
// CHECK-NEXT:   Line 5: expected statement
// CHECK-NEXT: error: 'note' diagnostic expected but not seen: 
// CHECK-NEXT:   Line 7: random note
