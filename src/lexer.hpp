#ifndef MIO_LEXER_HPP
#define MIO_LEXER_HPP
#include"token.hpp"
#include<cstdlib>
#include<cstring>
#include<cctype>
#include<cstdio>
#include<string>
#include<vector>
struct KeywordEntry{
	const char* keyword;
	TokenKind kind;
};
class Lexer{
	friend class Parser;
public:
	Lexer(const std::string& source,const std::string& filename):source(source),filename(filename),pos(0),line(1),col(1),bol(0){
		current=token();
		peekToken=token();
	}
	~Lexer(){
		tok_free(current);
		tok_free(peekToken);
	}
	Token* next(){
		current=peekToken;
		peekToken=token();
		return current;
	}
	Token* peek(){
		return peekToken;
	}
private:
	#define match(c) (cur()==c?(advance(),true):false)
	#define cur() (source[pos])
	std::string source;
	std::string filename;
	int pos,line,col,bol;
	Token* current;
	Token* peekToken;
	static char* mioStrndup(const char* s,int n){
		char* buf=(char*)malloc(n+1);
		if(!buf){
			fprintf(stderr,"fatal: out of memory\n");
			exit(1);
		}
		memcpy(buf,s,n);
		buf[n]='\0';
		return buf;
	}
	static const KeywordEntry keywords[];
	char advance(){
		char c=source[pos];
		if(c=='\0')return c;
		pos++;
		if(c=='\n'){
			line++;
			col=1;
			bol=pos;
		}else{
			col++;
		}
		return c;
	}
	void skipWhitespace(){
		while(true){
			char c=cur();
			switch(c){
				case ' ':
				case '\t':
				case '\r':
					advance();
					break;
				case '\n':
					advance();
					break;
				case '#':
					while(cur()!='\n'&&cur()!='\0')
						advance();
					break;
				default:
					return;
			}
		}
	}
	TokenKind keywordKind(const std::string& s){
		for(int i=0; keywords[i].keyword; i++){
			if(s==keywords[i].keyword)
				return keywords[i].kind;
		}
		return TOK_IDENT;
	}
	Token* ident(){
		int start=pos;
		int startCol=col;
		while(isalnum(cur())||cur()=='_')
			advance();
		int len=pos-start;
		std::string text=source.substr(start,len);
		TokenKind kind=keywordKind(text);
		if(kind!=TOK_IDENT){
			return tok_new(kind,std::string(),line,startCol);
		}
		char* textCopy=mioStrndup(text.c_str(),text.length());
		Token* t=tok_new(TOK_IDENT,textCopy,line,startCol);
		free(textCopy);
		return t;
	}
	Token* number(){
		int start=pos;
		int startCol=col;
		bool isFloat=false;
		if(cur()=='0'&&(cur()=='x'||cur()=='X')){
			advance();
			advance();
			while(isxdigit(cur()))advance();
		}else{
			while(isdigit(cur()))advance();
			if(cur()=='.'){
				isFloat=true;
				advance();
				while(isdigit(cur()))advance();
			}
		}
		int len=pos-start;
		char* text=mioStrndup(source.c_str()+start,len);
		Token* t;
		if(isFloat){
			t=tok_new(TOK_FLOAT_LIT,text,line,startCol);
			t->float_val=atof(text);
		}else{
			t=tok_new(TOK_INT_LIT,text,line,startCol);
			t->int_val=atoll(text);
		}
		free(text);
		return t;
	}
	Token* stringLit(){
		int startCol=col;
		advance();
		char* buffer=(char*)malloc(256);
		if(!buffer){
			fprintf(stderr,"fatal: out of memory\n");
			exit(1);
		}
		int capacity=256;
		int length=0;
		while(cur()!='"'&&cur()!='\0'){
			if(length>=capacity-1){
				capacity*=2;
				char* newBuffer=(char*)realloc(buffer,capacity);
				if(!newBuffer){
					free(buffer);
					fprintf(stderr,"fatal: out of memory\n");
					exit(1);
				}
				buffer=newBuffer;
			}
			if(cur()=='\\'){
				advance();
				char next=cur();
				switch(next){
					case 'n':  buffer[length++]='\n';break;
					case 't':  buffer[length++]='\t';break;
					case 'r':  buffer[length++]='\r';break;
					case '\\': buffer[length++]='\\';break;
					case '"':  buffer[length++]='"'; break;
					case '0':  buffer[length++]='\0';break;
					default:   buffer[length++]=next;break;
				}
				advance();
			}else{
				buffer[length++]=cur();
				advance();
			}
		}
		if(cur()=='"')advance();
		buffer[length]='\0';
		Token* t=tok_new(TOK_STRING_LIT,buffer,line,startCol);
		free(buffer);
		return t;
	}
	Token* charLit(){
		int startCol=col;
		advance();
		char c=cur();
		if(c=='\\'){
			advance();
			char next=cur();
			switch(next){
				case 'n':  c='\n';break;
				case 't':  c='\t';break;
				case 'r':  c='\r';break;
				case '\\': c='\\';break;
				case '\'': c='\'';break;
				case '0':  c='\0';break;
				default:   c=next;break;
			}
			advance();
		}else{
			advance();
		}
		if(cur()=='\'')advance();
		Token* t=tok_new(TOK_CHAR_LIT,std::string(),line,startCol);
		t->char_val=c;
		return t;
	}
	Token* token(){
		skipWhitespace();
		if(cur()=='\0')return tok_new(TOK_EOF,std::string(),line,col);
		int lineNum=line;
		int colNum=col;
		char c=advance();
		if(isalpha(c)||c=='_'){
			pos--;
			col--;
			return ident();
		}
		if(isdigit(c)){
			pos--;
			col--;
			return number();
		}
		switch(c){
			case '"': pos--;col--;return stringLit();
			case '\'': pos--;col--;return charLit();
			case '+': return tok_new(TOK_PLUS,std::string(),lineNum,colNum);
			case '-':
				if(match('>'))return tok_new(TOK_ARROW,std::string(),lineNum,colNum);
				return tok_new(TOK_MINUS,std::string(),lineNum,colNum);
			case '*': return tok_new(TOK_STAR,std::string(),lineNum,colNum);
			case '/': return tok_new(TOK_SLASH,std::string(),lineNum,colNum);
			case '%': return tok_new(TOK_PERCENT,std::string(),lineNum,colNum);
			case '(': return tok_new(TOK_LPAREN,std::string(),lineNum,colNum);
			case ')': return tok_new(TOK_RPAREN,std::string(),lineNum,colNum);
			case '{': return tok_new(TOK_LBRACE,std::string(),lineNum,colNum);
			case '}': return tok_new(TOK_RBRACE,std::string(),lineNum,colNum);
			case '[': return tok_new(TOK_LBRACKET,std::string(),lineNum,colNum);
			case ']': return tok_new(TOK_RBRACKET,std::string(),lineNum,colNum);
			case ';': return tok_new(TOK_SEMICOLON,std::string(),lineNum,colNum);
			case ':': return tok_new(TOK_COLON,std::string(),lineNum,colNum);
			case ',': return tok_new(TOK_COMMA,std::string(),lineNum,colNum);
			case '.': 
				if(match('.')&&match('.'))return tok_new(TOK_VARARG,"...",lineNum,colNum);
				return tok_new(TOK_DOT,std::string(),lineNum,colNum);
			case '=':
				if(match('='))return tok_new(TOK_EQ,std::string(),lineNum,colNum);
				return tok_new(TOK_ASSIGN,std::string(),lineNum,colNum);
			case '!':
				if(match('='))return tok_new(TOK_NEQ,std::string(),lineNum,colNum);
				return tok_new(TOK_NOT,std::string(),lineNum,colNum);
			case '<':
				if(match('='))return tok_new(TOK_LTE,std::string(),lineNum,colNum);
				if(match('<'))return tok_new(TOK_LSHIFT,std::string(),lineNum,colNum);
				return tok_new(TOK_LT,std::string(),lineNum,colNum);
			case '>':
				if(match('='))return tok_new(TOK_GTE,std::string(),lineNum,colNum);
				if(match('>'))return tok_new(TOK_RSHIFT,std::string(),lineNum,colNum);
				return tok_new(TOK_GT,std::string(),lineNum,colNum);
			case '&':
				if(match('&'))return tok_new(TOK_AND,std::string(),lineNum,colNum);
				return tok_new(TOK_BIT_AND,std::string(),lineNum,colNum);
			case '|':
				if(match('|'))return tok_new(TOK_OR,std::string(),lineNum,colNum);
				return tok_new(TOK_BIT_OR,std::string(),lineNum,colNum);
			case '^': return tok_new(TOK_BIT_XOR,std::string(),lineNum,colNum);
			case '~': return tok_new(TOK_BIT_NOT,std::string(),lineNum,colNum);
			case '@':{
				int start=pos;
				int startCol=col;
				while(isalnum(cur())||cur()=='_')
					advance();
				int len=pos-start;
				char* text=mioStrndup(source.c_str()+start,len);
				if(strcmp(text,"if")==0){free(text);return tok_new(TOK_AT_IF,std::string(),lineNum,startCol);}
				if(strcmp(text,"elif")==0){free(text);return tok_new(TOK_AT_ELIF,std::string(),lineNum,startCol);}
				if(strcmp(text,"else")==0){free(text);return tok_new(TOK_AT_ELSE,std::string(),lineNum,startCol);}
				if(strcmp(text,"end")==0){free(text);return tok_new(TOK_AT_END,std::string(),lineNum,startCol);}
				char buf[64];
				snprintf(buf,sizeof(buf),"unknown directive '@%s'",text);
				free(text);
				return tok_new(TOK_ERROR,buf,lineNum,colNum);
			}
			default:{
				char buf[64];
				snprintf(buf,sizeof(buf),"unexpected character '%c'",c);
				Token* t=tok_new(TOK_ERROR,buf,lineNum,colNum);
				return t;
			}
		}
	}
};
#undef cur
#undef match
const KeywordEntry Lexer::keywords[]={
	{"import",TOK_IMPORT},{"extern",TOK_EXTERN},{"var",TOK_VAR},{"def",TOK_DEF},
	{"const",TOK_CONST},{"if",TOK_IF},{"else",TOK_ELSE},
	{"elif",TOK_ELIF},{"while",TOK_WHILE},{"for",TOK_FOR},
	{"break",TOK_BREAK},{"continue",TOK_CONTINUE},{"goto",TOK_GOTO},
	{"return",TOK_RETURN},{"struct",TOK_STRUCT},{"enum",TOK_ENUM},
	{"union",TOK_UNION},{"static",TOK_STATIC},{"operator",TOK_OPERATOR},
	{"true",TOK_TRUE},{"false",TOK_FALSE},
	{"this",TOK_THIS},{"macro",TOK_MACRO},
	{"i8",TOK_I8},{"i16",TOK_I16},{"i32",TOK_I32},{"i64",TOK_I64},{"i128",TOK_I128},
	{"u8",TOK_U8},{"u16",TOK_U16},{"u32",TOK_U32},{"u64",TOK_U64},{"u128",TOK_U128},
	{"usize",TOK_USIZE},{"isize",TOK_ISIZE},
	{"f32",TOK_F32},{"f64",TOK_F64},
	{"bool",TOK_BOOL},{"char",TOK_CHAR},
	{NULL,TOK_EOF}
};
#endif