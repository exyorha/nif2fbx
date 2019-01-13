#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include "FBXNIFPluginNS.h"
#include <json-forwards.h>

#include <fbxsdk/core/fbxdatatypes.h>

namespace fbxnif {
	Json::Value toJsonValue(const FbxVector2 &value);
	Json::Value toJsonValue(const FbxVector4 &value);
	Json::Value toJsonValue(const FbxDouble3 &value);
}

#endif
