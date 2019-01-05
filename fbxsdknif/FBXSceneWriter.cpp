#include <fbxsdk/scene/fbxscene.h>
#include <fbxsdk/scene/geometry/fbxnode.h>
#include <fbxsdk/scene/geometry/fbxmesh.h>
#include <fbxsdk/scene/geometry/fbxskeleton.h>
#include <fbxsdk/scene/geometry/fbxskin.h>
#include <fbxsdk/scene/geometry/fbxcluster.h>
#include <fbxsdk/fileio/fbximporter.h>
#include <fbxsdk/utils/fbxclonemanager.h>
#include <fbxsdk/scene/animation/fbxanimstack.h>
#include <fbxsdk/scene/animation/fbxanimlayer.h>
#include <fbxsdk/scene/animation/fbxanimcurve.h>
#include <fbxsdk/scene/animation/fbxanimcurvenode.h>
#include <fbxsdk/scene/animation/fbxanimcurvefilters.h>

#include <nifparse/NIFFile.h>
#include <nifparse/PrettyPrinter.h>

#include <array>

#include "FBXSceneWriter.h"
#include "NIFUtils.h"
#include "SkeletonProcessor.h"

namespace fbxnif {
	FBXSceneWriter::FBXSceneWriter(const NIFFile &file, const SkeletonProcessor &skeleton) : m_file(file), m_skeleton(skeleton) {

	}

	FBXSceneWriter::~FBXSceneWriter() {

	}

	void FBXSceneWriter::write(FbxDocument *document) {
		m_scene = FbxCast<FbxScene>(document);

		m_meshesGenerated = 0;
		m_skeletonNodesGenerated = 0;
		m_skeletonImported = false;

		if (m_file.rootObjects().data.empty()) {
			throw std::runtime_error("no root object in NIF");
		}
			   
		FbxAxisSystem(FbxAxisSystem::eZAxis, FbxAxisSystem::eParityOdd, FbxAxisSystem::eRightHanded).ConvertScene(m_scene);
		FbxSystemUnit(1.4287109375).ConvertScene(m_scene);

		const auto &root = std::get<NIFReference>(m_file.rootObjects().data.front());
		const auto &rootDict = std::get<NIFDictionary>(*root.ptr);

		if (rootDict.kindOf("NiAVObject")) {
			convertSceneNode(root, m_scene->GetRootNode());
		}
		else if (rootDict.kindOf("NiSequence")) {
			ensureSkeletonImported(m_scene->GetRootNode());

			processControllerSequence(rootDict, NIFReference());

			m_meshesGenerated++;
		}

		for (const auto manager : m_controllerManagers) {
			const auto &palette = manager->getValue<NIFReference>("Object Palette");
			const auto &sequences = manager->getValue<NIFArray>("Controller Sequences");

			for (const auto &obj : sequences.data) {
				const auto &ref = std::get<NIFReference>(obj);

				if (ref.ptr) {
					processControllerSequence(std::get<NIFDictionary>(*ref.ptr), palette);
				}
			}
		}

		if (m_meshesGenerated == 0 && m_skeletonNodesGenerated >= 0 /* && m_animationsGenerated == 0 */) {
			fprintf(stderr, "FBXSceneWriter: skeleton-only FBX generated, adding null geometry\n");

			auto firstRootChild = m_scene->GetRootNode()->GetChild(0);
			if (!firstRootChild) {
				auto node = FbxNode::Create(m_scene, "DummyGeneratedNode");
				firstRootChild->AddChild(node);
				auto mesh = FbxMesh::Create(m_scene, "DummyGeneratedNode Mesh");
				node->AddNodeAttribute(mesh);
				mesh->InitControlPoints(1);
				mesh->SetControlPointAt(FbxVector4(0.0f, 0.0f, 0.0f, 0.0f), 0);
				mesh->ReservePolygonCount(1);
				mesh->BeginPolygon();
				mesh->AddPolygon(0); mesh->AddPolygon(0); mesh->AddPolygon(0);
				mesh->EndPolygon();
				auto it = m_nodeMap.find(m_skeleton.commonBoneRoot());
				auto root = it->second;
				auto fbxSkin = FbxSkin::Create(m_scene, "");
				auto cluster = FbxCluster::Create(m_scene, "");
				cluster->SetLink(root);
				cluster->SetLinkMode(FbxCluster::eTotalOne);
				cluster->AddControlPointIndex(0, 1.0);
				fbxSkin->AddCluster(cluster);
				mesh->AddDeformer(fbxSkin);
			}
		}
	}

