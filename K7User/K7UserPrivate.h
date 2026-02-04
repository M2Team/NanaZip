/*
 * PROJECT:    NanaZip Platform User Library (K7User)
 * FILE:       K7UserPrivate.h
 * PURPOSE:    Definition for NanaZip Platform User Private Interfaces
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef K7_USER_PRIVATE
#define K7_USER_PRIVATE

#include "K7User.h"

#ifndef K7_USER_WIN_USER_PRIVATE
#define K7_USER_WIN_USER_PRIVATE

// Reference: https://github.com/adzm/win32-custom-menubar-aero-theme

// window messages related to menu bar drawing
#define WM_UAHDESTROYWINDOW 0x0090 // handled by DefWindowProc
#define WM_UAHDRAWMENU 0x0091 // lParam is UAHMENU
#define WM_UAHDRAWMENUITEM 0x0092 // lParam is UAHDRAWMENUITEM
#define WM_UAHINITMENU 0x0093 // handled by DefWindowProc
#define WM_UAHMEASUREMENUITEM 0x0094 // lParam is UAHMEASUREMENUITEM
#define WM_UAHNCPAINTMENUPOPUP 0x0095 // handled by DefWindowProc

// describes the sizes of the menu bar or menu item
typedef union tagUAHMENUITEMMETRICS
{
    // cx appears to be 14 / 0xE less than rcItem's width!
    // cy 0x14 seems stable, i wonder if it is 4 less than rcItem's height which
    //    is always 24 atm
    struct {
        DWORD cx;
        DWORD cy;
    } rgsizeBar[2];
    struct {
        DWORD cx;
        DWORD cy;
    } rgsizePopup[4];
} UAHMENUITEMMETRICS, *PUAHMENUITEMMETRICS;

// not really used in our case but part of the other structures
typedef struct tagUAHMENUPOPUPMETRICS
{
    DWORD rgcx[4];

    // from kernel symbols, padded to full dword
    DWORD fUpdateMaxWidths : 2;
} UAHMENUPOPUPMETRICS, *PUAHMENUPOPUPMETRICS;

// hmenu is the main window menu; hdc is the context to draw in
typedef struct tagUAHMENU
{
    HMENU hmenu;
    HDC hdc;
    // no idea what these mean, in my testing it's either 0x00000a00 or
    // sometimes 0x00000a10
    DWORD dwFlags;
} UAHMENU, *PUAHMENU;

// menu items are always referred to by iPosition here
typedef struct tagUAHMENUITEM
{
    int iPosition; // 0-based position of menu item in menubar
    UAHMENUITEMMETRICS umim;
    UAHMENUPOPUPMETRICS umpm;
} UAHMENUITEM, *PUAHMENUITEM;

// the DRAWITEMSTRUCT contains the states of the menu items, as well as
// the position index of the item in the menu, which is duplicated in
// the UAHMENUITEM's iPosition as well
typedef struct UAHDRAWMENUITEM
{
    DRAWITEMSTRUCT dis; // itemID looks uninitialized
    UAHMENU um;
    UAHMENUITEM umi;
} UAHDRAWMENUITEM, *PUAHDRAWMENUITEM;

// the MEASUREITEMSTRUCT is intended to be filled with the size of the item
// height appears to be ignored, but width can be modified
typedef struct tagUAHMEASUREMENUITEM
{
    MEASUREITEMSTRUCT mis;
    UAHMENU um;
    UAHMENUITEM umi;
} UAHMEASUREMENUITEM, *PUAHMEASUREMENUITEM;

#endif // !K7_USER_WIN_USER_PRIVATE

#ifndef K7_USER_DARK_MODE_PRIVATE
#define K7_USER_DARK_MODE_PRIVATE

/**
 * @brief Uninitializes the dark mode support.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7UserUninitializeDarkModeSupport();

#endif // !K7_USER_DARK_MODE_PRIVATE


#endif // !K7_USER_PRIVATE
