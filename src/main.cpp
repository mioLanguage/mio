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
#include<sys/stat.h>

#ifdef _WIN32
#define mio_stat _stat64
#define mio_stat_t struct _stat64
#else
#define mio_stat stat
#define mio_stat_t struct stat
#endif
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
static uint64_t file_mtime(const std::string& path){
	mio_stat_t st;
	if(mio_stat(path.c_str(),&st)!=0)return 0;
	return (uint64_t)st.st_mtime;
}
static bool file_exists(const std::string& path){
	mio_stat_t st;
	return mio_stat(path.c_str(),&st)==0;
}
static void help(const char* prog){
	fprintf(stderr,"Usage: %s <input.mio> [-o <output>] [-I <include_path>] [-D <macro>[=<value>]] [-S] [-c] [-O<0|1|2|3>] [--release]\n",prog);
	fprintf(stderr,"  Mio compiler - compiles Mio source to native code\n");
	fprintf(stderr,"  -o <file>   specify output file (.ll/.s/.o/.exe or no extension)\n");
	fprintf(stderr,"  -S          emit assembly (.s) instead of executable\n");
	fprintf(stderr,"  -c          compile only, emit object file (.o) instead of executable\n");
	fprintf(stderr,"  -I <path>   add include path for .mio file resolution\n");
	fprintf(stderr,"  -D <macro>  define a macro (e.g. -D DEBUG or -D VERSION=2)\n");
	fprintf(stderr,"  -O0         no optimization\n");
	fprintf(stderr,"  -O1         basic optimization\n");
	fprintf(stderr,"  -O2         standard optimization (default for --release)\n");
	fprintf(stderr,"  -O3         aggressive optimization\n");
	fprintf(stderr,"  --release   release mode (enables -O2 and caching)\n");
	fprintf(stderr,"  -l <lib>    link with library (e.g. -lstdmio -lm). default: -lstdmio -lm\n");
	fprintf(stderr,"  -static     link statically (no DLL dependencies)\n");
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
		std::vector<std::string> include_paths,defines,link_libs;
		bool emit_asm=false,compile_only=false,static_link=false,release=false;
		int opt_level=0;
		for(int i=1;i<argc;i++){
			if(strcmp(argv[i],"-o")==0&&i+1<argc)output_file=argv[++i];
			else if(strcmp(argv[i],"-I")==0&&i+1<argc)include_paths.push_back(argv[++i]);
			else if(strcmp(argv[i],"-D")==0&&i+1<argc)defines.push_back(argv[++i]);
			else if(strcmp(argv[i],"-S")==0)emit_asm=true;
			else if(strcmp(argv[i],"-c")==0)compile_only=true;
			else if(strcmp(argv[i],"-static")==0)static_link=true;
			else if(strcmp(argv[i],"-l")==0&&i+1<argc)link_libs.push_back(argv[++i]);
			else if(strncmp(argv[i],"-l",2)==0&&strlen(argv[i])>2)link_libs.push_back(argv[i]+2);
			else if(strcmp(argv[i],"--release")==0){release=true;opt_level=2;}
			else if(strncmp(argv[i],"-O",2)==0&&strlen(argv[i])==3&&argv[i][2]>='0'&&argv[i][2]<='3')opt_level=argv[i][2]-'0';
			else if(input_file.empty())input_file=argv[i];
		}
		if(input_file.empty()){help(argv[0]);exit(1);}
		if(link_libs.empty()){
			link_libs.push_back("stdmio");
			link_libs.push_back("m");
		}
		std::string compiler_dir;
		{
			const char* ls=nullptr;
			for(const char* p=argv[0];*p;p++)
				if(*p=='/'||*p=='\\')ls=p;
			if(ls)compiler_dir=std::string(argv[0],ls-argv[0]);
		}
		std::vector<std::string> resolved_libs;
		for(const auto& lib:link_libs){
			std::string lib_path=compiler_dir+"/lib/lib"+lib+".a";
			if(file_exists(lib_path)){
				resolved_libs.push_back(lib_path);
			}else{
				lib_path=compiler_dir+"/../lib/lib"+lib+".a";
				if(file_exists(lib_path)){
					resolved_libs.push_back(lib_path);
				}
			}
		}
		std::string bundled_lib_path=compiler_dir+"/lib/windows";
		if(!file_exists(bundled_lib_path+"/.")){
			bundled_lib_path=compiler_dir+"/../lib/windows";
		}
		if(!compiler_dir.empty()){
			std::string inc=compiler_dir+"/include";
			if(file_exists(inc+"/stdio.mio")){
				include_paths.push_back(inc);
			}else{
				inc=compiler_dir+"/../include";
				if(file_exists(inc+"/stdio.mio")){
					include_paths.push_back(inc);
				}
			}
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
		CodeGen cg(base_name,input_file,opt_level);
		if(!cg.generate(program)){
			fprintf(stderr,"error: code generation failed\n");
			delete program;
			exit(1);
		}
		bool ok=true;
		bool useCache=false;
		if(release){
			std::string cache_obj_path=base_name+".o";
			if(file_exists(cache_obj_path)){
				uint64_t src_mtime=file_mtime(input_file);
				uint64_t obj_mtime=file_mtime(cache_obj_path);
				if(obj_mtime>=src_mtime){
					useCache=true;
					fprintf(stdout,"[cache] using cached object file '%s'\n",cache_obj_path.c_str());
				}
			}
		}
		if(useCache){
			std::string obj_path=base_name+".o";
			if(emit_asm||compile_only||CodeGen::isLLVMFile(output_file)||CodeGen::isAssemblyFile(output_file)||CodeGen::isObjectFile(output_file)){
				ok=true;
			}else{
				std::string exe_path=output_file.empty()?base_name+CodeGen::getExeExtension():output_file;
				ok=cg.linkExecutable(obj_path,exe_path,static_link,resolved_libs,bundled_lib_path);
				if(ok)fprintf(stdout,"Generated: %s\n",exe_path.c_str());
				else fprintf(stderr,"error: linking failed\n");
			}
		}else if(CodeGen::isLLVMFile(output_file)){
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
				fprintf(stderr,"error: failed to emit object file '%s'\n",obj_path.c_str());
				delete program;
				exit(1);
			}
			std::string exe_path=output_file.empty()?base_name+CodeGen::getExeExtension():output_file;
			ok=cg.linkExecutable(obj_path,exe_path,static_link,resolved_libs,bundled_lib_path);
			if(ok){
				fprintf(stdout,"Generated: %s\n",exe_path.c_str());
				std::remove(obj_path.c_str());
			}else{
				fprintf(stderr,"error: linking failed for '%s'\n",exe_path.c_str());
			}
		}
		delete program;
		if(!ok)exit(1);
	}catch(const std::exception& e){
		fprintf(stderr,"fatal error: %s\n",e.what());
		exit(1);
	}catch(...){
		fprintf(stderr,"fatal: unknown error (exception not derived from std::exception)\n");
		exit(1);
	}
	return 0;
}