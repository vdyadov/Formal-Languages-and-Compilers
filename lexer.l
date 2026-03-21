%{
#include "parser.tab.h"
#include <string.h>
%}

%option yylineno

%%
"const"|"Const"     { return CONST_KW; }
"string"            { return STRING_TYPE; }
[a-zA-Z_][a-zA-Z0-9_]* { return ID; }
":"                 { return COLON; }
"="                 { return EQUALS; }
";"                 { return SEMICOLON; }
\'[^\']*\'          { return STRING_LITERAL; }
[0-9]+              { return NUMBER; }
[ \t\r]+            {  }
\n                  {  }
.                   { return ERROR_TOKEN; }
%%

int yywrap() { return 1; }