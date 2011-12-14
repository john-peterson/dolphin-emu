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

#ifndef _CTRLDLG_H_
#define _CTRLDLG_H_

#define SLIDER_TICK_COUNT			100
#define DETECT_WAIT_TIME			1500

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
#include "ControllerEmu.h"
#include "InputConfig.h"
//#include "InputConfigDiag.h"

//class ControlButton;

class ControlDialog : public wxDialog
{
public:
	ControlDialog(wxWindow* const parent, InputPlugin& plugin, const ControllerInterface::DeviceQualifier& default_device, ControllerInterface::ControlReference* const ref);
	
	wxStaticBoxSizer* CreateControlChooser(wxWindow* const parent, wxWindow* const eventsink);

	void DetectControl(wxCommandEvent& event);
	void ClearControl(wxCommandEvent& event);
	void SetControl(wxCommandEvent& event);
	void SetDevice(wxCommandEvent& event);

	void UpdateGUI();
	void UpdateListContents();
	void UpdateDeviceComboBox();
	void SelectControl(const std::string& name);

	void SetSelectedControl(wxCommandEvent& event);
	void AppendControl(wxCommandEvent& event);
	void AdjustControlOption(wxCommandEvent& event);

	ControllerInterface::ControlReference* const		control_reference;
	InputPlugin&			m_plugin;
	wxComboBox*				device_cbox;

	wxTextCtrl*		textctrl;
	wxListBox*		control_lbox;
	wxSlider*		range_slider;

private:
	wxWindow* const		m_parent;
	wxStaticText*		m_bound_label;
	ControllerInterface::DeviceQualifier	m_default_device;
	ControllerInterface::DeviceQualifier	m_devq;
};

class ControlButton : public wxButton
{
public:
	ControlButton(wxWindow* const parent, ControllerInterface::ControlReference* const _ref, const unsigned int width, const std::string& label = "");

	ControllerInterface::ControlReference* const		control_reference;
};

#endif