	void FBXSceneWriter::ensureSkeletonImported(FbxNode *node) {
		if (!m_skeletonFile.IsEmpty() && !m_skeletonImported) {
			m_skeletonImported = true;

			auto manager = m_scene->GetFbxManager();
			auto ios = FbxIOSettings::Create(manager, IOSROOT);
			ios->SetBoolProp(IMP_FBX_EXT_SDK_GRP "|FBXSDKNIF|SkeletonImport", true);

			auto importer = FbxImporter::Create(manager, "");
			auto status = importer->Initialize(m_skeletonFile, -1, ios);
			if (!status) {
				std::stringstream error;
				error << "FbxImporter::Initialize failed: " << importer->GetStatus().GetErrorString();
				throw std::runtime_error(error.str());
			}

			auto skeletonScene = FbxScene::Create(manager, "");

			status = importer->Import(skeletonScene);
			if (!status) {
				std::stringstream error;
				error << "FbxImporter::Import failed: " << importer->GetStatus().GetErrorString();
				skeletonScene->Destroy();
				importer->Destroy();
				ios->Destroy();
				throw std::runtime_error(error.str());
			}

			importer->Destroy();

			ios->Destroy();

			auto skeletonRoot = findSkeletonRoot(skeletonScene->GetRootNode());

			if (!skeletonRoot) {
				skeletonScene->Destroy();

				throw std::runtime_error("skeleton root not found in imported skeleton");
			}

			auto newSkeletonRoot = static_cast<FbxNode *>(FbxCloneManager::Clone(skeletonRoot, m_scene));

			skeletonScene->Destroy();

			node->AddChild(newSkeletonRoot);

			registerImportedBones(newSkeletonRoot);
		}
	}

	void FBXSceneWriter::convertNiNode(const NIFDictionary &dict, fbxsdk::FbxNode *node) {
		ensureSkeletonImported(node);

		if (dict.typeChain.front() != Symbol("NiNode")) {
			fprintf(stderr, "FBXSceneWriter: %s: unsupported NiNode subclass interpreted as NiNode: %s\n", node->GetName(), dict.typeChain.front().toString());
		}
		
		for (const auto &child : dict.getValue<NIFArray>("Children").data) {
			auto childRef = std::get<NIFReference>(child);
			if (!childRef.ptr)
				continue;

			convertSceneNode(childRef, node);
		}
	}

	void FBXSceneWriter::registerImportedBones(FbxNode *bone) {
		auto skeleton = bone->GetSkeleton();
		if (skeleton) {
			m_importedBoneMap.emplace(bone->GetName(), bone);
		}

		for (int index = 0, count = bone->GetChildCount(); index < count; index++) {
			registerImportedBones(bone->GetChild(index));
		}
	}

	FbxNode *FBXSceneWriter::findSkeletonRoot(FbxNode *parent) {
		auto skeleton = parent->GetSkeleton();
		if (skeleton)
			return parent;

		for (int index = 0, count = parent->GetChildCount(); index < count; index++) {
			auto child = parent->GetChild(index);
			auto skeleton = findSkeletonRoot(child);
			if (skeleton)
				return skeleton;
		}

		return nullptr;
	}

	void FBXSceneWriter::convertSceneNode(const NIFReference &var, fbxsdk::FbxNode *containingNode) {
		const auto &dict = std::get<NIFDictionary>(*var.ptr);
		if (!dict.kindOf("NiAVObject")) {
			throw std::runtime_error("scene node is not an instance of NiAVObject");
		}

		const auto &name = getString(dict.getValue<NIFDictionary>("Name"), m_file.header());

		auto it = m_importedBoneMap.find(name);
		if (it != m_importedBoneMap.end()) {
			fprintf(stderr, "FBXSceneWriter: '%s' is replaced by the imported skeleton\n", name.c_str());

			m_nodeMap.emplace(var.ptr, it->second);
			return;
		}

		bool forceHidden = dict.isA("RootCollisionNode");

		auto node = FbxNode::Create(m_scene, name.c_str());
		containingNode->AddChild(node);

		m_nodeMap.emplace(var.ptr, node);

		fprintf(stderr, "%s: %s\n", node->GetName(), dict.typeChain.front().toString());

		node->Visibility = (dict.getValue<uint32_t>("Flags") & NiAVObjectFlagHidden) == 0 && !forceHidden;
		node->LclTranslation = getVector3(dict.getValue<NIFDictionary>("Translation"));
		node->LclRotation = getMatrix3x3(dict.getValue<NIFDictionary>("Rotation")).GetR();
		node->LclScaling = FbxDouble3(dict.getValue<float>("Scale"));

		if (m_skeleton.allBones().count(var.ptr) != 0) {
			auto skeleton = FbxSkeleton::Create(m_scene, (std::string(node->GetName()) + " Skeleton").c_str());
			node->AddNodeAttribute(skeleton);

			if (m_skeleton.commonBoneRoot() == var.ptr) {
				skeleton->SetSkeletonType(FbxSkeleton::eRoot);
			}
			else {
				skeleton->SetSkeletonType(FbxSkeleton::eLimbNode);
			}

			m_skeletonNodesGenerated++;
		}

		for (auto controller = dict.getValue<NIFReference>("Controller"); controller.ptr; controller = std::get<NIFDictionary>(*controller.ptr).getValue<NIFReference>("Next Controller")) {
			processController(std::get<NIFDictionary>(*controller.ptr), node);
		}

		if (dict.kindOf("NiNode")) {
			convertNiNode(dict, node);
		}
		else if (dict.kindOf("NiTriBasedGeom")) {
			convertNiTriBasedGeom(dict, node);
		}
		else if (dict.isA("BSTriShape")) {
			convertBSTriShape(dict, node);
		}
		else {
			fprintf(stderr, "FBXSceneWriter: %s: unsupported type: %s\n", node->GetName(), dict.typeChain.front().toString());
		}
	}
	
