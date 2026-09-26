#ifndef MIO_SEMANTIC_HPP
#define MIO_SEMANTIC_HPP
#include"ast.hpp"
#include"types.hpp"
#include<string>
#include<vector>
#include<unordered_map>
#include<unordered_set>

extern int g_error_count;

class AstCloner{
public:
	std::unordered_map<std::string,MioType*>* typeSubst;
	std::unordered_map<std::string,AstNode*>* exprSubst;
	const std::string* fn;
	AstCloner(): typeSubst(nullptr),exprSubst(nullptr),fn(nullptr){}
	
	MioType* cloneType(MioType* original){
		if(!original) return nullptr;
		if(original->kind == MioTypeKind::CLASS && !original->name.empty() && typeSubst){
			auto it=typeSubst->find(original->name);
			if(it != typeSubst->end()){
				MioType* sub=it->second;
				if(sub->kind == MioTypeKind::RVALUE_REFERENCE){
					MioType* base=sub->base_type;
					if(base && (base->kind == MioTypeKind::REFERENCE || base->kind == MioTypeKind::RVALUE_REFERENCE)){
						return mio_type_new_reference(base,false);
					}
				}
				return new MioType(*sub);
			}
		}
		MioType* cloned=new MioType(*original);
		cloned->base_type=nullptr;
		cloned->param_types.clear();
		if(original->base_type){
			cloned->base_type=cloneType(original->base_type);
		}
		for(auto* p : original->param_types){
			cloned->param_types.push_back(cloneType(p));
		}
		return cloned;
	}
	
	AstNode* cloneNode(AstNode* node){
		if(!node) return nullptr;
		switch(node->kind){
			case AstNodeKind::VAR_DECL:{
				auto* cloned=new AstNode(AstNodeKind::VAR_DECL,node->line,node->col,fn);
				cloned->var_decl.name=node->var_decl.name;
				cloned->var_decl.var_type=cloneType(node->var_decl.var_type);
				cloned->var_decl.is_static=node->var_decl.is_static;
				cloned->var_decl.init=cloneNode(node->var_decl.init);
				return cloned;
			}
			case AstNodeKind::CONST_DECL:{
				auto* cloned=new AstNode(AstNodeKind::CONST_DECL,node->line,node->col,fn);
				cloned->const_decl.name=node->const_decl.name;
				cloned->const_decl.var_type=cloneType(node->const_decl.var_type);
				cloned->const_decl.is_static=node->const_decl.is_static;
				cloned->const_decl.init=cloneNode(node->const_decl.init);
				return cloned;
			}
			case AstNodeKind::EXPR_STMT:{
				auto* cloned=new AstNode(AstNodeKind::EXPR_STMT,node->line,node->col,fn);
				cloned->expr_stmt.expr=cloneNode(node->expr_stmt.expr);
				return cloned;
			}
			case AstNodeKind::RETURN_STMT:{
				auto* cloned=new AstNode(AstNodeKind::RETURN_STMT,node->line,node->col,fn);
				cloned->return_stmt.value=cloneNode(node->return_stmt.value);
				return cloned;
			}
			case AstNodeKind::BLOCK:{
				auto* cloned=new AstNode(AstNodeKind::BLOCK,node->line,node->col,fn);
				cloned->block.is_scope=node->block.is_scope;
				for(auto* stmt : node->block.stmts){
					cloned->block.stmts.push_back(cloneNode(stmt));
				}
				return cloned;
			}
			case AstNodeKind::IF_STMT:{
				auto* cloned=new AstNode(AstNodeKind::IF_STMT,node->line,node->col,fn);
				cloned->if_stmt.cond=cloneNode(node->if_stmt.cond);
				cloned->if_stmt.then_body=cloneNode(node->if_stmt.then_body);
				cloned->if_stmt.else_body=cloneNode(node->if_stmt.else_body);
				return cloned;
			}
			case AstNodeKind::WHILE_STMT:{
				auto* cloned=new AstNode(AstNodeKind::WHILE_STMT,node->line,node->col,fn);
				cloned->while_stmt.cond=cloneNode(node->while_stmt.cond);
				cloned->while_stmt.body=cloneNode(node->while_stmt.body);
				return cloned;
			}
			case AstNodeKind::FOR_STMT:{
				auto* cloned=new AstNode(AstNodeKind::FOR_STMT,node->line,node->col,fn);
				cloned->for_stmt.init=cloneNode(node->for_stmt.init);
				cloned->for_stmt.cond=cloneNode(node->for_stmt.cond);
				cloned->for_stmt.update=cloneNode(node->for_stmt.update);
				cloned->for_stmt.body=cloneNode(node->for_stmt.body);
				return cloned;
			}
			case AstNodeKind::BINARY_EXPR:{
				auto* cloned=new AstNode(AstNodeKind::BINARY_EXPR,node->line,node->col,fn);
				cloned->binary.op=node->binary.op;
				cloned->binary.left=cloneNode(node->binary.left);
				cloned->binary.right=cloneNode(node->binary.right);
				return cloned;
			}
			case AstNodeKind::UNARY_EXPR:{
				auto* cloned=new AstNode(AstNodeKind::UNARY_EXPR,node->line,node->col,fn);
				cloned->unary.op=node->unary.op;
				cloned->unary.operand=cloneNode(node->unary.operand);
				return cloned;
			}
			case AstNodeKind::IDENT_EXPR:{
			if(exprSubst){
				auto it=exprSubst->find(node->ident.name);
				if(it!=exprSubst->end()){
					return cloneNode(it->second);
				}
			}
			auto* cloned=new AstNode(AstNodeKind::IDENT_EXPR,node->line,node->col,fn);
			cloned->ident.name=node->ident.name;
			cloned->ident.namespace_name=node->ident.namespace_name;
			return cloned;
		}
		case AstNodeKind::INT_LIT:{
			auto* cloned=new AstNode(AstNodeKind::INT_LIT,node->line,node->col,fn);
			cloned->int_lit.value=node->int_lit.value;
			return cloned;
		}
		case AstNodeKind::FLOAT_LIT:{
			auto* cloned=new AstNode(AstNodeKind::FLOAT_LIT,node->line,node->col,fn);
			cloned->float_lit.value=node->float_lit.value;
			return cloned;
		}
		case AstNodeKind::STRING_LIT:{
			auto* cloned=new AstNode(AstNodeKind::STRING_LIT,node->line,node->col,fn);
			cloned->string_lit.value=node->string_lit.value;
			return cloned;
		}
		case AstNodeKind::BOOL_LIT:{
			auto* cloned=new AstNode(AstNodeKind::BOOL_LIT,node->line,node->col,fn);
			cloned->bool_lit.value=node->bool_lit.value;
			return cloned;
		}
			case AstNodeKind::CALL_EXPR:{
				auto* cloned=new AstNode(AstNodeKind::CALL_EXPR,node->line,node->col,fn);
				cloned->call.callee=cloneNode(node->call.callee);
				for(auto* arg : node->call.args){
					cloned->call.args.push_back(cloneNode(arg));
				}
				return cloned;
			}
			case AstNodeKind::INDEX_EXPR:{
				auto* cloned=new AstNode(AstNodeKind::INDEX_EXPR,node->line,node->col,fn);
				cloned->index_expr.base=cloneNode(node->index_expr.base);
				cloned->index_expr.index=cloneNode(node->index_expr.index);
				return cloned;
			}
			case AstNodeKind::MEMBER_EXPR:{
			auto* cloned=new AstNode(AstNodeKind::MEMBER_EXPR,node->line,node->col,fn);
			cloned->member.base=cloneNode(node->member.base);
			cloned->member.member=node->member.member;
			cloned->member.arrow=node->member.arrow;
			return cloned;
		}
			case AstNodeKind::ASSIGN_EXPR:{
				auto* cloned=new AstNode(AstNodeKind::ASSIGN_EXPR,node->line,node->col,fn);
				cloned->assign.left=cloneNode(node->assign.left);
				cloned->assign.right=cloneNode(node->assign.right);
				cloned->assign.op=node->assign.op;
				return cloned;
			}
			case AstNodeKind::CAST_EXPR:{
			auto* cloned=new AstNode(AstNodeKind::CAST_EXPR,node->line,node->col,fn);
			cloned->cast_expr.target_type=cloneType(node->cast_expr.target_type);
			cloned->cast_expr.expr=cloneNode(node->cast_expr.expr);
			return cloned;
		}
			case AstNodeKind::SIZEOF_EXPR:{
				auto* cloned=new AstNode(AstNodeKind::SIZEOF_EXPR,node->line,node->col,fn);
				cloned->sizeof_expr.target_type=cloneType(node->sizeof_expr.target_type);
				return cloned;
			}
			case AstNodeKind::ARRAY_LIT:{
				auto* cloned=new AstNode(AstNodeKind::ARRAY_LIT,node->line,node->col,fn);
				for(auto* elem : node->array_lit.elements){
					cloned->array_lit.elements.push_back(cloneNode(elem));
				}
				return cloned;
			}
			case AstNodeKind::LITERAL_OP_EXPR:{
				auto* cloned=new AstNode(AstNodeKind::LITERAL_OP_EXPR,node->line,node->col,fn);
				cloned->literal_op.operand=cloneNode(node->literal_op.operand);
				cloned->literal_op.suffix=node->literal_op.suffix;
				return cloned;
			}
			case AstNodeKind::BREAK_STMT:
			case AstNodeKind::CONTINUE_STMT:
				return new AstNode(node->kind,node->line,node->col,fn);
			default:
				return node;
		}
	}
};

inline std::string ast_value_str(AstNode* e){
	if(!e) return "";
	switch(e->kind){
		case AstNodeKind::INT_LIT: return std::to_string(e->int_lit.value);
		case AstNodeKind::FLOAT_LIT: return std::to_string(e->float_lit.value);
		case AstNodeKind::STRING_LIT: return e->string_lit.value;
		case AstNodeKind::CHAR_LIT: return std::string(1,e->char_lit.value);
		case AstNodeKind::BOOL_LIT: return e->bool_lit.value?"true":"false";
		case AstNodeKind::IDENT_EXPR: return e->ident.name;
		default: return "?";
	}
}

class SemanticAnalyzer{
public:
	SemanticAnalyzer(){}
	
	void analyze(AstNode* prog){
		if(!prog) return;
		currentProgram=prog;
		registerPass(prog);
		analyzeProgram(prog);
		while(!pendingFuncInstantiations.empty()){
			auto pending=std::move(pendingFuncInstantiations);
			pendingFuncInstantiations.clear();
			for(auto* inst:pending){
				prog->program.nodes.push_back(inst);
				funcDefMap[inst->func_def.name]=inst;
				analyzeFuncDef(inst);
			}
		}
	}
	
	void registerPass(AstNode* prog){
		for(auto* node:prog->program.nodes){
			registerDecl(node);
		}
	}
	
