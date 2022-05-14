//#include "CoronaGraphics.h"
#include <stdexcept>
#include "Core/Rtt_Allocator.h"
#include "Core/Rtt_Macros.h"
#include "Core/Rtt_Config.h"
#include "Core/Rtt_Assert.h"
#include "Rtt_LuaProxy.h"
#include "Rtt_LuaProxyVTable.h"
#include "Rtt_LuaContext.h"
#include "Rtt_Lua.h"
#include "Display/Rtt_Display.h"
#include "Renderer/Rtt_Uniform.h"
#include "Renderer/Rtt_Renderer.h"
#include "Display/Rtt_BitmapMask.h"
#include <string>
#include "CoronaLua.h"
#include <spine/spine.h>
#include "Rtt_Spine.h"
#include "Display/Rtt_BitmapPaint.h"
#include "Display/Rtt_Display.h"
#include "Rtt_Runtime.h"
#include "Rtt_MPlatform.h"
#include "Display/Rtt_TextureFactory.h"
#include "Display/Rtt_GroupObject.h"
#include "Display/Rtt_Scene.h"
#include "Display/Rtt_ShapeObject.h"
#include "Display/Rtt_ShapePath.h"
#include "Display/Rtt_TesselatorMesh.h"
#include "Display/Rtt_ShapeAdapterMesh.h"
#include "Display/Rtt_DisplayDefaults.h"
#include "lua.h"
#include "Rtt_LuaResource.h"
#include <type_traits>
#include <stack>
#include <stdarg.h>
#include <list>
#include "CoronaSpineExtension.h"
#include <spine/Extension.h>


using namespace  std;
using namespace spine;
using namespace  Rtt;

namespace spine
{
	const   unsigned short  QUAD_TRIANGLES[6] = { 0, 1, 2, 2, 3, 0 };

	const static int QUAD_INDEX_SIX = 6;//QUAD_TRIANGLES ������6 2�������� 6��index
	const static  int EIGHT = 8; //region�ĸ��� �������̶�Ϊ 4 1��������2D��� , ����2*4=8
	const int LOG_BUFFER_SIZE = 1024;

	class SpineShapeObj : public Rtt::ShapeObject {
	public:
		SpineShapeObj(ClosedPath *path) : ShapeObject(path)
		{
			this->SetHitTestable(false);//����Ҫ�������,��ʡ����,���ҵ����Ҫ�Ļ�,
		}
		//��Ϊspine������������󶼲��ɲü�, Ϊ�˱���һЩ��spineִ��clip��mesh������Ļ��Ե, ������ϻᷢ����˸������,
		virtual bool CanCull() const { return false; }
	};

	class SpineDataBuffer
	{
	public:

#define defualtCacheCapacity  10000 // 10000 �ܶ���  lua�Ǳߵ�����ʱֱ��д��8w�Ĵ�С
		SpineDataBuffer(Rtt::Display & display)
		{
			auto _ac = display.GetAllocator();
			verts = Rtt_NEW(_ac, spine::Vector<float>());
			indices = Rtt_NEW(_ac, spine::Vector<unsigned short>());
			uvs = Rtt_NEW(_ac, spine::Vector<float>());

			verts->ensureCapacity(defualtCacheCapacity);
			uvs->ensureCapacity(defualtCacheCapacity);
			indices->ensureCapacity(defualtCacheCapacity);
		}
		~SpineDataBuffer()
		{
			Rtt_DELETE(verts);
			Rtt_DELETE(indices);
			Rtt_DELETE(uvs);
		}
		Rtt_INLINE void Clear() {
			this->verts->clear();
			this->indices->clear();
			this->uvs->clear();
		}
		Rtt_INLINE bool IsEmpty()
		{
			return
				this->verts->size() == 0
				&& this->uvs->size() == 0
				&& this->indices->size() == 0;
		}
		spine::Vector<float>*  verts;
		spine::Vector<unsigned short>* indices;//�����ƶ�����Ա����ȥ...
		spine::Vector<float >*  uvs;
	private:
		SpineDataBuffer() {}
	};

	class TextureLoadArgs
	{
	private:
		Rtt::MPlatform::Directory _baseDir;
		Rtt::String  *   _relativePath;
		std::string  _strTextureCacheKey;
	public:
		std::string &  GetTextureCacheKey() { return  _strTextureCacheKey; }
	public:
		Rtt::MPlatform::Directory	GetBaseDir()
		{
			return _baseDir;
		}
		Rtt::String & GetRelativePath()
		{
			return *_relativePath;
		}
	public:
		TextureLoadArgs(
			Rtt_Allocator*  ac,
			Rtt::MPlatform::Directory baseDir,
			const char *     relativePath,
			std::string & strTextureCacheKey
		) :
			_baseDir(baseDir),
			_strTextureCacheKey(strTextureCacheKey)//�Կ�.
		{
			_relativePath = Rtt_NEW(ac, Rtt::String(ac, relativePath));
		}
		~TextureLoadArgs()
		{
			Rtt_DELETE(_relativePath);
		}

	};
	enum ESpineLog
	{
		Info = 0,
		Warning,
		Error,
	};

	template<typename  TOriginType, typename  TargetType>
	static bool Is(TOriginType  * const o, TargetType*& outP)
	{
		//��vs��profile������ ����ؼ����� ��һ���ʱ�ĵģ�Ŀǰ��֪����ô�Ż����...
		outP = dynamic_cast<TargetType*>(o);
		return outP != nullptr;
	}


	static void SpineLog(lua_State * L, const ESpineLog & level, const char * fmt, ...)
	{
		static char *logBuffer = new char[LOG_BUFFER_SIZE];
		static char *logBuffer2 = new char[LOG_BUFFER_SIZE];
		va_list argp;
		va_start(argp, fmt);
		vsprintf(logBuffer, fmt, argp);
		va_end(argp);
		/*�˴�����һ����, ���ǲ��ᱬը,
		1.�����־����,�õ��ĵط����ޱ�ҳ����, ���Բ鿴��������,�ַ�������û�г���LOG_BUFFER_SIZE��
		2.����ʹ��sprintf_s ���� snprintf_s  ��buffersize���� , �������ַ�����������, Ҳ��ͨ��ע��ȫ�ֻص��������,_set_invalid_parameter_handler
		3.��Ϊ���ûص����ֶ�����ȫ����, ��֪��corona��û�����ƵĲ���,�ʲ�����
		4.������ɶ����,Ӧ��...
		*/
		sprintf(logBuffer2, "SpineCpp:: %s", logBuffer);

		switch (level)
		{
		case ESpineLog::Info:CoronaLuaLog(L, logBuffer2); break;
		case ESpineLog::Warning:CoronaLuaWarning(L, logBuffer2); break;
		case ESpineLog::Error: CoronaLuaError(L, logBuffer2); break;
		default:break;
		}
	}

	static Rtt::RenderTypes::BlendType  toCoronaBlendMode(spine::BlendMode  mode)
	{
		switch (mode)
		{
		case   spine::BlendMode::BlendMode_Normal://0
			return Rtt::RenderTypes::BlendType::kNormal;//0

		case   spine::BlendMode::BlendMode_Additive://1
			return Rtt::RenderTypes::BlendType::kAdditive;//1

		case   spine::BlendMode::BlendMode_Multiply://2
			return Rtt::RenderTypes::BlendType::kMultiply;//3

		case   spine::BlendMode::BlendMode_Screen://3
			return Rtt::RenderTypes::BlendType::kScreen;//2
		default:
			//����Ӧ�þ���һ��,���spine����ʱ�ⷢ���˸���
			return Rtt::RenderTypes::BlendType::kNormal;
			break;
		}
	}

	static bool AssignParen(lua_State * L, Display& display, DisplayObject* const displayO, GroupObject *const pParent, bool setInitProperty)
	{
		if (displayO == NULL
			|| pParent == NULL)
		{
			SpineLog(L, ESpineLog::Error, "AssignParen argument is null...");
			return false;
		}


		pParent->Insert(-1, displayO, false);
		displayO->AddedToParent(L, pParent);//spine���������ص��������κ����飬����û���κ�����...
		/*
		��Ϊ���ڲ����룬 ���Բ���Ҫ��ʼ��������� (����������ô��Ϊ��...)
		���������¼��ص���ʱ��, corona ���ǻ����ÿ�������luaTable ���ڽ��¼�֪ͨ��lua��,�������ʼ������,��ô�ͻ��ѿ�!!
		���� ������Ҫ�����,
		*/
		displayO->InitProxy(L);

		//������Щ����Ӧ��ֻ�ڴ���ʱ������, �����ĵ��ÿ��ܱȽϰ���...

		//ûɶ��
		if (setInitProperty)
		{
			// NOTE: V1 compatibility on an object can only be set on creation.
			const Rtt::DisplayDefaults& defaults = display.GetDefaults();
			bool isV1Compatibility = defaults.IsV1Compatibility();
			displayO->SetV1Compatibility(isV1Compatibility);

			if (!isV1Compatibility)
			{
				displayO->SetAnchorX(defaults.GetAnchorX());
				displayO->SetAnchorY(defaults.GetAnchorY());
			}

			// NOTE: This restriction should only be set on creation.
			// This is a performance optimization to make the initial restriction
			// check fast. Any subsequent checking can be more expensive.
			displayO->SetRestricted(display.IsRestricted());
		}

		//��Ϊmesh�����table����Ҫ���ظ�lua��, ���Բ���Ҫѹջ�����ԾͲ���Ҫ����
		//LuaProxy* proxy = displayO->GetProxy();
		//Rtt_ASSERT(proxy);
		//return proxy->PushTable(L);

		return true;
	}

