#ifndef TLISTWINDOW_H_DEF
#define TLISTWINDOW_H_DEF

#include "winheader.h"
#include "niconamaalert.h"

#include <string>
#include <vector>
#include <map>

#define T_LIST_CLASS	L"Miku39_tListClass"


enum {
	GO_TYPE_BROADCASTING_PAGE,
	GO_TYPE_COMMUNITY_PAGE,
};


// prototype
LRESULT CALLBACK ListWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );


// class define
class tListWindow {
	friend LRESULT CALLBACK ListWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

	HWND		m_hwnd;
	HWND		m_parenthwnd;
	HWND		m_listview;
	HWND		m_searchdialog;

	NicoNamaAlert *m_nico;
	std::vector<NicoNamaRSSProgram>	m_rssprog;
	std::vector<NicoNamaRSSProgram> m_filteredlist;

	void InitColumn();
	void InsertItem( int i, NicoNamaRSSProgram& prog );

public:
	tListWindow( HINSTANCE hinst, HWND parent );
	~tListWindow();

	inline HWND getWindowHandle() const { return m_hwnd; }
	inline HWND getListViewHandle() const { return m_listview; }

	inline void setSearchDialog( HWND h ){ m_searchdialog = h; }
	inline HWND getSearchDialog() const { return m_searchdialog; }
	inline NicoNamaAlert* getNicoNamaAlert(){ return m_nico; }
	inline void setNicoNamaAlert( NicoNamaAlert*n ){ m_nico = n; }

	void SetFilter( std::string filter );
	void SetBoadcastingList( std::map<std::string,NicoNamaRSSProgram>& rssprog );
	void ShowContextMenu( int index );
	void GoPage( int col, int type );

	void Show( NicoNamaAlert*nico );
	void Hide();

	inline void ResizeClientArea( int w, int h ){
		MoveWindow( m_listview, 0, 0, w, h, TRUE ); 
	}
};


#endif
