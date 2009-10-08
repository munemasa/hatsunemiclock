#include "winheader.h"
#include <uxtheme.h>

#include <string>
#include <vector>
#include <algorithm>
#include <time.h>

#include "tListWindow.h"
#include "stringlib.h"

#include "udmessages.h"

#include "misc.h"
#include "debug.h"

#include "../resource.h"

#include "myregexp.h"

static HMODULE g_uxtheme = NULL;

typedef HRESULT (__stdcall *LPSETWINDOWTHEME)(HWND,LPCWSTR,LPCWSTR);
static LPSETWINDOWTHEME SetWindowTheme_ = NULL;
#define SETWINDOWTHEME(x,y,z)	{ if(SetWindowTheme_)SetWindowTheme_(x,y,z); }



static WNDPROC	g_oldlistviewwndproc;

static std::string g_old_filter = "dummy";
static std::string g_utf8_filteringpattern = "";
static std::string g_community_id = "";

static HWND g_listwindow_hwnd;



static struct _dmy {
	_dmy(){
		g_uxtheme = LoadLibrary(L"uxtheme.dll");
		if( g_uxtheme ){
			SetWindowTheme_ = (LPSETWINDOWTHEME)GetProcAddress( g_uxtheme, "SetWindowTheme" );
		}
	}
	~_dmy(){
		if( g_uxtheme ){
			FreeLibrary( g_uxtheme );
		}
	}
}g_dmy;



/** キーワードをレジストリに保存する.
 */
static void SaveKeyword( std::wstring& wstr )
{
	HKEY hkey;
	RegCreateKeyEx( HKEY_CURRENT_USER, REG_SUBKEY, 0, L"", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkey, NULL );
	RegSetValueEx( hkey, L"NicoMatchKeyword", 0, REG_SZ, (BYTE*)wstr.c_str(), sizeof(WCHAR)*(wstr.length()+1) );
	RegCloseKey( hkey );
}

/** フィルタリングルールをレジストリに保存する.
 */
static void SaveFilteringKeyword()
{
	HKEY hkey;
	std::wstring wstr;
	strtowstr( g_utf8_filteringpattern, wstr );
	RegCreateKeyEx( HKEY_CURRENT_USER, REG_SUBKEY, 0, L"", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hkey, NULL );
	RegSetValueEx( hkey, L"NicoFilterKeyword", 0, REG_SZ, (BYTE*)wstr.c_str(), sizeof(WCHAR)*(wstr.length()+1) );
	RegCloseKey( hkey );
}

/** フィルタリングルールをレジストリからロードする.
 */
static void LoadFilteringKeyword( )
{
	HKEY hkey;
	LONG err;
	std::wstring wstr;
	RegCreateKeyEx( HKEY_CURRENT_USER, REG_SUBKEY, 0, L"", REG_OPTION_NON_VOLATILE, KEY_READ, NULL,	&hkey, NULL );
	err = ReadRegistorySTR( hkey, L"NicoFilterKeyword", wstr );
	if( err!=ERROR_SUCCESS ){
		wstr = L"";
	}
	wstrtostr( wstr, g_utf8_filteringpattern );
	RegCloseKey( hkey );
}

/** フィルタリングダイアログのウィンドウプロシージャ.
 */
static INT_PTR CALLBACK DlgFilterProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch(msg){
	case WM_INITDIALOG:{
		std::wstring wbuf;
		strtowstr( g_utf8_filteringpattern, wbuf );
		SetDlgItemText( hDlgWnd, IDC_EDIT_SEARCHSTRING, wbuf.c_str() );
		return TRUE;}

    case WM_COMMAND:
        switch(LOWORD(wp)){
        case ID_BTN_FILTERRELEASE:
			// フィルタ解除.
			g_old_filter = "dummy";
			g_utf8_filteringpattern = "";
			SaveFilteringKeyword();
            EndDialog( hDlgWnd, ID_BTN_FILTERRELEASE );
            break;

		case IDC_BTN_SEARCH:{
			g_old_filter = g_utf8_filteringpattern;
			WCHAR buf[8192];
			GetDlgItemText( hDlgWnd, IDC_EDIT_SEARCHSTRING, buf, 8192 );
			std::wstring wbuf = buf;
			wstrtostr( wbuf, g_utf8_filteringpattern );
			SaveFilteringKeyword();
			EndDialog( hDlgWnd, IDC_BTN_SEARCH );
			break;}

		case IDCANCEL:
			EndDialog( hDlgWnd, IDCANCEL );
			break;

		default:
            return FALSE;
        }
        break;
    case WM_CLOSE:
        PostMessage( hDlgWnd, WM_COMMAND, IDCANCEL, 0 );
        return TRUE;

    default:
        return FALSE;
    }
    return TRUE;
}