	static void GetFullPath(const Rtt::Display& fDisplay, const Rtt::MPlatform::Directory& baseDir, const char * const filename, spine::String& filePathForFill)
	{
		Rtt_ASSERT(filePathForFill.isEmpty());
		Rtt::String filePath;
		if (baseDir != MPlatform::kUnknownDir)
		{
			fDisplay.GetRuntime().Platform().PathForFile(filename, baseDir, MPlatform::kTestFileExists, filePath);
		}
		else
		{
			filePath.Set(filename);
		}
		filePathForFill.append(filePath.GetString());
	}


	//��ȡһ�����·�� , ���紫��  fullPath = d:a/b/c/1.png  basepath=d:a/b ��ô pathForFill ����c/1.png
	static void GetRelativePath(const Rtt::Display & display, const char * const  fullPath, const Rtt::MPlatform::Directory & baseDir, Rtt::String & pathForFill)
	{
		auto ac = display.GetAllocator();
		auto fullLen = strlen(fullPath);
		spine::String basePath;

		GetFullPath(display, baseDir, "", basePath);

		auto basePathLen = strlen(basePath.buffer());
		//����fullPathΪ/a/b/c/file.txt��
		//���basePath����/a/b/c/����ôֱ��ȡ�䳤�ȣ����basePath����/a/b/c����ôȡ�䳤��+1
		//������ȡ�����·���ĵ�һ���ַ��Ͳ�����'/'
		if (basePath.buffer()[basePathLen - 1] != '/')
		{
			basePathLen++;
		}
		auto retBufferLen = fullLen - basePathLen + 1;// \0ռ��1λ
		char *retBuffer = (char *)Rtt_MALLOC(ac, (retBufferLen) * sizeof(char));
		memcpy(retBuffer, fullPath + basePathLen, retBufferLen - 1);
		retBuffer[retBufferLen - 1] = '\0';
		pathForFill.Set(retBuffer);
		Rtt_FREE(retBuffer);
	}


	//�ο� ShapeAdapterMesh::InitializeMesh ��д��  ȥ�����lua��ص�����
	static bool InitCoronaMesh
	(
		Rtt::TesselatorMesh & tesselator,
		SpineDataBuffer * spineDataBuffer
	)
	{

		spine::Vector<float >& inputUVs = *(spineDataBuffer->uvs);
		spine::Vector<float>& inputVertices = *(spineDataBuffer->verts);
		spine::Vector<unsigned short> & inputIndices = *(spineDataBuffer->indices);
		//TODO �Ż�  �˴��Ĵ���������������� Ӧ�ø���memcpy������ �ӿ쿽�����ٶȣ�����
		if (
			inputVertices.size() == 0
			|| inputUVs.size() == 0
			|| inputIndices.size() == 0
			|| inputUVs.size() != inputVertices.size()
			)
		{
			return false;
		}
		Rtt::ArrayVertex2& mesh = tesselator.GetMesh();
		mesh.Clear();
		Rtt_ASSERT(mesh.Length() == 0);
		//vertices
		//auto numVertices = inputVertices.size() / VERT_TWO;
		auto numVertices = inputVertices.size() >> 1;
		//mesh.Reserve(numVertices);//Ԥ����ռ�
		for (int i = 0; i < numVertices; ++i)
		{
			mesh.Append
			(
				{
				(Rtt_Real)inputVertices[i << 1],
				(Rtt_Real)inputVertices[(i << 1) + 1]
				}
			);
		}
		if (mesh.Length() < 3)
		{
			//SpineLog(L,ESpineLog::Error ,"display.newMesh() at least 3 pairs of (x;y) coordinates must be provided in 'vertices' parameter");
			return false;
		}


		//uv
		ArrayVertex2& UVs = tesselator.GetUV();
		//auto numUVs = inputUVs.size() / UV_TWO;
		auto numUVs = inputUVs.size() >> 1;
		for (int i = 0; i < numUVs; ++i)
		{
			UVs.Append
			(
				{
				(Rtt_Real)inputUVs[i << 1]	,
				(Rtt_Real)inputUVs[(i << 1) + 1]
				}
			);
		}


		//index
		TesselatorMesh::ArrayIndex& indices = tesselator.GetIndices();

		for (int i = 0; i < inputIndices.size(); ++i)
		{
			indices.Append(inputIndices[i]);
		}


		tesselator.Invalidate();
		tesselator.Update();
		return true;
	}

	static bool toFront(lua_State * L, GroupObject * const parent, DisplayObject * const o)
	{
		if (parent != NULL && o != NULL)
		{
			if (!o->IsVisible())
			{
				o->SetVisible(true);
			}
			parent->Insert(-1, o, false);
			return true;
		}
		else
		{
			CoronaLuaWarning(L, "DisplayObject:toFront() cannot be used on a snapshot group or texture canvas cache");
			return false;
		}
	}
	//�ο� ShapeAdapterMesh::update ��д�� ȥ�����lua��ص�����
	static bool UpdateCoronaMesh
	(
		lua_State *const  L,
		Rtt::ShapePath * const path,
		Rtt::TesselatorMesh *  const tesselator,
		SpineDataBuffer * spineDataBuffer
	)
	{
		if (tesselator == NULL)
		{
			return false;
		}
		auto & inputUVs = *(spineDataBuffer->uvs);
		auto & inputVertices = *(spineDataBuffer->verts);
		auto & inputIndices = *(spineDataBuffer->indices);
		bool updated = false;
		int pathInvalidated = 0;
		int observerInvalidated = 0;
		if (
			inputVertices.size() == 0
			|| inputUVs.size() == 0
			|| inputIndices.size() == 0
			|| inputUVs.size() != inputVertices.size()
			)
		{
			SpineLog(L, ESpineLog::Error, "UpdateCoronaMesh invalid input data ...");
			return false;
		}

		auto *  oldMesh = tesselator->GetMesh().WriteAccess();
		//auto numVertices = inputVertices.size() / VERT_TWO;
		auto numVertices = inputVertices.size() >> 1;
		if (numVertices == (U32)tesselator->GetMesh().Length())
		{
			updated = true;
			for (U32 i = 0; i < numVertices; ++i)
			{
				oldMesh[i].x = Rtt_FloatToReal(inputVertices[i << 1]);
				oldMesh[i].y = Rtt_FloatToReal(inputVertices[(i << 1) + 1]);
			}



			pathInvalidated = pathInvalidated | ClosedPath::kFillSource |
				ClosedPath::kStrokeSource;

			observerInvalidated = observerInvalidated | DisplayObject::kGeometryFlag |
				DisplayObject::kStageBoundsFlag |
				DisplayObject::kTransformFlag;

		}
		else
		{
			CoronaLuaWarning(L, "Vertices not updated: the amount of vertices in the mesh is not equal to the amount of vertices in the table");
		}


		//U32 numUVs = (U32)inputUVs.size() / UV_TWO;
		U32 numUVs = (U32)inputUVs.size() >> 1;
		Vertex2 *oldUvs = tesselator->GetUV().WriteAccess();
		if (numUVs == (U32)tesselator->GetUV().Length())
		{
			updated = true;
			for (U32 i = 0; i < numUVs; ++i)
			{
				oldUvs[i].x = Rtt_FloatToReal(inputUVs[i << 1]);
				oldUvs[i].y = Rtt_FloatToReal(inputUVs[(i << 1) + 1]);
			}
			pathInvalidated = pathInvalidated | ClosedPath::kFillSourceTexture;
			observerInvalidated = observerInvalidated | DisplayObject::kGeometryFlag;
		}
		else
		{
			CoronaLuaWarning(L, "UVS not updated: the amount of UVS in the mesh is not equal to the amount of UVS in the table");
		}

		U16 *oldIndices = tesselator->GetIndices().WriteAccess();
		U32 numIndices = inputIndices.size();
		if (numIndices == (U32)tesselator->GetIndices().Length())
		{
			for (U32 i = 0; i < numIndices; i++)
			{
				oldIndices[i] = (U16)inputIndices[i];
			}

			updated = true;
			pathInvalidated = pathInvalidated | ClosedPath::kFillSourceIndices;
			observerInvalidated = observerInvalidated | DisplayObject::kGeometryFlag |
				DisplayObject::kStageBoundsFlag |
				DisplayObject::kTransformFlag;
		}
		else
		{
			CoronaLuaWarning(L, "Indices not updated: the amount of Indices in the mesh is not equal to the amount of UVS in the table");
		}
		if (updated)
		{
			path->Invalidate(pathInvalidated);
			path->GetObserver()->Invalidate(observerInvalidated);
		}
		return true;
	}