	void registerDecl(AstNode* node){
		if(!node) return;
		switch(node->kind){
			case AstNodeKind::IMPORT:{
				for(auto* stmt:node->block.stmts){
					registerDecl(stmt);
				}
				break;
			}
			case AstNodeKind::BLOCK:{
				for(auto* stmt:node->block.stmts){
					registerDecl(stmt);
				}
				break;
			}
			case AstNodeKind::NAMESPACE_IMPORT:{
				importedNamespaces.insert(node->namespace_import.namespace_name);
				break;
			}
			case AstNodeKind::NAMESPACE_DEF:{
				std::string savedNs=currentNamespace;
				if(!currentNamespace.empty())
					currentNamespace=currentNamespace+"::"+node->namespace_def.name;
				else
					currentNamespace=node->namespace_def.name;
				for(auto* decl:node->namespace_def.body){
					registerDecl(decl);
				}
				currentNamespace=savedNs;
				break;
			}
			case AstNodeKind::TEMPLATE_DEF:{
				if(node->template_def.def->kind==AstNodeKind::FUNC_DEF){
					std::string name=node->template_def.def->func_def.name;
					if(!currentNamespace.empty())
						name=currentNamespace+"::"+name;
					if(templateMap.count(name)){
						error(node,"redefinition of template '"+name+"'");
						return;
					}
					templateMap[name]={node->template_def.type_params,node->template_def.def};
				}else if(node->template_def.def->kind==AstNodeKind::CLASS_DEF){
					std::string name=node->template_def.def->class_def.name;
					if(!currentNamespace.empty())
						name=currentNamespace+"::"+name;
					for(auto& existing:classTemplateMap[name]){
						if(existing.first.size()==node->template_def.type_params.size()){
							error(node,"redefinition of class template '"+name+"'");
							return;
						}
					}
					classTemplateMap[name].push_back({node->template_def.type_params,node->template_def.def});
				}else{
					error(node,"template only supports functions and classes");
				}
				break;
			}
			case AstNodeKind::CLASS_DEF:{
				std::string name=node->class_def.name;
				if(!currentNamespace.empty())
					name=currentNamespace+"::"+name;
				if(classTypes.count(name)){
					error(node,"redefinition of class '"+name+"'");
					return;
				}
				classTypes.insert(name);
				if(!node->class_def.base_name.empty()){
					std::string baseName=resolveClassName(node->class_def.base_name);
					classBaseMap[name]=baseName;
				}
				for(auto& f:node->class_def.fields){
					classFields[name].insert(f.name);
					classFieldTypes[name][f.name]=mio_type_clone(f.type);
				}
				if(!node->class_def.base_name.empty()){
					std::string baseName=classBaseMap[name];
					auto bit=classFields.find(baseName);
					if(bit!=classFields.end()){
						for(auto& fn:bit->second)
							classFields[name].insert(fn);
					}
					auto bft=classFieldTypes.find(baseName);
					if(bft!=classFieldTypes.end()){
						for(auto& [fn,ft]:bft->second)
							classFieldTypes[name][fn]=mio_type_clone(ft);
					}
				}
				for(auto* m:node->class_def.methods){
				std::string mname=m->func_def.name;
				classMethodSet[name].insert(mname);
				if(m->func_def.is_pure_virtual){
					classPureVirtuals[name].insert(mname);
				}
				std::string fullName=name+"::"+mname;
				if(!m->func_def.is_operator&&funcDecls.count(fullName)){
					error(node,"redefinition of method '"+fullName+"'");
					return;
				}
				funcDecls.insert(fullName);
				funcDefMap[fullName]=m;
				if(m->func_def.is_operator){
					std::string mangledFullName=fullName+"(";
					for(size_t i=0;i<m->func_def.params.size();i++){
						if(i>0)mangledFullName+=",";
						mangledFullName+=resolveClassName(mio_type_str(m->func_def.params[i].type));
					}
					mangledFullName+=")";
					funcDecls.insert(mangledFullName);
				funcDefMap[mangledFullName]=m;
				}
				if(m->func_def.is_literal_operator){
					if(m->func_def.params.size()!=1){
						error(node,"literal operator must have exactly one parameter");
						return;
					}
					MioType* pt=m->func_def.params[0].type;
					bool valid=false;
					if(pt->kind==MioTypeKind::U64||pt->kind==MioTypeKind::F64||
						pt->kind==MioTypeKind::CHAR)valid=true;
					if(pt->kind==MioTypeKind::POINTER&&pt->base_type&&
						pt->base_type->kind==MioTypeKind::CHAR)valid=true;
					if(!valid){
						error(node,"literal operator parameter must be u64, f64, char, or const *char");
						return;
					}
					literalOperatorMap[m->func_def.literal_suffix].push_back(fullName);
				}
			}
				for(auto* c:node->class_def.constructors){
				std::string ctorName=name+"::"+node->class_def.name;
				int paramCount=(int)c->func_def.params.size();
				std::string sig;
				for(size_t pi=0;pi<c->func_def.params.size();pi++){
					if(pi>0)sig+=",";
					sig+=mio_type_str(c->func_def.params[pi].type);
				}
				auto it=classConstructorSigs.find(name);
				if(it!=classConstructorSigs.end()){
					for(auto& cs:it->second){
						if(cs.second==sig){
							error(node,"ambiguous constructor in class '"+name+"': multiple constructors with same signature");
							return;
						}
					}
				}
				classConstructorSigs[name].push_back({ctorName,sig});
				funcDecls.insert(ctorName);
				funcDefMap[ctorName]=c;
				if(!sig.empty()){
					std::string mangledCtor=ctorName+"("+sig+")";
					funcDecls.insert(mangledCtor);
					funcDefMap[mangledCtor]=c;
				}
			}
				for(auto* nc:node->class_def.nested_classes){
					if(nc->kind==AstNodeKind::TYPE_ALIAS){
						typeAliases[name+"::"+nc->type_alias.name]=mio_type_clone(nc->type_alias.aliased_type);
					}
				}
				break;
			}
			case AstNodeKind::TYPE_ALIAS:{
				std::string name=node->type_alias.name;
				if(!currentNamespace.empty())
					name=currentNamespace+"::"+name;
				if(typeAliases.count(name)){
					error(node,"redefinition of type alias '"+name+"'");
					return;
				}
				typeAliases[name]=mio_type_clone(node->type_alias.aliased_type);
				break;
			}
			case AstNodeKind::FUNC_DEF:{
				std::string name=node->func_def.name;
				if(!currentNamespace.empty())
					name=currentNamespace+"::"+name;
				std::string mangledName=name;
				if(node->func_def.is_literal_operator){
					mangledName+="(";
					for(size_t i=0;i<node->func_def.params.size();i++){
						if(i>0)mangledName+=",";
						mangledName+=resolveClassName(mio_type_str(node->func_def.params[i].type));
					}
					mangledName+=")";
				}
				if(funcDecls.count(mangledName)){
					error(node,"redefinition of function '"+mangledName+"'");
					return;
				}
				funcDecls.insert(mangledName);
				funcDefMap[mangledName]=node;
				if(mangledName!=name){
					funcDecls.insert(name);
					funcDefMap[name]=node;
				}
				if(node->func_def.is_literal_operator){
					if(node->func_def.params.size()!=1){
						error(node,"literal operator must have exactly one parameter");
						return;
					}
					MioType* pt=node->func_def.params[0].type;
					bool valid=false;
					if(pt->kind==MioTypeKind::U64||pt->kind==MioTypeKind::F64||
						pt->kind==MioTypeKind::CHAR)valid=true;
					if(pt->kind==MioTypeKind::POINTER&&pt->base_type&&
						pt->base_type->kind==MioTypeKind::CHAR)valid=true;
					if(!valid){
						error(node,"literal operator parameter must be u64, f64, char, or const *char");
						return;
					}
					literalOperatorMap[node->func_def.literal_suffix].push_back(mangledName);
				}
				break;
			}
			case AstNodeKind::VAR_DECL:{
				std::string name=node->var_decl.name;
				if(!currentNamespace.empty())
					name=currentNamespace+"::"+name;
				if(varDecls.count(name)){
					error(node,"redefinition of variable '"+name+"'");
					return;
				}
				varDecls.insert(name);
				if(node->var_decl.var_type){
					globalMioTypes[name]=mio_type_clone(node->var_decl.var_type);
				}else{
					error(node,"variable '"+name+"' has no type");
					globalMioTypes[name]=mio_type_new(MioTypeKind::I32);
				}
				break;
			}
			case AstNodeKind::CONST_DECL:{
				std::string name=node->const_decl.name;
				if(!currentNamespace.empty())
					name=currentNamespace+"::"+name;
				if(varDecls.count(name)){
					error(node,"redefinition of constant '"+name+"'");
					return;
				}
				varDecls.insert(name);
				if(node->const_decl.var_type){
					globalMioTypes[name]=mio_type_clone(node->const_decl.var_type);
				}else{
					error(node,"constant '"+name+"' has no type");
					globalMioTypes[name]=mio_type_new(MioTypeKind::I32);
				}
				break;
			}
			case AstNodeKind::ENUM_DEF:{
				std::string name=node->enum_def.name;
				if(!currentNamespace.empty())
					name=currentNamespace+"::"+name;
				if(enumNames.count(name)){
					error(node,"redefinition of enum '"+name+"'");
					return;
				}
				enumNames.insert(name);
				for(auto& v:node->enum_def.variants){
					enumVariantMap[v.name]=name;
				}
				break;
			}
			case AstNodeKind::UNION_DEF:{
				std::string name=node->union_def.name;
				if(!currentNamespace.empty())
					name=currentNamespace+"::"+name;
				if(unionNames.count(name)){
					error(node,"redefinition of union '"+name+"'");
					return;
				}
				unionNames.insert(name);
				for(auto& f:node->union_def.fields){
					unionFields[name].insert(f.name);
				}
				break;
			}
			default: break;
		}
	}
	bool hasErrors() const { return g_error_count>0; }
	
	std::unordered_map<std::string,AstNode*> instantiatedClasses;
	std::unordered_set<std::string> instantiatedClassNames;
	std::unordered_set<std::string> classTypes;
	std::unordered_map<std::string,MioType*> typeAliases;
	std::unordered_set<std::string> funcDecls;
	std::unordered_map<std::string,std::vector<std::string>>literalOperatorMap;
	std::unordered_map<std::string,AstNode*> funcDefMap;
	std::unordered_set<std::string> varDecls;
	std::unordered_map<std::string,MioType*> globalMioTypes;
	std::unordered_set<std::string> enumNames;
	std::unordered_set<std::string> unionNames;
	std::unordered_map<std::string,std::string> enumVariantMap;
	std::unordered_map<std::string,std::vector<std::pair<std::vector<TemplateParam>,AstNode*>>> classTemplateMap;
	std::unordered_map<std::string,std::pair<std::vector<TemplateParam>,AstNode*>> templateMap;
	std::unordered_set<std::string> instantiatedFuncNames;
	std::vector<AstNode*> pendingFuncInstantiations;
	AstNode* currentProgram=nullptr;
	std::unordered_map<std::string,std::unordered_set<std::string>> classFields;
	std::unordered_map<std::string,std::unordered_map<std::string,MioType*>> classFieldTypes;
	std::unordered_map<std::string,std::unordered_set<std::string>> unionFields;
	std::unordered_map<std::string,std::unordered_set<std::string>> classMethodSet;
	std::unordered_map<std::string,std::unordered_set<std::string>> classPureVirtuals;
	std::unordered_map<std::string,std::string> classBaseMap;
	std::unordered_map<std::string,std::vector<std::pair<std::string,std::string>>> classConstructorSigs;
	MioType* currentFuncReturnType=nullptr;
	bool hasReturnStmt=false;
	
private:
	void error(AstNode* node,const std::string& msg){
		const char* fn=node&&node->filename?node->filename->c_str():"";
		fprintf(stderr,"%s:%d:%d: error: %s\n",fn,node?node->line:0,node?node->col:0,msg.c_str());
		g_error_count++;
	}
	
	void analyzeProgram(AstNode* prog){
		for(auto* node:prog->program.nodes){
			analyzeDecl(node);
		}
	}
	
	void analyzeDecl(AstNode* node){
		if(!node) return;
		switch(node->kind){
			case AstNodeKind::IMPORT:{
				for(auto* stmt:node->block.stmts){
					analyzeDecl(stmt);
				}
				break;
			}
			case AstNodeKind::BLOCK:{
				for(auto* stmt:node->block.stmts){
					analyzeDecl(stmt);
				}
				break;
			}
			case AstNodeKind::NAMESPACE_IMPORT:{
				importedNamespaces.insert(node->namespace_import.namespace_name);
				break;
			}
			case AstNodeKind::VAR_DECL: analyzeVarDecl(node); break;
			case AstNodeKind::CONST_DECL: analyzeVarDecl(node); break;
			case AstNodeKind::FUNC_DEF: analyzeFuncDef(node); break;
			case AstNodeKind::CLASS_DEF: analyzeClassDef(node); break;
			case AstNodeKind::TYPE_ALIAS:{
				MioType* aliased=node->type_alias.aliased_type;
				checkType(aliased,node);
				break;
			}
			case AstNodeKind::ENUM_DEF: break;
			case AstNodeKind::UNION_DEF: break;
			case AstNodeKind::TEMPLATE_DEF: break;
			case AstNodeKind::NAMESPACE_DEF:{
				std::string savedNs=currentNamespace;
				if(!currentNamespace.empty())
					currentNamespace=currentNamespace+"::"+node->namespace_def.name;
				else
					currentNamespace=node->namespace_def.name;
				for(auto* decl:node->namespace_def.body){
					analyzeDecl(decl);
				}
				currentNamespace=savedNs;
				break;
			}
			default: break;
		}
	}
	
	void analyzeVarDecl(AstNode* node){
		if(!node) return;
		bool isConst=(node->kind==AstNodeKind::CONST_DECL);
		bool isExtern=isConst?node->const_decl.is_extern:node->var_decl.is_extern;
		MioType* varType=isConst?node->const_decl.var_type:node->var_decl.var_type;
		AstNode* initExpr=isConst?node->const_decl.init:node->var_decl.init;
		std::string varName=isConst?node->const_decl.name:node->var_decl.name;
		if(varType){
			checkType(varType,node);
		}
		if(isExtern){
			if(initExpr){
				error(node,"extern variable '"+varName+"' cannot have an initializer");
			}
			if(!varType){
				error(node,"extern variable '"+varName+"' requires an explicit type");
			}
			return;
		}
		if(initExpr){
			checkExpr(initExpr);
			if(initExpr->kind==AstNodeKind::ARRAY_LIT&&varType){
				initExpr->type=mio_type_clone(varType);
			}
			if(varType){
				MioType* initType=resolveExprMioType(initExpr);
				if(initType&&!isTypeCompatible(varType,initType)){
					error(node,"type mismatch: variable '"+varName+"' declared as '"+mio_type_str(varType)+"', initialized with '"+mio_type_str(initType)+"'");
				}
			}
		}
	}
	
	void analyzeFuncDef(AstNode* node){
		if(!node) return;
		if(node->func_def.return_type){
			checkType(node->func_def.return_type,node);
		}
		auto savedReturnType=currentFuncReturnType;
		currentFuncReturnType=node->func_def.return_type;
		auto savedLocals=locals;
		auto savedMioTypes=localMioTypes;
		auto savedHasReturn=hasReturnStmt;
		auto savedLabels=labels;
		auto savedClassName=currentClassName;
		hasReturnStmt=false;
		labels.clear();
		if(node->func_def.body){
			collectLabels(node->func_def.body);
		}
		if(!node->func_def.class_name.empty()){
			currentClassName=node->func_def.class_name;
			MioType* thisType=mio_type_new_pointer(mio_type_new_named(MioTypeKind::CLASS,node->func_def.class_name));
			locals["this"]=thisType;
			localMioTypes["this"]=thisType;
		}
		for(auto& p:node->func_def.params){
			if(p.type){
				checkType(p.type,node);
				locals[p.name]=p.type;
				localMioTypes[p.name]=p.type;
			}
		}
		bool seenDefault=false;
		for(size_t i=0;i<node->func_def.params.size();i++){
			bool hasDefault=node->func_def.params[i].default_val!=nullptr;
			if(hasDefault){
				seenDefault=true;
			}else if(seenDefault){
				error(node,"parameter '"+node->func_def.params[i].name+"' must have a default value (preceding parameter has one)");
			}
		}
		if(node->func_def.body){
			analyzeBlock(node->func_def.body);
		}
		if(currentFuncReturnType&&currentFuncReturnType->kind!=MioTypeKind::VOID&&!hasReturnStmt&&!node->func_def.is_extern&&node->func_def.body&&node->func_def.name[0]!='~'){
			std::string shortClassName=node->func_def.class_name;
			auto pos=shortClassName.rfind("::");
			if(pos!=std::string::npos) shortClassName=shortClassName.substr(pos+2);
			if(node->func_def.name!=shortClassName){
				error(node->func_def.body,"non-void function '"+node->func_def.name+"' does not return a value");
			}
		}
		locals=savedLocals;
		localMioTypes=savedMioTypes;
		currentFuncReturnType=savedReturnType;
		hasReturnStmt=savedHasReturn;
		labels=savedLabels;
		currentClassName=savedClassName;
	}
	
	void analyzeBlock(AstNode* block){
		if(!block) return;
		if(block->kind==AstNodeKind::BLOCK){
			for(auto* stmt:block->block.stmts){
				analyzeStmt(stmt);
			}
		}else{
			analyzeStmt(block);
		}
	}
	
	void collectLabels(AstNode* block){
		if(!block) return;
		if(block->kind==AstNodeKind::BLOCK){
			for(auto* stmt:block->block.stmts){
				collectLabels(stmt);
			}
		}else if(block->kind==AstNodeKind::LABEL_STMT){
			if(labels.find(block->label_stmt.label)!=labels.end()){
				error(block,"duplicate label '"+block->label_stmt.label+"' in current function");
			}
			labels.insert(block->label_stmt.label);
		}else if(block->kind==AstNodeKind::IF_STMT){
			collectLabels(block->if_stmt.then_body);
			if(block->if_stmt.else_body) collectLabels(block->if_stmt.else_body);
		}else if(block->kind==AstNodeKind::WHILE_STMT){
			collectLabels(block->while_stmt.body);
		}else if(block->kind==AstNodeKind::FOR_STMT){
			collectLabels(block->for_stmt.body);
		}
	}
	
