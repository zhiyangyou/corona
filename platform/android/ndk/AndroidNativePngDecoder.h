//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////


#ifndef _AndroidNativePngDecoder_H__
#define _AndroidNativePngDecoder_H__

#include "AndroidBaseNativeImageDecoder.h"
#include "AndroidOperationResult.h"

// Forward declarations.
struct Rtt_Allocator;
class AndroidBaseImageDecoder;
class AndroidBinaryReader;
class NativeToJavaBridge;


/// Decodes an image via the "libpng" C library.
/// <br>
/// Note that this image decoder loads PNGs faster than the AndroidJavaImageDecoder class.
class AndroidNativePngDecoder : public AndroidBaseNativeImageDecoder
{
	public:
		AndroidNativePngDecoder(Rtt_Allocator *allocatorPointer, NativeToJavaBridge *ntjb);
		AndroidNativePngDecoder(const AndroidBaseImageDecoder &decoder, NativeToJavaBridge *ntjb);
		virtual ~AndroidNativePngDecoder();

	protected:
		AndroidOperationResult OnDecodeFrom(AndroidBinaryReader &reader);
};

//增加1个头文件麻烦,得改4个构建文件,所以偷懒,直接就写在这里吧... zhiyangyou
class AndroidNativeETC2Decoder:public  AndroidBaseNativeImageDecoder
{
public:
	AndroidNativeETC2Decoder(Rtt_Allocator *allocatorPointer, NativeToJavaBridge *ntjb)
			:	AndroidBaseNativeImageDecoder(allocatorPointer, ntjb){}
	AndroidNativeETC2Decoder(const AndroidBaseImageDecoder &decoder, NativeToJavaBridge *ntjb)
			: AndroidBaseNativeImageDecoder(decoder,ntjb){}
	virtual ~AndroidNativeETC2Decoder(){}
protected:
	AndroidOperationResult OnDecodeFrom(AndroidBinaryReader &reader);
};


#endif // _AndroidNativePngDecoder_H__