	Rtt_INLINE	static void SpineColor2RttColor(
		lua_State *  L,
		bool premultipliedAlpha,
		spine::Color  & colBone,
		spine::Color  & colSlot,
		spine::Color  & colAttachment,
		Rtt::Color  & rttColor
	)
	{

		/*
		�ڴ沼�֣����Է�һ����صĴ��룬
		U8 r;
		U8 g;
		U8 b;
		U8 a;
		*/
		//�����ɫ������ʹ��solar2D-spine��صķ���

		//��˼���� �ڴ沼��
		// Rtt_RenderTypes.h �� union RGBA�Ľṹ

		auto alpha = colBone.a * colSlot.a* colAttachment.a;
		auto multiplier = premultipliedAlpha ? alpha : 1.0f;

#if Rtt_BIG_ENDIAN
		rttColor =
			(((U32)(alpha* 255.0f)))
			| (((U32)(multiplier*colBone.b * colSlot.b*colAttachment.b* 255.0f)) << 8)
			| (((U32)(multiplier*colBone.g * colSlot.g*colAttachment.g * 255.0f)) << 16)
			| (((U32)(multiplier*colBone.r *  colSlot.r*colAttachment.r*255.0f)) << 24);
#else
		rttColor =
			(((U32)(alpha* 255.0f)) << 24)
			| (((U32)(multiplier*colBone.b * colSlot.b*colAttachment.b* 255.0f)) << 16)
			| (((U32)(multiplier*colBone.g * colSlot.g*colAttachment.g * 255.0f)) << 8)
			| (((U32)(multiplier*colBone.r *  colSlot.r*colAttachment.r*255.0f)))
			;
#endif
	}



	static Rtt::BitmapPaint * NewBitmapPaint(lua_State * L, const Rtt::Display & diaplay, TextureLoadArgs * loadArgs)
	{
		if (loadArgs != NULL)
		{
			Rtt::SharedPtr< Rtt::TextureResource > pTexture =
				diaplay.GetTextureFactory().FindOrCreate
				(
					loadArgs->GetRelativePath().GetString(),
					loadArgs->GetBaseDir(),
					Rtt::PlatformBitmap::kIsNearestAvailablePixelDensity, false
				);
			//��RemoveMesh ���paint�����Ƴ�������
			return  Rtt_NEW(diaplay.GetAllocator(), Rtt::BitmapPaint(pTexture));
		}
		else
		{
			SpineLog(L, ESpineLog::Error, "NewBitMapPaint error ");
			return NULL;
		}
	}

	
	Rtt_INLINE static  bool EndWith(const spine::String & mainStr, const spine::String & subStr)
	{
		if (mainStr.length() >= subStr.length())
		{
			return 0==memcmp(mainStr.buffer() + mainStr.length() - subStr.length(), subStr.buffer(), subStr.length());
		}
		else
		{
			return false;
		}
	}
	Rtt_INLINE static  bool IsJsonFile(const spine::String & path)
	{
		static spine::String strJson = ".json";//Ӳ����
		return EndWith(path, strJson);
	}
	Rtt_INLINE static bool IsSkelFile(const spine::String & path)
	{
		static spine::String strSkel = ".skel";//Ӳ����
		return EndWith(path, strSkel);
	}

	class SpineAnimationEvent : public VirtualEvent
	{
	public:
		typedef VirtualEvent Super;
		typedef SpineAnimationEvent Self;
	public:

		int _eventType;
		spine::String   _name;
		SpineAnimationEvent(int eventType, const spine::String& str) :
			_eventType(eventType),
			_name(str.length() == 0 ? "" : str)
		{ 	}

		virtual const char* Name() const  override
		{
			return "Spine-Cpp AnimationEvent";
		}
		virtual int Push(lua_State *L) const
		{
			if (Rtt_VERIFY(Super::Push(L)))
			{
				lua_pushnumber(L, _eventType);
				lua_setfield(L, -2, "type");
				lua_pushstring(L, _name.buffer());
				lua_setfield(L, -2, "name");
			}
			return 1;
		}

	};

	class CoronaSpineAnimationListenerObject :public spine::AnimationStateListenerObject
	{
		LuaResource * & _luaResourceCallback;
	public:
		CoronaSpineAnimationListenerObject(LuaResource * & callBack) :_luaResourceCallback(callBack) {}
		virtual void callback(AnimationState *state, EventType type, TrackEntry *entry, Event *event) override
		{
			if (_luaResourceCallback != NULL)
			{
				_luaResourceCallback->DispatchEvent(
					SpineAnimationEvent
					(
					(int)type
						, entry == NULL ? "" : entry->getAnimation()->getName().buffer()
					)
				);
			}
		}
	};

	class LuaCppSpineCppObjectProxyVTable :public LuaGroupObjectProxyVTable
	{

	public:
		typedef LuaCppSpineCppObjectProxyVTable Self;
		typedef LuaGroupObjectProxyVTable Super;

	public:
		static const Self& Constant() {
			static const Self kVTable;
			return kVTable;
		}


#pragma region spine��ص�apiʵ��
		Rtt_INLINE static SpineCppObject * getSpine(lua_State * L)
		{

			//Rtt_WARN_SIM_PROXY_TYPE(L, 1, SpineCppObject);	//  Ҫinclude ��Ӧ ͷ�ļ� ����ʹ������꣬��ʱ�䣬�Ȳ���������ˡ�
			SpineCppObject *sp = (SpineCppObject*)LuaProxy::GetProxyableObject(L, 1);
			if (sp == NULL)
			{
				SpineLog(L, ESpineLog::Error, "getSpine error call spineMethod use ':'  instead of  '.' ");
			}
			return sp;
		}

		static int ClearunusedMeshCache(lua_State *L)
		{
			auto sp = getSpine(L);
			if (sp != NULL)
			{
				SpineLog(L, ESpineLog::Info, "ClearunusedMeshCache...");
				sp->SetFlagClearUnusedMeshCache(true);
				return 0;
			}
			else
			{
				SpineLog(L, ESpineLog::Error, "ClearunusedMeshCache error ...");
				return 0;
			}
		}

		static int Pause(lua_State *L)
		{
			auto sp = getSpine(L);
			if (sp != NULL)
			{
				SpineLog(L, ESpineLog::Info, "pause spine ...");
				sp->GetSpineAnimationState()->setTimeScale(0.0f);
				return 0;
			}
			else
			{
				SpineLog(L, ESpineLog::Error, "pause error ...");
				return 0;
			}
		}
		static int Resume(lua_State *L)
		{
			auto sp = getSpine(L);
			if (sp != NULL)
			{
				SpineLog(L, ESpineLog::Info, "Resume spine ...");
				sp->GetSpineAnimationState()->setTimeScale(1.0f);
				return 0;
			}
			else
			{
				SpineLog(L, ESpineLog::Error, "Resume error ...");
				return 0;
			}
		}

		static int  GetCurAnimationName(lua_State * L)
		{
			const auto  argIndex = 2;
			auto * sp = getSpine(L);
			if (lua_isnumber(L, argIndex))
			{
				auto trackIndex = lua_tonumber(L, argIndex);

				auto  * entry = sp->GetSpineAnimationState()->getCurrent(trackIndex);
				if (entry != NULL)
				{
					auto * ani = entry->getAnimation();
					if (ani == NULL)
					{
						SpineLog(L, ESpineLog::Warning, " this is bug ? cur animation is nulll");
						return 0;
					}
					//lua_pushstring()
					lua_pushinteger(L, 1111);
					//lua_pushfstring(L, ani->getName().buffer());
					return  0;
				}
				else
				{
					SpineLog(L, ESpineLog::Warning, " the track has no entry index=%d", trackIndex);
					return 0;
				}
			}
			else
			{
				SpineLog(L, ESpineLog::Warning, "first argument is not number... ");
				return 0;
			}
		}
		static int SetClipperEnable(lua_State * L)
		{
			const auto  argIndex = 2;
			auto * sp = getSpine(L);
			if (lua_isboolean(L, argIndex))
			{
				auto bCliperEnable = lua_toboolean(L, argIndex);
				sp->SetClipperEnable(bCliperEnable);
				return 0;
			}
			else
			{
				SpineLog(L, ESpineLog::Warning, "first argument is not boolean... ");
				return 0;
			}
			return 0;
		}
		static int SetAnimation(lua_State * L)
		{
			//lua����ð�ŵ��ã� ��1��������table�����Բ�����index ��2��ʼ
			const auto argTrackIndex = 2;
			const auto argNameIndex = 3;
			const auto argIsLoop = 4;

			auto * sp = getSpine(L);
			if (lua_isnumber(L, argTrackIndex)
				&& lua_isstring(L, argNameIndex)
				&& lua_isboolean(L, argIsLoop))
			{
				int trackIndex = lua_tonumber(L, argTrackIndex);
				std::string aniName = std::string(lua_tostring(L, argNameIndex));
				bool isLoop = lua_toboolean(L, argIsLoop);
				spine::String strAniName(aniName.c_str());

				//��ѯ�������Ƿ���ڴ˶���
				auto *animationEntry = sp->GetSpineSkeletonData()->findAnimation(strAniName);
				if (animationEntry == NULL)
				{
					//SpineLog(L, ESpineLog::Warning, "can not find animation ... name =%s", strAniName);
					return 0;
				}
			}
			else
			{
				SpineLog(L, ESpineLog::Error, "setAnimation error argument ...");
				return 0;
			}
			return 0;
		}

