//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#include "Core/Rtt_Build.h"

#include "Rtt_TextObject.h"

#include "Core/Rtt_String.h"
#include "Display/Rtt_BitmapMask.h"
#include "Display/Rtt_BitmapPaint.h"
#include "Display/Rtt_Display.h"
#include "Display/Rtt_DisplayDefaults.h"
#include "Display/Rtt_RectPath.h"
#include "Display/Rtt_Shader.h"
#include "Renderer/Rtt_Geometry_Renderer.h"
#include "Renderer/Rtt_Uniform.h"
#include "Rtt_GroupObject.h"
#include "Rtt_LuaProxyVTable.h"
#include "Display/Rtt_PlatformBitmap.h"
#include "Rtt_PlatformFont.h"
#include "Rtt_Runtime.h"

#include "Rtt_Lua.h"
#include "CoronaLua.h"

#ifdef Rtt_WIN_PHONE_ENV
#	include <vector>
#endif
#include "Renderer/Rtt_RenderTypes.h"

#ifdef Rtt_WIN_ENV
#	undef CreateFont
#endif

// [Experimental Feature]
// If the below is defined, text objects will be offsetted and rendered to the nearest screen pixel.
// This prevents the text from looking blurry/fuzzy if the text bitmap lands between screen pixels,
// causing the GPU to interpolate the text's bitmap pixels between screen pixels.
// Notes:
// - This issue is only noticeable on screens with a low DPI, which is typically the case on desktop platforms.
// - This feature won't prevent scaled or rotated text objects from looking blurry.
// - Currently only defined on Win32 to minimize any unforseen negative impacts this change might cause.
// - Theoretically, GL_NEAREST should do this for us, but it doesn't work with Corona's masks. (Why?)
#if defined(Rtt_WIN_DESKTOP_ENV)
#	define Rtt_RENDER_TEXT_TO_NEAREST_PIXEL
#endif

// ----------------------------------------------------------------------------

namespace Rtt
{

	static  Rtt::BitmapPaint* TextMaskPaintGetter(Runtime& runtime, const char text[], const PlatformFont& font, Real w, Real h, const char alignment[], Real& baselineOffset, Real outlineWidth, RGBA outlineColor)
	{
		Real offset;
		Rtt::BitmapPaint* ret = BitmapPaint::NewBitmap(runtime, text, font, w, h, alignment, offset);//这个paint会随着
		baselineOffset = offset;
		return ret;
	}

	Rtt::BitmapPaint* TextObject::DefaultPaintGetter(Runtime& runtime, const char text[], const PlatformFont& font, Real w, Real h, const char alignment[], Real& baselineOffset)
	{
		Real offset;
		Rtt::BitmapPaint* ret = TextMaskPaintGetter(runtime, text, font, w, h, alignment, offset, 0, { 0 });
		baselineOffset = offset;
		return ret;
	}


	// ----------------------------------------------------------------------------

	void
		TextObject::Unload(DisplayObject& parent)
	{
		// NOTE: Assumes no subclasses of TextObject exist
		if (&parent.ProxyVTable() == &LuaTextObjectProxyVTable::Constant())
		{
			TextObject& t = (TextObject&)parent;
			t.Reset();
		}

		// Tail recursion
		GroupObject* group = parent.AsGroupObject();
		if (group)
		{
			for (S32 i = 0, iMax = group->NumChildren(); i < iMax; i++)
			{
				Unload(group->ChildAt(i));
			}
		}
	}

	void
		TextObject::Reload(DisplayObject& parent)
	{
		// NOTE: Assumes no subclasses of TextObject exist
		if (&parent.ProxyVTable() == &LuaTextObjectProxyVTable::Constant())
		{
			TextObject& t = (TextObject&)parent;
			t.Initialize();
		}

		// Tail recursion
		GroupObject* group = parent.AsGroupObject();
		if (group)
		{
			for (S32 i = 0, iMax = group->NumChildren(); i < iMax; i++)
			{
				Reload(group->ChildAt(i));
			}
		}
	}

	// ------------------------------------------------------------------------------------------------------------------------
	// The following code only applies to Windows Phone 8.0.
	// It is needed because Windows Phone 8.0 can only render text to bitmap while Corona is not synchonrized/blocking
	// the Direct3D thread or else deadlock will occur. So, Corona must create the text bitmap before rendering.
	// ------------------------------------------------------------------------------------------------------------------------
#ifdef Rtt_WIN_PHONE_ENV
	typedef std::vector<TextObject*> TextObjectCollection;

	/// Gets a collection used to store all TextObjects that exist for all runtime instances.
	/// A TextObject constructor is expected to add an instance of itself to this collection and remove itself upon destruction.
	static TextObjectCollection& GetCollection()
	{
		static TextObjectCollection sCollection;
		return sCollection;
	}

	bool
		TextObject::IsUpdateNeededFor(Display& display)
	{
		TextObjectCollection& collection = GetCollection();
		TextObjectCollection::iterator iter;
		TextObject* textObjectPointer;

		for (iter = collection.begin(); iter != collection.end(); iter++)
		{
			textObjectPointer = *iter;
			if (textObjectPointer && (&display == &(textObjectPointer->fDisplay)))
			{
				if (textObjectPointer->IsInitialized() == false)
				{
					return true;
				}
			}
		}
		return false;
	}

	void
		TextObject::UpdateAllBelongingTo(Display& display)
	{
		TextObjectCollection& collection = GetCollection();
		TextObjectCollection::iterator iter;
		TextObject* textObjectPointer;

		for (iter = collection.begin(); iter != collection.end(); iter++)
		{
			textObjectPointer = *iter;
			if (textObjectPointer && (&display == &(textObjectPointer->fDisplay)))
			{
				if (textObjectPointer->IsInitialized() == false)
				{
					textObjectPointer->Initialize();
				}
			}
		}
	}

#endif

	// w,h are in content coordinates
	// font's size is in pixel coordinates
	TextObject::TextObject(Display& display, const char text[], PlatformFont* font, Real w, Real h, const char alignment[], PaintGetter getter)
		: Super(RectPath::NewRect(display.GetRuntime().GetAllocator(), w, h)),
		fDisplay(display),
		fText(display.GetRuntime().GetAllocator()),
		fOriginalFont(font),
		fScaledFont(NULL),
		fWidth(w),
		fHeight(h),
		fAlignment(display.GetRuntime().GetAllocator()),
		fGeometry(NULL),
		fBaselineOffset(Rtt_REAL_0),
		fPaintGetter(getter),
		fMaskUniform(Rtt_NEW(display.GetAllocator(), Uniform(display.GetAllocator(), Uniform::kMat3)))
	{
		if (!fOriginalFont)
		{
			const MPlatform& platform = display.GetRuntime().Platform();
			Real standardFontSize = platform.GetStandardFontSize() * display.GetSxUpright();
			fOriginalFont = platform.CreateFont(PlatformFont::kSystemFont, standardFontSize);
		}
		if (fPaintGetter == NULL)
		{
			fPaintGetter = DefaultPaintGetter;
		}
		SetText(text);
		SetAlignment(alignment);

		Invalidate(kMaskFlag);
		Initialize();
		SetHitTestMasked(false);

#ifdef Rtt_WIN_PHONE_ENV
		GetCollection().push_back(this);
#endif

		SetObjectDesc("TextObject");
	}




