// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/


/*
1.1 Windows

CFrame is the main parent window. Inside CFrame there is m_Panel which is the
parent for the rendering window (when we render to the main window). In Windows
the rendering window is created by giving CreateWindow() m_Panel->GetHandle()
as parent window and creating a new child window to m_Panel. The new child
window handle that is returned by CreateWindow() can be accessed from
Core::GetWindowHandle().
*/


#include "Setup.h" // Common

#include "NetWindow.h"
#include "Common.h" // Common
#include "FileUtil.h"
#include "FileSearch.h"
#include "Timer.h"
#include "VideoBackendBase.h"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

#include "Globals.h" // Local
#include "Frame.h"
#include "ConfigMain.h"
#include "MemcardManager.h"
#include "CheatsWindow.h"
#include "AboutDolphin.h"
#include "GameListCtrl.h"
#include "BootManager.h"
#include "LogWindow.h"
#include "LogConfigWindow.h"
#include "FifoPlayerDlg.h"
#include "WxUtils.h"

#include "ConfigManager.h" // Core
#include "Core.h"
#include "Movie.h"
#include "HW/CPU.h"
#include "PowerPC/PowerPC.h"
#include "HW/DVDInterface.h"
#include "HW/ProcessorInterface.h"
#include "HW/GCPad.h"
#include "HW/Wiimote.h"
#include "IPC_HLE/WII_IPC_HLE_Device_usb.h"
//#include "IPC_HLE/WII_IPC_HLE_Device_FileIO.h"
#include "State.h"
#include "VolumeHandler.h"
#include "NANDContentLoader.h"
#include "WXInputBase.h"
#include "WiimoteConfigDiag.h"
#include "InputConfigDiag.h"
#include "HotkeyDlg.h"
#include "TASInputDlg.h"

#include <wx/datetime.h> // wxWidgets


// Resources
extern "C" {
#include "../resources/Dolphin.c" // Dolphin icon
#include "../resources/Boomy.h" // Theme packages
#include "../resources/Vista.h"
#include "../resources/X-Plastik.h"
#include "../resources/KDE.h"
};

// Create menu items
// ---------------------
void CFrame::CreateMenu()
{
	if (GetMenuBar()) GetMenuBar()->Destroy();

	wxMenuBar *m_MenuBar = new wxMenuBar();

	// file menu
	wxMenu* fileMenu = new wxMenu;
	fileMenu->Append(wxID_OPEN, GetMenuLabel(HK_OPEN));
	fileMenu->Append(IDM_CHANGEDISC, GetMenuLabel(HK_CHANGE_DISC));

	wxMenu *externalDrive = new wxMenu;
	fileMenu->Append(IDM_DRIVES, _("&Boot from DVD Drive..."), externalDrive);
	
	drives = cdio_get_devices();
	// Windows Limitation of 24 character drives
	for (unsigned int i = 0; i < drives.size() && i < 24; i++) {
		externalDrive->Append(IDM_DRIVE1 + i, wxString::FromAscii(drives[i].c_str()));
	}

	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_REFRESH, GetMenuLabel(HK_REFRESH_LIST));
	fileMenu->AppendSeparator();
	fileMenu->Append(IDM_BROWSE, _("&Browse for ISOs..."));
	fileMenu->AppendSeparator();
	fileMenu->Append(IDM_RESTART, UseDebugger ? _T("Regular mode") : _T("Debugging mode"));
	fileMenu->AppendSeparator();
	fileMenu->Append(wxID_EXIT, _("E&xit") + wxString(wxT("\tAlt+F4")));
	m_MenuBar->Append(fileMenu, _("&File"));

	// Emulation menu
	wxMenu* emulationMenu = new wxMenu;
	emulationMenu->Append(IDM_PLAY, GetMenuLabel(HK_PLAY_PAUSE));
	emulationMenu->Append(IDM_STOP, GetMenuLabel(HK_STOP));
	emulationMenu->Append(IDM_RESET, GetMenuLabel(HK_RESET));
	emulationMenu->AppendSeparator();
	emulationMenu->Append(IDM_TOGGLE_FULLSCREEN, GetMenuLabel(HK_FULLSCREEN));
	emulationMenu->AppendSeparator();
	emulationMenu->Append(IDM_RECORD, GetMenuLabel(HK_START_RECORDING));
	emulationMenu->Append(IDM_PLAYRECORD, GetMenuLabel(HK_PLAY_RECORDING));
	emulationMenu->Append(IDM_RECORDEXPORT, GetMenuLabel(HK_EXPORT_RECORDING));
	emulationMenu->Append(IDM_RECORDREADONLY, GetMenuLabel(HK_READ_ONLY_MODE), wxEmptyString, wxITEM_CHECK);
	emulationMenu->Append(IDM_TASINPUT, _("TAS Input"));
	emulationMenu->Check(IDM_RECORDREADONLY, true);
	emulationMenu->AppendSeparator();
	
	emulationMenu->Append(IDM_FRAMESTEP, GetMenuLabel(HK_FRAME_ADVANCE), wxEmptyString, wxITEM_CHECK);

	wxMenu *skippingMenu = new wxMenu;
	emulationMenu->AppendSubMenu(skippingMenu, _("Frame S&kipping"));
	for(int i = 0; i < 10; i++)
		skippingMenu->Append(IDM_FRAMESKIP0 + i, wxString::Format(wxT("%i"), i), wxEmptyString, wxITEM_RADIO);

	emulationMenu->AppendSeparator();
	emulationMenu->Append(IDM_SCREENSHOT, GetMenuLabel(HK_SCREENSHOT));

	emulationMenu->AppendSeparator();
	wxMenu *saveMenu = new wxMenu;
	wxMenu *loadMenu = new wxMenu;
	emulationMenu->Append(IDM_LOADSTATE, _("&Load State"), loadMenu);
	emulationMenu->Append(IDM_SAVESTATE, _("Sa&ve State"), saveMenu);

	saveMenu->Append(IDM_SAVESTATEFILE, _("Save State..."));
	loadMenu->Append(IDM_UNDOSAVESTATE, _("Last Overwritten State") + wxString(wxT("\tShift+F12")));
	saveMenu->AppendSeparator();

	loadMenu->Append(IDM_LOADSTATEFILE, _("Load State..."));

	// Reserve F11 for the "step into" function in the debugger
	if (g_pCodeWindow)
		loadMenu->Append(IDM_LOADLASTSTATE, _("Last Saved State"));
	else
		loadMenu->Append(IDM_LOADLASTSTATE, _("Last Saved State") + wxString(wxT("\tF11")));
	
	loadMenu->Append(IDM_UNDOLOADSTATE, _("Undo Load State") + wxString(wxT("\tF12")));
	loadMenu->AppendSeparator();

	for (int i = 1; i <= 8; i++) {
		loadMenu->Append(IDM_LOADSLOT1 + i - 1, GetMenuLabel(HK_LOAD_STATE_SLOT_1 + i - 1));
		saveMenu->Append(IDM_SAVESLOT1 + i - 1, GetMenuLabel(HK_SAVE_STATE_SLOT_1 + i - 1));
	}
	m_MenuBar->Append(emulationMenu, _("&Emulation"));

	// Options menu
	wxMenu* pOptionsMenu = new wxMenu;
	pOptionsMenu->Append(wxID_PREFERENCES, _("Co&nfigure..."));
	pOptionsMenu->AppendSeparator();
	pOptionsMenu->Append(IDM_CONFIG_GFX_BACKEND, _("&Graphics Settings"));
	pOptionsMenu->Append(IDM_CONFIG_DSP_EMULATOR, _("&DSP Settings"));
	pOptionsMenu->Append(IDM_CONFIG_PAD_PLUGIN, _("Gamecube &Pad Settings"));
	pOptionsMenu->Append(IDM_CONFIG_WIIMOTE_PLUGIN, _("&Wiimote Settings"));
	pOptionsMenu->Append(IDM_CONFIG_HOTKEYS, _("&Hotkey Settings"));
	if (g_pCodeWindow)
	{
		pOptionsMenu->AppendSeparator();
		g_pCodeWindow->CreateMenuOptions(pOptionsMenu);
	}
	m_MenuBar->Append(pOptionsMenu, _("&Options"));

	// Tools menu
	wxMenu* toolsMenu = new wxMenu;
	toolsMenu->Append(IDM_MEMCARD, _("&Memcard Manager (GC)"));
	toolsMenu->Append(IDM_IMPORTSAVE, _("Wii Save Import"));
	toolsMenu->Append(IDM_CHEATS, _("&Cheats Manager"));

	toolsMenu->Append(IDM_NETPLAY, _("Start &NetPlay"));

	toolsMenu->Append(IDM_MENU_INSTALLWAD, _("Install WAD"));
	UpdateWiiMenuChoice(toolsMenu->Append(IDM_LOAD_WII_MENU, wxT("Dummy string to keep wxw happy")));

	toolsMenu->Append(IDM_FIFOPLAYER, _("Fifo Player"));

	toolsMenu->AppendSeparator();
	toolsMenu->AppendCheckItem(IDM_CONNECT_WIIMOTE1, GetMenuLabel(HK_WIIMOTE1_CONNECT));
	toolsMenu->AppendCheckItem(IDM_CONNECT_WIIMOTE2, GetMenuLabel(HK_WIIMOTE2_CONNECT));
	toolsMenu->AppendCheckItem(IDM_CONNECT_WIIMOTE3, GetMenuLabel(HK_WIIMOTE3_CONNECT));
	toolsMenu->AppendCheckItem(IDM_CONNECT_WIIMOTE4, GetMenuLabel(HK_WIIMOTE4_CONNECT));

	m_MenuBar->Append(toolsMenu, _("&Tools"));

	wxMenu* viewMenu = new wxMenu;
	viewMenu->AppendCheckItem(IDM_TOGGLE_TOOLBAR, _("Show &Toolbar"));
	viewMenu->Check(IDM_TOGGLE_TOOLBAR, SConfig::GetInstance().m_InterfaceToolbar);
	viewMenu->AppendCheckItem(IDM_TOGGLE_STATUSBAR, _("Show &Statusbar"));
	viewMenu->Check(IDM_TOGGLE_STATUSBAR, SConfig::GetInstance().m_InterfaceStatusbar);
	viewMenu->AppendSeparator();
	viewMenu->AppendCheckItem(IDM_LOGWINDOW, _("Show &Log"));
	viewMenu->AppendCheckItem(IDM_LOGCONFIGWINDOW, _("Show Log &Configuration"));
	viewMenu->AppendCheckItem(IDM_CONSOLEWINDOW, _("Show &Console"));
	viewMenu->AppendSeparator();