/** キーワード入力ダイアログのウィンドウプロシージャ.
 */
static INT_PTR CALLBACK DlgInputKeywordProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	HWND parent;
	tListWindow*listwin;
	NicoNamaAlert*nico;

	parent = GetWindow( hDlgWnd, GW_OWNER );
	listwin = (tListWindow*)GetWindowLongPtr( parent, GWLP_USERDATA );
	nico = listwin->getNicoNamaAlert();

	switch(msg){
	case WM_INITDIALOG:
		if( nico ){
			std::string& str = nico->getMatchKeyword();
			std::wstring wstr;
			strtowstr( str, wstr );
			SetDlgItemText( hDlgWnd, IDC_EDIT_NOTICE_KEYWORD, wstr.c_str() );
		}else{
			SetDlgItemText( hDlgWnd, IDC_EDIT_NOTICE_KEYWORD, L"" );
		}
		return TRUE;

    case WM_COMMAND:
        switch(LOWORD(wp)){
		case IDCANCEL:
			EndDialog( hDlgWnd, IDCANCEL );
			break;

		case IDOK:{
			WCHAR buf[8192];
			GetDlgItemText( hDlgWnd, IDC_EDIT_NOTICE_KEYWORD, buf, 8192 );
			std::wstring wstr = buf;
			std::string str;
			wstrtostr( wstr, str );
			if( nico ) nico->setMatchKeyword( str );
			SaveKeyword( wstr );
			EndDialog( hDlgWnd, IDOK );
			break;}

		default:
            return FALSE;
        }
        break;
    case WM_CLOSE:
        PostMessage( hDlgWnd, WM_COMMAND, IDCANCEL, 0 );
        return TRUE;

    default:
        return FALSE;
    }
    return TRUE;
}

/** 通知タイプをNicoNamaAlertに設定し、レジストリに保存する.
 */
static void SaveNoticeType( HWND hDlgWnd )
{
	HWND h;
	HWND parent;
	tListWindow*listwin;
	NicoNamaAlert*nico;
	int num;

	h = GetDlgItem( hDlgWnd, IDC_COMMUNITY_LIST );
	parent = GetWindow( hDlgWnd, GW_OWNER );
	listwin = (tListWindow*)GetWindowLongPtr( parent, GWLP_USERDATA );
	nico = listwin->getNicoNamaAlert();
	if( !nico ) return;

    // 今ある分を全消去して...
	std::map<std::string,NicoNamaNoticeType> backup;
	backup = nico->getNoticeTypeList();
	nico->getNoticeTypeList().clear();

	// リストビューにある分だけ登録.
	num = ListView_GetItemCount( h );
	for(int i=0; i<num; i++){
		std::string co;
		WCHAR wbuf[1024];
		ListView_GetItemText( h, i, 0, wbuf, 1024 );
		std::wstring wstr = wbuf;
		wstrtostr( wstr, co );

		// 現在の通知タイプを取得して...
		NicoNamaNoticeType&	alert = nico->getNoticeType( co );
		alert.command	= backup[co].command;
		alert.soundfile = backup[co].soundfile;
		alert.type		= backup[co].type;

		DWORD tmp = alert.type;
		tmp &= NICO_NOTICETYPE_EXEC|NICO_NOTICETYPE_CLIPBOARD;	// EXEC,CLIPBOARD以外を消して...
		tmp |= NICO_NOTICETYPE_WINDOW | NICO_NOTICETYPE_SOUND;
		// 再設定.
		if( ListView_GetCheckState( h, i )!=0 ) {
			alert.type = tmp | NICO_NOTICETYPE_BROWSER;
		}else{
			alert.type = tmp;
		}
	}
	nico->Save();
}

static void InitDlgCommunityProperty(HWND hDlgWnd)
{
	tListWindow*listwin = (tListWindow*)GetWindowLongPtr( g_listwindow_hwnd, GWLP_USERDATA );
	NicoNamaAlert*nico  = listwin->getNicoNamaAlert();
	NicoNamaNoticeType& atype = nico->getNoticeType( g_community_id );

	// コマンド実行まわり.
	HWND parts = GetDlgItem( hDlgWnd, IDC_CHK_CMDENABLE );
	Button_SetCheck( parts, (atype.type&NICO_NOTICETYPE_EXEC)?TRUE:FALSE );
	SetDlgItemText( hDlgWnd, IDC_EDIT_COMMAND, atype.command.c_str() );

	// クリップボードにコピーするかどうかフラグ.
	parts = GetDlgItem( hDlgWnd, IDC_CHK_COPY_CLIPBOARD );
	Button_SetCheck( parts, (atype.type&NICO_NOTICETYPE_CLIPBOARD)?TRUE:FALSE );
}

