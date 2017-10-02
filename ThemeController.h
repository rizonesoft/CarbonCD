#pragma once
#include "uxtheme.h"

typedef HRESULT ( PASCAL *tEnableThemeDialogTexture ) ( HWND hWnd, DWORD dwFlags );

class CThemeController {
    public:
        CThemeController ( void );
        ~CThemeController ( void );
    protected:
        HMODULE m_hDLL;
        tEnableThemeDialogTexture m_EnableThemeDialogTexture;
    public:
        HRESULT EnableThemeDialogTexture ( HWND hWnd, DWORD dwFlags );
};

extern CThemeController theTheme;