#ifndef _WIN32
	viewMenu->Enable(IDM_CONSOLEWINDOW, false);
#endif

	if (g_pCodeWindow)
	{
		viewMenu->Check(IDM_LOGWINDOW, g_pCodeWindow->bShowOnStart[0]);
		viewMenu->Check(IDM_CONSOLEWINDOW, g_pCodeWindow->bShowOnStart[1]);

		const wxString MenuText[] = {
			wxTRANSLATE("&Registers"),
			wxTRANSLATE("&Breakpoints"),
			wxTRANSLATE("&Memory"),
			wxTRANSLATE("&JIT"),
			wxTRANSLATE("&Sound"),
			wxTRANSLATE("&Video")
		};

		for (int i = IDM_REGISTERWINDOW; i <= IDM_VIDEOWINDOW; i++)
		{
			viewMenu->AppendCheckItem(i, wxGetTranslation(MenuText[i - IDM_REGISTERWINDOW]));
			viewMenu->Check(i, g_pCodeWindow->bShowOnStart[i - IDM_LOGWINDOW]);
		}

		viewMenu->AppendSeparator();
	}
	else
	{
		viewMenu->Check(IDM_LOGWINDOW, SConfig::GetInstance().m_InterfaceLogWindow);
		viewMenu->Check(IDM_LOGCONFIGWINDOW, SConfig::GetInstance().m_InterfaceLogConfigWindow);
		viewMenu->Check(IDM_CONSOLEWINDOW, SConfig::GetInstance().m_InterfaceConsole);
	}

	wxMenu *platformMenu = new wxMenu;
	viewMenu->AppendSubMenu(platformMenu, _("Show Platforms"));
	platformMenu->AppendCheckItem(IDM_LISTWII, _("Show Wii"));
	platformMenu->Check(IDM_LISTWII, SConfig::GetInstance().m_ListWii);
	platformMenu->AppendCheckItem(IDM_LISTGC, _("Show GameCube"));
	platformMenu->Check(IDM_LISTGC, SConfig::GetInstance().m_ListGC);
	platformMenu->AppendCheckItem(IDM_LISTWAD, _("Show Wad"));
	platformMenu->Check(IDM_LISTWAD, SConfig::GetInstance().m_ListWad);

	wxMenu *regionMenu = new wxMenu;
	viewMenu->AppendSubMenu(regionMenu, _("Show Regions"));
	regionMenu->AppendCheckItem(IDM_LISTJAP, _("Show JAP"));
	regionMenu->Check(IDM_LISTJAP, SConfig::GetInstance().m_ListJap);
	regionMenu->AppendCheckItem(IDM_LISTPAL, _("Show PAL"));
	regionMenu->Check(IDM_LISTPAL, SConfig::GetInstance().m_ListPal);
	regionMenu->AppendCheckItem(IDM_LISTUSA, _("Show USA"));
	regionMenu->Check(IDM_LISTUSA, SConfig::GetInstance().m_ListUsa);
	regionMenu->AppendSeparator();
	regionMenu->AppendCheckItem(IDM_LISTFRANCE, _("Show France"));
	regionMenu->Check(IDM_LISTFRANCE, SConfig::GetInstance().m_ListFrance);
	regionMenu->AppendCheckItem(IDM_LISTITALY, _("Show Italy"));
	regionMenu->Check(IDM_LISTITALY, SConfig::GetInstance().m_ListItaly);
	regionMenu->AppendCheckItem(IDM_LISTKOREA, _("Show Korea"));
	regionMenu->Check(IDM_LISTKOREA, SConfig::GetInstance().m_ListKorea);
	regionMenu->AppendCheckItem(IDM_LISTTAIWAN, _("Show Taiwan"));
	regionMenu->Check(IDM_LISTTAIWAN, SConfig::GetInstance().m_ListTaiwan);
	regionMenu->AppendCheckItem(IDM_LIST_UNK, _("Show unknown"));
	regionMenu->Check(IDM_LIST_UNK, SConfig::GetInstance().m_ListUnknown);
	viewMenu->AppendCheckItem(IDM_LISTDRIVES, _("Show Drives"));
	viewMenu->Check(IDM_LISTDRIVES, SConfig::GetInstance().m_ListDrives);
	viewMenu->Append(IDM_PURGECACHE, _("Purge Cache"));
	m_MenuBar->Append(viewMenu, _("&View"));

	if (g_pCodeWindow) g_pCodeWindow->CreateMenu(SConfig::GetInstance().m_LocalCoreStartupParameter, m_MenuBar);

	// Help menu
	wxMenu* helpMenu = new wxMenu;
	// Re-enable when there's something useful to display */
	// helpMenu->Append(wxID_HELP, _("&Help"));
	helpMenu->Append(IDM_HELPWEBSITE, _("Dolphin &Web Site"));
	helpMenu->Append(IDM_HELPGOOGLECODE, _("Dolphin at &Google Code"));
	helpMenu->AppendSeparator();
	helpMenu->Append(wxID_ABOUT, _("&About..."));
	m_MenuBar->Append(helpMenu, _("&Help"));

	// Associate the menu bar with the frame
	SetMenuBar(m_MenuBar);
}

wxString CFrame::GetMenuLabel(int Id)
{
	int hotkey = SConfig::GetInstance().\
		m_LocalCoreStartupParameter.iHotkey[Id];
	int hotkeymodifier = SConfig::GetInstance().\
		m_LocalCoreStartupParameter.iHotkeyModifier[Id];
	wxString Hotkey, Label, Modifier;

	switch (Id)
	{
		case HK_OPEN:
			Label = _("&Open...");
			break;
		case HK_CHANGE_DISC:
			Label = _("Change &Disc...");
			break;
		case HK_REFRESH_LIST:
			Label = _("&Refresh List");
			break;

		case HK_PLAY_PAUSE:
			if (Core::GetState() == Core::CORE_RUN)
				Label = _("&Pause");
			else
				Label = _("&Play");
			break;
		case HK_STOP:
			Label = _("&Stop");
			break;
		case HK_RESET:
			Label = _("&Reset");
			break;
		case HK_FRAME_ADVANCE:
			Label = _("&Frame Advance");
			break;

		case HK_START_RECORDING:
			Label = _("Start Re&cording");
			break;
		case HK_PLAY_RECORDING:
			Label = _("P&lay Recording...");
			break;
		case HK_EXPORT_RECORDING:
			Label = _("Export Recording...");
			break;
		case HK_READ_ONLY_MODE:
			Label = _("&Read-only mode");
			break;

		case HK_FULLSCREEN:
			Label = _("&Fullscreen");
			break;
		case HK_SCREENSHOT:
			Label = _("Take Screenshot");
			break;

		case HK_WIIMOTE1_CONNECT:
		case HK_WIIMOTE2_CONNECT:
		case HK_WIIMOTE3_CONNECT:
		case HK_WIIMOTE4_CONNECT:
			Label = wxString::Format(_("Connect Wiimote %i"),
					Id - HK_WIIMOTE1_CONNECT + 1);
			break;

		case HK_LOAD_STATE_SLOT_1:
		case HK_LOAD_STATE_SLOT_2:
		case HK_LOAD_STATE_SLOT_3:
		case HK_LOAD_STATE_SLOT_4:
		case HK_LOAD_STATE_SLOT_5:
		case HK_LOAD_STATE_SLOT_6:
		case HK_LOAD_STATE_SLOT_7:
		case HK_LOAD_STATE_SLOT_8:
			Label = wxString::Format(_("Slot %i"), 
					Id - HK_LOAD_STATE_SLOT_1 + 1);
			break;

		case HK_SAVE_STATE_SLOT_1:
		case HK_SAVE_STATE_SLOT_2:
		case HK_SAVE_STATE_SLOT_3:
		case HK_SAVE_STATE_SLOT_4:
		case HK_SAVE_STATE_SLOT_5:
		case HK_SAVE_STATE_SLOT_6:
		case HK_SAVE_STATE_SLOT_7:
		case HK_SAVE_STATE_SLOT_8:
			Label = wxString::Format(_("Slot %i"), 
					Id - HK_SAVE_STATE_SLOT_1 + 1);
			break;

		default:
			Label = wxString::Format(_("Undefined %i"), Id);
	}

	// wxWidgets only accepts Ctrl/Alt/Shift as menu accelerator
	// modifiers. On OS X, "Ctrl+" is mapped to the Command key.
#ifdef __APPLE__
	if (hotkeymodifier & wxMOD_CMD)
		hotkeymodifier |= wxMOD_CONTROL;
#endif
	hotkeymodifier &= wxMOD_CONTROL | wxMOD_ALT | wxMOD_SHIFT;
	
	Modifier = InputCommon::WXKeymodToString(hotkeymodifier);
	Hotkey = InputCommon::WXKeyToString(hotkey);
	if (Modifier.Len() + Hotkey.Len() > 0)
		Label += '\t';

	return Label + Modifier + Hotkey;
}