	TextObject::~TextObject()
	{
		Rtt_DELETE(fOriginalFont);
		Rtt_DELETE(fScaledFont);

		QueueRelease(fMaskUniform);
		QueueRelease(fGeometry);

#ifdef Rtt_WIN_PHONE_ENV
		TextObjectCollection& collection = GetCollection();
		TextObjectCollection::iterator iter;
		for (iter = collection.begin(); iter != collection.end(); iter++)
		{
			if (this == *iter)
			{
				collection.erase(iter);
				break;
			}
		}
#endif
	}

	bool
		TextObject::Initialize()
	{
		Rtt_ASSERT(fOriginalFont);

		// Fetch the content scales.
		Real sx = fDisplay.GetSxUpright();
		Real sy = fDisplay.GetSyUpright();
		bool shouldScale = !Rtt_RealIsOne(sx) || !Rtt_RealIsOne(sy);

		// If the rendering system has been scaled, then use a font with a scaled font size.
		UpdateScaledFont();
		PlatformFont* font = fScaledFont ? fScaledFont : fOriginalFont;

		// For text boxes (multiline), use pixel dimensions
		Real pixelW = fWidth;
		Real pixelH = fHeight;

		bool isTextBox = (pixelW > Rtt_REAL_0);
		if (shouldScale && isTextBox)
		{
			pixelW = Rtt_RealDiv(pixelW, sx);
			pixelH = Rtt_RealDiv(pixelH, sy);
			//outlineW = Rtt_RealDiv(outlineW, sy );
		}

		// Get the text to be rendered.
		// Note: If no text was assigned, then use a string with just a space so that we can generate
		//       a bitmap at about the same height as a bitmap with text. This is especially needed
		//       when this text object is initially created or else its center reference point will
		//       be at the same position as the top-left reference point, which might not be expected.
		const char* text = " ";
		if (fText.IsEmpty() == false)
		{
			text = fText.GetString();
		}

		// TODO: We are handling two cases here. Can we separate more cleanly?
		// We need to request an appropriate sized text bitmap (with proper pixel resolution)
		// (1) Single-line: we already scaled the font size (fWidth/fHeight should be 0)
		// (2) Multi-line: fWidth/fHeight define the box and needs to be scaled


		BitmapPaint* paint = fPaintGetter(fDisplay.GetRuntime(), text, *font, pixelW, pixelH, fAlignment.GetString(), fBaselineOffset);//这个paint会随着底下的BitmapMask对象析构而析构
		PlatformBitmap* bitmap = paint->GetBitmap(); Rtt_ASSERT(bitmap);

		Real contentW = Rtt_IntToReal(bitmap->Width());
		Real contentH = Rtt_IntToReal(bitmap->Height());

		// bitmap returns dimensions in pixels, so convert to content coords
		if (shouldScale)
		{
			contentW = Rtt_RealMul(contentW, sx);
			contentH = Rtt_RealMul(contentH, sy);
			fBaselineOffset = Rtt_RealMul(fBaselineOffset, sy);
		}

		if (isTextBox)
		{
			// Apple (Mac/iOS) weirdness. Need to round up to nearest multiple of 4.
			Rtt_ASSERT(contentW <= (fWidth + 4));
			Rtt_ASSERT(Rtt_RealIsZero(fHeight) || contentH <= (fHeight + 4));
		}

		BitmapMask* mask =
			Rtt_NEW(fDisplay.GetRuntime().GetAllocator(), BitmapMask(paint, contentW, contentH));
		SetMask(fDisplay.GetRuntime().GetAllocator(), mask);
		SetSelfBounds(contentW, contentH);
		return (NULL != mask);
	}

	/// Updates member variable "fScaledFont" with a scaled font size based on "fOriginalFont".
	/// Should be called every time the rendering system's scale factor has changed.
	/// Member variable "fScaledFont" will be set to NULL if the scale factor is 1.0.
	void
		TextObject::UpdateScaledFont()
	{
		// Fetch the content scaling factor from the rendering system.
		// Note: There is a bug in the Corona Simulator where the scales are wrongly swapped when rotating
		//       to an orientation that the app does not support. The below code works-around this issue.
		Real scale = fDisplay.GetSxUpright();

		// Scale the font's point size.
		Real fontSizeEpsilon = Rtt_FloatToReal(0.1f);
		Real scaledFontSize = Rtt_RealDiv(fOriginalFont->Size(), scale);

		// Create a scaled font, if necessary.
		if ((scaledFontSize >= (fOriginalFont->Size() + fontSizeEpsilon)) ||
			(scaledFontSize <= (fOriginalFont->Size() - fontSizeEpsilon)))
		{
			// Font needs to be scaled. Create a new scaled font, if not already made.
			if (fScaledFont)
			{
				if ((scaledFontSize >= (fScaledFont->Size() + fontSizeEpsilon)) ||
					(scaledFontSize <= (fScaledFont->Size() - fontSizeEpsilon)))
				{
					Reset();
				}
			}
			if (!fScaledFont)
			{
				Reset();
				fScaledFont = fOriginalFont->CloneUsing(fDisplay.GetRuntime().GetAllocator());
				if (fScaledFont)
				{
					fScaledFont->SetSize(scaledFontSize);
				}
			}
		}
		else if (fScaledFont)
		{
			// Font does not need to be scaled. Delete the old scaled font.
			Reset();
		}
	}

	void
		TextObject::Reset()
	{
		SetMask(NULL, NULL);

		Rtt_DELETE(fScaledFont);
		fScaledFont = NULL;

		Invalidate(kGeometryFlag | kStageBoundsFlag | kMaskFlag);
		GetPath().Invalidate(ClosedPath::kFillSource | ClosedPath::kStrokeSource);
	}

	bool
		TextObject::UpdateTransform(const Matrix& parentToDstSpace)
	{
		bool result = false;

		// First, attempt to scale the font, if necessary.
		// If the font does not need to be scaled, then this function will flag this object to be re-initialized.
		UpdateScaledFont();

		// Update the text object.
		if (IsInitialized() || Rtt_VERIFY(Initialize()))
		{
			// Update this object's transformation matrix.
			result = Super::UpdateTransform(parentToDstSpace);
		}

		return result;
	}

