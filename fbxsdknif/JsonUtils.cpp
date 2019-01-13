#include "JsonUtils.h"

#include <json.h>

namespace fbxnif {

	Json::Value toJsonValue(const FbxDouble3 &value) {
		auto result = Json::Value(Json::arrayValue);
		result[0] = value[0];
		result[1] = value[1];
		result[2] = value[2];
		return result;
	}
}

