#ifndef FBX_SCENE_WRITER_H
#define FBX_SCENE_WRITER_H

#include "FBXNIFPluginNS.h"

#include <nifparse/Types.h>
#include <fbxsdk/core/math/fbxvector4.h>
#include <fbxsdk/core/math/fbxaffinematrix.h>
#include <fbxsdk/scene/geometry/fbxmesh.h>

namespace nifparse {
	class NIFFile;
}

namespace fbxsdk {
	class FbxDocument;
	class FbxNode;
	class FbxScene;
}

namespace fbxnif {
	enum : uint32_t {
		// NiAVObject flags
		NiAVObjectFlagHidden = 1 << 0
	};

	class FBXSceneWriter {
	public:
		FBXSceneWriter(const NIFFile &file);
		~FBXSceneWriter();

		FBXSceneWriter(const FBXSceneWriter &other) = delete;
		FBXSceneWriter &operator =(const FBXSceneWriter &other) = delete;

		void write(FbxDocument *document);

	private:
		void convertSceneNode(const NIFReference &var, fbxsdk::FbxNode *containingNode);

		void convertNiNode(const NIFDictionary &dict, fbxsdk::FbxNode *node);
		void convertNiTriBasedGeom(const NIFDictionary &dict, fbxsdk::FbxNode *node);
		
		std::string getString(const NIFDictionary &dict) const;
		FbxVector4 getVector3(const NIFDictionary &dict) const;
		FbxAMatrix getMatrix3x3(const NIFDictionary &dict) const;
		FbxColor getColor4(const NIFDictionary &dict) const;
		FbxVector2 getTexCoord(const NIFDictionary &dict) const;

		template<typename ElementType>
		void importVectorElement(const NIFDictionary &data, FbxMesh *mesh, const Symbol &name, ElementType *(FbxGeometryBase::*createElement)());

		const NIFFile &m_file;
		fbxsdk::FbxScene *m_scene;
	};
}

#endif
