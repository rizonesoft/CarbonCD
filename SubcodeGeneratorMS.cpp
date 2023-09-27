#include "stdafx.h"

#include "Setting.h"
#include "subcodegeneratorms.h"

#define TO_HEX(a) (((a) / 10) * 0x10 + ((a) % 10))

CSubcodeGeneratorMS::CSubcodeGeneratorMS(void)
{
    int i, c, crc;

    //   generate crc table
    for (i = 0; i < 256; i++)
    {
        crc = i << 8;

        for (c = 0; c < 8; c++)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ 0x1021;
            }

            else
            {
                crc = (crc << 1);
            }
        }

        m_SubcodeCRCTable[i] = crc & 0xffff;
    }

    m_AbnormalImageSize = FALSE;
}

CSubcodeGeneratorMS::~CSubcodeGeneratorMS(void)
{
}

LPCSTR CSubcodeGeneratorMS::GetErrorMessage(void)
{
    return m_ErrorMessage;
}

bool CSubcodeGeneratorMS::ParseFile(LPCSTR FileName)
{
    char Buffer[1024], *p;
    BYTE *toc, *q;
    CString cs;
    int i, j;
    m_ErrorMessage = "";
    //   create file name to .img & .sub
    lstrcpy(Buffer, FileName);
    p = Buffer + lstrlen(Buffer) - 1;

    while (p >= Buffer)
    {
        if (*p == '.')
        {
            break;
        }

        p--;
    }

    if (*p == '.')
    {
        *p = '\0';
    }

    m_ImgFileName.Format("%s.img", Buffer);
    m_SubFileName.Format("%s.sub", Buffer);
    m_PreFileName = "";
    //   check image version
    m_CDM_Extension = false;
    m_PregapDelta = 0;
    i = GetPrivateProfileInt("CloneCD", "Version", 0, FileName);

    if (i > 1)
    {
        m_ImageVersion.Format("Compatible CCD Ver.%d", i);
    }

    else
    {
        i = GetPrivateProfileInt("CarbonCD", "Version", 0, FileName);

        if (i > 0)
        {
            char Buf[100];
            m_CDM_Extension = true;
            m_ImageVersion.Format("CarbonCD Format Ver.%d", i);
            GetPrivateProfileString("ReadDrive", "Vendor", "", Buf, 100, FileName);
            m_VendorName = Buf;
            GetPrivateProfileString("ReadDrive", "Product", "", Buf, 100, FileName);
            m_ProductName = Buf;
            GetPrivateProfileString("ReadDrive", "Revision", "", Buf, 100, FileName);
            m_Revision = Buf;

            if (GetPrivateProfileInt("Disc", "PregapAnalyze", 0, FileName) == 1)
            {
                m_PregapDelta = GetPrivateProfileInt("Disc", "PregapDelta", 0, FileName);
                m_PreFileName.Format("%s.pre", Buffer);
            }
        }

        else
        {
            m_ErrorMessage.Format(MSG(139), FileName);
            return false;
        }
    }

    //   read disc layout
    m_EntryCount = GetPrivateProfileInt("Disc", "TocEntries", 0, FileName);
    m_SessionCount = GetPrivateProfileInt("Disc", "Sessions", 0, FileName);
    m_Scrambled = GetPrivateProfileInt("Disc", "DataTracksScrambled", 0, FileName);
    m_CDTextLength = GetPrivateProfileInt("Disc", "CDTextLength", 0, FileName);
    m_AbnormalImageSize = GetPrivateProfileInt("Disc", "AbnormalImageSize", 0, FileName);

    //   read lead-in mode
    for (i = 0; i < m_SessionCount; i++)
    {
        cs.Format("Session %d", i + 1);
        m_PreGapMode[i] = GetPrivateProfileInt(cs, "PreGapMode", 0, FileName);
    }

    //   read toc entry
    for (i = 0; i < m_EntryCount; i++)
    {
        toc = m_Entry + i * 11;
        cs.Format("Entry %d", i);
        toc[0] = GetPrivateProfileInt(cs, "Session", 0, FileName);
        toc[1] = (GetPrivateProfileHex(cs, "ADR", 0, FileName) & 0x0f);
        toc[1] |= (GetPrivateProfileHex(cs, "Control", 0, FileName) & 0x0f) << 4;
        toc[2] = GetPrivateProfileInt(cs, "TrackNo", 0, FileName);
        toc[3] = GetPrivateProfileHex(cs, "Point", 0, FileName);
        toc[4] = GetPrivateProfileInt(cs, "AMin", 0, FileName);
        toc[5] = GetPrivateProfileInt(cs, "ASec", 0, FileName);
        toc[6] = GetPrivateProfileInt(cs, "AFrame", 0, FileName);
        toc[7] = GetPrivateProfileInt(cs, "Zero", 0, FileName);
        toc[8] = GetPrivateProfileInt(cs, "PMin", 0, FileName);
        toc[9] = GetPrivateProfileInt(cs, "PSec", 0, FileName);
        toc[10] = GetPrivateProfileInt(cs, "PFrame", 0, FileName);
    }

    //   read CD-TEXT entry
    if (GetPrivateProfileInt("Disc", "CDTextLength", 0, FileName) > 0)
    {
        m_CDTextEntries = GetPrivateProfileInt("CDText", "Entries", 0, FileName);

        for (i = 0; i < m_CDTextEntries; i++)
        {
            cs.Format("Entry %d", i);
            GetPrivateProfileString("CDText", cs, "", Buffer, 1024, FileName);
            q = m_CDText + i * 18;

            for (j = 0; j < 16; j++)
            {
                p = Buffer + j * 3;

                if (p[0] >= 'a')
                {
                    q[j] = static_cast<BYTE>((p[0] - 'a' + 10) * 0x10);
                }

                else
                {
                    q[j] = static_cast<BYTE>((p[0] - '0') * 0x10);
                }

                if (p[1] >= 'a')
                {
                    q[j] += static_cast<BYTE>(p[1] - 'a' + 10);
                }

                else
                {
                    q[j] += static_cast<BYTE>(p[1] - '0');
                }
            }

            CalcCDTextCRC(q);
        }
    }

    else
    {
        m_CDTextEntries = 0;
    }

    return true;
}