	template<typename ElementType>
	void FBXSceneWriter::importVectorElement(const NIFDictionary &data, FbxMesh *mesh, const Symbol &name, ElementType *(FbxGeometryBase::*createElement)()) {
		if (data.data.count(name) != 0) {
			const auto &vectors = data.getValue<NIFArray>(name);

			auto vectorElement = (mesh->*createElement)();
			vectorElement->SetMappingMode(FbxGeometryElement::eByControlPoint);
			vectorElement->SetReferenceMode(FbxGeometryElement::eDirect);

			auto &vectorData = vectorElement->GetDirectArray();
			vectorData.Resize(static_cast<int>(vectors.data.size()));

			for (size_t index = 0, size = vectors.data.size(); index < size; index++) {
				vectorData.SetAt(static_cast<int>(index), getVector3(std::get<NIFDictionary>(vectors.data[index])));
			}
		}
	}

	void FBXSceneWriter::convertNiTriBasedGeom(const NIFDictionary &dict, fbxsdk::FbxNode *node) {
		Symbol symVertices("Vertices");
		Symbol symVertexColors("Vertex Colors");

		auto mesh = FbxMesh::Create(m_scene, (std::string(node->GetName()) + " Mesh").c_str());
		node->AddNodeAttribute(mesh);

		m_meshesGenerated++;
		
		const auto &data = std::get<NIFDictionary>(*dict.getValue<NIFReference>("Data").ptr);

		/*
		 * NiGeometry data
		 */

		if (data.data.count(symVertices) != 0) {
			const auto &vertices = data.getValue<NIFArray>(symVertices);

			mesh->InitControlPoints(static_cast<int>(vertices.data.size()));
			auto controlPoints = mesh->GetControlPoints();
			for (size_t index = 0, size = vertices.data.size(); index < size; index++) {
				controlPoints[index] = getVector3(std::get<NIFDictionary>(vertices.data[index]));
			}
		}

		importVectorElement(data, mesh, "Normals", &FbxMesh::CreateElementNormal);
		importVectorElement(data, mesh, "Tangents", &FbxMesh::CreateElementTangent);
		importVectorElement(data, mesh, "Bitangents", &FbxMesh::CreateElementBinormal);

		if (data.data.count(symVertexColors) != 0) {
			const auto &vertexColors = data.getValue<NIFArray>(symVertexColors);

			auto colorElement = mesh->CreateElementVertexColor();
			colorElement->SetMappingMode(FbxGeometryElement::eByControlPoint);
			colorElement->SetReferenceMode(FbxGeometryElement::eDirect);

			auto &colorData = colorElement->GetDirectArray();
			colorData.Resize(static_cast<int>(vertexColors.data.size()));
			for (size_t index = 0, size = vertexColors.data.size(); index < size; index++) {
				colorData.SetAt(static_cast<int>(index), getColor4(std::get<NIFDictionary>(vertexColors.data[index])));
			}
		}

		const auto &uvSets = data.getValue<NIFArray>("UV Sets");
		for (size_t uvSetIndex = 0, uvSetCount = uvSets.data.size(); uvSetIndex < uvSetCount; uvSetIndex++) {
			const auto &uvSet = std::get<NIFArray>(uvSets.data[uvSetIndex]);

			std::stringstream uvName;
			uvName << "UV" << uvSetIndex;
			
			auto uv = mesh->CreateElementUV(uvName.str().c_str(), FbxLayerElement::eTextureDiffuse);
			uv->SetMappingMode(FbxGeometryElement::eByControlPoint);
			uv->SetReferenceMode(FbxGeometryElement::eDirect);

			auto &uvData = uv->GetDirectArray();
			uvData.Resize(static_cast<int>(uvSet.data.size()));
			for (size_t index = 0, size = uvSet.data.size(); index < size; index++) {
				uvData.SetAt(static_cast<int>(index), getTexCoord(std::get<NIFDictionary>(uvSet.data[static_cast<int>(index)])));
			}
		}

		/*
		 * Type-specific data
		 */




		if (data.isA("NiTriShapeData")) {
			importMeshTriangles(mesh, data);
		}
		else {
			fprintf(stderr, "%s: unknown type of geometry data: %s\n",
				mesh->GetName(), data.typeChain.front().toString());
		}

		/*
		 * Skinning
		 */

		Symbol symSkinInstance("Skin Instance");
		if (dict.data.count(symSkinInstance) != 0) {
			const auto &skinPtr = dict.getValue<NIFReference>(symSkinInstance).ptr;
			if (skinPtr) {
				const auto &skinInstance = std::get<NIFDictionary>(*skinPtr);
				const auto &skinData = std::get<NIFDictionary>(*skinInstance.getValue<NIFReference>("Data").ptr);

				auto skin = FbxSkin::Create(m_scene, (std::string(mesh->GetName()) + " Skin").c_str());

				const auto &skinDataBones = skinData.getValue<NIFArray>("Bone List").data;
				
				size_t boneIndex = 0;
				for (const auto &bone : skinInstance.getValue<NIFArray>("Bones").data) {
					std::shared_ptr<NIFVariant> bonePtr(std::get<NIFPointer>(bone).ptr);

					auto cluster = FbxCluster::Create(m_scene, "");

					auto it = m_nodeMap.find(bonePtr);
					if (it == m_nodeMap.end()) {
						throw std::logic_error("bone is not in the node map");
					}

					cluster->SetLink(it->second);
					cluster->SetLinkMode(FbxCluster::eTotalOne);

					const auto &boneData = std::get<NIFDictionary>(skinDataBones[boneIndex]);

					cluster->SetTransformMatrix(
						getTransform(boneData.getValue<NIFDictionary>("Skin Transform"))
					);

					for (const auto &weight : boneData.getValue<NIFArray>("Vertex Weights").data) {
						const auto &weightDict = std::get<NIFDictionary>(weight);

						cluster->AddControlPointIndex(
							weightDict.getValue<uint32_t>("Index"),
							weightDict.getValue<float>("Weight")
						);
					}

					skin->AddCluster(cluster);

					boneIndex++;
				}

				mesh->AddDeformer(skin);
			}
		}
	}
	
