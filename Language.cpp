#include "stdafx.h"
#include "language.h"

CLanguage::CLanguage ( void )
{
}

CLanguage::~CLanguage ( void )
{
}

void CLanguage::ReadLanguage ( LPCSTR FileName )
{
    int i;
    char Buffer[1024];
    FILE *fp;
    fp = fopen ( FileName, "r" );

    if ( fp == NULL )
        {
            sprintf ( Buffer, "Can't read language file\nFile:'%s'", FileName );
            MessageBox ( NULL, Buffer, "Caution!!", MB_OK );
            return;
        }

    i = 0;

    while ( !feof ( fp ) )
        {
            Buffer[0] = '\0';
            fgets ( Buffer, 1023, fp );

            if ( Buffer[0] == '\0' )
                {
                    break;
                }

            Buffer[1023] = '\0';

            if ( Buffer[lstrlen ( Buffer ) - 1] == '\n' )
                {
                    Buffer[lstrlen ( Buffer ) - 1] = '\0';
                }

            if ( Buffer[0] != '#' )
                {
                    m_Str[i].Format ( "%s", Buffer );
                    i++;
                }

            if ( i >= STRING_COUNT )
                {
                    break;
                }
        }

    fclose ( fp );
    /*  {
            CString cs;
            cs.Format("String count:%d",i);
            MessageBox(NULL,cs,"Assertion!!",MB_OK);
        }
    // */
}
