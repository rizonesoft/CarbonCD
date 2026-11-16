#include "stdafx.h"

#include "pregapanalyzer.h"
#include "CDType.h"
#include "CheckSector.h"
#include "Setting.h"

CPregapAnalyzer::CPregapAnalyzer(void)
{
    int i;

    for (i = 0; i < 150; i++)
    {
        m_Existing[i] = false;
    }
}

CPregapAnalyzer::~CPregapAnalyzer(void)
{
}

#define HexToBin(a) ((a >> 4) * 10 + (a & 0x0f))

void CPregapAnalyzer::AnalyzePregap(void)
{
    DWORD lba;
    MSFAddress msf;
    int ReadingMethod, i;
    CString cs;
    BYTE* Sub;
    int SectorType;

    for (i = 0; i < 150; i++)
    {
        m_Existing[i] = false;
    }

    ReadingMethod = 3;
    msf = 149;

    for (i = 0; i < 3; i++)
    {
        if (m_CD->ReadRawSub(msf, m_Buffer, i))
        {
            ReadingMethod = i;
            break;
        }
    }

    if (ReadingMethod == 3)
    {
        m_Log->AddMessage(LOG_ERROR, MSG(152));
        return;
    }

    lba = 149;
    Sub = m_Buffer + 2352 + 12;
    SectorType = 0;
    m_AddressDelta = 0;

    for (lba = 149; lba >= 0; lba--)
    {
        msf = lba;

        if (m_CD->ReadRawSub(msf, m_Buffer, ReadingMethod))
        {
            //   backup pregap data
            m_Existing[lba] = true;
            memcpy(m_Pregap[lba], m_Buffer, 2352);

            //   analyze sub channel
            if ((Sub[0] & 0x0f) == 1)
            {
                BYTE m, s, f;
                DWORD lba, alba;

                if ((Sub[0] & 0xf0) == 0x40)
                {
                    SectorType = m_Buffer[15];
                }

                else
                {
                    SectorType = 0;
                }

                m = HexToBin(Sub[3]);
                s = HexToBin(Sub[4]);
                f = HexToBin(Sub[5]);
                lba = ((m * 60) + s) * 75 + f;
                m = HexToBin(Sub[7]);
                s = HexToBin(Sub[8]);
                f = HexToBin(Sub[9]);
                alba = ((m * 60) + s) * 75 + f;

                if (alba < 150 && m_AddressDelta == 0)
                {
                    m_AddressDelta = (150 - alba) - lba;
                }
            }
        }

        else
        {
            break;
        }
    }

    if (!m_Existing[149])
    {
        m_Log->AddMessage(LOG_ERROR, MSG(152));
        return;
    }

    CCheckSector edc;

    for (i = 0; i < 150; i++)
    {
        if (m_Existing[i])
        {
            cs.Format(MSG(151), i - 150);
            m_Log->AddMessage(LOG_NORMAL, cs);
            break;
        }

        if (SectorType == 0)
        {
            memset(m_Pregap[i], 0, 2352);
        }

        else
        {
            memset(m_Pregap[i], 0, 2352);
            msf = i;

            if (SectorType == 1)
            {
                edc.Mode1Raw((m_Pregap[i]), msf.Minute, msf.Second, msf.Frame);
            }

            else
            {
                memset(m_Pregap[i] + 1, 0xff, 10);
                m_Pregap[i][0x000c] = ((msf.Minute / 10) * 0x10) + (msf.Minute % 10);
                m_Pregap[i][0x000d] = ((msf.Second / 10) * 0x10) + (msf.Second % 10);
                m_Pregap[i][0x000e] = ((msf.Frame / 10) * 0x10) + (msf.Frame % 10);
                m_Pregap[i][15] = 2;
                m_Pregap[i][0x12] = 0x20;
                m_Pregap[i][0x16] = 0x20;
            }
        }
    }
}

bool CPregapAnalyzer::Succeed(void)
{
    return m_Existing[149];
}

void CPregapAnalyzer::WritePregap(LPCSTR FileName)
{
    HANDLE hFile;
    int i;
    DWORD read;
    hFile = CreateFile(FileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return;
    }

    for (i = 0; i < 150; i++)
    {
        WriteFile(hFile, m_Pregap[i], 2352, &read, nullptr);
    }

    CloseHandle(hFile);
}
