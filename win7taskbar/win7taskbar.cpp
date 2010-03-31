/*
 * xdelfile
 *
 * Copyright(C) 2004-2008 Mallow <mallow at livedoor.com>
 * All Rights Reserved
 */

#include <Windows.h>
#include <ShObjIdl.h>
#include "resource.h"
#include "..\..\..\..\..\..\..\fittlesrc\source\fittle\src\plugin.h"

#if defined(_MSC_VER)
#pragma comment(lib,"kernel32.lib")
#pragma comment(lib,"user32.lib")
#endif
#if defined(_MSC_VER) && !defined(_DEBUG)
//#pragma comment(linker,"/ENTRY:DllMain")
#pragma comment(linker,"/MERGE:.rdata=.text")
#pragma comment(linker,"/OPT:NOWIN98")
#endif

#define MAX_FITTLE_PATH 260*2

#define ID_SEEKTIMER 200
#define IDM_PREV 40044
#define IDM_PLAYPAUSE 40164
#define IDM_STOP 40047
#define IDM_NEXT 40048

static HMODULE hDLL = 0;
static HICON hIconPlay = NULL;
static HICON hIconPause = NULL;

static ITaskbarList3* m_pTaskbarList = NULL; // タスクバーリスト
static THUMBBUTTON m_ThumbButtons[4];
static DWORD m_wmTBC = (DWORD)-1; // タスクバーボタン生成メッセージ

static BOOL OnInit();
static void OnQuit();
static void OnTrackChange();
static void OnStatusChange();
static void OnConfig();

static FITTLE_PLUGIN_INFO fpi = {
	PDK_VER,
	OnInit,
	OnQuit,
	OnTrackChange,
	OnStatusChange,
	OnConfig,
	NULL,
	NULL
};

static LRESULT SendFittleMessage(UINT uMsg, WPARAM wp, LPARAM lp){
	return SendMessage(fpi.hParent, uMsg, wp, lp);
}

static LRESULT SendFittleCommand(int nCmd){
	return SendFittleMessage(WM_COMMAND, MAKEWPARAM(nCmd, 0), 0);
}

static void SetTaskBarButton(THUMBBUTTON* button, THUMBBUTTONMASK dwMask, UINT iId, HICON hIcon, LPCWSTR szTip, THUMBBUTTONFLAGS dwFlag){
	button->dwMask = dwMask;
	button->iId = iId;
	button->hIcon = hIcon;
	wcscpy_s(button->szTip, sizeof(button->szTip)/sizeof(button->szTip[0]), szTip);
	button->dwFlags = dwFlag;
}

static void ResetTaskbarProgress(){
	if(m_pTaskbarList == NULL)return;

	switch((int)SendMessage(fpi.hParent, WM_FITTLE, GET_STATUS, 0)){
	case 1: // Play
		SetTaskBarButton(&m_ThumbButtons[1], THB_ICON|THB_TOOLTIP|THB_FLAGS, 1,	hIconPause, L"Pause", THBF_ENABLED);
		m_pTaskbarList->SetProgressState(fpi.hParent, TBPF_NORMAL);
		m_ThumbButtons[1].hIcon = hIconPause;
		break;
	case 3: // Pause
		SetTaskBarButton(&m_ThumbButtons[1], THB_ICON|THB_TOOLTIP|THB_FLAGS, 1,	hIconPlay, L"Play", THBF_ENABLED);
		m_pTaskbarList->SetProgressState(fpi.hParent, TBPF_PAUSED);
		m_ThumbButtons[1].hIcon = hIconPlay;
		break;
	default:
		SetTaskBarButton(&m_ThumbButtons[1], THB_ICON|THB_TOOLTIP|THB_FLAGS, 1,	hIconPlay, L"Play", THBF_ENABLED);
		m_pTaskbarList->SetOverlayIcon(fpi.hParent, NULL ,L"");
		m_pTaskbarList->SetProgressState(fpi.hParent, TBPF_NOPROGRESS);
		m_ThumbButtons[1].hIcon = hIconPlay;
		break;
	}
}
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved){
	(void)lpvReserved;
	if (fdwReason == DLL_PROCESS_ATTACH){
		hDLL = hinstDLL;
		DisableThreadLibraryCalls(hinstDLL);
	}else if (fdwReason == DLL_PROCESS_DETACH){
	}
	return TRUE;
}

static BOOL bOldProc;
static WNDPROC hOldProc;