void CSubcodeGeneratorMS::ModifyAddress(void)
{
    int i, j;
    BYTE* toc;

    for (i = 0; i < m_TocCount; i++)
    {
        toc = m_Toc + i * 11;

        if ((toc[1] & 0x0f) == 1 || (toc[1] & 0x0f) == 2)
        {
            if (toc[3] > 0 && toc[3] < 0x64)
            {
                //   track info
                for (j = 3; j < 11; j++)
                {
                    toc[j] = TO_HEX(toc[j]);
                }
            }

            else if (toc[3] == 0xa0 || toc[3] == 0xa1)
            {
                //   begin/end track
                for (j = 4; j < 9; j++)
                {
                    toc[j] = TO_HEX(toc[j]);
                }
            }

            else if (toc[3] == 0xa2)
            {
                //   lead-out
                for (j = 4; j < 11; j++)
                {
                    toc[j] = TO_HEX(toc[j]);
                }
            }
        }
    }

    for (i = 0; i < m_TocIntCount; i++)
    {
        toc = m_TocInt + i * 11;

        if ((toc[1] & 0x0f) == 5)
        {
            if (toc[3] <= 40)
            {
                for (j = 4; j < 11; j++)
                {
                    toc[j] = TO_HEX(toc[j]);
                }
            }

            else if (toc[3] == 0xb0)
            {
                toc[7] = 2;

                for (j = 4; j < 7; j++)
                {
                    toc[j] = TO_HEX(toc[j]);
                }

                for (j = 8; j < 11; j++)
                {
                    toc[j] = TO_HEX(toc[j]);
                }
            }

            else if (toc[3] == 0xc0)
            {
                toc[4] = 0x60;
                toc[8] = static_cast<BYTE>(m_LeadIn / (75 * 60));
                toc[9] = static_cast<BYTE>((m_LeadIn / 75) % 60);
                toc[10] = static_cast<BYTE>(m_LeadIn % 75);

                for (j = 8; j < 11; j++)
                {
                    toc[j] = TO_HEX(toc[j]);
                }
            }
        }
    }
}