	void FBXSceneWriter::importMeshTriangles(FbxMesh *mesh, const NIFDictionary &container) {

		Symbol symTriangles("Triangles");
		Symbol symV1("v1");
		Symbol symV2("v2");
		Symbol symV3("v3");

		if (container.data.count(symTriangles) != 0) {
			const auto &triangles = container.getValue<NIFArray>(symTriangles);

			mesh->ReservePolygonCount(static_cast<int>(triangles.data.size()));
			mesh->ReservePolygonVertexCount(static_cast<int>(3 * triangles.data.size()));

			for (const auto &triangleValue : triangles.data) {
				const auto &triangle = std::get<NIFDictionary>(triangleValue);

				mesh->BeginPolygon(-1, -1, -1, false);

				mesh->AddPolygon(triangle.getValue<uint32_t>(symV1));
				mesh->AddPolygon(triangle.getValue<uint32_t>(symV2));
				mesh->AddPolygon(triangle.getValue<uint32_t>(symV3));

				mesh->EndPolygon();
			}
		}
	}

	void FBXSceneWriter::convertBSTriShape(const NIFDictionary &dict, fbxsdk::FbxNode *node) {
		auto mesh = FbxMesh::Create(m_scene, (std::string(node->GetName()) + " Mesh").c_str());
		node->AddNodeAttribute(mesh);

		m_meshesGenerated++;

		auto numVertices = dict.getValue<uint32_t>("Num Vertices");

		mesh->InitControlPoints(static_cast<int>(numVertices));

		const auto &vertexAttributes = dict.getValue<NIFDictionary>("Vertex Desc").getValue<NIFBitflags>("Vertex Attributes");

		nifparse::PrettyPrinter prettyPrinter(std::cerr);
		prettyPrinter.print(vertexAttributes);

		Symbol symVertexData("Vertex Data");
		Symbol symTriangles("Triangles");

		if (dict.data.count(symVertexData) != 0) {
			const auto &vertexData = dict.getValue<NIFArray>(symVertexData);

			Symbol symVertex("Vertex");
			Symbol symUVs("UVs");
			Symbol symUV("UV");
			Symbol symNormals("Normals");
			Symbol symNormal("Normal");
			Symbol symTangents("Tangents");
			Symbol symTangent("Tangent");
			Symbol symBitangentX("Bitangent X");
			Symbol symBitangentY("Bitangent Y");
			Symbol symBitangentZ("Bitangent Z");
			Symbol symVertex_Colors("Vertex_Colors");
			Symbol symVertexColors("Vertex Colors");

			for (const auto &attribute : vertexAttributes.symbolicValues) {
				if (attribute == symVertex) {
					auto controlPoints = mesh->GetControlPoints();

					for (size_t index = 0, size = numVertices; index < size; index++) {
						controlPoints[index] = getVector3(std::get<NIFDictionary>(vertexData.data[index]).getValue<NIFDictionary>(symVertex));
					}
				}
				else if (attribute == symUVs) {
					auto uv = mesh->CreateElementUV("UV0", FbxLayerElement::eTextureDiffuse);
					uv->SetMappingMode(FbxGeometryElement::eByControlPoint);
					uv->SetReferenceMode(FbxGeometryElement::eDirect);

					auto &uvData = uv->GetDirectArray();
					uvData.Resize(static_cast<int>(numVertices));
					for (size_t index = 0; index < numVertices; index++) {
						uvData.SetAt(static_cast<int>(index), getTexCoord(std::get<NIFDictionary>(vertexData.data[index]).getValue<NIFDictionary>(symUV)));
					}
				}
				else if (attribute == symNormals) {
					auto normals = mesh->CreateElementNormal();
					normals->SetMappingMode(FbxGeometryElement::eByControlPoint);
					normals->SetReferenceMode(FbxGeometryElement::eDirect);

					auto &vectorData = normals->GetDirectArray();
					vectorData.Resize(static_cast<int>(numVertices));

					for (size_t index = 0; index < numVertices; index++) {
						vectorData.SetAt(static_cast<int>(index), getByteVector3(std::get<NIFDictionary>(vertexData.data[index]).getValue<NIFDictionary>(symNormal)));
					}
				}
				else if (attribute == symTangents) {
					auto tangents = mesh->CreateElementTangent();
					tangents->SetMappingMode(FbxGeometryElement::eByControlPoint);
					tangents->SetReferenceMode(FbxGeometryElement::eDirect);

					auto binormals = mesh->CreateElementBinormal();
					binormals->SetMappingMode(FbxGeometryElement::eByControlPoint);
					binormals->SetReferenceMode(FbxGeometryElement::eDirect);

					auto &tangentData = tangents->GetDirectArray();
					tangentData.Resize(static_cast<int>(numVertices));

					auto &binormalData = binormals->GetDirectArray();
					binormalData.Resize(static_cast<int>(numVertices));

					for (size_t index = 0; index < numVertices; index++) {
						const auto &vertex = std::get<NIFDictionary>(vertexData.data[index]);

						tangentData.SetAt(static_cast<int>(index), getByteVector3(vertex.getValue<NIFDictionary>(symTangent)));
						binormalData.SetAt(static_cast<int>(index), FbxVector4(
							vertex.getValue<float>(symBitangentX),
							getSignedFloatFromU8(vertex.getValue<uint32_t>(symBitangentY)),
							getSignedFloatFromU8(vertex.getValue<uint32_t>(symBitangentZ))
						));
					}
				}
				else if (attribute == symVertex_Colors) {
					auto colors = mesh->CreateElementVertexColor();
					colors->SetMappingMode(FbxGeometryElement::eByControlPoint);
					colors->SetReferenceMode(FbxGeometryElement::eDirect);

					auto &colorData = colors->GetDirectArray();
					colorData.Resize(static_cast<int>(numVertices));
					for (size_t index = 0; index < numVertices; index++) {
						const auto &vertex = std::get<NIFDictionary>(vertexData.data[index]);

						colorData.SetAt(static_cast<int>(index), getByteColor4(vertex.getValue<NIFDictionary>(symVertexColors)));
					}

				}
				else {
					fprintf(stderr, "Unsupported vertex attribute: %s\n", attribute.toString());
				}
			}
		}

		importMeshTriangles(mesh, dict);
	}


