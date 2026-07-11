#ifndef MIO_TOKEN_HPP
#define MIO_TOKEN_HPP
#include<string>
#include<cstdio>
#include<cstdint>
#include<cstdlib>
#include<cstring>
#ifdef _WIN32
#define mio_strdup _strdup
#else
#define mio_strdup strdup
#endif
typedef enum{
    TOK_EOF=0,
    TOK_ERROR,
    TOK_IDENT,
    TOK_INT_LIT,
    TOK_FLOAT_LIT,
    TOK_STRING_LIT,
    TOK_CHAR_LIT,
    TOK_IMPORT,
    TOK_VAR,
    TOK_DEF,
    TOK_CONST,
    TOK_IF,
    TOK_ELSE,
    TOK_ELIF,
    TOK_WHILE,
    TOK_FOR,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_GOTO,
    TOK_RETURN,
    TOK_STRUCT,
    TOK_ENUM,
    TOK_UNION,
    TOK_STATIC,
    TOK_OPERATOR,
    TOK_TRUE,
    TOK_FALSE,
    TOK_THIS,
    TOK_MACRO,
    TOK_AT_IF,
    TOK_AT_ELIF,
    TOK_AT_ELSE,
    TOK_AT_END,
    TOK_I32,
    TOK_I64,
    TOK_U32,
    TOK_U64,
    TOK_F32,
    TOK_F64,
    TOK_BOOL,
    TOK_CHAR,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_ASSIGN,
    TOK_EQ,
    TOK_NEQ,
    TOK_LT,
    TOK_GT,
    TOK_LTE,
    TOK_GTE,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_BIT_AND,
    TOK_BIT_OR,
    TOK_BIT_XOR,
    TOK_BIT_NOT,
    TOK_LSHIFT,
    TOK_RSHIFT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_SEMICOLON,
    TOK_COLON,
    TOK_COMMA,
    TOK_DOT,
    TOK_ARROW,
}TokenKind;
typedef struct{
    TokenKind kind;
    std::string lexeme;
    int line;
    int col;
    union{
        int64_t int_val;
        double float_val;
        char char_val;
        bool bool_val;
    };
}Token;
const std::string tok_name(TokenKind kind);
Token*tok_new(TokenKind kind,const std::string&lexeme=std::string(),int line=0,int col=0);
void tok_free(Token*tok);
const std::string tok_name(TokenKind kind){
    switch(kind){
        case TOK_EOF: return "EOF";
        case TOK_ERROR: return "ERROR";
        case TOK_IDENT: return "IDENT";
        case TOK_INT_LIT: return "INT_LIT";
        case TOK_FLOAT_LIT: return "FLOAT_LIT";
        case TOK_STRING_LIT: return "STRING_LIT";
        case TOK_CHAR_LIT: return "CHAR_LIT";
        case TOK_IMPORT: return "import";
        case TOK_VAR: return "var";
        case TOK_DEF: return "def";
        case TOK_CONST: return "const";
        case TOK_IF: return "if";
        case TOK_ELSE: return "else";
        case TOK_ELIF: return "elif";
        case TOK_WHILE: return "while";
        case TOK_FOR: return "for";
        case TOK_BREAK: return "break";
        case TOK_CONTINUE: return "continue";
        case TOK_GOTO: return "goto";
        case TOK_RETURN: return "return";
        case TOK_STRUCT: return "struct";
        case TOK_ENUM: return "enum";
        case TOK_UNION: return "union";
        case TOK_STATIC: return "static";
        case TOK_OPERATOR: return "operator";
        case TOK_TRUE: return "true";
        case TOK_FALSE: return "false";
        case TOK_THIS: return "this";
        case TOK_MACRO: return "macro";
        case TOK_AT_IF: return "@if";
        case TOK_AT_ELIF: return "@elif";
        case TOK_AT_ELSE: return "@else";
        case TOK_AT_END: return "@end";
        case TOK_I32: return "i32";
        case TOK_I64: return "i64";
        case TOK_U32: return "u32";
        case TOK_U64: return "u64";
        case TOK_F32: return "f32";
        case TOK_F64: return "f64";
        case TOK_BOOL: return "bool";
        case TOK_CHAR: return "char";
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_SLASH: return "/";
        case TOK_PERCENT: return "%";
        case TOK_ASSIGN: return "=";
        case TOK_EQ: return "==";
        case TOK_NEQ: return "!=";
        case TOK_LT: return "<";
        case TOK_GT: return ">";
        case TOK_LTE: return "<=";
        case TOK_GTE: return ">=";
        case TOK_AND: return "&&";
        case TOK_OR: return "||";
        case TOK_NOT: return "!";
        case TOK_BIT_AND: return "&";
        case TOK_BIT_OR: return "|";
        case TOK_BIT_XOR: return "^";
        case TOK_BIT_NOT: return "~";
        case TOK_LSHIFT: return "<<";
        case TOK_RSHIFT: return ">>";
        case TOK_LPAREN: return "(";
        case TOK_RPAREN: return ")";
        case TOK_LBRACE: return "{";
        case TOK_RBRACE: return "}";
        case TOK_LBRACKET: return "[";
        case TOK_RBRACKET: return "]";
        case TOK_SEMICOLON: return ";";
        case TOK_COLON: return ":";
        case TOK_COMMA: return ",";
        case TOK_DOT: return ".";
        case TOK_ARROW: return "->";
        default: return "UNKNOWN";
    }
}
Token*tok_new(TokenKind kind,const std::string&lexeme,int line,int col){
    Token*t=new Token();
    if(!t){
        fprintf(stderr,"fatal: out of memory\n");
        exit(1);
    }
    t->kind=kind;
    if(!lexeme.empty()){
        t->lexeme=lexeme;
    }else{
        t->lexeme="";
    }
    t->line=line;
    t->col=col;
    return t;
}
void tok_free(Token*tok){
    if(!tok)return;
    delete tok;
}
#endif