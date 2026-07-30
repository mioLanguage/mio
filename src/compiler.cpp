#ifndef MIO_COMPILER_HPP
#define MIO_COMPILER_HPP
#include"token.hpp"
#include"lexer.hpp"
#include"parser.hpp"
#include"ast.hpp"
#include"llvm/IR/IRBuilder.h"
#include"llvm/IR/LLVMContext.h"
#include"llvm/IR/Module.h"
#include"llvm/IR/Verifier.h"
#include"llvm/IR/GlobalVariable.h"
#include"llvm/IR/Constants.h"
#include"llvm/IR/DerivedTypes.h"
#include"llvm/IR/Instructions.h"
#include"llvm/Support/Casting.h"
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
#include<sys/stat.h>
#ifdef _WIN32
#undef VOID
#endif
extern int g_error_count;

class Compiler{
	llvm::LLVMContext ctx;
	std::unique_ptr<llvm::Module> mod;
	llvm::IRBuilder<> b;
	llvm::Function* curFn;
	llvm::BasicBlock* curBB;
	llvm::AllocaInst* thisAlloca;
	std::string currentClassName;
	std::string currentNamespace;
	std::unordered_set<std::string> importedNamespaces;
	std::unordered_map<std::string,std::string> namespaceMembers;
	std::vector<llvm::BasicBlock*>breakStack;
	std::vector<llvm::BasicBlock*>continueStack;
	std::unordered_map<std::string,llvm::AllocaInst*>locals;
	std::unordered_map<std::string,MioType*>localMioTypes;
	std::unordered_map<std::string,llvm::StructType*>structTypes;
	std::unordered_map<std::string,std::unordered_map<std::string,unsigned>>structFieldIdx;
	std::unordered_map<std::string,std::unordered_map<std::string,MioType*>>structFieldTypes;
	std::unordered_map<std::string,llvm::Function*>funcDecls;
	std::unordered_map<std::string,llvm::GlobalVariable*>stringGlobals;
	std::unordered_map<std::string,llvm::GlobalVariable*>globalVars;
	std::unordered_set<std::string>enumNames;
	std::unordered_map<std::string,std::string>enumVariantMap;
	std::unordered_map<std::string,std::vector<std::pair<std::string,llvm::Function*>>>classVTable;
	std::unordered_map<std::string,std::string>classBaseMap;
	std::unordered_map<std::string,std::string>classBaseAccessMap;
	std::unordered_map<std::string,std::string>classDestructorMap;
	std::unordered_map<std::string,llvm::StructType*>classVTableTypes;
	std::unordered_map<std::string,llvm::GlobalVariable*>classVTableGlobals;
	std::unordered_map<std::string,std::vector<std::string>>classVTableOrder;
	std::unordered_map<std::string,std::vector<llvm::FunctionType*>>classVTableFuncTypes;
	std::vector<std::pair<std::string,llvm::AllocaInst*>>cleanupStack;
	std::unordered_map<std::string,std::pair<std::vector<TemplateParam>,AstNode*>>templateMap;
	std::unordered_map<std::string,std::pair<std::string,std::vector<TemplateArg>>>templateInstances;
	std::unordered_map<std::string,std::pair<std::vector<TemplateParam>,AstNode*>>classTemplateMap;
	std::unordered_set<std::string>classTemplateInstances;
	std::unordered_map<std::string,AstNode*>funcDefMap;
	int optLevel;
	std::string modName;
	std::string filename;
	int stringCounter;
	llvm::Type* convertType(MioType* mt){
		if(!mt)return llvm::Type::getVoidTy(ctx);
		switch(mt->kind){
			case MioTypeKind::VOID:	return llvm::Type::getVoidTy(ctx);
			case MioTypeKind::I8:	return llvm::Type::getInt8Ty(ctx);
			case MioTypeKind::I16:	return llvm::Type::getInt16Ty(ctx);
			case MioTypeKind::I32:	return llvm::Type::getInt32Ty(ctx);
			case MioTypeKind::I64:	return llvm::Type::getInt64Ty(ctx);
			case MioTypeKind::I128:	return llvm::Type::getInt128Ty(ctx);
			case MioTypeKind::U8:	return llvm::Type::getInt8Ty(ctx);
			case MioTypeKind::U16:	return llvm::Type::getInt16Ty(ctx);
			case MioTypeKind::U32:	return llvm::Type::getInt32Ty(ctx);
			case MioTypeKind::U64:	return llvm::Type::getInt64Ty(ctx);
			case MioTypeKind::U128:	return llvm::Type::getInt128Ty(ctx);
			case MioTypeKind::USIZE:	return llvm::Type::getInt64Ty(ctx);
			case MioTypeKind::ISIZE:	return llvm::Type::getInt64Ty(ctx);
			case MioTypeKind::F32:	return llvm::Type::getFloatTy(ctx);
			case MioTypeKind::F64:	return llvm::Type::getDoubleTy(ctx);
			case MioTypeKind::BOOL:	return llvm::Type::getInt1Ty(ctx);
			case MioTypeKind::CHAR:	return llvm::Type::getInt8Ty(ctx);
			case MioTypeKind::POINTER: return llvm::PointerType::get(ctx,0);
			case MioTypeKind::REFERENCE:
				return llvm::PointerType::get(ctx,0);
			case MioTypeKind::ARRAY:{
					llvm::Type* elem=convertType(mt->base_type);
					if(mt->array_size>0)
						return llvm::ArrayType::get(elem,(unsigned)mt->array_size);
					return llvm::PointerType::get(ctx,0);
				}
			case MioTypeKind::STRUCT:{
				if(!mt->name.empty()){
					if(!mt->param_types.empty()){
						std::string instName=resolveStructName(mt->name);
						for(auto* pt:mt->param_types)
							instName+="_"+mio_type_str(pt);
						if(structTypes.count(instName))
							return structTypes[instName];
						instantiateClassTemplate(mt->name,mt->param_types,instName);
						if(structTypes.count(instName))
							return structTypes[instName];
						error(mt->line,mt->col,"unknown template type '"+instName+"'");
						return llvm::Type::getVoidTy(ctx);
					}
					
					llvm::StructType* st=findStructType(mt->name);
					if(st)return st;
					error(mt->line,mt->col,"unknown type '"+mt->name+"'");
					return llvm::Type::getVoidTy(ctx);
				}
				return llvm::StructType::create(ctx,"");
			}
			case MioTypeKind::ENUM:
				return llvm::Type::getInt32Ty(ctx);
			case MioTypeKind::UNION:{
				if(!mt->name.empty()){
					llvm::StructType* st=findStructType(mt->name);
					if(st)return st;
				}
				return llvm::StructType::create(ctx,mt->name.empty()?"":mt->name);
			}
			case MioTypeKind::FUNC:{
				llvm::Type* ret=convertType(mt->base_type);
				std::vector<llvm::Type*> params;
				for(auto* p:mt->param_types)params.push_back(convertType(p));
				return llvm::FunctionType::get(ret,params,false);
			}
		}
		error("unknown type kind "+std::to_string((int)mt->kind)+" in code generation");
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
				std::string name=node->ident.name;
				std::string ns=node->ident.namespace_name;
				if(ns=="::"){
					auto git=globalVars.find(name);
					if(git!=globalVars.end())
						return git->second->getValueType();
					error(node->line,node->col,"undefined global variable '"+name+"'");
					return llvm::Type::getInt64Ty(ctx);
				}
				std::string fullName=resolveNamespaceName(name,ns);
				auto it=locals.find(fullName);
				if(it!=locals.end())
					return it->second->getAllocatedType();
				auto git=globalVars.find(fullName);
				if(git!=globalVars.end())
					return git->second->getValueType();
				llvm::GlobalVariable* gv=nullptr;
				std::string foundName=findInImportedNs(name,globalVars,gv);
				if(!foundName.empty())
					return gv->getValueType();
				return llvm::Type::getInt64Ty(ctx);
			}
			case AstNodeKind::BINARY_EXPR:{
				llvm::Type* lt=resolveExprType(node->binary.left);
				llvm::Type* rt=resolveExprType(node->binary.right);
				if(lt->isDoubleTy()||rt->isDoubleTy())return llvm::Type::getDoubleTy(ctx);
				if(lt->isFloatTy()||rt->isFloatTy())return llvm::Type::getFloatTy(ctx);
				return lt;
			}
			case AstNodeKind::UNARY_EXPR:{
				if(node->unary.op==TOK_STAR){
					MioType* operandMio=resolveExprMioType(node->unary.operand);
					if(operandMio&&operandMio->kind==MioTypeKind::POINTER&&operandMio->base_type)
						return convertType(operandMio->base_type);
				}
				return resolveExprType(node->unary.operand);
			}
			case AstNodeKind::CALL_EXPR:
				if(node->type)return convertType(node->type);
				return llvm::Type::getInt64Ty(ctx);
			case AstNodeKind::CAST_EXPR:
				if(node->cast_expr.target_type)return convertType(node->cast_expr.target_type);
				return llvm::Type::getInt64Ty(ctx);
			case AstNodeKind::MEMBER_EXPR:{
				if(node->type)return convertType(node->type);
				std::string sn;
				if(node->member.base->type&&!node->member.base->type->name.empty())
					sn=resolveStructName(node->member.base->type->name);
				else if(node->member.base->kind==AstNodeKind::IDENT_EXPR){
					if(node->member.base->ident.name=="this"&&!currentClassName.empty())
						sn=currentClassName;
					else{
						std::string vname=node->member.base->ident.name;
						if(!node->member.base->ident.namespace_name.empty())
							vname=node->member.base->ident.namespace_name+"::"+vname;
						auto it=locals.find(vname);
						if(it!=locals.end()){
							llvm::Type* at=it->second->getAllocatedType();
							if(at->isStructTy())sn=std::string(at->getStructName());
						}
					}
				}
				if(!sn.empty()&&structFieldIdx.count(sn)){
					auto mit=structFieldIdx[sn].find(node->member.member);
					if(mit!=structFieldIdx[sn].end()){
						auto sit=structTypes.find(sn);
						if(sit!=structTypes.end()){
							unsigned idx=mit->second;
							return sit->second->getElementType(idx);
						}
					}
				}
				if(!sn.empty()){
					error(node->line,node->col,"field '"+node->member.member+"' not found in struct '"+sn+"'");
				}else{
					error(node->line,node->col,"cannot resolve type for member access '"+node->member.member+"'");
				}
				return llvm::Type::getInt64Ty(ctx);
			}
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
			default:
				error(expr->line,expr->col,"unsupported expression in constant context");
				return nullptr;
		}
	}
	std::string mangleName(const std::string& name){
		if(currentNamespace.empty())return name;
		return currentNamespace+"::"+name;
	}
	
	
	
	std::string resolveNamespaceName(const std::string& name,const std::string& explicitNs){
		
		if(explicitNs=="::")return name;
		
		if(!explicitNs.empty())return explicitNs+"::"+name;
		
		if(!currentNamespace.empty()){
			std::string fullName=currentNamespace+"::"+name;
			return fullName;
		}
		
		return name;
	}
	
	
	template<typename T>
	std::string findInImportedNs(const std::string& name,const std::unordered_map<std::string,T>& map,T& outResult){
		for(auto& impNs:importedNamespaces){
			std::string fullName=impNs+"::"+name;
			auto it=map.find(fullName);
			if(it!=map.end()){
				outResult=it->second;
				return fullName;
			}
		}
		return "";
	}
	
	
	template<typename T>
	bool existsInMap(const std::string& name,const std::unordered_map<std::string,T>& map){
		return map.find(name)!=map.end();
	}
	
	
	llvm::StructType* findStructType(const std::string& name){
		if(name.empty())return nullptr;
		
		
		auto it=structTypes.find(name);
		if(it!=structTypes.end())return it->second;
		
		
		if(!currentNamespace.empty()){
			std::string fullName=currentNamespace+"::"+name;
			it=structTypes.find(fullName);
			if(it!=structTypes.end())return it->second;
		}
		
		
		for(auto& impNs:importedNamespaces){
			std::string fullName=impNs+"::"+name;
			it=structTypes.find(fullName);
			if(it!=structTypes.end())return it->second;
		}
		
		return nullptr;
	}
	
	void genProgram(AstNode* prog){
		for(auto* node:prog->program.nodes){
			switch(node->kind){
				case AstNodeKind::IMPORT:break;
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
						switch(decl->kind){
							case AstNodeKind::VAR_DECL:		genGlobalVar(decl);break;
							case AstNodeKind::CONST_DECL:	genGlobalVar(decl);break;
							case AstNodeKind::FUNC_DEF:		genFuncDef(decl);break;
							case AstNodeKind::STRUCT_DEF:	genStructDef(decl);break;
							case AstNodeKind::ENUM_DEF:		genEnumDef(decl);break;
							case AstNodeKind::UNION_DEF:	genUnionDef(decl);break;
							case AstNodeKind::CLASS_DEF:	genClassDef(decl);break;
							case AstNodeKind::TEMPLATE_DEF:	registerTemplate(decl);break;
							case AstNodeKind::NAMESPACE_DEF:	genProgram(decl);break;
							default:break;
						}
					}
					currentNamespace=savedNs;
					break;
				}
				case AstNodeKind::VAR_DECL:		genGlobalVar(node);break;
				case AstNodeKind::CONST_DECL:	genGlobalVar(node);break;
				case AstNodeKind::FUNC_DEF:		genFuncDef(node);break;
				case AstNodeKind::STRUCT_DEF:	genStructDef(node);break;
				case AstNodeKind::ENUM_DEF:		genEnumDef(node);break;
				case AstNodeKind::UNION_DEF:	genUnionDef(node);break;
				case AstNodeKind::CLASS_DEF:	genClassDef(node);break;
				case AstNodeKind::MACRO_DEF:	break;
				case AstNodeKind::TEMPLATE_DEF:
					registerTemplate(node);
					break;
				case AstNodeKind::BLOCK:
				for(auto* stmt:node->block.stmts){
					switch(stmt->kind){
						case AstNodeKind::BLOCK:{
							auto* saved=node;
							node=stmt;
							for(auto* s:stmt->block.stmts){
								switch(s->kind){
									case AstNodeKind::VAR_DECL:		genGlobalVar(s);break;
									case AstNodeKind::CONST_DECL:	genGlobalVar(s);break;
									case AstNodeKind::FUNC_DEF:		genFuncDef(s);break;
									case AstNodeKind::STRUCT_DEF:	genStructDef(s);break;
									case AstNodeKind::ENUM_DEF:		genEnumDef(s);break;
									case AstNodeKind::UNION_DEF:	genUnionDef(s);break;
									case AstNodeKind::CLASS_DEF:	genClassDef(s);break;
									case AstNodeKind::TEMPLATE_DEF:	registerTemplate(s);break;
									case AstNodeKind::NAMESPACE_DEF:{
										std::string savedNs=currentNamespace;
										if(!currentNamespace.empty())
											currentNamespace=currentNamespace+"::"+s->namespace_def.name;
										else
											currentNamespace=s->namespace_def.name;
										for(auto* decl:s->namespace_def.body){
											switch(decl->kind){
												case AstNodeKind::VAR_DECL:		genGlobalVar(decl);break;
												case AstNodeKind::CONST_DECL:	genGlobalVar(decl);break;
												case AstNodeKind::FUNC_DEF:		genFuncDef(decl);break;
												case AstNodeKind::STRUCT_DEF:	genStructDef(decl);break;
												case AstNodeKind::ENUM_DEF:		genEnumDef(decl);break;
												case AstNodeKind::UNION_DEF:	genUnionDef(decl);break;
												case AstNodeKind::CLASS_DEF:	genClassDef(decl);break;
												case AstNodeKind::TEMPLATE_DEF:	registerTemplate(decl);break;
												case AstNodeKind::NAMESPACE_DEF:	genProgram(decl);break;
												default:break;
											}
										}
										currentNamespace=savedNs;
										break;
									}
									case AstNodeKind::NAMESPACE_IMPORT:{
										std::string ns=s->namespace_import.namespace_name;
										importedNamespaces.insert(ns);
										break;
									}
									default:break;
								}
							}
							node=saved;
							break;
						}
						case AstNodeKind::VAR_DECL:		genGlobalVar(stmt);break;
						case AstNodeKind::CONST_DECL:	genGlobalVar(stmt);break;
						case AstNodeKind::FUNC_DEF:		genFuncDef(stmt);break;
						case AstNodeKind::STRUCT_DEF:	genStructDef(stmt);break;
						case AstNodeKind::ENUM_DEF:		genEnumDef(stmt);break;
						case AstNodeKind::UNION_DEF:	genUnionDef(stmt);break;
						case AstNodeKind::CLASS_DEF:	genClassDef(stmt);break;
						case AstNodeKind::TEMPLATE_DEF:	registerTemplate(stmt);break;
						case AstNodeKind::NAMESPACE_DEF:{
							std::string savedNs=currentNamespace;
							if(!currentNamespace.empty())
								currentNamespace=currentNamespace+"::"+stmt->namespace_def.name;
							else
								currentNamespace=stmt->namespace_def.name;
							for(auto* decl:stmt->namespace_def.body){
								switch(decl->kind){
									case AstNodeKind::VAR_DECL:		genGlobalVar(decl);break;
									case AstNodeKind::CONST_DECL:	genGlobalVar(decl);break;
									case AstNodeKind::FUNC_DEF:		genFuncDef(decl);break;
									case AstNodeKind::STRUCT_DEF:	genStructDef(decl);break;
									case AstNodeKind::ENUM_DEF:		genEnumDef(decl);break;
									case AstNodeKind::UNION_DEF:	genUnionDef(decl);break;
									case AstNodeKind::CLASS_DEF:	genClassDef(decl);break;
									case AstNodeKind::TEMPLATE_DEF:	registerTemplate(decl);break;
									case AstNodeKind::NAMESPACE_DEF:	genProgram(decl);break;
									default:break;
								}
							}
							currentNamespace=savedNs;
							break;
						}
						case AstNodeKind::NAMESPACE_IMPORT:{
								std::string ns=stmt->namespace_import.namespace_name;
								importedNamespaces.insert(ns);
								break;
							}
							default:break;
						}
					}
					break;
				default:
					error(node->line,node->col,"unexpected node kind "+std::to_string((int)node->kind)+" at top level");
					break;
			}
		}
	}
	void registerTemplate(AstNode* node){
		std::string name;
		if(node->template_def.def->kind==AstNodeKind::FUNC_DEF){
			name=node->template_def.def->func_def.name;
			if(!currentNamespace.empty())
				name=currentNamespace+"::"+name;
			if(templateMap.count(name)){
				error(node->line,node->col,"redefinition of template '"+name+"'");
				return;
			}
			templateMap[name]={node->template_def.type_params,node->template_def.def};
		}else if(node->template_def.def->kind==AstNodeKind::CLASS_DEF){
			name=node->template_def.def->class_def.name;
			if(!currentNamespace.empty())
				name=currentNamespace+"::"+name;
			if(classTemplateMap.count(name)){
				error(node->line,node->col,"redefinition of class template '"+name+"'");
				return;
			}
			classTemplateMap[name]={node->template_def.type_params,node->template_def.def};
		}else{
			error(node->line,node->col,"template only supports functions and classes");
			return;
		}
	}
	int64_t evalConstExpr(AstNode* node){
		if(!node)return 0;
		if(node->kind==AstNodeKind::INT_LIT)return node->int_lit.value;
		if(node->kind==AstNodeKind::BOOL_LIT)return node->bool_lit.value?1:0;
		if(node->kind==AstNodeKind::CHAR_LIT)return (int64_t)node->char_lit.value;
		if(node->kind==AstNodeKind::UNARY_EXPR&&node->unary.op==TOK_MINUS)
			return -evalConstExpr(node->unary.operand);
		if(node->kind==AstNodeKind::BINARY_EXPR){
			int64_t l=evalConstExpr(node->binary.left);
			int64_t r=evalConstExpr(node->binary.right);
			switch(node->binary.op){
				case TOK_PLUS:return l+r;
				case TOK_MINUS:return l-r;
				case TOK_STAR:return l*r;
				case TOK_SLASH:return r!=0?l/r:0;
				case TOK_PERCENT:return r!=0?l%r:0;
				default:
					error(node->line,node->col,"unsupported operator in constant expression");
					return 0;
			}
		}
		return 0;
	}
	llvm::Value* genTemplateInstantiation(AstNode* node){
		std::string calleeName=node->call.callee->ident.name;
		std::string ns=node->call.callee->ident.namespace_name;
		if(!ns.empty()&&ns!="::")
			calleeName=ns+"::"+calleeName;
		auto it=templateMap.find(calleeName);
		if(it==templateMap.end()){
			error(node->line,node->col,"template '"+calleeName+"' not found");
			return nullptr;
		}
		std::string templateNs=ns;
		if(!ns.empty()&&ns!="::"){
			templateNs=ns;
		}else{
			templateNs="";
		}
		auto& params=it->second.first;
		auto& templateDef=it->second.second;
		if(node->call.template_args.size()!=params.size()){
			error(node->line,node->col,"template '"+calleeName+"' expects "+std::to_string(params.size())+" arguments, got "+std::to_string(node->call.template_args.size()));
			return nullptr;
		}
		std::string mangledName=calleeName;
		for(auto& ta:node->call.template_args){
			if(ta.is_type)
				mangledName+="_"+mio_type_str(ta.type_val);
			else
				mangledName+="_V";
		}
		auto instIt=templateInstances.find(mangledName);
		if(instIt!=templateInstances.end()){
			llvm::Function* fn=mod->getFunction(mangledName);
			if(fn)return fn;
			fn=funcDecls[mangledName];
			if(fn)return fn;
		}
		std::string savedNs=currentNamespace;
		auto savedFn=curFn;
		auto savedBB=curBB;
		auto savedThis=thisAlloca;
		auto savedClassName=currentClassName;
		auto savedLocals=std::move(locals);
		auto savedMioTypes=std::move(localMioTypes);
		auto savedCleanup=std::move(cleanupStack);
		locals.clear();
		localMioTypes.clear();
		cleanupStack.clear();
		std::unordered_map<std::string,MioType*> typeSubst;
		std::unordered_map<std::string,int64_t> valueSubst;
		for(size_t i=0;i<params.size();i++){
			auto& ta=node->call.template_args[i];
			if(params[i].is_type){
				if(!ta.is_type){
					error(node->line,node->col,"template parameter '"+params[i].name+"' expects a type argument");
					return nullptr;
				}
				typeSubst[params[i].name]=ta.type_val;
			}else{
				if(ta.is_type){
					error(node->line,node->col,"template parameter '"+params[i].name+"' expects a value argument");
					return nullptr;
				}
				valueSubst[params[i].name]=evalConstExpr(ta.expr_val);
			}
		}
		AstNode* instDef=instantiateTemplate(templateDef,typeSubst,valueSubst);
		if(!instDef){
			currentNamespace=savedNs;
			locals=std::move(savedLocals);
			localMioTypes=std::move(savedMioTypes);
			cleanupStack=std::move(savedCleanup);
			curFn=savedFn;
			curBB=savedBB;
			if(savedBB)b.SetInsertPoint(savedBB);
			thisAlloca=savedThis;
			currentClassName=savedClassName;
			return nullptr;
		}
		currentNamespace=templateNs;
		genFuncDef(instDef);
		locals=std::move(savedLocals);
		localMioTypes=std::move(savedMioTypes);
		cleanupStack=std::move(savedCleanup);
		curFn=savedFn;
		curBB=savedBB;
		if(savedBB)b.SetInsertPoint(savedBB);
		thisAlloca=savedThis;
		currentClassName=savedClassName;
		currentNamespace=savedNs;
		llvm::Function* fn=mod->getFunction(mangledName);
		if(fn)funcDecls[mangledName]=fn;
		templateInstances[mangledName]={calleeName,node->call.template_args};
		return fn;
	}
	MioType* llvmTypeToMioType(llvm::Type* ty){
		if(!ty)return nullptr;
		if(ty->isIntegerTy(1))return mio_type_new(MioTypeKind::BOOL);
		if(ty->isIntegerTy(8))return mio_type_new(MioTypeKind::I8);
		if(ty->isIntegerTy(16))return mio_type_new(MioTypeKind::I16);
		if(ty->isIntegerTy(32))return mio_type_new(MioTypeKind::I32);
		if(ty->isIntegerTy(64))return mio_type_new(MioTypeKind::I64);
		if(ty->isFloatTy())return mio_type_new(MioTypeKind::F32);
		if(ty->isDoubleTy())return mio_type_new(MioTypeKind::F64);
		if(ty->isPointerTy()){
		llvm::Type* elemTy=nullptr;
		if(ty->getNumContainedTypes()>0)
			elemTy=ty->getContainedType(0);
		MioType* mioElemTy=elemTy?llvmTypeToMioType(elemTy):nullptr;
		return mio_type_new_pointer(mioElemTy?mioElemTy:mio_type_new(MioTypeKind::VOID));
	}
		if(auto* st=llvm::dyn_cast<llvm::StructType>(ty)){
			std::string name=st->getName().str();
			if(!name.empty())return mio_type_new_named(MioTypeKind::STRUCT,name);
		}
		return nullptr;
	}
	std::string findOperatorMethod(const std::string& structName,TokenKind op,MioType* rightType){
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
		candidates.push_back(structName+"::"+opName+"_"+rightTypeStr);
		candidates.push_back(structName+"::"+opName);
		size_t nsSep=structName.find("::");
		if(nsSep!=std::string::npos){
			std::string shortName=structName.substr(nsSep+2);
			candidates.push_back(shortName+"::"+opName+"_"+rightTypeStr);
			candidates.push_back(shortName+"::"+opName);
		}
		for(auto& c:candidates){
			if(funcDecls.count(c))return c;
		}
		for(auto& kv:funcDecls){
			std::string prefix=structName+"::"+opName+"_";
			if(kv.first.size()>prefix.size()&&kv.first.substr(0,prefix.size())==prefix){
				return kv.first;
			}
			size_t nsSep2=structName.find("::");
			if(nsSep2!=std::string::npos){
				std::string shortName2=structName.substr(nsSep2+2);
				std::string prefix2=shortName2+"::"+opName+"_";
				if(kv.first.size()>prefix2.size()&&kv.first.substr(0,prefix2.size())==prefix2){
					return kv.first;
				}
			}
		}
		if(funcDecls.count(opName))return opName;
		return "";
	}
	MioType* resolveExprMioType(AstNode* node){
		if(!node)return nullptr;
		if(node->type)return node->type;
		switch(node->kind){
			case AstNodeKind::UNARY_EXPR:
				if(node->unary.op==TOK_BIT_AND){
					MioType* inner=resolveExprMioType(node->unary.operand);
					return inner?mio_type_new_pointer(inner):nullptr;
				}
				if(node->unary.op==TOK_STAR){
					MioType* inner=resolveExprMioType(node->unary.operand);
					if(inner&&inner->kind==MioTypeKind::POINTER)return inner->base_type;
					return nullptr;
				}
				return resolveExprMioType(node->unary.operand);
			case AstNodeKind::INDEX_EXPR:{
				MioType* baseMio=resolveExprMioType(node->index_expr.base);
				if(!baseMio)return nullptr;
				if(baseMio->kind==MioTypeKind::POINTER&&baseMio->base_type)
					return mio_type_clone(baseMio->base_type);
				return nullptr;
			}
			case AstNodeKind::INT_LIT:	return mio_type_new(MioTypeKind::I32);
			case AstNodeKind::FLOAT_LIT:return mio_type_new(MioTypeKind::F64);
			case AstNodeKind::BOOL_LIT:	return mio_type_new(MioTypeKind::BOOL);
			case AstNodeKind::CHAR_LIT:	return mio_type_new(MioTypeKind::CHAR);
			case AstNodeKind::MEMBER_EXPR:{
				std::string structName;
				if(node->member.base->type&&!node->member.base->type->name.empty())
					structName=resolveStructName(node->member.base->type->name);
				else if(node->member.base->kind==AstNodeKind::IDENT_EXPR){
					if(node->member.base->ident.name=="this"&&!currentClassName.empty()){
						structName=currentClassName;
					}else{
						std::string vname=node->member.base->ident.name;
						if(!node->member.base->ident.namespace_name.empty())
							vname=node->member.base->ident.namespace_name+"::"+vname;
						auto it=locals.find(vname);
						if(it!=locals.end()){
							llvm::Type* at=it->second->getAllocatedType();
							if(at->isStructTy()){
								structName=std::string(at->getStructName());
							}else if(at->isPointerTy()){
								MioType* bm=resolveExprMioType(node->member.base);
								if(bm&&bm->kind==MioTypeKind::POINTER&&bm->base_type&&bm->base_type->kind==MioTypeKind::STRUCT)
									structName=bm->base_type->name;
							}
						}
					}
				}
				if(!structName.empty()&&structFieldIdx.count(structName)){
					auto fit=structFieldIdx.find(structName);
					auto fi=fit->second.find(node->member.member);
					if(fi!=fit->second.end()){
						auto ftit=structFieldTypes.find(structName);
						if(ftit!=structFieldTypes.end()){
							auto fti=ftit->second.find(node->member.member);
							if(fti!=ftit->second.end()){
								return mio_type_clone(fti->second);
							}
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
				auto it=locals.find(name);
				if(it!=locals.end()){
					llvm::Type* at=it->second->getAllocatedType();
					MioType* mt=llvmTypeToMioType(at);
					return mt;
				}
				auto git=globalVars.find(name);
				if(git!=globalVars.end()){
					llvm::Type* gt=git->second->getValueType();
					return llvmTypeToMioType(gt);
				}
				return nullptr;
			}
			default:
				return nullptr;
		}
	}
	std::vector<TemplateArg> tryDeduceTemplateArgs(const std::string& calleeName,AstNode* node){
		auto it=templateMap.find(calleeName);
		if(it==templateMap.end())return {};
		auto& typeParams=it->second.first;
		auto* templateDef=it->second.second;
		if(templateDef->kind!=AstNodeKind::FUNC_DEF){
			return {};
		}
		auto& params=templateDef->func_def.params;
		if(params.size()!=node->call.args.size()){
			return {};
		}
		size_t typeParamCount=0;
		for(auto& p:typeParams)if(p.is_type)typeParamCount++;
		std::vector<MioType*> deduced(typeParamCount,nullptr);
		for(size_t i=0;i<params.size();i++){
			MioType* paramType=params[i].type;
			if(!paramType)continue;
			MioType* argType=resolveExprMioType(node->call.args[i]);
			if(!argType)continue;
			deduceTemplateArg(paramType,argType,typeParams,deduced);
			if(argType!=node->call.args[i]->type)mio_type_free(argType);
		}
		for(size_t i=0;i<deduced.size();i++){
			if(!deduced[i])return {};
		}
		std::vector<TemplateArg> result;
		size_t dedIdx=0;
		for(auto& p:typeParams){
			if(p.is_type){
				result.push_back({true,deduced[dedIdx++],nullptr});
			}else{
				AstNode* defVal=p.default_val;
				if(!defVal)return {};
				result.push_back({false,nullptr,defVal});
			}
		}
		return result;
	}
	void deduceTemplateArg(MioType* paramType, MioType* argType, const std::vector<TemplateParam>& typeParams, std::vector<MioType*>& deduced){
		if(!paramType||!argType)return;
		if(paramType->kind==MioTypeKind::POINTER){
			if(argType->kind==MioTypeKind::POINTER){
				deduceTemplateArg(paramType->base_type, argType->base_type, typeParams, deduced);
			}
			return;
		}
		if(paramType->kind==MioTypeKind::ARRAY){
			deduceTemplateArg(paramType->base_type, argType, typeParams, deduced);
			return;
		}
		size_t dedIdx=0;
		for(size_t i=0;i<typeParams.size();i++){
			if(!typeParams[i].is_type)continue;
			if(paramType->name==typeParams[i].name){
				if(deduced[dedIdx]&&(deduced[dedIdx]->name!=argType->name||deduced[dedIdx]->kind!=argType->kind)){
					return;
				}
				if(deduced[dedIdx])mio_type_free(deduced[dedIdx]);
				deduced[dedIdx]=mio_type_clone(argType);
				return;
			}
			dedIdx++;
		}
	}
	AstNode* instantiateTemplate(AstNode* templateDef,std::unordered_map<std::string,MioType*>& typeSubst,std::unordered_map<std::string,int64_t>& valueSubst){
		if(templateDef->kind!=AstNodeKind::FUNC_DEF)return nullptr;
		auto* def=templateDef;
		MioType* retType=substituteType(def->func_def.return_type,typeSubst,valueSubst);
		std::string instName=def->func_def.name;
		for(auto& p:typeSubst){
			instName+="_"+mio_type_str(p.second);
		}
		auto* inst=ast_new_func_def(instName,retType,nullptr,def->func_def.is_static,def->line,def->col,def->filename);
		inst->func_def.is_extern=def->func_def.is_extern;
		inst->func_def.is_variadic=def->func_def.is_variadic;
		inst->func_def.is_virtual=def->func_def.is_virtual;
		inst->func_def.is_override=def->func_def.is_override;
		inst->func_def.is_pure_virtual=def->func_def.is_pure_virtual;
		inst->func_def.is_operator=def->func_def.is_operator;
		inst->func_def.op_name=def->func_def.op_name;
		inst->func_def.access=def->func_def.access;
		inst->func_def.class_name=def->func_def.class_name;
		for(auto& p:def->func_def.params){
			MioType* pt=substituteType(p.type,typeSubst,valueSubst);
			ast_func_add_param(inst,p.name,pt);
		}
		inst->func_def.body=cloneAst(def->func_def.body,typeSubst,valueSubst);
		return inst;
	}
	MioType* substituteType(MioType* mt,std::unordered_map<std::string,MioType*>& typeSubst,std::unordered_map<std::string,int64_t>& valueSubst){
		if(!mt)return nullptr;
		if(mt->kind==MioTypeKind::STRUCT&&!mt->name.empty()){
			auto it=typeSubst.find(mt->name);
			if(it!=typeSubst.end()){
				return mio_type_clone(it->second);
			}
		}
		if(mt->kind==MioTypeKind::POINTER){
			auto* result=mio_type_new(MioTypeKind::POINTER);
			result->base_type=substituteType(mt->base_type,typeSubst,valueSubst);
			return result;
		}
		if(mt->kind==MioTypeKind::ARRAY){
			auto* base=substituteType(mt->base_type,typeSubst,valueSubst);
			auto* result=new MioType(base,mt->array_size);
			return result;
		}
		return mio_type_clone(mt);
	}
	AstNode* cloneAst(AstNode* node,std::unordered_map<std::string,MioType*>& typeSubst,std::unordered_map<std::string,int64_t>& valueSubst){
		if(!node)return nullptr;
		switch(node->kind){
			case AstNodeKind::BLOCK:{
				auto* n=ast_new_block(node->line,node->col,node->filename);
				n->block.is_scope=node->block.is_scope;
				for(auto* s:node->block.stmts){
					auto* cs=cloneAst(s,typeSubst,valueSubst);
					if(cs)n->block.stmts.push_back(cs);
				}
				return n;
			}
			case AstNodeKind::RETURN_STMT:
				return ast_new_return(cloneAst(node->return_stmt.value,typeSubst,valueSubst),node->line,node->col,node->filename);
			case AstNodeKind::EXPR_STMT:
				return ast_new_expr_stmt(cloneAst(node->expr_stmt.expr,typeSubst,valueSubst),node->line,node->col,node->filename);
			case AstNodeKind::BINARY_EXPR:{
				auto* n=ast_new_binary(cloneAst(node->binary.left,typeSubst,valueSubst),node->binary.op,cloneAst(node->binary.right,typeSubst,valueSubst),node->line,node->col,node->filename);
				return n;
			}
			case AstNodeKind::CALL_EXPR:{
				auto* callee=cloneAst(node->call.callee,typeSubst,valueSubst);
				auto* n=ast_new_call(callee,node->line,node->col,node->filename);
				for(auto* a:node->call.args)n->call.args.push_back(cloneAst(a,typeSubst,valueSubst));
				for(auto& ta:node->call.template_args){
					if(ta.is_type)
						n->call.template_args.push_back({true,substituteType(ta.type_val,typeSubst,valueSubst),nullptr});
					else
						n->call.template_args.push_back({false,nullptr,cloneAst(ta.expr_val,typeSubst,valueSubst)});
				}
				return n;
			}
			case AstNodeKind::IDENT_EXPR:{
				auto it=valueSubst.find(node->ident.name);
				if(it!=valueSubst.end()){
					return ast_new_int_lit(it->second,node->line,node->col,node->filename);
				}
				auto* n=ast_new_ident(node->ident.name,node->line,node->col,node->filename);
				n->ident.namespace_name=node->ident.namespace_name;
				if(node->type){
					n->type=substituteType(node->type,typeSubst,valueSubst);
				}
				return n;
			}
			case AstNodeKind::INT_LIT:
				return ast_new_int_lit(node->int_lit.value,node->line,node->col,node->filename);
			case AstNodeKind::FLOAT_LIT:
				return ast_new_float_lit(node->float_lit.value,node->line,node->col,node->filename);
			case AstNodeKind::STRING_LIT:
				return ast_new_string_lit(node->string_lit.value,node->line,node->col,node->filename);
			case AstNodeKind::BOOL_LIT:
				return ast_new_bool_lit(node->bool_lit.value,node->line,node->col,node->filename);
			case AstNodeKind::CHAR_LIT:
				return ast_new_char_lit(node->char_lit.value,node->line,node->col,node->filename);
			case AstNodeKind::UNARY_EXPR:
				return ast_new_unary(node->unary.op,cloneAst(node->unary.operand,typeSubst,valueSubst),node->line,node->col,node->filename);
			case AstNodeKind::VAR_DECL:{
				MioType* vt=substituteType(node->var_decl.var_type,typeSubst,valueSubst);
				auto* n=ast_new_var_decl(node->var_decl.name,vt,cloneAst(node->var_decl.init,typeSubst,valueSubst),node->var_decl.is_static,node->line,node->col,node->filename);
				return n;
			}
			case AstNodeKind::CONST_DECL:{
				MioType* vt=substituteType(node->const_decl.var_type,typeSubst,valueSubst);
				auto* n=ast_new_const_decl(node->const_decl.name,vt,cloneAst(node->const_decl.init,typeSubst,valueSubst),node->const_decl.is_static,node->line,node->col,node->filename);
				return n;
			}
			case AstNodeKind::ASSIGN_EXPR:
				return ast_new_assign(cloneAst(node->assign.left,typeSubst,valueSubst),node->assign.op,cloneAst(node->assign.right,typeSubst,valueSubst),node->line,node->col,node->filename);
			case AstNodeKind::SIZEOF_EXPR:{
				MioType* tt=substituteType(node->sizeof_expr.target_type,typeSubst,valueSubst);
				return ast_new_sizeof_expr(tt,node->line,node->col,node->filename);
			}
			case AstNodeKind::IF_STMT:{
				auto* n=ast_new_if(cloneAst(node->if_stmt.cond,typeSubst,valueSubst),cloneAst(node->if_stmt.then_body,typeSubst,valueSubst),cloneAst(node->if_stmt.else_body,typeSubst,valueSubst),node->line,node->col,node->filename);
				for(auto* elif:node->if_stmt.elif_list)
					n->if_stmt.elif_list.push_back(cloneAst(elif,typeSubst,valueSubst));
				return n;
			}
			case AstNodeKind::WHILE_STMT:
				return ast_new_while(cloneAst(node->while_stmt.cond,typeSubst,valueSubst),cloneAst(node->while_stmt.body,typeSubst,valueSubst),node->line,node->col,node->filename);
			case AstNodeKind::FOR_STMT:
				return ast_new_for(cloneAst(node->for_stmt.init,typeSubst,valueSubst),cloneAst(node->for_stmt.cond,typeSubst,valueSubst),cloneAst(node->for_stmt.update,typeSubst,valueSubst),cloneAst(node->for_stmt.body,typeSubst,valueSubst),node->line,node->col,node->filename);
			case AstNodeKind::MEMBER_EXPR:
				return ast_new_member(cloneAst(node->member.base,typeSubst,valueSubst),node->member.member,node->member.arrow,node->line,node->col,node->filename);
			case AstNodeKind::INDEX_EXPR:
				return ast_new_index(cloneAst(node->index_expr.base,typeSubst,valueSubst),cloneAst(node->index_expr.index,typeSubst,valueSubst),node->line,node->col,node->filename);
			case AstNodeKind::CAST_EXPR:{
				MioType* tt=substituteType(node->cast_expr.target_type,typeSubst,valueSubst);
				return ast_new_cast(tt,cloneAst(node->cast_expr.expr,typeSubst,valueSubst),node->line,node->col,node->filename);
			}
			case AstNodeKind::BREAK_STMT:
				return ast_new_break(node->line,node->col,node->filename);
			case AstNodeKind::CONTINUE_STMT:
				return ast_new_continue(node->line,node->col,node->filename);
			case AstNodeKind::GOTO_STMT:
				return ast_new_goto(node->goto_stmt.label,node->line,node->col,node->filename);
			case AstNodeKind::LABEL_STMT:
				return ast_new_label(node->label_stmt.label,node->line,node->col,node->filename);
			case AstNodeKind::ARRAY_LIT:{
				auto* n=ast_new_array_lit(node->line,node->col,node->filename);
				for(auto* e:node->array_lit.elements)
					n->array_lit.elements.push_back(cloneAst(e,typeSubst,valueSubst));
				return n;
			}
			default:
				error(node->line,node->col,"unsupported node in template instantiation");
				return nullptr;
		}
	}
	void genGlobalVar(AstNode* decl){
		if(!decl)return;
		bool isConst=(decl->kind==AstNodeKind::CONST_DECL);
		std::string name=isConst?decl->const_decl.name:decl->var_decl.name;
		std::string mangled=mangleName(name);
		if(!currentNamespace.empty())
			namespaceMembers[name]=mangled;
		if(globalVars.count(mangled)){
			error(decl->line,decl->col,"redefinition of variable '"+mangled+"'");
			return;
		}
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
		auto* gv=new llvm::GlobalVariable(*mod,ty,isConst,llvm::GlobalValue::InternalLinkage,init,mangled);
		globalVars[mangled]=gv;
	}
	void genFuncDef(AstNode* def){
		if(!def){
			error("null function definition");
			return;
		}
		std::string dbgName=def->func_def.name;
		auto* savedFn=curFn;
		auto* savedBB=curBB;
		auto* savedThisAlloca=thisAlloca;
		std::string savedClassName=currentClassName;
		std::vector<std::pair<std::string,llvm::AllocaInst*>> savedCleanupStack=cleanupStack;
		auto savedLocals=locals;
		auto savedLocalMioTypes=localMioTypes;
		llvm::IRBuilderBase::InsertPoint savedIP;
		bool hadInsertPoint=(b.GetInsertBlock()!=nullptr);
		if(hadInsertPoint){
			savedIP=b.saveIP();
		}
		
		std::string name=def->func_def.name;
		bool isMethod=!def->func_def.class_name.empty();
		std::string shortClassName=def->func_def.class_name;
		{
			auto pos=shortClassName.rfind("::");
			if(pos!=std::string::npos)shortClassName=shortClassName.substr(pos+2);
		}
		bool isCtor=isMethod&&def->func_def.name==shortClassName;
		bool isDtor=isMethod&&!name.empty()&&name[0]=='~';
		llvm::Type* retTy=nullptr;
		if(isCtor){
			retTy=llvm::Type::getVoidTy(ctx);
		}else{
			retTy=convertType(def->func_def.return_type);
		}
		std::vector<llvm::Type*> paramTys;
	if(isMethod){
		auto stit=structTypes.find(def->func_def.class_name);
		if(stit!=structTypes.end()){
			paramTys.push_back(stit->second->getPointerTo());
		}else{
			paramTys.push_back(llvm::PointerType::get(ctx,0));
		}
	}
		for(auto& p:def->func_def.params)paramTys.push_back(convertType(p.type));
		auto* ft=llvm::FunctionType::get(retTy,paramTys,def->func_def.is_variadic);
		std::string mangledName=name;
		if(isMethod&&!def->func_def.is_static){
			std::string fullClassName=def->func_def.class_name;
			if(fullClassName.find("::")==std::string::npos&&!currentNamespace.empty()){
				fullClassName=currentNamespace+"::"+fullClassName;
			}
			mangledName=fullClassName+"::"+name;
			if((isCtor||def->func_def.is_operator)&&def->func_def.params.size()>0){
				mangledName+="_";
				for(size_t i=0;i<def->func_def.params.size();i++){
					if(i>0)mangledName+="_";
					mangledName+=mio_type_str(def->func_def.params[i].type);
				}
			}
		}
		if(!isMethod&&!currentNamespace.empty()){
			mangledName=currentNamespace+"::"+name;
			namespaceMembers[name]=mangledName;
		}
		
		if(funcDecls.count(mangledName)&&!def->func_def.is_extern&&!def->func_def.is_pure_virtual){
			error(def->line,def->col,"redefinition of function '"+mangledName+"'");
			return;
		}
		
		auto* fn=llvm::Function::Create(ft,llvm::Function::ExternalLinkage,0,mangledName,mod.get());
		funcDecls[mangledName]=fn;
		funcDefMap[mangledName]=def;
		if(name!=mangledName&&!isCtor&&!def->func_def.is_operator){
			if(funcDecls.count(name)&&funcDecls[name]!=fn){
				error(def->line,def->col,"redefinition of function '"+name+"'");
				return;
			}
			funcDecls[name]=fn;
		}
		if(def->func_def.is_extern)return;
		if(def->func_def.is_pure_virtual)return;
		size_t idx=0;
		if(isMethod){
			fn->getArg(0)->setName("this");
			idx=1;
		}
		for(auto& arg:fn->args()){
			if(&arg==fn->getArg(0)){
				continue;
			}
			size_t paramIdx=idx-(isMethod?1:0);
			if(paramIdx<def->func_def.params.size())
				arg.setName(def->func_def.params[paramIdx].name);
			idx++;
		}
		curFn=fn;
		auto* entry=llvm::BasicBlock::Create(ctx,"entry",fn);
		curBB=entry;
		b.SetInsertPoint(entry);
		locals.clear();
		localMioTypes.clear();
		thisAlloca=nullptr;
		currentClassName.clear();
		if(isMethod){
			currentClassName=def->func_def.class_name;
			if(currentClassName.find("::")==std::string::npos&&!currentNamespace.empty()){
				currentClassName=currentNamespace+"::"+currentClassName;
			}
			auto* thisVal=fn->getArg(0);
			auto* alloca=createEntryAlloca(fn,"this",thisVal->getType());
			b.CreateStore(thisVal,alloca);
			thisAlloca=alloca;
			locals["this"]=alloca;
			if(!currentClassName.empty()){
				MioType* structType=mio_type_new_named(MioTypeKind::STRUCT,currentClassName);
				localMioTypes["this"]=mio_type_new_pointer(structType);
			}
		}
		for(size_t i=0;i<def->func_def.params.size();i++){
			auto& arg=*(fn->arg_begin()+(isMethod?1:0)+i);
			auto* alloca=createEntryAlloca(fn,def->func_def.params[i].name,arg.getType());
			b.CreateStore(&arg,alloca);
			locals[def->func_def.params[i].name]=alloca;
			localMioTypes[def->func_def.params[i].name]=def->func_def.params[i].type;
		}
		if(isCtor&&isMethod){
			genConstructorInit(def,fn->getArg(0));
		}
		if(def->func_def.body){
		genBlock(def->func_def.body);
		if(curBB&&!curBB->getTerminator()){
				genCleanupAll();
				if(isDtor){
					auto bit=classBaseMap.find(def->func_def.class_name);
					if(bit!=classBaseMap.end()){
						auto bdit=classDestructorMap.find(bit->second);
						if(bdit!=classDestructorMap.end()){
							llvm::Function* baseDtor=mod->getFunction(bdit->second);
							if(baseDtor&&!baseDtor->arg_empty()){
								auto* thisPtr=fn->getArg(0);
								auto* ptr=b.CreateBitCast(thisPtr,baseDtor->getArg(0)->getType());
								b.CreateCall(baseDtor,{ptr});
							}
						}
					}
				}
				if(retTy->isVoidTy())
					b.CreateRetVoid();
				else
					b.CreateRet(llvm::Constant::getNullValue(retTy));
			}
		}else{
			genCleanupAll();
			if(isDtor){
				auto bit=classBaseMap.find(def->func_def.class_name);
				if(bit!=classBaseMap.end()){
					auto bdit=classDestructorMap.find(bit->second);
					if(bdit!=classDestructorMap.end()){
						llvm::Function* baseDtor=mod->getFunction(bdit->second);
						if(baseDtor&&!baseDtor->arg_empty()){
							auto* thisPtr=fn->getArg(0);
							auto* ptr=b.CreateBitCast(thisPtr,baseDtor->getArg(0)->getType());
							b.CreateCall(baseDtor,{ptr});
						}
					}
				}
			}
			if(retTy->isVoidTy())
				b.CreateRetVoid();
			else
				b.CreateRet(llvm::Constant::getNullValue(retTy));
		}
		curFn=savedFn;
		curBB=savedBB;
		thisAlloca=savedThisAlloca;
		currentClassName=savedClassName;
		cleanupStack=savedCleanupStack;
		locals=savedLocals;
		localMioTypes=savedLocalMioTypes;
		if(hadInsertPoint){
			b.restoreIP(savedIP);
		}
	}
	void genStructDef(AstNode* def){
		std::string name=def->struct_def.name;
		std::string mangled=mangleName(name);
		if(!currentNamespace.empty())
			namespaceMembers[name]=mangled;
		if(structTypes.count(mangled)){
			error(def->line,def->col,"redefinition of struct '"+mangled+"'");
			return;
		}
		std::vector<llvm::Type*> fieldTys;
		for(auto& f:def->struct_def.fields)fieldTys.push_back(convertType(f.type));
		auto* st=llvm::StructType::create(ctx,fieldTys,mangled);
		structTypes[mangled]=st;
		for(unsigned i=0;i<def->struct_def.fields.size();i++){
			structFieldIdx[mangled][def->struct_def.fields[i].name]=i;
			structFieldTypes[mangled][def->struct_def.fields[i].name]=mio_type_clone(def->struct_def.fields[i].type);
		}
		for(auto* m:def->struct_def.methods){
			if(m->kind==AstNodeKind::FUNC_DEF){
				m->func_def.struct_name=mangled;
				genFuncDef(m);
			}
		}
	}
	void genEnumDef(AstNode* def){
		std::string name=def->enum_def.name;
		std::string mangled=mangleName(name);
		if(!currentNamespace.empty())
			namespaceMembers[name]=mangled;
		if(enumNames.count(mangled)){
			error(def->line,def->col,"redefinition of enum '"+mangled+"'");
			return;
		}
		enumNames.insert(mangled);
		for(auto& v:def->enum_def.variants){
			enumVariantMap[v.name]=mangled;
		}
	}
	std::string resolveStructName(const std::string& name){
		if(!currentNamespace.empty()){
			std::string nsFullName=currentNamespace+"::"+name;
			if(classTemplateMap.count(nsFullName)||structTypes.count(nsFullName))
				return nsFullName;
		}
		for(auto& impNs:importedNamespaces){
			std::string fullName=impNs+"::"+name;
			if(classTemplateMap.count(fullName)||structTypes.count(fullName))
				return fullName;
		}
		return name;
	}
	void instantiateClassTemplate(const std::string& name,std::vector<MioType*>& typeArgs,const std::string& instName){
		std::string fullName=resolveStructName(name);
		auto it=classTemplateMap.find(fullName);
		if(it==classTemplateMap.end())return;
		if(classTemplateInstances.count(instName))return;
		classTemplateInstances.insert(instName);
		auto& typeParams=it->second.first;
		auto* classDef=it->second.second;
		if(typeArgs.size()!=typeParams.size()){
			error(classDef->line,classDef->col,"class template argument count mismatch");
			return;
		}
		std::unordered_map<std::string,MioType*> typeSubst;
		for(size_t i=0;i<typeParams.size();i++)
			typeSubst[typeParams[i].name]=mio_type_clone(typeArgs[i]);
		AstNode* instClass=instantiateClassTemplateAst(classDef,typeSubst,instName);
		if(!instClass)return;
		std::string savedNs=currentNamespace;
		size_t pos=instName.rfind("::");
		if(pos!=std::string::npos){
			currentNamespace=instName.substr(0,pos);
			instClass->class_def.name=instName.substr(pos+2);
		}else{
			currentNamespace="";
			instClass->class_def.name=instName;
		}
		genClassDef(instClass);
		currentNamespace=savedNs;
	}
	AstNode* instantiateClassTemplateAst(AstNode* classDef,std::unordered_map<std::string,MioType*>& typeSubst,const std::string& instName){
		std::unordered_map<std::string,int64_t> emptyVS;
		auto* c=ast_new_class_def(instName,classDef->class_def.base_name,classDef->class_def.base_access,classDef->line,classDef->col,classDef->filename);
		for(auto& f:classDef->class_def.fields){
			MioType* ft=substituteType(f.type,typeSubst,emptyVS);
			AstNode* init=f.init?cloneAst(f.init,typeSubst,emptyVS):nullptr;
			ast_class_add_field(c,f.name,ft,init,f.access);
		}
		for(auto* m:classDef->class_def.methods){
			AstNode* instM=instantiateTemplate(m,typeSubst,emptyVS);
			if(instM){
				instM->func_def.class_name=instName;
				ast_class_add_method(c,instM);
			}
		}
		for(auto* ctor:classDef->class_def.constructors){
			AstNode* instCtor=instantiateTemplate(ctor,typeSubst,emptyVS);
			if(instCtor){
				instCtor->func_def.class_name=instName;
				ast_class_add_method(c,instCtor);
			}
		}
		if(classDef->class_def.destructor){
			AstNode* instDtor=instantiateTemplate(classDef->class_def.destructor,typeSubst,emptyVS);
			if(instDtor){
				instDtor->func_def.class_name=instName;
				c->class_def.destructor=instDtor;
			}
		}
		return c;
	}
	void genUnionDef(AstNode* def){
		std::string name=def->union_def.name;
		std::string mangled=mangleName(name);
		if(!currentNamespace.empty())
			namespaceMembers[name]=mangled;
		if(structTypes.count(mangled)){
			error(def->line,def->col,"redefinition of union '"+mangled+"'");
			return;
		}
		llvm::Type* maxTy=llvm::Type::getInt8Ty(ctx);
		size_t maxSize=0;
		for(auto& f:def->union_def.fields){
			llvm::Type* t=convertType(f.type);
			size_t sz=mod->getDataLayout().getTypeAllocSize(t);
			if(sz>maxSize){maxSize=sz;maxTy=t;}
		}
		auto* st=llvm::StructType::create(ctx,{maxTy,llvm::ArrayType::get(llvm::Type::getInt8Ty(ctx),(unsigned)maxSize)},mangled);
		structTypes[mangled]=st;
	}
	void genClassDef(AstNode* def){
		std::string name=def->class_def.name;
		std::string mangled=mangleName(name);
		
		if(!currentNamespace.empty())
			namespaceMembers[name]=mangled;
		if(structTypes.count(mangled)){
			error(def->line,def->col,"redefinition of class '"+mangled+"'");
			return;
		}
		std::string base_mangled=def->class_def.base_name;
		if(!def->class_def.base_name.empty()&&!currentNamespace.empty()){
			if(structTypes.count(def->class_def.base_name))
				base_mangled=def->class_def.base_name;
			else
				base_mangled=currentNamespace+"::"+def->class_def.base_name;
		}
		if(!def->class_def.base_name.empty()){
			classBaseMap[mangled]=base_mangled;
			classBaseAccessMap[mangled]=def->class_def.base_access.empty()?"private":def->class_def.base_access;
		}
				classVTableOrder[mangled].clear();
		if(!def->class_def.base_name.empty()){
			auto it=classVTableOrder.find(base_mangled);
			if(it!=classVTableOrder.end()){
				classVTableOrder[mangled]=it->second;
			}
		}
		for(auto* m:def->class_def.methods){
			if(m->kind==AstNodeKind::FUNC_DEF){
				if(m->func_def.is_virtual||m->func_def.is_override||m->func_def.is_pure_virtual){
					bool found=false;
					for(auto& vn:classVTableOrder[mangled]){
						if(vn==m->func_def.name){found=true;break;}
					}
					if(!found)classVTableOrder[mangled].push_back(m->func_def.name);
				}
			}
		}
		if(!def->class_def.base_name.empty()){
			auto it=classVTableOrder.find(base_mangled);
			if(it!=classVTableOrder.end()){
				for(auto& vn:it->second){
					bool overridden=false;
					for(auto& mn:classVTableOrder[mangled]){
						if(mn==vn){overridden=true;break;}
					}
					if(!overridden){
						error("class '"+mangled+"' does not override pure virtual method '"+vn+"' from base class '"+base_mangled+"'");
					}
				}
			}
		}
		std::vector<llvm::Type*> fieldTys;
		bool hasVTable=!classVTableOrder[mangled].empty();
		bool baseHasVTable=false;
		unsigned fieldOffset=0;
		if(hasVTable){
			fieldTys.push_back(llvm::PointerType::get(ctx,0));
			fieldOffset=1;
		}
		unsigned baseFieldCount=0;
		if(!def->class_def.base_name.empty()){
			auto bit=structTypes.find(base_mangled);
			if(bit!=structTypes.end()){
				auto* baseST=bit->second;
				baseHasVTable=!classVTableOrder[base_mangled].empty();
				unsigned startIdx=baseHasVTable?1:0;
				for(unsigned i=startIdx;i<baseST->getNumElements();i++){
					fieldTys.push_back(baseST->getElementType(i));
					baseFieldCount++;
				}
			}
		}
		for(auto& f:def->class_def.fields)fieldTys.push_back(convertType(f.type));
		auto* st=llvm::StructType::create(ctx,fieldTys,mangled);
		structTypes[mangled]=st;
		unsigned fidx=fieldOffset+baseFieldCount;
		for(unsigned i=0;i<def->class_def.fields.size();i++){
			structFieldIdx[mangled][def->class_def.fields[i].name]=fidx;
			structFieldTypes[mangled][def->class_def.fields[i].name]=mio_type_clone(def->class_def.fields[i].type);
			fidx++;
		}
		if(!def->class_def.base_name.empty()){
			auto it=structFieldIdx.find(base_mangled);
			if(it!=structFieldIdx.end()){
				unsigned baseVOff=baseHasVTable?1:0;
				unsigned derivedVOff=hasVTable?1:0;
				for(auto& [fname,idx]:it->second){
					structFieldIdx[mangled][fname]=idx-baseVOff+derivedVOff;
				}
			}
		}
		for(auto* m:def->class_def.methods){
			if(m->kind==AstNodeKind::FUNC_DEF){
				m->func_def.class_name=mangled;
				genFuncDef(m);
			}
		}
		if(hasVTable) genVTable(mangled);
		if(!def->class_def.constructors.empty()){
			for(auto* ctor:def->class_def.constructors){
				ctor->func_def.class_name=mangled;
				genFuncDef(ctor);
			}
		}
		if(def->class_def.destructor){
			auto* dtor=def->class_def.destructor;
			dtor->func_def.class_name=mangled;
			genFuncDef(dtor);
			classDestructorMap[mangled]=mangled+"::~"+name;
		}
	}
	void genVTable(const std::string& className){
		auto& vorder=classVTableOrder[className];
		if(vorder.empty())return;
		std::vector<llvm::Type*> vtableFieldTys;
		std::vector<llvm::Constant*> vtableEntries;
		std::vector<llvm::FunctionType*> vtableFuncTypes;
		for(auto& vname:vorder){
			std::string mangledName=className+"::"+vname;
			llvm::Function* fn=mod->getFunction(mangledName);
			if(!fn){
				fn=mod->getFunction(vname);
			}
			if(!fn){
				error("virtual method '"+className+"::"+vname+"' not found for vtable");
				vtableFieldTys.push_back(llvm::PointerType::get(ctx,0));
				vtableEntries.push_back(llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx,0)));
				vtableFuncTypes.push_back(nullptr);
				continue;
			}
			vtableFieldTys.push_back(llvm::PointerType::get(ctx,0));
			vtableEntries.push_back(fn);
			vtableFuncTypes.push_back(fn->getFunctionType());
			classVTable[className].push_back({vname,fn});
		}
		auto* vtableTy=llvm::StructType::create(ctx,vtableFieldTys,className+"_vtable");
		classVTableTypes[className]=vtableTy;
		classVTableFuncTypes[className]=vtableFuncTypes;
		auto* vtableConst=llvm::ConstantStruct::get(vtableTy,vtableEntries);
		auto* gv=new llvm::GlobalVariable(*mod,vtableTy,true,llvm::GlobalValue::InternalLinkage,vtableConst,className+"_vtable_inst");
		classVTableGlobals[className]=gv;
	}
	void genConstructorInit(AstNode* def,llvm::Value* thisPtr){
		std::string className=def->func_def.class_name;
		auto* st=structTypes[className];
		if(!st){
			error("struct type '"+className+"' not found for constructor initialization");
			return;
		}
		if(thisPtr->getType()!=st->getPointerTo()){
			thisPtr=b.CreateBitCast(thisPtr,st->getPointerTo());
		}
		auto it=classBaseMap.find(className);
		if(it!=classBaseMap.end()&&!it->second.empty()){
			std::string baseCtorName=it->second+"::"+it->second;
			llvm::Function* baseCtor=mod->getFunction(baseCtorName);
			if(baseCtor){
				std::vector<llvm::Value*> baseCtorArgs;
				baseCtorArgs.push_back(thisPtr);
				for(size_t i=1;i<curFn->arg_size();i++){
					baseCtorArgs.push_back(curFn->getArg(i));
				}
				b.CreateCall(baseCtor,baseCtorArgs);
			}
		}
		if(classVTableGlobals.count(className)){
			auto* vtableGV=classVTableGlobals[className];
			auto* st=structTypes[className];
			if(!st){
				error("struct type '"+className+"' not found for vtable initialization");
				return;
			}
			auto* vtablePtr=b.CreateGEP(st,thisPtr,{
				llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0),
				llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),0)
			});
			b.CreateStore(vtableGV,vtablePtr);
		}
		for(auto& init:def->func_def.init_list){
			llvm::Value* val=genExpr(init.expr);
			if(!val){
				error("failed to generate initializer for field '"+init.name+"'");
				return;
			}
			auto fit=structFieldIdx.find(className);
			if(fit!=structFieldIdx.end()){
				auto fi=fit->second.find(init.name);
				if(fi!=fit->second.end()){
					auto sit=structTypes.find(className);
					if(sit!=structTypes.end()){
						auto* st=sit->second;
						unsigned idx=fi->second;
						llvm::Value* gep=b.CreateGEP(st,thisPtr,{
							llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0),
							llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),idx)
						});
						llvm::Type* fieldTy=st->getElementType(idx);
						if(val->getType()!=fieldTy)val=genCastValue(val,fieldTy);
						b.CreateStore(val,gep);
					}
				}
			}
		}
	}
	void genBlock(AstNode* block){
		size_t savedSize=cleanupStack.size();
		for(auto* stmt:block->block.stmts)genStmt(stmt);
		if(curBB&&!curBB->getTerminator()&&block->block.is_scope){
			genScopeExit(savedSize);
		}
	}
	llvm::Function* findDestructor(const std::string& typeName){
		auto dit=classDestructorMap.find(typeName);
		if(dit!=classDestructorMap.end())
			return mod->getFunction(dit->second);
		for(auto& impNs:importedNamespaces){
			std::string fullName=impNs+"::"+typeName;
			auto dit2=classDestructorMap.find(fullName);
			if(dit2!=classDestructorMap.end())
				return mod->getFunction(dit2->second);
		}
		return nullptr;
	}
	void genCleanupAll(){
		for(int i=(int)cleanupStack.size()-1;i>=0;i--){
			auto& entry=cleanupStack[i];
			llvm::Function* dtor=findDestructor(entry.first);
			if(!dtor||dtor->arg_empty())continue;
			auto* ptr=b.CreateBitCast(entry.second,dtor->getArg(0)->getType());
			b.CreateCall(dtor,{ptr});
		}
	}
	void genScopeExit(size_t savedSize){
		for(int i=(int)cleanupStack.size()-1;i>=(int)savedSize;i--){
			auto& entry=cleanupStack[i];
			llvm::Function* dtor=findDestructor(entry.first);
			if(!dtor||dtor->arg_empty())continue;
			auto* ptr=b.CreateBitCast(entry.second,dtor->getArg(0)->getType());
			b.CreateCall(dtor,{ptr});
		}
		cleanupStack.resize(savedSize);
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
				if(!v)error(stmt->line,stmt->col,"failed to generate expression statement");
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
		localMioTypes[name]=mt;
		if(initExpr){
			llvm::Value* val=genExpr(initExpr);
			if(!val){
				error(decl->line,decl->col,"failed to generate initializer for '"+name+"'");
				return;
			}
			if(val->getType()!=ty)val=genCastValue(val,ty);
			b.CreateStore(val,alloca);
		}
		if(mt&&mt->kind==MioTypeKind::STRUCT){
			auto dit=classDestructorMap.find(mt->name);
			if(dit==classDestructorMap.end()){
				for(auto& impNs:importedNamespaces){
					std::string fullName=impNs+"::"+mt->name;
					dit=classDestructorMap.find(fullName);
					if(dit!=classDestructorMap.end())break;
				}
			}
			if(dit!=classDestructorMap.end()){
				cleanupStack.push_back({mt->name,alloca});
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
		genCleanupAll();
		if(stmt->return_stmt.value){
			llvm::Value* val=genExpr(stmt->return_stmt.value);
			llvm::Type* retTy=curFn->getReturnType();
			if(val->getType()!=retTy){
				if(val->getType()->isPointerTy()&&retTy->isStructTy()){
					val=b.CreateLoad(retTy,val);
				}else{
					val=genCastValue(val,retTy);
				}
			}
			b.CreateRet(val);
		}else{
			b.CreateRetVoid();
		}
	}
	void genExprStmt(AstNode* stmt){
		llvm::Value* v=genExpr(stmt->expr_stmt.expr);
		if(!v&&stmt->expr_stmt.expr->kind!=AstNodeKind::CALL_EXPR){
			error(stmt->line,stmt->col,"failed to generate expression");
		}
	}
	void genAssign(AstNode* expr){
		llvm::Value* rhs=genExpr(expr->assign.right);
		if(!rhs){
			error(expr->line,expr->col,"failed to generate right-hand side of assignment");
			return;
		}
		AstNode* lhs=expr->assign.left;
		llvm::Value* ptr=genLValue(lhs);
		if(!ptr){
			error(expr->line,expr->col,"failed to generate left-hand side of assignment");
			return;
		}
		llvm::Type* valTy=ptr->getType();
		if(valTy->isPointerTy()){
			llvm::Type* elemTy=resolveExprType(lhs);
			if(rhs->getType()!=elemTy)rhs=genCastValue(rhs,elemTy);
		}
		b.CreateStore(rhs,ptr);
	}
	llvm::Value* genLValue(AstNode* node){
		if(!node)return nullptr;
		switch(node->kind){
			case AstNodeKind::IDENT_EXPR:{
				std::string name=node->ident.name;
				if(node->ident.namespace_name=="::"){
					auto git=globalVars.find(name);
					if(git!=globalVars.end())return git->second;
					error(node->line,node->col,"undefined global variable '"+name+"'");
					return nullptr;
				}
				if(!node->ident.namespace_name.empty())
					name=node->ident.namespace_name+"::"+name;
				auto it=locals.find(name);
				if(it!=locals.end())return it->second;
				auto git=globalVars.find(name);
				if(git!=globalVars.end())return git->second;
				for(auto& impNs:importedNamespaces){
					std::string fullName=impNs+"::"+name;
					auto git2=globalVars.find(fullName);
					if(git2!=globalVars.end())return git2->second;
				}
				return nullptr;
			}
			case AstNodeKind::INDEX_EXPR:{
				
				MioType* baseMio=resolveExprMioType(node->index_expr.base);
				std::string structName;
				if(baseMio){
					if(baseMio->kind==MioTypeKind::STRUCT&&!baseMio->name.empty())
						structName=resolveStructName(baseMio->name);
					else if(baseMio->kind==MioTypeKind::POINTER&&baseMio->base_type&&baseMio->base_type->kind==MioTypeKind::STRUCT)
						structName=resolveStructName(baseMio->base_type->name);
				}
				
				if(!structName.empty()){
					MioType* rmt=resolveExprMioType(node->index_expr.index);
					std::string opMethod=findOperatorMethod(structName,TOK_LBRACKET,rmt);
					if(opMethod.empty()){
						error(node->line,node->col,"struct '"+structName+"' does not support operator[]");
						return nullptr;
					}
					llvm::Function* callee=funcDecls[opMethod];
					if(!callee){
						error(node->line,node->col,"operator[] for struct '"+structName+"' not found");
						return nullptr;
					}
					std::vector<llvm::Value*> args;
					llvm::Value* thisPtr=genLValue(node->index_expr.base);
					if(!thisPtr)thisPtr=genExpr(node->index_expr.base);
					if(thisPtr){
						auto* calleeThisTy=callee->getFunctionType()->getParamType(0);
						if(thisPtr->getType()!=calleeThisTy){
							if(thisPtr->getType()->isPointerTy()&&calleeThisTy->isPointerTy()){
								thisPtr=b.CreateBitCast(thisPtr,calleeThisTy);
							}else if(thisPtr->getType()->isPointerTy()&&calleeThisTy->isStructTy()){
								thisPtr=b.CreateLoad(calleeThisTy,thisPtr);
							}else if(auto* ai=llvm::dyn_cast<llvm::AllocaInst>(thisPtr)){
								thisPtr=b.CreateLoad(ai->getAllocatedType(),thisPtr);
							}
						}
					}
					args.push_back(thisPtr);
					llvm::Value* idx=genExpr(node->index_expr.index);
					if(idx)args.push_back(idx);
					return b.CreateCall(callee,args);
				}
				
				
				if(!baseMio){
					error(node->line,node->col,"cannot use operator[] on expression with unknown type");
					return nullptr;
				}
				
				if(baseMio->kind!=MioTypeKind::POINTER){
					error(node->line,node->col,"operator[] requires a pointer or struct with operator[], but got '"+mio_type_str(baseMio)+"'");
					return nullptr;
				}
				
				
				llvm::Value* base=genExpr(node->index_expr.base);
				if(!base)return nullptr;
				llvm::Value* idx=genExpr(node->index_expr.index);
				if(!idx)return nullptr;
				llvm::Type* elemTy=resolveExprType(node);
				if(!idx->getType()->isIntegerTy(64))
					idx=b.CreateSExt(idx,llvm::Type::getInt64Ty(ctx));
				return b.CreateGEP(elemTy,base,idx);
			}
			case AstNodeKind::MEMBER_EXPR:{
				llvm::Value* base=genLValue(node->member.base);
				if(!base)base=genExpr(node->member.base);
				if(!base)return nullptr;
				std::string structName;
				if(node->member.base->type&&!node->member.base->type->name.empty())
					structName=resolveStructName(node->member.base->type->name);
				else if(node->member.base->kind==AstNodeKind::IDENT_EXPR){
					if(node->member.base->ident.name=="this"&&!currentClassName.empty()){
						structName=currentClassName;
					}else{
						auto it=locals.find(node->member.base->ident.name);
						if(it!=locals.end()){
							llvm::Type* at=it->second->getAllocatedType();
							if(at->isStructTy())structName=std::string(at->getStructName());
							else if(at->isPointerTy()){
								if(node->member.base->type&&node->member.base->type->base_type&&node->member.base->type->base_type->kind==MioTypeKind::STRUCT)
									structName=resolveStructName(node->member.base->type->base_type->name);
							}
						}
					}
				}
				if(structName.empty()||!structFieldIdx.count(structName))return nullptr;
				auto mit=structFieldIdx[structName].find(node->member.member);
				if(mit==structFieldIdx[structName].end()){
					error(node->line,node->col,"field '"+node->member.member+"' not found in struct '"+structName+"'");
					return nullptr;
				}
				unsigned idx=mit->second;
				auto sit=structTypes.find(structName);
				llvm::StructType* st=sit!=structTypes.end()?sit->second:nullptr;
				if(!st)return nullptr;
				llvm::Value* ptr=base;
				if(auto* ai=llvm::dyn_cast<llvm::AllocaInst>(ptr)){
					if(ai->getAllocatedType()->isPointerTy())
						ptr=b.CreateLoad(ai->getAllocatedType(),ptr);
					else if(ai->getAllocatedType()->isStructTy()){
						ptr=ai;
					}else{
						error(node,"cannot access member '"+node->member.member+"' on non-struct type");
						return nullptr;
					}
				}
				if(!ptr->getType()->isPointerTy()){
					if(ptr->getType()->isStructTy()){
						auto* tmpAlloca=createEntryAlloca(curFn,"tmpstruct",st);
						b.CreateStore(ptr,tmpAlloca);
						ptr=tmpAlloca;
					}else{
						error(node,"cannot access member '"+node->member.member+"' on value");
						return nullptr;
					}
				}
				ptr=b.CreateBitCast(ptr,st->getPointerTo());
				llvm::Value* idx0=llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0);
				llvm::Value* idx1=llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),idx);
				return b.CreateGEP(st,ptr,{idx0,idx1});
			}
			case AstNodeKind::UNARY_EXPR:
				if(node->unary.op==TOK_STAR){
					llvm::Value* op=genExpr(node->unary.operand);
					if(op&&op->getType()->isPointerTy())return op;
				}
				return nullptr;
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
			case AstNodeKind::MEMBER_EXPR:{
				llvm::Value* ptr=genMemberExpr(node);
				if(!ptr)return nullptr;
				if(ptr->getType()->isPointerTy())
					return b.CreateLoad(resolveExprType(node),ptr);
				return ptr;
			}
			case AstNodeKind::ARRAY_LIT:
				return genArrayLit(node);
			case AstNodeKind::CAST_EXPR:
				return genCastExpr(node);
			case AstNodeKind::ASSIGN_EXPR:
				return genAssignExpr(node);
			case AstNodeKind::SIZEOF_EXPR:{
				llvm::Type* ty=convertType(node->sizeof_expr.target_type);
				uint64_t sz=mod->getDataLayout().getTypeAllocSize(ty);
				return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),sz);
			}
			default:
				error(node->line,node->col,"unsupported expression kind");
				return nullptr;
		}
	}
	llvm::Value* genStringRef(llvm::Constant* gv){
		return b.CreateGEP(llvm::Type::getInt8Ty(ctx),gv,{llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0),llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0)});
	}
	llvm::Value* genIdentExpr(AstNode* node){
		if(!node)return nullptr;
		std::string name=node->ident.name;
		std::string ns=node->ident.namespace_name;
		
		
		if(ns=="::"){
			auto git=globalVars.find(name);
			if(git!=globalVars.end()){
				llvm::Type* ty=git->second->getValueType();
				return b.CreateLoad(ty,git->second,name);
			}
			error(node->line,node->col,"undefined global variable '"+name+"'");
			return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0);
		}
		
		
		if(!ns.empty()){
			std::string fullName=ns+"::"+name;
			
			auto it=locals.find(fullName);
			if(it!=locals.end()){
				llvm::Type* ty=it->second->getAllocatedType();
				return b.CreateLoad(ty,it->second,fullName);
			}
			
			auto git=globalVars.find(fullName);
			if(git!=globalVars.end()){
				llvm::Type* ty=git->second->getValueType();
				return b.CreateLoad(ty,git->second,fullName);
			}
			
			auto fit=funcDecls.find(fullName);
			if(fit!=funcDecls.end())return fit->second;
			error(node->line,node->col,"undefined variable '"+fullName+"'");
			return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0);
		}
		
		
		
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
		
		
		llvm::GlobalVariable* gv=nullptr;
		std::string foundName=findInImportedNs(name,globalVars,gv);
		if(!foundName.empty()){
			llvm::Type* ty=gv->getValueType();
			return b.CreateLoad(ty,gv,foundName);
		}
		
		llvm::Function* fn=nullptr;
		foundName=findInImportedNs(name,funcDecls,fn);
		if(!foundName.empty())return fn;
		
		
		error(node->line,node->col,"undefined variable '"+name+"'");
		return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0);
	}
	llvm::Value* genBinaryExpr(AstNode* node){
		if(!node||!node->binary.left||!node->binary.right){
			error(node?node->line:0,node?node->col:0,"null operand in binary expression");
			return nullptr;
		}
		MioType* lmt=resolveExprMioType(node->binary.left);
		std::string structName;
		if(lmt){
			if(lmt->kind==MioTypeKind::STRUCT&&!lmt->name.empty())
				structName=resolveStructName(lmt->name);
			else if(lmt->kind==MioTypeKind::POINTER&&lmt->base_type&&lmt->base_type->kind==MioTypeKind::STRUCT)
				structName=resolveStructName(lmt->base_type->name);
		}
		if(!structName.empty()){
			MioType* rmt=resolveExprMioType(node->binary.right);
			std::string opMethod=findOperatorMethod(structName,node->binary.op,rmt);
			if(opMethod.empty()){
				error(node->line,node->col,"struct '"+structName+"' does not support operator"+tok_name(node->binary.op));
				return nullptr;
			}
			llvm::Function* callee=funcDecls[opMethod];
			if(!callee){
				error(node->line,node->col,"operator"+tok_name(node->binary.op)+" for struct '"+structName+"' not found");
				return nullptr;
			}
			std::vector<llvm::Value*> args;
			llvm::Value* thisPtr=genLValue(node->binary.left);
			if(!thisPtr)thisPtr=genExpr(node->binary.left);
			if(thisPtr){
				auto* calleeThisTy=callee->getFunctionType()->getParamType(0);
				if(thisPtr->getType()!=calleeThisTy){
					if(thisPtr->getType()->isPointerTy()&&calleeThisTy->isPointerTy()){
						thisPtr=b.CreateBitCast(thisPtr,calleeThisTy);
					}else if(thisPtr->getType()->isPointerTy()&&calleeThisTy->isStructTy()){
						thisPtr=b.CreateLoad(calleeThisTy,thisPtr);
					}else if(auto* ai=llvm::dyn_cast<llvm::AllocaInst>(thisPtr)){
						thisPtr=b.CreateLoad(ai->getAllocatedType(),thisPtr);
					}
				}else if(auto* ai=llvm::dyn_cast<llvm::AllocaInst>(thisPtr)){
					if(ai->getAllocatedType()->isPointerTy()){
						thisPtr=b.CreateLoad(ai->getAllocatedType(),thisPtr);
					}
				}
			}
			args.push_back(thisPtr);
			llvm::Value* r=genExpr(node->binary.right);
			if(r)args.push_back(r);
			return b.CreateCall(callee,args);
		}
		llvm::Value* l=genExpr(node->binary.left);
		llvm::Value* r=genExpr(node->binary.right);
		if(!l){
			error(node->line,node->col,"failed to generate left operand of binary expression");
			return nullptr;
		}
		if(!r){
			error(node->line,node->col,"failed to generate right operand of binary expression");
			return nullptr;
		}
		llvm::Type* lt=l->getType();
		llvm::Type* rt=r->getType();
		bool isFloat=lt->isFloatingPointTy()||rt->isFloatingPointTy();
		if(isFloat){
			if(!lt->isFloatingPointTy())l=genCastValue(l,rt);
			if(!rt->isFloatingPointTy())r=genCastValue(r,lt);
			if(lt!=rt)r=genCastValue(r,lt);
		}else if(lt->isPointerTy()&&rt->isPointerTy()){
			if(lt!=rt)r=b.CreatePointerCast(r,lt);
		}else if(lt->isIntegerTy()&&rt->isIntegerTy()){
			if(lt->getIntegerBitWidth()<rt->getIntegerBitWidth())l=genCastValue(l,rt);
			else r=genCastValue(r,lt);
		}
		switch(node->binary.op){
			case TOK_PLUS:
				if(lt->isPointerTy()&&rt->isIntegerTy()){
					llvm::Type* elemTy=llvm::Type::getInt8Ty(ctx);
					MioType* lmt=resolveExprMioType(node->binary.left);
					if(lmt&&lmt->kind==MioTypeKind::POINTER&&lmt->base_type&&lmt->base_type->kind!=MioTypeKind::VOID)
						elemTy=convertType(lmt->base_type);
					return b.CreateGEP(elemTy,l,r);
				}
				if(lt->isIntegerTy()&&rt->isPointerTy()){
					llvm::Type* elemTy=llvm::Type::getInt8Ty(ctx);
					MioType* rmt=resolveExprMioType(node->binary.right);
					if(rmt&&rmt->kind==MioTypeKind::POINTER&&rmt->base_type&&rmt->base_type->kind!=MioTypeKind::VOID)
						elemTy=convertType(rmt->base_type);
					return b.CreateGEP(elemTy,r,l);
				}
				return isFloat?b.CreateFAdd(l,r):b.CreateAdd(l,r);
			case TOK_MINUS:
				if(lt->isPointerTy()&&rt->isIntegerTy()){
					llvm::Type* elemTy=llvm::Type::getInt8Ty(ctx);
					MioType* lmt=resolveExprMioType(node->binary.left);
					if(lmt&&lmt->kind==MioTypeKind::POINTER&&lmt->base_type&&lmt->base_type->kind!=MioTypeKind::VOID)
						elemTy=convertType(lmt->base_type);
					return b.CreateGEP(elemTy,l,b.CreateNeg(r));
				}
				if(lt->isPointerTy()&&rt->isPointerTy()){
					l=b.CreatePtrToInt(l,llvm::Type::getInt64Ty(ctx));
					r=b.CreatePtrToInt(r,llvm::Type::getInt64Ty(ctx));
					return b.CreateSDiv(b.CreateSub(l,r),llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),1));
				}
				return isFloat?b.CreateFSub(l,r):b.CreateSub(l,r);
			case TOK_STAR:
				return isFloat?b.CreateFMul(l,r):b.CreateMul(l,r);
			case TOK_SLASH:
				return isFloat?b.CreateFDiv(l,r):b.CreateSDiv(l,r);
			case TOK_PERCENT:
				return isFloat?b.CreateFRem(l,r):b.CreateSRem(l,r);
			case TOK_EQ:
				if(lt->isPointerTy()&&rt->isIntegerTy()){l=b.CreatePtrToInt(l,rt);return b.CreateICmpEQ(l,r);}
				if(lt->isIntegerTy()&&rt->isPointerTy()){r=b.CreatePtrToInt(r,lt);return b.CreateICmpEQ(l,r);}
				return isFloat?b.CreateFCmpOEQ(l,r):b.CreateICmpEQ(l,r);
			case TOK_NEQ:
				if(lt->isPointerTy()&&rt->isIntegerTy()){l=b.CreatePtrToInt(l,rt);return b.CreateICmpNE(l,r);}
				if(lt->isIntegerTy()&&rt->isPointerTy()){r=b.CreatePtrToInt(r,lt);return b.CreateICmpNE(l,r);}
				return isFloat?b.CreateFCmpONE(l,r):b.CreateICmpNE(l,r);
			case TOK_LT:
				if(lt->isPointerTy()&&rt->isIntegerTy()){l=b.CreatePtrToInt(l,rt);return b.CreateICmpSLT(l,r);}
				if(lt->isIntegerTy()&&rt->isPointerTy()){r=b.CreatePtrToInt(r,lt);return b.CreateICmpSLT(l,r);}
				return isFloat?b.CreateFCmpOLT(l,r):b.CreateICmpSLT(l,r);
			case TOK_GT:
				if(lt->isPointerTy()&&rt->isIntegerTy()){l=b.CreatePtrToInt(l,rt);return b.CreateICmpSGT(l,r);}
				if(lt->isIntegerTy()&&rt->isPointerTy()){r=b.CreatePtrToInt(r,lt);return b.CreateICmpSGT(l,r);}
				return isFloat?b.CreateFCmpOGT(l,r):b.CreateICmpSGT(l,r);
			case TOK_LTE:
				if(lt->isPointerTy()&&rt->isIntegerTy()){l=b.CreatePtrToInt(l,rt);return b.CreateICmpSLE(l,r);}
				if(lt->isIntegerTy()&&rt->isPointerTy()){r=b.CreatePtrToInt(r,lt);return b.CreateICmpSLE(l,r);}
				return isFloat?b.CreateFCmpOLE(l,r):b.CreateICmpSLE(l,r);
			case TOK_GTE:
				if(lt->isPointerTy()&&rt->isIntegerTy()){l=b.CreatePtrToInt(l,rt);return b.CreateICmpSGE(l,r);}
				if(lt->isIntegerTy()&&rt->isPointerTy()){r=b.CreatePtrToInt(r,lt);return b.CreateICmpSGE(l,r);}
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
				error(node->line,node->col,"unsupported binary operator");
				return nullptr;
		}
	}
	llvm::Value* genUnaryExpr(AstNode* node){
		if(!node||!node->unary.operand){
			error(node?node->line:0,node?node->col:0,"null operand in unary expression");
			return nullptr;
		}
		switch(node->unary.op){
			case TOK_STAR:{
				llvm::Value* op=genExpr(node->unary.operand);
				if(!op){
					error(node->line,node->col,"failed to generate operand of dereference");
					return nullptr;
				}
				if(op->getType()->isPointerTy()){
					MioType* operandMio=resolveExprMioType(node->unary.operand);
					if(operandMio&&operandMio->kind==MioTypeKind::POINTER&&operandMio->base_type){
						llvm::Type* elemTy=convertType(operandMio->base_type);
						return b.CreateLoad(elemTy,op);
					}
					llvm::Type* elemTy=resolveExprType(node->unary.operand);
					return b.CreateLoad(elemTy,op);
				}
				return op;
			}
			case TOK_BIT_AND:{
				llvm::Value* ptr=genLValue(node->unary.operand);
				if(!ptr){
					error(node->line,node->col,"failed to generate operand of address-of");
					return nullptr;
				}
				return ptr;
			}
			case TOK_MINUS:{
				llvm::Value* op=genExpr(node->unary.operand);
				if(!op){
					error(node->line,node->col,"failed to generate operand of unary minus");
					return nullptr;
				}
				if(op->getType()->isFloatingPointTy())return b.CreateFNeg(op);
				return b.CreateNeg(op);
			}
			case TOK_NOT:{
				llvm::Value* op=genExpr(node->unary.operand);
				if(!op){
					error(node->line,node->col,"failed to generate operand of logical not");
					return nullptr;
				}
				return b.CreateNot(op);
			}
			case TOK_BIT_NOT:{
				llvm::Value* op=genExpr(node->unary.operand);
				if(!op){
					error(node->line,node->col,"failed to generate operand of bitwise not");
					return nullptr;
				}
				return b.CreateNot(op);
			}
			default:
				return genExpr(node->unary.operand);
		}
	}
	llvm::Value* genCallExpr(AstNode* node){
		if(!node||!node->call.callee){
			error(node?node->line:0,node?node->col:0,"null callee in function call");
			return nullptr;
		}
		std::string calleeName;
		llvm::Value* calleeVal=nullptr;
		bool isVirtualCall=false;
		int vtableIndex=-1;
		std::string virtualClassName;
		
		if(node->call.callee->kind==AstNodeKind::IDENT_EXPR){
			calleeName=node->call.callee->ident.name;
			std::string ns=node->call.callee->ident.namespace_name;
			if(ns=="::"){
				auto fit=funcDecls.find(calleeName);
				if(fit!=funcDecls.end())calleeVal=fit->second;
				if(!calleeVal){
					calleeVal=mod->getFunction(calleeName);
					if(calleeVal)funcDecls[calleeName]=llvm::cast<llvm::Function>(calleeVal);
				}
			}else if(!ns.empty()){
				calleeName=ns+"::"+calleeName;
			}
			if(!node->call.template_args.empty()){
				calleeVal=genTemplateInstantiation(node);
				if(!calleeVal)return nullptr;
			}else{
				if(!calleeVal){
					auto fit=funcDecls.find(calleeName);
					if(fit!=funcDecls.end())calleeVal=fit->second;
					if(!calleeVal){
						calleeVal=mod->getFunction(calleeName);
						if(calleeVal)funcDecls[calleeName]=llvm::cast<llvm::Function>(calleeVal);
					}
					
					if(!calleeVal&&ns.empty()){
						for(auto& impNs:importedNamespaces){
							std::string fullName=impNs+"::"+calleeName;
							auto fit2=funcDecls.find(fullName);
							if(fit2!=funcDecls.end()){
								calleeVal=fit2->second;
								calleeName=fullName;
								break;
							}
							llvm::Function* fn=mod->getFunction(fullName);
							if(fn){
								calleeVal=fn;
								funcDecls[fullName]=fn;
								calleeName=fullName;
								break;
							}
						}
					}
				}
			}
			
			if(!calleeVal&&node->call.template_args.empty()){
				auto deduced=tryDeduceTemplateArgs(calleeName,node);
				if(!deduced.empty()){
					node->call.template_args=deduced;
					calleeVal=genTemplateInstantiation(node);
					if(!calleeVal)return nullptr;
				}
			}
			
			if(!calleeVal&&ns=="::"){
				error(node->line,node->col,"undefined global function '"+calleeName+"'");
				return nullptr;
			}
			
			if(!calleeVal&&!structTypes.count(calleeName)&&ns.empty()){
				for(auto& impNs:importedNamespaces){
					std::string fullName=impNs+"::"+calleeName;
					if(structTypes.count(fullName)){
						calleeName=fullName;
						break;
					}
				}
			}
			
			if(!calleeVal&&structTypes.count(calleeName)){
				std::string className=calleeName;
				auto pos=calleeName.rfind("::");
				if(pos!=std::string::npos)className=calleeName.substr(pos+2);
				std::string ctorName=calleeName+"::"+className;
				unsigned expectedArgs=1+node->call.args.size();
				llvm::Function* ctor=nullptr;
				int matchCount=0;
				
				
				std::vector<llvm::Function*> candidates;
				for(auto& f:mod->functions()){
					std::string fnName=f.getName().str();
					if(fnName==ctorName||fnName.rfind(ctorName,0)==0){
						if(f.arg_size()==expectedArgs){
							candidates.push_back(&f);
						}
					}
				}
				
				
				if(candidates.size()>1){
					
					std::vector<llvm::Type*> argTypes;
					for(auto* a:node->call.args){
						llvm::Value* av=genExpr(a);
						if(av)argTypes.push_back(av->getType());
						else argTypes.push_back(nullptr);
					}
					
					
					for(auto* cand:candidates){
						bool match=true;
						for(unsigned i=0;i<argTypes.size()&&i<cand->arg_size()-1;i++){
							llvm::Type* paramTy=cand->getFunctionType()->getParamType(i+1); 
							if(argTypes[i]&&argTypes[i]!=paramTy){
								
								if(argTypes[i]->isPointerTy()&&paramTy->isPointerTy()){
									continue; 
								}
								match=false;
								break;
							}
						}
						if(match){
							if(ctor){
								error(node->line,node->col,"ambiguous constructor call for '"+ctorName+"' with "+std::to_string(expectedArgs-1)+" argument(s)");
								return nullptr;
							}
							ctor=cand;
						}
					}
					
					
					if(!ctor){
						ctor=candidates[0];
					}
				}else if(candidates.size()==1){
					ctor=candidates[0];
				}
				
				if(!ctor){
					error(node->line,node->col,"no matching constructor for '"+ctorName+"' with "+std::to_string(expectedArgs-1)+" argument(s)");
					return nullptr;
				}
				
				funcDecls[ctorName]=ctor;
			auto* st=structTypes[calleeName];
			auto* alloca=createEntryAlloca(curFn,calleeName+"_tmp",st);
			std::vector<llvm::Value*> args;
			args.push_back(alloca);
			for(auto* a:node->call.args){
					llvm::Value* av=genExpr(a);
					if(!av){
						error(node->line,node->col,"failed to generate constructor argument");
						return nullptr;
					}
					args.push_back(av);
				}
				
				b.CreateCall(ctor,args);
				return b.CreateLoad(st,alloca);
			}
		}else if(node->call.callee->kind==AstNodeKind::MEMBER_EXPR){
			auto* base=node->call.callee->member.base;
			std::string method=node->call.callee->member.member;
			std::string className;
			if(base->type&&!base->type->name.empty()){
				className=resolveStructName(base->type->name);
				if(!base->type->param_types.empty()){
					for(auto* pt:base->type->param_types)
						className+="_"+mio_type_str(pt);
				}
			}
			else if(base->kind==AstNodeKind::IDENT_EXPR){
				if(base->ident.name=="this"&&!currentClassName.empty()){
					className=currentClassName;
				}else{
					auto it=locals.find(base->ident.name);
					if(it!=locals.end()){
						llvm::Type* at=it->second->getAllocatedType();
						if(at->isStructTy())className=std::string(at->getStructName());
						else if(at->isPointerTy()){
							if(base->type&&base->type->base_type&&base->type->base_type->kind==MioTypeKind::STRUCT)
								className=base->type->base_type->name;
						}
					}
				}
			}
			if(!className.empty()){
				
				auto vit=classVTableOrder.find(className);
				if(vit!=classVTableOrder.end()){
					
					for(size_t i=0;i<vit->second.size();i++){
						
						if(vit->second[i]==method){
							isVirtualCall=true;
							virtualClassName=className;
							vtableIndex=(int)i;
							
							break;
						}
					}
				}
				
				if(!isVirtualCall){
					std::string mangledName=className+"::"+method;
					auto fit=funcDecls.find(mangledName);
					if(fit!=funcDecls.end())calleeVal=fit->second;
				}
				if(!calleeVal&&!isVirtualCall){
					auto fit=funcDecls.find(method);
					if(fit!=funcDecls.end())calleeVal=fit->second;
				}
				if(isVirtualCall){
					llvm::Value* thisPtr=genLValue(base);
					if(!thisPtr){
						thisPtr=genExpr(base);
					}
					if(!thisPtr){
						error(node->line,node->col,"cannot get 'this' pointer for virtual call");
						return nullptr;
					}
										if(auto* ai=llvm::dyn_cast<llvm::AllocaInst>(thisPtr)){
						if(ai->getAllocatedType()->isPointerTy()){
							thisPtr=b.CreateLoad(ai->getAllocatedType(),thisPtr);
						}
					}
					auto* st=structTypes[virtualClassName];
					if(!st){
						error(node->line,node->col,"struct type '"+virtualClassName+"' not found for virtual dispatch");
						return nullptr;
					}
					auto* vtablePtrPtr=b.CreateGEP(st,thisPtr,{
						llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0),
						llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),0)
					});
					auto* vtablePtr=b.CreateLoad(llvm::PointerType::get(ctx,0),vtablePtrPtr);
										auto* vtableTy=classVTableTypes[virtualClassName];
					if(!vtableTy){
						error(node->line,node->col,"vtable type for '"+virtualClassName+"' not found");
						return nullptr;
					}
					auto* fnPtrPtr=b.CreateGEP(vtableTy,vtablePtr,{
						llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0),
						llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),(unsigned)vtableIndex)
					});
					auto* fnPtr=b.CreateLoad(llvm::PointerType::get(ctx,0),fnPtrPtr);
										llvm::FunctionType* ft=nullptr;
					auto ftit=classVTableFuncTypes.find(virtualClassName);
					if(ftit!=classVTableFuncTypes.end()&&(unsigned)vtableIndex<ftit->second.size()){
						ft=ftit->second[(unsigned)vtableIndex];
					}
					if(!ft){
						error(node->line,node->col,"virtual function type not found for '"+virtualClassName+"'["+std::to_string(vtableIndex)+"]");
						return nullptr;
					}
										std::vector<llvm::Value*> args;
					args.push_back(thisPtr);
					for(auto* a:node->call.args){
						llvm::Value* av=genExpr(a);
						if(av)args.push_back(av);
					}
					return b.CreateCall(ft,fnPtr,args);
				}
				if(calleeVal){
					auto* fn=llvm::dyn_cast<llvm::Function>(calleeVal);
					if(fn&&!fn->arg_empty()&&fn->getArg(0)->getType()->isPointerTy()){
						llvm::Value* thisPtr=genLValue(base);
						if(!thisPtr)thisPtr=genExpr(base);
						std::vector<llvm::Value*> args;
						if(thisPtr){
							llvm::Type* expectedThisTy=fn->getArg(0)->getType();
							if(thisPtr->getType()==expectedThisTy){
								args.push_back(thisPtr);
							}else if(auto* ai=llvm::dyn_cast<llvm::AllocaInst>(thisPtr)){
								if(ai->getAllocatedType()->isPointerTy()){
									thisPtr=b.CreateLoad(ai->getAllocatedType(),thisPtr);
								}
								if(thisPtr->getType()!=expectedThisTy)
									thisPtr=b.CreateBitCast(thisPtr,expectedThisTy);
								args.push_back(thisPtr);
							}else{
								if(thisPtr->getType()!=expectedThisTy)
									thisPtr=b.CreateBitCast(thisPtr,expectedThisTy);
								args.push_back(thisPtr);
							}
						}
						for(auto* a:node->call.args){
							llvm::Value* av=genExpr(a);
							if(av)args.push_back(av);
						}
						for(size_t i=args.size()-(node->call.args.size());i<args.size();i++){
							size_t paramIdx=i;
							if(paramIdx<fn->arg_size()){
								llvm::Type* paramTy=fn->getArg(paramIdx)->getType();
								if(args[i]->getType()!=paramTy)
									args[i]=genCastValue(args[i],paramTy);
							}
						}
						return b.CreateCall(fn,args);
					}
				}
			}
		}
		if(!calleeVal){
			calleeVal=genExpr(node->call.callee);
			if(!calleeVal){
				error(node->line,node->col,"cannot resolve call target");
				return nullptr;
			}
		}
		auto* fn=llvm::dyn_cast<llvm::Function>(calleeVal);
		if(!fn){
			auto* ft=llvm::dyn_cast<llvm::FunctionType>(calleeVal->getType());
			if(!ft){
				error(node->line,node->col,"called value is not a function");
				return nullptr;
			}
			std::vector<llvm::Value*> args;
			for(auto* a:node->call.args){
				llvm::Value* av=genExpr(a);
				if(!av){
					error(node->line,node->col,"failed to generate constructor argument");
					return nullptr;
				}
				args.push_back(av);
			}
			return b.CreateCall(ft,calleeVal,args);
		}
		std::vector<llvm::Value*> args;
		for(size_t i=0;i<node->call.args.size();i++){
			llvm::Value* av=nullptr;
			if(i<fn->arg_size()){
				llvm::Type* paramTy=fn->getArg(i)->getType();
				MioType* argMio=resolveExprMioType(node->call.args[i]);
				if(paramTy->isPointerTy()&&argMio&&argMio->kind==MioTypeKind::ARRAY){
					llvm::Value* lv=genLValue(node->call.args[i]);
					if(lv){
						llvm::Type* arrTy=convertType(argMio);
						av=b.CreateGEP(arrTy,lv,{
							llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0),
							llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0)
						});
						if(av->getType()!=paramTy)av=b.CreateBitCast(av,paramTy);
					}
				}
				mio_type_free(argMio);
			}
			if(!av)av=genExpr(node->call.args[i]);
			if(!av){
				error(node->line,node->col,"failed to generate argument "+std::to_string(i)+" of function call");
				return nullptr;
			}
			if(i<fn->arg_size()){
				llvm::Type* paramTy=fn->getArg(i)->getType();
				if(av->getType()!=paramTy)av=genCastValue(av,paramTy);
			}
			args.push_back(av);
		}
		if(node->call.args.size()<fn->arg_size()){
			std::string lookupName=calleeName.empty()?fn->getName().str():calleeName;
			auto defIt=funcDefMap.find(lookupName);
			if(defIt==funcDefMap.end()&&!calleeName.empty()){
				defIt=funcDefMap.find(fn->getName().str());
			}
			if(defIt!=funcDefMap.end()){
				auto* funcDef=defIt->second;
				for(size_t i=node->call.args.size();i<funcDef->func_def.params.size()&&i<fn->arg_size();i++){
					if(funcDef->func_def.params[i].default_val){
						llvm::Value* dv=genExpr(funcDef->func_def.params[i].default_val);
						if(!dv){
							error(node->line,node->col,"failed to generate default value for parameter '"+funcDef->func_def.params[i].name+"'");
							return nullptr;
						}
						llvm::Type* paramTy=fn->getArg(i)->getType();
						if(dv->getType()!=paramTy)dv=genCastValue(dv,paramTy);
						args.push_back(dv);
					}
				}
			}
		}
		return b.CreateCall(fn,args);
	}
	llvm::Value* genIndexExpr(AstNode* node){
		if(!node||!node->index_expr.base||!node->index_expr.index){
			error(node?node->line:0,node?node->col:0,"null operand in index expression");
			return nullptr;
		}
		
		
		MioType* baseMio=resolveExprMioType(node->index_expr.base);
		std::string structName;
		if(baseMio){
			if(baseMio->kind==MioTypeKind::STRUCT&&!baseMio->name.empty())
				structName=resolveStructName(baseMio->name);
			else if(baseMio->kind==MioTypeKind::POINTER&&baseMio->base_type&&baseMio->base_type->kind==MioTypeKind::STRUCT)
				structName=resolveStructName(baseMio->base_type->name);
		}
		
		if(!structName.empty()){
			MioType* rmt=resolveExprMioType(node->index_expr.index);
			std::string opMethod=findOperatorMethod(structName,TOK_LBRACKET,rmt);
			if(opMethod.empty()){
				error(node->line,node->col,"struct '"+structName+"' does not support operator[]");
				return nullptr;
			}
			llvm::Function* callee=funcDecls[opMethod];
			if(!callee){
				error(node->line,node->col,"operator[] for struct '"+structName+"' not found");
				return nullptr;
			}
			std::vector<llvm::Value*> args;
			llvm::Value* thisPtr=genLValue(node->index_expr.base);
			if(!thisPtr)thisPtr=genExpr(node->index_expr.base);
			if(thisPtr){
				auto* calleeThisTy=callee->getFunctionType()->getParamType(0);
				if(thisPtr->getType()!=calleeThisTy){
					if(thisPtr->getType()->isPointerTy()&&calleeThisTy->isPointerTy()){
						thisPtr=b.CreateBitCast(thisPtr,calleeThisTy);
					}else if(thisPtr->getType()->isPointerTy()&&calleeThisTy->isStructTy()){
						thisPtr=b.CreateLoad(calleeThisTy,thisPtr);
					}else if(auto* ai=llvm::dyn_cast<llvm::AllocaInst>(thisPtr)){
						thisPtr=b.CreateLoad(ai->getAllocatedType(),thisPtr);
					}
				}
			}
			args.push_back(thisPtr);
			llvm::Value* idx=genExpr(node->index_expr.index);
			if(idx)args.push_back(idx);
			return b.CreateCall(callee,args);
		}
		
		
		if(!baseMio){
			error(node->line,node->col,"cannot use operator[] on expression with unknown type");
			return nullptr;
		}
		
		if(baseMio->kind!=MioTypeKind::POINTER){
			error(node->line,node->col,"operator[] requires a pointer or struct with operator[], but got '"+mio_type_str(baseMio)+"'");
			return nullptr;
		}
		
		
		llvm::Value* base=genExpr(node->index_expr.base);
		if(!base){
			error(node->line,node->col,"failed to generate base of index expression");
			return nullptr;
		}
		llvm::Value* idx=genExpr(node->index_expr.index);
		if(!idx){
			error(node->line,node->col,"failed to generate index of index expression");
			return nullptr;
		}
		llvm::Type* elemTy=resolveExprType(node);
		if(!idx->getType()->isIntegerTy(64))
			idx=b.CreateSExt(idx,llvm::Type::getInt64Ty(ctx));
		llvm::Value* ptr=b.CreateGEP(elemTy,base,idx);
		return b.CreateLoad(elemTy,ptr);
	}
	llvm::Value* genMemberExpr(AstNode* node){
		if(!node||!node->member.base){
			error(node?node->line:0,node?node->col:0,"null base in member expression");
			return nullptr;
		}
		llvm::Value* base=genLValue(node->member.base);
		if(!base)base=genExpr(node->member.base);
		if(!base){
			error(node->line,node->col,"failed to generate base of member expression");
			return nullptr;
		}
		std::string structName;
		if(node->member.base->type&&!node->member.base->type->name.empty())
			structName=resolveStructName(node->member.base->type->name);
		else if(node->member.base->kind==AstNodeKind::IDENT_EXPR){
			if(node->member.base->ident.name=="this"&&!currentClassName.empty()){
				structName=currentClassName;
			}else{
				auto it=locals.find(node->member.base->ident.name);
				if(it!=locals.end()){
					llvm::Type* at=it->second->getAllocatedType();
					if(at->isStructTy())structName=std::string(at->getStructName());
					else if(at->isPointerTy()){
						if(node->member.base->type&&node->member.base->type->base_type&&node->member.base->type->base_type->kind==MioTypeKind::STRUCT)
							structName=resolveStructName(node->member.base->type->base_type->name);
					}
				}
			}
		}
		if(!structName.empty()&&structFieldIdx.count(structName)){
			auto mit=structFieldIdx[structName].find(node->member.member);
			if(mit==structFieldIdx[structName].end()){
				error(node->line,node->col,"field '"+node->member.member+"' not found in struct '"+structName+"'");
				return nullptr;
			}
			unsigned idx=mit->second;
			auto sit=structTypes.find(structName);
			llvm::StructType* st=sit!=structTypes.end()?sit->second:nullptr;
			if(!st){
				error(node->line,node->col,"struct type '"+structName+"' not found");
				return nullptr;
			}
			
			llvm::Value* ptr=base;
			if(auto* ai=llvm::dyn_cast<llvm::AllocaInst>(ptr)){
				if(ai->getAllocatedType()->isPointerTy())
					ptr=b.CreateLoad(ai->getAllocatedType(),ptr);
			}
			
			if(ptr->getType()->isPointerTy()){
			if(!ptr->getType()->isStructTy()){
				ptr=b.CreateBitCast(ptr,st->getPointerTo());
			}
			llvm::Value* idx0=llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0);
			llvm::Value* idx1=llvm::ConstantInt::get(llvm::Type::getInt32Ty(ctx),idx);
			return b.CreateGEP(st,ptr,{idx0,idx1});
		}else if(ptr->getType()->isStructTy()){
			error(node,"cannot get address of member '"+node->member.member+"' on struct value");
			return nullptr;
			}else{
				error(node,"cannot access member '"+node->member.member+"' on value");
				return nullptr;
			}
		}
		error(node,"cannot access member '"+node->member.member+"' on type '"+structName+"'");
		return nullptr;
	}
	llvm::Value* genArrayLit(AstNode* node){
		if(!node)return nullptr;
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
			if(!ev){
				error(node->line,node->col,"failed to generate array element "+std::to_string(i));
				return nullptr;
			}
			if(ev->getType()!=elemTy)ev=genCastValue(ev,elemTy);
			llvm::Value* ptr=b.CreateGEP(arrTy,alloca,{llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),0),llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx),i)});
			b.CreateStore(ev,ptr);
		}
		return b.CreateLoad(arrTy,alloca);
	}
	llvm::Value* genCastExpr(AstNode* node){
		if(!node||!node->cast_expr.expr||!node->cast_expr.target_type){
			error(node?node->line:0,node?node->col:0,"null operand in cast expression");
			return nullptr;
		}
		llvm::Value* v=genExpr(node->cast_expr.expr);
		if(!v){
			error(node->line,node->col,"failed to generate expression in cast");
			return nullptr;
		}
		llvm::Type* target=convertType(node->cast_expr.target_type);
		return genCastValue(v,target);
	}
	llvm::Value* genAssignExpr(AstNode* node){
		if(!node||!node->assign.left||!node->assign.right){
			error(node?node->line:0,node?node->col:0,"null operand in assignment expression");
			return nullptr;
		}
		MioType* leftMio=resolveExprMioType(node->assign.left);
		std::string structName;
		if(leftMio){
			if(leftMio->kind==MioTypeKind::STRUCT&&!leftMio->name.empty())
				structName=resolveStructName(leftMio->name);
			else if(leftMio->kind==MioTypeKind::POINTER&&leftMio->base_type&&leftMio->base_type->kind==MioTypeKind::STRUCT)
				structName=resolveStructName(leftMio->base_type->name);
		}
		if(!structName.empty()&&node->assign.op!=TOK_ASSIGN){
			MioType* rmt=resolveExprMioType(node->assign.right);
			std::string opMethod=findOperatorMethod(structName,node->assign.op,rmt);
			if(!opMethod.empty()){
				llvm::Function* callee=funcDecls[opMethod];
				if(callee){
					std::vector<llvm::Value*> args;
					llvm::Value* thisPtr=genLValue(node->assign.left);
					if(!thisPtr)thisPtr=genExpr(node->assign.left);
					if(thisPtr){
						auto* calleeThisTy=callee->getFunctionType()->getParamType(0);
						if(thisPtr->getType()!=calleeThisTy){
							if(thisPtr->getType()->isPointerTy()&&calleeThisTy->isPointerTy()){
								thisPtr=b.CreateBitCast(thisPtr,calleeThisTy);
							}else if(thisPtr->getType()->isPointerTy()&&calleeThisTy->isStructTy()){
								thisPtr=b.CreateLoad(calleeThisTy,thisPtr);
							}else if(auto* ai=llvm::dyn_cast<llvm::AllocaInst>(thisPtr)){
								thisPtr=b.CreateLoad(ai->getAllocatedType(),thisPtr);
							}
						}
					}
					args.push_back(thisPtr);
					llvm::Value* r=genExpr(node->assign.right);
					if(r)args.push_back(r);
					return b.CreateCall(callee,args);
				}
			}
		}
		
		llvm::Value* rhs=genExpr(node->assign.right);
		if(!rhs){
			error(node->line,node->col,"failed to generate right-hand side of assignment expression");
			return nullptr;
		}
		llvm::Value* ptr=genLValue(node->assign.left);
		if(!ptr){
			error(node->line,node->col,"failed to generate left-hand side of assignment expression");
			return nullptr;
		}
		llvm::Type* valTy=ptr->getType();
		if(valTy->isPointerTy()){
			llvm::Type* elemTy=resolveExprType(node->assign.left);
			if(rhs->getType()!=elemTy)rhs=genCastValue(rhs,elemTy);
			if(node->assign.op!=TOK_ASSIGN){
				llvm::Value* lhs=b.CreateLoad(elemTy,ptr);
				switch(node->assign.op){
					case TOK_PLUS_ASSIGN:
						if(elemTy->isPointerTy()&&rhs->getType()->isIntegerTy()){
							MioType* lmio=resolveExprMioType(node->assign.left);
							llvm::Type* gepElemTy=llvm::Type::getInt8Ty(ctx);
							if(lmio&&lmio->kind==MioTypeKind::POINTER&&lmio->base_type&&lmio->base_type->kind!=MioTypeKind::VOID)
								gepElemTy=convertType(lmio->base_type);
							rhs=b.CreateGEP(gepElemTy,lhs,rhs);
						}else if(elemTy->isIntegerTy()||elemTy->isFloatingPointTy()){
							rhs=b.CreateAdd(lhs,rhs);
						}else{
							error(node->line,node->col,"unsupported operator+= for type '"+mio_type_str(leftMio)+"'");
							return nullptr;
						}
						break;
					case TOK_MINUS_ASSIGN:
						if(elemTy->isPointerTy()&&rhs->getType()->isIntegerTy()){
							MioType* lmio=resolveExprMioType(node->assign.left);
							llvm::Type* gepElemTy=llvm::Type::getInt8Ty(ctx);
							if(lmio&&lmio->kind==MioTypeKind::POINTER&&lmio->base_type&&lmio->base_type->kind!=MioTypeKind::VOID)
								gepElemTy=convertType(lmio->base_type);
							rhs=b.CreateGEP(gepElemTy,lhs,b.CreateNeg(rhs));
						}else if(elemTy->isIntegerTy()||elemTy->isFloatingPointTy()){
							rhs=b.CreateSub(lhs,rhs);
						}else{
							error(node->line,node->col,"unsupported operator-= for type '"+mio_type_str(leftMio)+"'");
							return nullptr;
						}
						break;
					case TOK_STAR_ASSIGN:
						if(elemTy->isIntegerTy()||elemTy->isFloatingPointTy())rhs=b.CreateMul(lhs,rhs);
						else{error(node->line,node->col,"unsupported operator*=");return nullptr;}
						break;
					case TOK_SLASH_ASSIGN:
						if(elemTy->isIntegerTy()||elemTy->isFloatingPointTy())rhs=elemTy->isFloatingPointTy()?b.CreateFDiv(lhs,rhs):b.CreateSDiv(lhs,rhs);
						else{error(node->line,node->col,"unsupported operator/=");return nullptr;}
						break;
					case TOK_PERCENT_ASSIGN:
						if(elemTy->isIntegerTy())rhs=b.CreateSRem(lhs,rhs);
						else{error(node->line,node->col,"unsupported operator%=");return nullptr;}
						break;
					case TOK_AND_ASSIGN:
						if(elemTy->isIntegerTy())rhs=b.CreateAnd(lhs,rhs);
						else{error(node->line,node->col,"unsupported operator&=");return nullptr;}
						break;
					case TOK_OR_ASSIGN:
						if(elemTy->isIntegerTy())rhs=b.CreateOr(lhs,rhs);
						else{error(node->line,node->col,"unsupported operator|=");return nullptr;}
						break;
					case TOK_XOR_ASSIGN:
						if(elemTy->isIntegerTy())rhs=b.CreateXor(lhs,rhs);
						else{error(node->line,node->col,"unsupported operator^=");return nullptr;}
						break;
					case TOK_LSHIFT_ASSIGN:
						if(elemTy->isIntegerTy())rhs=b.CreateShl(lhs,rhs);
						else{error(node->line,node->col,"unsupported operator<<=");return nullptr;}
						break;
					case TOK_RSHIFT_ASSIGN:
						if(elemTy->isIntegerTy())rhs=b.CreateAShr(lhs,rhs);
						else{error(node->line,node->col,"unsupported operator>>=");return nullptr;}
						break;
					default:
						error(node->line,node->col,"unsupported compound assignment operator");
						break;
				}
			}
		}
		b.CreateStore(rhs,ptr);
		return rhs;
	}