	template<typename PropertyType>
	void FBXSceneWriter::generateCurves(const NIFDictionary &keyGroup, FbxPropertyT<PropertyType> &prop, FbxAnimLayer *layer, CurveGenerationMode mode, FbxAnimCurveNode *&node) {
		auto numKeys = keyGroup.getValue<uint32_t>("Num Keys");
		if (numKeys > 0) {

			const auto &interpolation = keyGroup.getValue<NIFEnum>("Interpolation");
			const auto &keys = keyGroup.getValue<NIFArray>("Keys");

			generateCurves(interpolation, keys, prop, layer, mode, node);
		}
	}

	template<typename PropertyType>
	void FBXSceneWriter::generateCurves(const NIFEnum &interpolation, const NIFArray &keys, FbxPropertyT<PropertyType> &prop, FbxAnimLayer *layer, CurveGenerationMode mode, FbxAnimCurveNode *&node) {
		if (!node) {
			node = prop.CreateCurveNode(layer);
		}

		std::array<FbxAnimCurve *, 3> curves{ nullptr, nullptr, nullptr };

		if (mode == CurveGenerationMode::Translation || mode == CurveGenerationMode::RotationQuaternion) {
			curves[0] = FbxAnimCurve::Create(m_scene, "");
			curves[1] = FbxAnimCurve::Create(m_scene, "");
			curves[2] = FbxAnimCurve::Create(m_scene, "");

			node->ConnectToChannel(curves[0], 0U);
			node->ConnectToChannel(curves[1], 1U);
			node->ConnectToChannel(curves[2], 2U);
		}
		else if (mode == CurveGenerationMode::Scaling) {
			curves[0] = FbxAnimCurve::Create(m_scene, "");

			node->ConnectToChannel(curves[0], 0U);
			node->ConnectToChannel(curves[0], 1U);
			node->ConnectToChannel(curves[0], 2U);
		}
		else if (mode == CurveGenerationMode::RotationX) {
			curves[0] = FbxAnimCurve::Create(m_scene, "");
			node->ConnectToChannel(curves[0], 0U);
		}
		else if (mode == CurveGenerationMode::RotationY) {
			curves[0] = FbxAnimCurve::Create(m_scene, "");
			node->ConnectToChannel(curves[0], 1U);
		}
		else if (mode == CurveGenerationMode::RotationZ) {
			curves[0] = FbxAnimCurve::Create(m_scene, "");
			node->ConnectToChannel(curves[0], 2U);
		}

		for (auto curve : curves) {
			if (curve) {
				curve->KeyModifyBegin();
			}
		}

		for (const auto &keyVal : keys.data) {
			const auto &key = std::get<NIFDictionary>(keyVal);

			FbxTime time;
			time.SetSecondDouble(key.getValue<float>("Time"));

			if (mode == CurveGenerationMode::Translation) {
				const auto &value = getVector3(key.getValue<NIFDictionary>("Value"));

				FbxAnimCurveKey xKey;
				FbxAnimCurveKey yKey;
				FbxAnimCurveKey zKey;

				if (interpolation.symbolicValue == Symbol("TBC_KEY")) {
					const auto &tbc = key.getValue<NIFDictionary>("TBC");

					xKey.SetInterpolation(FbxAnimCurveDef::eInterpolationCubic);
					xKey.SetTangentMode(FbxAnimCurveDef::eTangentTCB);
					xKey.SetTCB(time, static_cast<float>(value[0]),
						tbc.getValue<float>("t"),
						tbc.getValue<float>("c"),
						tbc.getValue<float>("b"));

					yKey.SetInterpolation(FbxAnimCurveDef::eInterpolationCubic);
					yKey.SetTangentMode(FbxAnimCurveDef::eTangentTCB);
					yKey.SetTCB(time, static_cast<float>(value[1]),
						tbc.getValue<float>("t"),
						tbc.getValue<float>("c"),
						tbc.getValue<float>("b"));

					zKey.SetInterpolation(FbxAnimCurveDef::eInterpolationCubic);
					zKey.SetTangentMode(FbxAnimCurveDef::eTangentTCB);
					zKey.SetTCB(time, static_cast<float>(value[2]),
						tbc.getValue<float>("t"),
						tbc.getValue<float>("c"),
						tbc.getValue<float>("b"));
				}
				else if (interpolation.symbolicValue == Symbol("QUADRATIC_KEY")) {
					const auto &ftangent = getVector3(key.getValue<NIFDictionary>("Forward"));
					const auto &btangent = getVector3(key.getValue<NIFDictionary>("Backward"));

					xKey.SetInterpolation(FbxAnimCurveDef::eInterpolationCubic);
					xKey.SetTangentMode(FbxAnimCurveDef::eTangentUser);
					xKey.Set(time, static_cast<float>(value[0]));
					xKey.SetDataFloat(FbxAnimCurveDef::eNextLeftSlope, static_cast<float>(ftangent[0]));
					xKey.SetDataFloat(FbxAnimCurveDef::eRightSlope, static_cast<float>(btangent[0]));

					yKey.SetInterpolation(FbxAnimCurveDef::eInterpolationCubic);
					yKey.SetTangentMode(FbxAnimCurveDef::eTangentUser);
					yKey.Set(time, static_cast<float>(value[1]));
					yKey.SetDataFloat(FbxAnimCurveDef::eNextLeftSlope, static_cast<float>(ftangent[1]));
					yKey.SetDataFloat(FbxAnimCurveDef::eRightSlope, static_cast<float>(btangent[1]));

					zKey.SetInterpolation(FbxAnimCurveDef::eInterpolationCubic);
					zKey.SetTangentMode(FbxAnimCurveDef::eTangentUser);
					zKey.Set(time, static_cast<float>(value[2]));
					zKey.SetDataFloat(FbxAnimCurveDef::eNextLeftSlope, static_cast<float>(ftangent[2]));
					zKey.SetDataFloat(FbxAnimCurveDef::eRightSlope, static_cast<float>(btangent[2]));
				}
				else { // LINEAR_KEY and any others
					xKey.Set(time, static_cast<float>(value[0]));
					yKey.Set(time, static_cast<float>(value[1]));
					zKey.Set(time, static_cast<float>(value[2]));
				}

				curves[0]->KeyAdd(time, xKey);
				curves[1]->KeyAdd(time, yKey);
				curves[2]->KeyAdd(time, zKey);
			}
			else if (mode == CurveGenerationMode::Scaling || mode == CurveGenerationMode::RotationX || mode == CurveGenerationMode::RotationY || mode == CurveGenerationMode::RotationZ) {
				auto value = key.getValue<float>("Value");

				FbxAnimCurveKey skey;

				if (interpolation.symbolicValue == Symbol("TBC_KEY")) {
					const auto &tbc = key.getValue<NIFDictionary>("TBC");

					skey.SetInterpolation(FbxAnimCurveDef::eInterpolationCubic);
					skey.SetTangentMode(FbxAnimCurveDef::eTangentTCB);
					skey.SetTCB(time, value,
						tbc.getValue<float>("t"),
						tbc.getValue<float>("c"),
						tbc.getValue<float>("b"));

				}
				else if (interpolation.symbolicValue == Symbol("QUADRATIC_KEY")) {
					auto ftangent = key.getValue<float>("Forward");
					auto btangent = key.getValue<float>("Backward");

					skey.SetInterpolation(FbxAnimCurveDef::eInterpolationCubic);
					skey.SetTangentMode(FbxAnimCurveDef::eTangentUser);
					skey.Set(time, static_cast<float>(value));
					skey.SetDataFloat(FbxAnimCurveDef::eNextLeftSlope, ftangent);
					skey.SetDataFloat(FbxAnimCurveDef::eRightSlope, btangent);
				}
				else { // LINEAR_KEY and any others
					skey.Set(time, static_cast<float>(value));
				}

				curves[0]->KeyAdd(time, skey);
			}
			else if (mode == CurveGenerationMode::RotationQuaternion) {
				FbxVector4 rotation;
				rotation.SetXYZ(getQuaternion(key.getValue<NIFDictionary>("Value")));

				FbxAnimCurveKey xKey;
				FbxAnimCurveKey yKey;
				FbxAnimCurveKey zKey;

				if (interpolation.symbolicValue == Symbol("TBC_KEY")) {
					const auto &tbc = key.getValue<NIFDictionary>("TBC");

					xKey.SetInterpolation(FbxAnimCurveDef::eInterpolationCubic);
					xKey.SetTangentMode(FbxAnimCurveDef::eTangentTCB);
					xKey.SetTCB(time, static_cast<float>(rotation[0]),
						tbc.getValue<float>("t"),
						tbc.getValue<float>("c"),
						tbc.getValue<float>("b"));

					yKey.SetInterpolation(FbxAnimCurveDef::eInterpolationCubic);
					yKey.SetTangentMode(FbxAnimCurveDef::eTangentTCB);
					yKey.SetTCB(time, static_cast<float>(rotation[1]),
						tbc.getValue<float>("t"),
						tbc.getValue<float>("c"),
						tbc.getValue<float>("b"));

					zKey.SetInterpolation(FbxAnimCurveDef::eInterpolationCubic);
					zKey.SetTangentMode(FbxAnimCurveDef::eTangentTCB);
					zKey.SetTCB(time, static_cast<float>(rotation[2]),
						tbc.getValue<float>("t"),
						tbc.getValue<float>("c"),
						tbc.getValue<float>("b"));
				}
				else { // LINEAR_KEY and any others
					xKey.Set(time, static_cast<float>(rotation[0]));
					yKey.Set(time, static_cast<float>(rotation[1]));
					zKey.Set(time, static_cast<float>(rotation[2]));
				}

				curves[0]->KeyAdd(time, xKey);
				curves[1]->KeyAdd(time, yKey);
				curves[2]->KeyAdd(time, zKey);
			}
		}

		for (auto curve : curves) {
			if (curve) {
				curve->KeyModifyEnd();
			}
		}

		if (mode == CurveGenerationMode::RotationQuaternion) {
			FbxAnimCurveFilterUnroll unroller;
			unroller.Apply(curves.data(), static_cast<int>(curves.size()));
		}
	}