int CSubcodeGeneratorMS::GetPrivateProfileHex(LPCSTR AppName, LPCSTR KeyName, int DefValue, LPCSTR FileName)
{
    char Buffer[256], *p;
    int RetValue;
    GetPrivateProfileString(AppName, KeyName, "0x00", Buffer, 256, FileName);
    RetValue = 0;
    p = Buffer + 2;

    while (*p != '\0')
    {
        RetValue *= 16;

        if (*p >= 'a' && *p <= 'z')
        {
            RetValue += *p - 'a' + 10;
        }

        else if (*p >= 'A' && *p <= 'Z')
        {
            RetValue += *p - 'A' + 10;
        }

        else if (*p >= '0' && *p <= '9')
        {
            RetValue += *p - '0';
        }

        p++;
    }

    return RetValue;
}

int CSubcodeGeneratorMS::GetSessionCount(void)
{
    return m_SessionCount;
}

void CSubcodeGeneratorMS::ResetGenerator(DWORD StartLBA, int SubcodeType, int SessionNo)
{
    m_FirstTrack = 0;
    m_CurrentLBA = StartLBA;

    if (SubcodeType == 0)
    {
        int i;
        BYTE a0;
        BYTE *toc, *p, *q;
        m_TocCount = 0;
        m_TocIntCount = 0;
        p = m_Toc;
        q = m_TocInt;

        for (i = 0; i < m_EntryCount; i++)
        {
            toc = m_Entry + i * 11;

            if (toc[0] == static_cast<BYTE>(SessionNo + 1) && ((toc[1] & 0x0f) == 1) && toc[3] <= 0x63)
            {
                if (m_FirstTrack == 0)
                {
                    m_FirstTrack = toc[3];
                }

                memcpy(p, toc, 11);
                p += 11;
                m_TocCount++;
            }
        }

        for (i = 0; i < m_EntryCount; i++)
        {
            toc = m_Entry + i * 11;

            if (toc[0] == static_cast<BYTE>(SessionNo + 1) && ((toc[1] & 0x0f) == 0x05))
            {
                memcpy(q, toc, 11);
                q += 11;
                m_TocIntCount++;
            }

            else if (toc[0] == static_cast<BYTE>(SessionNo + 1) && toc[3] > 0x63)
            {
                memcpy(p, toc, 11);
                p += 11;
                m_TocCount++;
            }
        }

        //   recovery a0/a1/a2
        //   find a0
        for (i = 0; i < m_TocCount; i++)
        {
            toc = m_Toc + i * 11;

            if (toc[3] == 0xa0)
            {
                a0 = toc[1];
            }
        }

        //   set a1/a2
        for (i = 0; i < m_TocCount; i++)
        {
            toc = m_Toc + i * 11;

            if (toc[3] == 0xa1 || toc[3] == 0xa2)
            {
                toc[1] = a0;
            }
        }

        ModifyAddress();
        m_RelativeLBA = 0;
        m_TocPosCounter = 0;
        m_TocIntPosCounter = 0;
        m_Toc3Counter = 0;
        m_CDTextCounter = 0;
    }

    else if (SubcodeType == 1)
    {
        m_RelativeLBA = m_MainDataLBA[SessionNo] - m_PregapLBA[SessionNo] - m_PregapDelta;
        int i;
        BYTE* toc;
        m_PregapDelta = 0;

        for (i = 0; i < m_EntryCount; i++)
        {
            toc = m_Entry + i * 11;

            if (toc[0] == static_cast<BYTE>(SessionNo + 1) && ((toc[1] & 0x0f) == 1) && toc[3] <= 0x63)
            {
                m_FirstTrack = TO_HEX(toc[3]);
                break;
            }
        }
    }

    else
    {
        m_RelativeLBA = 0;
    }

    m_CurrentSession = SessionNo;
}