// Create toolbar items
// ---------------------
void CFrame::PopulateToolbar(wxAuiToolBar* ToolBar)
{
	int w = m_Bitmaps[Toolbar_FileOpen].GetWidth(),
		h = m_Bitmaps[Toolbar_FileOpen].GetHeight();
		ToolBar->SetToolBitmapSize(wxSize(w, h));
		

	ToolBar->AddTool(wxID_OPEN,    _("Open"),    m_Bitmaps[Toolbar_FileOpen], _("Open file..."));
	ToolBar->AddTool(wxID_REFRESH, _("Refresh"), m_Bitmaps[Toolbar_Refresh], _("Refresh game list"));
	ToolBar->AddTool(IDM_BROWSE, _("Browse"),   m_Bitmaps[Toolbar_Browse], _("Browse for an ISO directory..."));
	ToolBar->AddSeparator();
	ToolBar->AddTool(IDM_PLAY, _("Play"),   m_Bitmaps[Toolbar_Play], _("Play"));
	ToolBar->AddTool(IDM_STOP, _("Stop"),   m_Bitmaps[Toolbar_Stop], _("Stop"));
	ToolBar->AddTool(IDM_TOGGLE_FULLSCREEN, _("FullScr"),  m_Bitmaps[Toolbar_FullScreen], _("Toggle Fullscreen"));
	ToolBar->AddTool(IDM_SCREENSHOT, _("ScrShot"),   m_Bitmaps[Toolbar_FullScreen], _("Take Screenshot"));
	ToolBar->AddSeparator();
	ToolBar->AddTool(wxID_PREFERENCES, _("Config"), m_Bitmaps[Toolbar_ConfigMain], _("Configure..."));
	ToolBar->AddTool(IDM_CONFIG_GFX_BACKEND, _("Graphics"),  m_Bitmaps[Toolbar_ConfigGFX], _("Graphics settings"));
	ToolBar->AddTool(IDM_CONFIG_DSP_EMULATOR, _("DSP"),  m_Bitmaps[Toolbar_ConfigDSP], _("DSP settings"));
	ToolBar->AddTool(IDM_CONFIG_PAD_PLUGIN, _("GCPad"),  m_Bitmaps[Toolbar_ConfigPAD], _("Gamecube Pad settings"));
	ToolBar->AddTool(IDM_CONFIG_WIIMOTE_PLUGIN, _("Wiimote"),  m_Bitmaps[Toolbar_Wiimote], _("Wiimote settings"));

	// after adding the buttons to the toolbar, must call Realize() to reflect
	// the changes
	ToolBar->Realize();
}

void CFrame::PopulateToolbarAui(wxAuiToolBar* ToolBar)
{
	int w = m_Bitmaps[Toolbar_FileOpen].GetWidth(),
		h = m_Bitmaps[Toolbar_FileOpen].GetHeight();
	ToolBar->SetToolBitmapSize(wxSize(w, h));

	ToolBar->AddTool(IDM_SAVE_PERSPECTIVE,	_("Save"),	g_pCodeWindow->m_Bitmaps[Toolbar_GotoPC], _("Save current perspective"));
	ToolBar->AddTool(IDM_EDIT_PERSPECTIVES,	_("Edit"),	g_pCodeWindow->m_Bitmaps[Toolbar_GotoPC], _("Edit current perspective"));

	ToolBar->SetToolDropDown(IDM_SAVE_PERSPECTIVE, true);
	ToolBar->SetToolDropDown(IDM_EDIT_PERSPECTIVES, true);

	ToolBar->Realize();
}


// Delete and recreate the toolbar
void CFrame::RecreateToolbar()
{
	if (m_ToolBar)
	{
		m_Mgr->DetachPane(m_ToolBar);
		m_ToolBar->Destroy();
	}

	long TOOLBAR_STYLE = wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_TEXT  /*wxAUI_TB_OVERFLOW overflow visible*/;
	m_ToolBar = new wxAuiToolBar(this, ID_TOOLBAR, wxDefaultPosition, wxDefaultSize, TOOLBAR_STYLE);

	PopulateToolbar(m_ToolBar);
	
	m_Mgr->AddPane(m_ToolBar, wxAuiPaneInfo().
				Name(wxT("TBMain")).Caption(wxT("TBMain")).
				ToolbarPane().Top().
				LeftDockable(false).RightDockable(false).Floatable(false));

	if (g_pCodeWindow && !m_ToolBarDebug)
	{
		m_ToolBarDebug = new wxAuiToolBar(this, ID_TOOLBAR_DEBUG, wxDefaultPosition, wxDefaultSize, TOOLBAR_STYLE);
		g_pCodeWindow->PopulateToolbar(m_ToolBarDebug);
		
		m_Mgr->AddPane(m_ToolBarDebug, wxAuiPaneInfo().
				Name(wxT("TBDebug")).Caption(wxT("TBDebug")).
				ToolbarPane().Top().
				LeftDockable(false).RightDockable(false).Floatable(false));

		m_ToolBarAui = new wxAuiToolBar(this, ID_TOOLBAR_AUI, wxDefaultPosition, wxDefaultSize, TOOLBAR_STYLE);
		PopulateToolbarAui(m_ToolBarAui);
		m_Mgr->AddPane(m_ToolBarAui, wxAuiPaneInfo().
				Name(wxT("TBAui")).Caption(wxT("TBAui")).
				ToolbarPane().Top().
				LeftDockable(false).RightDockable(false).Floatable(false));
	}

	UpdateGUI();
}

