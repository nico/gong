// RUN: %gong_cc1 -dump-tokens %s 2>&1 | FileCheck %s

// http://golang.org/ref/spec#Rune_literals
// CHECK: rune_literal ''a''
'a'

// FIXME: utf8
//'ä'
//'本'
// CHECK: rune_literal ''\t''
'\t'
// CHECK: rune_literal ''\000''
'\000'
// CHECK: rune_literal ''\007''
'\007'
// CHECK: rune_literal ''\377''
'\377'
// CHECK: rune_literal ''\x07''
'\x07'
// CHECK: rune_literal ''\xff''
'\xff'
// CHECK: rune_literal ''\u12e4''
'\u12e4'
// CHECK: rune_literal ''\U00101234''
'\U00101234'

// FIXME: test \a \b \f \n \r \t \v \\ \'

// FIXME: diags
//'aa'         // illegal: too many characters
//'\xa'        // illegal: too few hexadecimal digits
//'\0'         // illegal: too few octal digits
//'\uDFFF'     // illegal: surrogate half
//'\U00110000' // illegal: invalid Unicode code point
// '\"'

// http://golang.org/ref/spec#String_literals
// FIXME: raw strings
//`abc`  // same as "abc"
//`\n
//\n`    // same as "\\n\n\\n"
// CHECK: string_literal '"\n"'
"\n"
// CHECK: string_literal '""'
""
// CHECK: string_literal '"Hello, world!\n"'
"Hello, world!\n"
//"日本語"
//"\u65e5本\U00008a9e"
// CHECK: string_literal '"\xff\u00FF"'
"\xff\u00FF"

// FIXME: diags
//"\uD800"       // illegal: surrogate half
//"\U00110000"   // illegal: invalid Unicode code point

// FIXME: test \a \b \f \n \r \t \v \\ \"
