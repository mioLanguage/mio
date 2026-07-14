#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace llvm {
class PassBuilder;
struct PassPluginLibraryInfo {
	uint32_t APIVersion;
	const char *PluginName;
	const char *PluginVersion;
	void (*RegisterPassBuilderCallbacks)(PassBuilder &);
};
} // namespace llvm

extern "C" {

typedef struct _xmlNode{void*_private;int type;unsigned char*name;struct _xmlNode*children;struct _xmlNode*last;struct _xmlNode*parent;struct _xmlNode*next;struct _xmlNode*prev;struct _xmlDoc*doc;void*ns;unsigned char*content;void*properties;void*nsDef;void*psvi;unsigned short line;unsigned short extra;}xmlNode;
typedef xmlNode xmlNs;
typedef struct _xmlDoc{void*_private;int type;char*name;struct _xmlNode*children;struct _xmlNode*last;struct _xmlNode*parent;struct _xmlNode*next;struct _xmlNode*prev;struct _xmlDoc*doc;int compression;int standalone;void*intSubset;void*extSubset;void*oldNs;const unsigned char*version;const unsigned char*encoding;void*ids;void*refs;const unsigned char*URL;int charset;void*dict;void*psvi;int parseFlags;int properties;void*closeFunc;void*contentType;}xmlDoc;

xmlDoc*xmlReadMemory(const char*b,int s,const char*u,const char*e,int o){return 0;}
void xmlFreeDoc(xmlDoc*d){}
void xmlFreeNode(xmlNode*n){}
void*xmlNewProp(xmlNode*n,const unsigned char*a,const unsigned char*b){return 0;}
unsigned char*xmlStrdup(const unsigned char*s){return 0;}
void*xmlCopyNamespace(void*n){return 0;}
void xmlFree(void*p){}
xmlNode*xmlAddChild(xmlNode*p,xmlNode*n){return n;}
xmlDoc*xmlNewDoc(const unsigned char*v){return 0;}
xmlNode*xmlDocSetRootElement(xmlDoc*d,xmlNode*r){return r;}
void xmlDocDumpFormatMemoryEnc(xmlDoc*d,unsigned char**m,int*s,const char*e,int f){*m=0;*s=0;}
void xmlFreeNs(xmlNs*n){}
xmlNs*xmlNewNs(xmlNode*n,const unsigned char*h,const unsigned char*p){return 0;}
void xmlUnlinkNode(xmlNode*n){}
void xmlSetGenericErrorFunc(void*c,void*f){}
xmlNode*xmlDocGetRootElement(const xmlDoc*d){return 0;}

llvm::PassPluginLibraryInfo __attribute__((used)) getPollyPluginInfo() {
	return {1, "Polly", "0.0", [](llvm::PassBuilder &) {}};
}

} // extern "C"