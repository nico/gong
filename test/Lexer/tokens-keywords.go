// RUN: %gong_cc1 -dump-tokens %s 2>&1 | FileCheck %s

// http://golang.org/ref/spec#Keywords

// CHECK: break 'break'
// CHECK: default 'default'
// CHECK: func 'func'
// CHECK: interface 'interface'
// CHECK: select 'select'
break        default      func         interface    select

// CHECK: case 'case'
// CHECK: defer 'defer'
// CHECK: go 'go'
// CHECK: map 'map'
// CHECK: struct 'struct'
case         defer        go           map          struct

// CHECK: chan 'chan'
// CHECK: else 'else'
// CHECK: goto 'goto'
// CHECK: package 'package'
// CHECK: switch 'switch'
chan         else         goto         package      switch

// CHECK: const 'const'
// CHECK: fallthrough 'fallthrough'
// CHECK: if 'if'
// CHECK: range 'range'
// CHECK: type 'type'
const        fallthrough  if           range        type

// CHECK: continue 'continue'
// CHECK: for 'for'
// CHECK: import 'import'
// CHECK: return 'return'
// CHECK: var 'var'
continue     for          import       return       var

