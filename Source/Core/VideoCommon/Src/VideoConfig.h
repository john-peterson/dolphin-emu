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


// IMPORTANT: UI etc should modify g_Config. Graphics code should read g_ActiveConfig.
// The reason for this is to get rid of race conditions etc when the configuration
// changes in the middle of a frame. This is done by copying g_Config to g_ActiveConfig
// at the start of every frame. Noone should ever change members of g_ActiveConfig 
// directly.

#ifndef _VIDEO_CONFIG_H_
#define _VIDEO_CONFIG_H_

#include "Common.h"
#include "VideoCommon.h"
#include "ControllerEmu.h"

#include <vector>
#include <string>

// Log in two categories, and save three other options in the same byte
#define CONF_LOG			1
#define CONF_PRIMLOG		2
#define CONF_SAVETEXTURES	4
#define CONF_SAVETARGETS	8
#define CONF_SAVESHADERS	16

enum MultisampleMode {
	MULTISAMPLE_OFF,
	MULTISAMPLE_2X,
	MULTISAMPLE_4X,
	MULTISAMPLE_8X,
	MULTISAMPLE_CSAA_8X,
	MULTISAMPLE_CSAA_8XQ,
	MULTISAMPLE_CSAA_16X,
	MULTISAMPLE_CSAA_16XQ,
};

enum AspectMode {
	ASPECT_AUTO = 0,
	ASPECT_FORCE_16_9 = 1,
	ASPECT_FORCE_4_3 = 2,
	ASPECT_STRETCH = 3,
};

class IniFile;

// NEVER inherit from this class.
struct VideoConfig
{
	enum
	{
		INPUT_AR,
		INPUT_FPS,
		INPUT_EFB_ACCESS,
		INPUT_EFB_COPY,
		INPUT_EFB_SCALE,
		INPUT_XFB,
		INPUT_FOG,
		INPUT_LIGHTING,
		INPUT_WIREFRAME,
		INPUT_SIZE,
	};

	VideoConfig();
	void Load(const char *ini_file);
	void GameIniLoad(const char *ini_file);
	void VerifyValidity();
	void Save(const char *ini_file);
	void GameIniSave(const char* default_ini, const char* game_ini);
	void UpdateProjectionHack();

	// General
	std::vector<ControllerEmu::ControlGroup::Control*> controls;
	bool bVSync;

	bool bRunning;
	bool bWidescreenHack;
	int iAspectRatio;
	bool bCrop;   // Aspect ratio controls.
	bool bUseXFB;
	bool bUseRealXFB;
	bool bUseNativeMips;

	// OpenCL/OpenMP
	bool bEnableOpenCL;
	bool bOMPDecoder;

	// Enhancements
	int iMultisampleMode;
	int iEFBScale;
	bool bForceFiltering;
	int iMaxAnisotropy;
	std::string sPostProcessingShader;

	// Information
	bool bShowFPS;
	bool bShowInputDisplay;
	bool bOverlayStats;
	bool bOverlayProjStats;
	bool bTexFmtOverlayEnable;
	bool bTexFmtOverlayCenter;
	bool bShowEFBCopyRegions;
	
	// Render
	bool bWireFrame;
	bool bDisableLighting;
	bool bDisableTexturing;
	bool bDstAlphaPass;
	bool bDisableFog;
	
	// Utility
	bool bDumpTextures;
	bool bHiresTextures;
	bool bDumpEFBTarget;
	bool bDumpFrames;
	bool bUseFFV1;
	bool bFreeLook;
	bool bAnaglyphStereo;
	int iAnaglyphStereoSeparation;
	int iAnaglyphFocalAngle;
	bool b3DVision;
	
	// Hacks
	bool bEFBAccessEnable;
	bool bDlistCachingEnable;

	bool bEFBCopyEnable;
	bool bEFBCopyCacheEnable;
	bool bEFBEmulateFormatChanges;
	bool bHotKey;
	bool bCopyEFBToTexture;	
	bool bCopyEFBScaled;
	bool bSafeTextureCache;
	int iSafeTextureCache_ColorSamples;
	int iPhackvalue[4];
	std::string sPhackvalue[2];
	float fAspectRatioHackW, fAspectRatioHackH;
	bool bZTPSpeedHack; // The Legend of Zelda: Twilight Princess
	bool bUseBBox;
	bool bEnablePixelLighting;
	bool bEnablePerPixelDepth;

	int iLog; // CONF_ bits
	int iSaveTargetId;
	
	//currently unused:
	int iCompileDLsLevel;

	// D3D only config, mostly to be merged into the above
	int iAdapter;

	// Debugging
	bool bEnableShaderDebugging;

	// Static config per API
	// TODO: Move this out of VideoConfig
	struct
	{
		API_TYPE APIType;

		std::vector<std::string> Adapters; // for D3D9 and D3D11
		std::vector<std::string> AAModes;
		std::vector<std::string> PPShaders; // post-processing shaders

		bool bUseRGBATextures; // used for D3D11 in TextureCache
		bool bSupports3DVision;
		bool bSupportsDualSourceBlend; // only supported by D3D11 and OpenGL
		bool bSupportsFormatReinterpretation;
		bool bSupportsPixelLighting;
	} backend_info;
};

extern VideoConfig g_Config;
extern VideoConfig g_ActiveConfig;

// Called every frame.
void UpdateActiveConfig();

void ComputeDrawRectangle(int backbuffer_width, int backbuffer_height, bool flip, TargetRectangle *rc);

#endif  // _VIDEO_CONFIG_H_
