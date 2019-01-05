#include <fbxsdk/core/fbxmanager.h>
#include <fbxsdk/core/base/fbxutils.h>
#include <fbxsdk/core/arch/fbxarch.h>
#include <fbxsdk/fileio/fbxiosettings.h>
#include <fbxsdk/fileio/fbximporter.h>
#include <fbxsdk/fileio/fbxexporter.h>
#include <fbxsdk/fileio/fbx/fbxio.h>
#include <fbxsdk/fileio/fbxiopluginregistry.h>

#include <fbxsdk/scene/fbxscene.h>

#include <functional>

bool convert(fbxsdk::FbxManager *manager, const char *from, const char *to, std::function<void(fbxsdk::FbxIOSettings *)> &&config) {

	auto ios = fbxsdk::FbxIOSettings::Create(manager, IOSROOT);

	config(ios);

	auto importer = fbxsdk::FbxImporter::Create(manager, "");
	auto status = importer->Initialize(from, -1, ios);
	if (!status) {
		fprintf(stderr, "FbxImporter::Initialize failed: %s\n", importer->GetStatus().GetErrorString());
		return false;
	}

	auto scene = fbxsdk::FbxScene::Create(manager, "myScene");
	status = importer->Import(scene);
	if (!status) {
		fprintf(stderr, "FbxImporter::Import failed: %s\n", importer->GetStatus().GetErrorString());
		return false;
	}

	importer->Destroy();

	ios->Destroy();

	auto exporter = fbxsdk::FbxExporter::Create(manager, "");
	exporter->SetFileExportVersion(FBX_2013_00_COMPATIBLE);
	status = exporter->Initialize(to, 1, manager->GetIOSettings());
	if (!status) {
		fprintf(stderr, "FbxExporter::Initialize failed: %s\n", exporter->GetStatus().GetErrorString());
		return false;
	}

	status = exporter->Export(scene);
	if (!status) {
		fprintf(stderr, "FbxExporter::Export failed: %s\n", exporter->GetStatus().GetErrorString());
		return false;
	}
	exporter->Destroy();

	scene->Destroy();

	return true;
}

int main(int argc, char *argv[]) {
	auto manager = fbxsdk::FbxManager::Create();
	
	auto ios = fbxsdk::FbxIOSettings::Create(manager, IOSROOT);
	manager->SetIOSettings(ios);

	// Load plugins from the executable directory
	auto lPath = fbxsdk::FbxGetApplicationDirectory();
#if defined(FBXSDK_ENV_WIN)
	auto lExtension = "dll";
#elif defined(FBXSDK_ENV_MACOSX)
	auto lExtension = "dylib";
#elif defined(FBXSDK_ENV_LINUX)
	auto lExtension = "so";
#endif
	manager->LoadPluginsDirectory(lPath.Buffer(), lExtension);
	/*
	if (!convert(manager, "C:\\projects\\nifparse\\meshes\\skeleton.nif", "C:\\projects\\nifparse\\meshes\\skeleton.fbx", [](fbxsdk::FbxIOSettings *ios) {
		ios->SetBoolProp(IMP_FBX_EXT_SDK_GRP "|FBXSDKNIF|SkeletonImport", true);
	}))
		return 1;


	if (!convert(manager, "C:\\projects\\nifparse\\meshes\\classic\\alduin.nif", "C:\\projects\\nifparse\\meshes\\alduin.fbx", [](fbxsdk::FbxIOSettings *ios) {
		ios->SetStringProp(IMP_FBX_EXT_SDK_GRP "|FBXSDKNIF|Skeleton", "C:\\projects\\nifparse\\meshes\\skeleton.fbx");
	}))
		return 1;
	*//*
	if (!convert(manager, "C:\\projects\\nifparse\\meshes\\classic\\mtidle_hover.kf", "C:\\projects\\nifparse\\meshes\\mtidle_hover.fbx", [](fbxsdk::FbxIOSettings *ios) {
		ios->SetStringProp(IMP_FBX_EXT_SDK_GRP "|FBXSDKNIF|Skeleton", "C:\\projects\\nifparse\\meshes\\skeleton.fbx");
	}))
		return 1;
	*/

	/*
	if (!convert(manager, "C:\\projects\\nifparse\\meshes\\azura.nif", "C:\\projects\\nifparse\\meshes\\azura.fbx", [](fbxsdk::FbxIOSettings *ios) {
		//ios->SetBoolProp(IMP_FBX_EXT_SDK_GRP "|FBXSDKNIF|SkeletonImport", true);
	}))
		return 1;*/
	/*
	if (!convert(manager, "C:\\projects\\nifparse\\meshes\\dwebrentrance01.nif", "C:\\projects\\nifparse\\meshes\\dwebrentrance01.fbx", [](fbxsdk::FbxIOSettings *ios) {
		//ios->SetBoolProp(IMP_FBX_EXT_SDK_GRP "|FBXSDKNIF|SkeletonImport", true);
	}))
		return 1;
	*/
	/*
	if (!convert(manager, "C:\\projects\\nifparse\\meshes\\azura.nif", "C:\\projects\\nifparse\\meshes\\azura.fbx", [](fbxsdk::FbxIOSettings *ios) {
		//ios->SetBoolProp(IMP_FBX_EXT_SDK_GRP "|FBXSDKNIF|SkeletonImport", true);
	}))
		return 1;
	*/

	if (!convert(manager, "C:\\projects\\nifparse\\meshes\\oblivion\\meshes\\creatures\\xivilai\\skeleton.nif", "C:\\projects\\nifparse\\meshes\\xivilai\\skeleton.fbx", [](fbxsdk::FbxIOSettings *ios) {
		//ios->SetStringProp(IMP_FBX_EXT_SDK_GRP "|FBXSDKNIF|Skeleton", "C:\\projects\\nifparse\\meshes\\skeleton.fbx");
	}))
		return 1;


	if (!convert(manager, "C:\\projects\\nifparse\\meshes\\oblivion\\meshes\\creatures\\xivilai\\xivilai.nif", "C:\\projects\\nifparse\\meshes\\xivilai\\xivilai.fbx", [](fbxsdk::FbxIOSettings *ios) {
		ios->SetStringProp(IMP_FBX_EXT_SDK_GRP "|FBXSDKNIF|Skeleton", "C:\\projects\\nifparse\\meshes\\xivilai\\skeleton.fbx");
	}))
		return 1;

	if (!convert(manager, "C:\\projects\\nifparse\\meshes\\oblivion\\meshes\\creatures\\xivilai\\idleanims\\idlescowl.kf", "C:\\projects\\nifparse\\meshes\\xivilai\\idlescowl.fbx", [](fbxsdk::FbxIOSettings *ios) {
		ios->SetStringProp(IMP_FBX_EXT_SDK_GRP "|FBXSDKNIF|Skeleton", "C:\\projects\\nifparse\\meshes\\xivilai\\skeleton.fbx");
	}))
		return 1;

	manager->Destroy();

	return 0;
}