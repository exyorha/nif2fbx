#include "JsonUtils.h"

#include <json.h>

namespace fbxnif {

	Json::Value toJsonValue(const FbxVector2 &value) {
		auto result = Json::Value(Json::arrayValue);
		result[0] = value[0];
		result[1] = value[1];
		return result;
	}

	Json::Value toJsonValue(const FbxVector4 &value) {
		auto result = Json::Value(Json::arrayValue);
		result[0] = value[0];
		result[1] = value[1];
		result[2] = value[2];
		result[3] = value[3];
		return result;
	}

	Json::Value toJsonValue(const FbxDouble3 &value) {
		auto result = Json::Value(Json::arrayValue);
		result[0] = value[0];
		result[1] = value[1];
		result[2] = value[2];
		return result;
	}
}