/** コミュニティのプロパティダイアログのウィンドウプロシージャ.
 */
static INT_PTR CALLBACK DlgCommunityPropertyProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg){
	case WM_INITDIALOG:
		InitDlgCommunityProperty( hDlgWnd );
		return TRUE;
		break;

	case WM_COMMAND:
        switch(LOWORD(wp)){
		case IDCANCEL:
			EndDialog( hDlgWnd, IDCANCEL );
			break;

		case IDC_SELECT:{
			OPENFILENAME openfile;
			ZeroMemory( &openfile, sizeof(openfile) );
			openfile.lStructSize = sizeof(openfile);
			openfile.hwndOwner = hDlgWnd;
			openfile.lpstrFilter = L"All Files\0*.*\0\0";

			WCHAR buf[2048];
			GetDlgItemText( hDlgWnd, IDC_EDIT_COMMAND, buf, 2048 );
			openfile.lpstrFile = buf;
			openfile.nMaxFile = 2048;
			openfile.Flags = OFN_FILEMUSTEXIST | OFN_LONGNAMES;
			if( GetOpenFileName( &openfile ) ){
				dprintf( L"select: %s\n", buf );
				SetDlgItemText( hDlgWnd, IDC_EDIT_COMMAND, buf );
			}else{
				dprintf( L"You didn't select the file\n" );
			}
			return TRUE;
			break;}

		case IDOK:{
			tListWindow*listwin = (tListWindow*)GetWindowLongPtr( g_listwindow_hwnd, GWLP_USERDATA );
			NicoNamaAlert*nico  = listwin->getNicoNamaAlert();
			NicoNamaNoticeType& atype = nico->getNoticeType( g_community_id );

			// クリップボードにコピーする.
			if( IsDlgButtonChecked( hDlgWnd, IDC_CHK_COPY_CLIPBOARD) ){
				atype.type |= NICO_NOTICETYPE_CLIPBOARD;
			}else{
				atype.type &= ~NICO_NOTICETYPE_CLIPBOARD;
			}

			// コマンドの有効チェックボックス.
			if( IsDlgButtonChecked( hDlgWnd, IDC_CHK_CMDENABLE) ){
				atype.type |= NICO_NOTICETYPE_EXEC;
			}else{
				atype.type &= ~NICO_NOTICETYPE_EXEC;
			}
			// コマンド.
			WCHAR buf[2048];
			GetDlgItemText( hDlgWnd, IDC_EDIT_COMMAND, buf, 2048 );
			atype.command = buf;

			EndDialog( hDlgWnd, IDOK );
			break;}

		default:
            return FALSE;
        }
        break;

	case WM_CLOSE:
        PostMessage( hDlgWnd, WM_COMMAND, IDCANCEL, 0 );
        return TRUE;

	default:
		return FALSE;
		break;
	}
	return TRUE;
}

/** コミュニティの追加ダイアログのウィンドウプロシージャ.
 */
static INT_PTR CALLBACK DlgNewCommunityProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg){
	case WM_INITDIALOG:
		return TRUE;
		break;

	case WM_COMMAND:
        switch(LOWORD(wp)){
		case IDCANCEL:
			EndDialog( hDlgWnd, IDCANCEL );
			break;

		case IDOK:{
			WCHAR wbuf[1024];
			std::wstring wstr;
			GetDlgItemText( hDlgWnd, IDC_EDIT_COMMUNITY_ID, wbuf, 1024 );
			wstr = wbuf;
			wstrtostr( wstr, g_community_id );
			EndDialog( hDlgWnd, IDOK );
			break;}

		default:
            return FALSE;
        }
        break;

	case WM_CLOSE:
        PostMessage( hDlgWnd, WM_COMMAND, IDCANCEL, 0 );
        return TRUE;

	default:
		return FALSE;
		break;
	}
	return TRUE;
}


/** コミュニティリストダイアログの初期設定.
 */
