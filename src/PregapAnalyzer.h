#pragma once
#include "CDController.h"
#include "LogWindow.h"

class CPregapAnalyzer
{
public:
    CPregapAnalyzer(void);
    ~CPregapAnalyzer(void);
    CCDController* m_CD;
    CLogWindow* m_Log;
    int m_AddressDelta;
    void AnalyzePregap(void);

protected:
    BYTE m_Buffer[2352 + 96];
    BYTE m_Pregap[150][2352];
    bool m_Existing[150];

public:
    bool Succeed(void);
    void WritePregap(LPCSTR FileName);
};