void CSubcodeGeneratorMS::CalcPositions(DWORD LeadInLBA)
{
    int session, i;
    BYTE* toc;
    m_LeadInLBA[0] = LeadInLBA;
    m_PregapLBA[0] = 0;
    m_MainDataLBA[0] = 150;
    m_LeadIn = LeadInLBA;

    if (m_LeadIn > 404849)
    {
        m_LeadIn = 450000 + m_LeadIn;
    }

    for (i = 0; i < m_EntryCount; i++)
    {
        toc = m_Entry + 11 * i;

        if (toc[0] == 1 && toc[3] == 0xa2)
        {
            m_LeadOutLBA[0] = (toc[8] * 60 + toc[9]) * 75 + toc[10];
            break;
        }
    }

    for (session = 1; session < m_SessionCount; session++)
    {
        if (session == 1)
        {
            m_LeadInLBA[session] = m_LeadOutLBA[session - 1] + 90 * 75; //   90 sec lead-out
        }

        else
        {
            m_LeadInLBA[session] = m_LeadOutLBA[session - 1] + 30 * 75; //   30 sec lead-out
        }

        for (i = 0; i < m_EntryCount; i++)
        {
            toc = m_Entry + 11 * i;

            if (toc[0] == session && toc[3] == 0xb0)
            {
                m_PregapLBA[session] = (toc[4] * 60 + toc[5]) * 75 + toc[6];
                break;
            }
        }

        for (i = 0; i < m_EntryCount; i++)
        {
            toc = m_Entry + 11 * i;

            if (toc[0] == session + 1 && toc[3] <= 100)
            {
                m_MainDataLBA[session] = (toc[8] * 60 + toc[9]) * 75 + toc[10];
                break;
            }
        }

        for (i = 0; i < m_EntryCount; i++)
        {
            toc = m_Entry + 11 * i;

            if (toc[0] == session + 1 && toc[3] == 0xa2)
            {
                m_LeadOutLBA[session] = (toc[8] * 60 + toc[9]) * 75 + toc[10];
                break;
            }
        }
    }

    if (m_SessionCount == 1)
    {
        m_LeadInLBA[m_SessionCount] = m_LeadOutLBA[m_SessionCount - 1] + 90 * 75; //   90 sec lead-out
    }

    else
    {
        m_LeadInLBA[m_SessionCount] = m_LeadOutLBA[m_SessionCount - 1] + 30 * 75; //   30 sec lead-out
    }
}

void CSubcodeGeneratorMS::CalcCRC(BYTE* Buffer)
{
    WORD crc;
    register BYTE index;
    int i;
    //   fast crc calcuration
    crc = 0;

    for (i = 0; i < 10; i++)
    {
        index = static_cast<BYTE>(Buffer[i] ^ (crc >> 8));
        crc = m_SubcodeCRCTable[index] ^ (crc << 8);
    }

    Buffer[10] = ~static_cast<BYTE>(crc >> 8);
    Buffer[11] = ~static_cast<BYTE>(crc);
}

void CSubcodeGeneratorMS::EncodeSub96(BYTE* dest, BYTE* src)
{
    int i, j, k;
    BYTE *p, *q;
    memset(dest, 0, 96);
    q = src;

    for (i = 0; i < 8; i++)
    {
        p = dest;

        for (j = 0; j < 12; j++)
        {
            for (k = 0; k < 8; k++)
            {
                *p = (*p << 1) | ((*q >> (7 - k)) & 0x01);
                p++;
            }

            q++;
        }
    }
}