		static int SetSkin(lua_State * L)
		{
			//lua����ð�ŵ��ã� ��1��������table�����Բ�����index ��2��ʼ
			const auto argNameIndex = 2;

			auto * sp = getSpine(L);
			if (lua_isstring(L, argNameIndex))
			{
				auto *skinName = lua_tostring(L, argNameIndex);
				spine::String strSkinName(skinName);
				//��ѯ�������Ƿ���ڴ�Ƥ���͸���Ƥ����һ����λ
				sp->GetSpineSkeleton()->setSkin(skinName);
				sp->GetSpineSkeleton()->setSlotsToSetupPose();
				sp->GetSpineAnimationState()->apply(*sp->GetSpineSkeleton());
			}
			else
			{
				SpineLog(L, ESpineLog::Error, "setAnimation error argument ...");
				return 0;
			}
			return 0;
		}

		static int SetPremultipliedAlpha(lua_State * L)
		{
			//lua����ð�ŵ��ã� ��1��������table�����Բ�����index ��2��ʼ
			const auto argNameIndex = 2;

			auto * sp = getSpine(L);
			if (lua_isboolean(L, argNameIndex))
			{
				bool pmaValue = lua_toboolean(L, argNameIndex);
				sp->SetPMA(pmaValue);
#if Rtt_WIN_ENV
				SpineLog(L, ESpineLog::Info, "SetPremultipliedAlpha value=%d", pmaValue);
#endif
				return 0;
			}
			else
			{
				SpineLog(L, ESpineLog::Error, "setAnimation error argument ...");
				return 0;
			}
		}

		static int SetSpeed(lua_State * L)
		{
			//lua����ð�ŵ��ã� ��1��������table�����Բ�����index ��2��ʼ
			const auto argNameIndex = 2;

			auto * sp = getSpine(L);
			if (lua_isnumber(L, argNameIndex))
			{
				float speed = lua_tonumber(L, argNameIndex);
				sp->GetSpineAnimationState()->setTimeScale(speed);
			}
			else
			{
				SpineLog(L, ESpineLog::Error, "setAnimation error argument ...");
				return 0;
			}
			return 0;
		}


		static int getAnimationDuration(lua_State * L)
		{
			const auto argTrackIndex = 2;
			const auto argNameIndex = 3;
			const auto argIsLoop = 4;

			auto * sp = getSpine(L);
			int trackIndex = lua_tonumber(L, argTrackIndex);
			std::string aniName = std::string(lua_tostring(L, argNameIndex));
			bool isLoop = lua_toboolean(L, argIsLoop);
			float d = sp->getDuration(trackIndex, aniName, isLoop);
			lua_pushnumber(L, d);
			return 1;
		}
		static int resetTime(lua_State * L)
		{
			auto * sp = getSpine(L);
			if (lua_isnumber(L, 2))
			{
				float request_t = lua_tonumber(L, 2);
				//ȡģ����ֹԽ��
				request_t = MathUtil::fmod(request_t, sp->getCurrentDuration());
				float current_t = sp->GetSpineSkeleton()->getTime();
				//ȡģ�����㵱ǰʱ����һ������ѭ�������λ��
				current_t = MathUtil::fmod(current_t, sp->getCurrentDuration());
				float offset_t = request_t - current_t;
				sp->GetSpineSkeleton()->update(offset_t);
				sp->GetSpineAnimationState()->update(offset_t);
			}
			else
			{
				SpineLog(L, ESpineLog::Error, "setAnimation error argument ...");
			}

			return 0;
		}
		static int getAnimationTime(lua_State * L)
		{
			auto * sp = getSpine(L);
			float d = sp->getAnimationTime();
			lua_pushnumber(L, d);
			return 1;
		}

		Rtt_INLINE static  void setArrayString(lua_State * L, int index, const char * value)
		{
			Rtt_ASSERT(lua_istable(L, -1));
			lua_pushnumber(L, index);
			lua_pushlstring(L, value, strlen(value));
			lua_rawset(L, -3);
		}

		Rtt_INLINE static 	void setNumberProperty(lua_State *L, const char *  key, Rtt_Real value)
		{
			Rtt_ASSERT(lua_istable(L, -1));
			lua_pushlstring(L, key, strlen(key));
			lua_pushnumber(L, Rtt_RealToFloat(value));
			lua_settable(L, -3);
		}
		Rtt_INLINE static 	void setStringProperty(lua_State *L, const char *  key, const  char *  value)
		{
			Rtt_ASSERT(lua_istable(L, -1));
			lua_pushlstring(L, key, strlen(key));
			lua_pushlstring(L, value, strlen(value));
			lua_settable(L, -3);
		}

		static int SetAnimationEventCallback(lua_State * L)
		{
			auto * sp = getSpine(L);
			if (sp != NULL)
			{
				auto setRet = sp->SetAnimationCallback(L);
				if (!setRet)
				{
					SpineLog(L, ESpineLog::Warning, "set callback error ...");
					return 0;
				}
				return 0;
			}
			else {
				SpineLog(L, ESpineLog::Warning, "error");
				return 0;
			}
			return 0;
		}
		int PushSpineAbout(lua_State *L, const GroupObject& o, const char *key) const
		{
			auto retSuper = Super::PushMethod(L, o, key);

			int result = 1;

			static const char * keys[] =
			{
				"pause",			// 0
				"resume",			// 1
				"setAnimation",			// 2
				"setAnimationEventCallback",	// 3
				"setSkin",                  //4
				"setSpeed",             //5-���ò����ٶ�
				"addOnCompleteListener", //6-spine������ɺ�Ļص�����
				"clearOnCompleteListener", //7-spine������ɺ�Ļص�����
				"addFrameListener",  //8-ÿ֡����ʱ��ļ���
				"clearFrameListener",  //9-ÿ֡����ʱ��ļ���
				"getAnimationDuration", //10-ĳ�������Ĳ�����ʱ��
				"resetTime", //11-���ò��ŵ�ĳ��ʱ��
				"getAnimationTime", //12-��ȡ���������ĵ�ǰʱ��
				"setClipperEnable",	//13
				"getAnimationName", //14
				"vertNum",//15
				"debugInfo",//16
				"clearMeshCache",//17 ���mesh�����
				"setPremultipliedAlpha",//18 �����Ƿ���pma
			};
			static const int numKeys = sizeof(keys) / sizeof(const char *);
			static StringHash sHash(*Rtt::LuaContext::GetAllocator(L), keys, numKeys, numKeys + 1, numKeys + 1, 1, __FILE__, __LINE__);
			StringHash *hash = &sHash;

			int index = hash->Lookup(key);
			switch (index)
			{
			case 0:
			{
				Lua::PushCachedFunction(L, Self::Pause);
				result = 1;
			}
			break;
			case 1:
			{
				Lua::PushCachedFunction(L, Self::Resume);
				result = 1;
			}
			break;

			case 2:
			{
				Lua::PushCachedFunction(L, Self::SetAnimation);
				result = 1;
			}
			break;
			case 3:
			{
				Lua::PushCachedFunction(L, Self::SetAnimationEventCallback);
				result = 1;
			}
			break;
			case 4:
			{
				Lua::PushCachedFunction(L, Self::SetSkin);
				result = 1;
			}
			break;
			case 5:
			{
				Lua::PushCachedFunction(L, Self::SetSpeed);
				result = 1;
			}
			break;
			case 6:// TODO 
			{

				result = 1;
			}
			break;
			case 7:// TODO 
			{

				result = 1;
			}
			break;
			case 8://TODO
			{

				result = 1;
			}
			break;
			case 9://TODO
			{

				result = 1;
			}
			break;
			case 10:
			{
				Lua::PushCachedFunction(L, Self::getAnimationDuration);
				result = 1;
			}
			break;
			case 11:
			{
				Lua::PushCachedFunction(L, Self::resetTime);
				result = 1;
			}
			break;
			case 12:
			{
				Lua::PushCachedFunction(L, Self::getAnimationTime);
				result = 1;
			}
			break;
			case 13:
			{
				Lua::PushCachedFunction(L, Self::SetClipperEnable);
				result = 1;
			}
			break;
			case 14://TODO������ֵ�ĳ�Ա����, ��Ҫ�ûص��ķ�ʽ  ʵ�ֲο� requestLocation ���api 2022��3��25��16:06:29
			{
				Lua::PushCachedFunction(L, Self::GetCurAnimationName);
				result = 1;
			}
			break;
			case 15://��ȡ��������field����
			{
				lua_pushnumber(L, ((SpineCppObject*)(&o))->GetSpineVertNum() >> 1);
				result = 1;
			}

			break;
			case 16://��ȡtable����field����
			{
				auto * sp = ((SpineCppObject*)(&o));
				spine::Vector<Skin *> & skins = sp->GetSpineSkeletonData()->getSkins();
				lua_createtable(L, skins.size(), 4);//Ԥ����x��arrayԪ�� ,Ԥ����4����arrayԪ��(��ֵ) 4�������,���Ը��ݾ��尸���޸�
				setNumberProperty(L, "vertNum", sp->GetSpineVertNum() >> 1);
				setNumberProperty(L, "clipperNum", sp->GetClipperAttachmentCount());
				setNumberProperty(L, "batchNum", sp->GetBatchNum());
				setNumberProperty(L, "saveByBatchNum", sp->GetVisiableAttachNum() - sp->GetBatchNum());
				setNumberProperty(L, "visiableAttachmentNum", sp->GetVisiableAttachNum());
				setNumberProperty(L, "cacheMeshNum", sp->GetMeshCacheNum());
				setNumberProperty(L, "activeMeshNum", sp->GetActiveMeshCacheNum());
				setNumberProperty(L, "childrenNum", sp->GetChildrenNum());
				setNumberProperty(L, "textureNum", sp->GetTextureNum());

				setStringProperty(L, "name", "hi this is test string  for field value ");
				for (int i = 0; i < skins.size(); ++i)
				{
					Skin *sk = skins[i];
					setArrayString(L, i + 1, sk->getName().buffer());
				}
			}
			break;
			case 17://17 ���mesh�����
			{
				Lua::PushCachedFunction(L, Self::ClearunusedMeshCache);
				result = 1;
			}
			break;
			case 18://18 ����pma
			{
				Lua::PushCachedFunction(L, Self::SetPremultipliedAlpha);
				result = 1;
			}
			break;
			default:
			{
				result = 0;
			}
			break;
			}
			return result;

		}
#pragma endregion

