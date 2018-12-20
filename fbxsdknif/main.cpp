#include <fbxsdk/core/fbxmanager.h>
#include <fbxsdk/core/arch/fbxarch.h>
#include <fbxsdk/core/fbxplugincontainer.h>

#include "FBXNIFPlugin.h"

static fbxnif::FBXNIFPlugin *PluginInstance;

extern "C" {
	FBXSDK_DLLEXPORT void FBXPluginRegistration(fbxsdk::FbxPluginContainer &container, fbxsdk::FbxModule libHandle) {
		if (!PluginInstance) {
			fbxsdk::FbxPluginDef pluginDef;
			pluginDef.mName = "FBXSDKNIF";
			pluginDef.mVersion = "1.0";

			PluginInstance = fbxnif::FBXNIFPlugin::Create(pluginDef, libHandle);
			container.Register(*PluginInstance);
		}
	}
}
