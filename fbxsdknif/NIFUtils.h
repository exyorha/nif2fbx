#ifndef NIF_UTILS_H
#define NIF_UTILS_H

#include "FBXNIFPluginNS.h"

#include <nifparse/Types.h>

#include <fbxsdk/core/math/fbxaffinematrix.h>
#include <fbxsdk/core/math/fbxvector2.h>
#include <fbxsdk/core/math/fbxvector4.h>
#include <fbxsdk/core/fbxpropertytypes.h>

namespace fbxnif {
	std::string getString(const NIFDictionary &dict, const NIFDictionary &header);
	std::string getStringFromPalette(uint32_t offset, const NIFDictionary &palette);
	FbxVector4 getVector3(const NIFDictionary &dict);
	FbxAMatrix getMatrix3x3(const NIFDictionary &dict);
	FbxDouble3 getColor3(const NIFDictionary &dict);
	FbxColor getColor4(const NIFDictionary &dict);
	FbxColor getByteColor4(const NIFDictionary &dict);
	FbxVector2 getTexCoord(const NIFDictionary &dict);
	FbxAMatrix getTransform(const NIFDictionary &dict);
	FbxAMatrix getQuatTransform(const NIFDictionary &dict);
	FbxVector4 getByteVector3(const NIFDictionary &dict);
	FbxQuaternion getQuaternion(const NIFDictionary &dict);

	NIFDictionary makeVector3(const FbxVector4 &vector);
	NIFDictionary makeMatrix3x3(const FbxAMatrix &matrix);
	NIFDictionary makeTransform(const FbxAMatrix &matrix);

	static inline float getSignedFloatFromU8(uint8_t value) {
		return (static_cast<float>(value) / 255.0f) * 2.0f - 1.0f;
	}

	static inline float getUnsignedFloatFromU8(uint8_t value) {
		return (static_cast<float>(value) / 255.0f);
	}
}

#endif