		//��spine��api

		//����
		virtual int ValueForKey(lua_State *L, const MLuaProxyable& object, const char key[], bool overrideRestriction = false) const {
			int result = 0;

			Rtt_WARN_SIM_PROXY_TYPE(L, 1, GroupObject);
			const GroupObject& o = static_cast<const GroupObject&>(object);

			if (lua_type(L, 2) == LUA_TNUMBER)
			{
				result = PushChild(L, o);
			}
			else if (key)
			{
				result = PushSpineAbout(L, o, key);

				if (0 == result)
				{
					result = Super::ValueForKey(L, object, key, overrideRestriction);
				}
			}

			return result;
		}
		virtual bool SetValueForKey(lua_State *L, MLuaProxyable& object, const char key[], int valueIndex) const {

			if (!key) { return false; }

			bool result = true;

			Rtt_WARN_SIM_PROXY_TYPE(L, 1, GroupObject);

			if (0 == strcmp(key, "anchorChildren"))
			{
				GroupObject& o = static_cast<GroupObject&>(object);

				o.SetAnchorChildren(!!lua_toboolean(L, valueIndex));

#if defined( Rtt_DEBUG ) || defined( Rtt_AUTHORING_SIMULATOR )
				if (o.IsV1Compatibility())
				{
					CoronaLuaWarning(L, "group.anchorChildren is only supported in graphics 2.0. Your mileage may vary in graphicsCompatibility 1.0 mode");
				}
#endif
			}
			else
			{
				result = Super::SetValueForKey(L, object, key, valueIndex);
			}
			return result;

		}
		virtual const LuaProxyVTable& Parent() const
		{
			return Super::Constant();
		}


	};


	class CoronaTextureLoader : public  TextureLoader
	{
		Rtt::Display& _display;
		Rtt::MPlatform::Directory _baseDir;
		int _textureNum = 0;
	public:
		Rtt_INLINE int  GetLoadedTextureNum() { return _textureNum; }
		CoronaTextureLoader(Rtt::Display & diaplay, Rtt::MPlatform::Directory baseDir) : _display(diaplay), _baseDir(baseDir) {}
		virtual void load(AtlasPage& page, const spine::String& path)
		{
			_textureNum++;
			Rtt::Runtime  & runtime = _display.GetRuntime();
			Rtt::TextureFactory& factory = runtime.GetDisplay().GetTextureFactory();

			Rtt::String relativePath(_display.GetAllocator());
			GetRelativePath(_display, path.buffer(), _baseDir, relativePath);

			bool bExists = _display.GetRuntime().Platform().FileExists(path.buffer());


			Rtt::SharedPtr< Rtt::TextureResource > pTexture = factory.FindOrCreate(relativePath.GetString(), _baseDir, Rtt::PlatformBitmap::kIsNearestAvailablePixelDensity, false);

			auto textureKey = pTexture->GetCacheKey();
			auto * textureLoadArgs = Rtt_NEW
			(
				_display.GetAllocator(),
				TextureLoadArgs(_display.GetAllocator(), _baseDir, relativePath.GetString(), textureKey)
			);
			page.setRendererObject(textureLoadArgs); //���������new������ָ��, ���»�ͨ��unload�����ָ�뷵��
		}
		virtual void unload(void* texture) {
			//�ͷż�����
			auto * textureLoadArgs = (TextureLoadArgs*)texture;
			_display.GetRuntime().GetDisplay().GetTextureFactory().Release(textureLoadArgs->GetTextureCacheKey());
			Rtt_DELETE(textureLoadArgs);
		}
	};


	class StringHashValueMap
	{
	private:
		typedef unsigned int  HashValueType;
		struct SStrHashValuePair
		{
			const char * str;
			HashValueType hashValue;
		};
		typedef spine::Vector<SStrHashValuePair > vecStrHash;
		vecStrHash    _vec;
		Rtt_INLINE HashValueType  BKDRHash(const char* str)
		{
			HashValueType seed = 131;
			HashValueType hash = 0;
			while (*str)
			{
				hash = hash * seed + (*str++);
			}
			return hash & 0x7FFFFFFF;
		}

		bool CheckHashCollision(HashValueType  value)
		{
			for (int i = 0; i < _vec.size(); ++i)
			{
				if (value == _vec[i].hashValue)
				{
					return true;
				}
			}
			return false;
		}
	public:
		StringHashValueMap(/*Rtt_Allocator *  ac*/)
		{
		};
		~StringHashValueMap()
		{
		};
	public:
		HashValueType GetHashKey(lua_State * L, const char *  str)
		{
			//static std::string strBuffer;
			HashValueType  retV = 0;
			bool finded = false;
			for (int i = 0; i < _vec.size(); ++i)
			{
				if (_vec[i].str == str)
				{
					retV = _vec[i].hashValue;
					finded = true;
					break;
				}
			}
			if (finded)
			{
				return retV;
			}
			else
			{
				retV = BKDRHash(str);
				//  ���·�����ȽϺ㶨���������hash��ͻ �ڿ����׶μ��ɱ�¶����
				if (CheckHashCollision(retV))
				{
					SpineLog(L, ESpineLog::Error, "atlasFileName:%s hash collision in spine atlas file name ...please rename atlas filename. ", str);
				}
				//_vec.insert({ str ,retV });
				_vec.add({ str , retV });
			}
			return retV;
		}
	};
	class MeshPool
	{
		struct MeshInfoPair {
			SpineShapeObj* shapeObj;
			Rtt::TesselatorMesh * meshObj;
			const std::string  * strShapeObjPaintTextureCacheKey;
		public:
			MeshInfoPair() :
				shapeObj(NULL),
				meshObj(NULL),
				strShapeObjPaintTextureCacheKey(NULL)
			{	}
			MeshInfoPair(const MeshInfoPair& info)
			{
				this->CopyInfo(info);
			}
			Rtt_INLINE void CopyInfo(const MeshInfoPair & info)
			{
				this->shapeObj = info.shapeObj;
				this->meshObj = info.meshObj;
				this->strShapeObjPaintTextureCacheKey = info.strShapeObjPaintTextureCacheKey;
			}
		};

		typedef spine::Vector<MeshInfoPair> ListMeshInfo;
		ListMeshInfo _listPool;//Ƶ��insert remove
		ListMeshInfo _listRent;

	public:
		int GetMeshTotalNum()
		{
			return _listRent.size() + _listPool.size();
		}
		ListMeshInfo &  GetRentList()
		{
			return this->_listRent;
		}
	private:

		Rtt_INLINE bool  TryRentSuitableMesh(
			TextureLoadArgs * textureLoadArgs,
			int needVertNum,
			int needIndexNum,

			MeshInfoPair& info
		)
		{


			//ͬ�����Ǳ�����ʹ���ڴ��������������Ч�ʸ��ߣ�
			//��spine��vectorʵ����new�Ķ����ǿ����ģ� ����new��λ�� ��ָ�����ڴ�λ�ã�����buffer�����ҵ����˿������캯��
			MeshInfoPair * foundedInfo = NULL;
			int founedIndex = -1;
			for (int i = 0; i < _listPool.size(); ++i)
			{

				MeshInfoPair &  info = _listPool[i];
				if (
					info.meshObj->GetMesh().Length() == needVertNum
					&& info.meshObj->GetIndices().Length() == needIndexNum
					&& info.strShapeObjPaintTextureCacheKey == &(textureLoadArgs->GetTextureCacheKey())
					)
				{
					foundedInfo = &info;
					founedIndex = i;
					break;
				}
			}
			if (foundedInfo != NULL)
			{
				info.CopyInfo(*foundedInfo);
				_listRent.add(*foundedInfo);
				_listPool.removeAt(founedIndex);
				return true;
			}
			else
			{
				return false;
			}
		}
	public:
		SpineShapeObj *  GetShapeObj
		(
			SpineCppObject * pSpine,
			TextureLoadArgs * textureLoadArgs,
			SpineDataBuffer * spineDataBuffer,
			Rtt::TesselatorMesh * & meshObj
		)
		{

			int needVertNum = spineDataBuffer->verts->size() >> 1;
			int needIndexNum = spineDataBuffer->indices->size();//����index�������ж�,������Ϊindex������һ�µ���mesh����ʧ��.

			MeshInfoPair info;
			if (TryRentSuitableMesh(textureLoadArgs, needVertNum, needIndexNum, info))
			{
				meshObj = info.meshObj;
				return info.shapeObj;
			}
			else
			{
				auto shapeObj = pSpine->CreateMeshAndAssignParent
				(
					spineDataBuffer,
					meshObj,
					textureLoadArgs
				);
				info.meshObj = meshObj;
				info.shapeObj = shapeObj;
				info.strShapeObjPaintTextureCacheKey = &(textureLoadArgs->GetTextureCacheKey());

				_listRent.add(info);
				return shapeObj;
			}
		}
		void ReleaseAndHideAllMesh(SpineCppObject * sp)
		{
			for (int i = 0; i < _listRent.size(); ++i)
			{
				_listPool.add(_listRent[i]);
				sp->Release(sp->Find(*_listRent[i].shapeObj));
			}
			_listRent.clear();

			for (int i = 0; i < _listPool.size(); ++i)
			{
				if (_listPool[i].shapeObj->IsVisible())
				{
					_listPool[i].shapeObj->SetVisible(false);
				}
			}
		}

		void ClearMeshs(SpineCppObject * pSpine)
		{
			ReleaseAndHideAllMesh(pSpine);
			ClearUnusedMeshs(pSpine);
		}
		void ClearUnusedMeshs(SpineCppObject * pSpine)
		{
			for (int i = 0; i < _listPool.size(); ++i)
			{
				_listPool[i].shapeObj->SetParent(pSpine);//����shapeObj������ʱ���Ҳ���stage������ɶ��Դ���
				pSpine->RemoveMesh(_listPool[i].shapeObj);
			}
			_listPool.clear();
		}
	};



	bool SpineCppObject::SetAnimationCallback(lua_State * L)
	{
		if (_luaResourceOnAnimationCallback != NULL)
		{
			SpineLog(L, ESpineLog::Warning, "repeat set spine animation callback ...");
			return false;
		}
		if (lua_isfunction(L, 2))
		{
			_luaResourceOnAnimationCallback = Rtt_NEW(_ac,
				Rtt::LuaResource(LuaContext::GetContext(L)->LuaState(), 2));
			return true;
		}
		else
		{
			SpineLog(L, ESpineLog::Warning, "set animation call failed ..");
			return false;
		}
	}

	SpineCppObject::SpineCppObject
	(
		lua_State * L,
		Rtt::Display & display,
		Rtt_Allocator* ac,
		const  Rtt::MPlatform::Directory  & baseDir,
		const std::string &  atlasPath,
		const std::string &  dataPath,//.skel����.json
		bool premultipliedAlpha,
		const float   posX,
		const float   poxY,
		const float   scaleX,
		const float   scaleY,
		const std::string & skin,
		const std::string & animation,
		int trackIndex,
		bool isLoop,
		float defaultMix
	) :
		GroupObject(display.GetAllocator(), NULL),//newSpine��ʱ���ⲿ��SetParent�� ��������������groupObject���ڲ���������һ��ParentStageObject
		_L(L),
		_ac(ac),
		_display(&display),
		_baseDir(baseDir),
		_premultipliedAlpha(premultipliedAlpha),
		_textureLoader(Rtt_NEW(_ac, CoronaTextureLoader(display, baseDir)))
	{

		//�ڴ������
		((CoronaSpineExtension*)SpineExtension::getInstance())->UpdateCoronaAlloctor(ac);



		//�ļ������ڸ���Σ� �ⲿ�Ѿ��ж�Ӧ���ж���

		spine::String atlasFullPath; GetFullPath(display, baseDir, atlasPath.c_str(), atlasFullPath);
		spine::String dataFullPath; GetFullPath(display, baseDir, dataPath.c_str(), dataFullPath);
		_atlas = Rtt_NEW(_ac, spine::Atlas(atlasFullPath, _textureLoader, true));
		//TODO ���ݴ�����ļ���׺��������ʹ��json���� ���Ƕ����Ƽ���

		if (IsJsonFile(dataFullPath))
		{
			SkeletonJson json(_atlas);
			_skeletonData = json.readSkeletonDataFile(dataFullPath);
		}
		else if (IsSkelFile(dataFullPath))
		{
			SkeletonBinary binary(_atlas);
			_skeletonData = binary.readSkeletonDataFile(dataFullPath);
		}
		else 
		{
			CoronaLuaWarning(_L, "unvalid spine data file path=%s",dataFullPath.buffer());
			_skeletonData = NULL;
		}
		assert(_skeletonData);

		//���ع���
		_skeleton = Rtt_NEW(_ac, spine::Skeleton(_skeletonData));
		assert(_skeleton != NULL);

		//���ض�������
		_aniStateData = Rtt_NEW(_ac, spine::AnimationStateData(_skeletonData));
		assert(_aniStateData != NULL);

		_aniStateData->setDefaultMix(defaultMix);
		_aniState = Rtt_NEW(_ac, spine::AnimationState(_aniStateData));
		_clipper = Rtt_NEW(_ac, spine::SkeletonClipping());
		_spineDataBuffer = Rtt_NEW(_ac, spine::SpineDataBuffer(display));
		_spineDataBufferBatch = Rtt_NEW(_ac, spine::SpineDataBuffer(display));
		_meshPool = Rtt_NEW(_ac, MeshPool());

		_skeleton->setScaleX(scaleX);
		_skeleton->setScaleY(-scaleY);
		_skeleton->setPosition(posX, poxY);
		_skeleton->setSkin(spine::String(skin.c_str()));
		_aniState->setAnimation(trackIndex, spine::String(animation.c_str()), isLoop);


		_luaResourceOnAnimationCallback = NULL;
		_coronaSpineAnimationListenerObject = Rtt_NEW(_ac, CoronaSpineAnimationListenerObject(_luaResourceOnAnimationCallback));

		(*_display).GetScene().AddActiveUpdatable(this);
		_aniState->setListener((CoronaSpineAnimationListenerObject*)_coronaSpineAnimationListenerObject);
	}

	float SpineCppObject::getCurrentDuration()
	{
		//TODO 
		return 0.0f;
	};


	//TODO  ��Ҫʹ��string����--. 
	float SpineCppObject::getDuration(int trackIndex, std::string& animation, bool isLoop)
	{
		CoronaLuaWarning(_L, "TODO: get duration for animation name=%s isLoop=%d ", animation.c_str(), isLoop);
		return 0;
	};

	float SpineCppObject::getAnimationTime()
	{
		//TODO 
		return  0.0f;
	};

	void SpineCppObject::addCompleteCallback() {

	};

	void SpineCppObject::FinalizeSelf(lua_State *L)
	{
		UnloadSpine();

		StageObject *stage = GetStage();
		if (stage)
		{
			stage->GetScene().RemoveActiveUpdatable(this);
		}
		//(*_display).GetScene().RemoveActiveUpdatable(this);
		Super::FinalizeSelf(L);
	}

	bool SpineCppObject::UpdateTransform(const Rtt::Matrix& parentToDstSpace)
	{
		return Super::UpdateTransform(parentToDstSpace);

	}

	void SpineCppObject::Prepare(const Rtt::Display& display)
	{
		Super::Prepare(display);
	}

	void SpineCppObject::Translate(Rtt::Real deltaX, Rtt::Real deltaY)
	{
		Super::Translate(deltaX, deltaY);
	}

	void SpineCppObject::GetSelfBoundsForAnchor(Rtt::Rect& rect) const
	{
		Super::GetSelfBoundsForAnchor(rect);
	}

	bool SpineCppObject::HitTest(Rtt::Real contentX, Rtt::Real contentY)
	{
		return 	Super::HitTest(contentX, contentY);
	}

	void SpineCppObject::DidUpdateTransform(Rtt::Matrix& srcToDst)
	{
		Super::DidUpdateTransform(srcToDst);
	}

	void SpineCppObject::Scale(Rtt::Real sx, Rtt::Real sy, bool isNewValue)
	{
		Super::Scale(sx, sy, isNewValue);
	}

	void SpineCppObject::Rotate(Rtt::Real deltaTheta)
	{
		Super::Rotate(deltaTheta);
	}

	void SpineCppObject::DidMoveOffscreen()
	{
		Super::DidMoveOffscreen();
	}

	void SpineCppObject::WillMoveOnscreen()
	{
		Super::WillMoveOnscreen();
	}

	bool SpineCppObject::CanCull() const
	{
		return false;
	}

