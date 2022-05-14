
	
#include "CoronaSpineExtension.h"
#include <spine/SpineString.h>
#include "Core/Rtt_Allocator.h"
#include "Core/Rtt_FileSystem.h"


using namespace spine;
SpineExtension *SpineExtension::_instance = NULL;


Rtt_INLINE   static void coronaFree(void * mem)
{
	Rtt_FREE(mem);
}

Rtt_INLINE static  void* coronaRealloc(Rtt_Allocator * ac, void * ptr, size_t size)
{
	return Rtt_REALLOC(ac, ptr, size);
}


SpineExtension * spine::getDefaultExtension()
{
	return new CoronaSpineExtension();
}

void SpineExtension::setInstance(SpineExtension *inValue) {
	assert(inValue);

	_instance = inValue;
}

SpineExtension *SpineExtension::getInstance() {
	if (!_instance) _instance = spine::getDefaultExtension();
	assert(_instance);
	return _instance;
}

SpineExtension::~SpineExtension() {}
SpineExtension::SpineExtension() {}
CoronaSpineExtension::~CoronaSpineExtension() {}

void CoronaSpineExtension::UpdateCoronaAlloctor(Rtt_Allocator* ac)
{
		
	assert(ac != NULL);
		
	_ac = ac;
}

CoronaSpineExtension::CoronaSpineExtension() : SpineExtension() {
	
}


#define CoronaMalloc(size)  Rtt_MALLOC(GetAlloctor(),(size))

void *CoronaSpineExtension::_alloc(size_t size, const char *file, int line)
{
	SP_UNUSED(file);
	SP_UNUSED(line);

	if (size == 0)
		return 0;
	
	void *ptr = CoronaMalloc(size);
	return ptr;

}

void *CoronaSpineExtension::_calloc(size_t size, const char *file, int line) {
	SP_UNUSED(file);
	SP_UNUSED(line);

	if (size == 0)
		return 0;

	
	void *ptr = CoronaMalloc(size);
	if (ptr)
	{
		memset(ptr, 0, size);
	}
	return ptr;
}


void *CoronaSpineExtension::_realloc(void *ptr, size_t size, const char *file, int line)
{
	SP_UNUSED(file);
	SP_UNUSED(line);

	void *mem = NULL;
	if (size == 0)
		return 0;
	if (ptr == NULL)
	{
	
		mem = CoronaMalloc(size);
	}
	else
	{
		
		mem = coronaRealloc(GetAlloctor(), ptr, size);
	}

	return mem;
}


void CoronaSpineExtension::_free(void *mem, const char *file, int line) {
	SP_UNUSED(file);
	SP_UNUSED(line);
	
	coronaFree(mem);
}

char *CoronaSpineExtension::_readFile(const String &path, int *length) {
	
		
	char * data = NULL;
	auto * file = Rtt_FileOpen(path.buffer(), "rb");
	if (!file)return 0;

	Rtt_FileSeek(file, 0, SEEK_END);
	*length = (int)Rtt_FileTell(file);
	Rtt_FileSeek(file, 0, SEEK_SET);
	data = SpineExtension::alloc<char>(*length, __FILE__, __LINE__);
	Rtt_FileRead(data, 1, *length, file);
	Rtt_FileClose(file);

	return data;
}