	void analyzeStmt(AstNode* stmt){
		if(!stmt) return;
		switch(stmt->kind){
			case AstNodeKind::VAR_DECL:{
				if(stmt->var_decl.var_type){
					checkType(stmt->var_decl.var_type,stmt);
				}
				if(stmt->var_decl.init){
					checkExpr(stmt->var_decl.init);
					if(stmt->var_decl.init->kind==AstNodeKind::ARRAY_LIT&&stmt->var_decl.var_type){
						stmt->var_decl.init->type=mio_type_clone(stmt->var_decl.var_type);
					}
					if(stmt->var_decl.var_type){
						MioType* initType=resolveExprMioType(stmt->var_decl.init);
						if(initType&&!isTypeCompatible(stmt->var_decl.var_type,initType)){
							error(stmt,"type mismatch: variable '"+stmt->var_decl.name+"' declared as '"+mio_type_str(stmt->var_decl.var_type)+"', initialized with '"+mio_type_str(initType)+"'");
						}
					}
				}
				MioType* inferredType=stmt->var_decl.var_type;
				if(!inferredType&&stmt->var_decl.init){
					MioType* t=resolveExprMioType(stmt->var_decl.init);
					if(t!=stmt->var_decl.init->type){
						inferredType=t;
					}else{
						inferredType=mio_type_clone(t);
					}
					stmt->var_decl.var_type=inferredType;
				}
				if(!inferredType){
					error(stmt,"cannot infer type for variable '"+stmt->var_decl.name+"'");
					inferredType=mio_type_new(MioTypeKind::I32);
					stmt->var_decl.var_type=inferredType;
				}
				locals[stmt->var_decl.name]=inferredType;
				localMioTypes[stmt->var_decl.name]=inferredType;
				break;
			}
			case AstNodeKind::CONST_DECL:{
				if(stmt->const_decl.var_type){
					checkType(stmt->const_decl.var_type,stmt);
				}
				if(stmt->const_decl.init){
					checkExpr(stmt->const_decl.init);
					if(stmt->const_decl.init->kind==AstNodeKind::ARRAY_LIT&&stmt->const_decl.var_type){
						stmt->const_decl.init->type=mio_type_clone(stmt->const_decl.var_type);
					}
					if(stmt->const_decl.var_type){
						MioType* initType=resolveExprMioType(stmt->const_decl.init);
						if(initType&&!isTypeCompatible(stmt->const_decl.var_type,initType)){
							error(stmt,"type mismatch: constant '"+stmt->const_decl.name+"' declared as '"+mio_type_str(stmt->const_decl.var_type)+"', initialized with '"+mio_type_str(initType)+"'");
						}
					}
				}
				MioType* inferredType=stmt->const_decl.var_type;
				if(!inferredType&&stmt->const_decl.init){
					MioType* t=resolveExprMioType(stmt->const_decl.init);
					if(t!=stmt->const_decl.init->type){
						inferredType=t;
					}else{
						inferredType=mio_type_clone(t);
					}
					stmt->const_decl.var_type=inferredType;
				}
				if(!inferredType){
					error(stmt,"cannot infer type for constant '"+stmt->const_decl.name+"'");
					inferredType=mio_type_new(MioTypeKind::I32);
					stmt->const_decl.var_type=inferredType;
				}
				locals[stmt->const_decl.name]=inferredType;
				localMioTypes[stmt->const_decl.name]=inferredType;
				break;
			}
			case AstNodeKind::EXPR_STMT:
				checkExpr(stmt->expr_stmt.expr);
				break;
			case AstNodeKind::IF_STMT:
				checkExpr(stmt->if_stmt.cond);
				analyzeBlock(stmt->if_stmt.then_body);
				if(stmt->if_stmt.else_body) analyzeBlock(stmt->if_stmt.else_body);
				break;
			case AstNodeKind::WHILE_STMT:
				loopDepth++;
				checkExpr(stmt->while_stmt.cond);
				analyzeBlock(stmt->while_stmt.body);
				loopDepth--;
				break;
			case AstNodeKind::FOR_STMT:{
				loopDepth++;
				auto savedLocals=locals;
				auto savedMioTypes=localMioTypes;
				if(stmt->for_stmt.init){
					if(stmt->for_stmt.init->kind==AstNodeKind::VAR_DECL||stmt->for_stmt.init->kind==AstNodeKind::CONST_DECL){
						analyzeDecl(stmt->for_stmt.init);
						if(stmt->for_stmt.init->kind==AstNodeKind::VAR_DECL){
							MioType* inferredType=stmt->for_stmt.init->var_decl.var_type;
							if(!inferredType&&stmt->for_stmt.init->var_decl.init){
								MioType* t=resolveExprMioType(stmt->for_stmt.init->var_decl.init);
								if(t!=stmt->for_stmt.init->var_decl.init->type){
									inferredType=t;
								}else{
									inferredType=mio_type_clone(t);
								}
								stmt->for_stmt.init->var_decl.var_type=inferredType;
							}
							if(!inferredType){
								error(stmt->for_stmt.init,"cannot infer type for variable '"+stmt->for_stmt.init->var_decl.name+"'");
								inferredType=mio_type_new(MioTypeKind::I32);
								stmt->for_stmt.init->var_decl.var_type=inferredType;
							}
							locals[stmt->for_stmt.init->var_decl.name]=inferredType;
							localMioTypes[stmt->for_stmt.init->var_decl.name]=inferredType;
						}else{
							MioType* inferredType=stmt->for_stmt.init->const_decl.var_type;
							if(!inferredType&&stmt->for_stmt.init->const_decl.init){
								MioType* t=resolveExprMioType(stmt->for_stmt.init->const_decl.init);
								if(t!=stmt->for_stmt.init->const_decl.init->type){
									inferredType=t;
								}else{
									inferredType=mio_type_clone(t);
								}
								stmt->for_stmt.init->const_decl.var_type=inferredType;
							}
							if(!inferredType){
								error(stmt->for_stmt.init,"cannot infer type for constant '"+stmt->for_stmt.init->const_decl.name+"'");
								inferredType=mio_type_new(MioTypeKind::I32);
								stmt->for_stmt.init->const_decl.var_type=inferredType;
							}
							locals[stmt->for_stmt.init->const_decl.name]=inferredType;
							localMioTypes[stmt->for_stmt.init->const_decl.name]=inferredType;
						}
					}else
						checkExpr(stmt->for_stmt.init);
				}
				if(stmt->for_stmt.cond) checkExpr(stmt->for_stmt.cond);
				if(stmt->for_stmt.update) checkExpr(stmt->for_stmt.update);
				analyzeBlock(stmt->for_stmt.body);
				locals=savedLocals;
				localMioTypes=savedMioTypes;
				loopDepth--;
				break;
			}
			case AstNodeKind::RETURN_STMT:
				hasReturnStmt=true;
				if(stmt->return_stmt.value){
					checkExpr(stmt->return_stmt.value);
					if(currentFuncReturnType&&currentFuncReturnType->kind==MioTypeKind::VOID){
						error(stmt,"void function cannot return a value");
					}else if(currentFuncReturnType){
						MioType* retType=resolveExprMioType(stmt->return_stmt.value);
						if(retType&&!isTypeCompatible(currentFuncReturnType,retType)){
							error(stmt,"return type mismatch: expected '"+mio_type_str(currentFuncReturnType)+"', got '"+mio_type_str(retType)+"'");
						}
					}
				}else if(currentFuncReturnType&&currentFuncReturnType->kind!=MioTypeKind::VOID){
					error(stmt,"non-void function must return a value");
				}
				break;
			case AstNodeKind::BLOCK:
				analyzeBlock(stmt);
				break;
			case AstNodeKind::GOTO_STMT:
				if(labels.find(stmt->goto_stmt.label)==labels.end()){
					error(stmt,"goto target label '"+stmt->goto_stmt.label+"' not found in current function");
				}
				break;
			case AstNodeKind::LABEL_STMT:
				break;
			case AstNodeKind::BREAK_STMT:
				if(loopDepth==0){
					error(stmt,"break statement not within a loop");
				}
				break;
			case AstNodeKind::CONTINUE_STMT:
				if(loopDepth==0){
					error(stmt,"continue statement not within a loop");
				}
				break;
			case AstNodeKind::TYPE_ALIAS:{
				MioType* aliased=stmt->type_alias.aliased_type;
				checkType(aliased,stmt);
				std::string name=stmt->type_alias.name;
				typeAliases[name]=mio_type_clone(aliased);
				break;
			}
			default:
				break;
		}
	}
	
	void checkCircularDep(const std::string& targetClass,const std::string& currentClass,std::unordered_set<std::string>& visited,AstNode* ctx){
		if(targetClass==currentClass)return;
		if(visited.count(currentClass))return;
		visited.insert(currentClass);
		auto it=classFieldTypes.find(currentClass);
		if(it==classFieldTypes.end())return;
		for(auto& [fname,ftype]:it->second){
			if(!ftype)continue;
			if(ftype->kind==MioTypeKind::POINTER||ftype->kind==MioTypeKind::REFERENCE||ftype->kind==MioTypeKind::RVALUE_REFERENCE)continue;
			if(ftype->kind==MioTypeKind::CLASS&&!ftype->name.empty()){
				std::string resolved=resolveClassName(ftype->name);
				if(resolved==targetClass){
					error(ctx,"circular type dependency: class '"+targetClass+"' contains '"+currentClass+"' which contains '"+targetClass+"'");
					return;
				}
				checkCircularDep(targetClass,resolved,visited,ctx);
			}
		}
	}
	void analyzeClassDef(AstNode* node){
		if(!node) return;
		std::string savedClassName=currentClassName;
		currentClassName=node->class_def.name;
		if(!currentNamespace.empty())
			currentClassName=currentNamespace+"::"+currentClassName;
		for(auto& f:node->class_def.fields){
			if(f.type){
				checkType(f.type,node);
				auto it=classFieldTypes[currentClassName].find(f.name);
				if(it!=classFieldTypes[currentClassName].end()){
					mio_type_free(it->second);
					it->second=mio_type_clone(f.type);
				}
			}
		}
		for(auto* m:node->class_def.methods){
			analyzeFuncDef(m);
		}
		for(auto* c:node->class_def.constructors){
			analyzeFuncDef(c);
		}
		if(node->class_def.destructor){
			analyzeFuncDef(node->class_def.destructor);
		}
		std::string name=node->class_def.name;
		if(!currentNamespace.empty())
			name=currentNamespace+"::"+name;
		for(auto& f:node->class_def.fields){
			if(f.type&&f.type->kind==MioTypeKind::CLASS&&!f.type->name.empty()){
				std::string resolved=resolveClassName(f.type->name);
				if(resolved==name){
					error(node,"class '"+name+"' cannot contain itself by value");
				}else{
					std::unordered_set<std::string> visited;
					checkCircularDep(name,resolved,visited,node);
				}
			}
		}
		if(!node->class_def.base_name.empty()){
			std::string baseName=resolveClassName(node->class_def.base_name);
			auto it=classPureVirtuals.find(baseName);
			if(it!=classPureVirtuals.end()){
				for(auto& pv:it->second){
					bool overridden=false;
					for(auto* m:node->class_def.methods){
						if(m->func_def.name==pv&&m->func_def.is_override){
							overridden=true;
							break;
						}
					}
					if(!overridden){
						error(node,"class '"+name+"' does not override pure virtual method '"+pv+"' from base class '"+baseName+"'");
					}
				}
			}
		}
		for(auto* nc:node->class_def.nested_classes){
			if(nc->kind==AstNodeKind::TYPE_ALIAS){
				checkType(nc->type_alias.aliased_type,node);
			}
		}
		currentClassName=savedClassName;
	}
	
	void checkExpr(AstNode* node){
		if(!node) return;
		switch(node->kind){
			case AstNodeKind::BINARY_EXPR:
				checkBinaryExpr(node);
				break;
			case AstNodeKind::UNARY_EXPR:
				checkUnaryExpr(node);
				break;
			case AstNodeKind::CALL_EXPR:
				checkCallExpr(node);
				break;
			case AstNodeKind::INDEX_EXPR:
				checkIndexExpr(node);
				break;
			case AstNodeKind::MEMBER_EXPR:
				checkMemberExpr(node);
				break;
			case AstNodeKind::ASSIGN_EXPR:
				checkAssignExpr(node);
				break;
			case AstNodeKind::CAST_EXPR:
				checkCastExpr(node);
				break;
			case AstNodeKind::IDENT_EXPR:
				checkIdentExpr(node);
				break;
			case AstNodeKind::STRING_LIT:
				node->type=mio_type_new_pointer(mio_type_new(MioTypeKind::CHAR));
				break;
			case AstNodeKind::SIZEOF_EXPR:
				node->type=mio_type_new(MioTypeKind::USIZE);
				break;
			case AstNodeKind::LITERAL_OP_EXPR:
				checkExpr(node->literal_op.operand);
				resolveLiteralOperator(node);
				break;
			case AstNodeKind::ARRAY_LIT:
				for(auto* e:node->array_lit.elements)
					checkExpr(e);
				if(!node->array_lit.elements.empty()){
					MioType* elemType=resolveExprMioType(node->array_lit.elements[0]);
					if(elemType){
						node->type=mio_type_new_array(elemType,(int)node->array_lit.elements.size());
					}
				}
				break;
			default: break;
		}
	}
	