public:
	Compiler():b(ctx),curFn(nullptr),curBB(nullptr),thisAlloca(nullptr),optLevel(0),modName("mio"),filename(""),stringCounter(0){
		mod=std::make_unique<llvm::Module>("mio",ctx);
	}
	void error(AstNode* node,const std::string& msg){
		fprintf(stderr,"%s:%d:%d: error: %s\n",node->filename?node->filename->c_str():filename.c_str(),node->line,node->col,msg.c_str());
		g_error_count++;
	}
	void error(int line,int col,const std::string& msg){
		fprintf(stderr,"%s:%d:%d: error: %s\n",filename.c_str(),line,col,msg.c_str());
		g_error_count++;
	}
	void error(const std::string& msg){
		fprintf(stderr,"error: %s\n",msg.c_str());
		g_error_count++;
	}
	~Compiler()=default;
	void generate(AstNode* program){
		if(!program){
			error("null program");
			return;
		}
		genProgram(program);
		if(llvm::verifyModule(*mod,&llvm::errs()))
			error("Error verifying module");
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
			fprintf(stderr,"error: failed to get target for triple '%s': %s\n",targetTriple.c_str(),errMsg?errMsg:"unknown target error");
			if(errMsg)LLVMDisposeErrorMessage(errMsg);
			return false;
		}
		LLVMCodeGenOptLevel cgLevel=LLVMCodeGenLevelDefault;
		switch(optLevel){
			case 0: cgLevel=LLVMCodeGenLevelNone;break;
			case 1: cgLevel=LLVMCodeGenLevelLess;break;
			case 2: cgLevel=LLVMCodeGenLevelDefault;break;
			case 3: cgLevel=LLVMCodeGenLevelAggressive;break;
			default: cgLevel=LLVMCodeGenLevelDefault;break;
		}
		LLVMTargetMachineRef tm=LLVMCreateTargetMachine(
			targetRef,targetTriple.c_str(),"generic","",
			cgLevel,LLVMRelocPIC,LLVMCodeModelDefault);
		if(!tm){
			fprintf(stderr,"error: cannot create target machine for triple '%s'\n",targetTriple.c_str());
			return false;
		}
		auto* targetMachine=reinterpret_cast<llvm::TargetMachine*>(tm);
		mod->setDataLayout(targetMachine->createDataLayout());
		mod->setTargetTriple(llvm::Triple(targetTriple));
		if(LLVMTargetMachineEmitToFile(tm,reinterpret_cast<LLVMModuleRef>(mod.get()),const_cast<char*>(path.c_str()),LLVMObjectFile,&errMsg)){
			fprintf(stderr,"error: failed to emit object file '%s': %s\n",path.c_str(),errMsg?errMsg:"unknown emit error");
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
			fprintf(stderr,"error: failed to get target for triple '%s': %s\n",targetTriple.c_str(),errMsg?errMsg:"unknown target error");
			if(errMsg)LLVMDisposeErrorMessage(errMsg);
			return false;
		}
		LLVMCodeGenOptLevel cgLevel=LLVMCodeGenLevelDefault;
		switch(optLevel){
			case 0: cgLevel=LLVMCodeGenLevelNone;break;
			case 1: cgLevel=LLVMCodeGenLevelLess;break;
			case 2: cgLevel=LLVMCodeGenLevelDefault;break;
			case 3: cgLevel=LLVMCodeGenLevelAggressive;break;
			default: cgLevel=LLVMCodeGenLevelDefault;break;
		}
		LLVMTargetMachineRef tm=LLVMCreateTargetMachine(
			targetRef,targetTriple.c_str(),"generic","",
			cgLevel,LLVMRelocPIC,LLVMCodeModelDefault);
		if(!tm){
			fprintf(stderr,"error: cannot create target machine for triple '%s'\n",targetTriple.c_str());
			return false;
		}
		auto* targetMachine=reinterpret_cast<llvm::TargetMachine*>(tm);
		mod->setDataLayout(targetMachine->createDataLayout());
		mod->setTargetTriple(llvm::Triple(targetTriple));
		if(LLVMTargetMachineEmitToFile(tm,reinterpret_cast<LLVMModuleRef>(mod.get()),const_cast<char*>(path.c_str()),LLVMAssemblyFile,&errMsg)){
			fprintf(stderr,"error: failed to emit assembly file '%s': %s\n",path.c_str(),errMsg?errMsg:"unknown emit error");
			if(errMsg)LLVMDisposeErrorMessage(errMsg);
			LLVMDisposeTargetMachine(tm);
			return false;
		}
		LLVMDisposeTargetMachine(tm);
		return true;
	}
	bool linkExecutable(const std::string& objPath,const std::string& exePath,bool staticLink=false,const std::vector<std::string>& linkLibs={},const std::string& bundledLibPath=""){
		std::vector<std::string> files;
		files.push_back(objPath);
		return linkExecutableFiles(files,exePath,staticLink,linkLibs,bundledLibPath);
	}
	bool linkExecutableFiles(const std::vector<std::string>& objPaths,const std::string& exePath,bool staticLink=false,const std::vector<std::string>& linkLibs={},const std::string& bundledLibPath=""){
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
				for(const auto& obj:objPaths){
					addArg(obj);
				}
				addArg("/out:"+exePath);
				addArg("/subsystem:console");
				if(!bundledLibPath.empty()){
					addArg("/libpath:"+bundledLibPath);
				}
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
					addArg("/defaultlib:libvcruntime");
					addArg("/defaultlib:legacy_stdio_definitions");
					addArg("/nodefaultlib:libcmt");
				}
				for(const auto& lib:linkLibs){
					addArg(lib);
				}
				return lld::coff::link(args,llvm::outs(),llvm::errs(),false,false);
			}
			case llvm::Triple::ELF:{
				std::string cmd="cc";
				for(const auto& obj:objPaths){
					cmd+=" "+obj;
				}
				cmd+=" -o "+exePath;
				if(staticLink){
					cmd+=" -static";
				}
				for(const auto& lib:linkLibs){
					cmd+=" "+lib;
				}
				int ret=std::system(cmd.c_str());
				if(ret!=0){
					fprintf(stderr,"error: linking failed\n");
					return false;
				}
				return true;
			}
			case llvm::Triple::MachO:{
				addArg("mioc");
				for(const auto& obj:objPaths){
					addArg(obj);
				}
				addArg("-o");
				addArg(exePath);
				addArg("-lSystem");
				for(const auto& lib:linkLibs){
					addArg(lib);
				}
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
	bool compiling(
		const std::string& input_file,
		const std::string& output_file,
		const std::vector<std::string>& include_paths,
		const std::vector<std::string>& defines,
		const std::vector<std::string>& link_libs,
		const std::string& bundled_lib_path,
		bool emit_asm=false,
		bool compile_only=false,
		bool static_link=false,
		bool release=false,
		int opt_level=0
	){
		optLevel=opt_level;
		filename=input_file;
		size_t dot=input_file.find_last_of('.');
		modName=(dot!=std::string::npos)?input_file.substr(0,dot):input_file;
		mod=std::make_unique<llvm::Module>(modName,ctx);
		std::ifstream file(input_file,std::ios::binary);
		if(!file.is_open()){
			fprintf(stderr,"error: cannot open file '%s'\n",input_file.c_str());
			return false;
		}
		file.seekg(0,std::ios::end);
		std::streamsize size=file.tellg();
		file.seekg(0,std::ios::beg);
		if(size<=0){
			fprintf(stderr,"error: file '%s' is empty\n",input_file.c_str());
			return false;
		}
		std::string source(size,'\0');
		if(!file.read(&source[0],size)){
			fprintf(stderr,"error: failed to read file '%s'\n",input_file.c_str());
			return false;
		}
		file.close();
		Lexer lexer(source,input_file);
		Parser parser(&lexer,input_file,include_paths);
		for(const auto& m:defines)parser.add_macro(m,"1");
		AstNode* program=parser.parse();
		if(!program) return false;
		if(g_error_count){
			delete program;
			return false;
		}
		generate(program);
		if(g_error_count){
			delete program;
			return false;
		}
		std::string base_name=output_file;
		if(base_name.empty()){
			base_name=input_file;
			size_t dot=base_name.find_last_of('.');
			if(dot!=std::string::npos)base_name=base_name.substr(0,dot);
		}
		bool useCache=false;
		if(release){
			std::string cache_obj_path=base_name+".o";
			struct stat st;
			if(stat(cache_obj_path.c_str(),&st)==0){
				struct stat src_st;
				if(stat(input_file.c_str(),&src_st)==0){
					if(st.st_mtime>=src_st.st_mtime){
						useCache=true;
						fprintf(stdout,"[cache] using cached object file '%s'\n",cache_obj_path.c_str());
					}
				}
			}
		}
		bool ok=true;
		if(useCache){
			std::string obj_path=base_name+".o";
			if(emit_asm||compile_only||isLLVMFile(output_file)||isAssemblyFile(output_file)||isObjectFile(output_file)){
				ok=true;
			}else{
				std::string exe_path=output_file.empty()?base_name+getExeExtension():output_file;
				ok=linkExecutable(obj_path,exe_path,static_link,link_libs,bundled_lib_path);
				if(ok)fprintf(stdout,"Generated: %s\n",exe_path.c_str());
				else fprintf(stderr,"error: linking failed\n");
			}
		}else if(isLLVMFile(output_file)){
			std::string ll_path=output_file.empty()?base_name+".ll":output_file;
			ok=emitLLVM(ll_path);
			if(ok)fprintf(stdout,"Generated: %s\n",ll_path.c_str());
		}else if(emit_asm||isAssemblyFile(output_file)){
			std::string asm_path=output_file.empty()?base_name+".s":output_file;
			ok=emitAssembly(asm_path);
			if(ok)fprintf(stdout,"Generated: %s\n",asm_path.c_str());
		}else if(compile_only||isObjectFile(output_file)){
			std::string obj_path=output_file.empty()?base_name+".o":output_file;
			ok=emitObject(obj_path);
			if(ok)fprintf(stdout,"Generated: %s\n",obj_path.c_str());
		}else{
			std::string obj_path=base_name+".o";
			ok=emitObject(obj_path);
			if(!ok){
				fprintf(stderr,"error: failed to emit object file '%s'\n",obj_path.c_str());
				delete program;
				return false;
			}
			std::string exe_path=output_file.empty()?base_name+getExeExtension():output_file;
			ok=linkExecutable(obj_path,exe_path,static_link,link_libs,bundled_lib_path);
			if(ok){
				fprintf(stdout,"Generated: %s\n",exe_path.c_str());
				std::remove(obj_path.c_str());
			}else{
				fprintf(stderr,"error: linking failed for '%s'\n",exe_path.c_str());
			}
		}
		delete program;
		return ok;
	}
};
#endif