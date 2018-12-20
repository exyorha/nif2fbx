#ifndef NIF_READER_H
#define NIF_READER_H

#include "FBXNIFPluginNS.h"

#include <fbxsdk/fileio/fbxreader.h>

#include <fstream>

namespace fbxnif {
	class NIFReader final : public FbxReader {
	public:
		NIFReader(FbxManager &manager, int id);
		virtual ~NIFReader();

		static FbxReader *Create(FbxManager &manager, FbxImporter &importer, int subID, int pluginID);
		static void *GetInfo(EInfoRequest request, int readerTypeId);
		static void IOSettingsFiller(FbxIOSettings &ios);

		virtual bool FileOpen(char *pFileName) override;
		virtual bool FileClose() override;
		virtual bool IsFileOpen() override;
		virtual bool Read(FbxDocument *pDocument) override;
		virtual bool GetReadOptions(bool pParseFileAsNeeded = true) override;

	private:
		static const char *const m_extensions[];
		static const char *const m_descriptions[];

		std::fstream m_stream;
	};
}

#endif
