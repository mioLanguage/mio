#ifndef MIO_CODEGEN_HPP
#define MIO_CODEGEN_HPP
#include"ast.hpp"
#include"llvm/IR/IRBuilder.h"
#include"llvm/IR/LLVMContext.h"
#include"llvm/IR/Module.h"
#include"llvm/IR/Verifier.h"
#include"llvm/IR/GlobalVariable.h"
#include"llvm/IR/Constants.h"
#include"llvm/IR/DerivedTypes.h"
#include"llvm/IR/DataLayout.h"
#include"llvm/Target/TargetMachine.h"
#include"llvm/Target/TargetOptions.h"
#include"llvm-c/Target.h"
#include"llvm-c/TargetMachine.h"
#include"llvm-c/Core.h"
#include"llvm/Support/FileSystem.h"
#include"llvm/TargetParser/Host.h"
#include"llvm/TargetParser/Triple.h"
#include"llvm/Support/raw_ostream.h"
#include"llvm/IR/LegacyPassManager.h"
#include"llvm/InitializePasses.h"
#include"llvm/PassRegistry.h"
#include"llvm/MC/TargetRegistry.h"
#include"llvm/Support/CodeGen.h"
#include"lld/Common/Driver.h"
LLD_HAS_DRIVER(coff);
LLD_HAS_DRIVER(elf);
LLD_HAS_DRIVER(macho);
#include<memory>
#include<vector>
#include<deque>
#include<string>
#include<cstdio>
#include<cstdlib>
#include<optional>
#include<unordered_map>
#include<unordered_set>
#include<fstream>
#ifdef _WIN32
#undef VOID
#endif


