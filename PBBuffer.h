#pragma once

class CPBBuffer
{
public:
    CPBBuffer(void);
    ~CPBBuffer(void);

protected:
    BYTE* m_Buffer;
    BYTE* m_PBuffer;
    int m_BufferLength;

public:
    BYTE* CreateBuffer(DWORD BufferSize);
    void DeleteBuffer(void);
    BYTE* GetBuffer(void);
    DWORD GetBufferSize(void);
};