void CFrame::InitBitmaps()
{
	// Get selected theme
	int Theme = SConfig::GetInstance().m_LocalCoreStartupParameter.iTheme;

	// Save memory by only having one set of bitmaps loaded at any time. I mean, they are still
	// in the exe, which is in memory, but at least we wont make another copy of all of them. 
	switch (Theme)
	{
	case BOOMY:
	{
		// These are stored as 48x48
		m_Bitmaps[Toolbar_FileOpen]		= wxGetBitmapFromMemory(toolbar_file_open_png);
		m_Bitmaps[Toolbar_Refresh]		= wxGetBitmapFromMemory(toolbar_refresh_png);
		m_Bitmaps[Toolbar_Browse]		= wxGetBitmapFromMemory(toolbar_browse_png);
		m_Bitmaps[Toolbar_Play]			= wxGetBitmapFromMemory(toolbar_play_png);
		m_Bitmaps[Toolbar_Stop]			= wxGetBitmapFromMemory(toolbar_stop_png);
		m_Bitmaps[Toolbar_Pause]		= wxGetBitmapFromMemory(toolbar_pause_png);
		m_Bitmaps[Toolbar_ConfigMain]	= wxGetBitmapFromMemory(toolbar_plugin_options_png);
		m_Bitmaps[Toolbar_ConfigGFX]	= wxGetBitmapFromMemory(toolbar_plugin_gfx_png);
		m_Bitmaps[Toolbar_ConfigDSP]	= wxGetBitmapFromMemory(toolbar_plugin_dsp_png);
		m_Bitmaps[Toolbar_ConfigPAD]	= wxGetBitmapFromMemory(toolbar_plugin_pad_png);
		m_Bitmaps[Toolbar_Wiimote]		= wxGetBitmapFromMemory(toolbar_plugin_wiimote_png);
		m_Bitmaps[Toolbar_Screenshot]	= wxGetBitmapFromMemory(toolbar_fullscreen_png);
		m_Bitmaps[Toolbar_FullScreen]	= wxGetBitmapFromMemory(toolbar_fullscreen_png);
		m_Bitmaps[Toolbar_Help]			= wxGetBitmapFromMemory(toolbar_help_png);

		// Scale the 48x48 bitmaps to 24x24
		for (size_t n = Toolbar_FileOpen; n < EToolbar_Max; n++)
		{
			m_Bitmaps[n] = wxBitmap(m_Bitmaps[n].ConvertToImage().Scale(24, 24));
		}
	}
	break;

	case VISTA:
	{
		// These are stored as 24x24 and need no scaling
		m_Bitmaps[Toolbar_FileOpen]		= wxGetBitmapFromMemory(Toolbar_Open1_png);
		m_Bitmaps[Toolbar_Refresh]		= wxGetBitmapFromMemory(Toolbar_Refresh1_png);
		m_Bitmaps[Toolbar_Browse]		= wxGetBitmapFromMemory(Toolbar_Browse1_png);
		m_Bitmaps[Toolbar_Play]			= wxGetBitmapFromMemory(Toolbar_Play1_png);
		m_Bitmaps[Toolbar_Stop]			= wxGetBitmapFromMemory(Toolbar_Stop1_png);
		m_Bitmaps[Toolbar_Pause]		= wxGetBitmapFromMemory(Toolbar_Pause1_png);
		m_Bitmaps[Toolbar_ConfigMain]	= wxGetBitmapFromMemory(Toolbar_Options1_png);
		m_Bitmaps[Toolbar_ConfigGFX]	= wxGetBitmapFromMemory(Toolbar_Gfx1_png);
		m_Bitmaps[Toolbar_ConfigDSP]	= wxGetBitmapFromMemory(Toolbar_DSP1_png);
		m_Bitmaps[Toolbar_ConfigPAD]	= wxGetBitmapFromMemory(Toolbar_Pad1_png);
		m_Bitmaps[Toolbar_Wiimote]		= wxGetBitmapFromMemory(Toolbar_Wiimote1_png);
		m_Bitmaps[Toolbar_Screenshot]	= wxGetBitmapFromMemory(Toolbar_Fullscreen1_png);
		m_Bitmaps[Toolbar_FullScreen]	= wxGetBitmapFromMemory(Toolbar_Fullscreen1_png);
		m_Bitmaps[Toolbar_Help]			= wxGetBitmapFromMemory(Toolbar_Help1_png);
	}
	break;

	case XPLASTIK:
	{
		m_Bitmaps[Toolbar_FileOpen]		= wxGetBitmapFromMemory(Toolbar_Open2_png);
		m_Bitmaps[Toolbar_Refresh]		= wxGetBitmapFromMemory(Toolbar_Refresh2_png);
		m_Bitmaps[Toolbar_Browse]		= wxGetBitmapFromMemory(Toolbar_Browse2_png);
		m_Bitmaps[Toolbar_Play]			= wxGetBitmapFromMemory(Toolbar_Play2_png);
		m_Bitmaps[Toolbar_Stop]			= wxGetBitmapFromMemory(Toolbar_Stop2_png);
		m_Bitmaps[Toolbar_Pause]		= wxGetBitmapFromMemory(Toolbar_Pause2_png);
		m_Bitmaps[Toolbar_ConfigMain]	= wxGetBitmapFromMemory(Toolbar_Options2_png);
		m_Bitmaps[Toolbar_ConfigGFX]	= wxGetBitmapFromMemory(Toolbar_Gfx2_png);
		m_Bitmaps[Toolbar_ConfigDSP]	= wxGetBitmapFromMemory(Toolbar_DSP2_png);
		m_Bitmaps[Toolbar_ConfigPAD]	= wxGetBitmapFromMemory(Toolbar_Pad2_png);
		m_Bitmaps[Toolbar_Wiimote]		= wxGetBitmapFromMemory(Toolbar_Wiimote2_png);
		m_Bitmaps[Toolbar_Screenshot]	= wxGetBitmapFromMemory(Toolbar_Fullscreen2_png);
		m_Bitmaps[Toolbar_FullScreen]	= wxGetBitmapFromMemory(Toolbar_Fullscreen2_png);
		m_Bitmaps[Toolbar_Help]			= wxGetBitmapFromMemory(Toolbar_Help2_png);
	}
	break;

	case KDE:
	{
		m_Bitmaps[Toolbar_FileOpen]		= wxGetBitmapFromMemory(Toolbar_Open3_png);
		m_Bitmaps[Toolbar_Refresh]		= wxGetBitmapFromMemory(Toolbar_Refresh3_png);
		m_Bitmaps[Toolbar_Browse]		= wxGetBitmapFromMemory(Toolbar_Browse3_png);
		m_Bitmaps[Toolbar_Play]			= wxGetBitmapFromMemory(Toolbar_Play3_png);
		m_Bitmaps[Toolbar_Stop]			= wxGetBitmapFromMemory(Toolbar_Stop3_png);
		m_Bitmaps[Toolbar_Pause]		= wxGetBitmapFromMemory(Toolbar_Pause3_png);
		m_Bitmaps[Toolbar_ConfigMain]	= wxGetBitmapFromMemory(Toolbar_Options3_png);
		m_Bitmaps[Toolbar_ConfigGFX]	= wxGetBitmapFromMemory(Toolbar_Gfx3_png);
		m_Bitmaps[Toolbar_ConfigDSP]	= wxGetBitmapFromMemory(Toolbar_DSP3_png);
		m_Bitmaps[Toolbar_ConfigPAD]	= wxGetBitmapFromMemory(Toolbar_Pad3_png);
		m_Bitmaps[Toolbar_Wiimote]		= wxGetBitmapFromMemory(Toolbar_Wiimote3_png);
		m_Bitmaps[Toolbar_Screenshot]	= wxGetBitmapFromMemory(Toolbar_Fullscreen3_png);
		m_Bitmaps[Toolbar_FullScreen]	= wxGetBitmapFromMemory(Toolbar_Fullscreen3_png);
		m_Bitmaps[Toolbar_Help]			= wxGetBitmapFromMemory(Toolbar_Help3_png);
	}
	break;

	default: PanicAlertT("Theme selection went wrong");
	}

	// Update in case the bitmap has been updated
	if (m_ToolBar != NULL) RecreateToolbar();
}

// Menu items

// Start the game or change the disc.
// Boot priority:
// 1. Show the game list and boot the selected game.
// 2. Default ISO
// 3. Boot last selected game
void CFrame::BootGame(const std::string& filename)
{
	std::string bootfile = filename;
	SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

	if (Core::GetState() != Core::CORE_UNINITIALIZED)
		return;

	// Start filename if non empty.
	// Start the selected ISO, or try one of the saved paths.
	// If all that fails, ask to add a dir and don't boot
	if (bootfile.empty())
	{
		if (m_GameListCtrl->GetSelectedISO() != NULL)
		{
			if (m_GameListCtrl->GetSelectedISO()->IsValid())
				bootfile = m_GameListCtrl->GetSelectedISO()->GetFileName();
		}
		else if (!StartUp.m_strDefaultGCM.empty()
				&&	wxFileExists(wxString(StartUp.m_strDefaultGCM.c_str(), wxConvUTF8)))
			bootfile = StartUp.m_strDefaultGCM;
		else
		{
			if (!SConfig::GetInstance().m_LastFilename.empty()
					&& wxFileExists(wxString(SConfig::GetInstance().m_LastFilename.c_str(), wxConvUTF8)))
				bootfile = SConfig::GetInstance().m_LastFilename;
			else
			{
				m_GameListCtrl->BrowseForDirectory();
				return;
			}
		}
	}
	if (!bootfile.empty())
		StartGame(bootfile);
}

// Open file to boot
void CFrame::OnOpen(wxCommandEvent& WXUNUSED (event))
{
	DoOpen(true);
}

void CFrame::DoOpen(bool Boot)
{
	std::string currentDir = File::GetCurrentDir();

	wxString path = wxFileSelector(
			_("Select the file to load"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			_("All GC/Wii files (elf, dol, gcm, iso, ciso, gcz, wad)") +
			wxString::Format(wxT("|*.elf;*.dol;*.gcm;*.iso;*.ciso;*.gcz;*.wad;*.dff;*.tmd|%s"),
				wxGetTranslation(wxALL_FILES)),
			wxFD_OPEN | wxFD_FILE_MUST_EXIST,
			this);

	std::string currentDir2 = File::GetCurrentDir();

	if (currentDir != currentDir2)
	{
		PanicAlertT("Current dir changed from %s to %s after wxFileSelector!",
				currentDir.c_str(), currentDir2.c_str());
		File::SetCurrentDir(currentDir);
	}

	if (path.IsEmpty())
		return;

	// Should we boot a new game or just change the disc?
	if (Boot)
		BootGame(std::string(path.mb_str()));
	else
	{
		char newDiscpath[2048];
		strncpy(newDiscpath, path.mb_str(), strlen(path.mb_str())+1);
		DVDInterface::ChangeDisc(newDiscpath);
	}
}

void CFrame::OnRecordReadOnly(wxCommandEvent& event)
{
	Movie::SetReadOnly(event.IsChecked());
}

void CFrame::OnTASInput(wxCommandEvent& event)
{
	g_TASInputDlg->Show(true);
}

void CFrame::OnFrameStep(wxCommandEvent& event)
{
	Movie::SetFrameStepping(event.IsChecked());
}

void CFrame::OnChangeDisc(wxCommandEvent& WXUNUSED (event))
{
	DoOpen(false);
}

void CFrame::OnRecord(wxCommandEvent& WXUNUSED (event))
{
	int controllers = 0;
	
	if (Movie::IsReadOnly())
	{
		PanicAlertT("Cannot record movies in read-only mode.");
		return;
	}

	for (int i = 0; i < 4; i++) {
		if (SConfig::GetInstance().m_SIDevice[i] == SIDEVICE_GC_CONTROLLER)
			controllers |= (1 << i);

		if (g_wiimote_sources[i] != WIIMOTE_SRC_NONE)
			controllers |= (1 << (i + 4));
	}

	if(Movie::BeginRecordingInput(controllers))
		BootGame(std::string(""));
}

void CFrame::OnPlayRecording(wxCommandEvent& WXUNUSED (event))
{
	wxString path = wxFileSelector(
			_("Select The Recording File"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			_("Dolphin TAS Movies (*.dtm)") + 
				wxString::Format(wxT("|*.dtm|%s"), wxGetTranslation(wxALL_FILES)),
			wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
			this);

	if(path.IsEmpty())
		return;

	if(Movie::PlayInput(path.mb_str()))
		BootGame(std::string(""));
}

void CFrame::OnRecordExport(wxCommandEvent& WXUNUSED (event))
{
	DoRecordingSave();
}

void CFrame::OnPlay(wxCommandEvent& WXUNUSED (event))
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		// Core is initialized and emulator is running
		if (UseDebugger)
		{
			if (CCPU::IsStepping())
				CCPU::EnableStepping(false);
			else
				CCPU::EnableStepping(true);  // Break

			wxThread::Sleep(20);
			g_pCodeWindow->JumpToAddress(PC);
			g_pCodeWindow->Update();
			// Update toolbar with Play/Pause status
			UpdateGUI();
		}
		else
			DoPause();
	}
	else
		// Core is uninitialized, start the game
		BootGame(std::string(""));
}

