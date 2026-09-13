#ifndef MIO_LEXER_HPP
#define MIO_LEXER_HPP
#include"token.hpp"
#include<cstdlib>
#include<cstring>
#include<cctype>
#include<cstdio>
#include<string>
#include<vector>
#include<unordered_map>
struct KeywordEntry{
	const char* keyword;
	TokenKind kind;
};
struct CondState{
	bool in_true_branch;
	bool skipping;
	bool has_else;
};
class Lexer{
	friend class Parser;
	friend class Compiler;
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
	void add_macro(const std::string& name,int value){
		macros[name]=value;
	}
	int get_macro_value(const std::string& name){
		auto it=macros.find(name);
		return it!=macros.end()?it->second:0;
	}
	void set_macros(const std::unordered_map<std::string,int>& src){macros=src;}
	const std::unordered_map<std::string,int>& get_macros()const{return macros;}
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
	std::unordered_map<std::string,int> macros;
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
				int at_line=t->line;
				int at_col=t->col;
				tok_free(t);
				auto tokens=tokenize_pp_expr();
				int idx=0;
				int result=eval_pp_or(tokens,idx);
				if(result){
					cond_stack.push_back({true,false,false});
				}else{
					cond_stack.push_back({false,true,false});
				}
				continue;
			}
			case IK_AT_ELIF:{
				int at_line=t->line;
				int at_col=t->col;
				tok_free(t);
				if(cond_stack.empty()){
					fprintf(stderr,"%s:%d:%d: error: stray '@elif' outside of conditional compilation block\n",filename.c_str(),at_line,at_col);
					continue;
				}
				auto& state=cond_stack.back();
				if(state.has_else){
					fprintf(stderr,"%s:%d:%d: error: '@elif' after '@else'\n",filename.c_str(),at_line,at_col);
					continue;
				}
				if(state.in_true_branch){
					state.skipping=true;
				}else{
					auto tokens=tokenize_pp_expr();
					int idx=0;
					int result=eval_pp_or(tokens,idx);
					if(result){
						state.in_true_branch=true;
						state.skipping=false;
					}
				}
				continue;
			}
			case IK_AT_ELSE:{
				int at_line=t->line;
				int at_col=t->col;
				tok_free(t);
				if(cond_stack.empty()){
					fprintf(stderr,"%s:%d:%d: error: stray '@else' outside of conditional compilation block\n",filename.c_str(),at_line,at_col);
					continue;
				}
				auto& state=cond_stack.back();
				if(state.has_else){
					fprintf(stderr,"%s:%d:%d: error: duplicate '@else'\n",filename.c_str(),at_line,at_col);
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
				int at_line=t->line;
				int at_col=t->col;
				tok_free(t);
				if(cond_stack.empty()){
					fprintf(stderr,"%s:%d:%d: error: stray '@end' outside of conditional compilation block\n",filename.c_str(),at_line,at_col);
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
				int ival=1;
				if(cur()!='\0'&&cur()!='\n'){
					int val_start=pos;
					if(isdigit(cur())){
						while(isdigit(cur()))advance();
					}else if(isalpha(cur())||cur()=='_'){
						while(isalnum(cur())||cur()=='_')advance();
						ival=1;
					}else{
						while(cur()!='\n'&&cur()!='\0')advance();
					}
					int val_len=pos-val_start;
					if(val_len>0&&isdigit(source[val_start])){
						char* val_buf=mioStrndup(source.c_str()+val_start,val_len);
						ival=atoi(val_buf);
						free(val_buf);
					}
				}
				if(macros.find(name)!=macros.end()){
					fprintf(stderr,"%s:%d:%d: error: macro '%s' is already defined\n",filename.c_str(),macro_line,macro_col,name.c_str());
				}
				add_macro(name,ival);
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
	enum PPKind{
		PPE_END,PPE_INT,PPE_ID,PPE_PLUS,PPE_MINUS,PPE_STAR,PPE_SLASH,
		PPE_EQ,PPE_NEQ,PPE_LT,PPE_GT,PPE_LTE,PPE_GTE,
		PPE_AND,PPE_OR,PPE_NOT,PPE_LPAREN,PPE_RPAREN
	};
	struct PPTok{PPKind kind;int val;std::string name;};
	std::vector<PPTok> tokenize_pp_expr(){
		std::vector<PPTok> tokens;
		while(true){
			while(cur()==' '||cur()=='\t')advance();
			char c=cur();
			if(c=='\n'||c=='\r'||c=='\0'){
				tokens.push_back({PPE_END,0,""});
				return tokens;
			}
			if(isalpha(c)||c=='_'){
				std::string name;
				while(isalnum(c=cur())||c=='_')name+=advance();
				tokens.push_back({PPE_ID,0,name});
			}else if(isdigit(c)){
				std::string num;
				while(isdigit(cur()))num+=advance();
				tokens.push_back({PPE_INT,atoi(num.c_str()),""});
			}else switch(advance()){
				case '+':tokens.push_back({PPE_PLUS,0,""});break;
				case '-':tokens.push_back({PPE_MINUS,0,""});break;
				case '*':tokens.push_back({PPE_STAR,0,""});break;
				case '/':if(cur()=='/'){while(cur()!='\n'&&cur()!='\0')advance();break;}tokens.push_back({PPE_SLASH,0,""});break;
				case '!':if(cur()=='='){advance();tokens.push_back({PPE_NEQ,0,""});}else tokens.push_back({PPE_NOT,0,""});break;
				case '=':if(cur()=='=')advance();tokens.push_back({PPE_EQ,0,""});break;
				case '<':if(cur()=='='){advance();tokens.push_back({PPE_LTE,0,""});}else tokens.push_back({PPE_LT,0,""});break;
				case '>':if(cur()=='='){advance();tokens.push_back({PPE_GTE,0,""});}else tokens.push_back({PPE_GT,0,""});break;
				case '&':if(cur()=='&'){advance();tokens.push_back({PPE_AND,0,""});}else tokens.push_back({PPE_END,0,""});break;
				case '|':if(cur()=='|'){advance();tokens.push_back({PPE_OR,0,""});}else tokens.push_back({PPE_END,0,""});break;
				case '(':tokens.push_back({PPE_LPAREN,0,""});break;
				case ')':tokens.push_back({PPE_RPAREN,0,""});break;
				default:tokens.push_back({PPE_END,0,""});break;
			}
		}
	}
	int eval_pp_or(const std::vector<PPTok>& ts,int& idx){
		int left=eval_pp_and(ts,idx);
		while(idx<(int)ts.size()&&ts[idx].kind==PPE_OR){idx++;int right=eval_pp_and(ts,idx);left=(left||right)?1:0;}
		return left;
	}
	int eval_pp_and(const std::vector<PPTok>& ts,int& idx){
		int left=eval_pp_cmp(ts,idx);
		while(idx<(int)ts.size()&&ts[idx].kind==PPE_AND){idx++;int right=eval_pp_cmp(ts,idx);left=(left&&right)?1:0;}
		return left;
	}
	int eval_pp_cmp(const std::vector<PPTok>& ts,int& idx){
		int left=eval_pp_add(ts,idx);
		while(idx<(int)ts.size()&&(ts[idx].kind==PPE_EQ||ts[idx].kind==PPE_NEQ||ts[idx].kind==PPE_LT||ts[idx].kind==PPE_GT||ts[idx].kind==PPE_LTE||ts[idx].kind==PPE_GTE)){
			PPKind op=ts[idx++].kind;
			int right=eval_pp_add(ts,idx);
			if(op==PPE_EQ)left=(left==right)?1:0;
			else if(op==PPE_NEQ)left=(left!=right)?1:0;
			else if(op==PPE_LT)left=(left<right)?1:0;
			else if(op==PPE_GT)left=(left>right)?1:0;
			else if(op==PPE_LTE)left=(left<=right)?1:0;
			else left=(left>=right)?1:0;
		}
		return left;
	}
	int eval_pp_add(const std::vector<PPTok>& ts,int& idx){
		int left=eval_pp_mul(ts,idx);
		while(idx<(int)ts.size()&&(ts[idx].kind==PPE_PLUS||ts[idx].kind==PPE_MINUS)){
			PPKind op=ts[idx++].kind;
			int right=eval_pp_mul(ts,idx);
			left=(op==PPE_PLUS)?left+right:left-right;
		}
		return left;
	}
	int eval_pp_mul(const std::vector<PPTok>& ts,int& idx){
		int left=eval_pp_unary(ts,idx);
		while(idx<(int)ts.size()&&(ts[idx].kind==PPE_STAR||ts[idx].kind==PPE_SLASH)){
			PPKind op=ts[idx++].kind;
			int right=eval_pp_unary(ts,idx);
			if(op==PPE_STAR)left=left*right;
			else left=(right==0)?0:left/right;
		}
		return left;
	}
	int eval_pp_unary(const std::vector<PPTok>& ts,int& idx){
		if(idx>=(int)ts.size())return 0;
		if(ts[idx].kind==PPE_NOT){idx++;return eval_pp_unary(ts,idx)?0:1;}
		if(ts[idx].kind==PPE_MINUS){idx++;return -eval_pp_unary(ts,idx);}
		if(ts[idx].kind==PPE_PLUS){idx++;return eval_pp_unary(ts,idx);}
		return eval_pp_primary(ts,idx);
	}
	int eval_pp_primary(const std::vector<PPTok>& ts,int& idx){
		if(idx>=(int)ts.size())return 0;
		if(ts[idx].kind==PPE_INT)return ts[idx++].val;
		if(ts[idx].kind==PPE_ID){std::string name=ts[idx++].name;return get_macro_value(name);}
		if(ts[idx].kind==PPE_LPAREN){idx++;int val=eval_pp_or(ts,idx);if(idx<(int)ts.size()&&ts[idx].kind==PPE_RPAREN)idx++;return val;}
		return 0;
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