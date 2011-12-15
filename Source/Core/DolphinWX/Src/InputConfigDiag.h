// Copyright (C) 2010 Dolphin Project.

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

#ifndef _CONFIGBOX_H_
#define _CONFIGBOX_H_

#define PREVIEW_UPDATE_TIME			25

// might have to change this setup for wiimote
#define PROFILES_PATH				"Profiles/"

#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>

#include <sstream>
#include <vector>

#include "ControllerInterface/ControllerInterface.h"
#include "ControlDiag.h"
#include "ControllerEmu.h"
#include "InputConfig.h"
#include "FileSearch.h"
#include "UDPWrapper.h"

class GamepadPage;

class PadSetting
{
protected:
	PadSetting(wxControl* const _control) : wxcontrol(_control) { wxcontrol->SetClientData(this); }

public:
	virtual void UpdateGUI() = 0;
	virtual void UpdateValue() = 0;

	virtual ~PadSetting() {}

	wxControl* const	wxcontrol;
};

class PadSettingExtension : public PadSetting
{
public:
	PadSettingExtension(wxWindow* const parent, ControllerEmu::Extension* const ext);
	void UpdateGUI();
	void UpdateValue();

	ControllerEmu::Extension* const	extension;
};

class PadSettingSpin : public PadSetting
{
public:
	PadSettingSpin(wxWindow* const parent, ControllerEmu::ControlGroup::Setting* const setting)
		: PadSetting(new wxSpinCtrl(parent, -1, wxEmptyString, wxDefaultPosition
			, wxSize(54, -1), 0, setting->low, setting->high, (int)(setting->value * 100)))
		, value(setting->value) {}

	void UpdateGUI();
	void UpdateValue();

	ControlState& value;
};

class PadSettingCheckBox : public PadSetting
{
public:
	PadSettingCheckBox(wxWindow* const parent, ControlState& _value, const char* const label);
	void UpdateGUI();
	void UpdateValue();

	ControlState&		value;
};

class ExtensionButton : public wxButton
{
public:
	ExtensionButton(wxWindow* const parent, ControllerEmu::Extension* const ext)
		: wxButton(parent, -1, _("Configure"), wxDefaultPosition)
		, extension(ext) {}

	ControllerEmu::Extension* const	extension;
};

class UDPConfigButton : public wxButton
{
public:
	UDPWrapper* const wrapper;
	UDPConfigButton(wxWindow* const parent, UDPWrapper * udp)
		: wxButton(parent, -1, _("Configure"), wxDefaultPosition)
		, wrapper(udp)
	{}
};

class ControlGroupBox : public wxBoxSizer
{
public:
	ControlGroupBox(ControllerEmu::ControlGroup* const group, wxWindow* const parent, wxWindow* const eventsink);
	~ControlGroupBox();

	std::vector<PadSetting*>		options;

	ControllerEmu::ControlGroup* const	control_group;
	wxStaticBitmap*					static_bitmap;
	std::vector<ControlButton*>		control_buttons;
};

class ControlGroupsSizer : public wxBoxSizer
{
public:
	ControlGroupsSizer(ControllerEmu* const controller, wxWindow* const parent,  wxWindow* const eventsink, std::vector<ControlGroupBox*>* const groups = NULL);
};

class InputConfigDialog;

class GamepadPage : public wxNotebookPage
{
	friend class InputConfigDialog;
	friend class ControlDialog;

public:
	GamepadPage(wxWindow* parent, InputPlugin& plugin, const unsigned int pad_num, InputConfigDialog* const config_dialog);

	void UpdateGUI();

	void RefreshDevices(wxCommandEvent& event);

	void LoadProfile(wxCommandEvent& event);
	void SaveProfile(wxCommandEvent& event);
	void DeleteProfile(wxCommandEvent& event);

	void ConfigControl(wxCommandEvent& event);
	void ClearControl(wxCommandEvent& event);
	void DetectControl(wxCommandEvent& event);

	void ConfigExtension(wxCommandEvent& event);

	void ConfigUDPWii(wxCommandEvent& event);

	void SetDevice(wxCommandEvent& event);

	void ClearAll(wxCommandEvent& event);
	void LoadDefaults(wxCommandEvent& event);
	
	void AdjustSetting(wxCommandEvent& event);

	void GetProfilePath(std::string& path);

	wxComboBox*					profile_cbox;
	wxComboBox*					device_cbox;

	std::vector<ControlGroupBox*>		control_groups;

protected:
	
	ControllerEmu* const				controller;

private:

	ControlDialog*				m_control_dialog;
	InputConfigDialog* const	m_config_dialog;
	InputPlugin &m_plugin;
};

class InputConfigDialog : public wxDialog
{
public:
	InputConfigDialog(wxWindow* const parent, InputPlugin& plugin, const wxString& name, const int tab_num = 0);
	//~InputConfigDialog();

	void OnClose(wxCloseEvent& event);

	void Save(wxCommandEvent& event);
	void Apply(wxCommandEvent& event);
	void Cancel(wxCommandEvent& event);

	void UpdateDeviceComboBox();
	void UpdateProfileComboBox(std::string fname = "");

	void UpdateControlReferences();
	void UpdateBitmaps(wxTimerEvent&);
	void UpdateGUI();

private:

	wxString					m_title;
	wxWindow*					m_parent;
	wxNotebook*					m_pad_notebook;
	std::vector<GamepadPage*>	m_padpages;
	InputPlugin&				m_plugin;
	wxTimer*					m_update_timer;
};

#endif