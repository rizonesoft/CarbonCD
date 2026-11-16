#pragma once
#include "uxtheme.h"

using tEnableThemeDialogTexture = HRESULT(PASCAL *)(HWND hWnd, DWORD dwFlags);

class CThemeController
{
public:
    CThemeController(void);
    ~CThemeController(void);

protected:
    HMODULE m_hDLL;
    tEnableThemeDialogTexture m_EnableThemeDialogTexture;

public:
    HRESULT EnableThemeDialogTexture(HWND hWnd, DWORD dwFlags);
};

extern CThemeController theTheme;