	void
		TextObject::GetSelfBounds(Rect& rect) const
	{
		if (!IsInitialized())
		{
			const_cast<TextObject*>(this)->Initialize();
		}

		if (Rtt_VERIFY(IsInitialized()))
		{
			// NOTE: we do not use fWidth/fHeight b/c that's only used for multiline text
			// The final bounds might be slightly larger for byte alignment/pixel row stride issues
			Super::GetSelfBounds(rect);
		}
		else
		{
			rect.SetEmpty();
		}
	}

	void
		TextObject::Prepare(const Display& display)
	{
#ifdef Rtt_RENDER_TEXT_TO_NEAREST_PIXEL
		Real offsetX = Rtt_REAL_0;
		Real offsetY = Rtt_REAL_0;

		// The base display object must be prepared first.
		// This generates/updates its text bitmap and updates its transformation matrix.
		Super::Prepare(display);
		if (IsInitialized() == false)
		{
			return;
		}

		// Fetch this text object's geometry/vertices based on the bitmap generated above.
		const Geometry* geometryPointer = GetFillData().fGeometry;
		if (!geometryPointer)
		{
			return;
		}

		// Calculate offsets so that this object's bitmap pixels line-up with the closest screen pixels.
		// This prevents the bitmap from appearing blurry/fuzzy onscreen. (Avoids interpolation between screen pixels.)
		Rect bounds;
		ClosedPath& path = GetPath();
		path.GetSelfBounds(bounds);
		Matrix translationMatrix(GetSrcToDstMatrix());
		{
			offsetX = translationMatrix.Tx() + Rtt_RealMul(bounds.Width(), GetAnchorX() - Rtt_REAL_HALF);
			offsetX -= Rtt_RealMul(bounds.Width(), GetAnchorX());
			offsetX += display.GetXOriginOffset();
			offsetX = Rtt_RealDiv(offsetX, fDisplay.GetSx());                           // Convert to pixels.
			offsetX = Rtt_FloatToReal(floorf(Rtt_RealToFloat(offsetX))) - offsetX;  // Round-down to nearest pixel.
			offsetX = Rtt_RealMul(offsetX, fDisplay.GetSx());                           // Convert to content coordinates.
		}
		{
			offsetY = translationMatrix.Ty() + Rtt_RealMul(bounds.Height(), GetAnchorY() - Rtt_REAL_HALF);
			offsetY -= Rtt_RealMul(bounds.Height(), GetAnchorY());
			offsetY += display.GetYOriginOffset();
			offsetY = Rtt_RealDiv(offsetY, fDisplay.GetSy());                           // Convert to pixels.
			offsetY = Rtt_FloatToReal(floorf(Rtt_RealToFloat(offsetY))) - offsetY;  // Round-down to nearest pixel.
			offsetY = Rtt_RealMul(offsetY, fDisplay.GetSy());                           // Convert to content coordinates.
		}

		// Apply the above offsets to this object's temporary geometry and mask uniform, to be used by Draw() method later.
		// Note: Don't apply to base class' geometry. We do not want this object's (x,y) properties to change in Lua.
		{
			// Offset uniform to nearest screen pixel.
			translationMatrix.Translate(offsetX, offsetY);
			UpdateMaskUniform(*fMaskUniform, translationMatrix, *GetMask());
		}
		{
			// Offset geometry to nearest screen pixel.
			//TODO: Avoid memory allocation.
			QueueRelease(fGeometry);
			fGeometry = Rtt_NEW(display.GetAllocator(), Geometry(*geometryPointer));
			for (int index = (int)fGeometry->GetVerticesUsed() - 1; index >= 0; index--)
			{
				Geometry::Vertex* vertexPointer = &(fGeometry->GetVertexData()[index]);
				vertexPointer->x += offsetX;
				vertexPointer->y += offsetY;
			}
		}
#else
		Super::Prepare(display);
#endif


	}

	void
		TextObject::Draw(Renderer& renderer) const
	{
#ifdef Rtt_RENDER_TEXT_TO_NEAREST_PIXEL
		if (ShouldDraw())
		{
			RenderData fillData = GetFillData();
			fillData.fGeometry = fGeometry;
			fillData.fMaskUniform = fMaskUniform;
			GetFillShader()->Draw(renderer, fillData);
		}
#else
		Super::Draw(renderer);
#endif
	}

	const LuaProxyVTable&
		TextObject::ProxyVTable() const
	{
		return LuaTextObjectProxyVTable::Constant();
	}

	/*
	void
	TextObject::SetColor( Paint* newValue )
	{
		if ( fShape )
		{
			fShape->SetFill( newValue );
			InvalidateDisplay();
		}
	}
	*/

	void
		TextObject::SetText(const char* newValue)
	{
		const char kEmptyString[] = "";
		if (!newValue)
		{
			newValue = kEmptyString;
		}

		if (0 != Rtt_StringCompare(fText.GetString(), newValue))
		{
			fText.Set(newValue);
			Reset();
		}
	}

	void
		TextObject::SetSize(Real newValue)
	{
		if (fOriginalFont)
		{
			// If the given font size is invalid, then use the system's default font size.
			if (newValue < Rtt_REAL_1)
			{
				newValue = fDisplay.GetRuntime().Platform().GetStandardFontSize() * fDisplay.GetSxUpright();
			}

			// Update the font and text bitmap, but only if the font size has changed.
			if (GetSize() != newValue)
			{
				fOriginalFont->SetSize(newValue);
				Reset();
			}
		}
	}

	Real
		TextObject::GetSize() const
	{
		Rtt_ASSERT(fOriginalFont);
		return fOriginalFont->Size();
	}

	void
		TextObject::SetFont(PlatformFont* newValue)
	{
		if (newValue && fOriginalFont != newValue)
		{
			Rtt_DELETE(fOriginalFont);
			fOriginalFont = newValue;
			Reset();
		}
	}
	void
		TextObject::SetAlignment(const char* newValue)
	{
		if (newValue)
		{
			if (0 != Rtt_StringCompare(fAlignment.GetString(), newValue))
			{
				fAlignment.Set(newValue);
				Reset();
			}
		}
	}
#pragma region outlinetext about