void CFrame::OnRenderParentClose(wxCloseEvent& event)
{
	DoStop();
	if (Core::GetState() == Core::CORE_UNINITIALIZED)
		event.Skip();
}

void CFrame::OnRenderParentMove(wxMoveEvent& event)
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED &&
		!RendererIsFullscreen() && !m_RenderFrame->IsMaximized() && !m_RenderFrame->IsIconized())
	{
		SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos = m_RenderFrame->GetPosition().x;
		SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos = m_RenderFrame->GetPosition().y;
	}
	event.Skip();
}

void CFrame::OnRenderParentResize(wxSizeEvent& event)
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		int width, height;
		if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain &&
		    !RendererIsFullscreen() && !m_RenderFrame->IsMaximized() && !m_RenderFrame->IsIconized())
		{
			m_RenderFrame->GetClientSize(&width, &height);
			SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth = width;
			SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight = height;
		}
#if defined(HAVE_X11) && HAVE_X11
		int x, y;
		m_RenderParent->GetClientSize(&width, &height);
		m_RenderParent->GetPosition(&x, &y);
		X11Utils::SendClientEvent(X11Utils::XDisplayFromHandle(GetHandle()),
				"RESIZE", x, y, width, height);
#endif
		m_LogWindow->Refresh();
		m_LogWindow->Update();
	}
	event.Skip();
}

void CFrame::ToggleDisplayMode(bool bFullscreen)
{
#ifdef _WIN32
	if (bFullscreen)
	{
		DEVMODE dmScreenSettings;
		memset(&dmScreenSettings,0,sizeof(dmScreenSettings));
		dmScreenSettings.dmSize = sizeof(dmScreenSettings);
		sscanf(SConfig::GetInstance().m_LocalCoreStartupParameter.strFullscreenResolution.c_str(),
				"%dx%d", &dmScreenSettings.dmPelsWidth, &dmScreenSettings.dmPelsHeight);
		dmScreenSettings.dmBitsPerPel = 32;
		dmScreenSettings.dmFields = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;

		// Try To Set Selected Mode And Get Results.  NOTE: CDS_FULLSCREEN Gets Rid Of Start Bar.
		ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN);
	}
	else
	{
		// Change to default resolution
		ChangeDisplaySettings(NULL, CDS_FULLSCREEN);
	}
#elif defined(HAVE_XRANDR) && HAVE_XRANDR
	m_XRRConfig->ToggleDisplayMode(bFullscreen);
#elif defined __APPLE__
	if (!bFullscreen) {
		CGRestorePermanentDisplayConfiguration();
		CGDisplayRelease(CGMainDisplayID());
		return;
	}

	CFArrayRef modes = CGDisplayAvailableModes(CGMainDisplayID());
	for (CFIndex i = 0; i < CFArrayGetCount(modes); i++)
	{
		CFDictionaryRef mode;
		CFNumberRef ref;
		int x, y, w, h, d;

		sscanf(SConfig::GetInstance().m_LocalCoreStartupParameter.\
			strFullscreenResolution.c_str(), "%dx%d", &x, &y);

		mode = (CFDictionaryRef)CFArrayGetValueAtIndex(modes, i);
		ref = (CFNumberRef)CFDictionaryGetValue(mode, kCGDisplayWidth);
		CFNumberGetValue(ref, kCFNumberIntType, &w);
		ref = (CFNumberRef)CFDictionaryGetValue(mode, kCGDisplayHeight);
		CFNumberGetValue(ref, kCFNumberIntType, &h);
		ref = (CFNumberRef)CFDictionaryGetValue(mode,
			kCGDisplayBitsPerPixel);
		CFNumberGetValue(ref, kCFNumberIntType, &d);

		if (CFDictionaryContainsKey(mode, kCGDisplayModeIsStretched))
			continue;
		if (w != x || h != y || d != 32)
			continue;;

		CGDisplayCapture(CGMainDisplayID());
		CGDisplaySwitchToMode(CGMainDisplayID(), mode);
	}

#endif
}

// Prepare the GUI to start the game.
void CFrame::StartGame(const std::string& filename)
{
	m_bGameLoading = true;

	if (m_ToolBar)
		m_ToolBar->EnableTool(IDM_PLAY, false);
	GetMenuBar()->FindItem(IDM_PLAY)->Enable(false);

	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
	{
		// Game has been started, hide the game list
		m_GameListCtrl->Disable();
		m_GameListCtrl->Hide();

		m_RenderParent = m_Panel;
		m_RenderFrame = this;
	}
	else
	{
		wxPoint position(SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos,
				SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos);
#ifdef __APPLE__
		// On OS X, the render window's title bar is not visible,
		// and the window therefore not easily moved, when the
		// position is 0,0. Weed out the 0's from existing configs.
		if (position == wxPoint(0, 0))
			position = wxDefaultPosition;
#endif

		m_RenderFrame = new CRenderFrame((wxFrame*)this, wxID_ANY, _("Dolphin"), position);
		wxSize size(SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth,
				SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight);
		m_RenderFrame->SetClientSize(size.GetWidth(), size.GetHeight());
		m_RenderFrame->Connect(wxID_ANY, wxEVT_CLOSE_WINDOW,
				wxCloseEventHandler(CFrame::OnRenderParentClose),
				(wxObject*)0, this);
		m_RenderFrame->Connect(wxID_ANY, wxEVT_ACTIVATE,
				wxActivateEventHandler(CFrame::OnActive),
				(wxObject*)0, this);
		m_RenderFrame->Connect(wxID_ANY, wxEVT_MOVE,
				wxMoveEventHandler(CFrame::OnRenderParentMove),
				(wxObject*)0, this);
		m_RenderParent = new CPanel(m_RenderFrame, wxID_ANY);
		m_RenderFrame->Show();
	}

	wxBeginBusyCursor();

	DoFullscreen(SConfig::GetInstance().m_LocalCoreStartupParameter.bFullscreen);

	if (!BootManager::BootCore(filename))
	{
		DoFullscreen(false);
		// Destroy the renderer frame when not rendering to main
		if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
			m_RenderFrame->Destroy();
		m_RenderParent = NULL;
		m_bGameLoading = false;
		UpdateGUI();
	}
	else
	{
#if defined(HAVE_X11) && HAVE_X11
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bDisableScreenSaver)
		X11Utils::InhibitScreensaver(X11Utils::XDisplayFromHandle(GetHandle()),
				X11Utils::XWindowFromHandle(GetHandle()), true);
#endif

#ifdef _WIN32
		::SetFocus((HWND)m_RenderParent->GetHandle());
#else
		m_RenderParent->SetFocus();
#endif
		
		wxTheApp->Connect(wxID_ANY, wxEVT_KEY_DOWN, // Keyboard
				wxKeyEventHandler(CFrame::OnKeyDown),
				(wxObject*)0, this);
		wxTheApp->Connect(wxID_ANY, wxEVT_KEY_UP,
				wxKeyEventHandler(CFrame::OnKeyUp),
				(wxObject*)0, this);
		wxTheApp->Connect(wxID_ANY, wxEVT_RIGHT_DOWN, // Mouse
				wxMouseEventHandler(CFrame::OnMouse),
				(wxObject*)0, this);
		wxTheApp->Connect(wxID_ANY, wxEVT_RIGHT_UP,
				wxMouseEventHandler(CFrame::OnMouse),
				(wxObject*)0, this);
		wxTheApp->Connect(wxID_ANY, wxEVT_MIDDLE_DOWN,
				wxMouseEventHandler(CFrame::OnMouse),
				(wxObject*)0, this);
		wxTheApp->Connect(wxID_ANY, wxEVT_MIDDLE_UP,
				wxMouseEventHandler(CFrame::OnMouse),
				(wxObject*)0, this);
		wxTheApp->Connect(wxID_ANY, wxEVT_MOTION,
				wxMouseEventHandler(CFrame::OnMouse),
				(wxObject*)0, this);
		m_RenderParent->Connect(wxID_ANY, wxEVT_SIZE,
				wxSizeEventHandler(CFrame::OnRenderParentResize),
				(wxObject*)0, this);
	}

	wxEndBusyCursor();
}

