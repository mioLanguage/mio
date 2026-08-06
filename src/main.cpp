#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<string>
#include<vector>
#include<sys/stat.h>

#ifdef _WIN32
#define mio_stat _stat64
#define mio_stat_t struct _stat64
#else
#define mio_stat stat
#define mio_stat_t struct stat
#endif
#include"compiler.hpp"
static void help(const char* prog){
	fprintf(stderr,"Usage: %s <input...> [-o <output>] [-I <include_path>] [-D <macro>[=<value>]] [-S] [-c] [-O<0|1|2|3>] [--release]\n",prog);
	fprintf(stderr,"  Mio compiler - compiles Mio source to native code\n");
	fprintf(stderr,"  Input files can be: .mio (source),.ll (LLVM IR),.s (assembly),.o (object)\n");
	fprintf(stderr,"  -o <file>   specify output file (.ll/.s/.o/.exe or no extension)\n");
	fprintf(stderr,"  -S          emit assembly (.s) instead of executable\n");
	fprintf(stderr,"  -c          compile only,emit object file (.o) instead of executable\n");
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
static bool file_exists(const std::string& path){
	mio_stat_t st;
	return mio_stat(path.c_str(),&st)==0;
}
static std::string getFileExtension(const std::string& path){
	size_t dot=path.find_last_of('.');
	return (dot!=std::string::npos)?path.substr(dot):"";
}
static bool isMioFile(const std::string& path){return getFileExtension(path)==".mio";}
static bool isLLVMFile(const std::string& path){return getFileExtension(path)==".ll";}
static bool isAssemblyFile(const std::string& path){return getFileExtension(path)==".s";}
static bool isObjectFile(const std::string& path){return getFileExtension(path)==".o";}
static bool isLibFile(const std::string& path){
#ifdef _WIN32
	return getFileExtension(path)==".lib";
#else
	return getFileExtension(path)==".a";
#endif
}
int main(int argc,char* argv[]){
	try{
		if(argc<2){help(argv[0]);exit(0);}
		if(argc==2&&(strcmp(argv[1],"-v")==0||strcmp(argv[1],"--version")==0))
			printf("mio version 2.1.2\nCopyright (c) 2026 mioLanguage\nMIT License\n"),
			help(argv[0]),
			exit(0);
		else if(argc==2&&(strcmp(argv[1],"-h")==0||strcmp(argv[1],"--help")==0))
			help(argv[0]),exit(0);
		std::string output_file;
		std::vector<std::string> input_files,include_paths,defines,link_libs;
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
			else input_files.push_back(argv[i]);
		}
		if(input_files.empty()){help(argv[0]);exit(1);}
		if(link_libs.empty()){
			link_libs.push_back("compiler_rt.builtins");
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
#ifdef _WIN32
			std::string lib_path=compiler_dir+"/lib/"+lib+".lib";
			if(file_exists(lib_path)){
				resolved_libs.push_back(lib_path);
			}else{
				lib_path=compiler_dir+"/../lib/"+lib+".lib";
				if(file_exists(lib_path)){
					resolved_libs.push_back(lib_path);
				}else{
					lib_path=compiler_dir+"/lib/windows/"+lib+".lib";
					if(file_exists(lib_path)){
						resolved_libs.push_back(lib_path);
					}else{
						lib_path=compiler_dir+"/../lib/windows/"+lib+".lib";
						if(file_exists(lib_path)){
							resolved_libs.push_back(lib_path);
						}
					}
				}
			}
#else
			std::string lib_path=compiler_dir+"/lib/lib"+lib+".a";
			if(file_exists(lib_path)){
				resolved_libs.push_back(lib_path);
			}else{
				lib_path=compiler_dir+"/../lib/lib"+lib+".a";
				if(file_exists(lib_path)){
					resolved_libs.push_back(lib_path);
				}
			}
#endif
		}
		std::string bundled_lib_path=compiler_dir+"/lib/windows";
		if(!file_exists(bundled_lib_path+"/.")){
			bundled_lib_path=compiler_dir+"/../lib/windows";
		}
		if(!compiler_dir.empty()){
			std::string inc=compiler_dir+"/include";
			if(file_exists(inc+"/std.mio")){
				include_paths.push_back(inc);
			}else{
				inc=compiler_dir+"/../include";
				if(file_exists(inc+"/std.mio")){
					include_paths.push_back(inc);
				}
			}
		}
		if(input_files.size()==1){
			const auto& input_file=input_files[0];
			std::string ext=getFileExtension(input_file);
			if(isMioFile(input_file)){
				Compiler cg;
				std::string output;
				if(!output_file.empty()){
					output=output_file;
				}else if(emit_asm){
					output=input_file.substr(0,input_file.size()-ext.size())+".s";
				}else if(compile_only){
					output=input_file.substr(0,input_file.size()-ext.size())+".o";
				}else{
					output=input_file.substr(0,input_file.size()-ext.size())+".exe";
				}
				bool ok=cg.compiling(input_file,output,include_paths,defines,resolved_libs,bundled_lib_path,emit_asm,compile_only,static_link,release,opt_level);
				if(!ok)exit(1);
			}else if(isObjectFile(input_file)||isLLVMFile(input_file)||isAssemblyFile(input_file)||isLibFile(input_file)){
				std::string exe_path=output_file.empty()?(input_file.substr(0,input_file.size()-ext.size())+".exe"):output_file;
				Compiler cg;
				std::vector<std::string> files={input_file};
				bool ok=cg.linkExecutableFiles(files,exe_path,static_link,resolved_libs,bundled_lib_path);
				if(!ok)exit(1);
			}else{
				fprintf(stderr,"error: unknown file type '%s'\n",input_file.c_str());
				exit(1);
			}
		}else{
			std::vector<std::string> link_files;
			for(const auto& input_file:input_files){
				std::string ext=getFileExtension(input_file);
				if(isMioFile(input_file)){
					Compiler cg;
					std::string obj_path=input_file.substr(0,input_file.size()-ext.size())+".o";
					bool ok=cg.compiling(input_file,obj_path,include_paths,defines,resolved_libs,bundled_lib_path,false,true,static_link,release,opt_level);
					if(!ok)exit(1);
					link_files.push_back(obj_path);
				}else if(isObjectFile(input_file)||isLLVMFile(input_file)||isAssemblyFile(input_file)||isLibFile(input_file)){
					link_files.push_back(input_file);
				}else{
					fprintf(stderr,"error: unknown file type '%s'\n",input_file.c_str());
					exit(1);
				}
			}
			std::string exe_path=output_file.empty()?"a"+std::string(".exe"):output_file;
			Compiler cg;
			bool ok=cg.linkExecutableFiles(link_files,exe_path,static_link,resolved_libs,bundled_lib_path);
			if(!ok)exit(1);
			fprintf(stdout,"Generated: %s\n",exe_path.c_str());
		}
	}catch(const std::exception& e){
		fprintf(stderr,"fatal error: %s\n",e.what());
		exit(1);
	}catch(...){
		fprintf(stderr,"fatal: unknown error (exception not derived from std::exception)\n");
		exit(1);
	}
	return 0;
}