	void SpineCppObject::InitProxy(lua_State *L)
	{
		Super::InitProxy(L);
	}

	Rtt::LuaProxy* SpineCppObject::GetProxy() const
	{
		return Super::GetProxy();
	}

	void SpineCppObject::ReleaseProxy()
	{
		Super::ReleaseProxy();
	}

	void SpineCppObject::AddedToParent(lua_State * L, Rtt::GroupObject * parent)
	{
		Super::AddedToParent(L, parent);
	}

	void SpineCppObject::RemovedFromParent(lua_State * L, Rtt::GroupObject * parent)
	{
		Super::RemovedFromParent(L, parent);
	}

	const Rtt::LuaProxyVTable& SpineCppObject::ProxyVTable() const
	{

		return LuaCppSpineCppObjectProxyVTable::Constant();
	}

	Rtt::GroupObject* SpineCppObject::AsGroupObject()
	{
		return this;
	}

	void SpineCppObject::WillDraw(Rtt::Renderer& renderer) const
	{
		Super::WillDraw(renderer);
	}

	void SpineCppObject::DidDraw(Rtt::Renderer& renderer) const
	{
		Super::DidDraw(renderer);
	}

	void SpineCppObject::SetSelfBounds(Rtt::Real width, Rtt::Real height)
	{
		Super::SetSelfBounds(width, height);
	}

	bool SpineCppObject::ShouldOffsetWithAnchor() const
	{
		return Super::ShouldOffsetWithAnchor();
	}

	void SpineCppObject::SetV1Compatibility(bool newValue)
	{
		Super::SetV1Compatibility(newValue);
	}

	void SpineCppObject::Draw(Rtt::Renderer& renderer) const
	{
		Super::Draw(renderer);
	}

	Rtt_INLINE  void SpineCppObject::Batch(SpineDataBuffer * copyBuffer, SpineDataBuffer * batchBuffer)
	{
		//��ȫ���
		auto copyIndexSize = copyBuffer->indices->size();
		auto copyUVSize = copyBuffer->uvs->size();
		auto copyVertSize = copyBuffer->verts->size();
		if (copyIndexSize == 0 || copyUVSize == 0 || copyVertSize == 0)//���ܻ���Ϊ��clipper�����ü�,���������ǿյ�.
		{
			return;
		}
		if (copyUVSize != copyVertSize
			|| copyVertSize % 2 != 0
			|| copyIndexSize % 3 != 0
			)
		{
			CoronaLuaWarning(_L, "this is bug ,please report to dianchu engine dev team, Rtt_Spine::Batch  unvalid batch data ...");
			return;
		}

		auto oldUVSize = batchBuffer->uvs->size();
		auto oldVertSize = batchBuffer->verts->size();
		auto oldIndexSize = batchBuffer->indices->size();

		batchBuffer->uvs->setSizeUnsafe(oldUVSize + copyUVSize);
		batchBuffer->verts->setSizeUnsafe(oldVertSize + copyVertSize);
		batchBuffer->indices->setSizeUnsafe(oldIndexSize + copyIndexSize);

		memcpy(batchBuffer->uvs->buffer() + oldUVSize, copyBuffer->uvs->buffer(), copyUVSize * sizeof(float));
		memcpy(batchBuffer->verts->buffer() + oldVertSize, copyBuffer->verts->buffer(), copyVertSize * sizeof(float));

		auto * bufferBatchIndex = batchBuffer->indices->buffer();
		auto * bufferCopyIndex = copyBuffer->indices->buffer();
		auto indexOffset = oldVertSize >> 1;
		for (size_t i = 0; i < copyIndexSize; ++i)
		{
			*(bufferBatchIndex + oldIndexSize + i) = bufferCopyIndex[i] + indexOffset;//maybe simd.
		}
	}

	//vertex:�������飨2ά�������ͣ�
	//index:���������±꣨1ά�����Σ�
	//uv:������Ⱦ��ֵ��
	void SpineCppObject::UpdateMeshs()
	{

		_meshPool->ReleaseAndHideAllMesh(this);


		if (_flagClearUnusedMeshCache)
		{
			ClearUnusedMeshCache();
		}

		spine::Vector<spine::Slot*>&  slots = _skeleton->getDrawOrder();

		Rtt::Color lastFillColor = 0xffffffff;
		TextureLoadArgs * lastTextureLoadArgs = NULL;
		Rtt::RenderTypes::BlendType lastBlendMode = Rtt::RenderTypes::BlendType::kNumBlendTypes;
		_spineDataBufferBatch->Clear();
		_batchNum = 0;
		_commitVertNum = 0;
		_spineVertNum = 0;
		_visiableAttachmentNum = 0;
		for (int i = 0; i < slots.size(); ++i)
		{
			Slot*  slot = slots[i];
			Attachment*  attachment = slot->getAttachment();
			_spineDataBuffer->Clear();
			if (slot->getBone().isActive())
			{
				auto  isClippingAttachment = false;
				RegionAttachment * rigionAttachment = NULL;
				MeshAttachment  * meshAttachment = NULL;
				ClippingAttachment * clippingAttachment = NULL;
				Rtt::Color curFillColor = Rtt::ColorWhite();//0~255
				TextureLoadArgs *curTextureLoadArgs = NULL;
				Rtt::RenderTypes::BlendType curBlendMode = Rtt::RenderTypes::BlendType::kNumBlendTypes;
				if (Is<Attachment, RegionAttachment>(attachment, rigionAttachment))
				{
					_spineDataBuffer->verts->setSize(EIGHT, 0);
					_spineDataBuffer->indices->setSize(QUAD_INDEX_SIX, 0);
					rigionAttachment->computeWorldVertices(slot->getBone(), _spineDataBuffer->verts->buffer(), 0, 2);//2���������� 2ά��
					spine::Vector<float>& spineUVs = rigionAttachment->getUVs();
					_spineDataBuffer->uvs->setSize(spineUVs.size(), 0);//Ԥ����ռ�,������size��ֵ
					memcpy(_spineDataBuffer->uvs->buffer(), spineUVs.buffer(), sizeof(float) * spineUVs.size());
					memcpy(_spineDataBuffer->indices->buffer(), QUAD_TRIANGLES, sizeof(unsigned short) * QUAD_INDEX_SIX);
					AtlasRegion* atlasRegion = (AtlasRegion*)rigionAttachment->getRendererObject();
					curTextureLoadArgs = (TextureLoadArgs*)(atlasRegion->page->getRendererObject());
					SpineColor2RttColor
					(
						_L,
						_premultipliedAlpha,
						slot->getBone().getSkeleton().getColor(),
						slot->getColor(),
						rigionAttachment->getColor(),
						curFillColor
					);
					curBlendMode = toCoronaBlendMode(slot->getData().getBlendMode());
				}
				else if (Is<Attachment, MeshAttachment>(attachment, meshAttachment))
				{
					_spineDataBuffer->verts->setSize(meshAttachment->getWorldVerticesLength(), 0);
					meshAttachment->computeWorldVertices(*slot, 0, meshAttachment->getWorldVerticesLength(), _spineDataBuffer->verts->buffer(), 0, 2);
					spine::Vector<float>& spineUVs = meshAttachment->getUVs();
					Vector<unsigned short> & spineIndices = meshAttachment->getTriangles();
					_spineDataBuffer->indices->setSize(spineIndices.size(), 0);
					_spineDataBuffer->uvs->setSize(spineUVs.size(), 0);
					memcpy(_spineDataBuffer->uvs->buffer(), spineUVs.buffer(), sizeof(float) * spineUVs.size());
					memcpy(_spineDataBuffer->indices->buffer(), spineIndices.buffer(), sizeof(unsigned short) * spineIndices.size());
					AtlasRegion* atlasRegion = (AtlasRegion*)meshAttachment->getRendererObject();
					curTextureLoadArgs = (TextureLoadArgs*)(atlasRegion->page->getRendererObject());
					SpineColor2RttColor
					(
						_L,
						_premultipliedAlpha,
						slot->getBone().getSkeleton().getColor(),
						slot->getColor(),
						meshAttachment->getColor(),
						curFillColor
					);
					curBlendMode = toCoronaBlendMode(slot->getData().getBlendMode());
				}
				else if (Is<Attachment, ClippingAttachment>(attachment, clippingAttachment))
				{
					this->_clipper->clipStart(*slot, clippingAttachment);
					isClippingAttachment = true;
				}

				if (this->_cliperEnable && this->_clipper->isClipping())
				{
					this->_clipper->clipTriangles
					(
						_spineDataBuffer->verts->buffer(),
						_spineDataBuffer->indices->buffer(),
						_spineDataBuffer->indices->size(),
						_spineDataBuffer->uvs->buffer(),
						2
					);
					auto & clipedVerts = this->_clipper->getClippedVertices();
					auto & clipedUVs = this->_clipper->getClippedUVs();
					auto & clipedIndices = this->_clipper->getClippedTriangles();


					_spineDataBuffer->verts->setSize(clipedVerts.size(), 0);
					_spineDataBuffer->uvs->setSize(clipedUVs.size(), 0);
					_spineDataBuffer->indices->setSize(clipedIndices.size(), 0);

					memcpy(_spineDataBuffer->verts->buffer(), clipedVerts.buffer(), sizeof(float) *clipedVerts.size());
					memcpy(_spineDataBuffer->uvs->buffer(), clipedUVs.buffer(), sizeof(float) *clipedUVs.size());
					memcpy(_spineDataBuffer->indices->buffer(), clipedIndices.buffer(), sizeof(unsigned short) *clipedIndices.size());

				}
				_spineVertNum += _spineDataBuffer->verts->size();
				if (lastTextureLoadArgs == NULL) //lastTextureLoadArgs==NULL  ˵��last�����ǵ�1�α���ֵ
				{
					lastTextureLoadArgs = curTextureLoadArgs;
					lastFillColor = curFillColor;
					lastBlendMode = curBlendMode;
				}

				if (!isClippingAttachment)
				{
					_visiableAttachmentNum++;
				}
				auto batchCondition =
					(lastBlendMode == curBlendMode)
					&& (lastTextureLoadArgs == curTextureLoadArgs)
					&& (lastFillColor == curFillColor);

				//��������������ҵ�ǰ���ǲü�����,���ύ����, ֮���Բ����ǲü�����, ����Ϊ�ü�������blendMode color texture �ǷǷ���, �����ƻ�last����
				if ((!batchCondition) && (!isClippingAttachment))
				{
					CommitShapeObject(_spineDataBufferBatch, lastTextureLoadArgs, lastFillColor, lastBlendMode);

					//����last����
					lastFillColor = curFillColor;
					lastBlendMode = curBlendMode;
					lastTextureLoadArgs = curTextureLoadArgs;

					//���batch����
					_spineDataBufferBatch->Clear();
				}

				Batch(_spineDataBuffer, _spineDataBufferBatch);

				if (!isClippingAttachment)
				{
					this->_clipper->clipEnd(*slot);
				}
			}//slot->getBone().isActive()
			else
			{
				this->_clipper->clipEnd(*slot);
			}//else slot->getBone().isActive()
		}//for

		//�ύ��������
		if (!_spineDataBufferBatch->IsEmpty())
		{
			CommitShapeObject(_spineDataBufferBatch, lastTextureLoadArgs, lastFillColor, lastBlendMode);
		}

		this->_clipper->clipEnd();
		if (_commitVertNum != _spineVertNum)
		{
			CoronaLuaWarning(_L, "this is bug ,please report to dianchu engine dev team, Rtt_Spine::UpdateMeshs   _commitVertNum!= _spineVertNum");
		}

	}