	//static void OutlineTextLog(const  lua_State* L, const char* fmt, ...)
	//{
	//	constexpr int LOG_BUFFER_SIZE = 1024;
	//	static char* logBuffer = new char[LOG_BUFFER_SIZE];
	//	static char* logBuffer2 = new char[LOG_BUFFER_SIZE];
	//	va_list argp;
	//	va_start(argp, fmt);
	//	vsprintf(logBuffer, fmt, argp);
	//	va_end(argp);
	//	sprintf(logBuffer2, "OutlineText:: %s", logBuffer);
	//	CoronaLuaWarning(const_cast<lua_State*>(L), logBuffer2);
	//}

#define ColorEqual(v1,v2)  ( ((v1).r==(v2).r) && ( (v1).g==(v2).g ) && ( (v1).b==(v2).b ) && ( (v1).a==(v2).a) )
#define SetVertColor(vert,col,multyAlpha)  \
(vert).as = (col).a*(multyAlpha);\
(vert).rs = (col).r*(multyAlpha);\
(vert).gs = (col).g*(multyAlpha);\
(vert).bs = (col).b*(multyAlpha);\

#define SetVertUserData(vert,data)  \
(vert).ux = (data)[0];\
(vert).uy = (data)[1];\
(vert).uz = (data)[2];\
(vert).uw =(data)[3];

#define SetVertOffset(vert,data)  \
(vert).x += (data)[0];\
(vert).y += (data)[1];

#define SetVertUserDataAndColor(vert,data,col,multyAlpha) \
SetVertUserData(vert,data);\
SetVertColor(vert,col,multyAlpha)

#define SetVertVertOffsetAndColor(vert,data,col,multyAlpha) \
SetVertOffset(vert,data);\
SetVertColor(vert,col,multyAlpha)


#define To255ColorValue(value) (value )* 255.0f
#define Clamp255(value)  ( (value) < 0 ? 0 : ( (value )> 255.0f ? 255.0f : (value)))
#define ClampNum1Num2(value,num1,num2)  ( (value) < (num1 )? (num1): ( (value )> (num2) ? (num2) : (value)))
#define Clamp01(value)   ClampNum1Num2(value,0.0f,1.0f)
#define SafeLuaNumberCheck(LL,valueIndex,funcName ) \
if (!lua_isnumber(LL, valueIndex))  \
{ CoronaLog("%s SafeLuaNumberCheck error  value is not a number ... , please ensure argument is valid ", funcName);\
return;}

#define OutlineTextBindFunc_HeadCode  	OutlineTextObject* txt = &( const_cast<OutlineTextObject&> (static_cast<const OutlineTextObject&>(object)) );\
Rtt_WARN_SIM_PROXY_TYPE(L, 1, OutlineTextObject);\
if (txt == NULL) {  CoronaLog( "Can not find textObject , call '%s' method use : instead .", __func__); return 0; }

#define ParseColorErr_ReturnZero(L)  CoronaLog( "%s parse argument error,please check argument ", __func__); return 0;
#define ParseColorErr_Return(L)  ("%s parse argument error,please check argument ", __func__);  ;



	OutlineTextObject::OutlineTextObject
	(
		Display& display
		, const char text[]
		, PlatformFont* font
		, Real w
		, Real h
		, const char alignment[]
		, Real outlineWidth
		, RGBA fillColor
		, RGBA outlineColor
		, bool beautyOutline
	) :Super(display, text, font, w, h, alignment,
		[outlineWidth, outlineColor, beautyOutline](
			Runtime& runtime,
			const char text[],
			const PlatformFont& font,
			Real w,
			Real h,
			const char alignment[],
			Real& baselineOffset
			) {
		Real  offset = 0;
		Rtt::BitmapPaint* ret = NULL;
		if (beautyOutline)
		{
			ret = TextMaskPaintGetter(runtime, text, font, w, h, alignment, offset, outlineWidth, outlineColor);
		}
		else
		{
			ret = TextMaskPaintGetter(runtime, text, font, w, h, alignment, offset, 0, { 0 });
		}
		baselineOffset = offset;
		return ret;
	})

		//描边相关的参数
		, foutlineWidth(outlineWidth)
		, foutlineColor(outlineColor)
		, fBeautyOutline(beautyOutline)
		, fMeshDirty(true)
	{
		fS9RGBA.SetColor(fillColor);
		fNideArgs.SetDefault();
	}


	Rtt_INLINE void OutlineTextObject::SetOutlineWidth(Real v)
	{
		if (v < 0)
		{
			v = 0;
		}
		if (foutlineWidth != v)
		{
			SetMeshDirty();
			foutlineWidth = v;
		}
	}


	Rtt_INLINE void OutlineTextObject::SetOutlineColor(RGBA v)
	{
		if (!ColorEqual(v, foutlineColor))
		{
			SetMeshDirty();
			foutlineColor = v;
		}
	}

	//LT LB RT RB
	Rtt_INLINE void OutlineTextObject::GetFillS9Color(S9RGBA& colForFill)
	{
		this->fS9RGBA.CopyTo(colForFill);
	}


	Rtt_INLINE void OutlineTextObject::SetFill16Color(S9RGBA& col)
	{
		bool isDiff = false;
		fS9RGBA.CopyFrom(col, isDiff);
		if (isDiff)
		{
			SetMeshDirty();
		}
	}

	Rtt_INLINE void OutlineTextObject::GetNineArgs(SNineImageArg& args)
	{
		this->fNideArgs.CopyTo(args);
	}

	Rtt_INLINE void OutlineTextObject::SetNineArgs(SNineImageArg col)
	{
		bool isDiff = false;
		this->fNideArgs.CopyFrom(col, isDiff);
		if (isDiff)
		{
			SetMeshDirty();
		}
	}

	void OutlineTextObject::Prepare(const Display& display)
	{
		bool shouldPrepare = this->ShouldPrepare();
		Super::Prepare(display);
		if (
			//false &&
			!fBeautyOutline
			)
		{
#ifdef Rtt_RENDER_TEXT_TO_NEAREST_PIXEL
			//windows 平台下， 作者为了实时相应系统DPI发生修改，文本会每帧都进行更新和渲染，其实大可不必...
			ProcessMeshCopyOutline(fGeometry);
#else 
			if (shouldPrepare || GetMeshDirty())
			{
				ProcessMeshCopyOutline(GetFillData().fGeometry);
			}
#endif
		}

	}


