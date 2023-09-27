#include "StdAfx.h"
#include "imagefile.h"

struct t_fmt_chunk_wave
{
    DWORD riff_id;
    DWORD riff_size;
    DWORD riff_form_type;

    DWORD fmt_id;
    DWORD fmt_size;

    WORD formatType;
    WORD channel;
    DWORD samplesPerSec;
    DWORD bytesPersec;
    WORD blockSize;
    WORD bitsPerSample;

    DWORD data_id;
    DWORD data_size;
};

CImageFile::CImageFile(void)
{
    m_hFile = INVALID_HANDLE_VALUE;
    m_hSubFile = INVALID_HANDLE_VALUE;
    m_Type = FILE_DATA;
    m_ImgPos = 0;
    m_SubPos = 0;
}

CImageFile::~CImageFile(void)
{
    Close();
}

void CImageFile::OpenSub(LPCSTR FileName)
{
    m_hSubFile = CreateFile(FileName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
}

bool CImageFile::Open(LPCSTR FileName, int Type)
{
    Close();
    m_Type = Type;
    m_hFile = CreateFile(FileName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                         nullptr);

    if (m_hFile == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    if (Type == FILE_AUDIO)
    {
        SetFilePointer(m_hFile, 44, nullptr, FILE_BEGIN);
    }

    return true;
}

void CImageFile::Close(void)
{
    if (m_hSubFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hSubFile);
        m_hSubFile = INVALID_HANDLE_VALUE;
    }

    if (m_hFile == INVALID_HANDLE_VALUE)
    {
        return;
    }

    if (m_Type == FILE_AUDIO)
    {
        DWORD FilePos;
        struct t_fmt_chunk_wave header;
        FilePos = SetFilePointer(m_hFile, 0, nullptr, FILE_CURRENT) - 44;
        header.riff_id = 'FFIR';
        header.riff_size = FilePos + 36;
        header.riff_form_type = 'EVAW';
        header.fmt_id = ' tmf';
        header.fmt_size = 16;
        header.formatType = 1;
        header.channel = 2;
        header.samplesPerSec = 44100;
        header.bitsPerSample = 16;
        header.blockSize = header.bitsPerSample * header.channel / 8;
        header.bytesPersec = header.blockSize * header.samplesPerSec;
        header.data_id = 'atad';
        header.data_size = FilePos;
        SetFilePointer(m_hFile, 0, nullptr, FILE_BEGIN);
        WriteFile(m_hFile, &header, 44, &FilePos, nullptr);
    }

    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hFile);
        m_hFile = INVALID_HANDLE_VALUE;
    }
}

bool CImageFile::AcceptSubCode(void)
{
    if (m_hSubFile != INVALID_HANDLE_VALUE)
    {
        return true;
    }

    return false;
}


void CImageFile::Write(BYTE* Buffer)
{
    if (m_hFile == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DWORD wrote;
    wrote = 0;
    WriteFile(m_hFile, Buffer, 2352, &wrote, nullptr);
}

void CImageFile::WriteSub(BYTE* Buffer)
{
    if (m_hSubFile != INVALID_HANDLE_VALUE)
    {
        DWORD wrote;
        wrote = 0;
        WriteFile(m_hSubFile, Buffer, 96, &wrote, nullptr);
    }
}

void CImageFile::SaveFilePointer(void)
{
    m_ImgPos = SetFilePointer(m_hFile, 0, nullptr, FILE_CURRENT);
    m_SubPos = SetFilePointer(m_hSubFile, 0, nullptr, FILE_CURRENT);
}

void CImageFile::LoadFilePointer(void)
{
    SetFilePointer(m_hFile, m_ImgPos, nullptr, FILE_BEGIN);
    SetFilePointer(m_hSubFile, m_SubPos, nullptr, FILE_BEGIN);
}

void CImageFile::SeekFromCurrentPosition(int SeekSectors)
{
    DWORD ImgPos, SubPos;
    ImgPos = SetFilePointer(m_hFile, 0, nullptr, FILE_CURRENT);
    SubPos = SetFilePointer(m_hSubFile, 0, nullptr, FILE_CURRENT);
    ImgPos = ImgPos + SeekSectors * 2352;
    SubPos = SubPos + SeekSectors * 96;
    SetFilePointer(m_hFile, ImgPos, nullptr, FILE_BEGIN);
    SetFilePointer(m_hSubFile, SubPos, nullptr, FILE_BEGIN);
}

BOOL CImageFile::Read(BYTE* Buffer)
{
    DWORD read;

    if (m_hFile != INVALID_HANDLE_VALUE && m_hSubFile != INVALID_HANDLE_VALUE)
    {
        ReadFile(m_hFile, Buffer, 2352, &read, nullptr);
        ReadFile(m_hSubFile, Buffer + 2352, 96, &read, nullptr);
        return TRUE;
    }

    return FALSE;
}
