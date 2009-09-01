#include <windows.h>
#include <comutil.h>
#include "iTunesCOMInterface.h"

#include "iTunesLib.h"

#pragma comment(lib, "comsupp.lib")		// COM
#pragma comment(lib, "comsuppw.lib")	// COM

static HWND g_parenthwnd = NULL;

static DWORD g_adviceCookie = 0;
static CITunesEventSink *g_pSink = NULL;
static IConnectionPoint * g_pCP = NULL;
static IUnknown *g_pSinkUnk = NULL;

HWND FindITunes()
{
	return FindWindow( L"iTunes", NULL );
}

bool CreateITunesCOM( IiTunes ** iTunes, HWND hwnd )
{
	if( CoCreateInstance( CLSID_iTunesApp, NULL, CLSCTX_LOCAL_SERVER, IID_IiTunes, (PVOID*)iTunes)==S_OK ){
		g_parenthwnd = hwnd;
		return true;
	}
	return false;
}

bool ConnectITunesEvent(IiTunes *iTunes)
{
	HRESULT hr;
	if( iTunes==NULL ) return false;

	// get CPC
	IConnectionPointContainer *pCPC;
	hr = iTunes->QueryInterface( IID_IConnectionPointContainer, (void **)&pCPC );
	_IiTunesEvents *pITEvent;
	hr = pCPC->FindConnectionPoint( __uuidof(pITEvent), &g_pCP );
	// release CPC
	pCPC->Release();

	g_pSink = new CITunesEventSink;
	// get SinkUnk
	hr = g_pSink->QueryInterface(IID_IUnknown,(void **)&g_pSinkUnk);
	hr = g_pCP->Advise(g_pSinkUnk, &g_adviceCookie);
	return true;
}

bool DisconnectITunesEvent()
{
	if( g_pCP ){
		g_pCP->Unadvise( g_adviceCookie );
		g_pCP->Release();
		g_pCP = NULL;
	}
	// release SinkUnk
	if( g_pSinkUnk ){
		g_pSinkUnk->Release();
		g_pSinkUnk = NULL;
	}
	g_pSink = NULL;
	return true;
}

HRESULT STDMETHODCALLTYPE CITunesEventSink::Invoke(DISPID dispidMember, REFIID riid,
		   LCID lcid, WORD wFlags, DISPPARAMS* pdispparams, VARIANT* pvarResult,
		   EXCEPINFO* pexcepinfo, UINT* puArgErr)
{
	PostMessage( g_parenthwnd, WM_ITUNES, (WPARAM)dispidMember, 0 );
	return S_OK;
}