	void OutlineTextObject::Draw(Renderer& renderer)const
	{
		Super::Draw(renderer);
	}
	//处理基于mesh描边逻辑
	void OutlineTextObject::ProcessMeshCopyOutline(Geometry* geometry)
	{
		constexpr int FOUR = 4;//1个方块 4个顶点表示
		constexpr int SIX = 6;//1个方块，2个3角形
		constexpr int FIVE = 5;
		constexpr int NINE = 9;//前景字九宫 划分成9个顶点
#if 0

		if (geometry == NULL || fBeautyOutline)   return;
		//if (!GetMeshDirty() && geometry->GetVerticesUsed() != 6 ){return;}

		geometry->SetPrimitiveType(Rtt::Geometry::PrimitiveType::kTriangles); //corona默认的三角形模式， 逆时针

		//备份原有的4个顶点
		Geometry::Vertex* originData = geometry->GetVertexData();
		/*
		 三角形顺序是 逆时针！ 比如 左侧三角形可以是：0 1 2  。。。，右侧三角形可以是：2 1 3 ， 1 3 2 ， 3 2 1
		 此处采用的顺序是  0 1 2  ，  1 3 2
			0             2
			┌─────┐
			│            ┃
			└━━━━━━┘
			1              3
		*/

		Geometry::Vertex v0;//LT
		Geometry::Vertex v1;//LB
		Geometry::Vertex v2;//RT
		Geometry::Vertex v3;//RB

		if (geometry->GetVerticesUsed() == FOUR)
		{
			v0 = originData[0];//LT
			v1 = originData[1];//LB
			v2 = originData[2];//RT
			v3 = originData[3];//RB
		}
		else
		{
			v0 = originData[0];//LT
			v1 = originData[1];//LB
			v2 = originData[2];//RT
			v3 = originData[4];//RB
		}


		//CoronaLog("LT: %f %f  LB: %f %f  RT: %f %f  RB: %f %f  vertUsed=%d",v0.x,v0.y,v1.x,v1.y,v2.x,v2.y,v3.x,v3.y,geometry->GetVerticesUsed());
		Real ZeroUserData[4] = { 0,0,0,0 };
		SetVertUserData(v0, ZeroUserData); //顶点上的userData置0
		SetVertUserData(v1, ZeroUserData);
		SetVertUserData(v2, ZeroUserData);
		SetVertUserData(v3, ZeroUserData);
		//容量不够就扩容
		constexpr int vertNum = 4 * SIX + 9 * SIX;//4 = 4个边缘字  9 = 前景9宫格字
		if (geometry->GetVerticesAllocated() < vertNum)
		{
			geometry->Resize(vertNum, 0, false);
		}
		geometry->SetVerticesUsed(vertNum);
		geometry->SetIndicesUsed(0);
		Geometry::Vertex* vData = geometry->GetVertexData();
		float offset = foutlineWidth;
		//顺序：上下左右
		float symbolX[FOUR] = { 0,0,-1,1 };
		float symbolY[FOUR] = { -1,1,0,0 };
		float cumulativeAlpha = (float)(AlphaCumulative() / 255.0f);
		//4份背景字
		for (int i = 0; i < FIVE - 1; ++i)
		{
			vData[i * SIX + 0] = v0;
			vData[i * SIX + 1] = v1;
			vData[i * SIX + 2] = v2;
			vData[i * SIX + 3] = v1;
			vData[i * SIX + 4] = v3;
			vData[i * SIX + 5] = v2;
			Real offset0[4] = { symbolX[i] * offset,symbolY[i] * offset,0,0 };
			Real offset1[4] = { symbolX[i] * offset,symbolY[i] * offset,0,0 };
			Real offset2[4] = { symbolX[i] * offset,symbolY[i] * offset,0,0 };
			Real offset3[4] = { symbolX[i] * offset,symbolY[i] * offset,0,0 };
			Real offset4[4] = { symbolX[i] * offset,symbolY[i] * offset,0,0 };
			Real offset5[4] = { symbolX[i] * offset,symbolY[i] * offset,0,0 };



			SetVertUserDataAndColor(vData[i * SIX + 0], offset0, foutlineColor, cumulativeAlpha);//0
			SetVertUserDataAndColor(vData[i * SIX + 1], offset1, foutlineColor, cumulativeAlpha);//1
			SetVertUserDataAndColor(vData[i * SIX + 2], offset2, foutlineColor, cumulativeAlpha);//2
			SetVertUserDataAndColor(vData[i * SIX + 3], offset3, foutlineColor, cumulativeAlpha);//1
			SetVertUserDataAndColor(vData[i * SIX + 4], offset4, foutlineColor, cumulativeAlpha);//3
			SetVertUserDataAndColor(vData[i * SIX + 5], offset5, foutlineColor, cumulativeAlpha);//2
		}
		//1份  九宫图的前景字
		{
			Real x1 = fNideArgs.x1;
			Real x2 = fNideArgs.x2;
			Real y1 = fNideArgs.y1;
			Real y2 = fNideArgs.y2;

			Geometry::Vertex nine_v0[NINE];
			Geometry::Vertex nine_v1[NINE];
			Geometry::Vertex nine_v2[NINE];
			Geometry::Vertex nine_v3[NINE];
			Real width = (v2.x - v0.x);
			Real height = (v1.y - v0.y);



			Real VertLT_X[3] = { v0.x + 0,   v0.x + x1 * width,      v0.x + x2 * width };
			Real VertLT_Y[3] = { v0.y + 0,   v0.y + y1 * height,      v0.y + y2 * height };
			Real VertLB_X[3] = { v0.x + 0,   v0.x + x1 * width,      v0.x + x2 * width };
			Real VertLB_Y[3] = { v0.y + y1 * height ,   v0.y + y2 * height,     v0.y + height };
			Real VertRT_X[3] = { v0.x + x1 * width, v0.x + x2 * width, v0.x + width };
			Real VertRT_Y[3] = { v0.y + 0,   v0.y + y1 * height,      v0.y + y2 * height };
			Real VertRB_X[3] = { v0.x + x1 * width, v0.x + x2 * width, v0.x + width };
			Real VertRB_Y[3] = { v0.y + y1 * height ,   v0.y + y2 * height,     v0.y + height };


			Real UVLT_X[3] = { v0.u + 0,   v0.u + x1 * 1,      v0.u + x2 * 1 };
			Real UVLT_Y[3] = { v0.v + 0,   v0.v + y1 * 1,      v0.v + y2 * 1 };
			Real UVLB_X[3] = { v0.u + 0,   v0.u + x1 * 1,      v0.u + x2 * 1 };
			Real UVLB_Y[3] = { v0.v + y1 * 1 ,   v0.v + y2 * 1,     v0.v + 1 };
			Real UVRT_X[3] = { v0.u + x1 * 1, v0.u + x2 * 1, v0.u + 1 };
			Real UVRT_Y[3] = { v0.v + 0,   v0.v + y1 * 1,      v0.v + y2 * 1 };
			Real UVRB_X[3] = { v0.u + x1 * 1, v0.u + x2 * 1, v0.u + 1 };
			Real UVRB_Y[3] = { v0.v + y1 * 1 ,   v0.v + y2 * 1,     v0.v + 1 };

			for (int i = 0; i < NINE; ++i)
			{
				int index1 = i % 3;
				int index2 = i / 3;
				Real x = VertLT_X[i % 3];
				nine_v0[i] = v0;  nine_v0[i].x = (VertLT_X[i % 3]);  nine_v0[i].y = (VertLT_Y[i / 3]);
				nine_v1[i] = v1;  nine_v1[i].x = (VertLB_X[i % 3]);  nine_v1[i].y = (VertLB_Y[i / 3]);
				nine_v2[i] = v2;  nine_v2[i].x = (VertRT_X[i % 3]);  nine_v2[i].y = (VertRT_Y[i / 3]);
				nine_v3[i] = v3;  nine_v3[i].x = (VertRB_X[i % 3]);  nine_v3[i].y = (VertRB_Y[i / 3]);

				nine_v0[i].u = (UVLT_X[i % 3]);  nine_v0[i].v = (UVLT_Y[i / 3]);
				nine_v1[i].u = (UVLB_X[i % 3]);  nine_v1[i].v = (UVLB_Y[i / 3]);
				nine_v2[i].u = (UVRT_X[i % 3]);  nine_v2[i].v = (UVRT_Y[i / 3]);
				nine_v3[i].u = (UVRB_X[i % 3]);  nine_v3[i].v = (UVRB_Y[i / 3]);
			}

			for (int i = 0; i < NINE; ++i)
			{
				SQuadGBA cols = this->fS9RGBA.Cols9[i];
				int baseIndexOfVert = (FIVE - 1) * SIX + i * SIX;
				vData[baseIndexOfVert + 0] = nine_v0[i]; SetVertColor(vData[baseIndexOfVert + 0], cols.Cols4[0], cumulativeAlpha);
				vData[baseIndexOfVert + 1] = nine_v1[i]; SetVertColor(vData[baseIndexOfVert + 1], cols.Cols4[1], cumulativeAlpha);
				vData[baseIndexOfVert + 2] = nine_v2[i]; SetVertColor(vData[baseIndexOfVert + 2], cols.Cols4[2], cumulativeAlpha);
				vData[baseIndexOfVert + 3] = nine_v1[i]; SetVertColor(vData[baseIndexOfVert + 3], cols.Cols4[1], cumulativeAlpha);
				vData[baseIndexOfVert + 4] = nine_v3[i]; SetVertColor(vData[baseIndexOfVert + 4], cols.Cols4[3], cumulativeAlpha);
				vData[baseIndexOfVert + 5] = nine_v2[i]; SetVertColor(vData[baseIndexOfVert + 5], cols.Cols4[2], cumulativeAlpha);
			}
		}

#else
		geometry->SetPrimitiveType(Rtt::Geometry::PrimitiveType::kIndexedTriangles);  

		//备份原有的4个顶点
		Geometry::Vertex* originData = geometry->GetVertexData();
		//  0 1 2  , 2 1 3
		/*
			0             2
			┌─────┐
			│            ┃
			└━━━━━━┘
			1              3
		*/
		Geometry::Vertex v0 = originData[0];//LT
		Geometry::Vertex v1 = originData[1];//LB
		Geometry::Vertex v2 = originData[2];//RT
		Geometry::Vertex v3 = originData[3];//RB
		Real ZeroUserData[4] = { 0,0,0,0 };
		SetVertUserData(v0, ZeroUserData); //顶点上的userData置0
		SetVertUserData(v1, ZeroUserData);
		SetVertUserData(v2, ZeroUserData);
		SetVertUserData(v3, ZeroUserData);
		//容量不够就扩容
		if (geometry->GetVerticesAllocated() < (FIVE * FOUR))
		{
			geometry->Resize(FIVE * FOUR, FIVE* SIX, false);
		}
		geometry->SetVerticesUsed(FIVE * FOUR);
		geometry->SetIndicesUsed(FIVE * SIX);
		Geometry::Vertex* vData = geometry->GetVertexData();
		Geometry::Index* indexData = geometry->GetIndexData();
		float offset = foutlineWidth;
		offset = 30  ;
		//顺序：上下左右
		float symbolX[FOUR] = { -1,-1,1,1 };
		float symbolY[FOUR] = { -1,1,-1,1 };
		//4份背景字
		for (int i = 0; i < FIVE - 1; ++i)
		{
			vData[i * FOUR + 0] = v0;
			vData[i * FOUR + 1] = v1;
			vData[i * FOUR + 2] = v2;
			vData[i * FOUR + 3] = v3;
			Real offset0[4] = { symbolX[i] * offset,symbolY[i] * offset,0,0 };
			Real offset1[4] = { symbolX[i] * offset,symbolY[i] * offset,0,0 };
			Real offset2[4] = { symbolX[i] * offset,symbolY[i] * offset,0,0 };
			Real offset3[4] = { symbolX[i] * offset,symbolY[i] * offset,0,0 };
			 
			SetVertVertOffsetAndColor(vData[i * FOUR + 0], offset0, foutlineColor,1);//0
			SetVertVertOffsetAndColor(vData[i * FOUR + 1], offset1, foutlineColor,1);//1
			SetVertVertOffsetAndColor(vData[i * FOUR + 2], offset2, foutlineColor,1);//2
			SetVertVertOffsetAndColor(vData[i * FOUR + 3], offset3, foutlineColor,1);//1
		}
		//1份前景字
		vData[(FIVE - 1) * FOUR + 0] = v0; SetVertColor(vData[(FIVE - 1) * FOUR + 0], foutlineColor,1);
		vData[(FIVE - 1) * FOUR + 1] = v1; SetVertColor(vData[(FIVE - 1) * FOUR + 1], foutlineColor,1);
		vData[(FIVE - 1) * FOUR + 2] = v2; SetVertColor(vData[(FIVE - 1) * FOUR + 2], foutlineColor,1);
		vData[(FIVE - 1) * FOUR + 3] = v3; SetVertColor(vData[(FIVE - 1) * FOUR + 3], foutlineColor,1);

		for (int i = 0; i < FIVE ; i++)
		{
			//  0 1 2 , 2 1 3 
			indexData[i * SIX + 0] = i * FOUR + 0;
			indexData[i * SIX + 1] = i * FOUR + 1;
			indexData[i * SIX + 2] = i * FOUR + 2;
			indexData[i * SIX + 3] = i * FOUR + 2;
			indexData[i * SIX + 4] = i * FOUR + 1;
			indexData[i * SIX + 5] = i * FOUR + 3;
		}

#endif // 0



		fMeshDirty = false;

	}
	typedef std::function<void(lua_State*, const char*, int, int)> OnIterateStackTable;
	bool IterateStackTable(lua_State* L, int indexOfTable, const char* funcName, OnIterateStackTable f)
	{
		if (!lua_istable(L, indexOfTable))
		{
			int luatype = lua_type(L, indexOfTable);

			CoronaLog(" call %s function error ,  can not get table in lua stack , use : instead of . to call this func ... ,index = %d, type = %d", funcName, indexOfTable, luatype);
			return false;
		}

		// 将栈上index位置的对象拷贝一份到栈顶  https://www.lua.org/manual/5.1/manual.html#lua_pushvalue
		lua_pushvalue(L, indexOfTable); // 现在的栈 : -1 => table ...
		lua_pushnil(L); //  现在的栈 : -1 => nil;   -2 => table
		while (lua_next(L, -2))  //  在index位置 压入1对 key-value，  https://www.lua.org/manual/5.1/manual.html#lua_next
		{
			// 现在的栈 :   -1 => value; -2 => key; -3 => table
			lua_pushvalue(L, -2); // 拷贝1份刚才lua_next弹出来的key，避免被修改      // key |  value | key(刚push进来的) | table | ...
			// 现在的栈 :  -1 => key; -2 => value; -3 => key; -4 => table   
			const char* key = lua_tostring(L, -1);  //
			//const char* value = lua_tostring(L, -2);
			 //void* p = lua_touserdata(L, -2);
			 //printf("%s => %s %x \n", key, value,p);
			f(L, key, -1, -2);
			//  弹出由lua_next() 压入的key-value 对
			lua_pop(L, 2); //2：代表栈顶的2个对象
			// stack now contains: -1 => key; -2 => table 
		}
		// 现在的栈： -1 => table  官方原话：If there are no more elements in the table, then lua_next returns 0 (and pushes nothing).

		lua_pop(L, 1); //弹出拷贝的table， 对应函数的第1句.  然后栈状态恢复如初，和原来的保持一致
		return true;
	}