static void InitCommunityDialog(HWND hDlgWnd)
{
	HWND h;
	HWND parent;
	tListWindow*listwin;
	NicoNamaAlert*nico;

	h = GetDlgItem( hDlgWnd, IDC_COMMUNITY_LIST );
	ListView_SetExtendedListViewStyle( h, LVS_SINGLESEL );
	ListView_SetExtendedListViewStyle( h, LVS_EX_FULLROWSELECT|LVS_EX_FLATSB|LVS_EX_DOUBLEBUFFER|LVS_EX_INFOTIP|LVS_EX_CHECKBOXES );
	SETWINDOWTHEME( h, L"explorer", NULL );

	LV_COLUMN column;
	ZeroMemory( &column, sizeof(column) );
	column.mask		= LVCF_FMT|LVCF_TEXT|LVCF_SUBITEM|LVCF_WIDTH;
	column.fmt		= LVCFMT_LEFT;
	column.pszText	= L"コミュニティID";
	column.iSubItem = 0;
	column.cx       = 100;
	ListView_InsertColumn( h, 0, &column );

	column.mask		= LVCF_FMT|LVCF_TEXT|LVCF_SUBITEM|LVCF_WIDTH;
	column.fmt		= LVCFMT_LEFT;
	column.pszText	= L"コミュニティ名";
	column.iSubItem = 1;
	column.cx       = 220;
	ListView_InsertColumn( h, 1, &column );

	parent = GetWindow( hDlgWnd, GW_OWNER );
	listwin = (tListWindow*)GetWindowLongPtr( parent, GWLP_USERDATA );
	nico = listwin->getNicoNamaAlert();
    if( !nico ) return;

    int i=0;

    // まず参加コミュニティからリストビューに追加.
    std::vector<std::wstring>& commu = nico->getCommunities();
    for( std::vector<std::wstring>::iterator it=commu.begin(); it!=commu.end(); it++,i++){
        LV_ITEM item;
        ZeroMemory( &item, sizeof(item) );
        // コミュニティID
        item.mask		= LVIF_TEXT;
        item.iItem		= i;
        item.pszText	= (LPWSTR)(*it).c_str();
        item.iSubItem	= 0;
        ListView_InsertItem(h , &item );
        // コミュニティ名.
        std::wstring communityname = nico->getCommunityName( (*it) );
        item.pszText	= (LPWSTR)communityname.c_str();
        item.iSubItem	= 1;
        ListView_SetItem( h, &item );

        std::string mbstr;
        wstrtostr( (*it), mbstr );
        NicoNamaNoticeType &t = nico->getNoticeType( mbstr );
        if( t.type & NICO_NOTICETYPE_BROWSER ){
            ListView_SetCheckState( h, i, TRUE );
        }else{
            ListView_SetCheckState( h, i, FALSE );
        }
    }// end of for.

    // 次は参加していないコミュで通知登録したコミュをリストビューに追加.
    std::map<std::string,NicoNamaNoticeType>& noticelist = nico->getNoticeTypeList();
    for( std::map<std::string,NicoNamaNoticeType>::iterator it=noticelist.begin(); it!=noticelist.end(); it++ ){
        const std::string commu_id = (*it).first;
        if( nico->isParticipant( commu_id ) ) continue;

        std::wstring wstr;
        LV_ITEM item;
        ZeroMemory( &item, sizeof(item) );
        // コミュニティID
        item.mask		= LVIF_TEXT;
        item.iItem		= i;
        strtowstr( commu_id, wstr );
        item.pszText	= (LPWSTR)wstr.c_str();
        item.iSubItem	= 0;
        ListView_InsertItem( h , &item );
        // コミュニティ名.
        std::wstring communityname = nico->getCommunityName( commu_id );
        item.pszText	= (LPWSTR)communityname.c_str();
        item.iSubItem	= 1;
        ListView_SetItem( h, &item );

        NicoNamaNoticeType &t = nico->getNoticeType( commu_id );
        if( t.type & NICO_NOTICETYPE_BROWSER ){
            ListView_SetCheckState( h, i, TRUE );
        }else{
            ListView_SetCheckState( h, i, FALSE );
        }
        i++;
    }
}

/** コミュニティリストダイアログのウィンドウプロシージャ.
 */
