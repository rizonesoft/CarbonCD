#include "stdafx.h"
#include <string>

#include "language.h"

CLanguage::CLanguage()
= default;

CLanguage::~CLanguage()
= default;

void CLanguage::read_language(const LPCSTR file_name)
{
    CStdioFile file;

    if (!file.Open(file_name, CFile::modeRead | CFile::typeBinary))
    {
        char buffer[1024];
        sprintf(buffer, "Can't read language file\nFile:'%s'", file_name);
        MessageBox(nullptr, buffer, "Caution!!", MB_OK);
        return;
    }

    int i = 0;
    CString line;

    while (file.ReadString(line))
    {
        if (line.IsEmpty())
        {
            break;
        }

        // Convert CString to std::string
        CT2CA pszConvertedAnsiString(line);
        std::string strStd(pszConvertedAnsiString);

        if (strStd[0] != '#')
        {
            m_Str[i].Format("%s", strStd.c_str());
            i++;
        }

        if (i >= STRING_COUNT)
        {
            break;
        }
    }

    file.Close();
}