	void resolveLiteralOperator(AstNode* node){
		std::string suffix=node->literal_op.suffix;
		auto it=literalOperatorMap.find(suffix);
		if(it==literalOperatorMap.end()){
			error(node,"no literal operator found for suffix '"+suffix+"'");
			return;
		}
		AstNode* operand=node->literal_op.operand;
		MioTypeKind targetKind;
		bool useStringMatch=false;
		switch(operand->kind){
			case AstNodeKind::INT_LIT:
				targetKind=MioTypeKind::U64;
				break;
			case AstNodeKind::FLOAT_LIT:
				targetKind=MioTypeKind::F64;
				break;
			case AstNodeKind::CHAR_LIT:
				targetKind=MioTypeKind::CHAR;
				break;
			case AstNodeKind::STRING_LIT:
				useStringMatch=true;
				break;
			default:
				error(node,"literal operator suffix can only be applied to literals");
				return;
		}
		AstNode* chosen=nullptr;
		if(!useStringMatch){
			for(std::string& name:it->second){
				auto fit=funcDefMap.find(name);
				if(fit==funcDefMap.end())continue;
				AstNode* fn=fit->second;
				if(fn->func_def.params.size()!=1)continue;
				MioType* paramType=fn->func_def.params[0].type;
				if(paramType->kind==targetKind){
					chosen=fn;
					break;
				}
			}
		}
		if(!chosen){
			for(std::string& name:it->second){
				auto fit=funcDefMap.find(name);
				if(fit==funcDefMap.end())continue;
				AstNode* fn=fit->second;
				if(fn->func_def.params.size()!=1)continue;
				MioType* paramType=fn->func_def.params[0].type;
				if(paramType->kind==MioTypeKind::POINTER&&paramType->base_type&&paramType->base_type->kind==MioTypeKind::CHAR){
					chosen=fn;
					break;
				}
			}
		}
		if(!chosen){
			error(node,"no matching literal operator for suffix '"+suffix+"'");
			return;
		}
		node->literal_op.resolved_func=chosen;
		std::string mangled=chosen->func_def.name;
		mangled+="(";
		for(size_t i=0;i<chosen->func_def.params.size();i++){
			if(i>0)mangled+=",";
			mangled+=resolveClassName(mio_type_str(chosen->func_def.params[i].type));
		}
		mangled+=")";
		node->literal_op.resolved_mangled_name=mangled;
		node->type=mio_type_clone(chosen->func_def.return_type);
	}
	void checkBinaryExpr(AstNode* node){
		if(!node||!node->binary.left||!node->binary.right) return;
		checkExpr(node->binary.left);
		checkExpr(node->binary.right);
		MioType* lmt=resolveExprMioType(node->binary.left);
		MioType* rmt=resolveExprMioType(node->binary.right);
		std::string className;
		if(lmt){
			if(lmt->kind==MioTypeKind::CLASS&&!lmt->name.empty())
				className=resolveClassName(lmt->name);
		}
		if(!className.empty()){
			std::string opMethod=findOperatorMethod(className,node->binary.op,rmt,node);
			if(opMethod.empty()){
				error(node,"class '"+className+"' does not support operator"+tok_name(node->binary.op));
				node->type=mio_type_new(MioTypeKind::BOOL);
			}
			node->binary.resolved_op_method=opMethod;
			if(!opMethod.empty()){
				auto defIt=funcDefMap.find(opMethod);
				if(defIt!=funcDefMap.end()&&defIt->second->kind==AstNodeKind::FUNC_DEF&&defIt->second->func_def.return_type){
					node->type=mio_type_clone(defIt->second->func_def.return_type);
				}
			}
		}
		switch(node->binary.op){
			case TOK_PLUS:
			case TOK_MINUS:{
				if(lmt&&lmt->kind==MioTypeKind::POINTER&&lmt->base_type&&lmt->base_type->kind==MioTypeKind::VOID){
					error(node,"cannot perform pointer arithmetic on void*");
					return;
				}
				if(rmt&&rmt->kind==MioTypeKind::POINTER&&rmt->base_type&&rmt->base_type->kind==MioTypeKind::VOID&&node->binary.op==TOK_PLUS){
					error(node,"cannot perform pointer arithmetic on void*");
					return;
				}
				break;
			}
			default: break;
		}
		if(!node->type&&lmt)node->type=mio_type_clone(lmt);
	}
	
	void checkUnaryExpr(AstNode* node){
		if(!node||!node->unary.operand) return;
		checkExpr(node->unary.operand);
		MioType* operandType=resolveExprMioType(node->unary.operand);
		switch(node->unary.op){
			case TOK_STAR:{
				if(!operandType){
					error(node,"cannot dereference expression of unknown type");
					return;
				}
				if(operandType->kind!=MioTypeKind::POINTER&&operandType->kind!=MioTypeKind::REFERENCE&&operandType->kind!=MioTypeKind::RVALUE_REFERENCE){
					error(node,"cannot dereference non-pointer type '"+mio_type_str(operandType)+"'");
					return;
				}
				if(operandType->kind==MioTypeKind::POINTER&&operandType->base_type&&operandType->base_type->kind==MioTypeKind::VOID){
					error(node,"cannot dereference pointer to void");
					return;
				}
				break;
			}
			case TOK_BIT_AND:{
				if(node->unary.operand->kind!=AstNodeKind::IDENT_EXPR&&node->unary.operand->kind!=AstNodeKind::MEMBER_EXPR&&node->unary.operand->kind!=AstNodeKind::INDEX_EXPR&&node->unary.operand->kind!=AstNodeKind::UNARY_EXPR){
					error(node,"cannot take address of non-lvalue expression");
					return;
				}
				if(node->unary.operand->kind==AstNodeKind::UNARY_EXPR&&node->unary.operand->unary.op!=TOK_STAR){
					error(node,"cannot take address of non-lvalue expression");
					return;
				}
				break;
			}
			default: break;
		}
		switch(node->unary.op){
			case TOK_NOT:
				node->type=mio_type_new(MioTypeKind::BOOL);
				break;
			case TOK_MINUS:
				if(operandType) node->type=mio_type_clone(operandType);
				break;
			case TOK_STAR:
				if(operandType&&operandType->kind==MioTypeKind::POINTER&&operandType->base_type)
					node->type=mio_type_clone(operandType->base_type);
				break;
			case TOK_BIT_AND:
				if(operandType) node->type=mio_type_new_pointer(operandType);
				break;
			default:
				if(operandType) node->type=mio_type_clone(operandType);
				break;
		}
	}
	
	void checkFuncPtrCall(AstNode* node,const std::string& calleeName,MioType* funcType){
		if(funcType->param_types.size()!=node->call.args.size()){
			error(node,"function pointer call argument count mismatch: expected "+std::to_string(funcType->param_types.size())+", got "+std::to_string(node->call.args.size()));
			return;
		}
		for(size_t i=0;i<node->call.args.size();i++){
			MioType* argType=resolveExprMioType(node->call.args[i]);
			if(argType&&!isTypeCompatible(funcType->param_types[i],argType)){
				error(node,"function pointer call argument "+std::to_string(i+1)+" type mismatch: expected '"+mio_type_str(funcType->param_types[i])+"', got '"+mio_type_str(argType)+"'");
			}
		}
		node->type=mio_type_clone(funcType->base_type);
	}
	
	void checkCallExpr(AstNode* node){
		if(!node||!node->call.callee) return;
		checkExpr(node->call.callee);
		for(auto* arg:node->call.args)
			checkExpr(arg);
		if(node->call.callee->kind==AstNodeKind::IDENT_EXPR){
			std::string calleeName=node->call.callee->ident.name;
			std::string ns=node->call.callee->ident.namespace_name;
			if(!ns.empty()&&ns!="::")
				calleeName=ns+"::"+calleeName;
			if(locals.count(calleeName)){
				MioType* varType=locals[calleeName];
				MioType* unwrapped=varType;
				while(unwrapped&&unwrapped->kind==MioTypeKind::REFERENCE)unwrapped=unwrapped->base_type;
				std::string className;
				if(unwrapped&&unwrapped->kind==MioTypeKind::CLASS&&!unwrapped->name.empty())
					className=resolveClassName(unwrapped->name);
				if(!className.empty()){
					std::string opName="operator()";
					std::string opBase=className+"::"+opName;
					std::string opMangled=opBase+"(";
					for(size_t i=0;i<node->call.args.size();i++){
						if(i>0)opMangled+=",";
						MioType* at=resolveExprMioType(node->call.args[i]);
						std::string atStr=at?mio_type_str(at):"";
						opMangled+=atStr;
					}
					opMangled+=")";
					std::string foundOp;
					if(funcDecls.count(opMangled))foundOp=opMangled;
					std::string shortClassName=className;
					{
						auto pos=shortClassName.rfind("::");
						if(pos!=std::string::npos)shortClassName=shortClassName.substr(pos+2);
					}
					if(foundOp.empty()){
						std::string shortMangled=shortClassName+"::"+opName+"(";
						for(size_t i=0;i<node->call.args.size();i++){
							if(i>0)shortMangled+=",";
							MioType* at=resolveExprMioType(node->call.args[i]);
							std::string atStr=at?mio_type_str(at):"";
							shortMangled+=atStr;
						}
						shortMangled+=")";
						if(funcDecls.count(shortMangled))foundOp=shortMangled;
					}
					if(foundOp.empty()){
						std::string prefix1=opBase+"(";
						std::string prefix2=shortClassName+"::"+opName+"(";
						std::vector<std::string> candidates;
						for(const std::string& fnName:funcDecls){
							if(fnName.size()>prefix1.size()&&fnName.substr(0,prefix1.size())==prefix1)
								candidates.push_back(fnName);
							else if(fnName.size()>prefix2.size()&&fnName.substr(0,prefix2.size())==prefix2)
								candidates.push_back(fnName);
						}
						std::vector<std::string> implicitMatches;
						for(auto& c:candidates){
							std::vector<std::string> paramTypes=parseParamTypesFromMangled(c);
							if(paramTypes.size()!=node->call.args.size())continue;
							bool allConvert=true;
							for(size_t i=0;i<paramTypes.size();i++){
								MioType* at=resolveExprMioType(node->call.args[i]);
								std::string atStr=at?mio_type_str(at):"";
								if(!canImplicitConvertStr(atStr,paramTypes[i])){allConvert=false;break;}
							}
							if(allConvert)implicitMatches.push_back(c);
						}
						if(implicitMatches.size()==1)foundOp=implicitMatches[0];
						else if(implicitMatches.size()>1){
							std::string msg="ambiguous operator() call. candidates:";
							for(auto& m:implicitMatches)msg+=" "+m;
							error(node,msg);
						}
					}
					if(!foundOp.empty()){
						node->call.resolved_op_method=foundOp;
						auto defIt=funcDefMap.find(foundOp);
						if(defIt!=funcDefMap.end()&&defIt->second->kind==AstNodeKind::FUNC_DEF&&defIt->second->func_def.return_type)
							node->type=mio_type_clone(defIt->second->func_def.return_type);
						return;
					}
					error(node,"class '"+className+"' does not support operator() with given arguments");
					return;
				}
				if(unwrapped&&unwrapped->kind==MioTypeKind::FUNC){
					checkFuncPtrCall(node,calleeName,unwrapped);
					return;
				}
				return;
			}
			if(node->call.callee->type&&node->call.callee->type->kind==MioTypeKind::FUNC){
				checkFuncPtrCall(node,calleeName,node->call.callee->type);
				return;
			}
			std::string resolvedClass=resolveClassName(calleeName);
			if(classTypes.count(resolvedClass)){
				node->call.callee->ident.name=resolvedClass;
				node->call.callee->ident.namespace_name="";
				auto it=classConstructorSigs.find(resolvedClass);
				if(it!=classConstructorSigs.end()){
					int argCount=(int)node->call.args.size();
					std::vector<std::string> candidateCtors;
					for(auto& cs:it->second){
						int cnt=0;
						for(char ch:cs.second)if(ch==',')cnt++;
						if(cs.second.empty())cnt=-1;
						if(cnt+1==argCount)candidateCtors.push_back(cs.first);
					}
					if(candidateCtors.empty()){
						error(node,"no matching constructor for '"+resolvedClass+"' with "+std::to_string(argCount)+" argument(s)");
						node->type=mio_type_new_named(MioTypeKind::CLASS,resolvedClass);
						return;
					}
					if(candidateCtors.size()>1&&argCount>0){
						std::vector<std::string> exactMatches;
						std::vector<std::string> implicitMatches;
						for(auto& ctorName:candidateCtors){
							std::string mangled=ctorName+"(";
							for(size_t i=0;i<node->call.args.size();i++){
								if(i>0)mangled+=",";
								MioType* at=resolveExprMioType(node->call.args[i]);
								mangled+=at?mio_type_str(at):"";
							}
							mangled+=")";
							if(funcDecls.count(mangled))exactMatches.push_back(ctorName);
							else{
								std::vector<std::string> paramTypes=parseParamTypesFromMangled(ctorName);
								if(paramTypes.size()==(size_t)argCount){
									bool allConvert=true;
									for(size_t i=0;i<paramTypes.size();i++){
										MioType* at=resolveExprMioType(node->call.args[i]);
										std::string atStr=at?mio_type_str(at):"";
										if(!canImplicitConvertStr(atStr,paramTypes[i])){allConvert=false;break;}
									}
									if(allConvert)implicitMatches.push_back(ctorName);
								}
							}
						}
						if(exactMatches.size()==1)candidateCtors=exactMatches;
						else if(exactMatches.empty()&&implicitMatches.size()==1)candidateCtors=implicitMatches;
						else if(exactMatches.empty()&&implicitMatches.size()>1){
							std::string msg="ambiguous constructor for '"+resolvedClass+"'. candidates:";
							for(auto& m:implicitMatches)msg+=" "+m;
							error(node,msg);
							node->type=mio_type_new_named(MioTypeKind::CLASS,resolvedClass);
							return;
						}
					}
					if(candidateCtors.size()>0){
						std::string ctorName=candidateCtors[0];
						node->call.resolved_constructor=ctorName;
						if(argCount>0){
							std::string mangled=ctorName+"(";
							for(size_t i=0;i<node->call.args.size();i++){
								if(i>0)mangled+=",";
								MioType* at=resolveExprMioType(node->call.args[i]);
								mangled+=at?mio_type_str(at):"";
							}
							mangled+=")";
							node->call.resolved_op_method=mangled;
						}
					}
					node->type=mio_type_new_named(MioTypeKind::CLASS,resolvedClass);
			}else if(node->call.args.size()>0){
				error(node,"no matching constructor for '"+resolvedClass+"'");
				node->type=mio_type_new_named(MioTypeKind::CLASS,resolvedClass);
			}
			return;
			}
			if(!node->call.template_args.empty()){
				std::string resolvedClass=resolveClassName(calleeName);
				if(classTemplateMap.count(resolvedClass)){
					for(auto& ta:node->call.template_args){
						if(ta.is_type) continue;
						AstNode* e=ta.expr_val;
						if(!e) continue;
						switch(e->kind){
							case AstNodeKind::INT_LIT:
							case AstNodeKind::FLOAT_LIT:
							case AstNodeKind::STRING_LIT:
							case AstNodeKind::CHAR_LIT:
							case AstNodeKind::BOOL_LIT:
								break;
							case AstNodeKind::IDENT_EXPR:{
								std::string fname=e->ident.name;
								if(!funcDecls.count(fname)){
									bool found=false;
									for(auto& impNs:importedNamespaces){
										if(funcDecls.count(impNs+"::"+fname)){
											found=true;
											break;
										}
									}
									if(!found){
										error(node,"'"+fname+"' is not a valid template value argument (must be a compile-time constant or a top-level/namespace function)");
										node->type=mio_type_new(MioTypeKind::VOID);
										return;
									}
								}
								break;
							}
							default:
								error(node,"invalid template value argument: not a compile-time constant");
								node->type=mio_type_new(MioTypeKind::VOID);
								return;
						}
					}
					std::string instName=resolvedClass;
					for(auto& ta:node->call.template_args){
						if(ta.is_type)
							instName+="$"+mio_type_str(ta.type_val)+"$";
						else
							instName+="$"+ast_value_str(ta.expr_val)+"$";
					}
					if(instantiatedClassNames.find(instName)==instantiatedClassNames.end()){
						instantiatedClassNames.insert(instName);
						std::vector<MioType*> typeArgs;
						std::vector<AstNode*> valueArgs;
						for(auto& ta:node->call.template_args){
							if(ta.is_type)
								typeArgs.push_back(ta.type_val);
							else
								valueArgs.push_back(ta.expr_val);
						}
						AstNode* inst=instantiateClassTemplate(resolvedClass,typeArgs,valueArgs,instName,node);
						if(inst){
							instantiatedClasses[instName]=inst;
							classTypes.insert(instName);
							for(auto& f:inst->class_def.fields){
								classFields[instName].insert(f.name);
								classFieldTypes[instName][f.name]=mio_type_clone(f.type);
							}
							if(!inst->class_def.base_name.empty()){
								std::string baseName=resolveClassName(inst->class_def.base_name);
								classBaseMap[instName]=baseName;
								auto bit=classFields.find(baseName);
								if(bit!=classFields.end()){
									for(auto& fn:bit->second)
										classFields[instName].insert(fn);
								}
								auto bft=classFieldTypes.find(baseName);
								if(bft!=classFieldTypes.end()){
									for(auto& [fn,ft]:bft->second)
										classFieldTypes[instName][fn]=mio_type_clone(ft);
								}
							}
							for(auto* m:inst->class_def.methods){
								std::string mname=m->func_def.name;
								classMethodSet[instName].insert(mname);
								std::string fullMethodName=instName+"::"+mname;
								funcDecls.insert(fullMethodName);
								funcDefMap[fullMethodName]=m;
								if(m->func_def.is_operator){
									std::string mangledFullName=fullMethodName+"(";
									for(size_t i=0;i<m->func_def.params.size();i++){
										if(i>0)mangledFullName+=",";
										mangledFullName+=resolveClassName(mio_type_str(m->func_def.params[i].type));
									}
									mangledFullName+=")";
									funcDecls.insert(mangledFullName);
									funcDefMap[mangledFullName]=m;
								}
							}
							for(auto* c:inst->class_def.constructors){
								std::string ctorName=instName+"::"+c->func_def.name;
								funcDecls.insert(ctorName);
								funcDefMap[ctorName]=c;
								classConstructorSigs[instName].push_back({ctorName,""});
							}
							if(inst->class_def.destructor){
								std::string dtorName=instName+"::"+inst->class_def.destructor->func_def.name;
								funcDecls.insert(dtorName);
								funcDefMap[dtorName]=inst->class_def.destructor;
							}
							registerInstantiatedNestedClasses(inst,instName);
							for(auto* m:inst->class_def.methods){
								currentProgram->program.nodes.push_back(m);
								analyzeFuncDef(m);
							}
							for(auto* c:inst->class_def.constructors){
								currentProgram->program.nodes.push_back(c);
								analyzeFuncDef(c);
							}
							if(inst->class_def.destructor){
								currentProgram->program.nodes.push_back(inst->class_def.destructor);
								analyzeFuncDef(inst->class_def.destructor);
							}
						}
					}
					node->call.callee->ident.name=instName;
					node->call.callee->ident.namespace_name="";
					node->call.template_args.clear();
					if(classTypes.count(instName)){
						auto it=classConstructorSigs.find(instName);
						if(it!=classConstructorSigs.end()){
							int argCount=(int)node->call.args.size();
							int matchCount=0;
							for(auto& cs:it->second){
								int cnt=0;
								for(char ch:cs.second)if(ch==',')cnt++;
								if(cs.second.empty())cnt=-1;
								if(cnt+1==argCount)matchCount++;
							}
							if(matchCount==0){
								error(node,"no matching constructor for '"+instName+"' with "+std::to_string(argCount)+" argument(s)");
								node->type=mio_type_new_named(MioTypeKind::CLASS,instName);
							}
						}else if(node->call.args.size()>0){
							error(node,"no matching constructor for '"+instName+"'");
							node->type=mio_type_new_named(MioTypeKind::CLASS,instName);
						}
						node->type=mio_type_new_named(MioTypeKind::CLASS,instName);
						return;
					}
				}
			}
			if(node->call.template_args.empty()){
				bool found=funcDecls.count(calleeName)>0;
				if(!found){
					for(auto& impNs:importedNamespaces){
						std::string fullName=impNs+"::"+calleeName;
						if(funcDecls.count(fullName)){
							node->call.callee->ident.namespace_name=impNs;
							calleeName=fullName;
							found=true;
							break;
						}
					}
				}
				if(!found&&templateMap.count(calleeName)==0){
					bool templateFound=false;
					for(auto& impNs:importedNamespaces){
						std::string fullName=impNs+"::"+calleeName;
						if(templateMap.count(fullName)){
							calleeName=fullName;
							templateFound=true;
							break;
						}
					}
					if(!templateFound){
						auto deduced=tryDeduceTemplateArgs(calleeName,node);
						if(deduced.empty()){
							error(node,"undefined function '"+calleeName+"'");
							node->type=mio_type_new(MioTypeKind::I32);
						}else{
							node->call.template_args=deduced;
						}
					}
				}
			}
			if(!node->call.template_args.empty()){
				std::string resolvedCallee=calleeName;
				if(templateMap.count(calleeName)==0){
					for(auto& impNs:importedNamespaces){
						std::string fullName=impNs+"::"+calleeName;
						if(templateMap.count(fullName)){
							resolvedCallee=fullName;
							break;
						}
					}
				}
				std::string mangledName=calleeName;
				for(auto& ta:node->call.template_args){
					if(ta.is_type)
						mangledName+="$"+mio_type_str(ta.type_val)+"$";
					else
						mangledName+="$"+ast_value_str(ta.expr_val)+"$";
				}
				if(instantiatedFuncNames.find(mangledName)==instantiatedFuncNames.end()){
					instantiatedFuncNames.insert(mangledName);
					AstNode* inst=instantiateStandaloneFuncTemplate(resolvedCallee,node->call.template_args,mangledName,node);
					if(inst){
						currentProgram->program.nodes.push_back(inst);
						funcDefMap[inst->func_def.name]=inst;
						analyzeFuncDef(inst);
					}
				}
				node->call.callee->ident.name=mangledName;
				node->call.template_args.clear();
				funcDecls.insert(mangledName);
			}
			checkFuncCallArgs(node->call.callee->ident.name,node);
		}else if(node->call.callee->kind==AstNodeKind::MEMBER_EXPR){
			auto* base=node->call.callee->member.base;
			std::string method=node->call.callee->member.member;
			std::string className;
			if(base->type&&!base->type->name.empty()){
				className=resolveClassName(base->type->name);
			}else if(base->kind==AstNodeKind::IDENT_EXPR){
				if(base->ident.name=="this"){
					auto it=locals.find("this");
					if(it!=locals.end()&&it->second&&it->second->kind==MioTypeKind::POINTER&&it->second->base_type&&it->second->base_type->kind==MioTypeKind::CLASS){
						className=resolveClassName(it->second->base_type->name);
					}
				}else{
					auto it=locals.find(base->ident.name);
					if(it!=locals.end()&&it->second){
						if(it->second->kind==MioTypeKind::CLASS)
							className=resolveClassName(it->second->name);
						else if(it->second->kind==MioTypeKind::POINTER&&it->second->base_type&&it->second->base_type->kind==MioTypeKind::CLASS)
							className=resolveClassName(it->second->base_type->name);
					}
				}
			}
			if(!className.empty()){
				std::string mangledName=className+"::"+method;
				bool found=funcDecls.count(mangledName)>0;
				if(!found){
					for(auto& kv:funcDecls){
						if(kv.size()>mangledName.size()+1&&kv.substr(0,mangledName.size()+1)==mangledName+"_"){
							found=true;
							break;
						}
					}
				}
				std::string foundClass=className;
				if(!found){
					std::string baseClass=className;
					while(!found){
						auto bit=classBaseMap.find(baseClass);
						if(bit==classBaseMap.end())break;
						baseClass=bit->second;
						foundClass=baseClass;
						std::string baseMangled=baseClass+"::"+method;
						found=funcDecls.count(baseMangled)>0;
						if(!found){
							for(auto& kv:funcDecls){
								if(kv.size()>baseMangled.size()+1&&kv.substr(0,baseMangled.size()+1)==baseMangled+"_"){
									found=true;
									break;
								}
							}
						}
					}
				}
				if(!found){
					error(node,"method '"+method+"' not found in class '"+className+"'");
					node->type=mio_type_new(MioTypeKind::I32);
				}else{
					checkFuncCallArgs(foundClass+"::"+method,node);
				}
			}
		}
	}
	