void CFrame::OnBootDrive(wxCommandEvent& event)
{
	BootGame(drives[event.GetId()-IDM_DRIVE1]);
}

// Refresh the file list and browse for a favorites directory
void CFrame::OnRefresh(wxCommandEvent& WXUNUSED (event))
{
	if (m_GameListCtrl) m_GameListCtrl->Update();
}


void CFrame::OnBrowse(wxCommandEvent& WXUNUSED (event))
{
	if (m_GameListCtrl) m_GameListCtrl->BrowseForDirectory();
}

// Create screenshot
void CFrame::OnScreenshot(wxCommandEvent& WXUNUSED (event))
{
	Core::SaveScreenShot();
}

// Pause the emulation
void CFrame::DoPause()
{
	if (Core::GetState() == Core::CORE_RUN)
	{
		Core::SetState(Core::CORE_PAUSE);
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
			m_RenderParent->SetCursor(wxCURSOR_ARROW);
	}
	else
	{
		Core::SetState(Core::CORE_RUN);
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor &&
				RendererHasFocus())
			m_RenderParent->SetCursor(wxCURSOR_BLANK);
	}
	UpdateGUI();
}

// Stop the emulation
void CFrame::DoStop()
{
	m_bGameLoading = false;
	if (Core::GetState() != Core::CORE_UNINITIALIZED ||
			m_RenderParent != NULL)
	{
#if defined __WXGTK__
		wxMutexGuiLeave();
		std::lock_guard<std::recursive_mutex> lk(keystate_lock);
		wxMutexGuiEnter();
#endif
		// Ask for confirmation in case the user accidentally clicked Stop / Escape
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bConfirmStop)
		{
			wxMessageDialog *m_StopDlg = new wxMessageDialog(
				this,
				_("Do you want to stop the current emulation?"),
				_("Please confirm..."),
				wxYES_NO | wxSTAY_ON_TOP | wxICON_EXCLAMATION,
				wxDefaultPosition);

			int Ret = m_StopDlg->ShowModal();
			m_StopDlg->Destroy();
			if (Ret != wxID_YES)
				return;
		}

		// TODO: Show the author/description dialog here
		if(Movie::IsRecordingInput())
			DoRecordingSave();
		if(Movie::IsPlayingInput() || Movie::IsRecordingInput())
			Movie::EndPlayInput(false);

		wxBeginBusyCursor();
		BootManager::Stop();
		wxEndBusyCursor();

#if defined(HAVE_X11) && HAVE_X11
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.bDisableScreenSaver)
		X11Utils::InhibitScreensaver(X11Utils::XDisplayFromHandle(GetHandle()),
				X11Utils::XWindowFromHandle(GetHandle()), false);
#endif
		m_RenderFrame->SetTitle(wxString::FromAscii(scm_rev_str));

		// Destroy the renderer frame when not rendering to main
		m_RenderParent->Disconnect(wxID_ANY, wxEVT_SIZE,
				wxSizeEventHandler(CFrame::OnRenderParentResize),
				(wxObject*)0, this);
		wxTheApp->Disconnect(wxID_ANY, wxEVT_KEY_DOWN, // Keyboard
				wxKeyEventHandler(CFrame::OnKeyDown),
				(wxObject*)0, this);
		wxTheApp->Disconnect(wxID_ANY, wxEVT_KEY_UP,
				wxKeyEventHandler(CFrame::OnKeyUp),
				(wxObject*)0, this);
		wxTheApp->Disconnect(wxID_ANY, wxEVT_RIGHT_DOWN, // Mouse
				wxMouseEventHandler(CFrame::OnMouse),
				(wxObject*)0, this);
		wxTheApp->Disconnect(wxID_ANY, wxEVT_RIGHT_UP,
				wxMouseEventHandler(CFrame::OnMouse),
				(wxObject*)0, this);
		wxTheApp->Disconnect(wxID_ANY, wxEVT_MIDDLE_DOWN,
				wxMouseEventHandler(CFrame::OnMouse),
				(wxObject*)0, this);
		wxTheApp->Disconnect(wxID_ANY, wxEVT_MIDDLE_UP,
				wxMouseEventHandler(CFrame::OnMouse),
				(wxObject*)0, this);
		wxTheApp->Disconnect(wxID_ANY, wxEVT_MOTION,
				wxMouseEventHandler(CFrame::OnMouse),
				(wxObject*)0, this);
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
			m_RenderParent->SetCursor(wxCURSOR_ARROW);
		DoFullscreen(false);
		if (!SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
			m_RenderFrame->Destroy();
		m_RenderParent = NULL;

		// Clean framerate indications from the status bar.
		GetStatusBar()->SetStatusText(wxT(" "), 0);

		// Clear wiimote connection status from the status bar.
		GetStatusBar()->SetStatusText(wxT(" "), 1);

		// If batch mode was specified on the command-line, exit now.
		if (m_bBatchMode)
			Close(true);

		// If using auto size with render to main, reset the application size.
		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain &&
				SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderWindowAutoSize)
			SetSize(SConfig::GetInstance().m_LocalCoreStartupParameter.iWidth,
					SConfig::GetInstance().m_LocalCoreStartupParameter.iHeight);

		m_GameListCtrl->Enable();
		m_GameListCtrl->Show();
		UpdateGUI();
	}
}

void CFrame::DoRecordingSave()
{
	bool paused = (Core::GetState() == Core::CORE_PAUSE);
	
	if (!paused)
		DoPause();
	
	wxString path = wxFileSelector(
			_("Select The Recording File"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			_("Dolphin TAS Movies (*.dtm)") + 
				wxString::Format(wxT("|*.dtm|%s"), wxGetTranslation(wxALL_FILES)),
			wxFD_SAVE | wxFD_PREVIEW | wxFD_OVERWRITE_PROMPT,
			this);

	if(path.IsEmpty())
		return;
	
	Movie::SaveRecording(path.mb_str());
	
	if (!paused)
		DoPause();
}

void CFrame::OnStop(wxCommandEvent& WXUNUSED (event))
{
	DoStop();
}

void CFrame::OnReset(wxCommandEvent& WXUNUSED (event))
{
	ProcessorInterface::ResetButton_Tap();
}

void CFrame::OnConfigMain(wxCommandEvent& WXUNUSED (event))
{
	CConfigMain ConfigMain(this);
	if (ConfigMain.ShowModal() == wxID_OK)
		m_GameListCtrl->Update();
}

void CFrame::OnConfigGFX(wxCommandEvent& WXUNUSED (event))
{
	if (g_video_backend)
		g_video_backend->ShowConfig(this);
}

void CFrame::OnConfigDSP(wxCommandEvent& WXUNUSED (event))
{
	CConfigMain ConfigMain(this);
	ConfigMain.SetSelectedTab(CConfigMain::ID_AUDIOPAGE);
	if (ConfigMain.ShowModal() == wxID_OK)
		m_GameListCtrl->Update();
}

void CFrame::OnConfigPAD(wxCommandEvent& WXUNUSED (event))
{
	InputPlugin *const pad_plugin = Pad::GetPlugin();
	bool was_init = false;
	if (g_controller_interface.IsInit())	// check if game is running
		was_init = true;
	else
	{
#if defined(HAVE_X11) && HAVE_X11
		Window win = X11Utils::XWindowFromHandle(GetHandle());
		Pad::Initialize((void *)win);
#else
		Pad::Initialize(GetHandle());
#endif
	}
	InputConfigDialog m_ConfigFrame(this, *pad_plugin, _trans("Dolphin GCPad Configuration"));
	m_ConfigFrame.ShowModal();
	m_ConfigFrame.Destroy();
	if (!was_init)				// if game isn't running
	{
		Pad::Shutdown();
	}
}

void CFrame::OnConfigWiimote(wxCommandEvent& WXUNUSED (event))
{
	if (m_WiimoteConfigDiag) return;
	InputPlugin *const wiimote_plugin = Wiimote::GetPlugin();
	bool was_init = false;
	if (g_controller_interface.IsInit())	// check if game is running
		was_init = true;
	else
	{
#if defined(HAVE_X11) && HAVE_X11
		Window win = X11Utils::XWindowFromHandle(GetHandle());
		Wiimote::Initialize((void *)win);
#else
		Wiimote::Initialize(GetHandle());
#endif
	}
	m_WiimoteConfigDiag = new WiimoteConfigDiag(this, *wiimote_plugin, wxString::Format(_("Dolphin Wiimote Configuration%s%s"), (Core::IsRunning() ? _(" - ") : _("")),
		(Core::IsRunning() ? wxString(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strName.c_str(), wxConvUTF8).c_str() : _(""))));
	m_WiimoteConfigDiag->Show();
}

void CFrame::OnConfigHotkey(wxCommandEvent& WXUNUSED (event))
{
	HotkeyConfigDialog *m_HotkeyDialog = new HotkeyConfigDialog(this);
	m_HotkeyDialog->ShowModal();
	m_HotkeyDialog->Destroy();
	// Update the GUI in case menu accelerators were changed
	UpdateGUI();
}

void CFrame::OnHelp(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case wxID_ABOUT:
		{
			AboutDolphin frame(this);
			frame.ShowModal();
		}
		break;
	case IDM_HELPWEBSITE:
		WxUtils::Launch("http://www.dolphin-emulator.com/");
		break;
	case IDM_HELPGOOGLECODE:
		WxUtils::Launch("http://code.google.com/p/dolphin-emu/");
		break;
	}
}

