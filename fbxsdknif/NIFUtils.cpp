#include "NIFUtils.h"

#include <fbxsdk/core/math/fbxquaternion.h>

namespace fbxnif {
	std::string getString(const NIFDictionary &dict, const NIFDictionary &header)  {
		Symbol string("String");

		if (dict.data.count(string) == 0) {
			auto index = static_cast<int32_t>(dict.getValue<uint32_t>("Index"));

			if (index < 0)
				return std::string();

			const auto &strings = header.getValue<NIFArray>("Strings");
			if (index >= strings.data.size())
				throw std::logic_error("string index is out of range");

			return std::get<NIFDictionary>(strings.data[index]).getValue<std::string>("Value");
		}
		else {
			return dict.getValue<NIFDictionary>(string).getValue<std::string>("Value");
		}
	}

	FbxVector4 getVector3(const NIFDictionary &dict) {
		return FbxVector4(
			dict.getValue<float>("x"),
			dict.getValue<float>("y"),
			dict.getValue<float>("z")
		);
	}

	FbxVector4 getByteVector3(const NIFDictionary &dict) {
		return FbxVector4(
			getSignedFloatFromU8(dict.getValue<uint32_t>("x")),
			getSignedFloatFromU8(dict.getValue<uint32_t>("y")),
			getSignedFloatFromU8(dict.getValue<uint32_t>("z"))
		);
	}

	FbxAMatrix getMatrix3x3(const NIFDictionary &dict) {
		FbxAMatrix mat;

		auto dat = static_cast<double *>(mat);

		dat[0] = dict.getValue<float>("m11");
		dat[1] = dict.getValue<float>("m12");
		dat[2] = dict.getValue<float>("m13");

		dat[4] = dict.getValue<float>("m21");
		dat[5] = dict.getValue<float>("m22");
		dat[6] = dict.getValue<float>("m23");

		dat[8] = dict.getValue<float>("m31");
		dat[9] = dict.getValue<float>("m32");
		dat[10] = dict.getValue<float>("m33");

		return mat;
	}

	FbxDouble3 getColor3(const NIFDictionary &dict) {
		return FbxDouble3(
			dict.getValue<float>("r"),
			dict.getValue<float>("g"),
			dict.getValue<float>("b")
		);
	}

	FbxColor getColor4(const NIFDictionary &dict) {
		return FbxColor(
			dict.getValue<float>("r"),
			dict.getValue<float>("g"),
			dict.getValue<float>("b"),
			dict.getValue<float>("a")
		);
	}

	FbxColor getByteColor4(const NIFDictionary &dict) {
		return FbxColor(
			getUnsignedFloatFromU8(dict.getValue<uint32_t>("r")),
			getUnsignedFloatFromU8(dict.getValue<uint32_t>("g")),
			getUnsignedFloatFromU8(dict.getValue<uint32_t>("b")),
			getUnsignedFloatFromU8(dict.getValue<uint32_t>("a"))
		);
	}

	FbxVector2 getTexCoord(const NIFDictionary &dict) {
		return FbxVector2(
			dict.getValue<float>("u"),
			dict.getValue<float>("v")
		);
	}

	FbxAMatrix getTransform(const NIFDictionary &dict) {
		return FbxAMatrix(
			getVector3(dict.getValue<NIFDictionary>("Translation")),
			getMatrix3x3(dict.getValue<NIFDictionary>("Rotation")).GetQ(),
			FbxVector4(FbxDouble3(dict.getValue<float>("Scale")))
		);
	}

	NIFDictionary makeVector3(const FbxVector4 &vector) {
		NIFDictionary dict;
		dict.isNiObject = false;
		dict.typeChain.emplace_back("Vector3");
		dict.data.emplace("x", static_cast<float>(vector[0]));
		dict.data.emplace("y", static_cast<float>(vector[1]));
		dict.data.emplace("z", static_cast<float>(vector[2]));
		return dict;
	}

	NIFDictionary makeMatrix3x3(const FbxAMatrix &matrixIn) {
		FbxMatrix matrix(matrixIn);
		auto dat = static_cast<const double *>(matrix);

		NIFDictionary dict;
		dict.isNiObject = false;
		dict.typeChain.emplace_back("Matrix33");
		dict.data.emplace("m11", static_cast<float>(dat[0]));
		dict.data.emplace("m12", static_cast<float>(dat[1]));
		dict.data.emplace("m13", static_cast<float>(dat[2]));
		dict.data.emplace("m21", static_cast<float>(dat[4]));
		dict.data.emplace("m22", static_cast<float>(dat[5]));
		dict.data.emplace("m23", static_cast<float>(dat[6]));
		dict.data.emplace("m31", static_cast<float>(dat[8]));
		dict.data.emplace("m32", static_cast<float>(dat[9]));
		dict.data.emplace("m33", static_cast<float>(dat[10]));

		return dict;
	}

	NIFDictionary makeTransform(const FbxAMatrix &matrix) {
		NIFDictionary dict;
		dict.isNiObject = false;
		dict.typeChain.emplace_back("NiTransform");

		dict.data.emplace("Translation", makeVector3(matrix.GetT()));
		dict.data.emplace("Rotation", makeMatrix3x3(FbxAMatrix(FbxVector4(), matrix.GetQ(), FbxVector4(1.0, 1.0, 1.0, 1.0))));
		dict.data.emplace("Scale", static_cast<float>(matrix.GetS()[0]));

		return dict;
	}

	static inline bool isOkayTransformValue(float val) {
		return isfinite<float>(val) && val > -1e38f && val < 1e38f;
	}

	FbxAMatrix getQuatTransform(const NIFDictionary &dict) {
		FbxAMatrix transform;

		bool tValid = true, rValid = true, sValid = true;

		if (dict.data.count("TRS Valid") != 0) {
			const auto &trs = dict.getValue<NIFArray>("TRS Valid");
			tValid = std::get<uint32_t>(trs.data[0]) != 0;
			rValid = std::get<uint32_t>(trs.data[1]) != 0;
			sValid = std::get<uint32_t>(trs.data[2]) != 0;
		}

		if (tValid) {
			auto translation = getVector3(dict.getValue<NIFDictionary>("Translation"));
			if (isOkayTransformValue(translation[0]) && isOkayTransformValue(translation[1]) && isOkayTransformValue(translation[2])) {
				transform.SetT(translation);
			}
		}

		if (rValid) {
			auto rotation = getQuaternion(dict.getValue<NIFDictionary>("Rotation"));
			if (isOkayTransformValue(rotation[0]) && isOkayTransformValue(rotation[1]) && isOkayTransformValue(rotation[2]) && isOkayTransformValue(rotation[3])) {
				transform.SetQ(rotation);
			}
		}

		if (sValid) {
			auto scale = dict.getValue<float>("Scale");
			if (isOkayTransformValue(scale)) {
				transform.SetS(FbxVector4(scale, scale, scale, 1.0f));
			}
		}

		return transform;
	}

	FbxQuaternion getQuaternion(const NIFDictionary &dict) {
		return FbxQuaternion(
			dict.getValue<float>("x"),
			dict.getValue<float>("y"),
			dict.getValue<float>("z"),
			dict.getValue<float>("w")
		);
	}

	std::string getStringFromPalette(uint32_t offset, const NIFDictionary &palette) {
		const auto &string = palette.getValue<NIFDictionary>("Palette").getValue<NIFDictionary>("Palette").getValue<std::string>("Value");

		auto stringEnd = string.find('\0', offset);

		return string.substr(offset, stringEnd - offset);
	}
}