	void checkFuncCallArgs(const std::string& calleeName,AstNode* node){
		auto it=funcDefMap.find(calleeName);
		if(it==funcDefMap.end()){
			for(auto& kv:funcDefMap){
				if(kv.first.size()>calleeName.size()+1&&kv.first.substr(0,calleeName.size()+1)==calleeName+"_"){
					it=funcDefMap.find(kv.first);
					break;
				}
			}
		}
		if(it==funcDefMap.end()) return;
		AstNode* funcDef=it->second;
		if(funcDef->kind!=AstNodeKind::FUNC_DEF) return;
		auto& params=funcDef->func_def.params;
		bool isVariadic=funcDef->func_def.is_variadic;
		size_t paramCount=params.size();
		size_t argCount=node->call.args.size();
		if(isVariadic){
			if(argCount<paramCount){
				error(node,"function '"+it->first+"' requires at least "+std::to_string(paramCount)+" argument(s), got "+std::to_string(argCount));
				return;
			}
		}else{
			size_t requiredCount=paramCount;
			for(size_t i=paramCount;i>0;i--){
				if(!params[i-1].default_val) break;
				requiredCount--;
			}
			if(argCount<requiredCount){
				error(node,"function '"+it->first+"' requires at least "+std::to_string(requiredCount)+" argument(s), got "+std::to_string(argCount));
				return;
			}
			if(argCount>paramCount){
				error(node,"function '"+it->first+"' requires at most "+std::to_string(paramCount)+" argument(s), got "+std::to_string(argCount));
				return;
			}
		}
		for(size_t i=0;i<paramCount&&i<argCount;i++){
			MioType* paramType=params[i].type;
			if(!paramType) continue;
			MioType* argType=resolveExprMioType(node->call.args[i]);
			if(!argType) continue;
			if(!isTypeCompatible(paramType,argType)){
				error(node,"argument "+std::to_string(i+1)+" type mismatch: expected '"+mio_type_str(paramType)+"', got '"+mio_type_str(argType)+"'");
			}
		}
	}
	
	bool isTypeCompatible(MioType* expected,MioType* actual){
		if(!expected||!actual) return true;
		MioType* eBase=expected;
		while(eBase&&(eBase->kind==MioTypeKind::REFERENCE||eBase->kind==MioTypeKind::RVALUE_REFERENCE))
			eBase=eBase->base_type;
		MioType* aBase=actual;
		while(aBase&&(aBase->kind==MioTypeKind::REFERENCE||aBase->kind==MioTypeKind::RVALUE_REFERENCE))
			aBase=aBase->base_type;
		if(!eBase||!aBase) return true;
		if(eBase->kind==aBase->kind){
			if(eBase->kind==MioTypeKind::CLASS||eBase->kind==MioTypeKind::ENUM||eBase->kind==MioTypeKind::UNION)
				return resolveClassName(eBase->name)==resolveClassName(aBase->name);
			if(eBase->kind==MioTypeKind::FUNC){
				if(!isTypeCompatible(eBase->base_type,aBase->base_type)) return false;
				if(eBase->param_types.size()!=aBase->param_types.size()) return false;
				for(size_t i=0;i<eBase->param_types.size();i++){
					if(!isTypeCompatible(eBase->param_types[i],aBase->param_types[i])) return false;
				}
				return true;
			}
			return true;
		}
		if(eBase->kind==MioTypeKind::VOID||aBase->kind==MioTypeKind::VOID) return false;
		if(eBase->kind==MioTypeKind::FUNC||aBase->kind==MioTypeKind::FUNC) return false;
		bool eScalar=(eBase->kind>=MioTypeKind::I8&&eBase->kind<=MioTypeKind::USIZE)
			||(eBase->kind==MioTypeKind::F32||eBase->kind==MioTypeKind::F64)
			||eBase->kind==MioTypeKind::BOOL||eBase->kind==MioTypeKind::CHAR
			||eBase->kind==MioTypeKind::ENUM;
		bool aScalar=(aBase->kind>=MioTypeKind::I8&&aBase->kind<=MioTypeKind::USIZE)
			||(aBase->kind==MioTypeKind::F32||aBase->kind==MioTypeKind::F64)
			||aBase->kind==MioTypeKind::BOOL||aBase->kind==MioTypeKind::CHAR
			||aBase->kind==MioTypeKind::ENUM;
		if(eScalar&&aScalar) return true;
		bool ePtr=eBase->kind==MioTypeKind::POINTER;
		bool aPtr=aBase->kind==MioTypeKind::POINTER;
		if(ePtr&&aPtr) return true;
		if((eScalar&&aPtr)||(ePtr&&aScalar)) return true;
		return false;
	}
	