void CFrame::ClearStatusBar()
{
	if (this->GetStatusBar()->IsEnabled()) this->GetStatusBar()->SetStatusText(wxT(""),0);
}

void CFrame::StatusBarMessage(const char * Text, ...)
{
	const int MAX_BYTES = 1024*10;
	char Str[MAX_BYTES];
	va_list ArgPtr;
	int Cnt;
	va_start(ArgPtr, Text);
	Cnt = vsnprintf(Str, MAX_BYTES, Text, ArgPtr);
	va_end(ArgPtr);

	if (this->GetStatusBar()->IsEnabled()) this->GetStatusBar()->SetStatusText(wxString::FromAscii(Str),0);
}


// Miscellaneous menus
// ---------------------
// NetPlay stuff
void CFrame::OnNetPlay(wxCommandEvent& WXUNUSED (event))
{
	if (!g_NetPlaySetupDiag)
	{
		if (NetPlayDiag::GetInstance() != NULL)
			NetPlayDiag::GetInstance()->Raise();
		else
			g_NetPlaySetupDiag = new NetPlaySetupDiag(this, m_GameListCtrl);
	}
	else
		g_NetPlaySetupDiag->Raise();
}

void CFrame::OnMemcard(wxCommandEvent& WXUNUSED (event))
{
	CMemcardManager MemcardManager(this);
	MemcardManager.ShowModal();
}

void CFrame::OnImportSave(wxCommandEvent& WXUNUSED (event)) 
{
	wxString path = wxFileSelector(_("Select the save file"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			_("Wii save files (*.bin)|*.bin"),
			wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
			this);

	if (!path.IsEmpty())
	{
		CWiiSaveCrypted* saveFile = new CWiiSaveCrypted(path.mb_str());
		delete saveFile;
	}
}

void CFrame::OnShow_CheatsWindow(wxCommandEvent& WXUNUSED (event))
{
	if (!g_CheatsWindow)
		g_CheatsWindow = new wxCheatsWindow(this);
	else
		g_CheatsWindow->Raise();
}

void CFrame::OnLoadWiiMenu(wxCommandEvent& WXUNUSED(event))
{
	BootGame(Common::GetTitleContentPath(TITLEID_SYSMENU));
}

void CFrame::OnInstallWAD(wxCommandEvent& event)
{
	std::string fileName;

	switch(event.GetId())
	{
	case IDM_LIST_INSTALLWAD:
	{
		const GameListItem *iso = m_GameListCtrl->GetSelectedISO();
		if (!iso)
			return;
		fileName = iso->GetFileName();
		break;
	}
	case IDM_MENU_INSTALLWAD:
	{
		wxString path = wxFileSelector(
			_("Select a Wii WAD file to install"),
			wxEmptyString, wxEmptyString, wxEmptyString,
			_T("Wii WAD file (*.wad)|*.wad"),
			wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
			this);
		fileName = path.mb_str();
		break;
	}
	default:
		return;
	}

	wxProgressDialog dialog(_("Installing WAD..."),
		_("Working..."),
		1000, // range
		this, // parent
		wxPD_APP_MODAL |
		wxPD_ELAPSED_TIME |
		wxPD_ESTIMATED_TIME |
		wxPD_REMAINING_TIME |
		wxPD_SMOOTH // - makes indeterminate mode bar on WinXP very small
		);

	dialog.CenterOnParent();

	u64 titleID = DiscIO::CNANDContentManager::Access().Install_WiiWAD(fileName);
	if (titleID == TITLEID_SYSMENU)
	{
		UpdateWiiMenuChoice();
	}
}


void CFrame::UpdateWiiMenuChoice(wxMenuItem *WiiMenuItem)
{
	if (!WiiMenuItem)
	{
		WiiMenuItem = GetMenuBar()->FindItem(IDM_LOAD_WII_MENU);
	}

	const DiscIO::INANDContentLoader & SysMenu_Loader = DiscIO::CNANDContentManager::Access().GetNANDLoader(TITLEID_SYSMENU, true);
	if (SysMenu_Loader.IsValid())
	{
		int sysmenuVersion = SysMenu_Loader.GetTitleVersion();
		char sysmenuRegion = SysMenu_Loader.GetCountryChar();
		WiiMenuItem->Enable();
		WiiMenuItem->SetItemLabel(wxString::Format(_("Load Wii System Menu %d%c"), sysmenuVersion, sysmenuRegion));
	}
	else
	{
		WiiMenuItem->Enable(false);
		WiiMenuItem->SetItemLabel(_("Load Wii System Menu"));
	}
}

void CFrame::OnFifoPlayer(wxCommandEvent& WXUNUSED (event))
{
	if (m_FifoPlayerDlg)
	{
		m_FifoPlayerDlg->Show();
		m_FifoPlayerDlg->SetFocus();
	}
	else
	{
		m_FifoPlayerDlg = new FifoPlayerDlg(this);
	}
}

void CFrame::ConnectWiimote(int wm_idx, bool connect)
{
	if (Core::IsRunning() && SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
	{
		GetUsbPointer()->AccessWiiMote(wm_idx | 0x100)->Activate(connect);
		wxString msg(wxString::Format(wxT("Wiimote %i %s"), wm_idx + 1,
					connect ? wxT("Connected") : wxT("Disconnected")));
		Core::DisplayMessage(msg.ToAscii(), 3000);
	}
}

void CFrame::OnConnectWiimote(wxCommandEvent& event)
{
	ConnectWiimote(event.GetId() - IDM_CONNECT_WIIMOTE1, event.IsChecked());
}

// Toogle fullscreen. In Windows the fullscreen mode is accomplished by expanding the m_Panel to cover
// the entire screen (when we render to the main window).
void CFrame::OnToggleFullscreen(wxCommandEvent& WXUNUSED (event))
{
	DoFullscreen(!RendererIsFullscreen());
}

void CFrame::OnToggleDualCore(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread = !SConfig::GetInstance().m_LocalCoreStartupParameter.bCPUThread;
	SConfig::GetInstance().SaveSettings();
}

void CFrame::OnToggleSkipIdle(wxCommandEvent& WXUNUSED (event))
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle = !SConfig::GetInstance().m_LocalCoreStartupParameter.bSkipIdle;
	SConfig::GetInstance().SaveSettings();
}

void CFrame::OnLoadStateFromFile(wxCommandEvent& WXUNUSED (event))
{
	wxString path = wxFileSelector(
		_("Select the state to load"),
		wxEmptyString, wxEmptyString, wxEmptyString,
		_("All Save States (sav, s##)") + 
			wxString::Format(wxT("|*.sav;*.s??|%s"), wxGetTranslation(wxALL_FILES)),
		wxFD_OPEN | wxFD_PREVIEW | wxFD_FILE_MUST_EXIST,
		this);

	if (!path.IsEmpty())
		State::LoadAs((const char*)path.mb_str());
}

void CFrame::OnSaveStateToFile(wxCommandEvent& WXUNUSED (event))
{
	wxString path = wxFileSelector(
		_("Select the state to save"),
		wxEmptyString, wxEmptyString, wxEmptyString,
		_("All Save States (sav, s##)") + 
			wxString::Format(wxT("|*.sav;*.s??|%s"), wxGetTranslation(wxALL_FILES)),
		wxFD_SAVE,
		this);

	if (!path.IsEmpty())
		State::SaveAs((const char*)path.mb_str());
}

void CFrame::OnLoadLastState(wxCommandEvent& WXUNUSED (event))
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
		State::LoadLastSaved();
}

void CFrame::OnUndoLoadState(wxCommandEvent& WXUNUSED (event))
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
		State::UndoLoadState();
}

void CFrame::OnUndoSaveState(wxCommandEvent& WXUNUSED (event))
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
		State::UndoSaveState();
}


void CFrame::OnLoadState(wxCommandEvent& event)
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		int id = event.GetId();
		int slot = id - IDM_LOADSLOT1 + 1;
		State::Load(slot);
	}
}

void CFrame::OnSaveState(wxCommandEvent& event)
{
	if (Core::GetState() != Core::CORE_UNINITIALIZED)
	{
		int id = event.GetId();
		int slot = id - IDM_SAVESLOT1 + 1;
		State::Save(slot);
	}
}

void CFrame::OnFrameSkip(wxCommandEvent& event)
{
	int amount = event.GetId() - IDM_FRAMESKIP0;

	Movie::SetFrameSkipping((unsigned int)amount);
}




// GUI
// ---------------------

