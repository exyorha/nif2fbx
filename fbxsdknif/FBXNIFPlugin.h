#ifndef FBXNIFPLUGIN_H
#define FBXNIFPLUGIN_H

#include <fbxsdk/core/fbxplugin.h>
#include <fbxsdk/core/arch/fbxnew.h>
#include "FBXNIFPluginNS.h"

namespace fbxnif {

	class FBXNIFPlugin final : public FbxPlugin {
		FBXSDK_PLUGIN_DECLARE(FBXNIFPlugin)

	protected:
		FBXNIFPlugin(const FbxPluginDef &definition, FbxModule moduleHandle);

		virtual bool SpecificInitialize() override;
		virtual bool SpecificTerminate() override;
	};
}

#endif