static INT_PTR CALLBACK DlgCommunityProc(HWND hDlgWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch(msg){
	case WM_INITDIALOG:
		InitCommunityDialog( hDlgWnd );
		return TRUE;

	case WM_NOTIFY:{
		int idCtrl = (int)wp;
		LPNMHDR pnmh = (LPNMHDR)lp;

		switch( pnmh->code ){
		case NM_RCLICK:{
            // 以下、右クリックしたときの処理.
			LPNMITEMACTIVATE itemact = (LPNMITEMACTIVATE)lp;
			INT_PTR dlgret;
			HMENU menu = LoadMenu( GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_LISTWINDOW_CONTEXTMENU) );
			HMENU submenu = GetSubMenu( menu, 1 );
			HWND h = GetDlgItem( hDlgWnd, IDC_COMMUNITY_LIST );
		    POINT pt;
			GetCursorPos( &pt );
			int r = TrackPopupMenuEx( submenu, TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, hDlgWnd, NULL );
			switch( r ){
			case ID_MENU_BTN_COMMU_PROPERTY:{
                // プロパティ.
				if( itemact->iItem<0 ) break;
				dprintf(L"open property\n");
				myListView_GetItemText( h, itemact->iItem, 0, g_community_id );
				DialogBox( GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_COMMUNITY_PROPERTY), hDlgWnd, DlgCommunityPropertyProc );
				break;}

			case ID_MENU_BTN_CREATE_COMMUNITY:
                // コミュニティの追加.
				dlgret = DialogBox( GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ENTER_COMMUNITY_ID), hDlgWnd, DlgNewCommunityProc );
				if( dlgret==IDOK ){
					tListWindow*listwin = (tListWindow*)GetWindowLongPtr( g_listwindow_hwnd, GWLP_USERDATA );
					NicoNamaAlert*nico  = listwin->getNicoNamaAlert();
                    if( !nico ) break;

                    LV_ITEM item;
                    std::wstring wstr;
                    ZeroMemory( &item, sizeof(item) );
                    // community id
                    item.mask		= LVIF_TEXT;
                    item.iItem		= ListView_GetItemCount( h );
                    strtowstr( g_community_id, wstr );
                    item.pszText	= (LPWSTR)wstr.c_str();
                    item.iSubItem	= 0;
                    ListView_InsertItem(h , &item );
                    // community name
                    std::wstring communityname = nico->getCommunityName( g_community_id );
                    item.pszText	= (LPWSTR)communityname.c_str();
                    item.iSubItem	= 1;
                    ListView_SetItem( h, &item );
				}
				break;

			case ID_MENU_BTN_DELETE_COMMUNITY:{
                // コミュニティの削除.
				if( itemact->iItem<0 ) break;
				tListWindow*listwin = (tListWindow*)GetWindowLongPtr( g_listwindow_hwnd, GWLP_USERDATA );
				NicoNamaAlert*nico  = listwin->getNicoNamaAlert();
				std::wstring cid;
				myListView_GetItemText( h, itemact->iItem, 0, cid );
				// 参加コミュニティは削除させない.
				if( nico && !nico->isParticipant( cid ) ){
					dprintf(L"delete community\n");
					ListView_DeleteItem( h, itemact->iItem );
				}
				break;}

			case ID_MENU_BTN_GO_COMMUNITY:{
				if( itemact->iItem<0 ) break;
				std::wstring cid;
				myListView_GetItemText( h, itemact->iItem, 0, cid );
				NicoNamaAlert::GoCommunity( cid );
				break;}

			default:
				break;
			}
			DestroyMenu( menu );
			break;}

		default:
			return FALSE;
		}
		return TRUE;
		break;}

    case WM_COMMAND:
        switch(LOWORD(wp)){
		case IDCANCEL:
			EndDialog( hDlgWnd, IDCANCEL );
			break;

		case IDOK:
			SaveNoticeType( hDlgWnd );
			EndDialog( hDlgWnd, IDOK );
			break;

		default:
            return FALSE;
        }
        break;

	case WM_CLOSE:
        PostMessage( hDlgWnd, WM_COMMAND, IDCANCEL, 0 );
        return TRUE;

    default:
        return FALSE;
    }
    return TRUE;
}


