#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<string>
#include<vector>
#include<fstream>
#include<new>
#include<memory>
#include"token.hpp"
#include"lexer.hpp"
#include"parser.hpp"
#include"ast.hpp"
#include"codegen.hpp"
static std::string read_file(const std::string& path){
	std::ifstream file(path,std::ios::binary);
	if(!file.is_open()){
		std::fprintf(stderr,"error: cannot open file '%s'\n",path.c_str());
		exit(1);
	}
	try{
		file.seekg(0,std::ios::end);
		std::streamsize size=file.tellg();
		file.seekg(0,std::ios::beg);
		if(size<=0){
			fprintf(stderr,"error: file '%s' is empty\n",path.c_str());
			exit(1);
		}
		std::string buf(size,'\0');
		if(!file.read(&buf[0],size))
			fprintf(stderr,"error: failed to read file '%s'\n",path.c_str()),exit(1);
		return buf;
	}catch(const std::bad_alloc&){
		fprintf(stderr,"fatal: out of memory\n"),exit(1);
	}
}
static void help(const char* prog){
	fprintf(stderr,"Usage: %s <input.mio> [-o <output>] [-I <include_path>] [-D <macro>[=<value>]] [-S] [-c]\n",prog);
	fprintf(stderr,"  Mio compiler - compiles Mio source to native code\n");
	fprintf(stderr,"  -o <file>   specify output file (.ll/.s/.o/.exe or no extension)\n");
	fprintf(stderr,"  -S          emit assembly (.s) instead of executable\n");
	fprintf(stderr,"  -c          compile only, emit object file (.o) instead of executable\n");
	fprintf(stderr,"  -I <path>   add include path for .mio file resolution\n");
	fprintf(stderr,"  -D <macro>  define a macro (e.g. -D DEBUG or -D VERSION=2)\n");
}
int main(int argc,char* argv[]){
	try{
		if(argc<2){help(argv[0]);exit(0);}
		if(argc==2&&(strcmp(argv[1],"-v")==0||strcmp(argv[1],"--version")==0)){
			printf("mio version 2.1.2\nCopyright (c) 2026 mioLanguage\nMIT License\n");
			help(argv[0]);
			exit(0);
		}
		std::string input_file,output_file;
		std::vector<std::string> include_paths,defines;
		bool emit_asm=false,compile_only=false;
		for(int i=1;i<argc;i++){
			if(strcmp(argv[i],"-o")==0&&i+1<argc)output_file=argv[++i];
			else if(strcmp(argv[i],"-I")==0&&i+1<argc)include_paths.push_back(argv[++i]);
			else if(strcmp(argv[i],"-D")==0&&i+1<argc)defines.push_back(argv[++i]);
			else if(strcmp(argv[i],"-S")==0)emit_asm=true;
			else if(strcmp(argv[i],"-c")==0)compile_only=true;
			else if(input_file.empty())input_file=argv[i];
		}
		if(input_file.empty()){help(argv[0]);exit(1);}
		const char* last_sep=nullptr;
		for(const char* p=argv[0];*p;p++)
			if(*p=='/'||*p=='\\')last_sep=p;
		if(last_sep){
			int dir_len=static_cast<int>(last_sep-argv[0]);
			std::string default_include(argv[0],dir_len);
			include_paths.push_back(default_include+="/../include");
		}
		std::string source=read_file(input_file);
		Lexer lexer(source,input_file);
		Parser parser(&lexer,input_file,include_paths);
		for(const auto& m:defines)parser.add_macro(m,"1");
		AstNode* program=parser.parse();
		if(!program){
			fprintf(stderr,"error: parser returned null\n");
			exit(1);
		}
		if(parser.errorCount()>0){
			fprintf(stderr,"error: %d parse errors\n",parser.errorCount());
			delete program;
			exit(1);
		}
		std::string base_name=output_file;
		if(base_name.empty()){
			base_name=input_file;
			size_t dot=base_name.find_last_of('.');
			if(dot!=std::string::npos)base_name=base_name.substr(0,dot);
		}
		CodeGen cg(base_name);
		if(!cg.generate(program)){
			fprintf(stderr,"error: code generation failed\n");
			delete program;
			exit(1);
		}
		bool ok=true;
		if(CodeGen::isLLVMFile(output_file)){
			std::string ll_path=output_file.empty()?base_name+".ll":output_file;
			ok=cg.emitLLVM(ll_path);
			if(ok)fprintf(stdout,"Generated: %s\n",ll_path.c_str());
		}else if(emit_asm||CodeGen::isAssemblyFile(output_file)){
			std::string asm_path=output_file.empty()?base_name+".s":output_file;
			ok=cg.emitAssembly(asm_path);
			if(ok)fprintf(stdout,"Generated: %s\n",asm_path.c_str());
		}else if(compile_only||CodeGen::isObjectFile(output_file)){
			std::string obj_path=output_file.empty()?base_name+".o":output_file;
			ok=cg.emitObject(obj_path);
			if(ok)fprintf(stdout,"Generated: %s\n",obj_path.c_str());
		}else{
			std::string obj_path=base_name+".o";
			ok=cg.emitObject(obj_path);
			if(!ok){
				fprintf(stderr,"error: failed to emit object file\n");
				delete program;
				exit(1);
			}
			std::string exe_path=output_file.empty()?base_name+CodeGen::getExeExtension():output_file;
			ok=cg.linkExecutable(obj_path,exe_path);
			if(ok){
				fprintf(stdout,"Generated: %s\n",exe_path.c_str());
				std::remove(obj_path.c_str());
			}else{
				fprintf(stderr,"error: linking failed\n");
			}
		}
		delete program;
		if(!ok)exit(1);
	}catch(const std::exception& e){
		fprintf(stderr,"fatal error: %s\n",e.what());
		exit(1);
	}catch(...){
		fprintf(stderr,"fatal: unknown error\n");
		exit(1);
	}
	return 0;
}