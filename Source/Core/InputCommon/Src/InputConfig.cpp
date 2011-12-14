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

#include "InputConfig.h"

InputPlugin::~InputPlugin()
{
	// delete pads
	std::vector<ControllerEmu*>::const_iterator i = controllers.begin(),
		e = controllers.end();
	for ( ; i != e; ++i )
		delete *i;
}

bool InputPlugin::LoadConfig()
{
	IniFile inifile, gameiniFile;
	std::string ini = File::GetUserPath(D_CONFIG_IDX) + ini_name + ".ini";
	std::string gameini = Core::IsRunning() ? SConfig::GetInstance().m_LocalCoreStartupParameter.m_strGameIni : "";

	if (inifile.Load(ini))
	{
		std::vector< ControllerEmu* >::const_iterator
			i = controllers.begin(),
			e = controllers.end();
		for (; i!=e; ++i)
		{
			// load settings from ini
			if (gameiniFile.Load(gameini))
			{
				// copy from baseini
				if (gameiniFile.GetOrCreateSection((*i)->GetName().c_str())->Exists("FirstUse"))
				{
					gameiniFile.GetOrCreateSection((*i)->GetName().c_str())->Copy(inifile.GetOrCreateSection((*i)->GetName().c_str()));
					gameiniFile.GetOrCreateSection((*i)->GetName().c_str())->Delete("FirstUse");
					gameiniFile.Save(gameini);
				}
				(*i)->LoadConfig(gameiniFile.GetOrCreateSection((*i)->GetName().c_str()));
			}
			else if (inifile.Load(ini))
			{
				(*i)->LoadConfig(inifile.GetOrCreateSection((*i)->GetName().c_str()));
			}
			
			// update refs
			(*i)->UpdateReferences(g_controller_interface);
		}
		return true;
	}
	else
	{
		controllers[0]->LoadDefaults(g_controller_interface);
		controllers[0]->UpdateReferences(g_controller_interface);
		return false;
	}	
}

void InputPlugin::SaveConfig()
{
	std::string ini_filename = Core::IsRunning() ? SConfig::GetInstance().m_LocalCoreStartupParameter.m_strGameIni
		: File::GetUserPath(D_CONFIG_IDX) + ini_name + ".ini";

	IniFile inifile;
	inifile.Load(ini_filename);

	std::vector< ControllerEmu* >::const_iterator i = controllers.begin(),
		e = controllers.end();
	for ( ; i!=e; ++i )
		(*i)->SaveConfig(inifile.GetOrCreateSection((*i)->GetName().c_str()));
	
	inifile.Save(ini_filename);
}