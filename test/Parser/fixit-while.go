// RUN: %gong_cc1 -verify %s
// RUN: not %gong_cc1 -fdiagnostics-parseable-fixits %s 2>&1 | FileCheck %s

package p

func f() {
  i := 0
  // CHECK: fix-it:"{{.*}}":{[[@LINE+1]]:3-[[@LINE+1]]:8}:"for"
  while i < 2 {  // expected-diag {{'while' is spelled 'for' in Go}}
    i++
  }

  // CHECK: fix-it:"{{.*}}":{[[@LINE+1]]:3-[[@LINE+1]]:8}:"for"
  while {  // expected-diag {{'while' is spelled 'for' in Go}}
    break
  }

  // CHECK: fix-it:"{{.*}}":{[[@LINE+2]]:3-[[@LINE+2]]:8}:"for"
  // This one probably won't happen in practice.
  while ;; {  // expected-diag {{'while' is spelled 'for' in Go}}
    break
  }
}

func g() {
  // These shouldn't go down the 'for' parsing path!
  while := 0
  while + 4
}
