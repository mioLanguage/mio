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
	const std::string* fn;
	AstCloner(): typeSubst(nullptr),fn(nullptr){}
	
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
			auto* cloned=new AstNode(AstNodeKind::IDENT_EXPR,node->line,node->col,fn);
			cloned->ident.name=node->ident.name;
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
			case AstNodeKind::BREAK_STMT:
			case AstNodeKind::CONTINUE_STMT:
				return new AstNode(node->kind,node->line,node->col,fn);
			default:
				return node;
		}
	}
};

class SemanticAnalyzer{
public:
	SemanticAnalyzer(){}
	
	void analyze(AstNode* prog){
		if(!prog) return;
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
					if(classTemplateMap.count(name)){
						error(node,"redefinition of class template '"+name+"'");
						return;
					}
					classTemplateMap[name]={node->template_def.type_params,node->template_def.def};
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
					classBaseMap[name]=node->class_def.base_name;
				}
				for(auto& f:node->class_def.fields){
					classFields[name].insert(f.name);
					classFieldTypes[name][f.name]=mio_type_clone(f.type);
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
				if(m->func_def.is_operator&&m->func_def.params.size()>0){
					std::string mangledFullName=fullName+"_";
					for(size_t i=0;i<m->func_def.params.size();i++){
						if(i>0)mangledFullName+="_";
						mangledFullName+=resolveClassName(mio_type_str(m->func_def.params[i].type));
					}
					funcDecls.insert(mangledFullName);
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
			}
				break;
			}
			case AstNodeKind::FUNC_DEF:{
				std::string name=node->func_def.name;
				if(!currentNamespace.empty())
					name=currentNamespace+"::"+name;
				if(funcDecls.count(name)){
					error(node,"redefinition of function '"+name+"'");
					return;
				}
				funcDecls.insert(name);
				funcDefMap[name]=node;
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
	std::unordered_set<std::string> funcDecls;
	std::unordered_map<std::string,AstNode*> funcDefMap;
	std::unordered_set<std::string> varDecls;
	std::unordered_map<std::string,MioType*> globalMioTypes;
	std::unordered_set<std::string> enumNames;
	std::unordered_set<std::string> unionNames;
	std::unordered_map<std::string,std::string> enumVariantMap;
	std::unordered_map<std::string,std::pair<std::vector<TemplateParam>,AstNode*>> classTemplateMap;
	std::unordered_map<std::string,std::pair<std::vector<TemplateParam>,AstNode*>> templateMap;
	std::unordered_set<std::string> instantiatedFuncNames;
	std::vector<AstNode*> pendingFuncInstantiations;
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
			case AstNodeKind::ENUM_DEF: break;
			case AstNodeKind::UNION_DEF: break;
			case AstNodeKind::TEMPLATE_DEF:{
				if(node->template_def.def->kind==AstNodeKind::CLASS_DEF){
					std::string name=node->template_def.def->class_def.name;
					if(!currentNamespace.empty())
						name=currentNamespace+"::"+name;
					classTemplateMap[name]={node->template_def.type_params,node->template_def.def};
				}
				break;
			}
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
		hasReturnStmt=false;
		if(!node->func_def.class_name.empty()){
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
		if(currentFuncReturnType&&currentFuncReturnType->kind!=MioTypeKind::VOID&&!hasReturnStmt&&!node->func_def.is_extern&&node->func_def.body&&node->func_def.name!=node->func_def.class_name&&node->func_def.name[0]!='~'){
			error(node->func_def.body,"non-void function '"+node->func_def.name+"' does not return a value");
		}
		locals=savedLocals;
		localMioTypes=savedMioTypes;
		currentFuncReturnType=savedReturnType;
		hasReturnStmt=savedHasReturn;
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
				checkExpr(stmt->while_stmt.cond);
				analyzeBlock(stmt->while_stmt.body);
				break;
			case AstNodeKind::FOR_STMT:{
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
			default:
				break;
		}
	}
	
	void analyzeClassDef(AstNode* node){
		if(!node) return;
		for(auto& f:node->class_def.fields){
			if(f.type){
				checkType(f.type,node);
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
			else if(lmt->kind==MioTypeKind::POINTER&&lmt->base_type&&lmt->base_type->kind==MioTypeKind::CLASS)
				className=resolveClassName(lmt->base_type->name);
		}
		if(!className.empty()){
			std::string opMethod=findOperatorMethod(className,node->binary.op,rmt);
			if(opMethod.empty()){
				error(node,"class '"+className+"' does not support operator"+tok_name(node->binary.op));
				node->type=mio_type_new(MioTypeKind::BOOL);
			}
			node->binary.resolved_op_method=opMethod;
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
			if(locals.count(calleeName)) return;
			std::string resolvedClass=resolveClassName(calleeName);
			if(classTypes.count(resolvedClass)){
				node->call.callee->ident.name=resolvedClass;
				node->call.callee->ident.namespace_name="";
				auto it=classConstructorSigs.find(resolvedClass);
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
						error(node,"no matching constructor for '"+resolvedClass+"' with "+std::to_string(argCount)+" argument(s)");
						node->type=mio_type_new_named(MioTypeKind::CLASS,resolvedClass);
					}
				}else if(node->call.args.size()>0){
					error(node,"no matching constructor for '"+resolvedClass+"'");
					node->type=mio_type_new_named(MioTypeKind::CLASS,resolvedClass);
				}
				return;
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
					auto deduced=tryDeduceTemplateArgs(calleeName,node);
					if(deduced.empty()){
						error(node,"undefined function '"+calleeName+"'");
						node->type=mio_type_new(MioTypeKind::I32);
					}else{
						node->call.template_args=deduced;
					}
				}
			}
			if(!node->call.template_args.empty()){
				std::string mangledName=calleeName;
				for(auto& ta:node->call.template_args){
					if(ta.is_type)
						mangledName+="_"+mio_type_str(ta.type_val);
					else
						mangledName+="_V";
				}
				if(instantiatedFuncNames.find(mangledName)==instantiatedFuncNames.end()){
					instantiatedFuncNames.insert(mangledName);
					AstNode* inst=instantiateStandaloneFuncTemplate(calleeName,node->call.template_args,mangledName,node);
					if(inst){
						pendingFuncInstantiations.push_back(inst);
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
				if(!found){
					error(node,"method '"+method+"' not found in class '"+className+"'");
					node->type=mio_type_new(MioTypeKind::I32);
				}else{
					checkFuncCallArgs(className+"::"+method,node);
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
			return true;
		}
		bool eInt=(eBase->kind>=MioTypeKind::I8&&eBase->kind<=MioTypeKind::U64);
		bool aInt=(aBase->kind>=MioTypeKind::I8&&aBase->kind<=MioTypeKind::U64);
		if(eInt&&aInt) return true;
		if(eBase->kind==MioTypeKind::CHAR&&aInt) return true;
		if(aBase->kind==MioTypeKind::CHAR&&eInt) return true;
		if(eBase->kind==MioTypeKind::BOOL&&aInt) return true;
		if(aBase->kind==MioTypeKind::BOOL&&eInt) return true;
		if(eBase->kind==MioTypeKind::CLASS&&aBase->kind==MioTypeKind::POINTER&&aBase->base_type&&aBase->base_type->kind==MioTypeKind::CLASS&&resolveClassName(eBase->name)==resolveClassName(aBase->base_type->name)) return true;
		if(aBase->kind==MioTypeKind::CLASS&&eBase->kind==MioTypeKind::POINTER&&eBase->base_type&&eBase->base_type->kind==MioTypeKind::CLASS&&resolveClassName(aBase->name)==resolveClassName(eBase->base_type->name)) return true;
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
				std::string opMethod=findOperatorMethod(className,TOK_LBRACKET,idxMio);
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
				std::string opMethod=findOperatorMethod(className,TOK_LBRACKET,idxMio);
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
				else if(lmt->kind==MioTypeKind::POINTER&&lmt->base_type&&lmt->base_type->kind==MioTypeKind::CLASS)
					className=resolveClassName(lmt->base_type->name);
			}
			if(!className.empty()){
				TokenKind binOp=TOK_PLUS;
				switch(node->assign.op){
					case TOK_PLUS_ASSIGN: binOp=TOK_PLUS; break;
					case TOK_MINUS_ASSIGN: binOp=TOK_MINUS; break;
					case TOK_STAR_ASSIGN: binOp=TOK_STAR; break;
					case TOK_SLASH_ASSIGN: binOp=TOK_SLASH; break;
					case TOK_PERCENT_ASSIGN: binOp=TOK_PERCENT; break;
					case TOK_AND_ASSIGN: binOp=TOK_BIT_AND; break;
					case TOK_OR_ASSIGN: binOp=TOK_BIT_OR; break;
					case TOK_XOR_ASSIGN: binOp=TOK_BIT_XOR; break;
					case TOK_LSHIFT_ASSIGN: binOp=TOK_LSHIFT; break;
					case TOK_RSHIFT_ASSIGN: binOp=TOK_RSHIFT; break;
					default: break;
				}
				MioType* rmt=resolveExprMioType(node->assign.right);
				std::string opMethod=findOperatorMethod(className,binOp,rmt);
				if(opMethod.empty()){
					error(node,"class '"+className+"' does not support operator"+tok_name(binOp));
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
				if(varDecls.count(fullName)||funcDecls.count(fullName)||classTypes.count(fullName)){
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
		if(mt->kind==MioTypeKind::CLASS&&!mt->name.empty()&&mt->param_types.empty()){
			std::string resolved=resolveClassName(mt->name);
			mt->name=resolved;
			if(enumNames.count(resolved)){
				mt->kind=MioTypeKind::ENUM;
			}else if(unionNames.count(resolved)){
				mt->kind=MioTypeKind::UNION;
			}
		}
		if(mt->kind==MioTypeKind::CLASS&&!mt->param_types.empty()){
			std::string name=mt->name;
			std::string fullName=resolveClassName(name);
			auto it=classTemplateMap.find(fullName);
			if(it==classTemplateMap.end()){
				error(ctx,"unknown template '"+name+"'");
				return;
			}
			std::string instName=fullName;
			for(auto* t:mt->param_types){
				instName+="_"+mio_type_str(t);
			}
			if(instantiatedClassNames.find(instName)==instantiatedClassNames.end()){
				instantiatedClassNames.insert(instName);
				AstNode* inst=instantiateClassTemplate(fullName,mt->param_types,instName,ctx);
				if(inst){
					instantiatedClasses[instName]=inst;
					classTypes.insert(instName);
					for(auto& f:inst->class_def.fields){
						classFields[instName].insert(f.name);
						classFieldTypes[instName][f.name]=mio_type_clone(f.type);
					}
					for(auto* m:inst->class_def.methods){
						std::string mname=m->func_def.name;
						classMethodSet[instName].insert(mname);
						std::string fullMethodName=instName+"::"+mname;
						funcDecls.insert(fullMethodName);
						funcDefMap[fullMethodName]=m;
					}
					for(auto* c:inst->class_def.constructors){
						std::string ctorName=instName+"::"+inst->class_def.name;
						std::string sig;
						for(size_t pi=0;pi<c->func_def.params.size();pi++){
							if(pi>0)sig+=",";
							sig+=mio_type_str(c->func_def.params[pi].type);
						}
						classConstructorSigs[instName].push_back({ctorName,sig});
						funcDecls.insert(ctorName);
						funcDefMap[ctorName]=c;
					}
					if(inst->class_def.destructor){
						std::string dtorName=instName+"::~"+inst->class_def.name;
						funcDecls.insert(dtorName);
						funcDefMap[dtorName]=inst->class_def.destructor;
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
	
	std::string resolveClassName(const std::string& name){
		if(name.find("::")!=std::string::npos) return name;
		for(auto& ns:importedNamespaces){
			std::string fullName=ns+"::"+name;
			if(classTemplateMap.count(fullName)||classTypes.count(fullName)){
				return fullName;
			}
		}
		if(!currentNamespace.empty()){
			std::string fullName=currentNamespace+"::"+name;
			if(classTemplateMap.count(fullName)||classTypes.count(fullName)){
				return fullName;
			}
		}
		return name;
	}
	
	std::string findOperatorMethod(const std::string& className,TokenKind op,MioType* rightType){
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
		std::vector<std::string> candidates;
		candidates.push_back(className+"::"+opName+"_"+rightTypeStr);
		candidates.push_back(className+"::"+opName);
		size_t nsSep=className.find("::");
		if(nsSep!=std::string::npos){
			std::string shortName=className.substr(nsSep+2);
			candidates.push_back(shortName+"::"+opName+"_"+rightTypeStr);
			candidates.push_back(shortName+"::"+opName);
		}
		for(auto& c:candidates){
			if(funcDecls.count(c)){
				return c;
			}
		}
		for(auto& kv:funcDecls){
			std::string prefix=className+"::"+opName+"_";
			if(kv.size()>prefix.size()&&kv.substr(0,prefix.size())==prefix){
				return kv;
			}
			size_t nsSep2=className.find("::");
			if(nsSep2!=std::string::npos){
				std::string shortName2=className.substr(nsSep2+2);
				std::string prefix2=shortName2+"::"+opName+"_";
				if(kv.size()>prefix2.size()&&kv.substr(0,prefix2.size())==prefix2){
					return kv;
				}
			}
		}
		if(funcDecls.count(opName))return opName;
		return "";
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
	
	AstNode* instantiateClassTemplate(const std::string& name,std::vector<MioType*>& typeArgs,const std::string& instName,AstNode* ctx){
		auto it=classTemplateMap.find(name);
		if(it==classTemplateMap.end()){
			error(ctx,"unknown template '"+name+"'");
			return nullptr;
		}
		auto* templateDef=it->second.second;
		auto& typeParams=it->second.first;
		if(typeArgs.size()!=typeParams.size()){
			error(ctx,"template '"+name+"' requires "+std::to_string(typeParams.size())+" arguments,got "+std::to_string(typeArgs.size()));
			return nullptr;
		}
		std::unordered_map<std::string,MioType*> typeSubst;
		for(size_t i=0;i<typeParams.size();i++){
			typeSubst[typeParams[i].name]=typeArgs[i];
		}
		auto* inst=instantiateTemplate(templateDef,typeSubst,instName);
		if(!inst) return nullptr;
		inst->class_def.name=instName;
		return inst;
	}
	
	AstNode* instantiateTemplate(AstNode* templateDef,std::unordered_map<std::string,MioType*>& typeSubst,const std::string& instClassName){
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
			auto* nm=instantiateFuncTemplate(m,typeSubst,instClassName);
			if(nm) inst->class_def.methods.push_back(nm);
		}
		for(auto* c:def->class_def.constructors){
			auto* nc=instantiateFuncTemplate(c,typeSubst,instClassName);
			if(nc) inst->class_def.constructors.push_back(nc);
		}
		if(def->class_def.destructor){
			inst->class_def.destructor=instantiateFuncTemplate(def->class_def.destructor,typeSubst,instClassName);
		}
		return inst;
	}
	
	AstNode* instantiateFuncTemplate(AstNode* funcDef,std::unordered_map<std::string,MioType*>& typeSubst,const std::string& instClassName){
		if(funcDef->kind!=AstNodeKind::FUNC_DEF) return nullptr;
		auto* def=funcDef;
		MioType* retType=substituteType(def->func_def.return_type,typeSubst);
		if(retType&&retType->kind==MioTypeKind::CLASS&&!retType->name.empty()){
			auto it=typeSubst.find(retType->name);
			if(it==typeSubst.end()){
				retType->name=instClassName;
			}
		}
		auto* inst=ast_new_func_def(def->func_def.name,retType,nullptr,def->func_def.is_static,def->line,def->col,def->filename);
		inst->func_def.class_name=instClassName;
		for(auto& p:def->func_def.params){
			MioType* pt=substituteType(p.type,typeSubst);
			inst->func_def.params.push_back({p.name,pt,nullptr});
		}
		if(def->func_def.body){
			AstCloner cloner;
			cloner.typeSubst=&typeSubst;
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
		for(size_t i=0;i<typeParams.size();i++){
			if(typeParams[i].is_type){
				typeSubst[typeParams[i].name]=templateArgs[i].type_val;
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
	std::unordered_set<std::string> importedNamespaces;
	std::unordered_map<std::string,MioType*> locals;
	std::unordered_map<std::string,MioType*> localMioTypes;
};

#endif