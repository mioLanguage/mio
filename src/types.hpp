#ifndef MIO_TYPES_HPP
#define MIO_TYPES_HPP
#include<cstddef>
#include<cstdlib>
#include<string>
#include<vector>
#include<cstdio>
#include<set>
struct FilenamePool{
	std::set<std::string> pool;
	const std::string* get(const std::string& s){
		auto result=pool.insert(s);
		return &(*result.first);
	}
}g_filename_pool;
enum class MioTypeKind{
	VOID,
	I8,
	I16,
	I32,
	I64,
	I128,
	U8,
	U16,
	U32,
	U64,
	U128,
	USIZE,
	ISIZE,
	F32,
	F64,
	BOOL,
	CHAR,
	CLASS,
	ENUM,
	UNION,
	ARRAY,
	FUNC,
	POINTER,
	REFERENCE,
	RVALUE_REFERENCE
};
class AstNode;
class MioType{
public:
	MioTypeKind kind;
	std::string name;
	int array_size;
	int ref_count;
	int line;
	int col;
	const std::string* filename;
	MioType* base_type;
	std::vector<MioType*> param_types;
	std::vector<AstNode*> value_args;
	bool is_const;
	MioType(MioTypeKind k): kind(k),array_size(0),ref_count(0),line(0),col(0),filename(nullptr),base_type(nullptr),is_const(false) {}
	MioType(MioTypeKind k,const std::string& n): kind(k),name(n),array_size(0),ref_count(0),line(0),col(0),filename(nullptr),base_type(nullptr),is_const(false) {}
	MioType(MioType* base,int size): kind(MioTypeKind::ARRAY),array_size(size),ref_count(0),line(base?base->line:0),col(base?base->col:0),filename(base?base->filename:nullptr),base_type(base),is_const(false) {}
	~MioType(){
		if(base_type) delete base_type;
		for(auto* p:param_types) delete p;
	}
	MioType(const MioType& other){
		kind=other.kind;
		name=other.name;
		array_size=other.array_size;
		ref_count=other.ref_count;
		line=other.line;
		col=other.col;
		filename=other.filename;
		is_const=other.is_const;
		value_args=other.value_args;
		base_type=other.base_type ? new MioType(*other.base_type):nullptr;
		for(auto* p:other.param_types){
			param_types.push_back(new MioType(*p));
		}
	}
	MioType& operator=(const MioType& other){
		if(this==&other) return *this;
		if(base_type){ delete base_type; base_type=nullptr; }
		for(auto* p:param_types){ delete p; }
		param_types.clear();
		kind=other.kind;
		name=other.name;
		array_size=other.array_size;
		ref_count=other.ref_count;
		line=other.line;
		col=other.col;
		filename=other.filename;
		is_const=other.is_const;
		value_args=other.value_args;
		base_type=other.base_type ? new MioType(*other.base_type):nullptr;
		for(auto* p:other.param_types){
			param_types.push_back(new MioType(*p));
		}
		return *this;
	}
	MioType(MioType&& other) noexcept
		: kind(other.kind),name(std::move(other.name)),
		  array_size(other.array_size),ref_count(other.ref_count),
		  line(other.line),col(other.col),is_const(other.is_const),
		  value_args(std::move(other.value_args)),
		  base_type(other.base_type),
		  param_types(std::move(other.param_types)){
		other.base_type=nullptr;
	}
	MioType& operator=(MioType&& other) noexcept{
		if(this==&other) return *this;
		if(base_type){ delete base_type; }
		for(auto* p:param_types){ delete p; }
		kind=other.kind;
		name=std::move(other.name);
		array_size=other.array_size;
		ref_count=other.ref_count;
		line=other.line;
		col=other.col;
		is_const=other.is_const;
		value_args=std::move(other.value_args);
		base_type=other.base_type;
		param_types=std::move(other.param_types);
		other.base_type=nullptr;
		return *this;
	}
	const char* c_name() const{
		switch(kind){
			case MioTypeKind::VOID:    return "void";
			case MioTypeKind::I8:      return "int8_t";
			case MioTypeKind::I16:     return "int16_t";
			case MioTypeKind::I32:     return "int32_t";
			case MioTypeKind::I64:     return "int64_t";
			case MioTypeKind::I128:    return "__int128_t";
			case MioTypeKind::U8:      return "uint8_t";
			case MioTypeKind::U16:     return "uint16_t";
			case MioTypeKind::U32:     return "uint32_t";
			case MioTypeKind::U64:     return "uint64_t";
			case MioTypeKind::U128:    return "__uint128_t";
			case MioTypeKind::USIZE:   return "size_t";
			case MioTypeKind::ISIZE:   return "ssize_t";
			case MioTypeKind::F32:     return "float";
			case MioTypeKind::F64:     return "double";
			case MioTypeKind::BOOL:    return "bool";
			case MioTypeKind::CHAR:    return "char";
			case MioTypeKind::CLASS:
			case MioTypeKind::ENUM:
			case MioTypeKind::UNION:
				return name.empty() ? "void":name.c_str();
			default: return "void";
		}
	}
};
inline MioType* mio_type_new(MioTypeKind kind){
	return new MioType(kind);
}
inline MioType* mio_type_new_named(MioTypeKind kind,const std::string& name){
	return new MioType(kind,name);
}
inline MioType* mio_type_new_array(MioType* base,int size){
	return new MioType(base,size);
}
inline MioType* mio_type_new_pointer(MioType* base);
inline MioType* mio_type_new_reference(MioType* base,bool is_rvalue=false);
inline MioType* mio_type_add_ref(MioType* base,bool is_rvalue=false){
	if(!base)return nullptr;
	if(base->kind==MioTypeKind::REFERENCE||base->kind==MioTypeKind::RVALUE_REFERENCE){
		base->ref_count++;
		return base;
	}
	return mio_type_new_reference(base,is_rvalue);
}
inline MioType* mio_type_clone(const MioType* type){
	if(!type) return nullptr;
	return new MioType(*type);
}
inline MioType* mio_type_new_pointer(MioType* base){
	MioType* mt=new MioType(MioTypeKind::POINTER);
	mt->base_type=mio_type_clone(base);
	mt->filename=base?base->filename:nullptr;
	return mt;
}
inline MioType* mio_type_new_func(MioType* ret,const std::vector<MioType*>& params){
	MioType* mt=new MioType(MioTypeKind::FUNC);
	mt->base_type=mio_type_clone(ret);
	for(auto* p:params) mt->param_types.push_back(mio_type_clone(p));
	if(ret) mt->filename=ret->filename;
	return mt;
}
inline MioType* mio_type_new_reference(MioType* base,bool is_rvalue){
	if(!base)return nullptr;
	if(base->kind==MioTypeKind::REFERENCE){
		base->ref_count++;
		return base;
	}
	if(base->kind==MioTypeKind::RVALUE_REFERENCE){
		base->ref_count++;
		return base;
	}
	MioType* mt=new MioType(is_rvalue?MioTypeKind::RVALUE_REFERENCE:MioTypeKind::REFERENCE);
	mt->base_type=mio_type_clone(base);
	mt->filename=base->filename;
	mt->ref_count=1;
	return mt;
}
inline void mio_type_free(MioType* type){
	delete type;
}
inline const char* mio_type_c_name(const MioType* type){
	if(!type){
		fprintf(stderr,"error: mio_type_c_name called with null type\n");
		return "";
	}
	return type->c_name();
}
inline std::string mio_type_str(const MioType* type){
	if(!type){
		fprintf(stderr,"error: mio_type_str called with null type\n");
		return "";
	}
	std::string prefix=type->is_const?"const ":"";
	std::string base;
	switch(type->kind){
		case MioTypeKind::VOID: base="void"; break;
		case MioTypeKind::I8: base="i8"; break;
		case MioTypeKind::I16: base="i16"; break;
		case MioTypeKind::I32: base="i32"; break;
		case MioTypeKind::I64: base="i64"; break;
		case MioTypeKind::I128: base="i128"; break;
		case MioTypeKind::U8: base="u8"; break;
		case MioTypeKind::U16: base="u16"; break;
		case MioTypeKind::U32: base="u32"; break;
		case MioTypeKind::U64: base="u64"; break;
		case MioTypeKind::U128: base="u128"; break;
		case MioTypeKind::USIZE: base="usize"; break;
		case MioTypeKind::ISIZE: base="isize"; break;
		case MioTypeKind::F32: base="f32"; break;
		case MioTypeKind::F64: base="f64"; break;
		case MioTypeKind::BOOL: base="bool"; break;
		case MioTypeKind::CHAR: base="char"; break;
		case MioTypeKind::POINTER:
			if(type->base_type){
				base=mio_type_str(type->base_type)+"*";
			}else{
				base="void*";
			}
			break;
		case MioTypeKind::REFERENCE:
			if(type->base_type){
				base=mio_type_str(type->base_type)+"&";
				for(int i=1;i<type->ref_count;i++) base+="&";
			}else{
				base="void&";
			}
			break;
		case MioTypeKind::RVALUE_REFERENCE:
			if(type->base_type){
				base=mio_type_str(type->base_type)+"&&";
				for(int i=1;i<type->ref_count;i++) base+="&";
			}else{
				base="void&&";
			}
			break;
		case MioTypeKind::CLASS:
	case MioTypeKind::ENUM:
	case MioTypeKind::UNION:
		base=type->name.empty()?"":type->name;
		break;
	case MioTypeKind::FUNC:
		base="(";
		for(size_t i=0;i<type->param_types.size();i++){
			if(i>0) base+=",";
			base+=mio_type_str(type->param_types[i]);
		}
		base+=")"+mio_type_str(type->base_type);
		break;
	default:
		fprintf(stderr,"error: mio_type_str called with unknown type kind %d\n",(int)type->kind);
		base="";
		break;
	}
	return prefix+base;
}
#endif