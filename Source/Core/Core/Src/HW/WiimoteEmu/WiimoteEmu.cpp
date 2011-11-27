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

#include "Attachment/Classic.h"
#include "Attachment/Nunchuk.h"
#include "Attachment/Guitar.h"
#include "Attachment/Drums.h"
#include "Attachment/Turntable.h"

#include "WiimoteEmu.h"
#include "WiimoteHid.h"

#include "../WiimoteReal/WiimoteReal.h"

#include "Timer.h"
#include "Common.h"
#include "../../Host.h"
#include "../../ConfigManager.h"

#include "UDPTLayer.h"

inline double round(double x) { return (x-floor(x))>0.5 ? ceil(x) : floor(x); } //because damn MSVSC doesen't comply to C99

#include "MatrixMath.h"

#include "../../Movie.h"

namespace WiimoteEmu
{

const ReportFeatures reporting_mode_features[] = 
{
    //0x30: Core Buttons
	{ 2, 0, 0, 0, 4 },
    //0x31: Core Buttons and Accelerometer
	{ 2, 4, 0, 0, 7 },
    //0x32: Core Buttons with 8 Extension bytes
	{ 2, 0, 0, 4, 12 },
    //0x33: Core Buttons and Accelerometer with 12 IR bytes
	{ 2, 4, 7, 0, 19 },
    //0x34: Core Buttons with 19 Extension bytes
	{ 2, 0, 0, 4, 23 },
    //0x35: Core Buttons and Accelerometer with 16 Extension Bytes
	{ 2, 4, 0, 7, 23 },
    //0x36: Core Buttons with 10 IR bytes and 9 Extension Bytes
	{ 2, 0, 4, 14, 23 },
    //0x37: Core Buttons and Accelerometer with 10 IR bytes and 6 Extension Bytes
	{ 2, 4, 7, 17, 23 },

	// UNSUPPORTED:
    //0x3d: 21 Extension Bytes
	{ 0, 0, 0, 2, 23 },
    //0x3e / 0x3f: Interleaved Core Buttons and Accelerometer with 36 IR bytes
	{ 0, 0, 0, 0, 23 },
};

void EmulateShake( AccelData* const accel
	  , ControllerEmu::Buttons* const buttons_group
	  , u8* const shake_step )
{
	static const double shake_data[] = { -2.5f, -5.0f, -2.5f, 0.0f, 2.5f, 5.0f, 2.5f, 0.0f };
	static const unsigned int btns[] = { 0x01, 0x02, 0x04 };
	unsigned int shake = 0;

	buttons_group->GetState( &shake, btns );
	for ( unsigned int i=0; i<3; ++i )
		if (shake & (1 << i))
		{
			(&(accel->x))[i] = shake_data[shake_step[i]++];
			shake_step[i] %= sizeof(shake_data)/sizeof(double);
		}
		else
			shake_step[i] = 0;
}

void EmulateTilt(AccelData* const accel
	, ControllerEmu::Tilt* const tilt_group
	, const bool focus, const bool sideways, const bool upright)
{
	float roll, pitch, yaw;
	tilt_group->GetState( &roll, &pitch, &yaw, 0, focus ? (PI / 2) * tilt_group->settings[T_ACC_RANGE]->value : 0 ); // 90 degrees

	unsigned int	ud = 0, lr = 0, fb = 0;

	// some notes that no one will understand but me :p
	// left, forward, up
	// lr/ left == negative for all orientations
	// ud/ up == negative for upright longways
	// fb/ forward == positive for (sideways flat)

	// determine which axis is which direction
	ud = upright ? (sideways ? 0 : 1) : 2;
	lr = sideways;
	fb = upright ? 2 : (sideways ? 0 : 1);

	int sgn[3]={-1,1,1}; //sign fix

	if (sideways && !upright)
		sgn[fb] *= -1;
	if (!sideways && upright)
		sgn[ud] *= -1;

	(&accel->x)[ud] = (sin((PI / 2) - std::max(fabsf(roll), fabsf(pitch))))*sgn[ud];
	(&accel->x)[lr] = -sin(roll)*sgn[lr];
	(&accel->x)[fb] = sin(pitch)*sgn[fb];
}

#define SWING_INTENSITY		5.0f//-uncalibrated(aprox) 0x40-calibrated

void EmulateSwing(AccelData* const accel
	, ControllerEmu::Force* const swing_group
	, const bool sideways, const bool upright)
{
	float swing[3];
	swing_group->GetState(swing, 0, SWING_INTENSITY);

	s8 g_dir[3] = {-1, -1, -1};
	u8 axis_map[3];

	// determine which axis is which direction
	axis_map[0] = upright ? 2 : (sideways ? 0 : 1);	// forward/backward
	axis_map[1] = sideways;	// left|right
	axis_map[2] = upright ? (sideways ? 0 : 1) : 2;	// up/down

	// some orientations have up as positive, some as negative
	// same with forward
	if (sideways && !upright)
		g_dir[axis_map[2]] *= -1;
	if (!sideways && upright)
		g_dir[axis_map[0]] *= -1;

	for (unsigned int i=0; i<3; ++i)
		(&accel->x)[axis_map[i]] += swing[i] * g_dir[i];
	//SERROR_LOG(CONSOLE, "%0.2f %0.2f %0.2f", swing[0]*g_dir[0], swing[1]*g_dir[1], swing[2]*g_dir[2]);
}

const u16 button_bitmasks[] =
{
	Wiimote::BUTTON_A,
	Wiimote::BUTTON_B,
	Wiimote::BUTTON_ONE,
	Wiimote::BUTTON_TWO,
	Wiimote::BUTTON_MINUS,
	Wiimote::BUTTON_PLUS,
	Wiimote::BUTTON_HOME
};

const u16 dpad_bitmasks[] =
{
	Wiimote::PAD_UP, Wiimote::PAD_DOWN, Wiimote::PAD_LEFT, Wiimote::PAD_RIGHT
};
const u16 dpad_sideways_bitmasks[] =
{
	Wiimote::PAD_RIGHT, Wiimote::PAD_LEFT, Wiimote::PAD_UP, Wiimote::PAD_DOWN
};

const char* const named_buttons[] =
{
	"A", "B", "1", "2", "-", "+", "Home",
};

bool Wiimote::GetMotionPlusAttached() const
{
	return m_options->settings[SETTING_MOTIONPLUS]->value != 0;
}

bool Wiimote::GetMotionPlusActive() const
{
	return m_reg_motion_plus.ext_identifier[2] == 0xa4;
}

void Wiimote::Reset()
{
	m_reporting_mode = WM_REPORT_CORE;
	// i think these two are good
	m_reporting_channel = 0;
	m_reporting_auto = false;

	m_rumble_on = false;
	m_speaker_mute = false;
	mp_passthrough = false;
	// will make the first Update() call send a status request
	// the first call to RequestStatus() will then set up the status struct extension bit
	m_extension->active_extension = -1;

	// eeprom
	memset(m_eeprom, 0, sizeof(m_eeprom));
	// calibration data
	memcpy(m_eeprom, eeprom_data_0, sizeof(eeprom_data_0));
	// dunno what this is for, copied from old plugin
	memcpy(m_eeprom + 0x16D0, eeprom_data_16D0, sizeof(eeprom_data_16D0));

	// set up the register
	memset(&m_reg_speaker, 0, sizeof(m_reg_speaker));
	memset(&m_reg_ir, 0, sizeof(m_reg_ir));
	memset(&m_reg_ext, 0, sizeof(m_reg_ext));
	memset(&m_reg_motion_plus, 0, sizeof(m_reg_motion_plus));

	memcpy(&m_reg_motion_plus.calibration, motion_plus_calibration, sizeof(motion_plus_calibration));
	memcpy(&m_reg_motion_plus.gyro_calib, motionplus_accel_gyro_syncing, sizeof(motionplus_accel_gyro_syncing));
	memcpy(&m_reg_motion_plus.ext_identifier, motion_plus_id, sizeof(motion_plus_id));

	// status
	memset(&m_status, 0, sizeof(m_status));
	// Battery levels in voltage
	//   0x00 - 0x32: level 1
	//   0x33 - 0x43: level 2
	//   0x33 - 0x54: level 3
	//   0x55 - 0xff: level 4
	m_status.battery = 0x5f;

	memset(m_shake_step, 0, sizeof(m_shake_step));

	// clear read request queue
	while (m_read_requests.size())
	{
		delete[] m_read_requests.front().data;
		m_read_requests.pop();
	}
}

Wiimote::Wiimote( const unsigned int index )
	: m_index(index)
	, ir_sin(0)
	, ir_cos(1)
//	, m_sound_stream( NULL )
{
	// ---- set up all the controls ----

	// buttons
	groups.push_back(m_buttons = new Buttons("Buttons"));
	for (unsigned int i=0; i < sizeof(named_buttons)/sizeof(*named_buttons); ++i)
		m_buttons->controls.push_back(new ControlGroup::Input( named_buttons[i]));

	// ir
	groups.push_back(m_ir = new Cursor(_trans("IR")));

	// swing
	groups.push_back(m_swing = new Force(_trans("Thrust")));

	// tilt
	groups.push_back(m_tilt = new Tilt(_trans("Tilt"), true));

	// udp 
	groups.push_back(m_udp = new UDPWrapper(m_index, _trans("UDP Wiimote")));

	// shake
	groups.push_back(m_shake = new Buttons(_trans("Shake"), true));
	m_shake->controls.push_back(new ControlGroup::Input("X"));
	m_shake->controls.push_back(new ControlGroup::Input("Y"));
	m_shake->controls.push_back(new ControlGroup::Input("Z"));	

	// extension
	groups.push_back(m_extension = new Extension(_trans("Extension")));
	m_extension->attachments.push_back(new WiimoteEmu::None());
	m_extension->attachments.push_back(new WiimoteEmu::Nunchuk(m_udp));
	m_extension->attachments.push_back(new WiimoteEmu::Classic());
	m_extension->attachments.push_back(new WiimoteEmu::Guitar());
	m_extension->attachments.push_back(new WiimoteEmu::Drums());
	m_extension->attachments.push_back(new WiimoteEmu::Turntable());

	m_extension->settings.push_back(new ControlGroup::Setting(_trans("Motion Plus"), 0, 0, 1));

	// rumble
	groups.push_back(m_rumble = new ControlGroup(_trans("Rumble")));
	m_rumble->controls.push_back(new ControlGroup::Output(_trans("Motor")));

	// dpad
	groups.push_back(m_dpad = new Buttons("D-Pad"));
	for (unsigned int i=0; i < 4; ++i)
		m_dpad->controls.push_back(new ControlGroup::Input(named_directions[i]));

	// options
	groups.push_back( m_options = new ControlGroup(_trans("Options")));
	m_options->settings.push_back(new ControlGroup::Setting(_trans("Background Input"), false));
	m_options->settings.push_back(new ControlGroup::Setting(_trans("Sideways Wiimote"), false));
	m_options->settings.push_back(new ControlGroup::Setting(_trans("Upright Wiimote"), false));
	m_options->settings.push_back(new ControlGroup::Setting(_trans("MotionPlus"), true));
	m_options->settings.push_back(new ControlGroup::Setting(_trans("Hide IR"), false));

	// TODO: This value should probably be re-read if SYSCONF gets changed
	m_sensor_bar_on_top = (bool)SConfig::GetInstance().m_SYSCONF->GetData<u8>("BT.BAR");

	// --- reset eeprom/register/values to default ---
	Reset();
}

std::string Wiimote::GetName() const
{
	return std::string("Wiimote") + char('1'+m_index);
}

// if windows is focused or background input is enabled
#define HAS_FOCUS	(Host_RendererHasFocus() || (m_options->settings[0]->value != 0))

bool Wiimote::Step()
{
	const bool has_focus = HAS_FOCUS;
	const bool is_sideways = m_options->settings[1]->value != 0;

	// no rumble if no focus
	if (false == has_focus)
		m_rumble_on = false;

	m_rumble->controls[0]->control_ref->State(m_rumble_on);

	// update buttons in status struct
	memset(&m_status.buttons, 0, sizeof(m_status.buttons));
	if (has_focus)
	{
		m_buttons->GetState((u16*)&m_status.buttons, button_bitmasks);
		m_dpad->GetState((u16*)&m_status.buttons, is_sideways ? dpad_sideways_bitmasks : dpad_bitmasks);
		UDPTLayer::GetButtons(m_udp, (u16*)&m_status.buttons);
	}

	// check if there is a read data request
	if (m_read_requests.size())
	{
		ReadRequest& rr = m_read_requests.front();
		// send up to 16 bytes to the wii
		SendReadDataReply(rr);
		//SendReadDataReply(rr.channel, rr);

		// if there is no more data, remove from queue
		if (0 == rr.size)
		{
			delete[] rr.data;
			m_read_requests.pop();
		}

		// dont send any other reports
		return true;
	}

	// check if a status report needs to be sent
	// this happens on wiimote sync and when extensions are switched
	if (m_extension->active_extension != m_extension->switch_extension)
	{
		RequestStatus();

		// Wiibrew: Following a connection or disconnection event on the Extension Port,
		// data reporting is disabled and the Data Reporting Mode must be reset before new data can arrive.
		// after a game receives an unrequested status report,
		// it expects data reports to stop until it sets the reporting mode again
		m_reporting_auto = false;

		return true;
	}
	// m+ switch
	if (GetMotionPlusActive() && !GetMotionPlusAttached()) {
		RequestStatus(NULL, 0);
		if (m_extension->active_extension != EXT_NONE)
			RequestStatus();
	}

	return false;
}

void Wiimote::GetCoreData(u8* const data)
{
	memcpy(data, &m_status.buttons, sizeof(m_status.buttons));
}

void Wiimote::GetAccelData(u8* const data, u8* const buttons)
{
	const bool has_focus = HAS_FOCUS;
	const bool is_sideways = m_options->settings[1]->value != 0;
	const bool is_upright = m_options->settings[2]->value != 0;

	// ----TILT----
	EmulateTilt(&m_accel, m_tilt, has_focus, is_sideways, is_upright);

	// ----SWING----
	// ----SHAKE----
	if (has_focus)
	{
		EmulateSwing(&m_accel, m_swing, is_sideways, is_upright);
		EmulateShake(&m_accel, m_shake, m_shake_step);
		UDPTLayer::GetAcceleration(m_udp, &m_accel);
	}
	wm_accel* dt = (wm_accel*)data;
	accel_cal* calib = (accel_cal*)&m_eeprom[0x16];
	double cx,cy,cz;
	cx=trim(m_accel.x*(calib->one_g.x-calib->zero_g.x)+calib->zero_g.x);
	cy=trim(m_accel.y*(calib->one_g.y-calib->zero_g.y)+calib->zero_g.y);
	cz=trim(m_accel.z*(calib->one_g.z-calib->zero_g.z)+calib->zero_g.z);
	dt->x=u8(cx);
	dt->y=u8(cy);
	dt->z=u8(cz);
	if (buttons)
	{
		buttons[0]|=(u8(cx*4)&3)<<5;
		buttons[1]|=((u8(cy*2)&1)<<5)|((u8(cz*2)&1)<<6);
	}
	//SWARN_LOG(CONSOLE, "%02x %02x %02x", dt->x, dt->y, dt->z);
}
#define kCutoffFreq 5.0f
inline void LowPassFilter(double & var, double newval, double period)
{
	double RC=1.0/kCutoffFreq;
	double alpha=period/(period+RC);
	var = newval * alpha + var * (1.0 - alpha);
}

void Wiimote::GetIRData(u8* const data, bool use_accel)
{
	if (m_options->settings[SETTING_IR_HIDE]->value != 0) { memset(data, 0xFF, 10); return; }

	const bool has_focus = HAS_FOCUS;

	u16 x[4], y[4];
	memset(x, 0xFF, sizeof(x));

	if (has_focus)
	{
		float xx = 10000, yy = 0, zz = 0;
		double nsin,ncos;
		
		if (use_accel)
		{
			double ax,az,len;
			ax=m_accel.x;
			az=m_accel.z;
			len=sqrt(ax*ax+az*az);
			if (len)
			{
				ax/=len; 
				az/=len; //normalizing the vector
				nsin=ax;
				ncos=az;
			} else
			{
				nsin=0;
				ncos=1;
			}
		//	PanicAlert("%d %d %d\nx:%f\nz:%f\nsin:%f\ncos:%f",accel->x,accel->y,accel->z,ax,az,sin,cos);
			//PanicAlert("%d %d %d\n%d %d %d\n%d %d %d",accel->x,accel->y,accel->z,calib->zero_g.x,calib->zero_g.y,calib->zero_g.z,
			//	calib->one_g.x,calib->one_g.y,calib->one_g.z);
		} else
		{
			nsin=0; //m_tilt stuff here (can't figure it out yet....)
			ncos=1;
		}

		LowPassFilter(ir_sin,nsin,1.0f/60);
		LowPassFilter(ir_cos,ncos,1.0f/60);

		//SERROR_LOG(CONSOLE, "IR:GetState()");
		m_ir->GetState(&xx, &yy, &zz, true);
		UDPTLayer::GetIR(m_udp, &xx, &yy, &zz);

		Vertex v[4];
		
		static const int camWidth=1024;
		static const int camHeight=768;
		static const double bndup=-0.315447;	
		static const double bnddown=0.85;	
		static const double bndleft=0.443364;		
		static const double bndright=-0.443364;	
		static const double dist1=100.f/camWidth; //this seems the optimal distance for zelda
		static const double dist2=1.2f*dist1;

		for (int i=0; i<4; i++)
		{
			v[i].x=xx*(bndright-bndleft)/2+(bndleft+bndright)/2;
			if (m_sensor_bar_on_top) v[i].y=yy*(bndup-bnddown)/2+(bndup+bnddown)/2;
			else v[i].y=yy*(bndup-bnddown)/2-(bndup+bnddown)/2;
			v[i].z=0;
		}

		v[0].x-=(zz*0.5+1)*dist1;
		v[1].x+=(zz*0.5+1)*dist1;
		v[2].x-=(zz*0.5+1)*dist2;
		v[3].x+=(zz*0.5+1)*dist2;

#define printmatrix(m) PanicAlert("%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f\n",m[0][0],m[0][1],m[0][2],m[0][3],m[1][0],m[1][1],m[1][2],m[1][3],m[2][0],m[2][1],m[2][2],m[2][3],m[3][0],m[3][1],m[3][2],m[3][3])
		Matrix rot,tot;
		static Matrix scale;
		static bool isscale=false;
		if (!isscale)
		{
			MatrixScale(scale,1,camWidth/camHeight,1);
			//MatrixIdentity(scale);
		}
		MatrixRotationByZ(rot,ir_sin,ir_cos);
		//MatrixIdentity(rot);
		MatrixMultiply(tot,scale,rot);

		for (int i=0; i<4; i++)
		{
			MatrixTransformVertex(tot,v[i]);
			if ((v[i].x<-1)||(v[i].x>1)||(v[i].y<-1)||(v[i].y>1))
				continue;
			x[i]=(u16)round((v[i].x+1)/2*(camWidth-1));
			y[i]=(u16)round((v[i].y+1)/2*(camHeight-1));
		}
	//	PanicAlert("%f %f\n%f %f\n%f %f\n%f %f\n%d %d\n%d %d\n%d %d\n%d %d",
	//		v[0].x,v[0].y,v[1].x,v[1].y,v[2].x,v[2].y,v[3].x,v[3].y,
	//		x[0],y[0],x[1],y[1],x[2],y[2],x[3],y[38]);
	}
	// Fill report with valid data when full handshake was done
	if (m_reg_ir.data[0x30])
	// ir mode
	switch (m_reg_ir.mode)
	{
	// basic
	case 1 :
		{
		memset(data, 0xFF, 10);
		wm_ir_basic* const irdata = (wm_ir_basic*)data;
		for (unsigned int i=0; i<2; ++i)
		{
			if (x[i*2] < 1024 && y[i*2] < 768) 
			{
				irdata[i].x1 = u8(x[i*2]);
				irdata[i].x1hi = x[i*2] >> 8;

				irdata[i].y1 = u8(y[i*2]);
				irdata[i].y1hi = y[i*2] >> 8;
			}
			if (x[i*2+1] < 1024 && y[i*2+1] < 768)
			{
				irdata[i].x2 = u8(x[i*2+1]);
				irdata[i].x2hi = x[i*2+1] >> 8;

				irdata[i].y2 = u8(y[i*2+1]);
				irdata[i].y2hi = y[i*2+1] >> 8;
			}
		}
		}
		break;
	// extended
	case 3 :
		{
		memset(data, 0xFF, 12);
		wm_ir_extended* const irdata = (wm_ir_extended*)data;
		for (unsigned int i=0; i<4; ++i)
			if (x[i] < 1024 && y[i] < 768)
			{
				irdata[i].x = u8(x[i]);
				irdata[i].xhi = x[i] >> 8;

				irdata[i].y = u8(y[i]);
				irdata[i].yhi = y[i] >> 8;

				irdata[i].size = 10;
			}
		}
		break;
	// full
	case 5 :
		PanicAlert("Full IR report");
		// UNSUPPORTED
		break;
	}
}

void Wiimote::GetExtData(u8* const data)
{
	// motionplus pass-through modes
	if (GetMotionPlusActive())
	{
		if(mp_passthrough && m_extension->active_extension != EXT_NONE) {
			m_extension->GetState(data, HAS_FOCUS);
			memcpy(m_reg_ext.controller_data, data, sizeof(wm_nc));
			//static u8 _data[6] = {0x00, 0x80, 0x80, 0x80, 0xb3, 0x8c};
			switch (m_reg_motion_plus.ext_identifier[0x4])
			{
			// nunchuck pass-through mode
			// Bit 7 of byte 5 is moved to bit 6 of byte 5, overwriting it
			// Bit 0 of byte 4 is moved to bit 7 of byte 5
			// Bit 3 of byte 5 is moved to bit 4 of byte 5, overwriting it
			// Bit 1 of byte 5 is moved to bit 3 of byte 5
			// Bit 0 of byte 5 is moved to bit 2 of byte 5, overwriting it 
			case 0x5:
				//data[5] & (1 << 7);
				//data[4] & (1 << 0);
				//data[5] & (1 << 3);
				//data[5] & (1 << 1);
				//data[5] & (1 << 0);
				((wm_nc_mp*)data)->jx = ((wm_nc*)data)->jx;
				((wm_nc_mp*)data)->jy = ((wm_nc*)data)->jy;
				((wm_nc_mp*)data)->ax = ((wm_nc*)data)->ax;
				((wm_nc_mp*)data)->ay = ((wm_nc*)data)->ay;
				((wm_nc_mp*)data)->az = ((wm_nc*)data)->az>>1;
				((wm_nc_mp*)data)->azL = ((wm_nc*)data)->az&1;
				((wm_nc_mp*)data)->bz = ((wm_nc_bt*)&((wm_nc*)data)->bt)->z;
				((wm_nc_mp*)data)->bc = ((wm_nc_bt*)&((wm_nc*)data)->bt)->c;
				((wm_nc_mp*)data)->is_mp_data = 0;
				((wm_nc_mp*)data)->extension_connected = 1;
				((wm_nc_mp*)data)->dummy = 0;
				//memcpy(data, _data, 6);
				//SERROR_LOG(CONSOLE, "Ext: %s", ArrayToString(data, 6).c_str());
				break;
			// classic controller/musical instrument pass-through mode
			// Bit 0 of Byte 4 is overwritten
			// Bits 0 and 1 of Byte 5 are moved to bit 0 of Bytes 0 and 1, overwriting
			case 0x7:
				//data[4] & (1 << 0)
				//data[5] & (1 << 0)
				//data[5] & (1 << 1)
				break;
			// unknown pass-through mode
			default:
				break;
			}
		} else {
			memset(data, 0, sizeof(wm_motionplus));
			const bool is_sideways = m_options->settings[1]->value != 0;
			const bool is_upright = m_options->settings[2]->value != 0;			
			AccelData sh;
			memset(&sh, 0, sizeof(sh));
			static u8 m_shake_step_mp[3];
			static float sw[3] = {0};
			static float sw_state[3] = {0};
			float ty = 0, tp = 0, tr = 0;
			static float dx = 0, dy = 0, dz = 0;
			static u16 y, p, r;
			u8 y1,y2, p1,p2, r1,r2;
			if (HAS_FOCUS)
			{
				// shake
				EmulateShake(&sh, m_shake, m_shake_step_mp);				
				for (unsigned int i=0; i<3; ++i)
					(&(sh.x))[i] *= m_shake->settings[B_RANGE]->value;
				// swing
				//m_swing->GetState(sw, sw_state, 0, 1);
				// tilt
				m_tilt->GetState(&tr, &tp, &ty, 0, m_tilt->settings[T_GYRO_RANGE]->value, false);
				// cursor
				m_ir->GetState(&dx, &dy, &dz, true, true);
			}
			// TODO: use m+ calibration?
			//accel_cal* calib = (accel_cal*)&m_eeprom[0x16];
			//double y,r,p;
			//r=trim(m_accel.x); //*(calib->one_g.x-calib->zero_g.x)+calib->zero_g.x);
			//p=trim(m_accel.y); //*(calib->one_g.y-calib->zero_g.y)+calib->zero_g.y);
			//y=trim(m_accel.z); //*(calib->one_g.z-calib->zero_g.z)+calib->zero_g.z);

			// gyro controls
			p = trim14(dy*0x7f	-tp*0x1fff	+sh.y*0.5*0x1fff	+0x1f7f); // -sw[2]*0x1fff
			r = trim14(-dz*0x7f	-tr*0x1fff	+sh.z*0.5*0x1fff	+0x1f7f); // -sw[0]*0x1fff
			y = trim14(-dx*0x7f	-ty*0x1fff	+sh.x*0.5*0x1fff	+0x1f7f); // -sw[1]*0x1fff
			((wm_motionplus*)data)->pitch1 = p&0xff; ((wm_motionplus*)data)->pitch2 = ((p>>8)&0x3f);
			((wm_motionplus*)data)->roll1 = r&0xff; ((wm_motionplus*)data)->roll2 = ((r>>8)&0x3f);
			((wm_motionplus*)data)->yaw1 = y&0xff; ((wm_motionplus*)data)->yaw2 = ((y>>8)&0x3f);

			// fast or slow
			if ((ty==0 && tp==0 && tr==0) && (sw[1]==0 && sw[2]==0 && sw[0]==0) && (sh.x==0 && sh.y==0 && sh.z==0) && (abs(dx) < 100 && abs(dy) < 100)) {				
				((wm_motionplus*)data)->pitch_slow = 1;
				((wm_motionplus*)data)->roll_slow = 1;
				((wm_motionplus*)data)->yaw_slow = 1;
			}	
			// control bits			
			((wm_motionplus*)data)->is_mp_data = 1;
			((wm_motionplus*)data)->extension_connected = (m_extension->active_extension != EXT_NONE) ? 1 : 0;
			((wm_motionplus*)data)->dummy = 0;

			// logging
			float mx = 0, my = 0, mz = 0;
			if (m_options->settings[SETTING_IR_HIDE]->value == 0)  m_ir->GetState(&mx, &my, &mz, true);
			accel_cal* calib = (accel_cal*)&m_eeprom[0x16];
			AccelData n_accel;
			memset(&n_accel, 0, sizeof(n_accel));
			if(m_extension->active_extension > 0) n_accel = ((WiimoteEmu::Nunchuk*)m_extension->attachments[m_extension->active_extension])->m_accel;
			accel_cal* n_calib = (accel_cal*)&((WiimoteEmu::Nunchuk*)m_extension->attachments[m_extension->active_extension])->reg[0x20];			
			SNOTICE_LOG(CONSOLE, ""
			"%0.2f %0.2f"
			" | %02x %02x %02x"
			" | %02x %02x %02x"
			" | %4.2f %4.2f"
			//" | %0.2f %0.2f %0.2f"
			//" | %0.2f %0.2f %0.2f"
			" | %04x %04x %04x",
			//" (%02x %02x %02x %02x %02x %02x)",
				mx, my,					
				(u8)trim(m_accel.x*(calib->one_g.x-calib->zero_g.x)+calib->zero_g.x), (u8)trim(m_accel.y*(calib->one_g.y-calib->zero_g.y)+calib->zero_g.y), (u8)trim(m_accel.z*(calib->one_g.z-calib->zero_g.z)+calib->zero_g.z),
				(u8)trim(n_accel.x*(n_calib->one_g.x-n_calib->zero_g.x)+n_calib->zero_g.x), (u8)trim(n_accel.y*(n_calib->one_g.y-n_calib->zero_g.y)+n_calib->zero_g.y), (u8)trim(n_accel.z*(n_calib->one_g.z-n_calib->zero_g.z)+n_calib->zero_g.z),
				dx, dy,	
				//tp, tr, ty,
				//sw[1], sw[2], sw[0],
				//sh.y, sh.z, sh.x,
				p, r, y
				//((wm_motionplus*)data)->yaw1, ((wm_motionplus*)data)->yaw2, ((wm_motionplus*)data)->pitch1, ((wm_motionplus*)data)->pitch2, ((wm_motionplus*)data)->roll1, ((wm_motionplus*)data)->roll2
				);
		}
		mp_passthrough = !mp_passthrough;
	}
	else {
		m_extension->GetState(data, HAS_FOCUS);
		// i dont think anything accesses the extension data like this, but ill support it. Indeed, commercial games don't do this.
		// i think it should be unencrpyted in the register, encrypted when read.
		memcpy(m_reg_ext.controller_data, data, sizeof(wm_nc));
		if(0xAA == m_reg_ext.encryption)
			wiimote_encrypt(&m_ext_key, data, 0x00, sizeof(wm_nc));
	}
}

void Wiimote::Update()
{
	// no channel == not connected i guess
	if (0 == m_reporting_channel)
		return;

	// returns true if a report was sent
	if (Step())
		return;

	u8 data[MAX_PAYLOAD];
	memset(data, 0, sizeof(data));
	
	// figure out what data we need
	s8 rptf_size = MAX_PAYLOAD;

	Movie::SetPolledDevice();

	if (!Movie::IsPlayingInput() || !Movie::PlayWiimote(m_index, data, rptf_size))
	{
		const ReportFeatures& rptf = reporting_mode_features[m_reporting_mode - WM_REPORT_CORE];
		rptf_size = rptf.size;

		data[0] = 0xA1;
		data[1] = m_reporting_mode;
	
		// core buttons
		if (rptf.core)
			GetCoreData(data + rptf.core);
	
		// acceleration
		if (rptf.accel)
			GetAccelData(data + rptf.accel, rptf.core?(data+rptf.core):NULL);
	
		// IR
		if (rptf.ir)
			GetIRData(data + rptf.ir, (rptf.accel != 0)); 
	
		// extension
		if (rptf.ext)
			GetExtData(data + rptf.ext);
	
		// hybrid wiimote stuff
		if (WIIMOTE_SRC_HYBRID == g_wiimote_sources[m_index])
		{
			using namespace WiimoteReal;
	
			std::lock_guard<std::mutex> lk(g_refresh_lock);
			if (g_wiimotes[m_index])
			{
				Report rpt = g_wiimotes[m_index]->ProcessReadQueue();
				const u8 *real_data = rpt.first;
				if (real_data)
				{
					switch (real_data[1])
					{
						// use data reports
					default:
						if (real_data[1] >= WM_REPORT_CORE)
						{
							const ReportFeatures& real_rptf = reporting_mode_features[real_data[1] - WM_REPORT_CORE];
	
							// force same report type from real-wiimote
							if (&real_rptf != &rptf)
								rptf_size = 0;
	
							// core
							// mix real-buttons with emu-buttons in the status struct, and in the report
							if (real_rptf.core && rptf.core)
							{
								memcpy(&m_status.buttons, real_data + real_rptf.core, sizeof(m_status.buttons));
								memcpy(data + rptf.core, &m_status.buttons, sizeof(m_status.buttons));
							}
	
							// accel
							// use real-accel data always i guess
							if (real_rptf.accel && rptf.accel)
								memcpy(data + rptf.accel, real_data + real_rptf.accel, sizeof(wm_accel));
	
							// ir
							// TODO
	
							// ext
							// use real-ext data if an emu-extention isn't chosen
							if (real_rptf.ext && rptf.ext && (0 == m_extension->switch_extension))
								memcpy(data + rptf.ext, real_data + real_rptf.ext, sizeof(wm_nc));
						}
						else if (WM_ACK_DATA != real_data[1] || m_extension->active_extension > 0)
							rptf_size = 0;
						else
							// use real-acks if an emu-extension isn't chosen
							rptf_size = -1;
						break;
	
						// use all status reports, after modification of the extension bit
					case WM_STATUS_REPORT :
						//if (m_extension->switch_extension)
							//((wm_status_report*)(real_data + 2))->extension = (m_extension->active_extension > 0);
						if (m_extension->active_extension)
							((wm_status_report*)(real_data + 2))->extension = 1;
						rptf_size = -1;
						break;
	
						// use all read-data replies
					case WM_READ_DATA_REPLY:
						rptf_size = -1;
						break;
	
					}
	
					// copy over report from real-wiimote
					if (-1 == rptf_size)
					{
						memcpy(data, real_data, rpt.second);
						rptf_size = rpt.second;
					}
	
					if (real_data != g_wiimotes[m_index]->\
						m_last_data_report.first)
						delete[] real_data;
				}
			}
		}
		if (Movie::IsRecordingInput())
		{
			Movie::RecordWiimote(m_index, data, rptf_size);
		}
	}

	// don't send a data report if auto reporting is off
	if (false == m_reporting_auto && data[2] >= WM_REPORT_CORE)
		return;

	// send data report
	if (rptf_size) {
		::Wiimote::Eavesdrop(this, data, rptf_size);
		Core::Callback_WiimoteInterruptChannel(m_index, m_reporting_channel, data, rptf_size);
	}
}

void Wiimote::ControlChannel(const u16 _channelID, const void* _pData, u32 _Size) 
{
	// Check for custom communication
	if (99 == _channelID)
	{
		// wiimote disconnected
		//PanicAlert( "Wiimote Disconnected" );

		// reset eeprom/register/reporting mode
		Reset();
		return;
	}

	// this all good?
	m_reporting_channel = _channelID;

	const hid_packet* const hidp = (hid_packet*)_pData;

	INFO_LOG(WIIMOTE, "Emu ControlChannel (page: %i, type: 0x%02x, param: 0x%02x)", m_index, hidp->type, hidp->param);

	switch (hidp->type)
	{
	case HID_TYPE_HANDSHAKE :
		PanicAlert("HID_TYPE_HANDSHAKE - %s", (hidp->param == HID_PARAM_INPUT) ? "INPUT" : "OUPUT");
		break;

	case HID_TYPE_SET_REPORT :
		if (HID_PARAM_INPUT == hidp->param)
		{
			PanicAlert("HID_TYPE_SET_REPORT - INPUT"); 
		}
		else
		{
			// AyuanX: My experiment shows Control Channel is never used
			// shuffle2: but lwbt uses this, so we'll do what we must :)
			HidOutputReport((wm_report*)hidp->data);

			u8 handshake = HID_HANDSHAKE_SUCCESS;
			Core::Callback_WiimoteInterruptChannel(m_index, _channelID, &handshake, 1);
			::Wiimote::Eavesdrop(this, &handshake, 1);
		}
		break;

	case HID_TYPE_DATA :
		PanicAlert("HID_TYPE_DATA - %s", (hidp->param == HID_PARAM_INPUT) ? "INPUT" : "OUTPUT");
		break;

	default :
		PanicAlert("HidControlChannel: Unknown type %x and param %x", hidp->type, hidp->param);
		break;
	}

}

void Wiimote::InterruptChannel(const u16 _channelID, const void* _pData, u32 _Size)
{
	::Wiimote::Eavesdrop(this, _pData, _Size);

	// this all good?
	m_reporting_channel = _channelID;

	const hid_packet* const hidp = (hid_packet*)_pData;

	switch (hidp->type)
	{
	case HID_TYPE_DATA:
		switch (hidp->param)
		{
		case HID_PARAM_OUTPUT :
			{
				const wm_report* const sr = (wm_report*)hidp->data;

				if (WIIMOTE_SRC_HYBRID == g_wiimote_sources[m_index])
				{
					switch (sr->wm)
					{
						// these two types are handled in RequestStatus() & ReadData()
					case WM_REQUEST_STATUS :
					case WM_READ_DATA :
						break;

					default :
						WiimoteReal::InterruptChannel(m_index, _channelID, _pData, _Size);
						break;
					}

					HidOutputReport(sr, m_extension->switch_extension > 0);
				}
				else
					HidOutputReport(sr);
			}
			break;

		default :
			PanicAlert("HidInput: HID_TYPE_DATA - param 0x%02x", hidp->param);
			break;
		}
		break;

	default:
		PanicAlert("HidInput: Unknown type 0x%02x and param 0x%02x", hidp->type, hidp->param);
		break;
	}
}

void Wiimote::LoadDefaults(const ControllerInterface& ciface)
{
	ControllerEmu::LoadDefaults(ciface);

	#define set_control(group, num, str)	(group)->controls[num]->control_ref->expression = (str)

	// Buttons
#if defined HAVE_X11 && HAVE_X11
	set_control(m_buttons, 0, "Click 1");		// A
	set_control(m_buttons, 1, "Click 3");		// B
#else
	set_control(m_buttons, 0, "Click 0");		// A
	set_control(m_buttons, 1, "Click 1");		// B
#endif
	set_control(m_buttons, 2, "1");		// 1
	set_control(m_buttons, 3, "2");		// 2
	set_control(m_buttons, 4, "Q");		// -
	set_control(m_buttons, 5, "E");		// +

#ifdef _WIN32
	set_control(m_buttons, 6, "RETURN");		// Home
#else
	set_control(m_buttons, 6, "Return");		// Home
#endif

	// Shake
	for (size_t i = 0; i != 3; ++i)
		set_control(m_shake, i, "Click 2");

	// IR
	set_control(m_ir, 0, "Cursor Y-");
	set_control(m_ir, 1, "Cursor Y+");
	set_control(m_ir, 2, "Cursor X-");
	set_control(m_ir, 3, "Cursor X+");

	// DPad
#ifdef _WIN32
	set_control(m_dpad, 0, "UP");			// Up
	set_control(m_dpad, 1, "DOWN");		// Down
	set_control(m_dpad, 2, "LEFT");		// Left
	set_control(m_dpad, 3, "RIGHT");		// Right
#elif __APPLE__
	set_control(m_dpad, 0, "Up Arrow");		// Up
	set_control(m_dpad, 1, "Down Arrow");		// Down
	set_control(m_dpad, 2, "Left Arrow");		// Left
	set_control(m_dpad, 3, "Right Arrow");	// Right
#else
	set_control(m_dpad, 0, "Up");		// Up
	set_control(m_dpad, 1, "Down");		// Down
	set_control(m_dpad, 2, "Left");		// Left
	set_control(m_dpad, 3, "Right");	// Right
#endif

	// ugly stuff
	// enable nunchuk
	m_extension->switch_extension = 1;

	// set nunchuk defaults
	m_extension->attachments[1]->LoadDefaults(ciface);
}

}