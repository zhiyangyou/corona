//////////////////////////////////////////////////////////////////////////////
//
// This file is part of the Corona game engine.
// For overview and more information on licensing please refer to README.md 
// Home page: https://github.com/coronalabs/corona
// Contact: support@coronalabs.com
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _Rtt_TextObject_H__
#define _Rtt_TextObject_H__

#include "Core/Rtt_Real.h"
#include "Core/Rtt_String.h"
#include "Display/Rtt_RectObject.h"
#include <functional>

// ----------------------------------------------------------------------------
struct lua_State;
namespace Rtt
{

	class BitmapPaint;
	class Geometry;
	class Paint;
	class PlatformFont;
	class RectPath;
	class Runtime;
	class Uniform;

	// ----------------------------------------------------------------------------

	class TextObject : public RectObject
	{
		Rtt_CLASS_NO_COPIES(TextObject)

	public:
		typedef RectObject Super;
		typedef std::function<
			BitmapPaint*
			(
				Runtime& runtime,
				const char text[],
				const PlatformFont& font,
				Real w,
				Real h,
				const char alignment[],
				Real& baselineOffset
				)>  PaintGetter;
		static BitmapPaint* DefaultPaintGetter(
			Runtime& runtime,
			const char text[],
			const PlatformFont& font,
			Real w,
			Real h,
			const char alignment[],
			Real& baselineOffset);

	public:
		static void Unload(DisplayObject& parent);
		static void Reload(DisplayObject& parent);
#ifdef Rtt_WIN_PHONE_ENV
		/// <summary>
		///  <para>Determines if at least 1 text object is flagged as uninitialized for the given display.</para>
		///  <para>This determines if a call to UpdateAllBelongingTo() </para>
		/// </summary>
		/// <returns>
		///  <para>
		///   Returns true if at least 1 text object is uninitialized. This means that the caller needs to
		///   call the static UpdateAllBelongingTo() method to update the uninitialized text objects.
		///  </para>
		///  <para>Returns false if all text objects are updated or if no text objects exist.</para>
		/// </returns>
		static bool IsUpdateNeededFor(Display& display);

		/// <summary>
		///  <para>Creates bitmaps for all text objects flagged as uninitialized for the given display.</para>
		///  <para>This function is only available to Windows Phone builds of Corona.</para>
		///  <para>
		///   This function is needed because text cannot be rendered to a bitmap while Corona is synchronized
		///   with the Direct3D thread. Will cause deadlock.
		///  </para>
		/// </summary>
		/// <param name="display">Reference to one Corona runtime's display object.</param>
		static void UpdateAllBelongingTo(Display& display);
#endif

	public:
		// TODO: Use a string class instead...
		// TextObject retains ownership of font
		TextObject(Display& display, const char text[], PlatformFont* font, Real w, Real h, const char alignment[], PaintGetter getter = NULL);
		virtual ~TextObject();

	protected:
		bool Initialize();
		void UpdateScaledFont();
		void Reset();

	public:
		void Unload();
		void Reload();

	public:
		// MDrawable
		virtual bool UpdateTransform(const Matrix& parentToDstSpace);
		virtual void GetSelfBounds(Rect& rect) const;
		virtual void Prepare(const Display& display);
		virtual void Draw(Renderer& renderer) const;

	public:
		virtual const LuaProxyVTable& ProxyVTable() const;

	public:
		bool IsInitialized() const { return GetMask() ? true : false; }

	public:
		// TODO: Text properties (size, font, color, etc).  Ugh!
		void SetColor(Paint* newValue);

		void SetText(const char* newValue);
		const char* GetText() const { return fText.GetString(); }

		Real GetBaselineOffset() const { return fBaselineOffset; }
		void SetSize(Real newValue);
		Real GetSize() const;



		// Note: assumes receiver will own the font after SetFont() is called
		void SetFont(PlatformFont* newValue);
		//		const PlatformFont* GetFont() const { return fFont; }

		void SetAlignment(const char* newValue);
		const char* GetAlignment() const { return fAlignment.GetString(); };
	protected:
		Display& fDisplay;
		String fText;
		PlatformFont* fOriginalFont;
		PlatformFont* fScaledFont;
		Real fWidth;
		Real fHeight;
		Real fBaselineOffset;
		String fAlignment;
		PaintGetter fPaintGetter;

		mutable Geometry* fGeometry;
		mutable Uniform* fMaskUniform;
	};

	class OutlineTextObject :public TextObject
	{
		Rtt_CLASS_NO_COPIES(OutlineTextObject)
	public:

		struct SQuadGBA
		{

			/*
			0   2
			1   3
			*/
			RGBA Cols4[4] = { 0 }; //顺序是 LT LB RT RB

