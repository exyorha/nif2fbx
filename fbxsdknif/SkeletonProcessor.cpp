#include "SkeletonProcessor.h"

#include <nifparse/NIFFile.h>

#include <algorithm>

#include "NIFUtils.h"

#include <fbxsdk/core/math/fbxquaternion.h>

namespace fbxnif {
	
	/*
	 * In order from most preferred to least preferred
	 */
	const char *const SkeletonProcessor::m_rootBoneNames[]{
		"NPC Root [Root]", // Skyrim's dragon.nif has vestigial 'NPC' and real 'NPC Root' at top level
		"NPC"
	};

	SkeletonProcessor::SkeletonProcessor() : m_cleaningRequired(false), m_skeletonImport(false) {

	}

	SkeletonProcessor::~SkeletonProcessor() {

	}

	std::shared_ptr<NIFVariant> SkeletonProcessor::getParentOfNode(const std::shared_ptr<NIFVariant> &node) {
		auto it = m_parentNodes.find(node);
		if (it == m_parentNodes.end()) {
			return {};
		}

		return it->second;
	}

	void SkeletonProcessor::process(NIFFile &file) {
		m_file = &file;

		m_cleaningRequired = file.header().getValue<uint32_t>("Version") == 0x04000002; // Only Morrowind needs heavy-duty merging and cleaning

		auto &roots = file.rootObjects().data;
		if (roots.empty())
			return;

		auto &nifRoot = std::get<NIFReference>(roots.front());

		if (std::get<NIFDictionary>(*nifRoot.ptr).kindOf("NiAVObject")) {
			collectSkinsAndParents(nifRoot, nullptr);
		}

		if (!m_skins.empty()) {
			for (const auto &skinInfo : m_skins) {
				const auto &skin = std::get<NIFDictionary>(*skinInfo.skin);

				for (const auto &bone : skin.getValue<NIFArray>("Bones").data) {
					m_allBones.emplace(std::get<NIFPointer>(bone).ptr);
				}
			}
		}

		if(!m_allBones.empty()) {
			bool needRecalculation;
			do {
				needRecalculation = false;

				std::vector<std::list<std::shared_ptr<NIFVariant>>> boneAncestors;
				boneAncestors.reserve(m_allBones.size());

				for (const auto &bone : m_allBones) {
					std::list<std::shared_ptr<NIFVariant>> ancestors;

					for (auto current = bone; current; current = getParentOfNode(current)) {
						ancestors.push_front(current);
					}

					boneAncestors.emplace_back(std::move(ancestors));
				}

				if (boneAncestors.empty())
					throw std::logic_error("skins are present, but not bones");

				if (boneAncestors[0].empty())
					throw std::logic_error("common bone ancestor cannot be determined");

				auto root = boneAncestors[0].front();

				for (size_t index = 1, size = boneAncestors.size(); index < size; index++) {
					if (boneAncestors[0].empty() || boneAncestors[0].front() != root)
						throw std::logic_error("common bone ancestor cannot be determined");
				}

				for (auto &list : boneAncestors) {
					list.pop_front();
				}

				while (!boneAncestors[0].empty()) {
					auto firstAncestor = boneAncestors[0].front();
					bool allSame = true;

					printf("Examining %s\n", nodeName(std::get<NIFDictionary>(*firstAncestor)).c_str());

					for (size_t index = 1, size = boneAncestors.size(); index < size; index++) {
						if (boneAncestors[index].empty() || boneAncestors[index].front() != firstAncestor) {
							allSame = false;
							break;
						}
					}

					if (allSame) {
						root = firstAncestor;

						auto result = m_allBones.emplace(root);
						if (result.second)
							needRecalculation = true;

						for (auto &list : boneAncestors) {
							list.pop_front();
						}
					}
					else {
						break;
					}
				}

				auto result = m_allBones.emplace(root);
				if (result.second)
					needRecalculation = true;

				m_commonBoneRoot = root;

				if (needRecalculation) {
					printf("List of bones changed, rechecking root bone\n");
				}
			} while (needRecalculation);

			if (m_cleaningRequired) {
				auto skeletonParent = getParentOfNode(m_commonBoneRoot);
				if (!skeletonParent) {
					if (m_commonBoneRoot != nifRoot.ptr) {
						throw std::logic_error("skeleton root has no parent, but is not the root node");
					}

					auto newRoot = std::make_shared<NIFVariant>();
					*newRoot = NIFDictionary();
					auto &rootDict = std::get<NIFDictionary>(*newRoot);
					rootDict.isNiObject = true;
					rootDict.typeChain.emplace_back("NiNode");
					rootDict.typeChain.emplace_back("NiAVObject");
					rootDict.typeChain.emplace_back("NiObjectNET");
					rootDict.typeChain.emplace_back("NiObject");

					/*
					 * NiObject
					 */
					NIFDictionary name;
					name.isNiObject = false;
					name.typeChain.emplace_back("string");
					NIFDictionary nameString;
					nameString.isNiObject = false;
					nameString.typeChain.emplace_back("SizedString");
					nameString.data.emplace("Value", "SynthesizedRoot");
					name.data.emplace("String", std::move(nameString));
					rootDict.data.emplace("Name", std::move(name));

					rootDict.data.emplace("Extra Data", NIFReference());

					rootDict.data.emplace("Extra Data List", NIFArray());

					rootDict.data.emplace("Controller", NIFReference());

					/*
					 * NiAVObject
					 */
					rootDict.data.emplace("Flags", 0U);

					rootDict.data.emplace("Translation", makeVector3(FbxVector4()));

					rootDict.data.emplace("Rotation", makeMatrix3x3(FbxAMatrix()));

					rootDict.data.emplace("Scale", 1.0f);

					rootDict.data.emplace("Properties", NIFArray());

					rootDict.data.emplace("Has Bounding Volume", 0U);

					m_parentNodes.erase(m_commonBoneRoot);
					m_parentNodes.emplace(m_commonBoneRoot, newRoot);

					/*
					 * NiNode
					 */
					NIFArray children;
					children.data.emplace_back(std::move(nifRoot));
					rootDict.data.emplace("Children", std::move(children));

					nifRoot.type = Symbol("NiNode");
					nifRoot.target = -2;
					nifRoot.ptr = newRoot;


				}
			}
		}

		if (!m_commonBoneRoot && m_skeletonImport) {
			fprintf(stderr, "Requested skeleton import, but no bones found on the first pass. Trying heuristics\n");

			const auto &dict = std::get<NIFDictionary>(*std::get<NIFReference>(roots.front()).ptr);
			if (!dict.kindOf("NiNode"))
				throw std::runtime_error("Skeleton import is requested, but no NiNode at root level");

			const auto &children = dict.getValue<NIFArray>("Children");

			for (auto name : m_rootBoneNames) {
				for (const auto &child : children.data) {
					const auto &ref = std::get<NIFReference>(child);
					if (!ref.ptr)
						continue;

					const auto &childDict = std::get<NIFDictionary>(*ref.ptr);
					if (nodeName(childDict) == name) {
						fprintf(stderr, "Found '%s'\n", name);

						m_commonBoneRoot = ref.ptr;

						markBones(ref);

						goto breakOuter;
					}
				}
			}
		breakOuter:;
			
		}

		if (m_commonBoneRoot) {
			printf("Skeleton root: %s\nNIF Root: %s\nAll bones, unordered:\n", nodeName(std::get<NIFDictionary>(*m_commonBoneRoot)).c_str(),
				nodeName(std::get<NIFDictionary>(*nifRoot.ptr)).c_str());
			for (const auto &bone : m_allBones) {
				printf(" - %s\n", nodeName(std::get<NIFDictionary>(*bone)).c_str());
			}

			if (m_cleaningRequired)
				processNode(nifRoot);
		}
	}

