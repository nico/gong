// RUN: %gong_cc1 -dump-tokens %s 2>&1 | FileCheck %s

// http://golang.org/ref/spec#Semicolons

// CHECK: identifier 'foo'
// CHECK-NEXT: semi ''
// Make sure only one semi is emitted
// CHECK-NOT: semi ''
foo

// CHECK: numeric_literal '4'
// CHECK-NEXT: semi ''
4

// CHECK: numeric_literal '4.0'
// CHECK-NEXT: semi ''
4.0

// CHECK: numeric_literal '4i'
// CHECK-NEXT: semi ''
4i

// CHECK: rune_literal ''r''
// CHECK-NEXT: semi ''
'r'

// CHECK: string_literal '"s"'
// CHECK-NEXT: semi ''
"s"

// CHECK: break 'break'
// CHECK-NEXT: semi ''
break

// CHECK: continue 'continue'
// CHECK-NEXT: semi ''
continue

// CHECK: fallthrough 'fallthrough'
// CHECK-NEXT: semi ''
fallthrough

// CHECK: return 'return'
// CHECK-NEXT: semi ''
return

// CHECK: plusplus '++'
// CHECK-NEXT: semi ''
++

// CHECK: minusminus '--'
// CHECK-NEXT: semi ''
--

// CHECK: r_paren ')'
// CHECK-NEXT: semi ''
)

// CHECK: r_square ']'
// CHECK-NEXT: semi ''
]

// CHECK: r_brace '}'
// CHECK-NEXT: semi ''
}

// CHECK: break 'break'
// CHECK-NEXT: continue 'continue'
// CHECK-NEXT: semi ''
break continue

// CHECK: break 'break'
// CHECK-NEXT: semi '
break // end-of-line comment
// CHECK: break 'break'
// CHECK-NEXT: continue 'continue'
// CHECK-NEXT: semi '
// CHECK: return 'return'
break continue /*
*/ return

// No semicolon should be inserted after other keywords.
// CHECK: case 'case'
// CHECK-NEXT: chan 'chan'
// CHECK-NEXT: const 'const'
// CHECK-NEXT: default 'default'
// CHECK-NEXT: defer 'defer'
// CHECK-NEXT: else 'else'
// CHECK-NEXT: for 'for'
// CHECK-NEXT: func 'func'
// CHECK-NEXT: go 'go'
// CHECK-NEXT: goto 'goto'
// CHECK-NEXT: if 'if'
// CHECK-NEXT: import 'import'
// CHECK-NEXT: interface 'interface'
// CHECK-NEXT: map 'map'
// CHECK-NEXT: package 'package'
// CHECK-NEXT: range 'range'
// CHECK-NEXT: select 'select'
// CHECK-NEXT: struct 'struct'
// CHECK-NEXT: switch 'switch'
// CHECK-NEXT: type 'type'
// CHECK-NEXT: var 'var'
// CHECK-NEXT: case 'case'
case
chan
const
default
defer
else
for
func
go
goto
if
import
interface
map
package
range
select
struct
switch
type
var
case
