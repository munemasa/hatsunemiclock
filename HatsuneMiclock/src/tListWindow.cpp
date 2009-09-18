#include "winheader.h"
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


static WNDPROC	g_oldlistviewwndproc;

static std::string g_old_filter = "dummy";
static std::string g_utf8_filteringpattern = "";

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
        case IDCANCEL:
			// フィルタ解除.
			g_old_filter = "dummy";
			g_utf8_filteringpattern = "";
            EndDialog( hDlgWnd, IDCANCEL );
            break;

		case IDC_BTN_SEARCH:{
			g_old_filter = g_utf8_filteringpattern;
			WCHAR buf[8192];
			GetDlgItemText( hDlgWnd, IDC_EDIT_SEARCHSTRING, buf, 8192 );
			std::wstring wbuf = buf;
			wstrtostr( wbuf, g_utf8_filteringpattern );
			EndDialog( hDlgWnd, IDC_BTN_SEARCH );
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

static LRESULT CALLBACK CCLIstViewSubProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch( message ){
	case WM_KEYDOWN:
		PostMessage( GetParent( hWnd ), message, wParam, lParam );
		return 0;
		break;

	default:
		break;
	}
	return CallWindowProc( g_oldlistviewwndproc, hWnd, message, wParam, lParam );
}


static LRESULT OnMenu( HWND hWnd, WPARAM wParam, LPARAM lParam )
{
    WORD id = LOWORD(wParam);
	WORD notifycode = HIWORD(wParam);
	tListWindow*listwin = (tListWindow*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
	INT_PTR dlgret;
	int col;

	switch( id ){
    case ID_MENU_BTN_SEARCH:
		dlgret = DialogBox( GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_SEARCH_DIALOG), hWnd, DlgFilterProc );
		// フィルタリング.
		listwin->SetFilter( g_utf8_filteringpattern );
		break;

	case ID_BTN_GO_BORADCASTING:
		col = ListView_GetNextItem( listwin->getListViewHandle(), -1, LVNI_ALL|LVNI_SELECTED );
		listwin->GoPage( col, GO_TYPE_BROADCASTING_PAGE );
		break;

	case ID_BTN_GO_COMMUNITY:
		col = ListView_GetNextItem( listwin->getListViewHandle(), -1, LVNI_ALL|LVNI_SELECTED );
		listwin->GoPage( col, GO_TYPE_COMMUNITY_PAGE );
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
		dprintf( L"%d\n", itemact->iItem );

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

	column.pszText	= L"放送主";
	column.iSubItem = 2;
	ListView_InsertColumn( m_listview, 2, &column );

	column.pszText = L"開始時刻";
	column.iSubItem = 3;
	ListView_InsertColumn( m_listview, 3, &column );

	column.pszText = L"カテゴリ";
	column.iSubItem = 4;
	ListView_InsertColumn( m_listview, 4, &column );
}


tListWindow::tListWindow( HINSTANCE hinst, HWND parent )
{
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
	x = 0;
	y = 0;
	w = 640;
	h = 480;
	std::wstring caption = GetStringFromResource(IDS_NICO_PROGRAM_LIST_CAPTION);
	m_hwnd = myCreateWindowEx( dwExStyle, T_LIST_CLASS, (WCHAR*)caption.c_str(), dwStyle, x, y, w, h, NULL/*parent*/, TRUE );
	SetWindowPos( m_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE);


	m_listview = myCreateWindowEx( 0, WC_LISTVIEW, L"", WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS, 0,0,w,h, m_hwnd, FALSE );
	g_oldlistviewwndproc = (WNDPROC)GetWindowLong( m_listview, GWLP_WNDPROC );
	LONG l = SetWindowLong( m_listview, GWLP_WNDPROC, (LONG)CCLIstViewSubProc ); 
	if( l==0 ){
		DWORD err = GetLastError();
		dprintf( L"subclassing:%d\n", err );
	}

	dwExStyle = LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_FLATSB|LVS_EX_DOUBLEBUFFER;
	ListView_SetExtendedListViewStyle( m_listview, dwExStyle );

	InitColumn();

	NicoNamaRSSProgram prog;
	std::wstring wbuf = L"現Ver.はニコ生のサーバに接続しないと表示されません。";
	wstrtostr( wbuf, prog.title );
	InsertItem( 0, prog );

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
}

// リストビューに項目を設定する.
void tListWindow::SetBoadcastingList( std::map<std::string,NicoNamaRSSProgram>*rssprog )
{
	m_rssprog.clear();
	std::map<std::string,NicoNamaRSSProgram>::iterator it;
	for( it=rssprog->begin(); it!=rssprog->end(); it++ ){
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
		strtowstr( m_filteredlist[col].community_id, wstr );
		if( m_filteredlist[col].community_id.find("ch")!=std::string::npos ){
			// channel
			OpenURL( std::wstring( NICO_CHANNEL_URL + wstr ) );
		}else{
			// community
			OpenURL( std::wstring( NICO_COMMUNITY_URL + wstr ) );
		}
		break;

	default:
		break;
	}
}


void tListWindow::Show()
{
	ShowWindow( m_hwnd, SW_SHOW );
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
