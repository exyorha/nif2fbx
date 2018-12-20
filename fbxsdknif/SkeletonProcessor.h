#ifndef SKELETON_PROCESSOR_H
#define SKELETON_PROCESSOR_H

#include "FBXNIFPluginNS.h"

#include <nifparse/Types.h>

#include <unordered_set>
#include <unordered_map>

#include <fbxsdk/core/math/fbxaffinematrix.h>

namespace nifparse {
	class NIFFile;
}

namespace fbxnif {
	class SkeletonProcessor {
	public:
		SkeletonProcessor();
		~SkeletonProcessor();

		SkeletonProcessor(const SkeletonProcessor &other) = delete;
		SkeletonProcessor &operator =(const SkeletonProcessor &other) = delete;

		void process(NIFFile &file);

		inline const std::unordered_set<std::shared_ptr<NIFVariant>> &allBones() const { return m_allBones; }
		inline const std::shared_ptr<NIFVariant> &commonBoneRoot() const { return m_commonBoneRoot; }

	private:
		void collectSkinsAndParents(const NIFReference &node, const std::shared_ptr<NIFVariant> &parentNode);
		std::shared_ptr<NIFVariant> getParentOfNode(const std::shared_ptr<NIFVariant> &node);
		void processNode(const NIFReference &node);
		void processNodeInSkeleton(const NIFReference &node);
		
		std::string nodeName(const NIFDictionary &node) const;

		FbxAMatrix getLocalTransform(const NIFDictionary &node) const;

		struct SkinInfo {
			std::shared_ptr<NIFVariant> geometry;
			std::shared_ptr<NIFVariant> skin;
		};

		NIFFile *m_file;
		std::vector<SkinInfo> m_skins;
		std::unordered_map<std::shared_ptr<NIFVariant>, std::shared_ptr<NIFVariant>> m_parentNodes;
		std::shared_ptr<NIFVariant> m_commonBoneRoot;
		std::unordered_set<std::shared_ptr<NIFVariant>> m_allBones;
	};
}

#endif
