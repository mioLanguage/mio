#ifndef MIO_PARSER_HPP
#define MIO_PARSER_HPP
#include"ast.hpp"
#include"lexer.hpp"
#include"token.hpp"
#include<string>
#include<vector>
#include<unordered_set>
#include<unordered_map>
#include<cstdio>
static int g_error_count=0;
#include<cstdlib>
#include<cstring>
extern FilenamePool g_filename_pool;
struct Macro{
	std::string name;
	std::string value;
};
class Parser{
public:
	Lexer* lexer;
	Token* cur;
	Token* peek;
	std::string filename;
	std::vector<std::string> include_paths;
	std::vector<std::string> imported_files;
	std::vector<Macro> macros;
	std::unordered_set<std::string> class_names;
	std::unordered_map<std::string,std::unordered_map<std::string,bool>> class_virtual_methods;
	std::unordered_map<std::string,std::string> class_base_map;
	std::unordered_map<std::string,std::unordered_set<std::string>> class_method_names;
	Parser(Lexer* l,const std::string& f,const std::vector<std::string>& paths):lexer(l),filename(f),include_paths(paths){
		cur=lexer->current;
		peek=lexer->peek();
	}
	~Parser(){}
	const std::string* fn(){return g_filename_pool.get(filename);}
	AstNode* parse(){
		auto* program=new AstNode(AstNodeKind::PROGRAM,0,0,g_filename_pool.get(filename));
		while(!check(TOK_EOF)){
			if(check(TOK_AT_END)){advance();continue;}
			auto* decl=parse_decl();
			if(decl)program->program.nodes.push_back(decl);
		}
		return program;
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
private:
	void error(const std::string& msg){
		fprintf(stderr,"%s:%d:%d: error: %s\n",filename.c_str(),cur->line,cur->col,msg.c_str());
		g_error_count++;
	}
	void error_expected(const std::string& expected){
		char buf[256];
		if(cur->kind==TOK_ERROR)
			snprintf(buf,sizeof(buf),"%s",cur->lexeme.c_str());
		else
			snprintf(buf,sizeof(buf),"expected %s, got '%s'",expected.c_str(),tok_name(cur->kind).c_str());
		error(buf);
	}
	void advance(){
		cur=lexer->next();
		peek=lexer->peek();
	}
	bool check(TokenKind kind)const{
		return cur->kind==kind;
	}
	bool match(TokenKind kind){
		if(check(kind)){
			advance();
			return true;
		}
		return false;
	}
	bool expect(TokenKind kind){
		if(match(kind))return true;
		error_expected(tok_name(kind));
		return false;
	}
	std::string normalize_path(const std::string& path){
		std::vector<std::string> parts;
		size_t start=0;
		bool is_absolute=false;
		std::string drive_prefix;
		if(path.length()>1&&path[1]==':'){
			is_absolute=true;
			drive_prefix=path.substr(0,2);
			start=2;
			if(start<path.length()&&(path[start]=='/'||path[start]=='\\'))start++;
		}else if(path.length()>0&&(path[0]=='/'||path[0]=='\\')){
			is_absolute=true;
		}
		while(start<path.length()){
			size_t sep=path.find_first_of("/\\",start);
			if(sep==std::string::npos)sep=path.length();
			std::string part=path.substr(start,sep-start);
			if(part.empty()||part=="."){
			}else if(part==".."){
				if(!parts.empty())parts.pop_back();
			}else{
				parts.push_back(part);
			}
			start=sep+1;
		}
		std::string result=drive_prefix;
		if(is_absolute){
			if(!drive_prefix.empty())result+="\\";
			else result+="/";
		}
		for(size_t i=0;i<parts.size();i++){
			if(i>0)result+="/";
			result+=parts[i];
		}
		if(result.empty())return ".";
#ifdef _WIN32
		for(size_t i=0;i<result.length();i++){
			if(result[i]=='/')result[i]='\\';
		}
#endif
		return result;
	}
	std::string join_path(const std::string& dir,const std::string& file){
		return normalize_path(dir+"/"+file);
	}
	bool file_exists(const std::string& path){
		FILE* f=fopen(path.c_str(),"rb");
		if(f){fclose(f);return true;}
		return false;
	}
	std::string read_file_content(const std::string& path){
		FILE* f=fopen(path.c_str(),"rb");
		if(!f)return "";
		fseek(f,0,SEEK_END);
		long size=ftell(f);
		fseek(f,0,SEEK_SET);
		std::string buf(size,'\0');
		size_t read_bytes=fread(&buf[0],1,size,f);
		fclose(f);
		if(read_bytes!=(size_t)size)return "";
		return buf;
	}
	bool is_imported(const std::string& path){
		for(const auto& p:imported_files)
			if(p==path)return true;
		return false;
	}
	void mark_imported(const std::string& path){
		imported_files.push_back(path);
	}
	std::string resolve_mio_file(const std::string& import_path){
		size_t last_sep=filename.find_last_of("/\\");
		std::string dir=(last_sep==std::string::npos)?"." : filename.substr(0,last_sep);
		if(!dir.empty()){
			std::string candidate=join_path(dir,import_path);
			if(file_exists(candidate))return candidate;
		}
		for(const auto& path:include_paths){
			std::string candidate=join_path(path,import_path);
			if(file_exists(candidate))return candidate;
		}
		return "";
	}
	void add_import_to_block(AstNode* block,AstNode* node){
		if(!node)return;
		if(node->kind==AstNodeKind::BLOCK){
			for(auto* stmt:node->block.stmts)
				block->block.stmts.push_back(stmt);
			node->block.stmts.clear();
			delete node;
		}else{
			block->block.stmts.push_back(node);
		}
	}
	AstNode* parse_single_import(int line,int col){
		if(cur->kind==TOK_STRING_LIT){
			std::string path=cur->lexeme;
			advance();
			std::string resolved=resolve_mio_file(path.length()>4&&path.substr(path.length()-4)==".mio" ? path : path+".mio");
			if(!resolved.empty()){
				return parse_import_file(resolved,path,line,col);
			}
			char buf[512];
			snprintf(buf,sizeof(buf),"imported file '%s' not found",path.c_str());
			error(buf);
			return nullptr;
		}else{
			std::string path=parse_import_path();
			if(!path.empty()){
				return ast_new_namespace_import(path,line,col,fn());
			}
			return nullptr;
		}
	}
	AstNode* parse_import_file(const std::string& resolved,const std::string& display_path,int line,int col){
		if(is_imported(resolved))return nullptr;
		mark_imported(resolved);
		std::string source=read_file_content(resolved);
		if(source.empty()){
			char buf[512];
			snprintf(buf,sizeof(buf),"cannot read imported file '%s'",display_path.c_str());
			error(buf);
			return nullptr;
		}
		auto* old_lexer=lexer;
		auto* old_cur=cur;
		auto* old_peek=peek;
		std::string old_filename=filename;
		std::string norm_resolved=normalize_path(resolved);
		auto* new_lexer=new Lexer(source,norm_resolved);
		lexer=new_lexer;
		filename=norm_resolved;
		cur=new_lexer->current;
		peek=new_lexer->peek();
		auto* fn_ptr=g_filename_pool.get(norm_resolved);
		auto* block=new AstNode(AstNodeKind::BLOCK,line,col,fn_ptr);
	while(!check(TOK_EOF)){
		if(check(TOK_AT_END)){advance();continue;}
		auto* decl=parse_decl();
			if(decl)add_import_to_block(block,decl);
		}
		delete new_lexer;
		lexer=old_lexer;
		cur=old_cur;
		peek=old_peek;
		filename=old_filename;
		return block;
	}
	std::string parse_import_path(){
		if(cur->kind==TOK_STRING_LIT){
			std::string path=cur->lexeme;
			advance();
			return path;
		}
		std::string buf;
		while(cur->kind==TOK_IDENT){
			buf+=cur->lexeme;
			advance();
			if(cur->kind==TOK_DOUBLE_COLON){
				buf+="::";
				advance();
			}else{
				break;
			}
		}
		if(buf.empty()){
			error_expected("import path");
			return "void";
		}
		return buf;
	}
	MioType* parse_base_type(){
		switch(cur->kind){
			case TOK_I8: advance(); return mio_type_new(MioTypeKind::I8);
			case TOK_I16: advance(); return mio_type_new(MioTypeKind::I16);
			case TOK_I32: advance(); return mio_type_new(MioTypeKind::I32);
			case TOK_I64: advance(); return mio_type_new(MioTypeKind::I64);
			case TOK_I128: advance(); return mio_type_new(MioTypeKind::I128);
			case TOK_U8: advance(); return mio_type_new(MioTypeKind::U8);
			case TOK_U16: advance(); return mio_type_new(MioTypeKind::U16);
			case TOK_U32: advance(); return mio_type_new(MioTypeKind::U32);
			case TOK_U64: advance(); return mio_type_new(MioTypeKind::U64);
			case TOK_U128: advance(); return mio_type_new(MioTypeKind::U128);
			case TOK_USIZE: advance(); return mio_type_new(MioTypeKind::USIZE);
			case TOK_ISIZE: advance(); return mio_type_new(MioTypeKind::ISIZE);
			case TOK_F32: advance(); return mio_type_new(MioTypeKind::F32);
			case TOK_F64: advance(); return mio_type_new(MioTypeKind::F64);
			case TOK_BOOL: advance(); return mio_type_new(MioTypeKind::BOOL);
			case TOK_CHAR: advance(); return mio_type_new(MioTypeKind::CHAR);
			case TOK_VOID: advance(); return mio_type_new(MioTypeKind::VOID);
			case TOK_IDENT:{
				std::string name=cur->lexeme;
				int line=cur->line,col=cur->col;
				advance();
				if(match(TOK_DOUBLE_COLON)){
					if(cur->kind!=TOK_IDENT){
						error_expected("type name after '::'");
						return mio_type_new(MioTypeKind::VOID);
					}
					std::string fullName=name+"::"+cur->lexeme;
					advance();
					auto* mt=mio_type_new_named(MioTypeKind::STRUCT,fullName);
					mt->line=line;mt->col=col;
					if(match(TOK_DOLLAR)){
						do{
							mt->param_types.push_back(parse_type());
						}while(match(TOK_COMMA));
						if(!match(TOK_DOLLAR)){
							error_expected("'$'");
						}
					}
					return mt;
				}
				auto* mt=mio_type_new_named(MioTypeKind::STRUCT,name);
				mt->line=line;mt->col=col;
				if(match(TOK_DOLLAR)){
					do{
						mt->param_types.push_back(parse_type());
					}while(match(TOK_COMMA));
					if(!match(TOK_DOLLAR)){
						error_expected("'$'");
					}
				}
				return mt;
			}
			default:
				error_expected("type");
				return mio_type_new(MioTypeKind::VOID);
		}
	}
	MioType* parse_type(){
		if(match(TOK_STAR)){
			auto* base=parse_type();
			return mio_type_new_pointer(base);
		}
		if(match(TOK_BIT_AND)){
			auto* base=parse_type();
			return mio_type_new_reference(base);
		}
		auto* base=parse_base_type();
		while(true){
			if(match(TOK_LBRACKET)){
				if(cur->kind==TOK_INT_LIT){
					int size=(int)cur->int_val;
					advance();
					expect(TOK_RBRACKET);
					base=mio_type_new_array(base,size);
				}else{
					expect(TOK_RBRACKET);
					base=mio_type_new_array(base,0);
				}
			}else if(match(TOK_DOLLAR)){
				if(base->kind!=MioTypeKind::STRUCT){
					error("template arguments can only be applied to struct types");
					return base;
				}
				do{
					base->param_types.push_back(parse_type());
				}while(match(TOK_COMMA));
				if(!match(TOK_DOLLAR)){
					error_expected("'$'");
				}
			}else{
				break;
			}
		}
		return base;
	}
	AstNode* parse_primary(){
		auto* t=cur;
		switch(t->kind){
			case TOK_INT_LIT:{
				advance();
				return ast_new_int_lit(t->int_val,t->line,t->col,fn());
			}
			case TOK_FLOAT_LIT:{
				advance();
				return ast_new_float_lit(t->float_val,t->line,t->col,fn());
			}
			case TOK_STRING_LIT:{
				advance();
				return ast_new_string_lit(t->lexeme,t->line,t->col,fn());
			}
			case TOK_CHAR_LIT:{
				advance();
				return ast_new_char_lit(t->char_val,t->line,t->col,fn());
			}
			case TOK_SIZEOF:{
				advance();
				expect(TOK_LPAREN);
				auto* target_type=parse_type();
				expect(TOK_RPAREN);
				return ast_new_sizeof_expr(target_type,t->line,t->col,fn());
			}
			case TOK_TRUE:{
				advance();
				return ast_new_bool_lit(true,t->line,t->col,fn());
			}
			case TOK_FALSE:{
				advance();
				return ast_new_bool_lit(false,t->line,t->col,fn());
			}
			case TOK_THIS:{
				advance();
				return ast_new_ident("this",t->line,t->col,fn());
			}
			case TOK_IDENT:{
				std::string name=t->lexeme;
				int line=t->line,col=t->col;
				advance();
				if(cur->kind==TOK_STAR&&peek->kind==TOK_LPAREN){
					advance();
					advance();
					auto* expr=parse_expr();
					expect(TOK_RPAREN);
					return ast_new_cast(mio_type_new_pointer(mio_type_new_named(MioTypeKind::STRUCT,name)),expr,line,col,fn());
				}
				if(match(TOK_DOUBLE_COLON)){
					if(cur->kind!=TOK_IDENT){
						error_expected("identifier after '::'");
						return nullptr;
					}
					auto* n=ast_new_ident(cur->lexeme,cur->line,cur->col,fn());
					n->ident.namespace_name=name;
					advance();
					return n;
				}
				return ast_new_ident(name,line,col,fn());
			}
			case TOK_DOUBLE_COLON:{
				advance();
				if(cur->kind!=TOK_IDENT){
					error_expected("identifier after '::'");
					return nullptr;
				}
				auto* n=ast_new_ident(cur->lexeme,cur->line,cur->col,fn());
				n->ident.namespace_name="::";
				advance();
				return n;
			}
			case TOK_LPAREN:{
				advance();
				auto* expr=parse_expr();
				expect(TOK_RPAREN);
				return expr;
			}
			case TOK_LBRACE:{
				advance();
				auto* arr=ast_new_array_lit(t->line,t->col,fn());
				if(!check(TOK_RBRACE)){
					ast_array_add(arr,parse_expr());
					while(match(TOK_COMMA)){
						if(check(TOK_RBRACE))break;
						ast_array_add(arr,parse_expr());
					}
				}
				expect(TOK_RBRACE);
				return arr;
			}
			case TOK_I32:case TOK_I64:
			case TOK_U32:case TOK_U64:
			case TOK_F32:case TOK_F64:
			case TOK_BOOL:case TOK_CHAR:{
				MioTypeKind k;
				switch(t->kind){
					case TOK_I32: k=MioTypeKind::I32;break;
					case TOK_I64: k=MioTypeKind::I64;break;
					case TOK_U32: k=MioTypeKind::U32;break;
					case TOK_U64: k=MioTypeKind::U64;break;
					case TOK_F32: k=MioTypeKind::F32;break;
					case TOK_F64: k=MioTypeKind::F64;break;
					case TOK_BOOL: k=MioTypeKind::BOOL;break;
					case TOK_CHAR: k=MioTypeKind::CHAR;break;
					default: k=MioTypeKind::VOID;break;
				}
				advance();
				if(check(TOK_STAR)&&peek->kind==TOK_LPAREN){
					advance();
					advance();
					auto* expr=parse_expr();
					expect(TOK_RPAREN);
					return ast_new_cast(mio_type_new_pointer(mio_type_new(k)),expr,t->line,t->col,fn());
				}
				if(check(TOK_LPAREN)){
					advance();
					auto* expr=parse_expr();
					expect(TOK_RPAREN);
					return ast_new_cast(mio_type_new(k),expr,t->line,t->col,fn());
				}
				error_expected("expression");
				return ast_new_int_lit(0,t->line,t->col,fn());
			}
			default:
				error_expected("expression");
				advance();
				return ast_new_int_lit(0,t->line,t->col,fn());
		}
	}
	AstNode* parse_postfix(){
		auto* expr=parse_primary();
		while(true){
			if(expr->kind==AstNodeKind::IDENT_EXPR&&cur->kind==TOK_DOLLAR){
				if(lexer->is_template_instantiation()){
					advance();
					auto* call=ast_new_call(expr,expr->line,expr->col,fn());
					do{
						if(is_type_token(cur->kind)){
							call->call.template_args.push_back({true,parse_type(),nullptr});
						}else{
							call->call.template_args.push_back({false,nullptr,parse_expr()});
						}
					}while(match(TOK_COMMA));
					if(!match(TOK_DOLLAR)){
						error_expected("'$'");
					}
					if(match(TOK_LPAREN)){
						if(!check(TOK_RPAREN)){
							ast_call_add_arg(call,parse_expr());
							while(match(TOK_COMMA))
								ast_call_add_arg(call,parse_expr());
						}
						expect(TOK_RPAREN);
					}
					expr=call;
					continue;
				}
			}
			if(match(TOK_LPAREN)){
				auto* call=ast_new_call(expr,expr->line,expr->col,fn());
				if(!check(TOK_RPAREN)){
					ast_call_add_arg(call,parse_expr());
					while(match(TOK_COMMA))
						ast_call_add_arg(call,parse_expr());
				}
				expect(TOK_RPAREN);
				expr=call;
			}else if(match(TOK_LBRACKET)){
				auto* index=parse_expr();
				expect(TOK_RBRACKET);
				expr=ast_new_index(expr,index,expr->line,expr->col,fn());
			}else if(match(TOK_DOT)){
				if(cur->kind!=TOK_IDENT){
					error_expected("identifier after '.'");
					break;
				}
				expr=ast_new_member(expr,cur->lexeme,false,expr->line,expr->col,fn());
				advance();
			}else if(match(TOK_ARROW)){
				if(cur->kind!=TOK_IDENT){
					error_expected("identifier after '->'");
					break;
				}
				expr=ast_new_member(expr,cur->lexeme,true,expr->line,expr->col,fn());
				advance();
			}else{
				break;
			}
		}
		return expr;
	}
	AstNode* parse_unary(){
		TokenKind op=cur->kind;
		switch(op){
			case TOK_MINUS:
			case TOK_NOT:
			case TOK_BIT_NOT:
			case TOK_STAR:
			case TOK_BIT_AND:
				advance();
				return ast_new_unary(op,parse_unary(),cur->line,cur->col,fn());
			default:
				return parse_postfix();
		}
	}
	AstNode* parse_multiplicative(){
		auto* left=parse_unary();
		while(true){
			TokenKind op=cur->kind;
			if(op==TOK_STAR||op==TOK_SLASH||op==TOK_PERCENT){
				advance();
				left=ast_new_binary(left,op,parse_unary(),left->line,left->col,fn());
			}else{
				break;
			}
		}
		return left;
	}
	AstNode* parse_additive(){
		auto* left=parse_multiplicative();
		while(true){
			TokenKind op=cur->kind;
			if(op==TOK_PLUS||op==TOK_MINUS){
				advance();
				left=ast_new_binary(left,op,parse_multiplicative(),left->line,left->col,fn());
			}else{
				break;
			}
		}
		return left;
	}
	AstNode* parse_shift(){
		auto* left=parse_additive();
		while(true){
			TokenKind op=cur->kind;
			if(op==TOK_LSHIFT||op==TOK_RSHIFT){
				advance();
				left=ast_new_binary(left,op,parse_additive(),left->line,left->col,fn());
			}else{
				break;
			}
		}
		return left;
	}
	AstNode* parse_relational(){
		auto* left=parse_shift();
		while(true){
			TokenKind op=cur->kind;
			if(op==TOK_LT||op==TOK_GT||op==TOK_LTE||op==TOK_GTE){
				advance();
				left=ast_new_binary(left,op,parse_shift(),left->line,left->col,fn());
			}else{
				break;
			}
		}
		return left;
	}
	AstNode* parse_equality(){
		auto* left=parse_relational();
		while(true){
			TokenKind op=cur->kind;
			if(op==TOK_EQ||op==TOK_NEQ){
				advance();
				left=ast_new_binary(left,op,parse_relational(),left->line,left->col,fn());
			}else{
				break;
			}
		}
		return left;
	}
	AstNode* parse_bit_and(){
		auto* left=parse_equality();
		while(match(TOK_BIT_AND))
			left=ast_new_binary(left,TOK_BIT_AND,parse_equality(),left->line,left->col,fn());
		return left;
	}
	AstNode* parse_bit_xor(){
		auto* left=parse_bit_and();
		while(match(TOK_BIT_XOR))
			left=ast_new_binary(left,TOK_BIT_XOR,parse_bit_and(),left->line,left->col,fn());
		return left;
	}
	AstNode* parse_bit_or(){
		auto* left=parse_bit_xor();
		while(match(TOK_BIT_OR))
			left=ast_new_binary(left,TOK_BIT_OR,parse_bit_xor(),left->line,left->col,fn());
		return left;
	}
	AstNode* parse_logical_and(){
		auto* left=parse_bit_or();
		while(match(TOK_AND))
			left=ast_new_binary(left,TOK_AND,parse_bit_or(),left->line,left->col,fn());
		return left;
	}
	AstNode* parse_logical_or(){
		auto* left=parse_logical_and();
		while(match(TOK_OR))
			left=ast_new_binary(left,TOK_OR,parse_logical_and(),left->line,left->col,fn());
		return left;
	}
	AstNode* parse_assignment(){
		auto* left=parse_logical_or();
		TokenKind assignOp=TOK_EOF;
		if(cur->kind==TOK_ASSIGN||cur->kind==TOK_PLUS_ASSIGN||cur->kind==TOK_MINUS_ASSIGN||cur->kind==TOK_STAR_ASSIGN||cur->kind==TOK_SLASH_ASSIGN||cur->kind==TOK_PERCENT_ASSIGN||cur->kind==TOK_AND_ASSIGN||cur->kind==TOK_OR_ASSIGN||cur->kind==TOK_XOR_ASSIGN||cur->kind==TOK_LSHIFT_ASSIGN||cur->kind==TOK_RSHIFT_ASSIGN){
			assignOp=cur->kind;
			advance();
			auto* right=parse_assignment();
			return ast_new_assign(left,assignOp,right,left->line,left->col,fn());
		}
		return left;
	}
	AstNode* parse_expr(){
		return parse_assignment();
	}
	AstNode* parse_block(){
		int line=cur->line,col=cur->col;
		expect(TOK_LBRACE);
		auto* block=ast_new_block(line,col,fn());
		while(!check(TOK_RBRACE)&&!check(TOK_EOF)){
			auto* stmt=parse_stmt();
			if(stmt)block->block.stmts.push_back(stmt);
		}
		expect(TOK_RBRACE);
		return block;
	}
	AstNode* parse_var_items(bool is_const,bool is_static){
		int line=cur->line,col=cur->col;
		std::string name=cur->lexeme;
		expect(TOK_IDENT);
		MioType* type=nullptr;
		AstNode* init=nullptr;
		if(match(TOK_COLON))
			type=parse_type();
		if(match(TOK_ASSIGN))
			init=parse_expr();
		if(init&&init->kind==AstNodeKind::ARRAY_LIT){
			if(type&&type->kind==MioTypeKind::ARRAY&&type->array_size==0){
				type->array_size=init->array_lit.elements.size();
			}else if(!type){
				MioType* base=nullptr;
				if(!init->array_lit.elements.empty()){
					auto* first=init->array_lit.elements[0];
					switch(first->kind){
						case AstNodeKind::INT_LIT:   base=mio_type_new(MioTypeKind::I32);break;
						case AstNodeKind::FLOAT_LIT: base=mio_type_new(MioTypeKind::F64);break;
						case AstNodeKind::BOOL_LIT:  base=mio_type_new(MioTypeKind::BOOL);break;
						case AstNodeKind::CHAR_LIT:  base=mio_type_new(MioTypeKind::CHAR);break;
						case AstNodeKind::STRING_LIT:base=mio_type_new(MioTypeKind::CHAR);break;
						default:                     base=mio_type_new(MioTypeKind::I32);break;
					}
				}
				if(!base)base=mio_type_new(MioTypeKind::I32);
				type=mio_type_new_array(base,init->array_lit.elements.size());
			}
		}
		if(is_const)
			return ast_new_const_decl(name,type,init,is_static,line,col,fn());
		else
			return ast_new_var_decl(name,type,init,is_static,line,col,fn());
	}
	AstNode* parse_var_decl(bool is_const,bool is_static){
		advance();
		auto* block=ast_new_block(cur->line,cur->col,fn());
		block->block.is_scope=false;
		block->block.stmts.push_back(parse_var_items(is_const,is_static));
		while(match(TOK_COMMA))
			block->block.stmts.push_back(parse_var_items(is_const,is_static));
		expect(TOK_SEMICOLON);
		return block;
	}
	AstNode* parse_if_stmt(){
		int line=cur->line,col=cur->col;
		advance();
		expect(TOK_COLON);
		auto* cond=parse_expr();
		auto* then_body=parse_stmt();
		auto* if_node=ast_new_if(cond,then_body,nullptr,line,col,fn());
		while(match(TOK_ELIF)){
			expect(TOK_COLON);
			auto* elif_cond=parse_expr();
			auto* elif_body=parse_stmt();
			ast_if_add_elif(if_node,elif_cond,elif_body);
		}
		if(match(TOK_ELSE)){
			if_node->if_stmt.else_body=parse_stmt();
		}
		return if_node;
	}
	AstNode* parse_while_stmt(){
		int line=cur->line,col=cur->col;
		advance();
		expect(TOK_COLON);
		auto* cond=parse_expr();
		auto* body=parse_stmt();
		return ast_new_while(cond,body,line,col,fn());
	}
	AstNode* parse_for_stmt(){
		int line=cur->line,col=cur->col;
		advance();
		expect(TOK_COLON);
		bool has_paren=match(TOK_LPAREN);
		AstNode* init=nullptr;
		AstNode* cond=nullptr;
		AstNode* update=nullptr;
		if(!check(TOK_SEMICOLON))
			init=parse_expr();
		expect(TOK_SEMICOLON);
		if(!check(TOK_SEMICOLON)&&!check(TOK_RPAREN))
			cond=parse_expr();
		expect(TOK_SEMICOLON);
		if(!check(TOK_LBRACE)&&!check(TOK_RPAREN))
			update=parse_expr();
		if(has_paren)
			expect(TOK_RPAREN);
		auto* body=parse_stmt();
		return ast_new_for(init,cond,update,body,line,col,fn());
	}
	AstNode* parse_return_stmt(){
		int line=cur->line,col=cur->col;
		advance();
		AstNode* value=nullptr;
		if(!check(TOK_SEMICOLON)&&!check(TOK_RBRACE))
			value=parse_expr();
		expect(TOK_SEMICOLON);
		return ast_new_return(value,line,col,fn());
	}
	AstNode* parse_stmt(){
		switch(cur->kind){
			case TOK_VAR:
				return parse_var_decl(false,false);
			case TOK_CONST:
				return parse_var_decl(true,false);
			case TOK_IF:
				return parse_if_stmt();
			case TOK_WHILE:
				return parse_while_stmt();
			case TOK_FOR:
				return parse_for_stmt();
			case TOK_BREAK:
				advance();
				expect(TOK_SEMICOLON);
				return ast_new_break(cur->line,cur->col,fn());
			case TOK_CONTINUE:
				advance();
				expect(TOK_SEMICOLON);
				return ast_new_continue(cur->line,cur->col,fn());
			case TOK_GOTO:{
				advance();
				int line=cur->line,col=cur->col;
				std::string label=cur->lexeme;
				expect(TOK_IDENT);
				expect(TOK_SEMICOLON);
				return ast_new_goto(label,line,col,fn());
			}
			case TOK_COLON:{
				advance();
				int line=cur->line,col=cur->col;
				std::string label=cur->lexeme;
				expect(TOK_IDENT);
				return ast_new_label(label,line,col,fn());
			}
			case TOK_RETURN:
				return parse_return_stmt();
			case TOK_LBRACE:
				return parse_block();
			case TOK_SEMICOLON:
				advance();
				return nullptr;
			default:{
				auto* expr=parse_expr();
				if(!expr){
					error("failed to parse expression");
					return nullptr;
				}
				if(match(TOK_SEMICOLON))
					return ast_new_expr_stmt(expr,expr->line,expr->col,fn());
				return ast_new_return(expr,expr->line,expr->col,fn());
			}
		}
	}
	AstNode* parse_func_def(bool is_static,bool is_extern=false,AstNode* pre_parsed_return_type=nullptr){
		int line=pre_parsed_return_type?pre_parsed_return_type->line:cur->line;
		int col=pre_parsed_return_type?pre_parsed_return_type->col:cur->col;
		MioType* return_type=nullptr;
		if(pre_parsed_return_type){
			return_type=pre_parsed_return_type->func_def.return_type;
			pre_parsed_return_type->func_def.return_type=nullptr;
			delete pre_parsed_return_type;
		}else{
			return_type=parse_type();
		}
		bool is_operator=false;
		std::string func_name;
		std::string op_name;
		if(match(TOK_OPERATOR)){
			is_operator=true;
			func_name="operator"+tok_name(cur->kind);
			op_name=tok_name(cur->kind);
			if(cur->kind==TOK_LBRACKET){
				advance();
				expect(TOK_RBRACKET);
			}else{
				advance();
			}
		}else if(cur->kind==TOK_IDENT){
			func_name=cur->lexeme;
			advance();
		}else if(cur->kind==TOK_LPAREN){
			func_name=return_type->name.empty()?"constructor":return_type->name;
		}else{
			error_expected("function name");
			mio_type_free(return_type);
			return nullptr;
		}
		auto* func=ast_new_func_def(func_name,return_type,nullptr,is_static,line,col,fn());
		func->func_def.is_operator=is_operator;
		if(is_operator)func->func_def.op_name=op_name;
		expect(TOK_LPAREN);
		if(!check(TOK_RPAREN)){
			do{
				if(match(TOK_VARARG)){
					func->func_def.is_variadic=true;
					break;
				}
				std::string pname=cur->lexeme;
				expect(TOK_IDENT);
				expect(TOK_COLON);
				auto* ptype=parse_type();
				AstNode* default_val=nullptr;
				if(match(TOK_ASSIGN)){
					default_val=parse_expr();
					if(!default_val){
						error("expected default value after '='");
						mio_type_free(ptype);
						return nullptr;
					}
				}
				ast_func_add_param(func,pname,ptype,default_val);
			}while(match(TOK_COMMA));
		}
		expect(TOK_RPAREN);
		if(match(TOK_ASSIGN)){
			if(cur->kind==TOK_INT_LIT && cur->int_val==0){
				func->func_def.is_pure_virtual=true;
				advance();
			}else{
				error("expected '0' after '=' for pure virtual function");
			}
		}
		if(!is_operator&&!func->func_def.is_pure_virtual&&match(TOK_COLON)){
			while(!check(TOK_LBRACE)&&!check(TOK_SEMICOLON)&&!check(TOK_EOF)){
				if(cur->kind==TOK_IDENT){
					std::string field_name=cur->lexeme;
					advance();
					if(match(TOK_LPAREN)){
						auto* init_expr=parse_expr();
						expect(TOK_RPAREN);
						ast_func_add_init(func,field_name,init_expr);
					}else if(match(TOK_ASSIGN)){
						auto* init_expr=parse_expr();
						ast_func_add_init(func,field_name,init_expr);
					}else{
						char buf[128];
						snprintf(buf,sizeof(buf),"expected '(' or '=' after field name '%s' in initializer list",field_name.c_str());
						error(buf);
					}
				}else{
					char buf[128];
					snprintf(buf,sizeof(buf),"expected field name in initializer list, got '%s'",tok_name(cur->kind).c_str());
					error(buf);
					advance();
				}
				if(!match(TOK_COMMA))break;
			}
		}
		func->func_def.is_extern=is_extern;
		if(is_extern||func->func_def.is_pure_virtual){
			expect(TOK_SEMICOLON);
		}else{
			func->func_def.body=parse_block();
		}
		return func;
	}
	AstNode* parse_enum_def(){
		int line=cur->line,col=cur->col;
		advance();
		std::string name=cur->lexeme;
		expect(TOK_IDENT);
		expect(TOK_LBRACE);
		auto* e=ast_new_enum_def(name,line,col,fn());
		if(!check(TOK_RBRACE)){
			do{
				std::string vname=cur->lexeme;
				expect(TOK_IDENT);
				AstNode* init=nullptr;
				if(match(TOK_ASSIGN))
					init=parse_expr();
				ast_enum_add_variant(e,vname,init);
			}while(match(TOK_COMMA));
		}
		expect(TOK_RBRACE);
		return e;
	}
	AstNode* parse_union_def(){
		int line=cur->line,col=cur->col;
		advance();
		std::string name=cur->lexeme;
		expect(TOK_IDENT);
		expect(TOK_LBRACE);
		auto* u=ast_new_union_def(name,line,col,fn());
		while(!check(TOK_RBRACE)&&!check(TOK_EOF)){
			std::string fname=cur->lexeme;
			expect(TOK_IDENT);
			expect(TOK_COLON);
			auto* ftype=parse_type();
			expect(TOK_SEMICOLON);
			ast_union_add_field(u,fname,ftype);
		}
		expect(TOK_RBRACE);
		return u;
	}
	bool is_type_token(TokenKind kind){
		switch(kind){
			case TOK_I8: case TOK_I16: case TOK_I32: case TOK_I64: case TOK_I128:
			case TOK_U8: case TOK_U16: case TOK_U32: case TOK_U64: case TOK_U128:
			case TOK_USIZE: case TOK_ISIZE: case TOK_F32: case TOK_F64:
			case TOK_BOOL: case TOK_CHAR: case TOK_VOID: case TOK_IDENT:
			case TOK_STAR: case TOK_BIT_AND:
				return true;
			default: return false;
		}
	}
	AstNode* parse_class_def(){
		int line=cur->line,col=cur->col;
		advance();
		if(cur->kind!=TOK_IDENT){
			error("expected class name");
			return nullptr;
		}
		std::string name=cur->lexeme;
		advance();
		if(class_names.count(name)){
			char buf[256];
			snprintf(buf,sizeof(buf),"class '%s' is already defined",name.c_str());
			error(buf);
		}
		class_names.insert(name);
		std::string base_name;
		std::string base_access;
		if(match(TOK_LPAREN)){
			if(cur->kind!=TOK_IDENT){
				error("expected base class name after '('");
				return nullptr;
			}
			base_name=cur->lexeme;
			advance();
			if(!class_names.count(base_name)){
				char buf[256];
				snprintf(buf,sizeof(buf),"base class '%s' is not defined (forward declaration not supported)",base_name.c_str());
				error(buf);
			}
			if(match(TOK_COLON)){
				if(cur->kind==TOK_PUBLIC||cur->kind==TOK_PRIVATE||cur->kind==TOK_PROTECTED){
					base_access=tok_name(cur->kind);
					advance();
				}else{
					error("expected 'public', 'private', or 'protected' after ':' in base class declaration");
					return nullptr;
				}
			}
			expect(TOK_RPAREN);
		}
		expect(TOK_LBRACE);
		auto* c=ast_new_class_def(name,base_name,base_access,line,col,fn());
		class_base_map[name]=base_name;
		Access cur_access=Access::PUBLIC;
		std::unordered_set<std::string> field_names;
		while(!check(TOK_RBRACE)&&!check(TOK_EOF)){
			if(match(TOK_PUBLIC)){
				expect(TOK_COLON);
				cur_access=Access::PUBLIC;
			}else if(match(TOK_PRIVATE)){
				expect(TOK_COLON);
				cur_access=Access::PRIVATE;
			}else if(match(TOK_PROTECTED)){
				expect(TOK_COLON);
				cur_access=Access::PROTECTED;
			}else{
				bool matched_virtual=match(TOK_VIRTUAL);
				bool matched_override=false;
				if(!matched_virtual)matched_override=match(TOK_OVERRIDE);
				bool matched_static=false;
				if(!matched_virtual&&!matched_override)matched_static=match(TOK_STATIC);
				if(matched_override){
					if(base_name.empty()){
						error("'override' can only be used in a class that inherits from a base class");
					}
				}
				if(matched_virtual||matched_override||matched_static){
					if(matched_override&&!matched_virtual){
						matched_virtual=true;
					}
					auto* method=parse_func_def(matched_static);
					if(method){
						method->func_def.is_virtual=matched_virtual;
						method->func_def.is_override=matched_override;
						method->func_def.class_name=c->class_def.name;
						method->func_def.access=cur_access;
						if(matched_override&&!base_name.empty()){
							auto it=class_virtual_methods.find(base_name);
							if(it!=class_virtual_methods.end()&&!it->second.count(method->func_def.name)){
								char buf[256];
								snprintf(buf,sizeof(buf),"method '%s' marked 'override' but does not override any base class virtual method",method->func_def.name.c_str());
								error(buf);
							}
						}
						if(matched_virtual&&!method->func_def.is_pure_virtual){
							class_virtual_methods[name][method->func_def.name]=true;
						}
						if(method->func_def.is_pure_virtual){
							class_virtual_methods[name][method->func_def.name]=true;
						}
						if(matched_override&&!method->func_def.is_pure_virtual){
							class_virtual_methods[name][method->func_def.name]=true;
						}
						if(!method->func_def.is_operator&&class_method_names[name].count(method->func_def.name)){
							char buf[256];
							snprintf(buf,sizeof(buf),"method '%s' is already defined in class '%s'",method->func_def.name.c_str(),name.c_str());
							error(buf);
						}
						class_method_names[name].insert(method->func_def.name);
						if(method->func_def.name==name){
							c->class_def.constructors.push_back(method);
						}else{
							ast_class_add_method(c,method);
						}
					}
				}else if(match(TOK_BIT_NOT)){
					int dline=cur->line,dcol=cur->col;
					if(cur->kind!=TOK_IDENT){
						error("expected destructor name after '~'");
						advance();
						continue;
					}
					std::string dname=cur->lexeme;
					if(dname!=name){
						char buf[256];
						snprintf(buf,sizeof(buf),"destructor name '%s' must match class name '%s'",dname.c_str(),name.c_str());
						error(buf);
						advance();
						if(match(TOK_LPAREN)){match(TOK_RPAREN);}
						if(check(TOK_LBRACE))parse_block();
						continue;
					}
					advance();
					expect(TOK_LPAREN);
					expect(TOK_RPAREN);
					auto* dtor=ast_new_func_def("~"+name,mio_type_new(MioTypeKind::VOID),nullptr,false,dline,dcol,fn());
					dtor->func_def.class_name=name;
					dtor->func_def.access=cur_access;
					dtor->func_def.body=parse_block();
					c->class_def.destructor=dtor;
				}else if(is_type_token(cur->kind)){
										if(cur->kind==TOK_IDENT&&peek->kind==TOK_COLON){
						std::string fname=cur->lexeme;
						advance();
						if(field_names.count(fname)){
							char buf[256];
							snprintf(buf,sizeof(buf),"field '%s' is already defined in class '%s'",fname.c_str(),name.c_str());
							error(buf);
						}
						field_names.insert(fname);
						expect(TOK_COLON);
						auto* ftype=parse_type();
						AstNode* init=nullptr;
						if(match(TOK_ASSIGN))
							init=parse_expr();
						expect(TOK_SEMICOLON);
						ast_class_add_field(c,fname,ftype,init,cur_access);
					}else{
						auto* method=parse_func_def(false);
						if(method){
							method->func_def.class_name=c->class_def.name;
							method->func_def.access=cur_access;
							if(method->func_def.name==name){
								c->class_def.constructors.push_back(method);
							}else{
								if(!method->func_def.is_operator&&class_method_names[name].count(method->func_def.name)){
									char buf[256];
									snprintf(buf,sizeof(buf),"method '%s' is already defined in class '%s'",method->func_def.name.c_str(),name.c_str());
									error(buf);
								}
								class_method_names[name].insert(method->func_def.name);
								ast_class_add_method(c,method);
							}
						}
					}
				}else{
					char buf[128];
					snprintf(buf,sizeof(buf),"expected field, method, or access specifier in class '%s', got '%s'",name.c_str(),tok_name(cur->kind).c_str());
					error(buf);
					advance();
				}
			}
		}
		expect(TOK_RBRACE);
		match(TOK_SEMICOLON);
		return c;
	}
	AstNode* parse_namespace_def(){
		int line=cur->line,col=cur->col;
		advance();
		if(cur->kind!=TOK_IDENT){
			error("expected namespace name");
			return nullptr;
		}
		std::string name=cur->lexeme;
		advance();
		auto* n=ast_new_namespace_def(name,line,col,fn());
		expect(TOK_LBRACE);
		while(!check(TOK_RBRACE)&&!check(TOK_EOF)){
			auto* decl=parse_decl();
			if(decl)n->namespace_def.body.push_back(decl);
		}
		expect(TOK_RBRACE);
		return n;
	}
	AstNode* parse_template_def(){
		int line=cur->line,col=cur->col;
		advance();
		if(!match(TOK_DOLLAR)){
			error_expected("'$' after 'template'");
			return nullptr;
		}
		std::vector<TemplateParam> type_params;
		do{
			TemplateParam tp;
			tp.is_type=false;
			tp.type=nullptr;
			tp.default_type=nullptr;
			tp.default_val=nullptr;
			if(cur->kind!=TOK_IDENT){
				error_expected("template parameter name");
				return nullptr;
			}
			tp.name=cur->lexeme;
			advance();
			if(match(TOK_COLON)){
				if(match(TOK_TYPENAME)){
					tp.is_type=true;
				}else{
					tp.type=parse_type();
					if(!tp.type){
						error("expected type after ':' in template parameter");
						return nullptr;
					}
				}
			}else{
				tp.is_type=true;
			}
			if(match(TOK_ASSIGN)){
				if(tp.is_type){
					tp.default_type=parse_type();
					if(!tp.default_type){
						error("expected default type after '=' in template parameter");
						return nullptr;
					}
				}else{
					tp.default_val=parse_expr();
					if(!tp.default_val){
						error("expected default value after '=' in template parameter");
						return nullptr;
					}
				}
			}
			type_params.push_back(tp);
		}while(match(TOK_COMMA));
		if(!match(TOK_DOLLAR)){
			error_expected("'$' after template parameters");
			return nullptr;
		}
		auto* def=parse_decl();
		if(!def){
			error("expected declaration after template");
			return nullptr;
		}
		return ast_new_template_def(type_params,def,line,col,fn());
	}
	bool is_macro_defined(const std::string& name){
		for(const auto& m:macros)
			if(m.name==name)return true;
		return false;
	}
	void skip_cond_block(){
		while(!check(TOK_EOF)&&!check(TOK_AT_ELIF)&&!check(TOK_AT_ELSE)&&!check(TOK_AT_END))
			advance();
	}
	void skip_cond_to_end(){
		int depth=0;
		while(!check(TOK_EOF)){
			if(check(TOK_AT_IF))depth++;
			if(check(TOK_AT_ELIF)&&depth==0)return;
			if(check(TOK_AT_ELSE)&&depth==0)return;
			if(check(TOK_AT_END)){
				if(depth==0)return;
				depth--;
			}
			advance();
		}
	}
	AstNode* parse_cond_comp(int line,int col){
		advance();
		bool negate=false;
		if(check(TOK_NOT)){
			advance();
			negate=true;
		}
		if(check(TOK_IDENT)){
			bool defined=is_macro_defined(cur->lexeme);
			advance();
			bool result=negate?!defined:defined;
			if(result){
				auto* block=ast_new_block(line,col,fn());
				while(!check(TOK_EOF)&&!check(TOK_AT_ELIF)&&!check(TOK_AT_ELSE)&&!check(TOK_AT_END)){
					auto* decl=parse_decl();
					if(decl)block->block.stmts.push_back(decl);
				}
				skip_cond_to_end();
				if(check(TOK_AT_ELIF))parse_cond_comp(cur->line,cur->col);
				if(check(TOK_AT_ELSE)){
					advance();
					skip_cond_block();
				}
				if(check(TOK_AT_END))advance();
				return block;
			}else{
				skip_cond_block();
				if(check(TOK_AT_ELIF)){
					return parse_cond_comp(cur->line,cur->col);
				}
				if(check(TOK_AT_ELSE)){
					advance();
					auto* block=ast_new_block(line,col,fn());
					while(!check(TOK_EOF)&&!check(TOK_AT_ELIF)&&!check(TOK_AT_ELSE)&&!check(TOK_AT_END)){
						auto* decl=parse_decl();
						if(decl)block->block.stmts.push_back(decl);
					}
					if(check(TOK_AT_END))advance();
					return block;
				}
				if(check(TOK_AT_END))advance();
				return nullptr;
			}
		}else{
			error("expected identifier in @if condition");
			skip_cond_to_end();
			if(check(TOK_AT_END))advance();
			return nullptr;
		}
	}
	AstNode* parse_decl(){
		while(match(TOK_SEMICOLON));
		switch(cur->kind){
			case TOK_IMPORT:{
				advance();
				int line=cur->line,col=cur->col;
				auto* first=parse_single_import(line,col);
				if(!match(TOK_COMMA)){
					expect(TOK_SEMICOLON);
					return first;
				}
				auto* block=ast_new_block(line,col,fn());
				add_import_to_block(block,first);
				do{
					auto* import=parse_single_import(line,col);
					if(import)add_import_to_block(block,import);
				}while(match(TOK_COMMA));
				expect(TOK_SEMICOLON);
				return block->block.stmts.empty()?(delete block,nullptr):block;
			}
			case TOK_EXTERN:{
				advance();
				return parse_func_def(false,true);
			}
			case TOK_VAR:
				return parse_var_decl(false,false);
			case TOK_CONST:
				return parse_var_decl(true,false);
			case TOK_STATIC:{
				advance();
				if(match(TOK_VAR))
					return parse_var_decl(false,true);
				if(match(TOK_CONST))
					return parse_var_decl(true,true);
				return parse_func_def(true);
			}
			case TOK_ENUM:
				return parse_enum_def();
			case TOK_UNION:
				return parse_union_def();
			case TOK_CLASS:
				return parse_class_def();
			case TOK_NAMESPACE:
				return parse_namespace_def();
			case TOK_MACRO:{
				advance();
				if(cur->kind!=TOK_IDENT){
					error("expected macro name");
					return nullptr;
				}
				std::string name=cur->lexeme;
				int line=cur->line,col=cur->col;
				advance();
				std::string value;
				if(!check(TOK_SEMICOLON)){
					if(check(TOK_STRING_LIT)){
						value=cur->lexeme;
						advance();
					}else if(check(TOK_INT_LIT)||check(TOK_FLOAT_LIT)){
						value=cur->lexeme;
						advance();
					}else if(check(TOK_TRUE)){
						value="1";
						advance();
					}else if(check(TOK_FALSE)){
						value="0";
						advance();
					}else if(check(TOK_IDENT)){
						value=cur->lexeme;
						advance();
					}else{
						error("expected macro value or ';'");
						return nullptr;
					}
				}
				if(is_macro_defined(name)){
					char buf[256];
					snprintf(buf,sizeof(buf),"macro '%s' is already defined",name.c_str());
					error(buf);
				}
				add_macro(name,value.empty()?"1":value);
				auto* node=ast_new_macro_def(name,value.empty()?"1":value,line,col,fn());
				expect(TOK_SEMICOLON);
				return node;
			}
			case TOK_AT_IF:
				return parse_cond_comp(cur->line,cur->col);
			case TOK_TEMPLATE:
				return parse_template_def();
			case TOK_EOF:
			case TOK_AT_END:
				return nullptr;
			default:{
				bool mv=match(TOK_VIRTUAL);
				if(!mv)mv=match(TOK_OVERRIDE);
				if(mv||is_type_token(cur->kind)){
					return parse_func_def(false);
				}
				error("expected declaration");
				while(!check(TOK_EOF)&&!check(TOK_SEMICOLON)&&!check(TOK_RBRACE))
					advance();
				advance();
				return nullptr;
			}
		}
	}
};
#endif