	void FBXSceneWriter::processController(const NIFDictionary &controller, FbxNode *node) {
		if (controller.kindOf("NiKeyframeController")) {
			printf("Keyframe controller on %s\n", node->GetName());

			NIFReference data;

			if (controller.data.count("Interpolator") != 0) {
				const auto &interpolator = std::get<NIFDictionary>(*controller.getValue<NIFReference>("Interpolator").ptr);

				if (!interpolator.kindOf("NiTransformInterpolator")) {
					fprintf(stderr, "Unsupported interpolator on NiKeyframeController: %s\n", interpolator.typeChain.back().toString());
					return;
				}

				data = interpolator.getValue<NIFReference>("Data");

				if (!data.ptr) {
					const auto &quatTransform = getQuatTransform(interpolator.getValue<NIFDictionary>("Transform"));

					node->LclTranslation = quatTransform.GetT();
					node->LclRotation = quatTransform.GetR();
					node->LclScaling = quatTransform.GetS();
				}
			}
			else {
				data = controller.getValue<NIFReference>("Data");
			}

			if (!data.ptr) {
				fprintf(stderr, "Keyframe controller on %s has no data\n", node->GetName());

				return;
			}

			const auto &dataDict = std::get<NIFDictionary>(*data.ptr);

			auto layer = getDefaultTakelayer(getCurrentTake());

			auto numRotationKeys = dataDict.getValue<uint32_t>("Num Rotation Keys");
			if (numRotationKeys != 0) {
				const auto &rotationType = dataDict.getValue<NIFEnum>("Rotation Type");

				FbxAnimCurveNode *rotationNode = nullptr;

				if (rotationType.symbolicValue == Symbol("XYZ_ROTATION_KEY")) {
					auto &rotations = dataDict.getValue<NIFArray>("XYZ Rotations");

					generateCurves(std::get<NIFDictionary>(rotations.data[0]), node->LclRotation, layer, CurveGenerationMode::RotationX, rotationNode);
					generateCurves(std::get<NIFDictionary>(rotations.data[1]), node->LclRotation, layer, CurveGenerationMode::RotationY, rotationNode);
					generateCurves(std::get<NIFDictionary>(rotations.data[2]), node->LclRotation, layer, CurveGenerationMode::RotationZ, rotationNode);
				}
				else {
					generateCurves(
						rotationType,
						dataDict.getValue<NIFArray>("Quaternion Keys"),
						node->LclRotation,
						layer,
						CurveGenerationMode::RotationQuaternion,
						rotationNode);

				}
			}

			const auto &translations = dataDict.getValue<NIFDictionary>("Translations");

			FbxAnimCurveNode *translationNode = nullptr;
			generateCurves(translations, node->LclTranslation, layer, CurveGenerationMode::Translation, translationNode);

			const auto &scales = dataDict.getValue<NIFDictionary>("Scales");

			FbxAnimCurveNode *scaleNode = nullptr;
			generateCurves(scales, node->LclScaling, layer, CurveGenerationMode::Scaling, scaleNode);
		} else if(controller.kindOf("NiControllerManager")) {
			printf("NiControllerManager found, deferring\n");

			m_controllerManagers.emplace_back(&controller);
		}
		else {
			fprintf(stderr, "unsupported controller of type %s on node %s\n", controller.typeChain.front().toString(), node->GetName());
		}
	}