			void SetColors(RGBA LT, RGBA LB, RGBA RT, RGBA RB)
			{
				Cols4[0] = LT;
				Cols4[1] = LB;
				Cols4[2] = RT;
				Cols4[3] = RB;
			}
		};
		struct S9RGBA
		{
			/*
			0 1 2
			3 4 5
			6 7 8
			*/
			SQuadGBA Cols9[9];

		public:
			void CopyTo(S9RGBA& otherCols)
			{
				memcpy(&otherCols, this, sizeof(S9RGBA));
			}
			void CopyFrom(S9RGBA& otherCols, bool& isDifferent)
			{
				if (0 == memcmp(&otherCols, this, sizeof(S9RGBA)))
				{
					isDifferent = false;
				}
				else
				{
					memcpy(this, &otherCols, sizeof(S9RGBA));
					isDifferent = true;
				}
			}
			void SetColor(RGBA col)
			{
				for (size_t i = 0; i < 9; i++)
				{
					for (size_t j = 0; j < 4; j++)
					{
						Cols9[i].Cols4[j] = col;
					}
				}
			}
		};

		//九宫格参数的两条线 
		/*
		*       x1        x2
		┏┄┄┄┆┄┄┄┄┄┆┄┄┄┒
		┆      ┆          ┆      ┆
	y1 ┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄
		┆      ┆          ┆      ┆
	y2┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄
		┆      ┆          ┆      ┆
		┕┄┄┄┆┄┄┄┄┄┆┄┄┄┙

		*/
		struct SNineImageArg
		{
			Real x1 = 0;
			Real x2 = 1;
			Real y1 = 0;
			Real y2 = 1;
		public:
			void SetDefault()
			{
				x1 = 0;
				x2 = 1;
				y1 = 0;
				y2 = 1;
			}
			void CopyTo(SNineImageArg& otherArgs)
			{
				memcpy(&otherArgs, this, sizeof(SNineImageArg));
			}
			void CopyFrom(SNineImageArg otherArgs, bool& isDiff)
			{
				if (0 == memcmp(&otherArgs, this, sizeof(SNineImageArg)))
				{
					isDiff = false;
				}
				else
				{
					memcpy(this, &otherArgs, sizeof(SNineImageArg));
					isDiff = true;
				}
			}
		};

	public:
		typedef  Rtt::TextObject  Super;
		OutlineTextObject(Display& display, const char text[], PlatformFont* font, Real w, Real h, const char alignment[], Real outlineWidth, RGBA fillColor, RGBA outlineColor, bool beautyOutline);
		Rtt_INLINE  Real GetOutlineWidth() { return foutlineWidth; }
		Rtt_INLINE  void SetOutlineWidth(Real v);

		Rtt_INLINE RGBA GetOutlineColor() { return foutlineColor; }
		Rtt_INLINE void SetOutlineColor(RGBA v);


		Rtt_INLINE void GetFillS9Color(S9RGBA& colForFill);
		Rtt_INLINE void SetFill16Color(S9RGBA& col);

		Rtt_INLINE void GetNineArgs(SNineImageArg& args);
		Rtt_INLINE void SetNineArgs(SNineImageArg col);



		Rtt_INLINE bool GetMeshDirty() { return fMeshDirty; }
		Rtt_INLINE void  SetMeshDirty() { fMeshDirty = true; }

		void ProcessMeshCopyOutline(Geometry* geometry);
		virtual void Prepare(const Display& display);
		virtual void Draw(Renderer& renderer) const;
	public:
		virtual const LuaProxyVTable& ProxyVTable() const;

	public:
		static int BindingProperty_Set_FillVertColor(lua_State* L, const MLuaProxyable& object, int valueIndex);
		static int BindingProperty_Get_FillVertColor(lua_State* L, const MLuaProxyable& object);
		static int BindingProperty_Set_OutlineColor(lua_State* L, const MLuaProxyable& object, int valueIndex);
		static int BindingProperty_Get_OutlineColor(lua_State* L, const MLuaProxyable& object);
		static int BindingProperty_Set_OutlineWidth(lua_State* L, const MLuaProxyable& object, int valueIndex);
		static int BindingProperty_Get_OutlineWidth(lua_State* L, const MLuaProxyable& object);
		static int BindingProperty_Set_NineArgs(lua_State* L, const MLuaProxyable& object, int valueIndex);
		static int BindingProperty_Get_NineArgs(lua_State* L, const MLuaProxyable& object);


		static int BindingProperty_Get_MeshIsDirty(lua_State* L, const MLuaProxyable& object);

	private:
		bool fMeshDirty = true;
		Real foutlineWidth = 0;
		bool  fBeautyOutline = false;
		RGBA foutlineColor;
		SNineImageArg fNideArgs;
		S9RGBA fS9RGBA;
	};





	// ----------------------------------------------------------------------------
} // namespace Rtt

// ----------------------------------------------------------------------------




#endif // _Rtt_TextObject_H__
