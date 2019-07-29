#include "NIFReader.h"

#include <fbxsdk/fbxsdk_def.h>
#include <fbxsdk/fileio/fbximporter.h>
#include <fbxsdk/core/base/fbxutils.h>

#include <nifparse/NIFFile.h>

#include <Windows.h>

#include "FBXSceneWriter.h"
#include "SkeletonProcessor.h"

namespace fbxnif {
	const char *const NIFReader::m_extensions[]{ "nif", "kf", nullptr };
	const char *const NIFReader::m_descriptions[]{ "Gamebryo model files (*.nif)", "Gamebryo animation files (*.kf)", nullptr };
	
	NIFReader::NIFReader(FbxManager &manager, int id) : FbxReader(manager, id, FbxStatusGlobal::GetRef()) {

	}

	NIFReader::~NIFReader() {

	}

	FbxReader *NIFReader::Create(FbxManager &manager, FbxImporter &importer, int subID, int pluginID) {
		auto instance = FbxNew<NIFReader>(manager, pluginID);
		instance->SetIOSettings(importer.GetIOSettings());
		return instance;
	}
	
	void *NIFReader::GetInfo(EInfoRequest request, int readerTypeId) {
		switch (request) {
		case FbxReader::eInfoExtension:
			return (void *)m_extensions;

		case FbxReader::eInfoDescriptions:
			return (void *)m_descriptions;

		default:
			return nullptr;
		}
	}
		
	void NIFReader::IOSettingsFiller(FbxIOSettings &ios) {
		auto extensions = ios.GetProperty(IMP_FBX_EXT_SDK_GRP);
		if (!extensions.IsValid())
			return;

		auto plugin = ios.AddPropertyGroup(extensions, "FBXSDKNIF", FbxStringDT, "FBXSDKNIF");
		if (plugin.IsValid()) {
			bool skeletonImportDefault = false;

			ios.AddProperty(
				plugin,
				"SkeletonImport",
				FbxBoolDT,
				"Import as skeleton",
				&skeletonImportDefault,
				true);

			FbxString skeletonDefault = "";
			ios.AddProperty(
				plugin,
				"Skeleton",
				FbxStringDT,
				"Full path to skeleton file (FBX)",
				&skeletonDefault,
				true);

			unsigned long long extensionDefault = 0;
			ios.AddProperty(
				plugin,
				"Extension",
				FbxULongLongDT,
				"Pointer to NIF2FBXExtension (in integrated environments)",
				&extensionDefault,
				true);
		}
	}

	bool NIFReader::FileOpen(char *pFileName) {
		try {
			m_stream.exceptions(std::ios::failbit | std::ios::badbit | std::ios::eofbit);
			m_stream.open(pFileName, std::ios::in | std::ios::binary);
			return true;
		}
		catch (const std::exception &e) {
			m_stream = std::fstream();
			GetStatus().SetCode(FbxStatus::eInvalidParameter, "failed to open file %s: %s", pFileName, e.what());
			return false;
		}
	}

	bool NIFReader::FileClose() {
		try {
			m_stream.close();
			return true;
		}
		catch (const std::exception &e) {
			m_stream = std::fstream();
			GetStatus().SetCode(FbxStatus::eInvalidParameter, "failed to close file: %s", e.what());
			return false;
		}
	}

	bool NIFReader::IsFileOpen() {
		return m_stream.is_open();
	}

	bool NIFReader::Read(FbxDocument *document) {
		//try {
			NIFFile file;
			file.parse(m_stream);

			SkeletonProcessor skeletonProcessor;

			auto ios = GetIOSettings();
			if (ios) {
				skeletonProcessor.setSkeletonImport(ios->GetBoolProp(IMP_FBX_EXT_SDK_GRP "|FBXSDKNIF|SkeletonImport", false));
			}

			skeletonProcessor.process(file);

			FBXSceneWriter writer(file, skeletonProcessor);

			if (ios) {
				auto skeletonFile = ios->GetStringProp(IMP_FBX_EXT_SDK_GRP "|FBXSDKNIF|Skeleton", "");
				if (!skeletonFile.IsEmpty()) {
					writer.setSkeletonFile(skeletonFile);
				}

				auto extensionProperty = ios->GetProperty(IMP_FBX_EXT_SDK_GRP "|FBXSDKNIF|Extension");
				if (extensionProperty.IsValid()) {
					writer.setExtension(reinterpret_cast<NIF2FBXExtension *>(static_cast<uintptr_t>(extensionProperty.Get<unsigned long long>())));
				}
			}

			writer.write(document);

			return true;
		//}
		//catch (const std::exception &e) {
		//	GetStatus().SetCode(FbxStatus::eInvalidFile, "failed to parse NIF: %s", e.what());\
		//
		//	return false;
		//}
	}

	bool NIFReader::GetReadOptions(bool pParseFileAsNeeded) {
		return false;
	}
}
