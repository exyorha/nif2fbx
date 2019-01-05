#ifndef FBX_SCENE_WRITER_H
#define FBX_SCENE_WRITER_H

#include "FBXNIFPluginNS.h"

#include <nifparse/Types.h>

#include <fbxsdk/core/math/fbxvector4.h>
#include <fbxsdk/core/math/fbxaffinematrix.h>
#include <fbxsdk/scene/geometry/fbxmesh.h>

#include <unordered_map>

namespace nifparse {
	class NIFFile;
}

namespace fbxsdk {
	class FbxDocument;
	class FbxNode;
	class FbxScene;
	class FbxAnimCurve;
}

namespace fbxnif {
	class SkeletonProcessor;

	enum : uint32_t {
		// NiAVObject flags
		NiAVObjectFlagHidden = 1 << 0
	};

	class FBXSceneWriter {
	public:
		FBXSceneWriter(const NIFFile &file, const SkeletonProcessor &skeleton);
		~FBXSceneWriter();

		FBXSceneWriter(const FBXSceneWriter &other) = delete;
		FBXSceneWriter &operator =(const FBXSceneWriter &other) = delete;

		void write(FbxDocument *document);

		inline const FbxString &skeletonFile() const { return m_skeletonFile; }
		inline void setSkeletonFile(const FbxString &skeletonFile) { m_skeletonFile = skeletonFile; }

	private:
		void convertSceneNode(const NIFReference &var, fbxsdk::FbxNode *containingNode);

		void convertNiNode(const NIFDictionary &dict, fbxsdk::FbxNode *node);
		void convertNiTriBasedGeom(const NIFDictionary &dict, fbxsdk::FbxNode *node);
		void convertBSTriShape(const NIFDictionary &dict, fbxsdk::FbxNode *node);

		enum class CurveGenerationMode {
			Rotation,
			Translation,
			Scaling,
			RotationX,
			RotationY,
			RotationZ,
			RotationQuaternion
		};

		struct AnimationTake {
			FbxAnimStack *stack;
			FbxAnimLayer *defaultLayer;
		};

		template<typename ElementType>
		void importVectorElement(const NIFDictionary &data, FbxMesh *mesh, const Symbol &name, ElementType *(FbxGeometryBase::*createElement)());
		
		void importMeshTriangles(FbxMesh *mesh, const NIFDictionary &container);

		FbxNode *findSkeletonRoot(FbxNode *parent);
		void registerImportedBones(FbxNode *bone);

		void processController(const NIFDictionary &controller, FbxNode *node);

		template<typename PropertyType>
		void generateCurves(const NIFDictionary &keyGroup, FbxPropertyT<PropertyType> &prop, FbxAnimLayer *layer, CurveGenerationMode mode, FbxAnimCurveNode *&node);

		template<typename PropertyType>
		void generateCurves(const NIFEnum &interpolation, const NIFArray &keys, FbxPropertyT<PropertyType> &prop, FbxAnimLayer *layer, CurveGenerationMode mode, FbxAnimCurveNode *&node);

		void processControllerSequence(const NIFDictionary &sequence, const NIFReference &palette);
		
		void ensureSkeletonImported(FbxNode *node);
		
		AnimationTake &getCurrentTake();
		FbxAnimLayer *getDefaultTakelayer(AnimationTake &take);

		const NIFFile &m_file;
		const SkeletonProcessor &m_skeleton;
		FbxScene *m_scene;
		std::unordered_map<std::shared_ptr<NIFVariant>, FbxNode *> m_nodeMap;
		std::unordered_map<std::string, FbxNode *> m_importedBoneMap;
		unsigned int m_meshesGenerated;
		unsigned int m_skeletonNodesGenerated;
		FbxString m_skeletonFile;
		std::vector<const NIFDictionary *> m_controllerManagers;
		bool m_skeletonImported;
		std::vector<AnimationTake> m_animationTakes;
	};
}

#endif
