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
	FbxVector4 getVector3(const NIFDictionary &dict);
	FbxAMatrix getMatrix3x3(const NIFDictionary &dict);
	FbxColor getColor4(const NIFDictionary &dict);
	FbxVector2 getTexCoord(const NIFDictionary &dict);
	FbxAMatrix getTransform(const NIFDictionary &dict);

	NIFDictionary makeVector3(const FbxVector4 &vector);
	NIFDictionary makeMatrix3x3(const FbxAMatrix &matrix);
	NIFDictionary makeTransform(const FbxAMatrix &matrix);
}

#endif
