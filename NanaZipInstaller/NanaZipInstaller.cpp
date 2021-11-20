/*
 * PROJECT:   NanaZipInstaller
 * FILE:      NanaZipInstaller.cpp
 * PURPOSE:   Implementation for NanaZip Installer
 *
 * LICENSE:   The MIT License
 *
 * DEVELOPER: Mouri_Naruto (Mouri_Naruto AT Outlook.com)
 */

#include <Windows.h>

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);

    // 纳纳齐普安装器预想设计
    // 默认 自动判断，如果已安装则弹出配置（是否注册成全局应用，卸载）对话框
    // --Install 安装 NanaZip （没有 --Source 参数则从 Microsoft Store 下载）
    // --Source 本地 NanaZip 包
    // --Global 将 NanaZip 注册成全局应用
    // --Uninstall 卸载 NanaZip

    ::MessageBoxW(
        nullptr,
        L"Hello, NanaZipInstaller!\n",
        L"NanaZipInstaller",
        0);

    return 0;
}