class CodeGen{
	llvm::LLVMContext ctx;
	std::unique_ptr<llvm::Module> mod;
	llvm::IRBuilder<> b;
	llvm::Function* curFn;
	llvm::BasicBlock* curBB;
	std::vector<llvm::BasicBlock*>breakStack;
	std::vector<llvm::BasicBlock*>continueStack;
	std::unordered_map<std::string,llvm::AllocaInst*>locals;
	std::unordered_map<std::string,llvm::StructType*>structTypes;
	std::unordered_map<std::string,std::unordered_map<std::string,unsigned>>structFieldIdx;
	std::unordered_map<std::string,llvm::Function*>funcDecls;
	std::unordered_map<std::string,llvm::GlobalVariable*>stringGlobals;
	std::unordered_map<std::string,llvm::GlobalVariable*>globalVars;
	std::unordered_set<std::string>enumNames;
	std::unordered_map<std::string,std::string>enumVariantMap;
	std::string modName;
	int stringCounter;
	llvm::Type* convertType(MioType* mt){
		if(!mt)return llvm::Type::getVoidTy(ctx);
		switch(mt->kind){
			case MioTypeKind::VOID:	return llvm::Type::getVoidTy(ctx);
			case MioTypeKind::I32:	return llvm::Type::getInt32Ty(ctx);
			case MioTypeKind::I64:	return llvm::Type::getInt64Ty(ctx);
			case MioTypeKind::U32:	return llvm::Type::getInt32Ty(ctx);
			case MioTypeKind::U64:	return llvm::Type::getInt64Ty(ctx);
			case MioTypeKind::F32:	return llvm::Type::getFloatTy(ctx);
			case MioTypeKind::F64:	return llvm::Type::getDoubleTy(ctx);
			case MioTypeKind::BOOL:	return llvm::Type::getInt1Ty(ctx);
			case MioTypeKind::CHAR:	return llvm::Type::getInt8Ty(ctx);
			case MioTypeKind::POINTER:
				return llvm::PointerType::get(convertType(mt->base_type),0);
			case MioTypeKind::ARRAY:{
				llvm::Type* elem=convertType(mt->base_type);
				if(mt->array_size>0)
					return llvm::ArrayType::get(elem,(unsigned)mt->array_size);
				return llvm::PointerType::get(elem,0);
			}
			case MioTypeKind::STRUCT:{
				if(!mt->name.empty()&&structTypes.count(mt->name))
					return structTypes[mt->name];
				return llvm::StructType::create(ctx,mt->name.empty()?"":mt->name);
			}
			case MioTypeKind::ENUM:
				return llvm::Type::getInt32Ty(ctx);
			case MioTypeKind::UNION:{
				if(!mt->name.empty()&&structTypes.count(mt->name))
					return structTypes[mt->name];
				return llvm::StructType::create(ctx,mt->name.empty()?"":mt->name);
			}
			case MioTypeKind::FUNC:{
				llvm::Type* ret=convertType(mt->base_type);
				std::vector<llvm::Type*> params;
				for(auto* p:mt->param_types)params.push_back(convertType(p));
				return llvm::FunctionType::get(ret,params,false);
			}
		}
		return llvm::Type::getVoidTy(ctx);
	}
	llvm::Type* resolveExprType(AstNode* node){
		if(!node)return llvm::Type::getVoidTy(ctx);
		if(node->type)return convertType(node->type);
		switch(node->kind){
			case AstNodeKind::INT_LIT:	return llvm::Type::getInt64Ty(ctx);
			case AstNodeKind::FLOAT_LIT:return llvm::Type::getDoubleTy(ctx);
			case AstNodeKind::BOOL_LIT:	return llvm::Type::getInt1Ty(ctx);
			case AstNodeKind::CHAR_LIT:	return llvm::Type::getInt8Ty(ctx);
			case AstNodeKind::STRING_LIT:return llvm::PointerType::get(ctx,0);
			case AstNodeKind::IDENT_EXPR:{
				auto it=locals.find(node->ident.name);
				if(it!=locals.end())
					return it->second->getAllocatedType();
				auto git=globalVars.find(node->ident.name);
				if(git!=globalVars.end())
					return git->second->getValueType();
				return llvm::Type::getInt64Ty(ctx);
			}
			case AstNodeKind::BINARY_EXPR:{
				llvm::Type* lt=resolveExprType(node->binary.left);
				llvm::Type* rt=resolveExprType(node->binary.right);
				if(lt->isDoubleTy()||rt->isDoubleTy())return llvm::Type::getDoubleTy(ctx);
				if(lt->isFloatTy()||rt->isFloatTy())return llvm::Type::getFloatTy(ctx);
				return lt;
			}
			case AstNodeKind::UNARY_EXPR:
				return resolveExprType(node->unary.operand);
			case AstNodeKind::CALL_EXPR:
				if(node->type)return convertType(node->type);
				return llvm::Type::getInt64Ty(ctx);
			case AstNodeKind::CAST_EXPR:
				if(node->cast_expr.target_type)return convertType(node->cast_expr.target_type);
				return llvm::Type::getInt64Ty(ctx);
			default:
				return llvm::Type::getInt64Ty(ctx);
		}
	}
	llvm::AllocaInst* createEntryAlloca(llvm::Function* fn,const std::string& name,llvm::Type* ty){
		llvm::IRBuilder<> tmp(&fn->getEntryBlock(),fn->getEntryBlock().begin());
		return tmp.CreateAlloca(ty,nullptr,name);
	}
	llvm::Value* genCastValue(llvm::Value* val,llvm::Type* targetTy){
		if(!val||!targetTy)return val;
		llvm::Type* srcTy=val->getType();
		if(srcTy==targetTy)return val;
		if(targetTy->isIntegerTy()&&srcTy->isIntegerTy()){
			if(targetTy->getIntegerBitWidth()>srcTy->getIntegerBitWidth())
				return b.CreateSExt(val,targetTy);
			return b.CreateTrunc(val,targetTy);
		}
		if(targetTy->isFloatingPointTy()&&srcTy->isIntegerTy()){
			if(srcTy->getIntegerBitWidth()==1)
				return b.CreateUIToFP(val,targetTy);
			return b.CreateSIToFP(val,targetTy);
		}
		if(targetTy->isIntegerTy()&&srcTy->isFloatingPointTy()){
			if(targetTy->getIntegerBitWidth()==1)
				return b.CreateFPToUI(val,targetTy);
			return b.CreateFPToSI(val,targetTy);
		}
		if(targetTy->isFloatingPointTy()&&srcTy->isFloatingPointTy()){
			if(targetTy->getScalarSizeInBits()>srcTy->getScalarSizeInBits())
				return b.CreateFPExt(val,targetTy);
			return b.CreateFPTrunc(val,targetTy);
		}
		if(targetTy->isPointerTy()&&srcTy->isPointerTy())
			return b.CreateBitCast(val,targetTy);
		if(targetTy->isPointerTy()&&srcTy->isIntegerTy())
			return b.CreateIntToPtr(val,targetTy);
		if(targetTy->isIntegerTy()&&srcTy->isPointerTy())
			return b.CreatePtrToInt(val,targetTy);
		return val;
	}
	llvm::Value* genCond(AstNode* node){
		llvm::Value* v=genExpr(node);
		if(!v)return llvm::ConstantInt::getFalse(ctx);
		if(v->getType()->isIntegerTy(1))return v;
		if(v->getType()->isIntegerTy())
			return b.CreateICmpNE(v,llvm::ConstantInt::get(v->getType(),0));
		if(v->getType()->isFloatingPointTy())
			return b.CreateFCmpONE(v,llvm::ConstantFP::get(v->getType(),0.0));
		return b.CreateIsNotNull(v);
	}
	llvm::Constant* genStringConst(const std::string& str){
		auto it=stringGlobals.find(str);
		if(it!=stringGlobals.end())return it->second;
		auto* charTy=llvm::Type::getInt8Ty(ctx);
		auto* strTy=llvm::ArrayType::get(charTy,str.size()+1);
		std::vector<llvm::Constant*> chars;
		for(char c:str)chars.push_back(llvm::ConstantInt::get(charTy,(unsigned char)c));
		chars.push_back(llvm::ConstantInt::get(charTy,0));
		auto* init=llvm::ConstantArray::get(strTy,chars);
		std::string name=".str."+std::to_string(stringCounter++);
		auto* gv=new llvm::GlobalVariable(*mod,strTy,true,llvm::GlobalValue::PrivateLinkage,init,name);
		gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
		gv->setAlignment(llvm::Align(1));
		stringGlobals[str]=gv;
		return gv;
	}
	llvm::Constant* genConstExpr(AstNode* expr){
		switch(expr->kind){
			case AstNodeKind::INT_LIT:
				return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),expr->int_lit.value);
			case AstNodeKind::FLOAT_LIT:
				return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx),expr->float_lit.value);
			case AstNodeKind::BOOL_LIT:
				return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx),expr->bool_lit.value?1:0);
			case AstNodeKind::CHAR_LIT:
				return llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx),(unsigned char)expr->char_lit.value);
			case AstNodeKind::STRING_LIT:
				return genStringConst(expr->string_lit.value);
			case AstNodeKind::UNARY_EXPR:{
				llvm::Constant* c=genConstExpr(expr->unary.operand);
				if(!c)return nullptr;
				if(expr->unary.op==TOK_MINUS){
					if(auto* fp=llvm::dyn_cast<llvm::ConstantFP>(c))
						return llvm::ConstantFP::get(fp->getType(),-fp->getValueAPF().convertToDouble());
					if(auto* ci=llvm::dyn_cast<llvm::ConstantInt>(c))
						return llvm::ConstantInt::get(ci->getType(),-ci->getSExtValue());
				}
				return nullptr;
			}
			default:return nullptr;
		}
	}
	void genProgram(AstNode* prog){
		for(auto* node:prog->program.nodes){
			switch(node->kind){
				case AstNodeKind::IMPORT:		break;
				case AstNodeKind::VAR_DECL:		genGlobalVar(node);break;
				case AstNodeKind::CONST_DECL:	genGlobalVar(node);break;
				case AstNodeKind::FUNC_DEF:		genFuncDef(node);break;
				case AstNodeKind::STRUCT_DEF:	genStructDef(node);break;
				case AstNodeKind::ENUM_DEF:		genEnumDef(node);break;
				case AstNodeKind::UNION_DEF:	genUnionDef(node);break;
				case AstNodeKind::MACRO_DEF:	break;
				case AstNodeKind::BLOCK:
					for(auto* stmt:node->block.stmts){
						switch(stmt->kind){
							case AstNodeKind::VAR_DECL:		genGlobalVar(stmt);break;
							case AstNodeKind::CONST_DECL:	genGlobalVar(stmt);break;
							case AstNodeKind::FUNC_DEF:		genFuncDef(stmt);break;
							case AstNodeKind::STRUCT_DEF:	genStructDef(stmt);break;
							case AstNodeKind::ENUM_DEF:		genEnumDef(stmt);break;
							case AstNodeKind::UNION_DEF:	genUnionDef(stmt);break;
							default:break;
						}
					}
					break;
				default:break;
			}
		}
	}
	void genGlobalVar(AstNode* decl){
		bool isConst=(decl->kind==AstNodeKind::CONST_DECL);
		std::string name=isConst?decl->const_decl.name:decl->var_decl.name;
		MioType* mt=isConst?decl->const_decl.var_type:decl->var_decl.var_type;
		llvm::Type* ty=convertType(mt);
		llvm::Constant* init=nullptr;
		AstNode* initExpr=isConst?decl->const_decl.init:decl->var_decl.init;
		if(initExpr){
			llvm::Value* val=genConstExpr(initExpr);
			if(val&&llvm::isa<llvm::Constant>(val))
				init=llvm::cast<llvm::Constant>(val);
		}
		if(!init)init=llvm::Constant::getNullValue(ty);
		auto* gv=new llvm::GlobalVariable(*mod,ty,isConst,llvm::GlobalValue::InternalLinkage,init,name);
		globalVars[name]=gv;
	}
	void genFuncDef(AstNode* def){
		std::string name=def->func_def.name;
		llvm::Type* retTy=convertType(def->func_def.return_type);
		std::vector<llvm::Type*> paramTys;
		for(auto& p:def->func_def.params)paramTys.push_back(convertType(p.type));
		auto* ft=llvm::FunctionType::get(retTy,paramTys,def->func_def.is_variadic);
		auto* fn=llvm::Function::Create(ft,llvm::Function::ExternalLinkage,0,name,mod.get());
		funcDecls[name]=fn;
		if(def->func_def.is_extern)return;
		size_t idx=0;
		for(auto& arg:fn->args()){
			if(idx<def->func_def.params.size())
				arg.setName(def->func_def.params[idx].name);
			idx++;
		}
		curFn=fn;
		auto* entry=llvm::BasicBlock::Create(ctx,"entry",fn);
		curBB=entry;
		b.SetInsertPoint(entry);
		locals.clear();
		for(auto& arg:fn->args()){
			auto* alloca=createEntryAlloca(fn,std::string(arg.getName()),arg.getType());
			b.CreateStore(&arg,alloca);
			locals[std::string(arg.getName())]=alloca;
		}
		if(def->func_def.body){
			genBlock(def->func_def.body);
			if(curBB&&!curBB->getTerminator()){
				if(retTy->isVoidTy())
					b.CreateRetVoid();
				else
					b.CreateRet(llvm::Constant::getNullValue(retTy));
			}
		}else{
			if(retTy->isVoidTy())
				b.CreateRetVoid();
			else
				b.CreateRet(llvm::Constant::getNullValue(retTy));
		}
		curFn=nullptr;
		curBB=nullptr;
	}
	void genStructDef(AstNode* def){
		std::string name=def->struct_def.name;
		std::vector<llvm::Type*> fieldTys;
		for(auto& f:def->struct_def.fields)fieldTys.push_back(convertType(f.type));
		auto* st=llvm::StructType::create(ctx,fieldTys,name);
		structTypes[name]=st;
		for(unsigned i=0;i<def->struct_def.fields.size();i++)
			structFieldIdx[name][def->struct_def.fields[i].name]=i;
		for(auto* m:def->struct_def.methods){
			if(m->kind==AstNodeKind::FUNC_DEF){
				m->func_def.struct_name=name;
				genFuncDef(m);
			}
		}
	}
	void genEnumDef(AstNode* def){
		enumNames.insert(def->enum_def.name);
		int val=0;
		for(auto& v:def->enum_def.variants){
			enumVariantMap[v.name]=def->enum_def.name;
			val++;
		}
	}
	void genUnionDef(AstNode* def){
		std::string name=def->union_def.name;
		llvm::Type* maxTy=llvm::Type::getInt8Ty(ctx);
		size_t maxSize=0;
		for(auto& f:def->union_def.fields){
			llvm::Type* t=convertType(f.type);
			size_t sz=mod->getDataLayout().getTypeAllocSize(t);
			if(sz>maxSize){maxSize=sz;maxTy=t;}
		}
		auto* st=llvm::StructType::create(ctx,{maxTy,llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx),(unsigned)maxSize)},name);
		structTypes[name]=st;
	}
	void genBlock(AstNode* block){
		for(auto* stmt:block->block.stmts)genStmt(stmt);
	}
	void genStmt(AstNode* stmt){
		if(!curBB||curBB->getTerminator())return;
		switch(stmt->kind){
			case AstNodeKind::VAR_DECL:		genVarDecl(stmt);break;
			case AstNodeKind::CONST_DECL:	genVarDecl(stmt);break;
			case AstNodeKind::IF_STMT:		genIfStmt(stmt);break;
			case AstNodeKind::WHILE_STMT:	genWhileStmt(stmt);break;
			case AstNodeKind::FOR_STMT:		genForStmt(stmt);break;
			case AstNodeKind::RETURN_STMT:	genReturnStmt(stmt);break;
			case AstNodeKind::EXPR_STMT:	genExprStmt(stmt);break;
			case AstNodeKind::ASSIGN_EXPR:	genAssign(stmt);break;
			case AstNodeKind::BLOCK:		genBlock(stmt);break;
			case AstNodeKind::BREAK_STMT:	genBreakStmt(stmt);break;
			case AstNodeKind::CONTINUE_STMT:genContinueStmt(stmt);break;
			case AstNodeKind::GOTO_STMT:	break;
			case AstNodeKind::LABEL_STMT:	break;
			default:{
				llvm::Value* v=genExpr(stmt);
				break;
			}
		}
	}
	void genVarDecl(AstNode* decl){
		bool isConst=(decl->kind==AstNodeKind::CONST_DECL);
		std::string name=isConst?decl->const_decl.name:decl->var_decl.name;
		MioType* mt=isConst?decl->const_decl.var_type:decl->var_decl.var_type;
		AstNode* initExpr=isConst?decl->const_decl.init:decl->var_decl.init;
		llvm::Type* ty=convertType(mt);
		auto* alloca=createEntryAlloca(curFn,name,ty);
		locals[name]=alloca;
		if(initExpr){
			llvm::Value* val=genExpr(initExpr);
			if(val){
				if(val->getType()!=ty)val=genCastValue(val,ty);
				b.CreateStore(val,alloca);
			}
		}
	}
	void genIfStmt(AstNode* stmt){
		llvm::Function* fn=curBB->getParent();
		llvm::BasicBlock* thenBB=llvm::BasicBlock::Create(ctx,"then",fn);
		llvm::BasicBlock* elseBB=nullptr;
		llvm::BasicBlock* mergeBB=llvm::BasicBlock::Create(ctx,"ifcont",fn);
		std::vector<std::pair<AstNode*,AstNode*>> elifChain;
		AstNode* curCond=stmt->if_stmt.cond;
		AstNode* curBody=stmt->if_stmt.then_body;
		for(auto* e:stmt->if_stmt.elif_list)elifChain.push_back({e->if_stmt.cond,e->if_stmt.then_body});
		AstNode* elseBody=stmt->if_stmt.else_body;
		if(!elifChain.empty()||elseBody)elseBB=llvm::BasicBlock::Create(ctx,"else",fn);
		b.CreateCondBr(genCond(curCond),thenBB,elseBB?elseBB:mergeBB);
		b.SetInsertPoint(thenBB);
		curBB=thenBB;
		genBlock(curBody);
		if(!curBB->getTerminator())b.CreateBr(mergeBB);
		if(!elifChain.empty()){
			for(size_t i=0;i<elifChain.size();i++){
				llvm::BasicBlock* elifBB=llvm::BasicBlock::Create(ctx,"elif",fn);
				llvm::BasicBlock* nextBB=(i+1<elifChain.size()||elseBody)?llvm::BasicBlock::Create(ctx,"elif.next",fn):mergeBB;
				b.SetInsertPoint(elseBB);
				curBB=elseBB;
				b.CreateCondBr(genCond(elifChain[i].first),elifBB,nextBB);
				b.SetInsertPoint(elifBB);
				curBB=elifBB;
				genBlock(elifChain[i].second);
				if(!curBB->getTerminator())b.CreateBr(mergeBB);
				elseBB=nextBB;
			}
		}
		if(elseBody){
			b.SetInsertPoint(elseBB);
			curBB=elseBB;
			genBlock(elseBody);
			if(!curBB->getTerminator())b.CreateBr(mergeBB);
		}
		b.SetInsertPoint(mergeBB);
		curBB=mergeBB;
	}
	void genWhileStmt(AstNode* stmt){
		llvm::Function* fn=curBB->getParent();
		llvm::BasicBlock* condBB=llvm::BasicBlock::Create(ctx,"while.cond",fn);
		llvm::BasicBlock* bodyBB=llvm::BasicBlock::Create(ctx,"while.body",fn);
		llvm::BasicBlock* endBB=llvm::BasicBlock::Create(ctx,"while.end",fn);
		breakStack.push_back(endBB);
		continueStack.push_back(condBB);
		b.CreateBr(condBB);
		b.SetInsertPoint(condBB);
		curBB=condBB;
		b.CreateCondBr(genCond(stmt->while_stmt.cond),bodyBB,endBB);
		b.SetInsertPoint(bodyBB);
		curBB=bodyBB;
		genBlock(stmt->while_stmt.body);
		if(!curBB->getTerminator())b.CreateBr(condBB);
		b.SetInsertPoint(endBB);
		curBB=endBB;
		breakStack.pop_back();
		continueStack.pop_back();
	}
	void genForStmt(AstNode* stmt){
		llvm::Function* fn=curBB->getParent();
		if(stmt->for_stmt.init)genStmt(stmt->for_stmt.init);
		llvm::BasicBlock* condBB=llvm::BasicBlock::Create(ctx,"for.cond",fn);
		llvm::BasicBlock* bodyBB=llvm::BasicBlock::Create(ctx,"for.body",fn);
		llvm::BasicBlock* updateBB=llvm::BasicBlock::Create(ctx,"for.update",fn);
		llvm::BasicBlock* endBB=llvm::BasicBlock::Create(ctx,"for.end",fn);
		breakStack.push_back(endBB);
		continueStack.push_back(updateBB);
		b.CreateBr(condBB);
		b.SetInsertPoint(condBB);
		curBB=condBB;
		if(stmt->for_stmt.cond)b.CreateCondBr(genCond(stmt->for_stmt.cond),bodyBB,endBB);
		else b.CreateBr(bodyBB);
		b.SetInsertPoint(bodyBB);
		curBB=bodyBB;
		genBlock(stmt->for_stmt.body);
		if(!curBB->getTerminator())b.CreateBr(updateBB);
		b.SetInsertPoint(updateBB);
		curBB=updateBB;
		if(stmt->for_stmt.update)genExpr(stmt->for_stmt.update);
		b.CreateBr(condBB);
		b.SetInsertPoint(endBB);
		curBB=endBB;
		breakStack.pop_back();
		continueStack.pop_back();
	}
	void genBreakStmt(AstNode* stmt){
		if(!breakStack.empty())b.CreateBr(breakStack.back());
	}
	void genContinueStmt(AstNode* stmt){
		if(!continueStack.empty())b.CreateBr(continueStack.back());
	}
	void genReturnStmt(AstNode* stmt){
		if(stmt->return_stmt.value){
			llvm::Value* val=genExpr(stmt->return_stmt.value);
			llvm::Type* retTy=curFn->getReturnType();
			if(val->getType()!=retTy)val=genCastValue(val,retTy);
			b.CreateRet(val);
		}else{
			b.CreateRetVoid();
		}
	}
	void genExprStmt(AstNode* stmt){
		genExpr(stmt->expr_stmt.expr);
	}
	void genAssign(AstNode* expr){
		llvm::Value* rhs=genExpr(expr->assign.right);
		if(!rhs)return;
		AstNode* lhs=expr->assign.left;
		llvm::Value* ptr=genLValue(lhs);
		if(ptr){
			llvm::Type* valTy=ptr->getType();
			if(valTy->isPointerTy()){
				llvm::Type* elemTy=resolveExprType(lhs);
				if(rhs->getType()!=elemTy)rhs=genCastValue(rhs,elemTy);
			}
			b.CreateStore(rhs,ptr);
		}
	}
	llvm::Value* genLValue(AstNode* node){
		switch(node->kind){
			case AstNodeKind::IDENT_EXPR:{
				auto it=locals.find(node->ident.name);
				if(it!=locals.end())return it->second;
				auto git=globalVars.find(node->ident.name);
				if(git!=globalVars.end())return git->second;
				return nullptr;
			}
			case AstNodeKind::INDEX_EXPR:{
				llvm::Value* base=genExpr(node->index_expr.base);
				if(!base)return nullptr;
				llvm::Value* idx=genExpr(node->index_expr.index);
				if(!idx)return nullptr;
				llvm::Type* elemTy=resolveExprType(node);
				if(!idx->getType()->isIntegerTy(64))
					idx=b.CreateSExt(idx,llvm::Type::getInt64Ty(ctx));
				return b.CreateGEP(elemTy,base,{llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0),idx});
			}
			case AstNodeKind::MEMBER_EXPR:{
				llvm::Value* base=genExpr(node->member.base);
				if(!base)return nullptr;
				std::string structName;
				if(node->member.base->type&&!node->member.base->type->name.empty())
					structName=node->member.base->type->name;
				else if(node->member.base->kind==AstNodeKind::IDENT_EXPR){
					auto it=locals.find(node->member.base->ident.name);
					if(it!=locals.end()){
						llvm::Type* at=it->second->getAllocatedType();
						if(at->isStructTy())structName=std::string(at->getStructName());
					}
				}
				if(structName.empty()||!structFieldIdx.count(structName))return nullptr;
				unsigned idx=structFieldIdx[structName][node->member.member];
				llvm::Type* elemTy=resolveExprType(node);
				return b.CreateGEP(elemTy,base,{llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0),llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),idx)});
			}
			default:return nullptr;
		}
	}
	llvm::Value* genExpr(AstNode* node){
		if(!node)return nullptr;
		switch(node->kind){
			case AstNodeKind::INT_LIT:
				return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),node->int_lit.value);
			case AstNodeKind::FLOAT_LIT:
				return llvm::ConstantFP::get(llvm::Type::getDoubleTy(ctx),node->float_lit.value);
			case AstNodeKind::BOOL_LIT:
				return llvm::ConstantInt::get(llvm::Type::getInt1Ty(ctx),node->bool_lit.value?1:0);
			case AstNodeKind::CHAR_LIT:
				return llvm::ConstantInt::get(llvm::Type::getInt8Ty(ctx),node->char_lit.value);
			case AstNodeKind::STRING_LIT:
				return genStringRef(genStringConst(node->string_lit.value));
			case AstNodeKind::IDENT_EXPR:
				return genIdentExpr(node);
			case AstNodeKind::BINARY_EXPR:
				return genBinaryExpr(node);
			case AstNodeKind::UNARY_EXPR:
				return genUnaryExpr(node);
			case AstNodeKind::CALL_EXPR:
				return genCallExpr(node);
			case AstNodeKind::INDEX_EXPR:
				return genIndexExpr(node);
			case AstNodeKind::MEMBER_EXPR:
				return genMemberExpr(node);
			case AstNodeKind::ARRAY_LIT:
				return genArrayLit(node);
			case AstNodeKind::CAST_EXPR:
				return genCastExpr(node);
			case AstNodeKind::ASSIGN_EXPR:
				return genAssignExpr(node);
			default:
				return nullptr;
		}
	}
	llvm::Value* genStringRef(llvm::Constant* gv){
		return b.CreateGEP(llvm::Type::getInt8Ty(ctx),gv,{llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0),llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0)});
	}
	llvm::Value* genIdentExpr(AstNode* node){
		std::string name=node->ident.name;
		auto it=locals.find(name);
		if(it!=locals.end()){
			llvm::Type* ty=it->second->getAllocatedType();
			return b.CreateLoad(ty,it->second,name);
		}
		auto git=globalVars.find(name);
		if(git!=globalVars.end()){
			llvm::Type* ty=git->second->getValueType();
			return b.CreateLoad(ty,git->second,name);
		}
		auto fit=funcDecls.find(name);
		if(fit!=funcDecls.end())return fit->second;
		fprintf(stderr,"error: undefined variable '%s'\n",name.c_str());
		return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0);
	}
	llvm::Value* genBinaryExpr(AstNode* node){
		llvm::Value* l=genExpr(node->binary.left);
		llvm::Value* r=genExpr(node->binary.right);
		if(!l||!r)return nullptr;
		llvm::Type* lt=l->getType();
		llvm::Type* rt=r->getType();
		bool isFloat=lt->isFloatingPointTy()||rt->isFloatingPointTy();
		if(isFloat){
			if(!lt->isFloatingPointTy())l=genCastValue(l,rt);
			if(!rt->isFloatingPointTy())r=genCastValue(r,lt);
			if(lt!=rt)r=genCastValue(r,lt);
		}else if(lt->isIntegerTy()&&rt->isIntegerTy()){
			if(lt->getIntegerBitWidth()<rt->getIntegerBitWidth())l=genCastValue(l,rt);
			else r=genCastValue(r,lt);
		}
		switch(node->binary.op){
			case TOK_PLUS:
				return isFloat?b.CreateFAdd(l,r):b.CreateAdd(l,r);
			case TOK_MINUS:
				return isFloat?b.CreateFSub(l,r):b.CreateSub(l,r);
			case TOK_STAR:
				return isFloat?b.CreateFMul(l,r):b.CreateMul(l,r);
			case TOK_SLASH:
				return isFloat?b.CreateFDiv(l,r):b.CreateSDiv(l,r);
			case TOK_PERCENT:
				return isFloat?b.CreateFRem(l,r):b.CreateSRem(l,r);
			case TOK_EQ:
				return isFloat?b.CreateFCmpOEQ(l,r):b.CreateICmpEQ(l,r);
			case TOK_NEQ:
				return isFloat?b.CreateFCmpONE(l,r):b.CreateICmpNE(l,r);
			case TOK_LT:
				return isFloat?b.CreateFCmpOLT(l,r):b.CreateICmpSLT(l,r);
			case TOK_GT:
				return isFloat?b.CreateFCmpOGT(l,r):b.CreateICmpSGT(l,r);
			case TOK_LTE:
				return isFloat?b.CreateFCmpOLE(l,r):b.CreateICmpSLE(l,r);
			case TOK_GTE:
				return isFloat?b.CreateFCmpOGE(l,r):b.CreateICmpSGE(l,r);
			case TOK_AND:
				return b.CreateLogicalAnd(l,r);
			case TOK_OR:
				return b.CreateLogicalOr(l,r);
			case TOK_BIT_AND:
				return b.CreateAnd(l,r);
			case TOK_BIT_OR:
				return b.CreateOr(l,r);
			case TOK_BIT_XOR:
				return b.CreateXor(l,r);
			case TOK_LSHIFT:
				return b.CreateShl(l,r);
			case TOK_RSHIFT:
				return b.CreateAShr(l,r);
			default:
				return nullptr;
		}
	}
	llvm::Value* genUnaryExpr(AstNode* node){
		llvm::Value* op=genExpr(node->unary.operand);
		if(!op)return nullptr;
		switch(node->unary.op){
			case TOK_MINUS:
				if(op->getType()->isFloatingPointTy())return b.CreateFNeg(op);
				return b.CreateNeg(op);
			case TOK_NOT:
				return b.CreateNot(op);
			case TOK_BIT_NOT:
				return b.CreateNot(op);
			case TOK_STAR:{
				if(op->getType()->isPointerTy()){
					llvm::Type* elemTy=resolveExprType(node->unary.operand);
					return b.CreateLoad(elemTy,op);
				}
				return op;
			}
			case TOK_BIT_AND:
				return op;
			default:
				return op;
		}
	}
	llvm::Value* genCallExpr(AstNode* node){
		std::string calleeName;
		llvm::Value* calleeVal=nullptr;
		if(node->call.callee->kind==AstNodeKind::IDENT_EXPR){
			calleeName=node->call.callee->ident.name;
			auto fit=funcDecls.find(calleeName);
			if(fit!=funcDecls.end())calleeVal=fit->second;
			if(!calleeVal){
				calleeVal=mod->getFunction(calleeName);
				if(calleeVal)funcDecls[calleeName]=llvm::cast<llvm::Function>(calleeVal);
			}
		}
		if(!calleeVal){
			calleeVal=genExpr(node->call.callee);
			if(!calleeVal)return nullptr;
		}
		auto* fn=llvm::dyn_cast<llvm::Function>(calleeVal);
		if(!fn){
			auto* ft=llvm::dyn_cast<llvm::FunctionType>(calleeVal->getType());
			if(!ft)return nullptr;
			std::vector<llvm::Value*> args;
			for(auto* a:node->call.args){
				llvm::Value* av=genExpr(a);
				if(av)args.push_back(av);
			}
			return b.CreateCall(ft,calleeVal,args);
		}
		std::vector<llvm::Value*> args;
		for(size_t i=0;i<node->call.args.size();i++){
			llvm::Value* av=genExpr(node->call.args[i]);
			if(!av)continue;
			if(i<fn->arg_size()){
				llvm::Type* paramTy=fn->getArg(i)->getType();
				if(av->getType()!=paramTy)av=genCastValue(av,paramTy);
			}
			args.push_back(av);
		}
		return b.CreateCall(fn,args);
	}
	llvm::Value* genIndexExpr(AstNode* node){
		llvm::Value* base=genExpr(node->index_expr.base);
		if(!base)return nullptr;
		llvm::Value* idx=genExpr(node->index_expr.index);
		if(!idx)return nullptr;
		llvm::Type* elemTy=resolveExprType(node);
		if(!idx->getType()->isIntegerTy(64))
			idx=b.CreateSExt(idx,llvm::Type::getInt64Ty(ctx));
		llvm::Value* ptr=b.CreateGEP(elemTy,base,{llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0),idx});
		return b.CreateLoad(elemTy,ptr);
	}
	llvm::Value* genMemberExpr(AstNode* node){
		llvm::Value* base=genExpr(node->member.base);
		if(!base)return nullptr;
		std::string structName;
		if(node->member.base->type&&!node->member.base->type->name.empty())
			structName=node->member.base->type->name;
		else if(node->member.base->kind==AstNodeKind::IDENT_EXPR){
			auto it=locals.find(node->member.base->ident.name);
			if(it!=locals.end()){
				llvm::Type* at=it->second->getAllocatedType();
				if(at->isStructTy())structName=std::string(at->getStructName());
			}
		}
		if(!structName.empty()&&structFieldIdx.count(structName)){
			unsigned idx=structFieldIdx[structName][node->member.member];
			llvm::Type* elemTy=resolveExprType(node);
			llvm::Value* ptr=b.CreateGEP(elemTy,base,{llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0),llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),idx)});
			return b.CreateLoad(elemTy,ptr);
		}
		return nullptr;
	}
	llvm::Value* genArrayLit(AstNode* node){
		llvm::Type* elemTy=nullptr;
		if(node->type&&node->type->kind==MioTypeKind::ARRAY)
			elemTy=convertType(node->type->base_type);
		if(!elemTy&&!node->array_lit.elements.empty()){
			auto* first=node->array_lit.elements[0];
			switch(first->kind){
				case AstNodeKind::INT_LIT:   elemTy=llvm::Type::getInt64Ty(ctx);break;
				case AstNodeKind::FLOAT_LIT: elemTy=llvm::Type::getDoubleTy(ctx);break;
				case AstNodeKind::BOOL_LIT:  elemTy=llvm::Type::getInt1Ty(ctx);break;
				case AstNodeKind::CHAR_LIT:  elemTy=llvm::Type::getInt8Ty(ctx);break;
				case AstNodeKind::STRING_LIT:elemTy=llvm::Type::getInt8Ty(ctx);break;
				default:                     elemTy=llvm::Type::getInt64Ty(ctx);break;
			}
		}
		if(!elemTy)elemTy=llvm::Type::getInt64Ty(ctx);
		auto* arrTy=llvm::ArrayType::get(elemTy,node->array_lit.elements.size());
		auto* alloca=createEntryAlloca(curFn,"arrlit",arrTy);
		for(size_t i=0;i<node->array_lit.elements.size();i++){
			llvm::Value* ev=genExpr(node->array_lit.elements[i]);
			if(!ev)continue;
			if(ev->getType()!=elemTy)ev=genCastValue(ev,elemTy);
			llvm::Value* ptr=b.CreateGEP(arrTy,alloca,{llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0),llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),i)});
			b.CreateStore(ev,ptr);
		}
		return b.CreateLoad(arrTy,alloca);
	}
	llvm::Value* genCastExpr(AstNode* node){
		llvm::Value* v=genExpr(node->cast_expr.expr);
		if(!v)return nullptr;
		llvm::Type* target=convertType(node->cast_expr.target_type);
		return genCastValue(v,target);
	}
	llvm::Value* genAssignExpr(AstNode* node){
		llvm::Value* rhs=genExpr(node->assign.right);
		if(!rhs)return nullptr;
		llvm::Value* ptr=genLValue(node->assign.left);
		if(ptr){
			llvm::Type* valTy=ptr->getType();
			if(valTy->isPointerTy()){
				llvm::Type* elemTy=resolveExprType(node->assign.left);
				if(rhs->getType()!=elemTy)rhs=genCastValue(rhs,elemTy);
			}
			b.CreateStore(rhs,ptr);
		}
		return rhs;
	}
