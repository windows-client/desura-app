/*
Copyright (C) 2011 Mark Chandler (Desura Net Pty Ltd)
Copyright (C) 2014 Bad Juju Games, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.

Contact us at legal@badjuju.com.

*/

#include "Common.h"
#include "gcFrame.h"

#ifdef WIN32
#include <shobjidl.h>
extern ITaskbarList3* g_pITBL3;
#endif

#include "managers/CVar.h"
#include "managers/Managers.h"

#include "wx_controls/gcControls.h"


class CVarInfo
{
public:
	CVarInfo()
	{
		m_pFormWidth = nullptr;
		m_pFormHeight = nullptr;
		m_pFormXPos = nullptr;
		m_pFormYPos = nullptr;
		m_pFormMax = nullptr;

		m_bSaveMax = false;
		m_bPosSaveEnabled = false;
	}

	~CVarInfo()
	{
	}

	gcRefPtr<CVar> getCVar(std::string name, std::string def)
	{
		auto cvar = GetCVarManager()->findCVar(name.c_str());

		if (!cvar)
			cvar = gcRefPtr<CVar>::create(name.c_str(), def.c_str(), CFLAG_WINUSER);

		return cvar;
	}

	void onResize(int w, int h)
	{
		if (!m_bPosSaveEnabled)
			return;

		if (m_pFormWidth)
			m_pFormWidth->setValue(w);

		if (m_pFormHeight)
			m_pFormHeight->setValue(h);
	}

	void onMove(int x, int y)
	{
		if (!m_bPosSaveEnabled)
			return;

		if (m_pFormXPos)
			m_pFormXPos->setValue(x);

		if (m_pFormYPos)
			m_pFormYPos->setValue(y);
	}

	void onWindowClose(bool isMaxed)
	{
		if (!m_bPosSaveEnabled || !m_bSaveMax)
			return;

		if (m_pFormMax)
			m_pFormMax->setValue(isMaxed);
	}

	void setupPositionSave(const char* name, bool saveMax, int defW, int defH)
	{
		m_bSaveMax = saveMax;

		m_pFormWidth	= getCVar(gcString("gc_{0}_w", name), gcString("{0}", defW));
		m_pFormHeight	= getCVar(gcString("gc_{0}_h", name), gcString("{0}", defH));
		m_pFormXPos		= getCVar(gcString("gc_{0}_x", name), "-1");
		m_pFormYPos		= getCVar(gcString("gc_{0}_y", name), "-1");

		if (saveMax)
			m_pFormMax	= getCVar(gcString("gc_{0}_max", name), "0");
	}

	gcRefPtr<CVar> m_pFormWidth;
	gcRefPtr<CVar> m_pFormHeight;
	gcRefPtr<CVar> m_pFormXPos;
	gcRefPtr<CVar> m_pFormYPos;
	gcRefPtr<CVar> m_pFormMax;

	bool m_bSaveMax;
	bool m_bPosSaveEnabled;
};

BEGIN_EVENT_TABLE( gcFrame, gcCustomFrame<wxFrame> )
	EVT_IDLE( gcFrame::onIdle )
END_EVENT_TABLE()

gcFrame::gcFrame()
	: wxGuiDelegateImplementation<gcCustomFrame<wxFrame> >(nullptr, wxID_ANY,  wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE)
{
	initGCFrame(true, 0);
}

gcFrame::gcFrame(wxWindow *parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style, bool delayLoad)
	: wxGuiDelegateImplementation<gcCustomFrame<wxFrame>>(parent, id, title, pos, size, style)
{
	initGCFrame(delayLoad, style);
}

gcFrame::~gcFrame()
{
	safe_delete(m_pCVarInfo);
}

void gcFrame::initGCFrame(bool delayLoad, long style)
{
	m_bEnableIdealSize = false;

	WINDOW_REG();

	m_pCVarInfo = nullptr;

	if (!delayLoad)
		loadFrame(style);
}

void gcFrame::setIdealSize(int width, int height)
{
	SetSize(width, height);

#ifdef NIX
	if (width == -1)
		width = GetSize().GetWidth();

	if (height == -1)
		height = GetSize().GetHeight();
#endif

	m_IdealSize = wxSize(width, height);
	m_bEnableIdealSize = true;
}

void gcFrame::onIdle( wxIdleEvent& event )
{
	if (m_bEnableIdealSize && !(this->GetWindowStyle() & wxRESIZE_BORDER))
	{
		if (GetSize() != m_IdealSize)
			SetSize(m_IdealSize);
	}

	event.Skip();
}