	void SpineCppObject::CommitShapeObject
	(
		SpineDataBuffer  * dataBuffer,
		TextureLoadArgs* textureLoadArgs,
		Rtt::Color fillColor,
		CoronaBlendMode blendMode
	)
	{
		if (
			textureLoadArgs != NULL
			&& dataBuffer->uvs->size() > 0
			&& dataBuffer->verts->size() > 0
			&& dataBuffer->indices->size() > 0)
		{
			SpineShapeObj *  shapeObj = NULL;
			Rtt::TesselatorMesh * meshObj = NULL;

			shapeObj = _meshPool->GetShapeObj
			(this,
				textureLoadArgs,
				dataBuffer,
				meshObj);
			if (
				meshObj->GetUV().Length() == dataBuffer->uvs->size() >> 1
				&& meshObj->GetMesh().Length() == dataBuffer->verts->size() >> 1
				&& meshObj->GetIndices().Length() == dataBuffer->indices->size()
				)
			{
				UpdateCoronaMesh(_L, (Rtt::ShapePath*)&(shapeObj->GetPath()), meshObj, dataBuffer);
				_commitVertNum += dataBuffer->verts->size();
			}
			else
			{
				CoronaLuaWarning(_L, "Vertices  not updated: the amount of vertices in the mesh is not equal to the amount of vertices in the table ");
			}
			if (shapeObj != NULL && meshObj != NULL)
			{
				shapeObj->SetFillColor(fillColor);
				shapeObj->SetBlend(blendMode);
				toFront(_L, GetDrawingGroup(), shapeObj);
				_batchNum++;
			}
		}
	}

	void SpineCppObject::ClearMeshs()
	{
		_meshPool->ClearMeshs(this);
	}
	Rtt::GroupObject * SpineCppObject::GetDrawingGroup()
	{
		return this;
	}

	SpineShapeObj * SpineCppObject::CreateMeshAndAssignParent
	(
		SpineDataBuffer * spineDataBuffer,
		Rtt::TesselatorMesh * & mesh,
		TextureLoadArgs * textureLoadArgs
	)
	{
		SpineShapeObj * shapeObj = NULL;
		auto drawingGroup = GetDrawingGroup();
		Rtt::ShapePath *path = Rtt::ShapePath::NewMesh(_ac, Rtt::Geometry::kIndexedTriangles);//Ӳ���� ��ShapeAdapterMesh::GetMeshMode ��֪  indexed ���������ö��
		Rtt::TesselatorMesh *tesselator = (Rtt::TesselatorMesh *)path->GetTesselator();
		mesh = tesselator;
		if (InitCoronaMesh(*tesselator, spineDataBuffer))
		{
			shapeObj = Rtt_NEW(_ac, SpineShapeObj(path));

			if (tesselator->GetFillPrimitive() == Rtt::Geometry::kIndexedTriangles)
			{
				path->Invalidate(Rtt::ShapePath::kFillSourceIndices);
			}
			//����paint
			{
				auto * p = NewBitmapPaint(_L, *_display, textureLoadArgs);
				shapeObj->SetFill(p); //��߹��������paint ������
			}
			AssignParen(_L, *_display, shapeObj, GetDrawingGroup(), false);//����drawingGroup
		}
		else
		{
			Rtt_DELETE(path);
		}
		return shapeObj;
	}





	int SpineCppObject::GetMeshCacheNum()
	{
		return _meshPool->GetMeshTotalNum();
	}


	int SpineCppObject::GetActiveMeshCacheNum()
	{
		return _meshPool->GetRentList().size();
	}

	int SpineCppObject::GetClipperAttachmentCount()
	{
		spine::Vector<spine::Slot*>&  slots = _skeleton->getDrawOrder();
		int clipperCount = 0;
		for (int i = 0; i < slots.size(); ++i)
		{
			Attachment*  attachment = slots[i]->getAttachment();
			ClippingAttachment * clippingAttachment = NULL;
			if (Is<Attachment, ClippingAttachment>(attachment, clippingAttachment))
			{
				clipperCount++;
			}
		}
		return clipperCount;
	}


	int SpineCppObject::GetChildrenNum()
	{
		return fChildren.Length();
	}


	int SpineCppObject::GetTextureNum()
	{
		return _textureLoader->GetLoadedTextureNum();
	}


	void SpineCppObject::ClearUnusedMeshCache()
	{
		if (_flagClearUnusedMeshCache)
		{
			_meshPool->ClearUnusedMeshs(this);
			_flagClearUnusedMeshCache = false;
		}
	}

	void SpineCppObject::GetSelfBounds(Rtt::Rect& rect) const
	{
		//TODO ����ط���ʱ��֪����ʲô�� , ���Լ��㵱ǰ֡���ж����AABB ,Ȼ�󷵻�,����ûɶ��Ҫ..
		Super::GetSelfBounds(rect);
	}

	//ʵ��Rtt_MUpdatable.h���麯��
	void SpineCppObject::QueueUpdate()
	{
		auto deltaTime = _display->GetDeltaTimeInSeconds();
		_skeleton->update(deltaTime);
		_aniState->update(deltaTime);
		_aniState->apply(*_skeleton);
		_skeleton->updateWorldTransform();
		UpdateMeshs();
	}

	void SpineCppObject::DidSetMask(Rtt::BitmapMask *mask, Rtt::Uniform *uniform)
	{
		Super::DidSetMask(mask, uniform);
	}

	bool SpineCppObject::RemoveMesh(SpineShapeObj*   displayO)
	{

		if (displayO == NULL)
		{
			SpineLog(_L, ESpineLog::Error, " this id bug remove mesh error arg is null ...");
			return false;
		}
		ObjectFinalizer< SpineShapeObj >::Collect(*displayO);
		DidRemove();
		return true;
	}

	void SpineCppObject::UnloadSpine()
	{
		ClearMeshs();
		Rtt_DELETE(this->_atlas);
		Rtt_DELETE(this->_skeleton);
		Rtt_DELETE(this->_skeletonData);
		Rtt_DELETE(this->_aniState);
		Rtt_DELETE(this->_aniStateData);
		Rtt_DELETE(this->_clipper);
		Rtt_DELETE(this->_textureLoader);
		Rtt_DELETE(this->_spineDataBuffer);
		Rtt_DELETE(this->_spineDataBufferBatch);
		Rtt_DELETE(this->_meshPool);
		Rtt_DELETE((CoronaSpineAnimationListenerObject*)this->_coronaSpineAnimationListenerObject);
		if (_luaResourceOnAnimationCallback != NULL)
		{
			Rtt_DELETE(this->_luaResourceOnAnimationCallback);
		}

	}
}
