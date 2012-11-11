// RUN: %gong_cc1 -dump-tokens %s 2>&1 | FileCheck %s

// http://golang.org/ref/spec#Operators_and_Delimiters

// CHECK: plus '+'
// CHECK: amp '&'
// CHECK: plusequal '+='
// CHECK: ampequal '&='
// CHECK: ampamp '&&'
// CHECK: equalequal '=='
// CHECK: exclaimequal '!='
// CHECK: l_paren '('
// CHECK: r_paren ')'
+    &     +=    &=     &&    ==    !=    (    )

// CHECK: minus '-'
// CHECK: pipe '|'
// CHECK: minusequal '-='
// CHECK: pipeequal '|='
// CHECK: pipepipe '||'
// CHECK: less '<'
// CHECK: lessequal '<='
// CHECK: l_square '['
// CHECK: r_square ']'
-    |     -=    |=     ||    <     <=    [    ]

// CHECK: star '*'
// CHECK: caret '^'
// CHECK: starequal '*='
// CHECK: caretequal '^='
// CHECK: lessminus '<-'
// CHECK: greater '>'
// CHECK: greaterequal '>='
// CHECK: l_brace '{'
// CHECK: r_brace '}'
*    ^     *=    ^=     <-    >     >=    {    }

// CHECK: slash '/'
// CHECK: lessless '<<'
// CHECK: slashequal '/='
// CHECK: lesslessequal '<<='
// CHECK: plusplus '++'
// CHECK: equal '='
// CHECK: colonequal ':='
// CHECK: comma ','
// CHECK: semi ';'
/    <<    /=    <<=    ++    =     :=    ,    ;

// CHECK: percent '%'
// CHECK: greatergreater '>>'
// CHECK: percentequal '%='
// CHECK: greatergreaterequal '>>='
// CHECK: minusminus '--'
// CHECK: exclaim '!'
// CHECK: ellipsis '...'
// CHECK: period '.'
// CHECK: colon ':'
%    >>    %=    >>=    --    !     ...   .    :


// CHECK: ampcaret '&^'
// CHECK: ampcaretequal '&^='
     &^          &^=