// Update the enabled/disabled status
void CFrame::UpdateGUI()
{
	// Save status
	bool Initialized = Core::IsRunning();
	bool Running = Core::GetState() == Core::CORE_RUN;
	bool Paused = Core::GetState() == Core::CORE_PAUSE;

	// Make sure that we have a toolbar
	if (m_ToolBar)
	{
		// Enable/disable the Config and Stop buttons
		m_ToolBar->EnableTool(wxID_OPEN, !Initialized);
		m_ToolBar->EnableTool(wxID_REFRESH, !Initialized); // Don't allow refresh when we don't show the list
		m_ToolBar->EnableTool(IDM_STOP, Running || Paused);
		m_ToolBar->EnableTool(IDM_TOGGLE_FULLSCREEN, Running || Paused);
		m_ToolBar->EnableTool(IDM_SCREENSHOT, Running || Paused);
	}

	// File
	GetMenuBar()->FindItem(wxID_OPEN)->Enable(!Initialized);
	GetMenuBar()->FindItem(IDM_DRIVES)->Enable(!Initialized);
	GetMenuBar()->FindItem(wxID_REFRESH)->Enable(!Initialized);
	GetMenuBar()->FindItem(IDM_BROWSE)->Enable(!Initialized);

	// Emulation
	GetMenuBar()->FindItem(IDM_STOP)->Enable(Running || Paused);
	GetMenuBar()->FindItem(IDM_RESET)->Enable(Running || Paused);
	GetMenuBar()->FindItem(IDM_RECORD)->Enable(!Movie::IsRecordingInput());
	GetMenuBar()->FindItem(IDM_PLAYRECORD)->Enable(!Initialized);
	GetMenuBar()->FindItem(IDM_RECORDEXPORT)->Enable(Movie::IsRecordingInput());
	GetMenuBar()->FindItem(IDM_FRAMESTEP)->Enable(Running || Paused);
	GetMenuBar()->FindItem(IDM_SCREENSHOT)->Enable(Running || Paused);
	GetMenuBar()->FindItem(IDM_TOGGLE_FULLSCREEN)->Enable(Running || Paused);

	// Update Menu Accelerators
	for (unsigned int i = 0; i < NUM_HOTKEYS; i++)
		GetMenuBar()->FindItem(GetCmdForHotkey(i))->SetItemLabel(GetMenuLabel(i));

	GetMenuBar()->FindItem(IDM_LOADSTATE)->Enable(Initialized);
	GetMenuBar()->FindItem(IDM_SAVESTATE)->Enable(Initialized);
	// Misc
	GetMenuBar()->FindItem(IDM_CHANGEDISC)->Enable(Initialized);
	if (DiscIO::CNANDContentManager::Access().GetNANDLoader(TITLEID_SYSMENU).IsValid())
		GetMenuBar()->FindItem(IDM_LOAD_WII_MENU)->Enable(!Initialized);

	GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE1)->
		Enable(Initialized  && SConfig::GetInstance().m_LocalCoreStartupParameter.bWii);
	GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE2)->
		Enable(Initialized  && SConfig::GetInstance().m_LocalCoreStartupParameter.bWii);
	GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE3)->
		Enable(Initialized  && SConfig::GetInstance().m_LocalCoreStartupParameter.bWii);
	GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE4)->
		Enable(Initialized  && SConfig::GetInstance().m_LocalCoreStartupParameter.bWii);
	if (Initialized && SConfig::GetInstance().m_LocalCoreStartupParameter.bWii)
	{
		GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE1)->Check(GetUsbPointer()->
				AccessWiiMote(0x0100)->IsConnected());
		GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE2)->Check(GetUsbPointer()->
				AccessWiiMote(0x0101)->IsConnected());
		GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE3)->Check(GetUsbPointer()->
				AccessWiiMote(0x0102)->IsConnected());
		GetMenuBar()->FindItem(IDM_CONNECT_WIIMOTE4)->Check(GetUsbPointer()->
				AccessWiiMote(0x0103)->IsConnected());
	}

	if (Running)
	{
		if (m_ToolBar)
		{
			m_ToolBar->SetToolBitmap(IDM_PLAY, m_Bitmaps[Toolbar_Pause]);
			m_ToolBar->SetToolShortHelp(IDM_PLAY, _("Pause"));
			m_ToolBar->SetToolLabel(IDM_PLAY, _("Pause"));
		}
	}
	else
	{
		if (m_ToolBar)
		{
			m_ToolBar->SetToolBitmap(IDM_PLAY, m_Bitmaps[Toolbar_Play]);
			m_ToolBar->SetToolShortHelp(IDM_PLAY, _("Play"));
			m_ToolBar->SetToolLabel(IDM_PLAY, _("Play"));
		}
	}
	
	if (!Initialized && !m_bGameLoading)
	{
		if (m_GameListCtrl->IsEnabled())
		{
			// Prepare to load Default ISO, enable play button
			if (!SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM.empty())
			{
				if (m_ToolBar)
					m_ToolBar->EnableTool(IDM_PLAY, true);
				GetMenuBar()->FindItem(IDM_PLAY)->Enable(true);
			}
			// Prepare to load last selected file, enable play button
			else if (!SConfig::GetInstance().m_LastFilename.empty()
					&& wxFileExists(wxString(SConfig::GetInstance().m_LastFilename.c_str(), wxConvUTF8)))
			{
				if (m_ToolBar)
					m_ToolBar->EnableTool(IDM_PLAY, true);
				GetMenuBar()->FindItem(IDM_PLAY)->Enable(true);
			}
			else
			{
				// No game has been selected yet, disable play button
				if (m_ToolBar)
					m_ToolBar->EnableTool(IDM_PLAY, false);
				GetMenuBar()->FindItem(IDM_PLAY)->Enable(false);
			}
		}

		// Game has not started, show game list
		if (!m_GameListCtrl->IsShown())
		{
			m_GameListCtrl->Enable();
			m_GameListCtrl->Show();
		}
		// Game has been selected but not started, enable play button
		if (m_GameListCtrl->GetSelectedISO() != NULL && m_GameListCtrl->IsEnabled())
		{
			if (m_ToolBar)
				m_ToolBar->EnableTool(IDM_PLAY, true);
			GetMenuBar()->FindItem(IDM_PLAY)->Enable(true);
		}
	}
	else if (Initialized)
	{
		// Game has been loaded, enable the pause button
		if (m_ToolBar)
			m_ToolBar->EnableTool(IDM_PLAY, true);
		GetMenuBar()->FindItem(IDM_PLAY)->Enable(true);

		// Reset game loading flag
		m_bGameLoading = false;
	}

	if (m_ToolBar) m_ToolBar->Refresh();

	// Commit changes to manager
	m_Mgr->Update();
}

void CFrame::UpdateGameList()
{
	m_GameListCtrl->Update();
}

void CFrame::GameListChanged(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case IDM_LISTWII:
		SConfig::GetInstance().m_ListWii = event.IsChecked();
		break;
	case IDM_LISTGC:
		SConfig::GetInstance().m_ListGC = event.IsChecked();
		break;
	case IDM_LISTWAD:
		SConfig::GetInstance().m_ListWad = event.IsChecked();
		break;
	case IDM_LISTJAP:
		SConfig::GetInstance().m_ListJap = event.IsChecked();
		break;
	case IDM_LISTPAL:
		SConfig::GetInstance().m_ListPal = event.IsChecked();
		break;
	case IDM_LISTUSA:
		SConfig::GetInstance().m_ListUsa = event.IsChecked();
		break;
	case IDM_LISTFRANCE:
		SConfig::GetInstance().m_ListFrance = event.IsChecked();
		break;
	case IDM_LISTITALY:
		SConfig::GetInstance().m_ListItaly = event.IsChecked();
		break;
	case IDM_LISTKOREA:
		SConfig::GetInstance().m_ListKorea = event.IsChecked();
		break;
	case IDM_LISTTAIWAN:
		SConfig::GetInstance().m_ListTaiwan = event.IsChecked();
		break;
	case IDM_LIST_UNK:
		SConfig::GetInstance().m_ListUnknown = event.IsChecked();
		break;
	case IDM_LISTDRIVES:
		SConfig::GetInstance().m_ListDrives = event.IsChecked();
		break;
	case IDM_PURGECACHE:
		CFileSearch::XStringVector Directories;
		Directories.push_back(File::GetUserPath(D_CACHE_IDX).c_str());
		CFileSearch::XStringVector Extensions;
		Extensions.push_back("*.cache");
		
		CFileSearch FileSearch(Extensions, Directories);
		const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();
		
		for (u32 i = 0; i < rFilenames.size(); i++)
		{
			File::Delete(rFilenames[i]);
		}
		break;
	}
	
	if (m_GameListCtrl) m_GameListCtrl->Update();
}

// Enable and disable the toolbar
void CFrame::OnToggleToolbar(wxCommandEvent& event)
{
	SConfig::GetInstance().m_InterfaceToolbar = event.IsChecked();
	DoToggleToolbar(event.IsChecked());
}
void CFrame::DoToggleToolbar(bool _show)
{
	if (_show)
	{
		m_Mgr->GetPane(wxT("TBMain")).Show();
		if (g_pCodeWindow)
		{
			m_Mgr->GetPane(wxT("TBDebug")).Show();
			m_Mgr->GetPane(wxT("TBAui")).Show();
		}
		m_Mgr->Update();
	}
	else
	{
		m_Mgr->GetPane(wxT("TBMain")).Hide();
		if (g_pCodeWindow)
		{
			m_Mgr->GetPane(wxT("TBDebug")).Hide();
			m_Mgr->GetPane(wxT("TBAui")).Hide();
		}
		m_Mgr->Update();
	}
}

// Enable and disable the status bar
void CFrame::OnToggleStatusbar(wxCommandEvent& event)
{
	SConfig::GetInstance().m_InterfaceStatusbar = event.IsChecked();
	if (SConfig::GetInstance().m_InterfaceStatusbar == true)
		GetStatusBar()->Show();
	else
		GetStatusBar()->Hide();

	this->SendSizeEvent();
}