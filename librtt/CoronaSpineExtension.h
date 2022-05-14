#pragma once
#include <spine/Extension.h>

#include <assert.h>
#include "Core/Rtt_Macros.h"

struct Rtt_Allocator;
class SP_API CoronaSpineExtension : public spine::SpineExtension
{
public:
	CoronaSpineExtension();

	virtual ~CoronaSpineExtension();

	
	void UpdateCoronaAlloctor(Rtt_Allocator* ac);
	
	Rtt_INLINE    Rtt_Allocator * GetAlloctor()
	{ 
		assert(_ac != NULL); return _ac; 
	}
protected:

	Rtt_Allocator  * _ac = NULL;

	virtual char *_readFile(const spine::String &path, int *length);

	virtual void *_alloc(size_t size, const char *file, int line);

	virtual void *_calloc(size_t size, const char *file, int line);

	virtual void *_realloc(void *ptr, size_t size, const char *file, int line);

	virtual void _free(void *mem, const char *file, int line);

	

};
