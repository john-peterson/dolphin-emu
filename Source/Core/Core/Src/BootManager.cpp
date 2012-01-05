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

// File description
// -------------
// Purpose of this file: Collect boot settings for Core::Init()

// Call sequence: This file has one of the first function called when a game is booted,
// the boot sequence in the code is:
  
// DolphinWX:    FrameTools.cpp         StartGame
// Core          BootManager.cpp        BootCore
//               Core.cpp               Init                     Thread creation
//                                      EmuThread                Calls CBoot::BootUp
//               Boot.cpp               CBoot::BootUp()
//                                      CBoot::EmulatedBS2_Wii() / GC() or Load_BS2()

#include <string>
#include <vector>

#include "Common.h"
#include "FileSearch.h"
#include "IniFile.h"
#include "BootManager.h"
#include "Volume.h"
#include "VolumeCreator.h"
#include "Movie.h"
#include "ConfigManager.h"
#include "SysConf.h"
#include "Core.h"
#include "Host.h"
#include "VideoBackendBase.h"

namespace BootManager
{

// TODO this is an ugly hack which allows us to restore values trampled by per-game settings
// Apply fire liberally
struct ConfigCache
{
	bool valid, bCPUThread, bSkipIdle, bEnableFPRF, bMMU, bMMUBAT,
		bVBeam, bFastDiscSpeed, bMergeBlocks, bDSPHLE, bDisableWiimoteSpeaker;
	int iTLBHack;
	std::string m_strVideoBackend;
};
static ConfigCache config_cache;

// Boot the ISO or file
bool BootCore(const std::string& _rFilename)
{
	SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

	// Use custom settings for debugging mode
	Host_SetStartupDebuggingParameters();

	StartUp.m_BootType = SCoreStartupParameter::BOOT_ISO;
	StartUp.m_strFilename = _rFilename;
	if (StartUp.m_strFilename.empty())
	{
		if (!StartUp.m_strDefaultGCM.empty() && File::Exists(StartUp.m_strDefaultGCM))
			StartUp.m_strFilename = StartUp.m_strDefaultGCM;
		else if (!SConfig::GetInstance().m_LastFilename.empty() && File::Exists(SConfig::GetInstance().m_LastFilename))
			StartUp.m_strFilename = SConfig::GetInstance().m_LastFilename;
	}
	if (!StartUp.m_strFilename.empty()) SConfig::GetInstance().m_LastFilename = StartUp.m_strFilename;
	StartUp.bRunCompareClient = false;
	StartUp.bRunCompareServer = false;

	StartUp.hInstance = Host_GetInstance();
	#if defined(_WIN32) && defined(_M_X64)
		StartUp.bUseFastMem = true;
	#endif

	// Demo file
	std::string Extension;
	SplitPath(_rFilename, NULL, NULL, &Extension);
	if (!strcasecmp(Extension.c_str(), ".dtm"))
	{
		if (!File::Exists(_rFilename)) StartUp.m_strFilename = File::GetUserPath(D_STATESAVES_IDX) + _rFilename;

		if (!Movie::PlayInput(StartUp.m_strFilename.c_str())) return false;

		File::IOFile g_recordfd;	
		if (!g_recordfd.Open(StartUp.m_strFilename + ".sav", "rb"))
			return false;
	
		char tmpGameID[7];
		g_recordfd.ReadArray(&tmpGameID, 1);
		tmpGameID[6] = '\0';
		std::string gameID(tmpGameID);

		CFileSearch::XStringVector Directories(SConfig::GetInstance().m_ISOFolder);

		if (SConfig::GetInstance().m_RecursiveISOFolder)
		{
			File::FSTEntry FST_Temp;
			File::ScanDirectoryTreeRecursive(Directories, FST_Temp);
		}

		CFileSearch::XStringVector Extensions;
		Extensions.push_back("*.gcm");
		Extensions.push_back("*.iso");
		Extensions.push_back("*.ciso");
		Extensions.push_back("*.gcz");
		Extensions.push_back("*.wad");

		CFileSearch FileSearch(Extensions, Directories);
		const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();

		if (rFilenames.size() > 0)
		{
			for (u32 i = 0; i < rFilenames.size(); i++)
			{
				DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(rFilenames[i]);
				if (pVolume->GetUniqueID().find(gameID) != std::string::npos)
				{
					StartUp.m_strFilename = rFilenames[i];
					break;
				}
				if (i == rFilenames.size()-1) return false;
			}
		}
	}

	// If for example the ISO file is bad we return here
	if (!StartUp.AutoSetup(SCoreStartupParameter::BOOT_DEFAULT)) return false;

	// Load game specific settings
	IniFile game_ini;
	std::string unique_id = StartUp.GetUniqueID();
	StartUp.m_strGameIni = File::GetUserPath(D_GAMECONFIG_IDX) + unique_id + ".ini";
	if (unique_id.size() == 6 && game_ini.Load(StartUp.m_strGameIni.c_str()))
	{
		config_cache.valid = true;
		config_cache.m_strVideoBackend = StartUp.m_strVideoBackend;
		config_cache.bCPUThread = StartUp.bCPUThread;
		config_cache.bSkipIdle = StartUp.bSkipIdle;
		config_cache.bEnableFPRF = StartUp.bEnableFPRF;
		config_cache.bMMU = StartUp.bMMU;
		config_cache.bMMUBAT = StartUp.bMMUBAT;
		config_cache.iTLBHack = StartUp.iTLBHack;
		config_cache.bVBeam = StartUp.bVBeam;
		config_cache.bFastDiscSpeed = StartUp.bFastDiscSpeed;
		config_cache.bMergeBlocks = StartUp.bMergeBlocks;
		config_cache.bDSPHLE = StartUp.bDSPHLE;
		config_cache.bDisableWiimoteSpeaker = StartUp.bDisableWiimoteSpeaker;

		// General settings		
		game_ini.Get("Core", "GFXBackend",			&StartUp.m_strVideoBackend, StartUp.m_strVideoBackend.c_str());
		game_ini.Get("Core", "CPUThread",			&StartUp.bCPUThread, StartUp.bCPUThread);
		game_ini.Get("Core", "SkipIdle",			&StartUp.bSkipIdle, StartUp.bSkipIdle);
		game_ini.Get("Core", "EnableFPRF",			&StartUp.bEnableFPRF, StartUp.bEnableFPRF);
		game_ini.Get("Core", "MMU",					&StartUp.bMMU, StartUp.bMMU);
		game_ini.Get("Core", "BAT",					&StartUp.bMMUBAT, StartUp.bMMUBAT);
		game_ini.Get("Core", "TLBHack",				&StartUp.iTLBHack, StartUp.iTLBHack);
		game_ini.Get("Core", "VBeam",				&StartUp.bVBeam, StartUp.bVBeam);
		game_ini.Get("Core", "FastDiscSpeed",		&StartUp.bFastDiscSpeed, StartUp.bFastDiscSpeed);
		game_ini.Get("Core", "BlockMerging",		&StartUp.bMergeBlocks, StartUp.bMergeBlocks);
		game_ini.Get("Core", "DSPHLE",				&StartUp.bDSPHLE, StartUp.bDSPHLE);
		game_ini.Get("Wii", "DisableWiimoteSpeaker",&StartUp.bDisableWiimoteSpeaker, StartUp.bDisableWiimoteSpeaker);
		VideoBackend::ActivateBackend(StartUp.m_strVideoBackend);

		// Wii settings
		if (StartUp.bWii)
		{
			// Flush possible changes to SYSCONF to file
			SConfig::GetInstance().m_SYSCONF->Save();
		}
	} 

	// Run the game
	// Init the core
	if (!Core::Init())
	{
		PanicAlertT("Couldn't init the core.\nCheck your configuration.");
		return false;
	}

	return true;
}

void Stop()
{
	Core::Stop();

	SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;

	if (config_cache.valid)
	{
		config_cache.valid = false;
		StartUp.m_strVideoBackend = config_cache.m_strVideoBackend;
		StartUp.bCPUThread = config_cache.bCPUThread;
		StartUp.bSkipIdle = config_cache.bSkipIdle;
		StartUp.bEnableFPRF = config_cache.bEnableFPRF;
		StartUp.bMMU = config_cache.bMMU;
		StartUp.bMMUBAT = config_cache.bMMUBAT;
		StartUp.iTLBHack = config_cache.iTLBHack;
		StartUp.bVBeam = config_cache.bVBeam;
		StartUp.bFastDiscSpeed = config_cache.bFastDiscSpeed;
		StartUp.bMergeBlocks = config_cache.bMergeBlocks;
		StartUp.bDSPHLE = config_cache.bDSPHLE;
		StartUp.bDisableWiimoteSpeaker = config_cache.bDisableWiimoteSpeaker;
		VideoBackend::ActivateBackend(StartUp.m_strVideoBackend);
	}
}

} // namespace
