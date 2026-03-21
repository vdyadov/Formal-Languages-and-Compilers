%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yylineno;
extern char* yytext;
extern int yylex();

void yyerror(const char *s);

char all_errors[4096] = "";
%}

%define parse.error verbose

%token CONST_KW STRING_TYPE ID COLON EQUALS SEMICOLON STRING_LITERAL NUMBER ERROR_TOKEN

%%
program:
    | program line
    ;

line:
    declaration SEMICOLON
    | error SEMICOLON { yyerrok; }
    ;

declaration:
    CONST_KW ID COLON STRING_TYPE EQUALS STRING_LITERAL
    ;

%%

void yyerror(const char *s) {
    char buffer[512];
    sprintf(buffer, "Line: %d error: %s, unexpected %s\n", yylineno, s, yytext);
    strcat(all_errors, buffer);
}