	auto FBXSceneWriter::getCurrentTake() -> AnimationTake & {
		if (m_animationTakes.empty()) {
			AnimationTake take;
			take.stack = FbxAnimStack::Create(m_scene, "default");

			m_animationTakes.emplace_back(std::move(take));
		}

		return m_animationTakes.back();
	}

	FbxAnimLayer *FBXSceneWriter::getDefaultTakelayer(AnimationTake &take) {
		if (!take.defaultLayer) {
			take.defaultLayer = FbxAnimLayer::Create(m_scene, "base layer");
			take.stack->AddMember(take.defaultLayer);
		}

		return take.defaultLayer;
	}

	void FBXSceneWriter::processControllerSequence(const NIFDictionary &sequence, const NIFReference &palette) {
		auto sequenceName = getString(sequence.getValue<NIFDictionary>("Name"), m_file.header());

		printf("Processing controller sequence %s\n", sequenceName.c_str());

		auto stack = FbxAnimStack::Create(m_scene, sequenceName.c_str());

		if (sequence.data.count("Start Time") != 0) {
			FbxTime startTime;
			startTime.SetSecondDouble(sequence.getValue<float>("Start Time"));

			FbxTime stopTime;
			stopTime.SetSecondDouble(sequence.getValue<float>("Stop Time"));

			stack->LocalStart = startTime;
			stack->LocalStop = stopTime;
			stack->ReferenceStart = startTime;
			stack->ReferenceStop = stopTime;
		}

		AnimationTake take;
		take.stack = stack;
		take.defaultLayer = nullptr;
		m_animationTakes.emplace_back(std::move(take));

		for (const auto &block : sequence.getValue<NIFArray>("Controlled Blocks").data) {
			const auto &blockDict = std::get<NIFDictionary>(block);

			NIFReference palette;

			if (blockDict.data.count("String Palette") != 0) {
				palette = blockDict.getValue<NIFReference>("String Palette");
			}

			std::string targetNode;

			if (blockDict.data.count("Target Name") != 0) {
				targetNode = getString(blockDict.getValue<NIFDictionary>("Target Name"), m_file.header());
			}
			else if (blockDict.data.count("Node Name Offset") != 0) {
				targetNode = getStringFromPalette(blockDict.getValue<NIFDictionary>("Target Name Offset"), std::get<NIFDictionary>(*palette.ptr));
			}
			else {
				targetNode = getString(blockDict.getValue<NIFDictionary>("Node Name"), m_file.header());				
			}

			printf("Controller block for %s\n", targetNode.c_str());

			auto it = m_importedBoneMap.find(targetNode);
			if (it == m_importedBoneMap.end()) {
				fprintf(stderr, "Node %s, required by NiSequence, is not present in skeleton\n", targetNode.c_str());
				continue;
			}

			auto node = it->second;

			auto controller = blockDict.getValue<NIFReference>("Controller");
			if (controller.ptr) {
				processController(std::get<NIFDictionary>(*controller.ptr), node);
			}
			else {
				std::string controllerTypeName;

				if (blockDict.data.count("Controller Type Offset") != 0) {
					controllerTypeName = getStringFromPalette(blockDict.getValue<NIFDictionary>("Controller Type Offset"), std::get<NIFDictionary>(*palette.ptr));
				}
				else {
					controllerTypeName = getString(blockDict.getValue<NIFDictionary>("Controller Type"), m_file.header());
				}

				NIFVariant controllerValue;
				controllerValue = NIFDictionary();
				auto &controller = std::get<NIFDictionary>(controllerValue);

				for (Symbol controllerType(controllerTypeName.c_str()); !controllerType.isNull(); controllerType = controllerType.parentType()) {
					controller.typeChain.push_back(controllerType);
				}

				printf("No existing controller, creating new. Type chain: ");

				for (const auto type : controller.typeChain) {
					printf("'%s' ", type.toString());
				}

				printf("\n");

				controller.data.emplace("Interpolator", blockDict.getValue<NIFReference>("Interpolator"));

				processController(controller, node);

			}
		}

		m_animationTakes.pop_back();

	}


}
