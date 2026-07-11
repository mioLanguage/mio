#ifndef MIO_AST_HPP
#define MIO_AST_HPP
#include"types.hpp"
#include"token.hpp"
#include<vector>
#include<string>
#include<cstdint>
#include<cstdio>
#include<cstdlib>
#include<cstring>
enum class AstNodeKind{
	PROGRAM,
	IMPORT,
	VAR_DECL,
	CONST_DECL,
	FUNC_DEF,
	STRUCT_DEF,
	ENUM_DEF,
	UNION_DEF,
	BLOCK,
	IF_STMT,
	WHILE_STMT,
	FOR_STMT,
	BREAK_STMT,
	CONTINUE_STMT,
	GOTO_STMT,
	LABEL_STMT,
	RETURN_STMT,
	EXPR_STMT,
	BINARY_EXPR,
	UNARY_EXPR,
	CALL_EXPR,
	INDEX_EXPR,
	MEMBER_EXPR,
	IDENT_EXPR,
	INT_LIT,
	FLOAT_LIT,
	STRING_LIT,
	BOOL_LIT,
	CHAR_LIT,
	ARRAY_LIT,
	CAST_EXPR,
	ASSIGN_EXPR,
	MACRO_DEF,
};
class AstNode;
struct Param{
	std::string name;
	MioType*type;
};
struct Field{
	std::string name;
	MioType*type;
	AstNode*init;
};
struct Variant{
	std::string name;
	AstNode*init;
};
struct UnionField{
	std::string name;
	MioType*type;
};
struct InitField{
	std::string name;
	AstNode*expr;
};
class AstNode{
public:
	AstNodeKind kind;
	MioType*type;
	int line;
	int col;
	struct{
		std::vector<AstNode*> nodes;
	} program;
	struct{
		std::string path;
	} import;
	struct{
		std::string name;
		MioType*var_type;
		AstNode*init;
		bool is_static;
	} var_decl;
	struct{
		std::string name;
		MioType*var_type;
		AstNode*init;
		bool is_static;
	} const_decl;
	struct{
		std::string name;
		MioType*return_type;
		std::vector<Param> params;
		AstNode*body;
		bool is_static,is_operator;
		std::string op_name,struct_name;
		std::vector<InitField> init_list;
	} func_def;
	struct{
		std::string name;
		std::vector<Field> fields;
		std::vector<AstNode*> methods;
	} struct_def;
	struct{
		std::string name;
		std::vector<Variant> variants;
	} enum_def;
	struct{
		std::string name;
		std::vector<UnionField> fields;
	} union_def;
	struct{
		std::vector<AstNode*> stmts;
	} block;
	struct{
		AstNode*cond,*then_body,*else_body;
		std::vector<AstNode*> elif_list;
	} if_stmt;
	struct{
		AstNode*cond,*body;
	} while_stmt;
	struct{
		AstNode*init,*cond,*update,*body;
	} for_stmt;
	struct{
		std::string label;
	} goto_stmt;
	struct{
		std::string label;
	} label_stmt;
	struct{
		AstNode*value;
	} return_stmt;
	struct{
		AstNode*expr;
	} expr_stmt;
	struct{
		AstNode*left;
		TokenKind op;
		AstNode*right;
	} binary;
	struct{
		TokenKind op;
		AstNode*operand;
	} unary;
	struct{
		AstNode*callee;
		std::vector<AstNode*> args;
	} call;
	struct{
		AstNode*base;
		AstNode*index;
	} index_expr;
	struct{
		AstNode*base;
		std::string member;
		bool arrow;
	} member;
	struct{
		std::string name;
	} ident;
	struct{
		int64_t value;
	} int_lit;
	struct{
		double value;
	} float_lit;
	struct{
		std::string value;
	} string_lit;
	struct{
		bool value;
	} bool_lit;
	struct{
		char value;
	} char_lit;
	struct{
		std::vector<AstNode*> elements;
	} array_lit;
	struct{
		MioType*target_type;
		AstNode*expr;
	} cast_expr;
	struct{
		AstNode*left,*right;
	} assign;
	struct{
		std::string name,value;
	} macro_def;
	AstNode(AstNodeKind k,int l,int c):kind(k),type(nullptr),line(l),col(c){}
	~AstNode();
};
inline AstNode::~AstNode(){
	mio_type_free(type);
	switch(kind){
		case AstNodeKind::PROGRAM:
			for(auto*n:program.nodes)delete n;
			break;
		case AstNodeKind::IMPORT:
			break;
		case AstNodeKind::VAR_DECL:
			mio_type_free(var_decl.var_type);
			delete var_decl.init;
			break;
		case AstNodeKind::CONST_DECL:
			mio_type_free(const_decl.var_type);
			delete const_decl.init;
			break;
		case AstNodeKind::FUNC_DEF:
			mio_type_free(func_def.return_type);
			for(auto& p:func_def.params)mio_type_free(p.type);
			delete func_def.body;
			for(auto& f:func_def.init_list)delete f.expr;
			break;
		case AstNodeKind::STRUCT_DEF:
			for(auto& f:struct_def.fields){
				mio_type_free(f.type);
				delete f.init;
			}
			for(auto*m:struct_def.methods)delete m;
			break;
		case AstNodeKind::ENUM_DEF:
			for(auto& v:enum_def.variants)delete v.init;
			break;
		case AstNodeKind::UNION_DEF:
			for(auto& f:union_def.fields)mio_type_free(f.type);
			break;
		case AstNodeKind::BLOCK:
			for(auto*s:block.stmts)delete s;
			break;
		case AstNodeKind::IF_STMT:
			delete if_stmt.cond;
			delete if_stmt.then_body;
			delete if_stmt.else_body;
			for(auto*n:if_stmt.elif_list)delete n;
			break;
		case AstNodeKind::WHILE_STMT:
			delete while_stmt.cond;
			delete while_stmt.body;
			break;
		case AstNodeKind::FOR_STMT:
			delete for_stmt.init;
			delete for_stmt.cond;
			delete for_stmt.update;
			delete for_stmt.body;
			break;
		case AstNodeKind::GOTO_STMT:
			break;
		case AstNodeKind::LABEL_STMT:
			break;
		case AstNodeKind::RETURN_STMT:
			delete return_stmt.value;
			break;
		case AstNodeKind::EXPR_STMT:
			delete expr_stmt.expr;
			break;
		case AstNodeKind::BINARY_EXPR:
			delete binary.left;
			delete binary.right;
			break;
		case AstNodeKind::UNARY_EXPR:
			delete unary.operand;
			break;
		case AstNodeKind::CALL_EXPR:
			delete call.callee;
			for(auto*a:call.args)delete a;
			break;
		case AstNodeKind::INDEX_EXPR:
			delete index_expr.base;
			delete index_expr.index;
			break;
		case AstNodeKind::MEMBER_EXPR:
			delete member.base;
			break;
		case AstNodeKind::IDENT_EXPR:
			break;
		case AstNodeKind::STRING_LIT:
			break;
		case AstNodeKind::ARRAY_LIT:
			for(auto*e:array_lit.elements)delete e;
			break;
		case AstNodeKind::CAST_EXPR:
			mio_type_free(cast_expr.target_type);
			delete cast_expr.expr;
			break;
		case AstNodeKind::ASSIGN_EXPR:
			delete assign.left;
			delete assign.right;
			break;
		case AstNodeKind::MACRO_DEF:
			break;
		default:
			break;
	}
}
inline AstNode*ast_new(AstNodeKind kind,int line,int col){
	return new AstNode(kind,line,col);
}
inline void ast_free(AstNode*node){
	delete node;
}
inline void ast_program_add(AstNode*program,AstNode*node){
	program->program.nodes.push_back(node);
}
inline void ast_block_add(AstNode*block,AstNode*stmt){
	block->block.stmts.push_back(stmt);
}
inline AstNode*ast_new_import(const std::string& path,int line,int col){
	auto*n=new AstNode(AstNodeKind::IMPORT,line,col);
	n->import.path=path;
	return n;
}
inline AstNode*ast_new_var_decl(const std::string& name,MioType*type,AstNode*init,bool is_static,int line,int col){
	auto*n=new AstNode(AstNodeKind::VAR_DECL,line,col);
	n->var_decl.name=name;
	n->var_decl.var_type=type;
	n->var_decl.init=init;
	n->var_decl.is_static=is_static;
	return n;
}
inline AstNode*ast_new_const_decl(const std::string& name,MioType*type,AstNode*init,bool is_static,int line,int col){
	auto*n=new AstNode(AstNodeKind::CONST_DECL,line,col);
	n->const_decl.name=name;
	n->const_decl.var_type=type;
	n->const_decl.init=init;
	n->const_decl.is_static=is_static;
	return n;
}
inline AstNode*ast_new_func_def(const std::string& name,MioType*return_type,AstNode*body,bool is_static,int line,int col){
	auto*n=new AstNode(AstNodeKind::FUNC_DEF,line,col);
	n->func_def.name=name;
	n->func_def.return_type=return_type;
	n->func_def.body=body;
	n->func_def.is_static=is_static;
	n->func_def.is_operator=false;
	return n;
}
inline AstNode*ast_new_struct_def(const std::string& name,int line,int col){
	auto*n=new AstNode(AstNodeKind::STRUCT_DEF,line,col);
	n->struct_def.name=name;
	return n;
}
inline AstNode*ast_new_enum_def(const std::string& name,int line,int col){
	auto*n=new AstNode(AstNodeKind::ENUM_DEF,line,col);
	n->enum_def.name=name;
	return n;
}
inline AstNode*ast_new_union_def(const std::string& name,int line,int col){
	auto*n=new AstNode(AstNodeKind::UNION_DEF,line,col);
	n->union_def.name=name;
	return n;
}
inline AstNode*ast_new_block(int line,int col){
	return new AstNode(AstNodeKind::BLOCK,line,col);
}
inline AstNode*ast_new_if(AstNode*cond,AstNode*then_body,AstNode*else_body,int line,int col){
	auto*n=new AstNode(AstNodeKind::IF_STMT,line,col);
	n->if_stmt.cond=cond;
	n->if_stmt.then_body=then_body;
	n->if_stmt.else_body=else_body;
	return n;
}
inline AstNode*ast_new_while(AstNode*cond,AstNode*body,int line,int col){
	auto*n=new AstNode(AstNodeKind::WHILE_STMT,line,col);
	n->while_stmt.cond=cond;
	n->while_stmt.body=body;
	return n;
}
inline AstNode*ast_new_for(AstNode*init,AstNode*cond,AstNode*update,AstNode*body,int line,int col){
	auto*n=new AstNode(AstNodeKind::FOR_STMT,line,col);
	n->for_stmt.init=init;
	n->for_stmt.cond=cond;
	n->for_stmt.update=update;
	n->for_stmt.body=body;
	return n;
}
inline AstNode*ast_new_break(int line,int col){
	return new AstNode(AstNodeKind::BREAK_STMT,line,col);
}
inline AstNode*ast_new_continue(int line,int col){
	return new AstNode(AstNodeKind::CONTINUE_STMT,line,col);
}
inline AstNode*ast_new_goto(const std::string& label,int line,int col){
	auto*n=new AstNode(AstNodeKind::GOTO_STMT,line,col);
	n->goto_stmt.label=label;
	return n;
}
inline AstNode*ast_new_label(const std::string& label,int line,int col){
	auto*n=new AstNode(AstNodeKind::LABEL_STMT,line,col);
	n->label_stmt.label=label;
	return n;
}
inline AstNode*ast_new_return(AstNode*value,int line,int col){
	auto*n=new AstNode(AstNodeKind::RETURN_STMT,line,col);
	n->return_stmt.value=value;
	return n;
}
inline AstNode*ast_new_expr_stmt(AstNode*expr,int line,int col){
	auto*n=new AstNode(AstNodeKind::EXPR_STMT,line,col);
	n->expr_stmt.expr=expr;
	return n;
}
inline AstNode*ast_new_binary(AstNode*left,TokenKind op,AstNode*right,int line,int col){
	auto*n=new AstNode(AstNodeKind::BINARY_EXPR,line,col);
	n->binary.left=left;
	n->binary.op=op;
	n->binary.right=right;
	return n;
}
inline AstNode*ast_new_unary(TokenKind op,AstNode*operand,int line,int col){
	auto*n=new AstNode(AstNodeKind::UNARY_EXPR,line,col);
	n->unary.op=op;
	n->unary.operand=operand;
	return n;
}
inline AstNode*ast_new_call(AstNode*callee,int line,int col){
	auto*n=new AstNode(AstNodeKind::CALL_EXPR,line,col);
	n->call.callee=callee;
	return n;
}
inline AstNode*ast_new_index(AstNode*base,AstNode*index,int line,int col){
	auto*n=new AstNode(AstNodeKind::INDEX_EXPR,line,col);
	n->index_expr.base=base;
	n->index_expr.index=index;
	return n;
}
inline AstNode*ast_new_member(AstNode*base,const std::string& member,bool arrow,int line,int col){
	auto*n=new AstNode(AstNodeKind::MEMBER_EXPR,line,col);
	n->member.base=base;
	n->member.member=member;
	n->member.arrow=arrow;
	return n;
}
inline AstNode*ast_new_ident(const std::string& name,int line,int col){
	auto*n=new AstNode(AstNodeKind::IDENT_EXPR,line,col);
	n->ident.name=name;
	return n;
}
inline AstNode*ast_new_int_lit(int64_t value,int line,int col){
	auto*n=new AstNode(AstNodeKind::INT_LIT,line,col);
	n->int_lit.value=value;
	return n;
}
inline AstNode*ast_new_float_lit(double value,int line,int col){
	auto*n=new AstNode(AstNodeKind::FLOAT_LIT,line,col);
	n->float_lit.value=value;
	return n;
}
inline AstNode*ast_new_string_lit(const std::string& value,int line,int col){
	auto*n=new AstNode(AstNodeKind::STRING_LIT,line,col);
	n->string_lit.value=value;
	return n;
}
inline AstNode*ast_new_bool_lit(bool value,int line,int col){
	auto*n=new AstNode(AstNodeKind::BOOL_LIT,line,col);
	n->bool_lit.value=value;
	return n;
}
inline AstNode*ast_new_char_lit(char value,int line,int col){
	auto*n=new AstNode(AstNodeKind::CHAR_LIT,line,col);
	n->char_lit.value=value;
	return n;
}
inline AstNode*ast_new_array_lit(int line,int col){
	return new AstNode(AstNodeKind::ARRAY_LIT,line,col);
}
inline AstNode*ast_new_cast(MioType*type,AstNode*expr,int line,int col){
	auto*n=new AstNode(AstNodeKind::CAST_EXPR,line,col);
	n->cast_expr.target_type=type;
	n->cast_expr.expr=expr;
	return n;
}
inline AstNode*ast_new_assign(AstNode*left,AstNode*right,int line,int col){
	auto*n=new AstNode(AstNodeKind::ASSIGN_EXPR,line,col);
	n->assign.left=left;
	n->assign.right=right;
	return n;
}
inline AstNode*ast_new_macro_def(const std::string& name,const std::string& value,int line,int col){
	auto*n=new AstNode(AstNodeKind::MACRO_DEF,line,col);
	n->macro_def.name=name;
	n->macro_def.value=value.empty()?"1":value;
	return n;
}
inline void ast_call_add_arg(AstNode*call,AstNode*arg){
	call->call.args.push_back(arg);
}
inline void ast_array_add(AstNode*array,AstNode*elem){
	array->array_lit.elements.push_back(elem);
}
inline void ast_struct_add_field(AstNode*s,const std::string& name,MioType*type,AstNode*init){
	Field f{name,type,init};
	s->struct_def.fields.push_back(f);
}
inline void ast_struct_add_method(AstNode*s,AstNode*method){
	s->struct_def.methods.push_back(method);
}
inline void ast_enum_add_variant(AstNode*e,const std::string& name,AstNode*init){
	Variant v{name,init};
	e->enum_def.variants.push_back(v);
}
inline void ast_union_add_field(AstNode*u,const std::string& name,MioType*type){
	UnionField f{name,type};
	u->union_def.fields.push_back(f);
}
inline void ast_func_add_param(AstNode*func,const std::string& name,MioType*type){
	Param p{name,type};
	func->func_def.params.push_back(p);
}
inline void ast_func_add_init(AstNode*func,const std::string& field_name,AstNode*expr){
	InitField f{field_name,expr};
	func->func_def.init_list.push_back(f);
}
inline void ast_if_add_elif(AstNode*if_stmt,AstNode*cond,AstNode*body){
	if_stmt->if_stmt.elif_list.push_back(cond);
	if_stmt->if_stmt.elif_list.push_back(body);
}
#endif