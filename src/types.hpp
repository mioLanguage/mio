#ifndef MIO_TYPES_HPP
#define MIO_TYPES_HPP
#include<cstddef>
#include<cstdlib>
#include<string>
#include<vector>
#include<cstdio>
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
	STRUCT,
	ENUM,
	UNION,
	ARRAY,
	FUNC,
	POINTER
};
class MioType{
public:
	MioTypeKind kind;
	std::string name;
	int array_size;
	MioType* base_type;
	std::vector<MioType*> param_types;
	MioType(MioTypeKind k): kind(k), array_size(0), base_type(nullptr) {}
	MioType(MioTypeKind k, const std::string& n): kind(k), name(n), array_size(0), base_type(nullptr) {}
	MioType(MioType* base, int size): kind(MioTypeKind::ARRAY), array_size(size), base_type(base) {}
	~MioType(){
		if(base_type) delete base_type;
		for(auto* p : param_types) delete p;
	}
	MioType(const MioType& other){
		kind=other.kind;
		name=other.name;
		array_size=other.array_size;
		base_type=other.base_type ? new MioType(*other.base_type) : nullptr;
		for(auto* p : other.param_types){
			param_types.push_back(new MioType(*p));
		}
	}
	MioType& operator=(const MioType& other){
		if(this==&other) return *this;
		if(base_type){ delete base_type; base_type=nullptr; }
		for(auto* p : param_types){ delete p; }
		param_types.clear();
		kind=other.kind;
		name=other.name;
		array_size=other.array_size;
		base_type=other.base_type ? new MioType(*other.base_type) : nullptr;
		for(auto* p : other.param_types){
			param_types.push_back(new MioType(*p));
		}
		return *this;
	}
	MioType(MioType&& other) noexcept
		: kind(other.kind), name(std::move(other.name)), 
		  array_size(other.array_size), base_type(other.base_type),
		  param_types(std::move(other.param_types)){
		other.base_type=nullptr;
	}
	MioType& operator=(MioType&& other) noexcept{
		if(this==&other) return *this;
		if(base_type){ delete base_type; }
		for(auto* p : param_types){ delete p; }
		kind=other.kind;
		name=std::move(other.name);
		array_size=other.array_size;
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
			case MioTypeKind::STRUCT:
			case MioTypeKind::ENUM:
			case MioTypeKind::UNION:
				return name.empty() ? "void" : name.c_str();
			default: return "void";
		}
	}
};
inline MioType* mio_type_new(MioTypeKind kind){
	return new MioType(kind);
}
inline MioType* mio_type_new_named(MioTypeKind kind,const std::string& name){
	return new MioType(kind, name);
}
inline MioType* mio_type_new_array(MioType* base,int size){
	return new MioType(base, size);
}
inline MioType* mio_type_new_pointer(MioType* base){
	MioType* mt=new MioType(MioTypeKind::POINTER);
	mt->base_type=base;
	return mt;
}
inline MioType* mio_type_clone(const MioType* type){
	if(!type) return nullptr;
	return new MioType(*type);
}
inline void mio_type_free(MioType* type){
	delete type;
}
inline const char* mio_type_c_name(const MioType* type){
	if(!type) return "void";
	return type->c_name();
}
#endif