	static bool Parse01ColorFromLuaStack(lua_State* L, const char* funcName, int keyIndex, Rtt::RGBA& colorForFill)
	{
		colorForFill.a = 255;
		if (! IterateStackTable(L, keyIndex, funcName,
			[&colorForFill, funcName](lua_State* LL, const char* keyName, int keyIndex, int valueIndex)
		{
			//两种都支持  {r = 0,g = 1,b = 0,a = 1}  或者  {0,1,0, 1}
			if (0 == strcmp(keyName, "r") || 0 == strcmp(keyName, "1"))
			{
				SafeLuaNumberCheck(LL, valueIndex, funcName);
				colorForFill.r = Clamp255(To255ColorValue(lua_tonumber(LL, valueIndex)));
			}
			if (0 == strcmp(keyName, "g") || 0 == strcmp(keyName, "2"))
			{
				SafeLuaNumberCheck(LL, valueIndex, funcName);
				colorForFill.g = Clamp255(To255ColorValue(lua_tonumber(LL, valueIndex)));
			}
			if (0 == strcmp(keyName, "b") || 0 == strcmp(keyName, "3"))
			{
				SafeLuaNumberCheck(LL, valueIndex, funcName);
				colorForFill.b = Clamp255(To255ColorValue(lua_tonumber(LL, valueIndex)));
			}
			if (0 == strcmp(keyName, "a") || 0 == strcmp(keyName, "4"))
			{
				SafeLuaNumberCheck(LL, valueIndex, funcName);
				colorForFill.a = Clamp255(To255ColorValue(lua_tonumber(LL, valueIndex)));
			}
		}))
		{
			 CoronaLog( "%s parse color error , please check argument valid ...", funcName);
			return false;
		}
		return true;


	}