	void checkIndexExpr(AstNode* node){
		if(!node||!node->index_expr.base||!node->index_expr.index) return;
		checkExpr(node->index_expr.base);
		checkExpr(node->index_expr.index);
		MioType* baseMio=resolveExprMioType(node->index_expr.base);
		if(baseMio){
			if(baseMio->kind==MioTypeKind::CLASS&&!baseMio->name.empty()){
				std::string className=resolveClassName(baseMio->name);
				MioType* idxMio=resolveExprMioType(node->index_expr.index);
				std::string opMethod=findOperatorMethod(className,TOK_LBRACKET,idxMio,node);
				if(opMethod.empty()){
					error(node,"class '"+className+"' does not support operator[]");
					node->type=mio_type_new(MioTypeKind::I32);
				}
				node->index_expr.resolved_op_method=opMethod;
				if(!opMethod.empty()){
					std::string baseName=opMethod;
					size_t upos=opMethod.rfind('_');
					if(upos!=std::string::npos&&upos>0&&opMethod[upos-1]!=':'){
						baseName=opMethod.substr(0,upos);
					}
					auto defIt=funcDefMap.find(baseName);
					if(defIt==funcDefMap.end()){
						defIt=funcDefMap.find(opMethod);
					}
					if(defIt!=funcDefMap.end()&&defIt->second->kind==AstNodeKind::FUNC_DEF&&defIt->second->func_def.return_type){
						node->type=mio_type_clone(defIt->second->func_def.return_type);
					}
				}
			}else if(baseMio->kind==MioTypeKind::POINTER&&baseMio->base_type&&baseMio->base_type->kind==MioTypeKind::CLASS&&!baseMio->base_type->name.empty()){
				std::string className=resolveClassName(baseMio->base_type->name);
				MioType* idxMio=resolveExprMioType(node->index_expr.index);
				std::string opMethod=findOperatorMethod(className,TOK_LBRACKET,idxMio,node);
				if(opMethod.empty()){
					error(node,"class '"+className+"' does not support operator[]");
					node->type=mio_type_new(MioTypeKind::I32);
				}
				node->index_expr.resolved_op_method=opMethod;
				if(!opMethod.empty()){
					std::string baseName=opMethod;
					size_t upos=opMethod.rfind('_');
					if(upos!=std::string::npos&&upos>0&&opMethod[upos-1]!=':'){
						baseName=opMethod.substr(0,upos);
					}
					auto defIt=funcDefMap.find(baseName);
					if(defIt==funcDefMap.end()){
						defIt=funcDefMap.find(opMethod);
					}
					if(defIt!=funcDefMap.end()&&defIt->second->kind==AstNodeKind::FUNC_DEF&&defIt->second->func_def.return_type){
						node->type=mio_type_clone(defIt->second->func_def.return_type);
					}
				}
			}else if(baseMio->kind!=MioTypeKind::ARRAY&&baseMio->kind!=MioTypeKind::POINTER){
				error(node,"operator[] requires array or pointer type");
			}
		}
	}
	
	void checkMemberExpr(AstNode* node){
		if(!node||!node->member.base) return;
		checkExpr(node->member.base);
		std::string className;
		if(node->member.base->type&&!node->member.base->type->name.empty())
			className=resolveClassName(node->member.base->type->name);
		else if(node->member.base->kind==AstNodeKind::IDENT_EXPR){
			if(node->member.base->ident.name=="this"){
				auto it=locals.find("this");
				if(it!=locals.end()&&it->second&&it->second->kind==MioTypeKind::POINTER&&it->second->base_type&&it->second->base_type->kind==MioTypeKind::CLASS){
					className=resolveClassName(it->second->base_type->name);
				}
			}else{
				auto it=locals.find(node->member.base->ident.name);
				auto mit=localMioTypes.find(node->member.base->ident.name);
				MioType* baseType=nullptr;
				if(it!=locals.end())baseType=it->second;
				else if(mit!=localMioTypes.end())baseType=mit->second;
				if(baseType){
					if(baseType->kind==MioTypeKind::CLASS||baseType->kind==MioTypeKind::UNION){
						className=resolveClassName(baseType->name);
					}else if(baseType->kind==MioTypeKind::POINTER&&baseType->base_type&&(baseType->base_type->kind==MioTypeKind::CLASS||baseType->base_type->kind==MioTypeKind::UNION)){
						className=resolveClassName(baseType->base_type->name);
					}else if(baseType->kind!=MioTypeKind::POINTER&&baseType->kind!=MioTypeKind::REFERENCE&&baseType->kind!=MioTypeKind::RVALUE_REFERENCE){
						error(node,"cannot access member '"+node->member.member+"' on non-class type");
						return;
					}
				}else if(classTypes.count(node->member.base->ident.name)>0){
					className=resolveClassName(node->member.base->ident.name);
				}
			}
		}
		if(!className.empty()){
			std::string fieldName=node->member.member;
			auto it=classFields.find(className);
			bool foundField=false;
			if(it!=classFields.end()){
				foundField=it->second.count(fieldName)>0;
			}
			if(!foundField){
				auto uit=unionFields.find(className);
				if(uit!=unionFields.end()){
					foundField=uit->second.count(fieldName)>0;
				}
			}
			if(foundField){
				auto fit=classFieldTypes.find(className);
				if(fit!=classFieldTypes.end()){
					auto fi=fit->second.find(fieldName);
					if(fi!=fit->second.end()){
						node->type=mio_type_clone(fi->second);
						checkType(node->type,node);
					}
				}
			}
			if(!foundField){
				std::string mangledName=className+"::"+fieldName;
				bool isMethod=funcDecls.count(mangledName)>0;
				if(!isMethod){
					for(auto& kv:funcDecls){
						if(kv.size()>mangledName.size()+1&&kv.substr(0,mangledName.size()+1)==mangledName+"_"){
							isMethod=true;
							break;
						}
					}
				}
				if(!isMethod){
					std::string baseClass=className;
					while(!isMethod){
						auto bit=classBaseMap.find(baseClass);
						if(bit==classBaseMap.end())break;
						baseClass=bit->second;
						std::string baseMangled=baseClass+"::"+fieldName;
						isMethod=funcDecls.count(baseMangled)>0;
						if(!isMethod){
							for(auto& kv:funcDecls){
								if(kv.size()>baseMangled.size()+1&&kv.substr(0,baseMangled.size()+1)==baseMangled+"_"){
									isMethod=true;
									break;
								}
							}
						}
					}
				}
				if(!isMethod){
					error(node,"field '"+fieldName+"' not found in class '"+className+"'");
					node->type=mio_type_new(MioTypeKind::I32);
				}
			}
		}
	}
	
	void checkAssignExpr(AstNode* node){
		if(!node||!node->assign.left||!node->assign.right) return;
		checkExpr(node->assign.left);
		checkExpr(node->assign.right);
		if(node->assign.left->kind!=AstNodeKind::IDENT_EXPR&&node->assign.left->kind!=AstNodeKind::MEMBER_EXPR&&node->assign.left->kind!=AstNodeKind::INDEX_EXPR&&node->assign.left->kind!=AstNodeKind::UNARY_EXPR){
			error(node,"cannot assign to non-lvalue expression");
			return;
		}
		if(node->assign.left->kind==AstNodeKind::UNARY_EXPR&&node->assign.left->unary.op!=TOK_STAR){
			error(node,"cannot assign to non-lvalue expression");
			return;
		}
		if(node->assign.left->kind==AstNodeKind::IDENT_EXPR&&node->assign.left->ident.name=="this"){
			error(node,"cannot modify 'this' pointer");
			return;
		}
		MioType* lmt=resolveExprMioType(node->assign.left);
		if(node->assign.op==TOK_ASSIGN&&lmt){
			MioType* rmt=resolveExprMioType(node->assign.right);
			if(rmt&&!isTypeCompatible(lmt,rmt)){
				error(node,"assignment type mismatch: expected '"+mio_type_str(lmt)+"', got '"+mio_type_str(rmt)+"'");
			}
		}
		if(node->assign.op!=TOK_ASSIGN){
			std::string className;
			if(lmt){
				if(lmt->kind==MioTypeKind::CLASS&&!lmt->name.empty())
					className=resolveClassName(lmt->name);
			}
			if(!className.empty()){
				MioType* rmt=resolveExprMioType(node->assign.right);
				std::string opMethod=findOperatorMethod(className,node->assign.op,rmt,node);
				if(opMethod.empty()){
					error(node,"class '"+className+"' does not support operator"+tok_name(node->assign.op));
					node->type=mio_type_new(MioTypeKind::I32);
				}
				node->assign.resolved_op_method=opMethod;
			}
		}
		switch(node->assign.op){
			case TOK_PLUS_ASSIGN:
			case TOK_MINUS_ASSIGN:{
				if(lmt&&lmt->kind==MioTypeKind::POINTER&&lmt->base_type&&lmt->base_type->kind==MioTypeKind::VOID){
					error(node,"cannot perform pointer arithmetic on void*");
					return;
				}
				break;
			}
			default: break;
		}
	}
	
	void checkCastExpr(AstNode* node){
		if(!node||!node->cast_expr.expr) return;
		checkExpr(node->cast_expr.expr);
		if(node->cast_expr.target_type){
			checkType(node->cast_expr.target_type,node);
		}else{
			error(node,"cast expression has no target type");
			node->type=mio_type_new(MioTypeKind::I32);
		}
	}
	
	void checkIdentExpr(AstNode* node){
		if(!node) return;
		std::string name=node->ident.name;
		std::string ns=node->ident.namespace_name;
		if(name=="this") return;
		if(!ns.empty()&&ns!="::")
			name=ns+"::"+name;
		bool found=locals.count(name)>0||varDecls.count(name)>0||funcDecls.count(name)>0||enumNames.count(name)>0||unionNames.count(name)>0||classTypes.count(name)>0||enumVariantMap.count(name)>0||templateMap.count(name)>0||classTemplateMap.count(name)>0;
		if(!found){
			for(auto& impNs:importedNamespaces){
				std::string fullName=impNs+"::"+name;
				if(varDecls.count(fullName)||funcDecls.count(fullName)||classTypes.count(fullName)||templateMap.count(fullName)||classTemplateMap.count(fullName)){
					found=true;
					break;
				}
			}
		}
		if(!found){
			error(node,"undefined variable '"+node->ident.name+"'");
			node->type=mio_type_new(MioTypeKind::I32);
			return;
		}
		auto mit=localMioTypes.find(name);
		if(mit!=localMioTypes.end()){
			node->type=mio_type_clone(mit->second);
		}else{
			auto lit=locals.find(name);
			if(lit!=locals.end()){
				node->type=mio_type_clone(lit->second);
			}else{
				auto git=globalMioTypes.find(name);
				if(git!=globalMioTypes.end()){
					node->type=mio_type_clone(git->second);
				}else{
					auto fit=funcDefMap.find(name);
					if(fit!=funcDefMap.end()&&fit->second->kind==AstNodeKind::FUNC_DEF){
						auto* ret=mio_type_clone(fit->second->func_def.return_type);
						std::vector<MioType*> params;
						for(size_t i=0;i<fit->second->func_def.params.size();i++)
							params.push_back(fit->second->func_def.params[i].type);
						node->type=mio_type_new_func(ret,params);
						mio_type_free(ret);
					}
				}
			}
		}
	}
	
	void checkType(MioType* mt,AstNode* ctx){
		if(!mt) return;
		if(mt->kind==MioTypeKind::REFERENCE||mt->kind==MioTypeKind::RVALUE_REFERENCE){
			if(mt->base_type){
				if(mt->base_type->kind==MioTypeKind::POINTER){
					error(ctx,"cannot define reference to pointer");
					return;
				}
				if(mt->base_type->kind==MioTypeKind::REFERENCE||mt->base_type->kind==MioTypeKind::RVALUE_REFERENCE){
					error(ctx,"cannot define reference to reference");
					return;
				}
			}
		}
		if(mt->kind==MioTypeKind::FUNC){
			checkType(mt->base_type,ctx);
			for(auto* p:mt->param_types)
				checkType(p,ctx);
			return;
		}
		if(mt->kind==MioTypeKind::CLASS&&!mt->name.empty()&&mt->param_types.empty()){
			std::string resolved=resolveClassName(mt->name);
			mt->name=resolved;
			if(typeAliases.count(resolved)){
				auto* cloned=mio_type_clone(typeAliases[resolved]);
				if(mt->base_type) delete mt->base_type;
				for(auto* p:mt->param_types) delete p;
				mt->kind=cloned->kind;
				mt->name=std::move(cloned->name);
				mt->base_type=cloned->base_type; cloned->base_type=nullptr;
				mt->param_types=std::move(cloned->param_types);
				mt->filename=cloned->filename;
				mt->array_size=cloned->array_size;
				mt->is_const=cloned->is_const;
				mt->ref_count=cloned->ref_count;
				delete cloned;
				checkType(mt,ctx);
				return;
			}
			if(enumNames.count(resolved)){
				mt->kind=MioTypeKind::ENUM;
			}else if(unionNames.count(resolved)){
				mt->kind=MioTypeKind::UNION;
			}else if(!classTypes.count(resolved)&&!classTemplateMap.count(resolved)){
				error(ctx,"unknown type '"+resolved+"'");
			}
		}
		if(mt->kind==MioTypeKind::CLASS&&!mt->param_types.empty()){
			std::string name=mt->name;
			std::string fullName=resolveClassName(name);
			if(!classTemplateMap.count(fullName)){
				error(ctx,"unknown template '"+name+"'");
				return;
			}
			for(auto* e:mt->value_args){
				if(!e) continue;
				switch(e->kind){
					case AstNodeKind::INT_LIT:
					case AstNodeKind::FLOAT_LIT:
					case AstNodeKind::STRING_LIT:
					case AstNodeKind::CHAR_LIT:
					case AstNodeKind::BOOL_LIT:
						break;
					case AstNodeKind::IDENT_EXPR:{
						std::string fname=e->ident.name;
						if(funcDecls.count(fname)){
							break;
						}
						bool found=false;
						for(auto& impNs:importedNamespaces){
							if(funcDecls.count(impNs+"::"+fname)){
								found=true;
								break;
							}
						}
						if(!found){
							error(ctx,"'"+fname+"' is not a valid template value argument (must be a compile-time constant or a top-level/namespace function)");
							return;
						}
						break;
					}
					default:
						error(ctx,"invalid template value argument: not a compile-time constant");
						return;
				}
			}
			std::string instName=fullName;
			for(auto* t:mt->param_types){
				instName+="$"+mio_type_str(t)+"$";
			}
			for(auto* e:mt->value_args){
				instName+="$"+ast_value_str(e)+"$";
			}
			if(instantiatedClassNames.find(instName)==instantiatedClassNames.end()){
				instantiatedClassNames.insert(instName);
				AstNode* inst=instantiateClassTemplate(fullName,mt->param_types,mt->value_args,instName,ctx);
				if(inst){
					instantiatedClasses[instName]=inst;
					classTypes.insert(instName);
					for(auto& f:inst->class_def.fields){
						classFields[instName].insert(f.name);
						classFieldTypes[instName][f.name]=mio_type_clone(f.type);
					}
					if(!inst->class_def.base_name.empty()){
						std::string baseName=resolveClassName(inst->class_def.base_name);
						classBaseMap[instName]=baseName;
						auto bit=classFields.find(baseName);
						if(bit!=classFields.end()){
							for(auto& fn:bit->second)
								classFields[instName].insert(fn);
						}
						auto bft=classFieldTypes.find(baseName);
						if(bft!=classFieldTypes.end()){
							for(auto& [fn,ft]:bft->second)
								classFieldTypes[instName][fn]=mio_type_clone(ft);
						}
					}
					for(auto* m:inst->class_def.methods){
						std::string mname=m->func_def.name;
						classMethodSet[instName].insert(mname);
						std::string fullMethodName=instName+"::"+mname;
						funcDecls.insert(fullMethodName);
						funcDefMap[fullMethodName]=m;
						if(m->func_def.is_operator){
							std::string mangledFullName=fullMethodName+"(";
							for(size_t i=0;i<m->func_def.params.size();i++){
								if(i>0)mangledFullName+=",";
								mangledFullName+=resolveClassName(mio_type_str(m->func_def.params[i].type));
							}
							mangledFullName+=")";
							funcDecls.insert(mangledFullName);
							funcDefMap[mangledFullName]=m;
						}
					}
					for(auto* c:inst->class_def.constructors){
						std::string ctorName=instName+"::"+c->func_def.name;
						std::string sig;
						for(size_t pi=0;pi<c->func_def.params.size();pi++){
							if(pi>0)sig+=",";
							sig+=mio_type_str(c->func_def.params[pi].type);
						}
						classConstructorSigs[instName].push_back({ctorName,sig});
						funcDecls.insert(ctorName);
						funcDefMap[ctorName]=c;
						if(!sig.empty()){
							std::string mangledCtor=ctorName+"("+sig+")";
							funcDecls.insert(mangledCtor);
							funcDefMap[mangledCtor]=c;
						}
					}
					if(inst->class_def.destructor){
						std::string dtorName=instName+"::"+inst->class_def.destructor->func_def.name;
						funcDecls.insert(dtorName);
						funcDefMap[dtorName]=inst->class_def.destructor;
					}
					registerInstantiatedNestedClasses(inst,instName);
					for(auto* m:inst->class_def.methods){
						currentProgram->program.nodes.push_back(m);
						analyzeFuncDef(m);
					}
					for(auto* c:inst->class_def.constructors){
						currentProgram->program.nodes.push_back(c);
						analyzeFuncDef(c);
					}
					if(inst->class_def.destructor){
						currentProgram->program.nodes.push_back(inst->class_def.destructor);
						analyzeFuncDef(inst->class_def.destructor);
					}
				}
			}
			mt->name=instName;
			mt->param_types.clear();
		}
		if(mt->base_type){
			checkType(mt->base_type,ctx);
		}
		for(auto* p:mt->param_types){
			checkType(p,ctx);
		}
	}
	
