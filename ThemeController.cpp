#include "stdafx.h"
#include "themecontroller.h"

CThemeController::CThemeController(void)
{
    m_EnableThemeDialogTexture = nullptr;
    m_hDLL = LoadLibrary("uxtheme.dll");

    if (m_hDLL)
    {
        m_EnableThemeDialogTexture = (tEnableThemeDialogTexture)GetProcAddress(m_hDLL, "EnableThemeDialogTexture");
    }
}

CThemeController::~CThemeController(void)
{
    if (m_hDLL)
    {
        FreeLibrary(m_hDLL);
    }

    m_EnableThemeDialogTexture = nullptr;
}

HRESULT CThemeController::EnableThemeDialogTexture(HWND hWnd, DWORD dwFlags)
{
    if (m_EnableThemeDialogTexture == nullptr)
    {
        return S_OK;
    }

    return m_EnableThemeDialogTexture(hWnd, dwFlags);
}