static LRESULT OnMenu( HWND hWnd, WPARAM wParam, LPARAM lParam )
{
    WORD id = LOWORD(wParam);
	WORD notifycode = HIWORD(wParam);
	tListWindow*listwin = (tListWindow*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
	INT_PTR dlgret;
	int col;

	switch( id ){
    case ID_MENU_BTN_FILTER:
		// フィルタリング.
		dlgret = DialogBox( GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_FILTER_DIALOG), hWnd, DlgFilterProc );
		if( dlgret==IDCANCEL ) break;
		listwin->SetFilter( g_utf8_filteringpattern );
		break;

	case ID_MENU_BTN_COMMUNITY:
        // コミュニティリスト.
		dlgret = DialogBox( GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_COMMUNITY_LIST), hWnd, DlgCommunityProc );
		break;

	case ID_MENU_BTN_NOTICE:
        // キーワード通知.
		dlgret = DialogBox( GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_INPUT_KEYWORD), hWnd, DlgInputKeywordProc );
		break;

	case ID_BTN_GO_BORADCASTING:
        // 生放送に飛ぶ.
		col = ListView_GetNextItem( listwin->getListViewHandle(), -1, LVNI_ALL|LVNI_SELECTED );
		listwin->GoPage( col, GO_TYPE_BROADCASTING_PAGE );
		break;

	case ID_BTN_GO_COMMUNITY:
        // コミュに飛ぶ.
		col = ListView_GetNextItem( listwin->getListViewHandle(), -1, LVNI_ALL|LVNI_SELECTED );
		listwin->GoPage( col, GO_TYPE_COMMUNITY_PAGE );
		break;

	case ID_MENU_BTN_ADD_COMMUNITYLIST:
		// ニコ生アラートの通知コミュニティリストに追加する.
		col = ListView_GetNextItem( listwin->getListViewHandle(), -1, LVNI_ALL|LVNI_SELECTED );
		listwin->AddCommunityList( col );
		break;

	case ID_MEMU_BTN_VIEWGROUP:{
		// カテゴリで分けて表示.
		HMENU menu = GetMenu( hWnd );
		MENUITEMINFO info;
		info.cbSize = sizeof(info);
		info.fMask = MIIM_STATE;
		GetMenuItemInfo( menu, ID_MEMU_BTN_VIEWGROUP, FALSE, &info );
		if( info.fState == MFS_CHECKED ){
			info.fState = MFS_UNCHECKED;
			SetMenuItemInfo( menu, ID_MEMU_BTN_VIEWGROUP, FALSE, &info );
		}else{
			info.fState = MFS_CHECKED;
			SetMenuItemInfo( menu, ID_MEMU_BTN_VIEWGROUP, FALSE, &info );
		}
		DrawMenuBar( hWnd );
		break;}

	case ID_MENU_BTN_DEBUG:
        // for debug
		listwin->SetBoadcastingList( listwin->getNicoNamaAlert()->getRSSProgram() );
		break;

	default:
        break;
    }
    return S_OK;
}