void gcFrame::setProgressState(PROGSTATE state)
{
#ifdef WIN32
	if (!g_pITBL3)
		return;

	switch (state)
	{
		case P_NONE:	g_pITBL3->SetProgressState((HWND)this->GetHWND(), TBPF_NOPROGRESS); break;
		case P_NORMAL:	g_pITBL3->SetProgressState((HWND)this->GetHWND(), TBPF_NORMAL); break;
		case P_ERROR:	g_pITBL3->SetProgressState((HWND)this->GetHWND(), TBPF_ERROR);	break;
		case P_PAUSED:	g_pITBL3->SetProgressState((HWND)this->GetHWND(), TBPF_PAUSED); break;
	}
#endif
}

void gcFrame::setProgress(uint8 prog)
{
#ifdef WIN32
	if (!g_pITBL3)
		return;

	g_pITBL3->SetProgressValue((HWND)this->GetHWND(), prog, 100);
#endif
}

void gcFrame::setupPositionSave(const char* name, bool saveMax, int defWidth, int defHeight)
{
	if (m_pCVarInfo)
		return;

	if (defWidth == -1)
		defWidth = GetDefaultSize().GetWidth();

	if (defHeight == -1)
		defHeight = GetDefaultSize().GetHeight();

	m_pCVarInfo = new CVarInfo();
	m_pCVarInfo->setupPositionSave(name, saveMax, defWidth, defHeight);

	if (saveMax)
	{
		//Need to do this as at this stage inheritance may not be full formed and Maximize doesnt work correctly.
		onFormMaxEvent += guiDelegate(this, &gcFrame::onFormMax);
		onFormMaxEvent();
	}
}

void gcFrame::onFormMax()
{
	if (m_pCVarInfo && m_pCVarInfo->m_pFormMax)
		Maximize(m_pCVarInfo->m_pFormMax->getBool());
}

void gcFrame::enablePositionSave(bool state)
{
	if (!m_pCVarInfo)
		return;

	if (m_pCVarInfo->m_bPosSaveEnabled == state)
		return;

	if (state)
	{
		Bind(wxEVT_SIZE, &gcFrame::onResize, this);
		Bind(wxEVT_MOVE, &gcFrame::onMove, this);
		Bind(wxEVT_CLOSE_WINDOW, &gcFrame::onWindowClose, this);
	}
	else
	{
		Unbind(wxEVT_SIZE, &gcFrame::onResize, this);
		Unbind(wxEVT_MOVE, &gcFrame::onMove, this);
		Unbind(wxEVT_CLOSE_WINDOW, &gcFrame::onWindowClose, this);
	}

	m_pCVarInfo->m_bPosSaveEnabled = state;
}

bool gcFrame::loadSavedWindowPos()
{
	if (!m_pCVarInfo)
		return false;

	int x = -1;
	int y = -1;
	int w = -1;
	int h = -1;

	if (m_pCVarInfo->m_pFormWidth)
		w = m_pCVarInfo->m_pFormWidth->getInt();

	if (m_pCVarInfo->m_pFormHeight)
		h = m_pCVarInfo->m_pFormHeight->getInt();

	if (m_pCVarInfo->m_pFormXPos)
		x = m_pCVarInfo->m_pFormXPos->getInt();

	if (m_pCVarInfo->m_pFormYPos)
		y = m_pCVarInfo->m_pFormYPos->getInt();

	return setSavedWindowPos(x, y, w, h);
}

void gcFrame::onResize(wxSizeEvent &event)
{
	if (m_pCVarInfo && !IsMaximized())
		m_pCVarInfo->onResize(GetSize().GetWidth(), GetSize().GetHeight());

	event.Skip();
}

void gcFrame::onMove(wxMoveEvent  &event)
{
	if (m_pCVarInfo && !IsMaximized())
		m_pCVarInfo->onMove(GetPosition().x, GetPosition().y);

	event.Skip();
}

void gcFrame::onWindowClose(wxCloseEvent& event)
{
	if (m_pCVarInfo)
		m_pCVarInfo->onWindowClose(IsMaximized());

	event.Skip();
}

bool gcFrame::setMessageBox(wxWindow *pDialog)
{
	if ((!pDialog && !m_pMessageBox) || (pDialog && m_pMessageBox))
	{
		gcAssert(false);
		return false;
	}

	m_pMessageBox = pDialog;

	if (!pDialog && m_bPendingClose)
		Close();

	return true;
}

bool gcFrame::hasMessageBox() const
{
	return !!m_pMessageBox;
}

bool gcFrame::checkAndSetPendingClose()
{
	if (!m_pMessageBox)
		return false;

	m_bPendingClose = true;
	return true;
}
