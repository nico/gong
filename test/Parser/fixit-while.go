// RUN: %gong_cc1 -verify %s

package p

func f() {
  i := 0
  while i < 2 {  // expected-diag {{'while' is spelled 'for' in Go}}
    i++
  }

  while {  // expected-diag {{'while' is spelled 'for' in Go}}
    break
  }

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
