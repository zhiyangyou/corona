#ifndef _RTT_SPINE_H_
#define _RTT_SPINE_H_


#include <string>
#include <vector>
#include "Display/Rtt_DisplayObject.h"
#include "Display/Rtt_GroupObject.h"
#include "Display/Rtt_TesselatorMesh.h"
#include "Rtt_MUpdatable.h"
#include "Rtt_MPlatform.h"
#include "Rtt_LuaResource.h"



// ----------------------------------------------------------------------------

struct lua_State;
class  Atlas;
class  SkeletonData;
class  AnimationStateData;
class  Skeleton;
class  AnimationState;
class SkeletonClipping;


namespace spine {
	class CoronaTextureLoader;
	class Color;
	class PaintPool;
	class StringHashValueMap;
	class TextureLoadArgs;
	class MeshPool;
	class SpineDataBuffer;
	class SpineShapeObj;

	class SpineCppObject: public Rtt::GroupObject, public Rtt::MUpdatable
	{
		typedef Rtt::GroupObject Super;
		typedef Rtt::RenderTypes::BlendType CoronaBlendMode;
		//typedef  std::map<void*, CoronaMeshInfo> CoronaMeshPairMap;
	private:
		Rtt_CLASS_NO_COPIES(SpineCppObject)
			friend class MeshPool;
	private:
		//不要调用！
		SpineCppObject() :GroupObject(NULL, NULL) {}


		
	public://spine相关的方法
		AnimationState * GetSpineAnimationState() { return _aniState; }
		AnimationStateData * GetSpineAnimationStateData() { return _aniStateData; }
		Skeleton * GetSpineSkeleton() { return _skeleton; }
		SkeletonData * GetSpineSkeletonData() { return _skeletonData; }
		bool SetAnimationCallback(lua_State * L);
		Rtt::Display* GetDisplay( ){ return _display; };
		float getCurrentDuration( );
		float getDuration( int trackIndex, std::string& animation, bool isLoop );
		float getAnimationTime( );
		void addCompleteCallback();
	public:
		SpineCppObject(lua_State * L, Rtt::Display & display, Rtt_Allocator* ac, const Rtt::MPlatform::Directory & baseDir, const std::string & atlasPath, const std::string & dataPath,/*.skel或者.json */ bool premultipliedAlpha, const float  posX, const float  poxY, const float  scaleX, const float  scaleY, const std::string & skin, const std::string  &animation, int trackIndex, bool isLoop, float defaultMix);
		~SpineCppObject() {}
		virtual void FinalizeSelf(lua_State *L);
		virtual bool UpdateTransform(const Rtt::Matrix& parentToDstSpace);
		virtual void Prepare(const Rtt::Display& display);
		virtual void Translate(Rtt::Real deltaX, Rtt::Real deltaY);
		virtual void GetSelfBoundsForAnchor(Rtt::Rect& rect) const;
		virtual bool HitTest(Rtt::Real contentX, Rtt::Real contentY);
		virtual void DidUpdateTransform(Rtt::Matrix& srcToDst);
		virtual void Scale(Rtt::Real sx, Rtt::Real sy, bool isNewValue);
		virtual void Rotate(Rtt::Real deltaTheta);
		virtual void DidMoveOffscreen();
		virtual void WillMoveOnscreen();
		virtual bool CanCull() const;
		virtual void InitProxy(lua_State *L);
		virtual Rtt::LuaProxy* GetProxy() const;
		virtual void ReleaseProxy();
		virtual void AddedToParent(lua_State * L, Rtt::GroupObject * parent);
		virtual void RemovedFromParent(lua_State * L, Rtt::GroupObject * parent);
		virtual const Rtt::LuaProxyVTable& ProxyVTable() const;
		virtual Rtt::GroupObject* AsGroupObject() override;
		virtual void WillDraw(Rtt::Renderer& renderer) const;
		virtual void DidDraw(Rtt::Renderer& renderer) const;
		virtual void SetSelfBounds(Rtt::Real width, Rtt::Real height);
		virtual bool ShouldOffsetWithAnchor() const;
		virtual void SetV1Compatibility(bool newValue);
		virtual void Draw(Rtt::Renderer& renderer) const;
		
		virtual void GetSelfBounds(Rtt::Rect& rect) const;
		virtual void QueueUpdate() override;
	protected:
		virtual void DidSetMask(Rtt::BitmapMask *mask, Rtt::Uniform *uniform);

	private:
		Rtt_INLINE void Batch(SpineDataBuffer * buffer, SpineDataBuffer * bufferBatch);
		bool RemoveMesh(SpineShapeObj* displayO);
		void UnloadSpine();
		void UpdateMeshs();
		void CommitShapeObject(SpineDataBuffer * dataBuffer, TextureLoadArgs* textureLoadArgs, Rtt::Color fillColor, CoronaBlendMode blendMode);
		void ClearMeshs();

		Rtt::GroupObject * GetDrawingGroup();
		SpineShapeObj * CreateMeshAndAssignParent
		(
			SpineDataBuffer * spineDataBuffer,
			Rtt::TesselatorMesh * & mesh,
			TextureLoadArgs * textureLoadArgs);
	private:
		Rtt::MPlatform::Directory _baseDir;
		//spine-cpp相关的资源, 记得释放！
		Atlas * _atlas;
		SkeletonData * _skeletonData;
		AnimationStateData * _aniStateData;
		Skeleton * _skeleton;
		AnimationState *_aniState;
		SkeletonClipping * _clipper;
		CoronaTextureLoader * _textureLoader;
		//void *_track;
		void *  _coronaSpineAnimationListenerObject;
		//lua 回调资源, 记得释放...
		Rtt::LuaResource * _luaResourceOnAnimationCallback;
		//PaintPool *_paintPool;
		MeshPool * _meshPool;

		//容器
		//CoronaMeshPairMap * _meshs;
		SpineDataBuffer *  _spineDataBuffer;
		SpineDataBuffer *  _spineDataBufferBatch;


		//其他
		int _batchNum = 0;
		int _visiableAttachmentNum = 0;
		int _commitVertNum = 0;
		int _spineVertNum = 0;
		mutable bool _cliperEnable = true;
		bool _flagClearUnusedMeshCache = false;
		bool _premultipliedAlpha = false;
	public:
		void SetPMA(bool v) { _premultipliedAlpha = v; } //PMA  = PremultipliedAlpha
		int GetMeshCacheNum();
		int GetActiveMeshCacheNum();
		int GetBatchNum() { return _batchNum; }
		int GetVisiableAttachNum() { return _visiableAttachmentNum; }
		int GetSpineVertNum() { return _spineVertNum >> 1; }
		void SetClipperEnable(bool v) { _cliperEnable = v; }
		int GetClipperAttachmentCount();
		int GetChildrenNum();
		int GetTextureNum();
		void ClearUnusedMeshCache();
		void SetFlagClearUnusedMeshCache(bool value) { _flagClearUnusedMeshCache = value; }
	private:
		//不要删除
		Rtt_Allocator * _ac;
		Rtt::Display*  _display;
		lua_State * _L;
	};
}
#endif 


