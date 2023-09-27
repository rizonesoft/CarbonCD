#pragma once

#define STRING_COUNT    400

#define LP                  6
#define LP_MAINMENU         (0  + LP)
#define LP_TOCMENU          (22 + LP_MAINMENU)
#define LP_LOGMENU          (7  + LP_TOCMENU)
#define LP_MASTERINGMENU    (5  + LP_LOGMENU)
#define LP_MASTERINGPOPUP   (17 + LP_MASTERINGMENU)
#define LP_ABOUT            (7  + LP_MASTERINGPOPUP)
#define LP_MSF              (2  + LP_ABOUT)
#define LP_CREATEP          (3  + LP_MSF)
#define LP_DC               (3  + LP_CREATEP)
#define LP_ERASE            (13 + LP_DC)
#define LP_MAIN             (5  + LP_ERASE)
#define LP_MASTERING        (9  + LP_MAIN)
#define LP_READP            (34 + LP_MASTERING)
#define LP_READS            (3  + LP_READP)
#define LP_READT            (20 + LP_READS)
#define LP_SET              (2  + LP_READT)
#define LP_SUBQ             (19  + LP_SET)
#define LP_WRITEP           (3  + LP_SUBQ)
#define LP_WRITES           (3  + LP_WRITEP)
#define LP_TOC              (13 + LP_WRITES)
#define LP_LOG              (16 + LP_TOC)
#define LP_MESSAGE          (2  + LP_LOG)

class CLanguage
{
public:
    CLanguage(void);
    ~CLanguage(void);
    CString m_Str[STRING_COUNT];
    void read_language(LPCSTR file_name);
};