BYTE* CSubcodeGeneratorMS::GenerateLeadIn(void)
{
    BYTE *toc, *subQ;
    memset(m_Sub96, 0, 96);

    if (m_Toc3Counter >= 3)
    {
        toc = m_TocInt + m_TocIntPosCounter * 11;
    }

    else
    {
        toc = m_Toc + m_TocPosCounter * 11;
    }

    subQ = m_Sub96 + 12;
    subQ[0] = toc[1]; //   ctl/adr
    subQ[1] = toc[2]; //   TNO
    subQ[2] = toc[3]; //   POINT
    subQ[3] = toc[4]; //   MIN
    subQ[4] = toc[5]; //   SEC
    subQ[5] = toc[6]; //   FRAME
    subQ[6] = toc[7]; //   Zero
    subQ[7] = toc[8]; //   AMin
    subQ[8] = toc[9]; //   ASec
    subQ[9] = toc[10]; //   AFrame

    if ((toc[3] >= 0x01 && toc[3] <= 0x99) || (toc[3] >= 0xa0 && toc[3] <= 0xa2))
    {
        DWORD lba;
        lba = m_CurrentLBA;

        if (lba > 404849)
        {
            lba = 450000 + lba;
        }

        subQ[3] = static_cast<BYTE>(TO_HEX(lba / ( 75 * 60 )));
        subQ[4] = static_cast<BYTE>(TO_HEX(( lba / 75 ) % 60));
        subQ[5] = static_cast<BYTE>(TO_HEX(lba % 75));
    }

    m_CurrentLBA++;
    m_Toc3Counter++;

    if (m_Toc3Counter >= 3)
    {
        if (m_TocIntCount == 0)
        {
            m_Toc3Counter = 0;
            m_TocPosCounter = (m_TocPosCounter + 1) % m_TocCount;
        }

        else if (m_Toc3Counter >= 6)
        {
            m_Toc3Counter = 0;
            m_TocIntPosCounter = (m_TocIntPosCounter + 1) % m_TocIntCount;
            m_TocPosCounter = (m_TocPosCounter + 1) % m_TocCount;
        }
    }

    CalcCRC(m_Sub96 + 12);

    if (m_CDTextEntries > 0)
    {
        int i1, i2, i3, i4;
        i1 = ((m_CDTextCounter) % m_CDTextEntries) * 18;
        i2 = ((m_CDTextCounter + 1) % m_CDTextEntries) * 18;
        i3 = ((m_CDTextCounter + 2) % m_CDTextEntries) * 18;
        i4 = ((m_CDTextCounter + 3) % m_CDTextEntries) * 18;
        m_CDTextCounter += 4;
        memcpy(m_Sub96 + 24 + 18 * 0, m_CDText + i1, 18);
        memcpy(m_Sub96 + 24 + 18 * 1, m_CDText + i2, 18);
        memcpy(m_Sub96 + 24 + 18 * 2, m_CDText + i3, 18);
        memcpy(m_Sub96 + 24 + 18 * 3, m_CDText + i4, 18);
        BYTE EncodeBuffer[96], EncodeTmp[96];
        int i, j, k;
        BYTE* r;
        r = m_Sub96 + 24;

        for (j = 0; j < 96; j += 4)
        {
            EncodeBuffer[j + 0] = static_cast<BYTE>((r[0] >> 2) & 0x3f);
            EncodeBuffer[j + 1] = static_cast<BYTE>(((r[0] << 4) | (r[1] >> 4)) & 0x3f);
            EncodeBuffer[j + 2] = static_cast<BYTE>(((r[1] << 2) | (r[2] >> 6)) & 0x3f);
            EncodeBuffer[j + 3] = static_cast<BYTE>(r[2] & 0x3f);
            r += 3;
        }

        {
            BYTE *s, *t;
            memset(EncodeTmp, 0, 96);

            for (k = 0; k < 12; k++)
            {
                s = EncodeBuffer + k * 8;

                for (j = 7; j >= 0; j--)
                {
                    t = EncodeTmp + k + j * 12;
                    *t |= (s[7] & 1);
                    *t |= (s[6] & 1) << 1;
                    *t |= (s[5] & 1) << 2;
                    *t |= (s[4] & 1) << 3;
                    *t |= (s[3] & 1) << 4;
                    *t |= (s[2] & 1) << 5;
                    *t |= (s[1] & 1) << 6;
                    *t |= (s[0] & 1) << 7;

                    for (i = 0; i < 8; i++)
                    {
                        s[i] = s[i] >> 1;
                    }
                }
            }

            memcpy(m_Sub96 + 24, EncodeTmp + 24, 72);
        }
    }

    return m_Sub96;
}

BYTE* CSubcodeGeneratorMS::GeneratePreGap(void)
{
    BYTE* subQ;
    memset(m_Sub96, 0, 96);

    if (m_CurrentLBA != 0)
    {
        memset(m_Sub96, 0xff, 12);
    }

    subQ = m_Sub96 + 12;

    if (m_PreGapMode[m_CurrentSession] == 0)
    {
        subQ[0] = 0x01;
    }

    else
    {
        subQ[0] = 0x41;
    }

    subQ[1] = m_FirstTrack; //   track
    subQ[2] = 0; //   index;
    subQ[3] = static_cast<BYTE>(TO_HEX(m_RelativeLBA / ( 75 * 60 )));
    subQ[4] = static_cast<BYTE>(TO_HEX(( m_RelativeLBA / 75 ) % 60));
    subQ[5] = static_cast<BYTE>(TO_HEX(m_RelativeLBA % 75));
    subQ[7] = static_cast<BYTE>(TO_HEX(m_CurrentLBA / ( 75 * 60 )));
    subQ[8] = static_cast<BYTE>(TO_HEX(( m_CurrentLBA / 75 ) % 60));
    subQ[9] = static_cast<BYTE>(TO_HEX(m_CurrentLBA % 75));
    CalcCRC(m_Sub96 + 12);
    m_CurrentLBA++;
    m_RelativeLBA--;
    return m_Sub96;
}