	void registerInstantiatedNestedClasses(AstNode* inst,const std::string& instName){
		for(auto* nc:inst->class_def.nested_classes){
			if(nc->kind==AstNodeKind::TYPE_ALIAS){
				typeAliases[nc->type_alias.name]=mio_type_clone(nc->type_alias.aliased_type);
				continue;
			}
			std::string ncName=nc->class_def.name;
			classTypes.insert(ncName);
			for(auto& f:nc->class_def.fields){
				classFields[ncName].insert(f.name);
				classFieldTypes[ncName][f.name]=mio_type_clone(f.type);
			}
			for(auto* m:nc->class_def.methods){
				std::string mname=m->func_def.name;
				classMethodSet[ncName].insert(mname);
				std::string fullMethodName=ncName+"::"+mname;
				funcDecls.insert(fullMethodName);
				funcDefMap[fullMethodName]=m;
				if(m->func_def.is_operator){
					std::string mangledFullName=fullMethodName+"(";
					for(size_t i=0;i<m->func_def.params.size();i++){
						if(i>0)mangledFullName+=",";
						mangledFullName+=resolveClassName(mio_type_str(m->func_def.params[i].type));
					}
					mangledFullName+=")";
					funcDecls.insert(mangledFullName);
					funcDefMap[mangledFullName]=m;
				}
			}
			for(auto* c:nc->class_def.constructors){
				std::string ctorName=ncName+"::"+c->func_def.name;
				std::string sig;
				for(size_t pi=0;pi<c->func_def.params.size();pi++){
					if(pi>0)sig+=",";
					sig+=mio_type_str(c->func_def.params[pi].type);
				}
				classConstructorSigs[ncName].push_back({ctorName,sig});
				funcDecls.insert(ctorName);
				funcDefMap[ctorName]=c;
				if(!sig.empty()){
					std::string mangledCtor=ctorName+"("+sig+")";
					funcDecls.insert(mangledCtor);
					funcDefMap[mangledCtor]=c;
				}
			}
			if(nc->class_def.destructor){
				std::string dtorName=ncName+"::"+nc->class_def.destructor->func_def.name;
				funcDecls.insert(dtorName);
				funcDefMap[dtorName]=nc->class_def.destructor;
			}
			for(auto* m:nc->class_def.methods){
				currentProgram->program.nodes.push_back(m);
				analyzeFuncDef(m);
			}
			for(auto* c:nc->class_def.constructors){
				currentProgram->program.nodes.push_back(c);
				analyzeFuncDef(c);
			}
			if(nc->class_def.destructor){
				currentProgram->program.nodes.push_back(nc->class_def.destructor);
				analyzeFuncDef(nc->class_def.destructor);
			}
			registerInstantiatedNestedClasses(nc,ncName);
		}
	}
	
	std::string resolveClassName(const std::string& name){
		if(name.find("::")!=std::string::npos) return name;
		if(!currentClassName.empty()){
			std::string fullName=currentClassName+"::"+name;
			if(classTemplateMap.count(fullName)||classTypes.count(fullName)||typeAliases.count(fullName)){
				return fullName;
			}
			auto pos=currentClassName.rfind("::");
			std::string shortCurrent=(pos!=std::string::npos)?currentClassName.substr(pos+2):currentClassName;
			if(name==shortCurrent&&(classTypes.count(currentClassName)||classTemplateMap.count(currentClassName))){
				return currentClassName;
			}
		}
		for(auto& ns:importedNamespaces){
			std::string fullName=ns+"::"+name;
			if(classTemplateMap.count(fullName)||classTypes.count(fullName)){
				return fullName;
			}
			if(typeAliases.count(fullName)){
				return fullName;
			}
		}
		if(!currentNamespace.empty()){
			std::string fullName=currentNamespace+"::"+name;
			if(classTemplateMap.count(fullName)||classTypes.count(fullName)){
				return fullName;
			}
			if(typeAliases.count(fullName)){
				return fullName;
			}
		}
		return name;
	}
	
	std::string findOperatorMethod(const std::string& className,TokenKind op,MioType* rightType,AstNode* errCtx=nullptr){
		std::string opName;
		switch(op){
			case TOK_PLUS:opName="operator+";break;
			case TOK_MINUS:opName="operator-";break;
			case TOK_STAR:opName="operator*";break;
			case TOK_SLASH:opName="operator/";break;
			case TOK_PERCENT:opName="operator%";break;
			case TOK_EQ:opName="operator==";break;
			case TOK_NEQ:opName="operator!=";break;
			case TOK_LT:opName="operator<";break;
			case TOK_GT:opName="operator>";break;
			case TOK_LTE:opName="operator<=";break;
			case TOK_GTE:opName="operator>=";break;
			case TOK_LBRACKET:opName="operator[";break;
			case TOK_ASSIGN:opName="operator=";break;
			case TOK_PLUS_ASSIGN:opName="operator+=";break;
			case TOK_MINUS_ASSIGN:opName="operator-=";break;
			case TOK_STAR_ASSIGN:opName="operator*=";break;
			case TOK_SLASH_ASSIGN:opName="operator/=";break;
			case TOK_BIT_AND:opName="operator&";break;
			case TOK_BIT_OR:opName="operator|";break;
			case TOK_BIT_XOR:opName="operator^";break;
			case TOK_LSHIFT:opName="operator<<";break;
			case TOK_RSHIFT:opName="operator>>";break;
			default:return "";
		}
		std::string rightTypeStr=rightType?mio_type_str(rightType):"";
		std::string exactName=className+"::"+opName+"("+rightTypeStr+")";
		if(funcDecls.count(exactName))return exactName;
		std::string shortClassName=className;
		{
			auto pos=shortClassName.rfind("::");
			if(pos!=std::string::npos)shortClassName=shortClassName.substr(pos+2);
		}
		std::string exactShort=shortClassName+"::"+opName+"("+rightTypeStr+")";
		if(funcDecls.count(exactShort))return exactShort;
		std::string prefix1=className+"::"+opName+"(";
		std::string prefix2=shortClassName+"::"+opName+"(";
		std::vector<std::string> implicitMatches;
		for(const std::string& fnName:funcDecls){
			bool match=false;
			if(fnName.size()>prefix1.size()&&fnName.substr(0,prefix1.size())==prefix1)
				match=true;
			else if(fnName.size()>prefix2.size()&&fnName.substr(0,prefix2.size())==prefix2)
				match=true;
			if(match){
				std::string paramType=getFirstParamTypeFromMangled(fnName);
				if(!paramType.empty()&&canImplicitConvertStr(rightTypeStr,paramType))
					implicitMatches.push_back(fnName);
			}
		}
		if(implicitMatches.size()==1)return implicitMatches[0];
		if(implicitMatches.size()>1){
			std::string msg="ambiguous operator '"+opName+"' for type '"+rightTypeStr+"'. candidates:";
			for(auto& m:implicitMatches)msg+=" "+m;
			if(errCtx)error(errCtx,msg);
		}
		return "";
	}
	
	bool canImplicitConvertStr(const std::string& from,const std::string& to){
		if(from==to)return true;
		auto intRank=[](const std::string& t){
			if(t=="bool")return 1;
			if(t=="char")return 2;
			if(t=="i8"||t=="u8")return 3;
			if(t=="i16"||t=="u16")return 4;
			if(t=="i32"||t=="u32")return 5;
			if(t=="i64"||t=="u64"||t=="isize"||t=="usize")return 6;
			if(t=="i128"||t=="u128")return 7;
			return 0;
		};
		int fr=intRank(from);
		int tr=intRank(to);
		if(fr>0&&tr>0&&tr>=fr)return true;
		if(from=="f32"&&to=="f64")return true;
		if(to=="void*"&&!from.empty()&&from.back()=='*')return true;
		return false;
	}
	
	std::string getFirstParamTypeFromMangled(const std::string& name){
		size_t lp=name.find('(');
		if(lp==std::string::npos)return "";
		size_t rp=name.find(')',lp);
		if(rp==std::string::npos)return "";
		std::string params=name.substr(lp+1,rp-lp-1);
		size_t comma=params.find(',');
		if(comma!=std::string::npos)params=params.substr(0,comma);
		return params;
	}
	
	std::vector<std::string> parseParamTypesFromMangled(const std::string& name){
		std::vector<std::string> result;
		size_t lp=name.find('(');
		if(lp==std::string::npos)return result;
		size_t rp=name.find(')',lp);
		if(rp==std::string::npos)return result;
		std::string params=name.substr(lp+1,rp-lp-1);
		if(params.empty())return result;
		size_t start=0;
		while(start<params.size()){
			size_t comma=params.find(',',start);
			if(comma==std::string::npos){
				result.push_back(params.substr(start));
				break;
			}
			result.push_back(params.substr(start,comma-start));
			start=comma+1;
		}
		return result;
	}
	
	MioType* resolveExprMioType(AstNode* node){
		if(!node) return nullptr;
		if(node->type){
			MioType* t=node->type;
			while(t&&t->kind==MioTypeKind::REFERENCE){
				t=t->base_type;
			}
			return t;
		}
		switch(node->kind){
			case AstNodeKind::UNARY_EXPR:
				if(node->unary.op==TOK_BIT_AND){
					MioType* inner=resolveExprMioType(node->unary.operand);
					return inner?mio_type_new_pointer(inner):nullptr;
				}
				if(node->unary.op==TOK_STAR){
					MioType* inner=resolveExprMioType(node->unary.operand);
					if(inner&&inner->kind==MioTypeKind::POINTER) return inner->base_type;
					if(inner&&inner->kind==MioTypeKind::REFERENCE) return inner->base_type;
					return nullptr;
				}
				return resolveExprMioType(node->unary.operand);
			case AstNodeKind::BINARY_EXPR:{
				switch(node->binary.op){
					case TOK_EQ: case TOK_NEQ: case TOK_LT: case TOK_GT: case TOK_LTE: case TOK_GTE:
					case TOK_AND: case TOK_OR:
						return mio_type_new(MioTypeKind::BOOL);
					default:{
						MioType* lt=resolveExprMioType(node->binary.left);
						if(lt) return mio_type_clone(lt);
						MioType* rt=resolveExprMioType(node->binary.right);
						if(rt) return mio_type_clone(rt);
						return nullptr;
					}
				}
			}
			case AstNodeKind::INDEX_EXPR:{
				MioType* baseMio=resolveExprMioType(node->index_expr.base);
				if(!baseMio) return nullptr;
				if(baseMio->kind==MioTypeKind::POINTER&&baseMio->base_type)
					return mio_type_clone(baseMio->base_type);
				if(baseMio->kind==MioTypeKind::ARRAY&&baseMio->base_type)
					return mio_type_clone(baseMio->base_type);
				return nullptr;
			}
			case AstNodeKind::INT_LIT: return mio_type_new(MioTypeKind::I32);
			case AstNodeKind::FLOAT_LIT: return mio_type_new(MioTypeKind::F64);
			case AstNodeKind::BOOL_LIT: return mio_type_new(MioTypeKind::BOOL);
			case AstNodeKind::CHAR_LIT: return mio_type_new(MioTypeKind::CHAR);
			case AstNodeKind::MEMBER_EXPR:{
				std::string className;
				if(node->member.base->type&&!node->member.base->type->name.empty())
					className=resolveClassName(node->member.base->type->name);
				else if(node->member.base->kind==AstNodeKind::IDENT_EXPR){
					if(node->member.base->ident.name=="this"){
						auto it=localMioTypes.find("this");
						if(it!=localMioTypes.end()&&it->second&&it->second->kind==MioTypeKind::POINTER&&it->second->base_type&&it->second->base_type->kind==MioTypeKind::CLASS)
							className=resolveClassName(it->second->base_type->name);
					}else{
						auto it=locals.find(node->member.base->ident.name);
						auto mit=localMioTypes.find(node->member.base->ident.name);
						MioType* baseType=nullptr;
						if(it!=locals.end())baseType=it->second;
						else if(mit!=localMioTypes.end())baseType=mit->second;
						if(baseType){
							if(baseType->kind==MioTypeKind::CLASS||baseType->kind==MioTypeKind::UNION){
								className=resolveClassName(baseType->name);
							}else if(baseType->kind==MioTypeKind::POINTER&&baseType->base_type&&(baseType->base_type->kind==MioTypeKind::CLASS||baseType->base_type->kind==MioTypeKind::UNION)){
								className=resolveClassName(baseType->base_type->name);
							}
						}else if(classTypes.count(node->member.base->ident.name)>0){
							className=resolveClassName(node->member.base->ident.name);
						}
					}
				}
				if(!className.empty()){
					auto fit=classFieldTypes.find(className);
					if(fit!=classFieldTypes.end()){
						auto fi=fit->second.find(node->member.member);
						if(fi!=fit->second.end()){
							return mio_type_clone(fi->second);
						}
					}
				}
				return nullptr;
			}
			case AstNodeKind::IDENT_EXPR:{
				std::string name=node->ident.name;
				if(!node->ident.namespace_name.empty())
					name=node->ident.namespace_name+"::"+name;
				auto mit=localMioTypes.find(name);
			if(mit!=localMioTypes.end()&&mit->second){
				return mio_type_clone(mit->second);
			}
			auto git=globalMioTypes.find(name);
			if(git!=globalMioTypes.end()&&git->second){
				return mio_type_clone(git->second);
			}
			auto fit=funcDefMap.find(name);
			if(fit!=funcDefMap.end()&&fit->second->kind==AstNodeKind::FUNC_DEF){
				auto* ret=mio_type_clone(fit->second->func_def.return_type);
				std::vector<MioType*> params;
				for(size_t i=0;i<fit->second->func_def.params.size();i++)
					params.push_back(fit->second->func_def.params[i].type);
				auto* ft=mio_type_new_func(ret,params);
				mio_type_free(ret);
				return ft;
			}
			return nullptr;
			}
			default:
				return nullptr;
		}
	}
	
