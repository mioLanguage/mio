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
struct Macro{
	std::string name;
	std::string value;
};
struct CondState{
	bool in_true_branch;
	bool skipping;
	bool has_else;
};
class Lexer{
	friend class Parser;
public:
	Lexer(const std::string& source,const std::string& filename):source(source),filename(filename),pos(0),line(1),col(1),bol(0){
		current=preprocess_token();
		peekToken=preprocess_token();
	}
	~Lexer(){
		tok_free(current);
		tok_free(peekToken);
	}
	Token* next(){
		current=peekToken;
		peekToken=preprocess_token();
		return current;
	}
	Token* peek(){
		return peekToken;
	}
	void add_macro(const std::string& name,const std::string& value){
		for(auto& m:macros){
			if(m.name==name){
				m.value=value;
				return;
			}
		}
		macros.push_back({name,value});
	}
	bool is_macro_defined(const std::string& name){
		for(const auto& m:macros)
			if(m.name==name)return true;
		return false;
	}
	void copy_macros_from(const std::vector<Macro>& src){
		for(const auto& m:src)
			add_macro(m.name,m.value);
	}
	const std::vector<Macro>& get_macros()const{return macros;}
	bool is_template_instantiation(){
		int saved_pos=pos,saved_line=line,saved_col=col,saved_bol=bol;
		Token* saved_current=current;
		Token* saved_peek=peekToken;
		Token* t1=raw_token();
		bool result=false;
		if(t1->kind==TOK_DOLLAR){
			Token* t2=raw_token();
			result=(t2->kind==TOK_LPAREN);
		}
		pos=saved_pos;line=saved_line;col=saved_col;bol=saved_bol;
		current=saved_current;peekToken=saved_peek;
		return result;
	}
private:
	enum InternalKind:int{
		IK_AT_IF=1000,IK_AT_ELIF,IK_AT_ELSE,IK_AT_END,IK_AT_MACRO
	};
	#define match(c) (cur()==c?(advance(),true):false)
	#define cur() (source[pos])
	std::string source;
	std::string filename;
	int pos,line,col,bol;
	Token* current;
	Token* peekToken;
	std::vector<Macro> macros;
	std::vector<CondState> cond_stack;
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
				case '/':
					return;
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
		bool isHex=false;
		if(cur()=='0'&&(source[pos+1]=='x'||source[pos+1]=='X')){
			isHex=true;
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
			if(isHex)t->int_val=strtoll(text,nullptr,16);
			else t->int_val=atoll(text);
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
	void skip_cond_block(){
		int depth=0;
		while(true){
			Token* t=raw_token();
			if(t->kind==TOK_EOF){
				tok_free(t);
				return;
			}
			if((int)t->kind==IK_AT_IF){
				depth++;
				tok_free(t);
				continue;
			}
			if(depth==0&&((int)t->kind==IK_AT_ELIF||(int)t->kind==IK_AT_ELSE||(int)t->kind==IK_AT_END)){
				pos=t->line;line=t->line;col=t->col;bol=t->line;
				tok_free(t);
				return;
			}
			if((int)t->kind==IK_AT_END){
				depth--;
				tok_free(t);
				continue;
			}
			tok_free(t);
		}
	}
	void skip_cond_to_end(){
		int depth=0;
		while(true){
			Token* t=raw_token();
			if(t->kind==TOK_EOF){
				tok_free(t);
				return;
			}
			if((int)t->kind==IK_AT_IF){
				depth++;
				tok_free(t);
				continue;
			}
			if(depth==0&&(int)t->kind==IK_AT_END){
				tok_free(t);
				return;
			}
			if((int)t->kind==IK_AT_END){
				depth--;
			}
			tok_free(t);
		}
	}
	Token* preprocess_token(){
		while(true){
			Token* t=raw_token();
			switch((int)t->kind){
			case IK_AT_IF:{
				int line=t->line;
				int col=t->col;
				tok_free(t);
				bool negate=false;
				t=raw_token();
				if(t->kind==TOK_NOT){
					negate=true;
					tok_free(t);
					t=raw_token();
				}
				bool defined=false;
				if(t->kind==TOK_IDENT){
					defined=is_macro_defined(t->lexeme);
				}
				tok_free(t);
				bool result=negate?!defined:defined;
				if(result){
					cond_stack.push_back({true,false,false});
				}else{
					cond_stack.push_back({false,true,false});
				}
				continue;
			}
			case IK_AT_ELIF:{
				tok_free(t);
				if(cond_stack.empty()){
					fprintf(stderr,"%s:%d:%d: error: stray '@elif' outside of conditional compilation block\n",filename.c_str(),t->line,t->col);
					t=raw_token();
					if(t->kind==TOK_NOT){tok_free(t);t=raw_token();}
					tok_free(t);
					continue;
				}
				auto& state=cond_stack.back();
				if(state.has_else){
					fprintf(stderr,"%s:%d:%d: error: '@elif' after '@else'\n",filename.c_str(),t->line,t->col);
					t=raw_token();
					if(t->kind==TOK_NOT){tok_free(t);t=raw_token();}
					tok_free(t);
					continue;
				}
				if(state.in_true_branch){
					state.skipping=true;
				}else{
					bool negate=false;
					t=raw_token();
					if(t->kind==TOK_NOT){
						negate=true;
						tok_free(t);
						t=raw_token();
					}
					bool defined=false;
					if(t->kind==TOK_IDENT){
						defined=is_macro_defined(t->lexeme);
					}
					tok_free(t);
					bool result=negate?!defined:defined;
					if(result){
						state.in_true_branch=true;
						state.skipping=false;
					}
				}
				continue;
			}
			case IK_AT_ELSE:{
				tok_free(t);
				if(cond_stack.empty()){
					fprintf(stderr,"%s:%d:%d: error: stray '@else' outside of conditional compilation block\n",filename.c_str(),t->line,t->col);
					continue;
				}
				auto& state=cond_stack.back();
				if(state.has_else){
					fprintf(stderr,"%s:%d:%d: error: duplicate '@else'\n",filename.c_str(),t->line,t->col);
					continue;
				}
				state.has_else=true;
				if(state.in_true_branch){
					state.skipping=true;
				}else{
					state.in_true_branch=true;
					state.skipping=false;
				}
				continue;
			}
			case IK_AT_END:{
				tok_free(t);
				if(cond_stack.empty()){
					fprintf(stderr,"%s:%d:%d: error: stray '@end' outside of conditional compilation block\n",filename.c_str(),t->line,t->col);
					continue;
				}
				cond_stack.pop_back();
				continue;
			}
			case IK_AT_MACRO:{
				int at_line=t->line;
				int at_col=t->col;
				tok_free(t);
				while(cur()==' '||cur()=='\t'||cur()=='\r')advance();
				if(cur()=='\0'||cur()=='\n'){
					fprintf(stderr,"%s:%d:%d: error: expected macro name after '@macro'\n",filename.c_str(),at_line,at_col);
					continue;
				}
				int name_start=pos;
				if(!isalpha(cur())&&cur()!='_'){
					fprintf(stderr,"%s:%d:%d: error: expected macro name after '@macro'\n",filename.c_str(),at_line,at_col);
					continue;
				}
				while(isalnum(cur())||cur()=='_')advance();
				int name_len=pos-name_start;
				char* name_buf=mioStrndup(source.c_str()+name_start,name_len);
				std::string name(name_buf);
				free(name_buf);
				int macro_line=line;
				int macro_col=col;
				while(cur()==' '||cur()=='\t'||cur()=='\r')advance();
				std::string value="1";
				if(cur()!='\0'&&cur()!='\n'){
					int val_start=pos;
					if(isalpha(cur())||cur()=='_'){
						while(isalnum(cur())||cur()=='_')advance();
					}else if(isdigit(cur())){
						while(isdigit(cur())||cur()=='.')advance();
					}else if(cur()=='"'){
						advance();
						while(cur()!='"'&&cur()!='\0'&&cur()!='\n')advance();
						if(cur()=='"')advance();
					}
					int val_len=pos-val_start;
					if(val_len>0){
						char* val_buf=mioStrndup(source.c_str()+val_start,val_len);
						value=std::string(val_buf);
						free(val_buf);
					}
				}
				if(is_macro_defined(name)){
					fprintf(stderr,"%s:%d:%d: error: macro '%s' is already defined\n",filename.c_str(),macro_line,macro_col,name.c_str());
				}
				add_macro(name,value);
				continue;
			}
			case TOK_EOF:
				if(!cond_stack.empty()){
					fprintf(stderr,"%s:%d:%d: error: unclosed '@if' (expected '@end')\n",filename.c_str(),t->line,t->col);
					cond_stack.clear();
				}
				return t;
			default:
				if(!cond_stack.empty()&&cond_stack.back().skipping){
					tok_free(t);
					continue;
				}
				return t;
			}
		}
	}
	Token* raw_token(){
		while(true){
			skipWhitespace();
			if(cur()=='\0')return tok_new(TOK_EOF,std::string(),line,col);
			int lineNum=line;
			int colNum=col;
			char c=advance();
			if(isalpha(c)||c=='_'){
				pos--,col--;
				return ident();
			}
			if(isdigit(c)){
				pos--,col--;
				return number();
			}
			switch(c){
				case '"': pos--,col--;return stringLit();
				case '\'': pos--,col--;return charLit();
				case '+': 
					if(match('='))return tok_new(TOK_PLUS_ASSIGN,"+=",lineNum,colNum);
					return tok_new(TOK_PLUS,std::string(),lineNum,colNum);
				case '-':
					if(match('='))return tok_new(TOK_MINUS_ASSIGN,"-=",lineNum,colNum);
					if(match('>'))return tok_new(TOK_ARROW,std::string(),lineNum,colNum);
					return tok_new(TOK_MINUS,std::string(),lineNum,colNum);
				case '*': 
					if(match('='))return tok_new(TOK_STAR_ASSIGN,"*=",lineNum,colNum);
					return tok_new(TOK_STAR,std::string(),lineNum,colNum);
				case '/': 
					if(match('='))return tok_new(TOK_SLASH_ASSIGN,"/=",lineNum,colNum);
					return tok_new(TOK_SLASH,std::string(),lineNum,colNum);
				case '%': 
					if(match('='))return tok_new(TOK_PERCENT_ASSIGN,"%=",lineNum,colNum);
					return tok_new(TOK_PERCENT,std::string(),lineNum,colNum);
				case '(': return tok_new(TOK_LPAREN,std::string(),lineNum,colNum);
				case ')': return tok_new(TOK_RPAREN,std::string(),lineNum,colNum);
				case '{': return tok_new(TOK_LBRACE,std::string(),lineNum,colNum);
				case '}': return tok_new(TOK_RBRACE,std::string(),lineNum,colNum);
				case '[': return tok_new(TOK_LBRACKET,std::string(),lineNum,colNum);
				case ']': return tok_new(TOK_RBRACKET,std::string(),lineNum,colNum);
				case ';': return tok_new(TOK_SEMICOLON,std::string(),lineNum,colNum);
				case ':': 
					if(match(':'))return tok_new(TOK_DOUBLE_COLON,"::",lineNum,colNum);
					return tok_new(TOK_COLON,std::string(),lineNum,colNum);
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
					if(match('<')){
						if(match('='))return tok_new(TOK_LSHIFT_ASSIGN,"<<=",lineNum,colNum);
						return tok_new(TOK_LSHIFT,std::string(),lineNum,colNum);
					}
					return tok_new(TOK_LT,std::string(),lineNum,colNum);
				case '>':
					if(match('='))return tok_new(TOK_GTE,std::string(),lineNum,colNum);
					if(match('>')){
						if(match('='))return tok_new(TOK_RSHIFT_ASSIGN,">>=",lineNum,colNum);
						return tok_new(TOK_RSHIFT,std::string(),lineNum,colNum);
					}
					return tok_new(TOK_GT,std::string(),lineNum,colNum);
				case '&':
					if(match('&'))return tok_new(TOK_AND,std::string(),lineNum,colNum);
					if(match('='))return tok_new(TOK_AND_ASSIGN,"&=",lineNum,colNum);
					return tok_new(TOK_BIT_AND,std::string(),lineNum,colNum);
				case '|':
					if(match('|'))return tok_new(TOK_OR,std::string(),lineNum,colNum);
					if(match('='))return tok_new(TOK_OR_ASSIGN,"|=",lineNum,colNum);
					return tok_new(TOK_BIT_OR,std::string(),lineNum,colNum);
				case '^': 
					if(match('='))return tok_new(TOK_XOR_ASSIGN,"^=",lineNum,colNum);
					return tok_new(TOK_BIT_XOR,std::string(),lineNum,colNum);
				case '~': return tok_new(TOK_BIT_NOT,std::string(),lineNum,colNum);
				case '$': return tok_new(TOK_DOLLAR,std::string(),lineNum,colNum);
				case '@':{
					int start=pos;
					int startCol=col;
					while(isalnum(cur())||cur()=='_')
						advance();
					int len=pos-start;
					char* text=mioStrndup(source.c_str()+start,len);
					if(strcmp(text,"if")==0){free(text);return tok_new((TokenKind)IK_AT_IF,std::string(),lineNum,startCol);}
					if(strcmp(text,"elif")==0){free(text);return tok_new((TokenKind)IK_AT_ELIF,std::string(),lineNum,startCol);}
					if(strcmp(text,"else")==0){free(text);return tok_new((TokenKind)IK_AT_ELSE,std::string(),lineNum,startCol);}
					if(strcmp(text,"end")==0){free(text);return tok_new((TokenKind)IK_AT_END,std::string(),lineNum,startCol);}
					if(strcmp(text,"macro")==0){free(text);return tok_new((TokenKind)IK_AT_MACRO,std::string(),lineNum,startCol);}
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
	}
};
#undef cur
#undef match
const KeywordEntry Lexer::keywords[]={
	{"import",TOK_IMPORT},{"extern",TOK_EXTERN},{"var",TOK_VAR},
	{"const",TOK_CONST},{"if",TOK_IF},{"else",TOK_ELSE},
	{"while",TOK_WHILE},{"for",TOK_FOR},
	{"break",TOK_BREAK},{"continue",TOK_CONTINUE},{"goto",TOK_GOTO},
	{"return",TOK_RETURN},{"enum",TOK_ENUM},
	{"union",TOK_UNION},{"class",TOK_CLASS},{"namespace",TOK_NAMESPACE},
	{"public",TOK_PUBLIC},{"private",TOK_PRIVATE},{"protected",TOK_PROTECTED},
	{"virtual",TOK_VIRTUAL},{"override",TOK_OVERRIDE},
	{"static",TOK_STATIC},{"operator",TOK_OPERATOR},
	{"true",TOK_TRUE},{"false",TOK_FALSE},
	{"this",TOK_THIS},
	{"template",TOK_TEMPLATE},{"typename",TOK_TYPENAME},
	{"sizeof",TOK_SIZEOF},
	{"i8",TOK_I8},{"i16",TOK_I16},{"i32",TOK_I32},{"i64",TOK_I64},{"i128",TOK_I128},
	{"u8",TOK_U8},{"u16",TOK_U16},{"u32",TOK_U32},{"u64",TOK_U64},{"u128",TOK_U128},
	{"usize",TOK_USIZE},{"isize",TOK_ISIZE},
	{"f32",TOK_F32},{"f64",TOK_F64},
	{"bool",TOK_BOOL},{"char",TOK_CHAR},{"void",TOK_VOID},
	{NULL,TOK_EOF}
};
#endif