BYTE* CSubcodeGeneratorMS::GenerateLeadOut(void)
{
    BYTE* subQ;
    memset(m_Sub96, 0, 96);
    memset(m_Sub96, 0xff, 12);
    subQ = m_Sub96 + 12;

    if (m_PreGapMode[m_CurrentSession] == 0)
    {
        subQ[0] = 0x01;
    }

    else
    {
        subQ[0] = 0x41;
    }

    subQ[1] = 0xaa; //   track
    subQ[2] = 0; //   index;
    subQ[3] = static_cast<BYTE>(TO_HEX(m_RelativeLBA / ( 75 * 60 )));
    subQ[4] = static_cast<BYTE>(TO_HEX(( m_RelativeLBA / 75 ) % 60));
    subQ[5] = static_cast<BYTE>(TO_HEX(m_RelativeLBA % 75));
    subQ[7] = static_cast<BYTE>(TO_HEX(m_CurrentLBA / ( 75 * 60 )));
    subQ[8] = static_cast<BYTE>(TO_HEX(( m_CurrentLBA / 75 ) % 60));
    subQ[9] = static_cast<BYTE>(TO_HEX(m_CurrentLBA % 75));
    CalcCRC(m_Sub96 + 12);
    m_CurrentLBA++;
    m_RelativeLBA++;
    return m_Sub96;
}

void CSubcodeGeneratorMS::CreateZeroData(BYTE* Buffer, DWORD LBA)
{
    if (m_PreGapMode[m_CurrentSession] == 0)
    {
        memset(Buffer, 0, 2352);
    }

    else
    {
        DWORD lba = LBA;
        memset(Buffer, 0, 2352);

        if (lba > 0x80000000)
        {
            int i, j;
            BYTE LeadInData[32] =
            {
                0x16, 0x3a, 0x25, 0x2c, 0x27, 0x3c,
                0x32, 0x3d, 0x21, 0x7d, 0x36, 0x7c,
                0x75, 0x67, 0x65, 0x65, 0x66, 0x79,
                0x75, 0x0c, 0x7b, 0x1e, 0x34, 0x3b,
                0x30, 0x36, 0x3d, 0x3c, 0x3e, 0x34,
                0x75, 0x01
            };
            lba += 450000;
            j = 0;

            for (i = 0; i < 2352; i++)
            {
                Buffer[i] = LeadInData[j] ^ 0x55;
                j++;

                if (LeadInData[j] == 0x01)
                {
                    j = 0;
                }
            }
        }

        else
        {
            if (m_PreGapMode[m_CurrentSession] == 1)
            {
                m_EDC.Mode1Raw(Buffer, static_cast<BYTE>(lba / (75 * 60)), static_cast<BYTE>((lba / 75) % 60),
                               static_cast<BYTE>(lba % 75));
            }

            else
            {
                memset(Buffer + 1, 0xff, 10);
                Buffer[12] = TO_HEX(( BYTE ) ( lba / ( 75 * 60 ) ));
                Buffer[13] = TO_HEX(( BYTE ) ( ( lba / 75 ) % 60 ));
                Buffer[14] = TO_HEX(( BYTE ) ( lba % 75 ));
                Buffer[15] = 2;
                Buffer[0x12] = 0x20;
                Buffer[0x16] = 0x20;
            }
        }
    }
}

void CSubcodeGeneratorMS::CalcCDTextCRC(BYTE* Buffer)
{
    WORD crc;
    register BYTE index;
    int i;
    //   fast crc calcuration
    crc = 0;

    for (i = 0; i < 16; i++)
    {
        index = static_cast<BYTE>(Buffer[i] ^ (crc >> 8));
        crc = m_SubcodeCRCTable[index] ^ (crc << 8);
    }

    Buffer[16] = ~static_cast<BYTE>(crc >> 8);
    Buffer[17] = ~static_cast<BYTE>(crc);
}
