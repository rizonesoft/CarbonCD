#pragma once
#include "isowriter.h"
#include "DirStructure.h"
#include "FileList.h"
#include "Setting.h"

class CIsoCreator
{
public:
    CIsoCreator(void);
    ~CIsoCreator(void);

protected:
    BYTE m_CPBuffer[2352];
    CIsoWriter m_Writer;
    CDirStructure* m_Dir;
    DWORD m_PTSizeShort;
    DWORD m_PTSizeLong;
    DWORD m_ImageSize;
    CFileList* m_DirList;
    CFileList* m_FileList;
    HANDLE m_hFile;
    CFileList* m_ReadingFile;
    CFileList* ListDirectories(CDirStructure* Dir, CFileList* List, int Level);
    CFileList* ListFiles(CDirStructure* Dir, CFileList* List);
    CString m_VolumeLabel;
    DWORD LeToBe(DWORD Le);
    WORD LeToBeShort(WORD Le);
    BYTE m_Buffer[2352 * 2];
    int m_BufferSize;
    int m_DirRecordSize[2];
#if COPY_PROTECTION
    int m_CopyProtectionSize;
    int m_WroteProtection;
#endif
    void SetDirRecord(BYTE* Buffer, CDirStructure* Dir);
    void SetDate(BYTE* Buffer, SYSTEMTIME& Time);
    void SetDate_L(BYTE* Buffer, SYSTEMTIME& Time);
    void CreatePVD(BYTE* Buffer);
    void CreateSVD(BYTE* Buffer);
    void CreateEVD(BYTE* Buffer);
    void WriteBuffer(BYTE* Buffer, int Size);
    void WriteBufferNB(BYTE* Buffer, int Size);
    void CreatePathTable(int Type);
    CFileList* SortFiles(CDirStructure* Dir, int Type);
    CFileList* AddNode(CFileList* RootNode, CFileList* NewNode);
    void CreateDirRecord(CFileList* List, int Type, DWORD Current, DWORD Parent);
    DWORD EmulateDirRecord(CFileList* List, int Type, DWORD Current, DWORD Parent);

public:
    void CreateJolietHeader(CDirStructure* Dir);
    void SetParams(LPCSTR VolumeLabel, int CopyProtectionSize);
    void InitializeReading(void);
    bool OpenReadFile(void);
    void CloseReadFile(void);
    void ByteSwapCopy(void* Buffer, const void* Source, DWORD CopyCount);
    DWORD GetCurrentLBA(void);
    DWORD GetImageSize(void);
    bool GetFrame(BYTE* Buffer);
    bool GetHeaderFrame(BYTE* Buffer);
    void CreateZeroSector(BYTE* Buffer);
#if COPY_PROTECTION
    bool GetProtectionArea(BYTE* Buffer);
#endif
};