// サブクラス化したウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp){
	switch(msg){
		case WM_COMMAND:
			if (HIWORD(wp) == THBN_CLICKED) {
				switch(LOWORD(wp)){
				case 0:
					SendFittleCommand(IDM_PREV);
					break;
				case 1:
					SendFittleCommand(IDM_PLAYPAUSE);
					break;
				case 2:
					SendFittleCommand(IDM_STOP);
					break;
				case 3:
					SendFittleCommand(IDM_NEXT);
					break;
				}
			}
			break;
			
		case WM_TIMER:

			if(wp == ID_SEEKTIMER){
				if(m_pTaskbarList != NULL)m_pTaskbarList->SetProgressValue(fpi.hParent, SendMessage(hWnd, WM_FITTLE, GET_POSITION, 0), SendMessage(hWnd, WM_FITTLE, GET_DURATION, 0));
			}
			break;
		default:
			if(msg == m_wmTBC){ // case m_wmTBCとすると怒られるのでdefaultの下にいれます・・・
				ResetTaskbarProgress();
			    m_pTaskbarList->ThumbBarAddButtons(hWnd, sizeof(m_ThumbButtons)/sizeof(m_ThumbButtons[0]), m_ThumbButtons);
				break;
			}
	}
	return (bOldProc ? CallWindowProcW : CallWindowProcA)(hOldProc, hWnd, msg, wp, lp);
}

/* 起動時に一度だけ呼ばれます */
static BOOL OnInit(){
	bOldProc = IsWindowUnicode(fpi.hParent);
	hOldProc = (WNDPROC)(bOldProc ? SetWindowLongW : SetWindowLongA)(fpi.hParent, GWL_WNDPROC, (LONG)WndProc);
	CoCreateInstance(
	    CLSID_TaskbarList, NULL, CLSCTX_ALL,
	    IID_ITaskbarList3, (void**)&m_pTaskbarList);
	m_wmTBC = RegisterWindowMessage(L"TaskbarButtonCreated");
	hIconPlay = (HICON)LoadImage(hDLL, MAKEINTRESOURCE(IDI_PLAY),IMAGE_ICON, 16, 16, LR_DEFAULTSIZE | LR_SHARED );
	hIconPause = (HICON)LoadImage(hDLL, MAKEINTRESOURCE(IDI_PAUSE),IMAGE_ICON, 16, 16, LR_DEFAULTSIZE | LR_SHARED );
	SetTaskBarButton(&m_ThumbButtons[0], THB_ICON|THB_TOOLTIP|THB_FLAGS, 0,
		(HICON)LoadImage(hDLL, MAKEINTRESOURCE(IDI_PREV),IMAGE_ICON, 16, 16, LR_DEFAULTSIZE | LR_SHARED ),
		L"Prev", THBF_ENABLED);
	SetTaskBarButton(&m_ThumbButtons[1], THB_ICON|THB_TOOLTIP|THB_FLAGS, 1,	hIconPlay, L"Play", THBF_ENABLED);

	SetTaskBarButton(&m_ThumbButtons[2], THB_ICON|THB_TOOLTIP|THB_FLAGS, 2,
		(HICON)LoadImage(hDLL, MAKEINTRESOURCE(IDI_STOP),IMAGE_ICON, 16, 16, LR_DEFAULTSIZE | LR_SHARED ),
		L"Stop", THBF_ENABLED);
	SetTaskBarButton(&m_ThumbButtons[3], THB_ICON|THB_TOOLTIP|THB_FLAGS, 3,
		(HICON)LoadImage(hDLL, MAKEINTRESOURCE(IDI_NEXT),IMAGE_ICON, 16, 16, LR_DEFAULTSIZE | LR_SHARED ),
		L"Next", THBF_ENABLED);

	return TRUE;
}

/* 終了時に一度だけ呼ばれます */
static void OnQuit(){
	return;
}


/* 曲が変わる時に呼ばれます */
static void OnTrackChange(){

}

/* 再生状態が変わる時に呼ばれます */
static void OnStatusChange(){
	ResetTaskbarProgress();
	m_pTaskbarList->ThumbBarUpdateButtons(fpi.hParent, sizeof(m_ThumbButtons)/sizeof(m_ThumbButtons[0]), m_ThumbButtons);
}

/* 設定画面を呼び出します（未実装）*/
static void OnConfig(){
}

#ifdef __cplusplus
extern "C"
{
#endif
__declspec(dllexport) FITTLE_PLUGIN_INFO *GetPluginInfo(){
	return &fpi;
}
#ifdef __cplusplus
}
#endif