//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////


#ifndef _AndroidImageData_H__
#define _AndroidImageData_H__

#include "Core/Rtt_Build.h"
#include "Core/Rtt_Types.h"
#include "Display/Rtt_PlatformBitmap.h"


/// Stores an image's bytes and its information such as width, height, pixel format, etc.
/// <br>
/// This class is usd by the AndroidBitmap class to store image data.
class AndroidImageData
{
	public:
		AndroidImageData(Rtt_Allocator *allocatorPointer);
		virtual ~AndroidImageData();

		Rtt_Allocator* GetAllocator() const;
		U32 GetWidth() const;
		void SetWidth(U32 value);
		U32 GetHeight() const;
		void SetHeight(U32 value);

		Rtt_INLINE U32 GetOriginWidth() const{return fOriginWidth;}
		Rtt_INLINE void SetOriginWidth(U32 value){fOriginWidth = value ;};
		Rtt_INLINE U32 GetOriginHeight() const{return fOriginHeight;}
		Rtt_INLINE void SetOriginHeight(U32 value){fOriginHeight = value;}
		Rtt_INLINE void SetETC2Format(Rtt::PlatformBitmap::Format value){fETC2Format = value;}
		Rtt_INLINE Rtt::PlatformBitmap::Format GetETC2Format(){return fETC2Format;}
		Rtt_INLINE size_t GetETC2DataSize(){return fETC2DataSize ;}
		Rtt_INLINE void SetETC2DataSize(size_t size ){ fETC2DataSize = size ;}

		Rtt_Real GetScale() const;
		void SetScale(Rtt_Real value);
		Rtt::PlatformBitmap::Orientation GetOrientation() const;
		void SetOrientation(Rtt::PlatformBitmap::Orientation value);
		void SetOrientationInDegrees(int value);
		void SetPixelFormatToRGBA();
		void SetPixelFormatToGrayscale();
		bool IsPixelFormatRGBA() const;
		bool IsPixelFormatGrayscale() const;
		U32 GetPixelSizeInBytes() const;
		bool CreateImageByteBuffer();
		bool CreateImageByteBuffer(size_t size);
		void DestroyImageByteBuffer();
		U8* GetImageByteBuffer() const;

	private:
		Rtt_Allocator *fAllocatorPointer;
		U8 *fImageByteBufferPointer;
		U32 fWidth;
		U32 fHeight;
		U32 fOriginWidth;
		U32 fOriginHeight;
		Rtt::PlatformBitmap::Format fETC2Format;
		size_t  fETC2DataSize;
		Rtt_Real fScale;
		Rtt::PlatformBitmap::Orientation fOrientation;
		bool fIsPixelFormatGrayscale;
};

#endif // _AndroidImageData_H__