	void SkeletonProcessor::processNode(const NIFReference &node) {
		const auto &dict = std::get<NIFDictionary>(*node.ptr);
		if (dict.kindOf("NiNode")) {
			if (m_commonBoneRoot == node.ptr) {
				auto childrenCopy = dict.getValue<NIFArray>("Children").data;
				for (const auto &child : childrenCopy) {
					const auto &childDesc = std::get<NIFReference>(child);
					if (childDesc.ptr)
						processNodeInSkeleton(childDesc);
				}
			}
			else {
				auto childrenCopy = dict.getValue<NIFArray>("Children");
				for (const auto &child : childrenCopy.data) {
					const auto &childDesc = std::get<NIFReference>(child);
					if (childDesc.ptr)
						processNode(childDesc);
				}
			}
		}
	}

	void SkeletonProcessor::processNodeInSkeleton(const NIFReference &node) {
		auto &dict = std::get<NIFDictionary>(*node.ptr);
		if (dict.kindOf("NiNode")) {
			NIFArray childrenCopy = dict.getValue<NIFArray>("Children");
			for (const auto &child : childrenCopy.data) {
				const auto &childDesc = std::get<NIFReference>(child);
				if (childDesc.ptr)
					processNodeInSkeleton(childDesc);
			}

			if (node.ptr != m_commonBoneRoot &&
				m_allBones.count(node.ptr) == 0 &&
				dict.getValue<NIFArray>("Children").data.empty()) {

				fprintf(stderr, "node %s no longer has any children\n", nodeName(dict).c_str());

				auto parent = getParentOfNode(node.ptr);
				auto &parentChildren = std::get<NIFDictionary>(*parent).getValue<NIFArray>("Children").data;
				parentChildren.erase(std::remove_if(parentChildren.begin(), parentChildren.end(), [&node](const NIFVariant &ref) {
					return std::get<NIFReference>(ref).ptr == node.ptr;
				}), parentChildren.end());
			}
		}
		else if (dict.kindOf("NiGeometry")) {
			fprintf(stderr, "geometry in skeleton: %s\n", nodeName(dict).c_str());

			std::shared_ptr<NIFVariant> closestBone;

			for (auto parent = getParentOfNode(node.ptr); parent; parent = getParentOfNode(parent)) {
				if (m_allBones.count(parent) != 0) {
					closestBone = parent;
					break;
				}
			}

			auto target = getParentOfNode(m_commonBoneRoot);
			fprintf(stderr, "closest bone: %s, target: %s\n", nodeName(std::get<NIFDictionary>(*closestBone)).c_str(), nodeName(std::get<NIFDictionary>(*target)).c_str());

			auto localTransform = getLocalTransform(dict);
			auto skinTransform = localTransform;

			for (auto parent = getParentOfNode(node.ptr); parent != target; parent = getParentOfNode(parent)) {
				localTransform = getLocalTransform(std::get<NIFDictionary>(*parent)) * localTransform;
			}

			for (auto parent = getParentOfNode(node.ptr); parent != closestBone; parent = getParentOfNode(parent)) {
				skinTransform = getLocalTransform(std::get<NIFDictionary>(*parent)) * skinTransform;
			}

			dict.data.erase("Translation");
			dict.data.emplace("Translation", makeVector3(localTransform.GetT()));
			dict.data.erase("Rotation");
			dict.data.emplace("Rotation", makeMatrix3x3(FbxAMatrix(FbxVector4(), localTransform.GetQ(), FbxVector4(1.0, 1.0, 1.0, 1.0))));
			dict.data.erase("Scale");
			dict.data.emplace("Scale", static_cast<float>(localTransform.GetS()[0]));
			
			auto parent = getParentOfNode(node.ptr);
			auto &parentChildren = std::get<NIFDictionary>(*parent).getValue<NIFArray>("Children").data;
			parentChildren.erase(std::remove_if(parentChildren.begin(), parentChildren.end(), [&node](const NIFVariant &ref) {
				return std::get<NIFReference>(ref).ptr == node.ptr;
			}), parentChildren.end());

			std::get<NIFDictionary>(*target).getValue<NIFArray>("Children").data.emplace_back(node);

			m_parentNodes.erase(node.ptr);
			m_parentNodes.emplace(node.ptr, target);

			Symbol symSkinInstance("Skin Instance");
			if (dict.data.count(symSkinInstance) == 0) {
				dict.data.emplace(symSkinInstance, NIFReference());
			}

			auto &skinPtr = dict.getValue<NIFReference>(symSkinInstance).ptr;
			if (!skinPtr) {
				printf("Setting up skinning\n");

				auto newSkin = std::make_shared<NIFVariant>(NIFDictionary());
				auto &skin = std::get<NIFDictionary>(*newSkin);
				skinPtr = newSkin;

				skin.isNiObject = true;
				skin.typeChain.emplace_back("NiSkinInstance");
				skin.typeChain.emplace_back("NiObject");

				auto newSkinData = std::make_shared<NIFVariant>(NIFDictionary());
				auto &skinData = std::get<NIFDictionary>(*newSkinData);
				skinData.isNiObject = true;
				skinData.typeChain.emplace_back("NiSkinData");
				skinData.typeChain.emplace_back("NiObject");

				NIFReference skinDataRef;
				skinDataRef.ptr = newSkinData;
				skin.data.emplace("Data", std::move(skinDataRef));

				skin.data.emplace("Skin Partition", NIFReference());

				NIFPointer rootReference;
				rootReference.ptr = m_commonBoneRoot;
				skin.data.emplace("Skeleton Root", std::move(rootReference));

				NIFPointer boneReference;
				boneReference.ptr = closestBone;
				NIFArray boneReferences;
				boneReferences.data.emplace_back(std::move(boneReference));
				skin.data.emplace("Bones", std::move(boneReferences));

				SkinInfo skinInfo;
				skinInfo.geometry = node.ptr;
				skinInfo.skin = newSkin;
				m_skins.emplace_back(std::move(skinInfo));

				skinData.data.emplace("Skin Transform", makeTransform(FbxAMatrix()));

				auto &geomData = std::get<NIFDictionary>(*dict.getValue<NIFReference>("Data").ptr);
				auto numVertices = geomData.getValue<uint32_t>("Num Vertices");
				NIFArray weights;
				weights.data.reserve(numVertices);

				for (size_t vertex = 0; vertex < numVertices; vertex++) {
					NIFDictionary weight;
					weight.isNiObject = false;
					weight.typeChain.emplace_back("BoneVertData");
					weight.data.emplace("Index", static_cast<uint32_t>(vertex));
					weight.data.emplace("Weight", 1.0f);
					weights.data.emplace_back(std::move(weight));
				}

				NIFDictionary bone;
				bone.isNiObject = false;
				bone.typeChain.emplace_back("BoneData");
				bone.data.emplace("Vertex Weights", std::move(weights));
				bone.data.emplace("Skin Transform", makeTransform(FbxAMatrix(skinTransform)));

				NIFArray bones;
				bones.data.emplace_back(std::move(bone));

				skinData.data.emplace("Bone List", std::move(bones));
			}
		}
	}

