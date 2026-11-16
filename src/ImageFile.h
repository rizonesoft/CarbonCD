#pragma once

class CImageFile
{
public:
    CImageFile(void);
    ~CImageFile(void);
    bool Open(LPCSTR FileName, int Type);

protected:
    HANDLE m_hFile;
    HANDLE m_hSubFile;
    DWORD m_SubPos;
    DWORD m_ImgPos;

public:
    void Close(void);
    void Write(BYTE* Buffer);
    //  void Write(BYTE * Buffer);
protected:
    int m_Type;

public:
    void OpenSub(LPCSTR FileName);
    bool AcceptSubCode(void);
    void WriteSub(BYTE* Buffer);
    void SaveFilePointer(void);
    void LoadFilePointer(void);
    void SeekFromCurrentPosition(int SeekSectors);
    BOOL Read(BYTE* Buffer);
};

#define FILE_DATA       0
#define FILE_AUDIO      1
