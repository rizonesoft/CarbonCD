#pragma once

class CDirStructure
{
public:
    CDirStructure(void);
    ~CDirStructure(void);
    CString m_RealFileName;
    CString m_ImageFileName;
    CString m_ImageFileNameShort;
    DWORD m_FileSize;
    DWORD m_No;
    SYSTEMTIME m_TimeStamp;
    bool m_IsDirectory;
    HTREEITEM m_hTreeItem;
    DWORD m_LBA;
    DWORD m_ParentNo;

protected:
    CDirStructure* m_DirList;
    CDirStructure* m_FileList;
    CDirStructure* m_DirListTail;
    CDirStructure* m_FileListTail;
    CDirStructure* m_Next;

public:
    CDirStructure* CreateDirectory(void);
    CDirStructure* CreateFile(void);
    CDirStructure* GetNext(void);
    CDirStructure* GetDirectory(void);
    CDirStructure* GetFile(void);
    void SetNext(CDirStructure* Next);
    CDirStructure* SearchName(LPCSTR Name);
    CDirStructure* SearchNameShort(LPCSTR Name);
    CDirStructure* SearchFile(LPCSTR Name);
    CDirStructure* Search(LPCSTR Name);
    CDirStructure* SearchShort(LPCSTR Name);
    CDirStructure* SearchRealFile(LPCSTR Name);
    void DeleteNode(CDirStructure* DelNode);
    void CalcShortName(CWnd* ParentWnd);
    DWORD CalcPathTableSize(int TypeFlag);
    //  DWORD CalcRecordSize(int TypeFlag);
    //  DWORD CalcRecordTableSize(int TypeFlag);
    //  DWORD CalcRecordTableSectors(int TypeFlag);
    void Clear(void);
    void TruncSJIS(LPSTR String);
};