	static bool Parse01NineArgsFromLuaStack(lua_State* L, const char* funcName, int keyIndex, OutlineTextObject::SNineImageArg& args)
	{

		if (! IterateStackTable(L, keyIndex, funcName,
			[&args, funcName](lua_State* LL, const char* keyName, int keyIndex, int valueIndex)
		{

			if (0 == strcmp(keyName, "x1"))
			{
				SafeLuaNumberCheck(LL, valueIndex, funcName);
				args.x1 = Clamp01(lua_tonumber(LL, valueIndex));
			}
			if (0 == strcmp(keyName, "x2"))
			{
				SafeLuaNumberCheck(LL, valueIndex, funcName);
				args.x2 = Clamp01(lua_tonumber(LL, valueIndex));
			}
			if (0 == strcmp(keyName, "y1"))
			{
				SafeLuaNumberCheck(LL, valueIndex, funcName);
				args.y1 = Clamp01(lua_tonumber(LL, valueIndex));
			}
			if (0 == strcmp(keyName, "y2"))
			{
				SafeLuaNumberCheck(LL, valueIndex, funcName);
				args.y2 = Clamp01(lua_tonumber(LL, valueIndex));
			}

		}))
		{
			 CoronaLog( "%s parse  error , please check argument valid ...", funcName);
			return false;
		}
		return true;


	}

	static bool TryGetFloat(lua_State* L, int index, float& num, const char* errorMsg)
	{
		if (lua_isnumber(L, index))
		{
			num = lua_tonumber(L, index);
			return true;
		}
		else
		{
			CoronaLog(errorMsg);
			return false;
		}
	}

	static void setSomethingAsProperty(lua_State* L, const char* key, std::function<void(lua_State*)> onTablePush)
	{
		Rtt_ASSERT(lua_istable(L, -1));
		lua_pushlstring(L, key, strlen(key));
		onTablePush(L);
		lua_settable(L, -3);
	}
	static void setNumberProperty(lua_State* L, const char* key, float value)
	{
		Rtt_ASSERT(lua_istable(L, -1));
		lua_pushlstring(L, key, strlen(key));
		lua_pushnumber(L, Rtt_RealToFloat(value));
		lua_settable(L, -3);
	}
	static void CreateLuaTable(lua_State* L, int preAllocArrarNum, int preAllocKvNum)
	{
		lua_createtable(L, preAllocArrarNum, preAllocKvNum);
	}

	static void PushColorTable(lua_State* L, RGBA color)
	{
		 CreateLuaTable(L, 0, 4);
		const static char* rgbaKeys[4] = { "r","g", "b", "a" };
		ColorUnion uniColor;
		uniColor.rgba = color;
		for (int i = 0; i < 4; i++)
		{
			 setNumberProperty(L, rgbaKeys[i], uniColor.channels[i] / 255.0f);
		}
	}

	static void PushNineArgsTable(lua_State* L, OutlineTextObject::SNineImageArg args)
	{
		 CreateLuaTable(L, 0, 4);
		 setNumberProperty(L, "x1", args.x1);
		 setNumberProperty(L, "x2", args.x2);
		 setNumberProperty(L, "y1", args.y1);
		 setNumberProperty(L, "y2", args.y2);
	}


	/*
	参数：
	{
		LT={r = 1, g = 1, b = 1,a = 1},
		LB={r = 1, g = 1, b = 1,a = 1},
		RT={r = 1, g = 1, b = 1,a = 1},
		RB={r = 1, g = 1, b = 1,a = 1}
	}
	或者:
	{
		LT={1, 1, 1,1},
		LB={1, 1, 1,1},
		RT={1, 1, 1,1},
		RB={1, 1, 1,1}
	}
	*/

