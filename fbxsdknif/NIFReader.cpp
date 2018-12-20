#include "NIFReader.h"

#include <fbxsdk/fbxsdk_def.h>
#include <fbxsdk/fileio/fbximporter.h>
#include <fbxsdk/core/base/fbxutils.h>

#include <nifparse/NIFFile.h>

#include <Windows.h>

#include "FBXSceneWriter.h"

namespace fbxnif {
	const char *const NIFReader::m_extensions[]{ "nif", nullptr };
	const char *const NIFReader::m_descriptions[]{ "Reader for NIF-formatted model files", nullptr };
	
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

			FBXSceneWriter writer(file);
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