	void SkeletonProcessor::markBones(const NIFReference &node) {
		auto &dict = std::get<NIFDictionary>(*node.ptr);
		if (dict.kindOf("NiNode")) {
			m_allBones.emplace(node.ptr);

			NIFArray childrenCopy = dict.getValue<NIFArray>("Children");
			for (const auto &child : childrenCopy.data) {
				const auto &childDesc = std::get<NIFReference>(child);
				if (childDesc.ptr)
					markBones(childDesc);
			}
		}
	}

	FbxAMatrix SkeletonProcessor::getLocalTransform(const NIFDictionary &node) const {
		return FbxAMatrix(
			getVector3(node.getValue<NIFDictionary>("Translation")),
			getMatrix3x3(node.getValue<NIFDictionary>("Rotation")).GetQ(),
			FbxVector4(FbxDouble3(node.getValue<float>("Scale")))
		);
	}

	void SkeletonProcessor::collectSkinsAndParents(const NIFReference &node, const std::shared_ptr<NIFVariant> &parentNode) {
		auto &desc = std::get<NIFDictionary>(*node.ptr);
		m_parentNodes.emplace(node.ptr, parentNode);

		for (auto controller = desc.getValue<NIFReference>("Controller").ptr; controller; controller = std::get<NIFDictionary>(*controller).getValue<NIFReference>("Next Controller").ptr) {
			const auto &controllerDict = std::get<NIFDictionary>(*controller);
			
			if (controllerDict.kindOf("NiKeyframeController")) {
				if (std::get<NIFDictionary>(*controller).data.count(Symbol("Data")) != 0) {
					const auto& dataPtr = controllerDict.getValue<NIFReference>("Data").ptr;
					if (dataPtr) {
						auto& keyfData = std::get<NIFDictionary>(*controllerDict.getValue<NIFReference>("Data").ptr);

						auto rotationKeys = keyfData.getValue<uint32_t>("Num Rotation Keys");
						auto translationKeys = keyfData.getValue<NIFDictionary>("Translations").getValue<uint32_t>("Num Keys");
						auto scaleKeys = keyfData.getValue<NIFDictionary>("Scales").getValue<uint32_t>("Num Keys");

						if (rotationKeys < 2 && translationKeys < 2 && scaleKeys < 2) {
							fprintf(stderr, "%s: has degenerate keyframe controller, removing\n", nodeName(desc).c_str());

							// TODO: implement actual removal

						}
						else {
							m_allBones.emplace(node.ptr);
						}
					}
				}
			}
			else if (controllerDict.kindOf("NiMultiTargetTransformController")) {
				m_allBones.emplace(node.ptr);

				for (const auto &target : controllerDict.getValue<NIFArray>("Extra Targets").data) {
					const auto &targetPtr = std::get<NIFPointer>(target);

					std::shared_ptr<NIFVariant> targetVal(targetPtr.ptr);
					if(targetVal)
						m_allBones.emplace(targetVal);
				}

			}
		}

		if (desc.kindOf("NiNode")) {
			for (const auto &child : desc.getValue<NIFArray>("Children").data) {
				const auto &ref = std::get<NIFReference>(child);
				if (ref.ptr) {
					collectSkinsAndParents(ref, node.ptr);
				}
			}
		}
		else if (desc.kindOf("NiGeometry")) {
			Symbol symSkinInstance("Skin Instance");
			if (desc.data.count(symSkinInstance) != 0) {
				const auto &ref = desc.getValue<NIFReference>(symSkinInstance);
				if (ref.ptr) {
					SkinInfo skin;
					skin.geometry = node.ptr;
					skin.skin = ref.ptr;
					m_skins.emplace_back(std::move(skin));
				}
			}
		}
	}

	std::string SkeletonProcessor::nodeName(const NIFDictionary &node) const {
		return getString(node.getValue<NIFDictionary>("Name"), m_file->header());
	}
}