public:
	CodeGen(const std::string& name):modName(name),stringCounter(0),curFn(nullptr),curBB(nullptr),b(ctx){
		mod=std::make_unique<llvm::Module>(name,ctx);
	}
	~CodeGen()=default;
	bool generate(AstNode* program){
		if(!program){
			fprintf(stderr,"error: null program\n");
			return false;
		}
		genProgram(program);
		if(llvm::verifyModule(*mod,&llvm::errs())){
			fprintf(stderr,"Error verifying module\n");
			return false;
		}
		return true;
	}
	bool emitLLVM(const std::string& path){
		std::error_code ec;
		llvm::raw_fd_ostream os(path,ec,llvm::sys::fs::OF_None);
		if(ec){
			fprintf(stderr,"error: cannot open '%s': %s\n",path.c_str(),ec.message().c_str());
			return false;
		}
		mod->print(os,nullptr);
		return true;
	}
	bool emitObject(const std::string& path){
		LLVMInitializeAllTargetInfos();
		LLVMInitializeAllTargets();
		LLVMInitializeAllTargetMCs();
		LLVMInitializeAllAsmPrinters();
		LLVMInitializeAllAsmParsers();
		std::string targetTriple=llvm::sys::getProcessTriple();
		char* errMsg=nullptr;
		LLVMTargetRef targetRef=nullptr;
		if(LLVMGetTargetFromTriple(targetTriple.c_str(),&targetRef,&errMsg)){
			fprintf(stderr,"error: %s\n",errMsg?errMsg:"unknown error");
			if(errMsg)LLVMDisposeErrorMessage(errMsg);
			return false;
		}
		LLVMTargetMachineRef tm=LLVMCreateTargetMachine(
			targetRef,targetTriple.c_str(),"generic","",
			LLVMCodeGenLevelDefault,LLVMRelocPIC,LLVMCodeModelDefault);
		if(!tm){
			fprintf(stderr,"error: cannot create target machine\n");
			return false;
		}
		auto* targetMachine=reinterpret_cast<llvm::TargetMachine*>(tm);
		mod->setDataLayout(targetMachine->createDataLayout());
		mod->setTargetTriple(llvm::Triple(targetTriple));
		if(LLVMTargetMachineEmitToFile(tm,reinterpret_cast<LLVMModuleRef>(mod.get()),const_cast<char*>(path.c_str()),LLVMObjectFile,&errMsg)){
			fprintf(stderr,"error: %s\n",errMsg?errMsg:"unknown error");
			if(errMsg)LLVMDisposeErrorMessage(errMsg);
			LLVMDisposeTargetMachine(tm);
			return false;
		}
		LLVMDisposeTargetMachine(tm);
		return true;
	}
	bool emitAssembly(const std::string& path){
		LLVMInitializeAllTargetInfos();
		LLVMInitializeAllTargets();
		LLVMInitializeAllTargetMCs();
		LLVMInitializeAllAsmPrinters();
		LLVMInitializeAllAsmParsers();
		std::string targetTriple=llvm::sys::getProcessTriple();
		char* errMsg=nullptr;
		LLVMTargetRef targetRef=nullptr;
		if(LLVMGetTargetFromTriple(targetTriple.c_str(),&targetRef,&errMsg)){
			fprintf(stderr,"error: %s\n",errMsg?errMsg:"unknown error");
			if(errMsg)LLVMDisposeErrorMessage(errMsg);
			return false;
		}
		LLVMTargetMachineRef tm=LLVMCreateTargetMachine(
			targetRef,targetTriple.c_str(),"generic","",
			LLVMCodeGenLevelDefault,LLVMRelocPIC,LLVMCodeModelDefault);
		if(!tm){
			fprintf(stderr,"error: cannot create target machine\n");
			return false;
		}
		auto* targetMachine=reinterpret_cast<llvm::TargetMachine*>(tm);
		mod->setDataLayout(targetMachine->createDataLayout());
		mod->setTargetTriple(llvm::Triple(targetTriple));
		if(LLVMTargetMachineEmitToFile(tm,reinterpret_cast<LLVMModuleRef>(mod.get()),const_cast<char*>(path.c_str()),LLVMAssemblyFile,&errMsg)){
			fprintf(stderr,"error: %s\n",errMsg?errMsg:"unknown error");
			if(errMsg)LLVMDisposeErrorMessage(errMsg);
			LLVMDisposeTargetMachine(tm);
			return false;
		}
		LLVMDisposeTargetMachine(tm);
		return true;
	}
		bool linkExecutable(const std::string& objPath,const std::string& exePath,bool staticLink=false){
		std::string triple=llvm::sys::getProcessTriple();
		llvm::Triple t(triple);
		std::deque<std::string> strStorage;
		std::vector<const char*> args;
		auto addArg=[&](const std::string& s){
			strStorage.push_back(s);
			args.push_back(strStorage.back().c_str());
		};
		switch(t.getObjectFormat()){
			case llvm::Triple::COFF:{
				addArg("mioc");
				addArg(objPath);
				addArg("/out:"+exePath);
				addArg("/subsystem:console");
				if(staticLink){
					addArg("/entry:mainCRTStartup");
					addArg("/defaultlib:libcmt");
					addArg("/defaultlib:libucrt");
					addArg("/defaultlib:libvcruntime");
					addArg("/defaultlib:legacy_stdio_definitions");
				}else{
					addArg("/entry:main");
					addArg("/defaultlib:msvcrt");
					addArg("/defaultlib:ucrt");
					addArg("/defaultlib:legacy_stdio_definitions");
					addArg("/nodefaultlib:libcmt");
				}
				return lld::coff::link(args,llvm::outs(),llvm::errs(),false,false);
			}
			case llvm::Triple::ELF:{
				addArg("mioc");
				addArg(objPath);
				addArg("-o");
				addArg(exePath);
				addArg("-l:libc.so.6");
				return lld::elf::link(args,llvm::outs(),llvm::errs(),false,false);
			}
			case llvm::Triple::MachO:{
				addArg("mioc");
				addArg(objPath);
				addArg("-o");
				addArg(exePath);
				addArg("-lSystem");
				return lld::macho::link(args,llvm::outs(),llvm::errs(),false,false);
			}
			default:{
				fprintf(stderr,"error: unsupported target for linking\n");
				return false;
			}
		}
	}
	static std::string getExeExtension(){
#ifdef _WIN32
		return ".exe";
#else
#ifdef __APPLE__
		return "";
#else
		return "";
#endif
#endif
	}
	static bool isObjectFile(const std::string& path){
		return path.size()>2&&path.substr(path.size()-2)==".o";
	}
	static bool isLLVMFile(const std::string& path){
		return path.size()>3&&path.substr(path.size()-3)==".ll";
	}
	static bool isAssemblyFile(const std::string& path){
		return path.size()>2&&path.substr(path.size()-2)==".s";
	}
};
#endif