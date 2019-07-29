#ifndef NIF2FBXEXTENSION_H
#define NIF2FBXEXTENSION_H

#include <string>

class NIF2FBXExtension {
protected:
	inline NIF2FBXExtension() {}
	inline ~NIF2FBXExtension() {}

public:
	NIF2FBXExtension(const NIF2FBXExtension& other) = delete;
	NIF2FBXExtension &operator =(const NIF2FBXExtension& other) = delete;

	virtual void translateTextureAsset(const std::string& originalName, std::string& assetName, std::string& fileName) = 0;
};

#endif
