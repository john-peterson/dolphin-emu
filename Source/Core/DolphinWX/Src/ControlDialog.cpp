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

#include "ControlDialog.h"

#define _connect_macro_(b, f, c, s)	(b)->Connect(wxID_ANY, (c), wxCommandEventHandler(f), (wxObject*)0, (wxEvtHandler*)s)
#if wxCHECK_VERSION(2, 9, 0)
	#define WXSTR_FROM_STR(s)	(wxString(s))
	#define WXTSTR_FROM_CSTR(s)	(wxGetTranslation(wxString(s)))
	#define STR_FROM_WXSTR(w)	((w).ToStdString())
#else
	#define WXSTR_FROM_STR(s)	(wxString::FromUTF8((s).c_str()))
	#define WXTSTR_FROM_CSTR(s)	(wxGetTranslation(wxString::FromUTF8(s)))
	#define STR_FROM_WXSTR(w)	(std::string((w).ToUTF8()))
#endif

ControlDialog::ControlDialog(wxWindow* const parent, InputPlugin& plugin, const ControllerInterface::DeviceQualifier& default_device, ControllerInterface::ControlReference* const ref)
	: wxDialog(parent, -1, _("Configure Control"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	, control_reference(ref)
	, m_plugin(plugin)
	, m_devq(default_device)
	, m_default_device(default_device)
	, m_parent(parent)
{
	device_cbox = new wxComboBox(this, -1, WXSTR_FROM_STR(m_devq.ToString()), wxDefaultPosition, wxSize(256,-1), wxArrayString(), wxCB_SORT|wxCB_READONLY);

	_connect_macro_(device_cbox, ControlDialog::SetDevice, wxEVT_COMMAND_COMBOBOX_SELECTED, this);
	_connect_macro_(device_cbox, ControlDialog::SetDevice, wxEVT_COMMAND_TEXT_ENTER, this);

	wxStaticBoxSizer* const control_chooser = CreateControlChooser(this, parent);

	wxStaticBoxSizer* const d_szr = new wxStaticBoxSizer(wxVERTICAL, this, _("Device"));
	d_szr->Add(device_cbox, 0, wxEXPAND|wxALL, 5);

	wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
	szr->Add(d_szr, 0, wxEXPAND|wxLEFT|wxRIGHT|wxTOP, 5);
	szr->Add(control_chooser, 1, wxEXPAND|wxALL, 5);

	SetSizerAndFit(szr);	// needed

	UpdateDeviceComboBox();
	UpdateGUI();
	SetFocus();
}

wxStaticBoxSizer* ControlDialog::CreateControlChooser(wxWindow* const parent, wxWindow* const eventsink)
{
	wxStaticBoxSizer* const main_szr = new wxStaticBoxSizer(wxVERTICAL, parent, control_reference->is_input ? _("Input") : _("Output"));

	textctrl = new wxTextCtrl(parent, -1, wxEmptyString, wxDefaultPosition, wxSize(-1, 48), wxTE_MULTILINE);

	wxButton* const detect_button = new wxButton(parent, -1, control_reference->is_input ? _("Detect") : _("Test"));

	wxButton* const clear_button = new  wxButton(parent, -1, _("Clear"));
	wxButton* const set_button = new wxButton(parent, -1, _("Set"));

	wxButton* const select_button = new wxButton(parent, -1, _("Select"));
	_connect_macro_(select_button, ControlDialog::SetSelectedControl, wxEVT_COMMAND_BUTTON_CLICKED, parent);

	wxButton* const or_button = new  wxButton(parent, -1, _("| OR"), wxDefaultPosition);
	_connect_macro_(or_button, ControlDialog::AppendControl, wxEVT_COMMAND_BUTTON_CLICKED, parent);

	control_lbox = new wxListBox(parent, -1, wxDefaultPosition, wxSize(-1, 64));

	wxBoxSizer* const button_sizer = new wxBoxSizer(wxVERTICAL);
	button_sizer->Add(detect_button, 1, 0, 5);
	button_sizer->Add(select_button, 1, 0, 5);
	button_sizer->Add(or_button, 1, 0, 5);

	if (control_reference->is_input)
	{
		// TODO: check if && is good on other OS
		wxButton* const and_button = new  wxButton(parent, -1, _("&& AND"), wxDefaultPosition);
		wxButton* const not_button = new  wxButton(parent, -1, _("! NOT"), wxDefaultPosition);
		wxButton* const add_button = new  wxButton(parent, -1, _("^ ADD"), wxDefaultPosition);

		_connect_macro_(and_button, ControlDialog::AppendControl, wxEVT_COMMAND_BUTTON_CLICKED, parent);
		_connect_macro_(not_button, ControlDialog::AppendControl, wxEVT_COMMAND_BUTTON_CLICKED, parent);
		_connect_macro_(add_button, ControlDialog::AppendControl, wxEVT_COMMAND_BUTTON_CLICKED, parent);

		button_sizer->Add(and_button, 1, 0, 5);
		button_sizer->Add(not_button, 1, 0, 5);
		button_sizer->Add(add_button, 1, 0, 5);
	}

	_connect_macro_(detect_button, ControlDialog::DetectControl, wxEVT_COMMAND_BUTTON_CLICKED, parent);
	_connect_macro_(clear_button, ControlDialog::ClearControl, wxEVT_COMMAND_BUTTON_CLICKED, parent);
	_connect_macro_(set_button, ControlDialog::SetControl, wxEVT_COMMAND_BUTTON_CLICKED, parent);

	range_slider = new wxSlider(parent, wxID_ANY, SLIDER_TICK_COUNT, 0, SLIDER_TICK_COUNT * 10, wxDefaultPosition, wxDefaultSize, wxSL_TOP|wxSL_LABELS/*| wxSL_AUTOTICKS*/);
	range_slider->SetValue((int)(control_reference->range * SLIDER_TICK_COUNT));
	_connect_macro_(range_slider, ControlDialog::AdjustControlOption, wxEVT_SCROLL_CHANGED, parent);
	wxBoxSizer* const range_sizer = new wxBoxSizer(wxHORIZONTAL);
	range_sizer->Add(new wxStaticText(parent, wxID_ANY, _("Range")), 0, wxTOP|wxALIGN_CENTER_VERTICAL|wxLEFT, 5);
	range_sizer->Add(range_slider, 1, wxLEFT, 5);

	wxBoxSizer* const ctrls_sizer = new wxBoxSizer(wxHORIZONTAL);
	ctrls_sizer->Add(control_lbox, 1, wxEXPAND, 0);
	ctrls_sizer->Add(button_sizer, 0, wxEXPAND, 0);

	wxSizer* const bottom_btns_sizer = CreateButtonSizer(wxOK);
	bottom_btns_sizer->Prepend(set_button, 0, wxRIGHT, 5);
	bottom_btns_sizer->Prepend(clear_button, 0, wxLEFT, 5);

	m_bound_label = new wxStaticText(parent, -1, wxT(""));

	main_szr->Add(range_sizer, 0, wxEXPAND|wxLEFT|wxRIGHT, 5);
	main_szr->Add(ctrls_sizer, 0, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 5);
	main_szr->Add(textctrl, 1, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 5);
	main_szr->Add(bottom_btns_sizer, 0, wxEXPAND|wxBOTTOM|wxRIGHT, 5);
	main_szr->Add(m_bound_label, 0, wxCENTER, 0);

	UpdateListContents();

	return main_szr;
}

void ControlDialog::UpdateGUI()
{
	// update textbox
	textctrl->SetValue(WXSTR_FROM_STR(control_reference->expression));

	// updates the "bound controls:" label
	m_bound_label->SetLabel(wxString::Format(_("Bound Controls: %lu"),
		(unsigned long)control_reference->BoundCount()));
};

void ControlDialog::UpdateDeviceComboBox()
{
	device_cbox->Clear();
	ControllerInterface::DeviceQualifier dq;
	std::vector<ControllerInterface::Device*>::const_iterator
		di = g_controller_interface.Devices().begin(),
		de = g_controller_interface.Devices().end();
	for (; di!=de; ++di)
	{
		dq.FromDevice(*di);
		device_cbox->Append(WXSTR_FROM_STR(dq.ToString()));
	}
	device_cbox->SetSelection(0);
}

void ControlDialog::AdjustControlOption(wxCommandEvent&)
{
	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
	control_reference->range = ControlState(range_slider->GetValue()) / SLIDER_TICK_COUNT;
}

void ControlDialog::DetectControl(wxCommandEvent& event)
{
	wxButton* const btn = (wxButton*)event.GetEventObject();
	const wxString lbl = btn->GetLabel();

	ControllerInterface::Device* const dev = g_controller_interface.FindDevice(m_devq);
	if (dev)
	{
		btn->SetLabel(_("[ waiting ]"));

		// apparently, this makes the "waiting" text work on Linux
		wxTheApp->Yield();

		std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
		ControllerInterface::Device::Control* const ctrl = control_reference->Detect(DETECT_WAIT_TIME, dev);

		// if we got input, select it in the list
		if (ctrl)
			SelectControl(ctrl->GetName());

		btn->SetLabel(lbl);
	}
}

void ControlDialog::UpdateListContents()
{
	control_lbox->Clear();

	ControllerInterface::Device* const dev = g_controller_interface.FindDevice(m_devq);
	if (dev)
	{
		if (control_reference->is_input)
		{
			// for inputs
			std::vector<ControllerInterface::Device::Input*>::const_iterator
				i = dev->Inputs().begin(),
				e = dev->Inputs().end();
			for (; i!=e; ++i)
				control_lbox->Append(WXSTR_FROM_STR((*i)->GetName()));
		}
		else
		{
			// for outputs
			std::vector<ControllerInterface::Device::Output*>::const_iterator
				i = dev->Outputs().begin(),
				e = dev->Outputs().end();
			for (; i!=e; ++i)
				control_lbox->Append(WXSTR_FROM_STR((*i)->GetName()));
		}
	}
}

void ControlDialog::SelectControl(const std::string& name)
{
	//UpdateGUI();

	const int f = control_lbox->FindString(WXSTR_FROM_STR(name));
	if (f >= 0)
		control_lbox->Select(f);
}

void ControlDialog::SetControl(wxCommandEvent&)
{
	control_reference->expression = STR_FROM_WXSTR(textctrl->GetValue());

	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
	g_controller_interface.UpdateReference(control_reference, m_default_device);

	UpdateGUI();
}

void ControlDialog::SetDevice(wxCommandEvent&)
{
	m_devq.FromString(STR_FROM_WXSTR(device_cbox->GetValue()));
	
	// show user what it was validated as
	//device_cbox->SetValue(WXSTR_FROM_STR(m_devq.ToString()));

	// update gui
	UpdateListContents();
}

void ControlDialog::ClearControl(wxCommandEvent&)
{
	control_reference->expression.clear();

	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
	g_controller_interface.UpdateReference(control_reference, m_default_device);

	UpdateGUI();
}

void ControlDialog::SetSelectedControl(wxCommandEvent&)
{
	const int num = control_lbox->GetSelection();

	if (num < 0)
		return;

	wxString expr;

	// non-default device
	if (false == (m_devq == m_default_device))
		expr.append(wxT('`')).append(WXSTR_FROM_STR(m_devq.ToString())).append(wxT('`'));

	// append the control name
	expr += control_lbox->GetString(num);

	control_reference->expression = STR_FROM_WXSTR(expr);

	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
	g_controller_interface.UpdateReference(control_reference, m_default_device);

	UpdateGUI();
}

void ControlDialog::AppendControl(wxCommandEvent& event)
{
	const int num = control_lbox->GetSelection();

	if (num < 0)
		return;

	// o boy!, hax
	const wxString lbl = ((wxButton*)event.GetEventObject())->GetLabel();

	wxString expr = textctrl->GetValue();

	// append the operator to the expression
	if (wxT('!') == lbl[0] || false == expr.empty())
		expr += lbl[0];

	// non-default device
	if (false == (m_devq == m_default_device))
		expr.append(wxT('`')).append(WXSTR_FROM_STR(m_devq.ToString())).append(wxT('`'));

	// append the control name
	expr += control_lbox->GetString(num);

	control_reference->expression = STR_FROM_WXSTR(expr);

	std::lock_guard<std::recursive_mutex> lk(m_plugin.controls_lock);
	g_controller_interface.UpdateReference(control_reference, m_default_device);

	UpdateGUI();
}

ControlButton::ControlButton(wxWindow* const parent, ControllerInterface::ControlReference* const _ref, const unsigned int width, const std::string& label)
: wxButton(parent, -1, wxT(""), wxDefaultPosition, wxSize(width,20))
, control_reference(_ref)
{
	if (label.empty())
		SetLabel(WXSTR_FROM_STR(_ref->expression));
	else
		SetLabel(WXSTR_FROM_STR(label));
	SetFont(wxFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
}