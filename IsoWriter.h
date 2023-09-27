#pragma once
#include "checksector.h"
#include "cdtype.h"

using HeaderBuffer = struct tHeaderBuffer
{
    BYTE Data[2352];
    struct tHeaderBuffer* Next;
};

class CIsoWriter
{
public:
    CIsoWriter(void);
    ~CIsoWriter(void);

protected:
    CCheckSector m_EDC;
    MSFAddress m_Address;
    HeaderBuffer* m_Buffer;
    HeaderBuffer* m_BufferTail;

public:
    void Close(void);

public:
    void WriteHeader(BYTE* Buffer);
    DWORD GetLBA(void);

protected:
    void CreateNewBuffer(void);

public:
    bool GetHeaderFrame(BYTE* Buffer);
    void GenerateData(BYTE* Buffer, BYTE* Src);
    void GenerateRaw(BYTE* Buffer, BYTE* Src);
};
