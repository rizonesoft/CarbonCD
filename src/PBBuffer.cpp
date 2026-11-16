#include "stdafx.h"
#include "pbbuffer.h"

CPBBuffer::CPBBuffer(void)
{
    m_Buffer = nullptr;
    m_PBuffer = nullptr;
    m_BufferLength = 0;
}

CPBBuffer::~CPBBuffer(void)
{
}

BYTE* CPBBuffer::CreateBuffer(DWORD BufferSize)
{
    DeleteBuffer();
    m_Buffer = static_cast<BYTE*>(malloc(BufferSize + 0x0f));
    m_PBuffer = (BYTE*)(((DWORD)m_Buffer + 0x0f) & ~0x0f);
    m_BufferLength = BufferSize;
    return m_PBuffer;
}

void CPBBuffer::DeleteBuffer(void)
{
    if (m_Buffer == nullptr) { return; }

    free(m_Buffer);
    m_Buffer = nullptr;
    m_PBuffer = nullptr;
    m_BufferLength = 0;
}

BYTE* CPBBuffer::GetBuffer(void)
{
    return m_PBuffer;
}

DWORD CPBBuffer::GetBufferSize(void)
{
    return m_BufferLength;
}
