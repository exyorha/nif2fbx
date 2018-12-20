#include "FBXNIFPlugin.h"

#include <fbxsdk/core/fbxmanager.h>
#include <fbxsdk/fileio/fbxiopluginregistry.h>

#include "NIFReader.h"

namespace fbxnif {
	FBXNIFPlugin::FBXNIFPlugin(const fbxsdk::FbxPluginDef &definition, fbxsdk::FbxModule moduleHandle) : FbxPlugin(definition, moduleHandle) {

	}

	bool FBXNIFPlugin::SpecificInitialize() {
		int firstPluginID, registeredCount;

		GetData().mSDKManager->GetIOPluginRegistry()->RegisterReader(NIFReader::Create, NIFReader::GetInfo, firstPluginID, registeredCount,  NIFReader::IOSettingsFiller);

		return true;
	}

	bool FBXNIFPlugin::SpecificTerminate() {
		return true;
	}

	FBXSDK_PLUGIN_IMPLEMENT(FBXNIFPlugin)
}