	static void  ParseQuadColor(lua_State* L, int valueIndex, const char* funcName, OutlineTextObject::SQuadGBA& quadColorForFill)
	{
		constexpr RGBA blackValue = { 0,0,0,255 }; //alpha默认值是1
		RGBA LT = blackValue;
		RGBA LB = blackValue;
		RGBA RT = blackValue;
		RGBA RB = blackValue;
		if (! IterateStackTable(L, valueIndex, funcName,
			[&LT, &LB, &RT, &RB, funcName](lua_State* LL, const char* keyName, int keyIndex, int valueIndex)
		{
			if (0 == strcmp(keyName, "LT"))
			{
				if (!Parse01ColorFromLuaStack(LL, funcName, valueIndex, LT))
				{
					ParseColorErr_Return(LL);
				}
			}
			else if (0 == strcmp(keyName, "LB"))
			{
				if (!Parse01ColorFromLuaStack(LL, funcName, valueIndex, LB))
				{
					ParseColorErr_Return(LL);
				}
			}
			else if (0 == strcmp(keyName, "RT"))
			{
				if (!Parse01ColorFromLuaStack(LL, funcName, valueIndex, RT))
				{
					ParseColorErr_Return(LL);
				}

			}
			else if (0 == strcmp(keyName, "RB"))
			{
				if (!Parse01ColorFromLuaStack(LL, funcName, valueIndex, RB))
				{
					ParseColorErr_Return(LL);
				}
			}

		}))
		{
			ParseColorErr_Return(L);
		}
		quadColorForFill.SetColors(LT, LB, RT, RB);
	}


	int OutlineTextObject::BindingProperty_Set_FillVertColor(lua_State* L, const MLuaProxyable& object, int valueIndex)
	{
		OutlineTextBindFunc_HeadCode;

		const char* funcName = __func__;
		S9RGBA s9rgba;
		int colIndex = 0;
		if (! IterateStackTable(L, valueIndex, funcName,
			[funcName, &s9rgba, &colIndex](lua_State* LL, const char* keyName, int keyIndex, int valueIndex) {
			if (colIndex < 9)
			{
				ParseQuadColor(LL, valueIndex, funcName, s9rgba.Cols9[colIndex]);
			}
			else
			{
				 CoronaLog( "%s parse argument error, array length to long ... curIndex =%d, skip parse ", funcName, colIndex);
			}
			colIndex++;
		}))
		{
			ParseColorErr_Return(L);
		}
		txt->SetFill16Color(s9rgba);

		return 0;
	}

	int OutlineTextObject::BindingProperty_Get_FillVertColor(lua_State* L, const MLuaProxyable& object)
	{
		OutlineTextBindFunc_HeadCode;
		constexpr int NINE = 9;
		 CreateLuaTable(L, NINE, 0);//推送数组 9宫格的颜色信息
		static const  char* keys[4] = { "LT","LB","RT","RB" };
		OutlineTextObject::S9RGBA S9rgba;
		txt->GetFillS9Color(S9rgba);//LT LB RT RB
		for (size_t indexColor = 0; indexColor < NINE; indexColor++)
		{
			lua_pushnumber(L, indexColor + 1);
			OutlineTextObject::SQuadGBA quadColors = S9rgba.Cols9[indexColor];
			 CreateLuaTable(L, 0, 4);//推送数组 9宫格的颜色信息
			for (int i = 0; i < 4; i++)
			{
				 setSomethingAsProperty
				(
					L, keys[i],
					[i, quadColors](lua_State* LL)
				{
					PushColorTable(LL, quadColors.Cols4[i]);
				}
				);
			}
			lua_rawset(L, -3);
		}

		return 0;
	}


	/*
	参数：
	{r = 1, g = 1, b = 1,a = 1}
	或者:
	{1, 1, 1,1},
	*/
	int OutlineTextObject::BindingProperty_Set_OutlineColor(lua_State* L, const MLuaProxyable& object, int valueIndex)
	{
		OutlineTextBindFunc_HeadCode;
		RGBA color;
		if (!Parse01ColorFromLuaStack(L, __func__, valueIndex, color))
		{
			ParseColorErr_ReturnZero(L);
		}
		txt->SetOutlineColor(color);
		return 0;
	}


	int OutlineTextObject::BindingProperty_Get_OutlineColor(lua_State* L, const MLuaProxyable& object)
	{
		OutlineTextBindFunc_HeadCode;
		RGBA outlineColor = txt->GetOutlineColor();
		PushColorTable(L, outlineColor);
		return 0;
	}


	int OutlineTextObject::BindingProperty_Get_OutlineWidth(lua_State* L, const MLuaProxyable& object)
	{
		OutlineTextBindFunc_HeadCode;
		Real outlineColor = txt->GetOutlineWidth();
		lua_pushnumber(L, outlineColor);
		return 0;
	}


	int OutlineTextObject::BindingProperty_Set_NineArgs(lua_State* L, const MLuaProxyable& object, int valueIndex)
	{
		OutlineTextBindFunc_HeadCode;
		OutlineTextObject::SNineImageArg nineArgs;
		Parse01NineArgsFromLuaStack(L, __func__, valueIndex, nineArgs);
		txt->SetNineArgs(nineArgs);
		return 0;
	}

	int OutlineTextObject::BindingProperty_Get_NineArgs(lua_State* L, const MLuaProxyable& object)
	{
		OutlineTextBindFunc_HeadCode;

		OutlineTextObject::SNineImageArg nineArgs;
		txt->GetNineArgs(nineArgs);
		PushNineArgsTable(L, nineArgs);
		return 0;
	}

	int OutlineTextObject::BindingProperty_Set_OutlineWidth(lua_State* L, const MLuaProxyable& object, int valueIndex)
	{
		OutlineTextBindFunc_HeadCode;
		Real oldOutlineWidth = txt->GetOutlineWidth();
		float newOutlineWidth = 0;
		static const char* errMsg = "BindingProperty_Set_OutlineWidth error , argument must be number ...,please check it ";
		if (! TryGetFloat(L, valueIndex, newOutlineWidth, errMsg))
		{
			return 0;
		}
		if (newOutlineWidth < 0)
		{
			 CoronaLog( "outline width must be greater than zero value , input value = %f", newOutlineWidth);
		}
		if (newOutlineWidth == oldOutlineWidth)
		{
			// CoronaLog(L,  Warning, "newOutlineWidth==oldOutlineWidth  ,skip set outline with logic ...", newOutlineWidth);
			return 0;
		}
		txt->SetOutlineWidth(newOutlineWidth);
		return 0;
	}


	int OutlineTextObject::BindingProperty_Get_MeshIsDirty(lua_State* L, const MLuaProxyable& object)
	{
		OutlineTextBindFunc_HeadCode;
		lua_pushboolean(L, txt->GetMeshDirty());
		return 0;
	}


#pragma endregion
} // namespace Rtt

// ---------------------------------------------------------- ------------------