static LRESULT OnNotify( HWND hWnd, WPARAM wParam, LPARAM lParam )
{
	int idCtrl = (int)wParam;
	LPNMHDR pnmh = (LPNMHDR)lParam;

	switch( pnmh->code ){
	case NM_RCLICK:{
		LPNMITEMACTIVATE itemact = (LPNMITEMACTIVATE)lParam;
		tListWindow*listwin = (tListWindow*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
		listwin->ShowContextMenu( itemact->iItem );
		break;}

	default:
		break;
	}
	return S_OK;
}


static LRESULT CALLBACK ListWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	tListWindow*listwin;

	switch( message ){

	case WM_CLOSE:
		// 親の方で削除してくれるので.
		ShowWindow( hWnd, SW_HIDE );
		return 0;
		break;

	case WM_COMMAND:
		return OnMenu( hWnd, wParam, lParam );
		break;

	case WM_KEYDOWN:
		break;

	case WM_NOTIFY: return OnNotify( hWnd, wParam, lParam );
		break;

	case WM_SHOWWINDOW:
		break;

	case WM_SIZE:
		listwin = (tListWindow*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
		listwin->ResizeClientArea( LOWORD(lParam), HIWORD(lParam) );
		break;

	case WM_DESTROY:
		dprintf( L"WM_DESTROY ListWindow\n" );
		break;

	default:
		break;
	}
	return DefWindowProc( hWnd, message, wParam, lParam );
}

/** カラムの初期化.
 */
void tListWindow::InitColumn()
{
	LV_COLUMN column;
	ZeroMemory( &column, sizeof(column) );
	column.mask		= LVCF_FMT|LVCF_TEXT|LVCF_SUBITEM|LVCF_WIDTH;
	column.fmt		= LVCFMT_LEFT;
	column.pszText	= L"タイトル";
	column.iSubItem = 0;
	column.cx       = 200;
	ListView_InsertColumn( m_listview, 0, &column );

	column.pszText	= L"コミュニティ";
	column.iSubItem = 1;
	column.cx       = 100;
	ListView_InsertColumn( m_listview, 1, &column );

	column.pszText	= L"放送者";
	column.iSubItem = 2;
	ListView_InsertColumn( m_listview, 2, &column );

	column.pszText = L"開始時刻";
	column.iSubItem = 3;
	ListView_InsertColumn( m_listview, 3, &column );

	column.pszText = L"カテゴリ";
	column.iSubItem = 4;
	ListView_InsertColumn( m_listview, 4, &column );

	column.pszText = L"限定";
	column.iSubItem = 5;
	column.cx       = 50;
	ListView_InsertColumn( m_listview, 5, &column );
}


tListWindow::tListWindow( HINSTANCE hinst, HWND parent )
{
	LoadFilteringKeyword();

	m_nico = NULL;
	m_searchdialog = NULL;

	WNDCLASSEX wc;
	ZeroMemory( &wc, sizeof(wc) );
	wc.cbSize = sizeof(wc);

	wc.style = 0;
	wc.lpfnWndProc = ListWindowProc;
	wc.hInstance = hinst;
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.lpszClassName = T_LIST_CLASS;
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName = MAKEINTRESOURCE( IDR_LISTWINDOWMENU );
	if( RegisterClassEx( &wc )==NULL ){
		DWORD err = GetLastError();
		dprintf( L"tListWindow RegisterClass errorcode=%d\n", err );
	}

	DWORD dwStyle = WS_OVERLAPPEDWINDOW;
	DWORD dwExStyle = 0;
	int x,y,w,h;
	// 仮.
	x = 0; y = 0; w = 640; h = 480;
	std::wstring caption = GetStringFromResource(IDS_NICO_PROGRAM_LIST_CAPTION);
	m_hwnd = myCreateWindowEx( dwExStyle, T_LIST_CLASS, (WCHAR*)caption.c_str(), dwStyle, x, y, w, h, NULL, TRUE );
	SetWindowPos( m_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);
	g_listwindow_hwnd = m_hwnd;	// ダイアログ間でのやりとりが面倒なのでグローバルにしてしまえ.

	m_listview = myCreateWindowEx( 0, WC_LISTVIEW, L"", WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS, 0,0,w,h, m_hwnd, FALSE );
	g_oldlistviewwndproc = (WNDPROC)GetWindowLong( m_listview, GWLP_WNDPROC );

	dwExStyle = LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_FLATSB|LVS_EX_DOUBLEBUFFER|LVS_EX_INFOTIP;
	ListView_SetExtendedListViewStyle( m_listview, dwExStyle );

	SETWINDOWTHEME( m_listview, L"explorer", NULL );

	InitColumn();

	for(int i=0;i<10;i++){
		NicoNamaRSSProgram prog;
		std::wstring wbuf = L"現Ver.はニコ生のサーバに接続しないと何も機能しません。";
		wstrtostr( wbuf, prog.title );
		InsertItem( i, prog );
	}

	SetWindowLongPtr( m_hwnd, GWLP_USERDATA, (LONG_PTR)this );
}

static bool sort_pubdate_ascending_order(const NicoNamaRSSProgram& left, const NicoNamaRSSProgram& right)
{
  return left.pubDate > right.pubDate;
}

static bool sort_pubdate_descending_order(const NicoNamaRSSProgram& left, const NicoNamaRSSProgram& right)
{
  return left.pubDate < right.pubDate;
}

void tListWindow::InsertItem( int i, NicoNamaRSSProgram& prog )
{
	LV_ITEM item;
	ZeroMemory( &item, sizeof(item) );

	std::wstring wstr;

	// タイトル.
	item.mask		= LVIF_TEXT;
	item.iItem		= i;
	strtowstr( prog.title, wstr );
	item.pszText	= (LPWSTR)wstr.c_str();
	item.iSubItem	= 0;
	ListView_InsertItem( m_listview, &item );

	// コミュニティ.
	strtowstr( prog.community_name, wstr );
	item.pszText	= (LPWSTR)wstr.c_str();
	item.iSubItem	= 1;
	ListView_SetItem( m_listview, &item );

	// 生主.
	strtowstr( prog.owner_name, wstr );
	item.pszText	= (LPWSTR)wstr.c_str();
	item.iSubItem	= 2;
	ListView_SetItem( m_listview, &item );

	// 時刻.
	struct tm tm;
	WCHAR wbuf[128];
	localtime_s( &tm, &prog.pubDate );
	wcsftime( wbuf, 128, L"%H:%M", &tm );
	item.pszText = wbuf;
	item.iSubItem = 3;
	ListView_SetItem( m_listview, &item );

	// カテゴリ.
	strtowstr( prog.category, wstr );
	item.pszText	= (LPWSTR)wstr.c_str();
	item.iSubItem	= 4;
	ListView_SetItem( m_listview, &item );

	// コミュ限定.
	if( prog.member_only ){
		item.pszText	= L"限";
		item.iSubItem	= 5;
		ListView_SetItem( m_listview, &item );
	}
}

// リストビューに項目を設定する.
void tListWindow::SetBoadcastingList( std::map<std::string,NicoNamaRSSProgram>& rssprog )
{
	m_rssprog.clear();
	std::map<std::string,NicoNamaRSSProgram>::iterator it;
	for( it=rssprog.begin(); it!=rssprog.end(); it++ ){
		if( (*it).second.pubDate ){
			m_rssprog.push_back( (*it).second );
		}
	}
	std::sort( m_rssprog.begin(), m_rssprog.end(), sort_pubdate_ascending_order );

	SetFilter( g_utf8_filteringpattern );
}

/** フィルタリングをかける.
 * @param filter フィルタリングルール(正規表現) 破壊されるので参照渡しはなしで.
 */
void tListWindow::SetFilter( std::string filter )
{
	int i=0;
	if( g_old_filter==filter ){
		dprintf( L"フィルタリングルールが変わってないので何もしない\n" );
		return;
	}

	m_filteredlist.clear();
	if( filter.length() ){
		myPCRE re( filter.c_str(), PCRE_UTF8|PCRE_MULTILINE );
		if( re.hasError() ){
			MessageBoxA( m_hwnd, re.hasError(), "", MB_OK );
		}
		ListView_DeleteAllItems( m_listview );
		std::vector<NicoNamaRSSProgram>::iterator it2;
		for( it2=m_rssprog.begin(); it2!=m_rssprog.end(); it2++ ){
			if( re.match( (*it2).description.c_str() ) ||
				re.match( (*it2).title.c_str() ) ){
				InsertItem( i, (*it2) );
				i++;
				m_filteredlist.push_back( (*it2) );
			}
		}
	}else{
		ListView_DeleteAllItems( m_listview );
		filter = "(no filter)";
		std::vector<NicoNamaRSSProgram>::iterator it2;
		for( it2=m_rssprog.begin(); it2!=m_rssprog.end(); it2++,i++ ){
			InsertItem( i, (*it2) );
			m_filteredlist.push_back( (*it2) );
		}
	}

	std::wstring wstr = GetStringFromResource(IDS_NICO_PROGRAM_LIST_CAPTION);
	std::wstring wtmp;
	strtowstr( filter, wtmp );

	WCHAR wbuf[128];
	wsprintf( wbuf, L"(%d) ", i );
	wstr += wbuf;
	wstr += L"\"" + wtmp + L"\"";
	SetWindowText( m_hwnd, wstr.c_str() );
}

void tListWindow::ShowContextMenu( int index )
{
	if(index<0) return;

	HMENU menu;
    HMENU submenu;

    menu = LoadMenu( GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_LISTWINDOW_CONTEXTMENU) );
    submenu = GetSubMenu( menu, 0 ); 

    POINT pt;
	int r;
	GetCursorPos( &pt );
    r = TrackPopupMenuEx( submenu, TPM_LEFTALIGN, pt.x, pt.y, m_hwnd, NULL );

	DestroyMenu( menu );
}