	std::vector<TemplateArg> tryDeduceTemplateArgs(const std::string& calleeName,AstNode* node){
		auto it=templateMap.find(calleeName);
		if(it==templateMap.end()) return {};
		auto& typeParams=it->second.first;
		auto* templateDef=it->second.second;
		if(templateDef->kind!=AstNodeKind::FUNC_DEF) return {};
		auto& params=templateDef->func_def.params;
		if(params.size()!=node->call.args.size()) return {};
		size_t typeParamCount=0;
		for(auto& p:typeParams) if(p.is_type) typeParamCount++;
		std::vector<MioType*> deduced(typeParamCount,nullptr);
		for(size_t i=0;i<params.size();i++){
			MioType* paramType=params[i].type;
			if(!paramType) continue;
			MioType* argType=resolveExprMioType(node->call.args[i]);
			if(!argType) continue;
			deduceTemplateArg(paramType,argType,typeParams,deduced);
			if(argType!=node->call.args[i]->type) mio_type_free(argType);
		}
		for(size_t i=0;i<deduced.size();i++){
			if(!deduced[i]) return {};
		}
		std::vector<TemplateArg> result;
		size_t dedIdx=0;
		for(auto& p:typeParams){
			if(p.is_type){
				result.push_back({true,deduced[dedIdx++],nullptr});
			}else{
				AstNode* defVal=p.default_val;
				if(!defVal) return {};
				result.push_back({false,nullptr,defVal});
			}
		}
		return result;
	}
	
	void deduceTemplateArg(MioType* paramType,MioType* argType,const std::vector<TemplateParam>& typeParams,std::vector<MioType*>& deduced){
		if(!paramType||!argType) return;
		if(paramType->kind==MioTypeKind::POINTER){
			if(argType->kind==MioTypeKind::POINTER){
				deduceTemplateArg(paramType->base_type,argType->base_type,typeParams,deduced);
			}
			return;
		}
		if(paramType->kind==MioTypeKind::ARRAY){
			deduceTemplateArg(paramType->base_type,argType,typeParams,deduced);
			return;
		}
		size_t dedIdx=0;
		for(size_t i=0;i<typeParams.size();i++){
			if(!typeParams[i].is_type) continue;
			if(paramType->name==typeParams[i].name){
				if(deduced[dedIdx]&&(deduced[dedIdx]->name!=argType->name||deduced[dedIdx]->kind!=argType->kind)){
					return;
				}
				if(deduced[dedIdx]) mio_type_free(deduced[dedIdx]);
				deduced[dedIdx]=mio_type_clone(argType);
				return;
			}
			dedIdx++;
		}
	}
	
	AstNode* instantiateClassTemplate(const std::string& name,std::vector<MioType*>& typeArgs,std::vector<AstNode*>& valueArgs,const std::string& instName,AstNode* ctx){
		auto it=classTemplateMap.find(name);
		if(it==classTemplateMap.end()){
			error(ctx,"unknown template '"+name+"'");
			return nullptr;
		}
		size_t totalArgs=typeArgs.size()+valueArgs.size();
		auto& candidates=it->second;
		std::pair<std::vector<TemplateParam>,AstNode*>* selected=nullptr;
		for(auto& c:candidates){
			if(c.first.size()==totalArgs){
				if(selected){
					error(ctx,"ambiguous template '"+name+"' - multiple overloads with "+std::to_string(totalArgs)+" parameter(s)");
					return nullptr;
				}
				selected=&c;
			}
		}
		if(!selected){
			std::string counts;
			for(auto& c:candidates){
				if(!counts.empty())counts+=", ";
				counts+=std::to_string(c.first.size());
			}
			error(ctx,"template '"+name+"' has "+counts+" parameter(s), but got "+std::to_string(totalArgs)+" argument(s)");
			return nullptr;
		}
		auto* templateDef=selected->second;
		auto& typeParams=selected->first;
		std::unordered_map<std::string,MioType*> typeSubst;
		std::unordered_map<std::string,AstNode*> exprSubst;
		size_t ti=0,vi=0;
		for(size_t i=0;i<typeParams.size();i++){
			if(typeParams[i].is_type){
				if(ti>=typeArgs.size()){
					error(ctx,"internal: type arg index out of bounds");
					return nullptr;
				}
				typeSubst[typeParams[i].name]=typeArgs[ti++];
			}else{
				if(vi>=valueArgs.size()){
					error(ctx,"internal: value arg index out of bounds");
					return nullptr;
				}
				exprSubst[typeParams[i].name]=valueArgs[vi++];
			}
		}
		auto* inst=instantiateTemplate(templateDef,typeSubst,exprSubst,instName);
		if(!inst) return nullptr;
		inst->class_def.name=instName;
		return inst;
	}
	
	AstNode* instantiateTemplate(AstNode* templateDef,std::unordered_map<std::string,MioType*>& typeSubst,std::unordered_map<std::string,AstNode*>& exprSubst,const std::string& instClassName){
		if(templateDef->kind!=AstNodeKind::CLASS_DEF) return nullptr;
		auto* def=templateDef;
		auto* inst=ast_new_class_def(def->class_def.name,"","",def->line,def->col,def->filename);
		for(auto& f:def->class_def.fields){
			Field nf;
			nf.name=f.name;
			nf.type=substituteType(f.type,typeSubst);
			nf.access=f.access;
			nf.init=f.init;
			inst->class_def.fields.push_back(nf);
		}
		for(auto* m:def->class_def.methods){
			auto* nm=instantiateFuncTemplate(m,typeSubst,exprSubst,instClassName);
			if(nm) inst->class_def.methods.push_back(nm);
		}
		for(auto* c:def->class_def.constructors){
			auto* nc=instantiateFuncTemplate(c,typeSubst,exprSubst,instClassName);
			if(nc) inst->class_def.constructors.push_back(nc);
		}
		if(def->class_def.destructor){
			inst->class_def.destructor=instantiateFuncTemplate(def->class_def.destructor,typeSubst,exprSubst,instClassName);
		}
		for(auto* nc:def->class_def.nested_classes){
			if(nc->kind==AstNodeKind::TYPE_ALIAS){
				std::string ncName=nc->type_alias.name;
				auto pos=ncName.rfind("::");
				std::string shortName=(pos!=std::string::npos)?ncName.substr(pos+2):ncName;
				std::string newNcName=instClassName+"::"+shortName;
				typeAliases[newNcName]=mio_type_clone(nc->type_alias.aliased_type);
				continue;
			}
			std::string ncName=nc->class_def.name;
			auto pos=ncName.rfind("::");
			std::string shortName=(pos!=std::string::npos)?ncName.substr(pos+2):ncName;
			std::string newNcName=instClassName+"::"+shortName;
			auto* nnc=ast_new_class_def(newNcName,"","",nc->line,nc->col,nc->filename);
			nnc->class_def.base_name=nc->class_def.base_name;
			nnc->class_def.base_access=nc->class_def.base_access;
			for(auto& f:nc->class_def.fields){
				Field nf;
				nf.name=f.name;
				nf.type=substituteType(f.type,typeSubst);
				nf.access=f.access;
				nf.init=f.init;
				nnc->class_def.fields.push_back(nf);
			}
			for(auto* m:nc->class_def.methods){
				auto* nm=instantiateFuncTemplate(m,typeSubst,exprSubst,newNcName);
				if(nm) nnc->class_def.methods.push_back(nm);
			}
			for(auto* c:nc->class_def.constructors){
				auto* nc2=instantiateFuncTemplate(c,typeSubst,exprSubst,newNcName);
				if(nc2) nnc->class_def.constructors.push_back(nc2);
			}
			if(nc->class_def.destructor){
				nnc->class_def.destructor=instantiateFuncTemplate(nc->class_def.destructor,typeSubst,exprSubst,newNcName);
			}
			inst->class_def.nested_classes.push_back(nnc);
		}
		return inst;
	}
	
	AstNode* instantiateFuncTemplate(AstNode* funcDef,std::unordered_map<std::string,MioType*>& typeSubst,std::unordered_map<std::string,AstNode*>& exprSubst,const std::string& instClassName){
		if(funcDef->kind!=AstNodeKind::FUNC_DEF) return nullptr;
		auto* def=funcDef;
		MioType* retType=substituteType(def->func_def.return_type,typeSubst);
		if(retType&&retType->kind==MioTypeKind::CLASS&&!retType->name.empty()){
			auto it=typeSubst.find(retType->name);
			if(it==typeSubst.end()){
				retType->name=instClassName;
			}
		}
		std::string shortName=instClassName;
		auto pos=shortName.rfind("::");
		if(pos!=std::string::npos) shortName=shortName.substr(pos+2);
		std::string funcName=def->func_def.name;
		if(funcName==def->func_def.class_name) funcName=shortName;
		else if(!funcName.empty()&&funcName[0]=='~'&&funcName.substr(1)==def->func_def.class_name) funcName="~"+shortName;
		auto* inst=ast_new_func_def(funcName,retType,nullptr,def->func_def.is_static,def->line,def->col,def->filename);
		inst->func_def.class_name=instClassName;
		for(auto& p:def->func_def.params){
			MioType* pt=substituteType(p.type,typeSubst);
			inst->func_def.params.push_back({p.name,pt,nullptr});
		}
		if(def->func_def.body){
			AstCloner cloner;
			cloner.typeSubst=&typeSubst;
			cloner.exprSubst=&exprSubst;
			cloner.fn=def->filename;
			inst->func_def.body=cloner.cloneNode(def->func_def.body);
		}
		inst->func_def.is_operator=def->func_def.is_operator;
		inst->func_def.is_virtual=def->func_def.is_virtual;
		inst->func_def.is_override=def->func_def.is_override;
		inst->func_def.is_pure_virtual=def->func_def.is_pure_virtual;
		return inst;
	}
	
	AstNode* instantiateStandaloneFuncTemplate(const std::string& templateName,std::vector<TemplateArg>& templateArgs,const std::string& mangledName,AstNode* ctx){
		auto it=templateMap.find(templateName);
		if(it==templateMap.end()){
			error(ctx,"internal error: template '"+templateName+"' not found");
			return nullptr;
		}
		auto& typeParams=it->second.first;
		auto* templateDef=it->second.second;
		if(templateDef->kind!=AstNodeKind::FUNC_DEF) return nullptr;
		if(templateArgs.size()!=typeParams.size()){
			error(ctx,"template '"+templateName+"' requires "+std::to_string(typeParams.size())+" arguments, got "+std::to_string(templateArgs.size()));
			return nullptr;
		}
		std::unordered_map<std::string,MioType*> typeSubst;
		std::unordered_map<std::string,AstNode*> exprSubst;
		for(size_t i=0;i<typeParams.size();i++){
			if(typeParams[i].is_type){
				typeSubst[typeParams[i].name]=templateArgs[i].type_val;
			}else{
				exprSubst[typeParams[i].name]=templateArgs[i].expr_val;
			}
		}
		auto* def=templateDef;
		MioType* retType=substituteType(def->func_def.return_type,typeSubst);
		auto* inst=ast_new_func_def(mangledName,retType,nullptr,def->func_def.is_static,def->line,def->col,def->filename);
		inst->func_def.is_extern=def->func_def.is_extern;
		inst->func_def.is_variadic=def->func_def.is_variadic;
		inst->func_def.is_operator=def->func_def.is_operator;
		inst->func_def.is_virtual=def->func_def.is_virtual;
		inst->func_def.is_override=def->func_def.is_override;
		inst->func_def.is_pure_virtual=def->func_def.is_pure_virtual;
		inst->func_def.class_name=def->func_def.class_name;
		for(auto& p:def->func_def.params){
			MioType* pt=substituteType(p.type,typeSubst);
			inst->func_def.params.push_back({p.name,pt,nullptr});
		}
		if(def->func_def.body){
			AstCloner cloner;
			cloner.typeSubst=&typeSubst;
			cloner.exprSubst=&exprSubst;
			cloner.fn=def->filename;
			inst->func_def.body=cloner.cloneNode(def->func_def.body);
		}
		return inst;
	}
	
	MioType* substituteType(MioType* original,std::unordered_map<std::string,MioType*>& typeSubst){
		if(!original) return nullptr;
		if(original->kind==MioTypeKind::CLASS&&!original->name.empty()){
			auto it=typeSubst.find(original->name);
			if(it!=typeSubst.end()){
				MioType* sub=it->second;
				if(sub->kind==MioTypeKind::RVALUE_REFERENCE){
					MioType* base=sub->base_type;
					if(base&&(base->kind==MioTypeKind::REFERENCE||base->kind==MioTypeKind::RVALUE_REFERENCE)){
						return mio_type_new_reference(base,false);
					}
				}
				return new MioType(*sub);
			}
		}
		if(original->base_type){
			MioType* newBase=substituteType(original->base_type,typeSubst);
			if(original->kind==MioTypeKind::ARRAY){
				return mio_type_new_array(newBase,original->array_size);
			}else if(original->kind==MioTypeKind::POINTER){
				return mio_type_new_pointer(newBase);
			}else if(original->kind==MioTypeKind::REFERENCE||original->kind==MioTypeKind::RVALUE_REFERENCE){
				return mio_type_new_reference(newBase,original->kind==MioTypeKind::RVALUE_REFERENCE);
			}
		}
		return new MioType(*original);
	}
	
	std::string currentNamespace;
	std::string currentClassName;
	std::unordered_set<std::string> importedNamespaces;
	std::unordered_map<std::string,MioType*> locals;
	std::unordered_map<std::string,MioType*> localMioTypes;
	std::unordered_set<std::string> labels;
	int loopDepth=0;
};

#endif