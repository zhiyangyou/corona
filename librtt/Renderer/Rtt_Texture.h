//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_Texture_H__
#define _Rtt_Texture_H__

#include "Renderer/Rtt_CPUResource.h"
#include "Core/Rtt_Types.h"
#include <stdlib.h>

// ----------------------------------------------------------------------------

namespace Rtt
{

// ----------------------------------------------------------------------------

class Texture : public CPUResource
{
	public:
		typedef CPUResource Super;
		typedef Texture Self;

		typedef enum _Format
		{
			kAlpha,
			kLuminance,
			kRGB,
			kRGBA,
			kBGRA,
			kABGR,
			kARGB,
			kLuminanceAlpha,

			//ETC1定义的
			kETC1_RGB_NO_MIPMAPS ,
			//ETC2 定义的
			kETC2PACKAGE_RGB_NO_MIPMAPS ,
			kETC2PACKAGE_RGBA_NO_MIPMAPS_OLD ,
			kETC2PACKAGE_RGBA_NO_MIPMAPS ,
			kETC2PACKAGE_RGBA1_NO_MIPMAPS ,
			kETC2PACKAGE_R_NO_MIPMAPS ,
			kETC2PACKAGE_RG_NO_MIPMAPS ,
			kETC2PACKAGE_R_SIGNED_NO_MIPMAPS ,
			kETC2PACKAGE_RG_SIGNED_NO_MIPMAPS ,

			kNumFormats
		}
		Format;

		typedef enum _Filter
		{
			kNearest,
			kLinear,
			kNumFilters
		}
		Filter;

		typedef enum _Wrap
		{
			kClampToEdge,
			kRepeat,
			kMirroredRepeat,

			kNumWraps
		}
		Wrap;

		typedef enum _Unit
		{
			kFill0,
			kFill1,
			kMask0,
			kMask1,
			kMask2,
			kNumUnits
		}
		Unit;

	public:

		Texture( Rtt_Allocator* allocator ,bool isCompressTexture=false);
		virtual ~Texture();

		virtual ResourceType GetType() const;
		virtual void Allocate();
		virtual void Deallocate();
		virtual bool IsCompressedTexture () ;
		virtual U32 GetWidth() const = 0;
		virtual U32 GetHeight() const = 0;
		virtual Format GetFormat() const = 0;
		virtual Filter GetFilter() const = 0;
		virtual Wrap GetWrapX() const;
		virtual Wrap GetWrapY() const;
		virtual size_t GetSizeInBytes() const;
		virtual size_t GetCompressTextureSize() const{return fCompressTextureSize;}
		virtual void SetCompressTextureSize(size_t newSize)const{ fCompressTextureSize = newSize;}
		virtual U8 GetByteAlignment() const;

		virtual const U8* GetData() const;
		virtual void ReleaseData();

		virtual void SetFilter( Filter newValue );
		virtual void SetWrapX( Wrap newValue );
		virtual void SetWrapY( Wrap newValue );
	
	public:
		void SetRetina( bool newValue ){ fIsRetina = newValue; }
		bool IsRetina(){ return fIsRetina; }

	private:
		bool fIsRetina;
		bool fIsCompressTexture;
		mutable size_t fCompressTextureSize;
};

// ----------------------------------------------------------------------------

} // namespace Rtt

// ----------------------------------------------------------------------------

#endif // _Rtt_Texture_H__