// 番組一覧から通知コミュニティリストに追加してレジストリに通知リストを保存する.
void tListWindow::AddCommunityList( int col )
{
	if( col<0 ) return;
	NicoNamaAlert*nico = getNicoNamaAlert();
	if( !nico ) return;

	std::string& cid = m_filteredlist[col].community_id;
	NicoNamaNoticeType notice = nico->getNoticeType( cid );	// すでに登録済みかもしれないので一旦取得.
	notice.type |= NICO_NOTICETYPE_WINDOW | NICO_NOTICETYPE_SOUND;
	nico->setNoticeType( cid, notice );
	nico->Save();
}

void tListWindow::GoPage( int col, int type )
{
	std::wstring wstr;
	if( col<0 ) return;

	switch( type ){
	case GO_TYPE_BROADCASTING_PAGE:
		strtowstr( m_filteredlist[col].link, wstr );
		OpenURL( wstr );
		break;

	case GO_TYPE_COMMUNITY_PAGE:
		NicoNamaAlert::GoCommunity( m_filteredlist[col].community_id );
		break;

	default:
		break;
	}
}


void tListWindow::Show( NicoNamaAlert*nico )
{
	m_nico = nico;
	ShowWindow( m_hwnd, SW_SHOWNORMAL );
	UpdateWindow( m_hwnd );
}

void tListWindow::Hide()
{
	ShowWindow( m_hwnd, SW_HIDE );
}

tListWindow::~tListWindow()
{
	DestroyWindow( m_listview );
	DestroyWindow( m_hwnd );
}
