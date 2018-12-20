#include <fbxsdk/core/fbxmanager.h>
#include <fbxsdk/core/base/fbxutils.h>
#include <fbxsdk/core/arch/fbxarch.h>
#include <fbxsdk/fileio/fbxiosettings.h>
#include <fbxsdk/fileio/fbximporter.h>
#include <fbxsdk/fileio/fbxexporter.h>
#include <fbxsdk/fileio/fbx/fbxio.h>
#include <fbxsdk/fileio/fbxiopluginregistry.h>

#include <fbxsdk/scene/fbxscene.h>

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
	
	auto importer = fbxsdk::FbxImporter::Create(manager, "");
	auto status = importer->Initialize("C:\\projects\\nifparse\\meshes\\azura.nif", -1, manager->GetIOSettings());
	if (!status) {
		fprintf(stderr, "FbxImporter::Initialize failed: %s\n", importer->GetStatus().GetErrorString());
		return 1;
	}

	auto scene = fbxsdk::FbxScene::Create(manager, "myScene");
	status = importer->Import(scene);
	if (!status) {
		fprintf(stderr, "FbxImporter::Import failed: %s\n", importer->GetStatus().GetErrorString());
		return 1;
	}

	importer->Destroy();

	int formatIndex = -1;
	/*int count = manager->GetIOPluginRegistry()->GetWriterFormatCount();
	for (int index = 0; index < count; index++) {
		auto desc = manager->GetIOPluginRegistry()->GetWriterFormatDescription(index);
		if (strstr(desc, "FBX ascii")) {
			formatIndex = index;
			break;
		}
	}*/

	auto exporter = fbxsdk::FbxExporter::Create(manager, "");
	exporter->SetFileExportVersion(FBX_2013_00_COMPATIBLE);
	status = exporter->Initialize("C:\\projects\\nifparse\\meshes\\ex_hlaalu_b_01.fbx", formatIndex, manager->GetIOSettings());
	if (!status) {
		fprintf(stderr, "FbxExporter::Initialize failed: %s\n", exporter->GetStatus().GetErrorString());
		return 1;
	}

	status = exporter->Export(scene);
	if (!status) {
		fprintf(stderr, "FbxExporter::Export failed: %s\n", exporter->GetStatus().GetErrorString());
		return 1;
	}
	exporter->Destroy();

	manager->Destroy();
}