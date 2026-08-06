#include "StdAfx.h"

#include "InputAPI.h"
#include "InputCodes.h"
#include "../Platform/Event.h"
#include "../Platform/Clock.h"
#if !defined(BK_INPUT_EVENT_ONLY)
#include <dinput.h>
#endif

#if !defined(BK_INPUT_EVENT_ONLY)
#include <mmsystem.h>
#endif
#include "../UI/UIMessages.h"
#include "../Scene/Scene.h"
extern "C" WINBASEAPI BOOL WINAPI IsDebuggerPresent(void);
#define SAMPLE_BUFFER_SIZE 128
DWORD TIME_DIFF_DBL_CLK = 500;
int AREA_DBL_CLK_CX = 2;
int AREA_DBL_CLK_CY = 2;
const bool CControlKey::ChangeState( const int nNewState, const DWORD time, const int nParam, const CControl *pLastPressedKey )
{
	const bool bNewPressed = ( nNewState & 0x80 ) != 0;
	{
		bPressed = bNewPressed;
		NotifyAllCombos( bNewPressed, time, nParam );
		dwLastRepeatedTime = time;
		nRepeated = 0;
		if ( pDBLCLK ) 
		{
			const CVec2 vPos = GetSingleton<ICursor>()->GetPos();
			if ( bPressed ) 
			{
				if ( (pLastPressedKey == this) && (time <= dwLastTimePressed + TIME_DIFF_DBL_CLK) && 
					   (fabsf(vLastPos.x - vPos.x) < AREA_DBL_CLK_CX) && (fabsf(vLastPos.y - vPos.y) < AREA_DBL_CLK_CY) )
					pDBLCLK->ChangeState( 0x80, time, nParam, 0 );
				dwLastTimePressed = time;
				vLastPos = vPos;
			}
			else
				pDBLCLK->ChangeState( 0, time, nParam, 0 );
		}
	}
	return bPressed;
}
const int CControlKey::GenerateRepeats( const DWORD time, const DWORD delay, const DWORD period )
{
	int nCounter = 0;
	if ( nRepeated == 0 ) 
	{
		if ( dwLastRepeatedTime + delay <= time )
		{
			++nRepeated;
			dwLastRepeatedTime += delay;
			return GenerateRepeats( time, delay, period ) + 1;
		}
	}
	else
	{
		while ( dwLastRepeatedTime + period <= time )
		{
			dwLastRepeatedTime += period;
			++nRepeated;
			++nCounter;
		}
	}
	return nCounter;
}
const bool CControlAxis::ChangeState( const int nNewState, const DWORD time, const int nParam, const CControl *pLastPressedKey )
{
	if ( nAbsPos != nNewState )
	{
		nOffset = ( nNewState - nAbsPos );
		nAbsPos = nNewState;
		bActive = true;
		NotifyAllCombos( true, time, nParam );
		return true;
	}
	return false;
}
const bool CControlAxisR::ChangeState( const int nNewState, const DWORD time, const int nParam, const CControl *pLastPressedKey )
{
	if ( nNewState != nPos )
	{
		nPos = nNewState;
		NotifyAllCombos( nPos != 0, time, nParam );
	}
	return nPos != 0;
}
#if !defined(BK_INPUT_EVENT_ONLY)
struct SNameID
{
	const char *pszName;
	DWORD dwID;
};
static const SNameID kbdKeyMaps[] = 
{
	{ "ESC"					, DIK_ESCAPE			},
	{ "1"						, DIK_1						},
	{ "2"						,	DIK_2						},
	{ "3"						,	DIK_3						},
	{ "4"						,	DIK_4						},
	{ "5"						,	DIK_5						},
	{ "6"						, DIK_6						},
	{ "7"						,	DIK_7						},
	{ "8"						,	DIK_8						},
	{ "9"						,	DIK_9						},
	{ "0"						,	DIK_0						},
	{ "-"						,	DIK_MINUS				},  // - on main keyboard 
	{ "="						,	DIK_EQUALS			},        
	{ "BACKSPACE"		,	DIK_BACK				},  // backspace 
	{ "TAB"					,	DIK_TAB					},
	{ "Q"						,	DIK_Q						},
	{ "W"						,	DIK_W						},
	{ "E"						,	DIK_E						},
	{ "R"						,	DIK_R						},
	{ "T"						,	DIK_T						},
	{ "Y"						,	DIK_Y						},
	{ "U"						,	DIK_U						},
	{ "I"						,	DIK_I						},
	{ "O"						,	DIK_O						},
	{ "P"						,	DIK_P						},
	{ "["						,	DIK_LBRACKET		},
	{ "]"						,	DIK_RBRACKET		},
	{ "ENTER"				,	DIK_RETURN			},  // Enter on main keyboard 
	{ "LCTRL"				,	DIK_LCONTROL		},
	{ "A"						,	DIK_A						},
	{ "S"						,	DIK_S						},
	{ "D"						,	DIK_D						},
	{ "F"						,	DIK_F						},
	{ "G"						,	DIK_G						},
	{ "H"						,	DIK_H						},
	{ "J"						,	DIK_J						},
	{ "K"						,	DIK_K						},
	{ "L"						,	DIK_L						},
	{ ";"						,	DIK_SEMICOLON		},
	{ "'"						,	DIK_APOSTROPHE	},
	{ "`"						,	DIK_GRAVE       },  // accent grave 
	{ "LSHIFT"			,	DIK_LSHIFT      },
	{ "\\"					,	DIK_BACKSLASH   },
	{ "Z"						,	DIK_Z           },
	{ "X"						,	DIK_X           },
	{ "C"						,	DIK_C           },
	{ "V"						,	DIK_V           },
	{ "B"						,	DIK_B           },
	{ "N"						,	DIK_N           },
	{ "M"						,	DIK_M           },
	{ ","						,	DIK_COMMA       },
	{ "."						,	DIK_PERIOD      },  // . on main keyboard 
	{ "/"						,	DIK_SLASH       },  // / on main keyboard 
	{ "RSHIFT"			,	DIK_RSHIFT      },
	{ "NUM_MULTIPLY", DIK_MULTIPLY  },    // * on numeric keypad 
	{ "LALT"				,	DIK_LMENU       },  // left Alt 
	{ "SPACE"				,	DIK_SPACE       },
	{ "CAPITAL"			,	DIK_CAPITAL     },
	{ "F1"					,	DIK_F1          },
	{ "F2"					,	DIK_F2          },
	{ "F3"					,	DIK_F3          },
	{ "F4"					,	DIK_F4          },
	{ "F5"					,	DIK_F5          },
	{ "F6"					,	DIK_F6          },
	{ "F7"					,	DIK_F7          },
	{ "F8"					,	DIK_F8          },
	{ "F9"					,	DIK_F9          },
	{ "F10"					,	DIK_F10         },
	{ "NUM"					,	DIK_NUMLOCK     },
	{ "SCROLL"			,	DIK_SCROLL      },  // Scroll Lock 
	{ "NUM_7"				,	DIK_NUMPAD7     },
	{ "NUM_8"				,	DIK_NUMPAD8     },
	{ "NUM_9"				,	DIK_NUMPAD9     },
	{ "NUM_MINUS"		,	DIK_SUBTRACT    },  // - on numeric keypad 
	{ "NUM_4"				,	DIK_NUMPAD4     },
	{ "NUM_5"				,	DIK_NUMPAD5     },
	{ "NUM_6"				,	DIK_NUMPAD6     },
	{ "NUM_PLUS"		,	DIK_ADD         },  // + on numeric keypad 
	{ "NUM_1"				,	DIK_NUMPAD1     },
	{ "NUM_2"				,	DIK_NUMPAD2     },
	{ "NUM_3"				,	DIK_NUMPAD3     },
	{ "NUM_0"				,	DIK_NUMPAD0     },
	{ "NUM_PERIOD"	, DIK_DECIMAL     },  // . on numeric keypad 
	{ "OEM_102"			,	DIK_OEM_102     },  // < > | on UK/Germany keyboards 
	{ "F11"					,	DIK_F11         },
	{ "F12"					,	DIK_F12         },
	{ "F13"					,	DIK_F13         },  //                     (NEC PC98) 
	{ "F14"					,	DIK_F14         },  //                     (NEC PC98) 
	{ "F15"					,	DIK_F15         },  //                     (NEC PC98) 
	{ "KANA"				,	DIK_KANA        },  // (Japanese keyboard)            
	{ "ABNT_C1"			,	DIK_ABNT_C1     },  // / ? on Portugese (Brazilian) keyboards 
	{ "CONVERT"			,	DIK_CONVERT     },  // (Japanese keyboard)            
	{ "NOCONVERT"		,	DIK_NOCONVERT   },  // (Japanese keyboard)            
	{ "YEN"					,	DIK_YEN         },  // (Japanese keyboard)            
	{ "ABNT_C2"			,	DIK_ABNT_C2     },  // Numpad . on Portugese (Brazilian) keyboards 
	{ "NUM_EQUALS"	, DIK_NUMPADEQUALS},  // = on numeric keypad (NEC PC98) 
	{ "PREV_TRACK"	, DIK_PREVTRACK   },  // Previous Track (DIK_CIRCUMFLEX on Japanese keyboard) 
	{ "AT"					,	DIK_AT          },  //                     (NEC PC98) 
	{ "COLON"				,	DIK_COLON       },  //                     (NEC PC98) 
	{ "UNDERLINE"		,	DIK_UNDERLINE   },  //                     (NEC PC98) 
	{ "KANJI"				,	DIK_KANJI       },  // (Japanese keyboard)            
	{ "STOP"				,	DIK_STOP        },  //                     (NEC PC98) 
	{ "AX"					,	DIK_AX          },  //                     (Japan AX) 
	{ "UNLABELED"		,	DIK_UNLABELED   },  //                     (J3100) 
	{ "NEXT_TRACK"	, DIK_NEXTTRACK   },  // Next Track 
	{ "NUM_ENTER"		,	DIK_NUMPADENTER },  // Enter on numeric keypad 
	{ "RCTRL"				,	DIK_RCONTROL    },
	{ "MUTE"				,	DIK_MUTE        },  // Mute 
	{ "CALCULATOR"	, DIK_CALCULATOR  },  // Calculator 
	{ "PLAY"				,	DIK_PLAYPAUSE   },  // Play / Pause 
	{ "MEDIA_STOP"	, DIK_MEDIASTOP   },  // Media Stop 
	{ "VOL_DOWN"		,	DIK_VOLUMEDOWN  },  // Volume - 
	{ "VOL_UP"			,	DIK_VOLUMEUP    },  // Volume + 
	{ "WEB_HOME"		,	DIK_WEBHOME     },  // Web home 
	{ "NUM_COMMA"		,	DIK_NUMPADCOMMA },  // , on numeric keypad (NEC PC98) 
	{ "NUM_DIVIDE"	, DIK_DIVIDE      },  // / on numeric keypad 
	{ "SYSRQ"				,	DIK_SYSRQ       },
	{ "RALT"				,	DIK_RMENU       },  // right Alt 
	{ "PAUSE"				,	DIK_PAUSE       },  // Pause 
	{ "HOME"				,	DIK_HOME        },  // Home on arrow keypad 
	{ "UP"					,	DIK_UP          },  // UpArrow on arrow keypad 
	{ "PG_UP"				,	DIK_PRIOR       },  // PgUp on arrow keypad 
	{ "LEFT"				,	DIK_LEFT        },  // LeftArrow on arrow keypad 
	{ "RIGHT"				,	DIK_RIGHT       },  // RightArrow on arrow keypad 
	{ "END"					,	DIK_END         },  // End on arrow keypad 
	{ "DOWN"				,	DIK_DOWN        },  // DownArrow on arrow keypad 
	{ "PG_DOWN"			,	DIK_NEXT        },  // PgDn on arrow keypad 
	{ "INSERT"			,	DIK_INSERT      },  // Insert on arrow keypad 
	{ "DELETE"			,	DIK_DELETE      },  // Delete on arrow keypad 
	{ "LWIN"				,	DIK_LWIN        },  // Left Windows key 
	{ "RWIN"				,	DIK_RWIN        },  // Right Windows key 
	{ "APP_MENU"		,	DIK_APPS        },  // AppMenu key 
	{ "POWER"				,	DIK_POWER       },  // System Power 
	{ "SLEEP"				,	DIK_SLEEP       },  // System Sleep 
	{ "WAKE"				,	DIK_WAKE        },  // System Wake 
	{ "WEB_SEARCH"	,	DIK_WEBSEARCH   },  // Web Search 
	{ "WEB_FAVOR"		,	DIK_WEBFAVORITES},  // Web Favorites 
	{ "WEB_REFRESH"	, DIK_WEBREFRESH  },  // Web Refresh 
	{ "WEB_STOP"		,	DIK_WEBSTOP     },  // Web Stop 
	{ "WEB_FORWARD"	, DIK_WEBFORWARD  },  // Web Forward 
	{ "WEB_BACK"		,	DIK_WEBBACK     },  // Web Back 
	{ "MYCOMPUTER"	, DIK_MYCOMPUTER  },  // My Computer 
	{ "MAIL"				,	DIK_MAIL        },  // Mail 
	{ "MEDIA_SELECT",	DIK_MEDIASELECT },  // Media Select 
	{ 0							,								0 }
};
inline const char* GetKeyName( int nID )
{
	if ( nID >= 0 && nID < (sizeof( kbdKeyMaps ) / sizeof( kbdKeyMaps[0] )) )
	{
		if ( kbdKeyMaps[nID].dwID == nID )
			return kbdKeyMaps[nID].pszName;
	}
	for ( const SNameID *pDesc = kbdKeyMaps; pDesc->pszName != 0; ++pDesc )
	{
		if ( pDesc->dwID == nID ) 
			return pDesc->pszName;
	}
	return "UNKNOWN_KEY";
}
struct SDeviceEnumDesc : public SDeviceDesc
{
	bool bPoll;
	std::unordered_map<int, int> cntrltypes;
	std::vector<SControlDesc> controls;
	std::vector<DIOBJECTDATAFORMAT> objects;
};
struct SDevicesEnumDesc
{
	std::vector<SDeviceEnumDesc> devices;
	std::unordered_map<int, int> devtypes;
};
BOOL CALLBACK DIEnumDevicesCallback( LPCDIDEVICEINSTANCE lpddi, LPVOID pvRef )
{
	SDevicesEnumDesc *pDesc = reinterpret_cast<SDevicesEnumDesc*>( pvRef );
	const DIDEVICEINSTANCE &instance = *lpddi;	
	EDeviceType eDeviceType = DEVICE_TYPE_UNKNOWN;
	const char *pszName = "DEVICE";
	switch( GET_DIDEVICE_TYPE(instance.dwDevType) )
	{
		case DI8DEVTYPE_1STPERSON:
			eDeviceType = DEVICE_TYPE_FIRST_PERSON;
			pszName = "FIRST_PERSON";
			break;
		case DI8DEVTYPE_DRIVING:
			eDeviceType = DEVICE_TYPE_DRIVING;
			pszName = "DRIVING";
			break;
		case DI8DEVTYPE_FLIGHT:
			eDeviceType = DEVICE_TYPE_FLIGHT;
			pszName = "FLIGHT";
			break;
		case DI8DEVTYPE_GAMEPAD:
			eDeviceType = DEVICE_TYPE_GAMEPAD;
			pszName = "GAMEPAD";
			break;
		case DI8DEVTYPE_JOYSTICK:
			eDeviceType = DEVICE_TYPE_JOYSTICK;
			pszName = "JOYSTICK";
			break;
		case DI8DEVTYPE_KEYBOARD:
			eDeviceType = DEVICE_TYPE_KEYBOARD;
			pszName = "KEYBOARD";
			break;
		case DI8DEVTYPE_MOUSE:
			eDeviceType = DEVICE_TYPE_MOUSE;
			pszName = "MOUSE";
			break;
	}
	if ( ( (eDeviceType != DEVICE_TYPE_KEYBOARD) && (eDeviceType != DEVICE_TYPE_MOUSE) ) ||
			 ( pDesc->devtypes[eDeviceType] >= 1 ) )
	{
		return DIENUM_CONTINUE;
	}
	SDeviceEnumDesc device;
	device.guid = instance.guidInstance;
	device.eType = eDeviceType;
	device.szFriendlyName = instance.tszInstanceName;
	device.szName = pDesc->devtypes[eDeviceType] > 0 ? NStr::Format( "%s%d", pszName, pDesc->devtypes[eDeviceType] ) : pszName;
	pDesc->devices.push_back( device );
	pDesc->devtypes[eDeviceType]++;
	return DIENUM_CONTINUE;
}
BOOL CALLBACK DIEnumDeviceObjectsCallback( LPCDIDEVICEOBJECTINSTANCE lpddoi, LPVOID pvRef )
{
	const DIDEVICEOBJECTINSTANCE &instance = *lpddoi;
	SDeviceEnumDesc *pDesc = reinterpret_cast<SDeviceEnumDesc*>( pvRef );
	EControlType eControlType = CONTROL_TYPE_UNKNOWN;
	std::string szControlName;
	const GUID *pGUID = 0;
	if ( instance.guidType == GUID_XAxis ) 
	{
		eControlType = CONTROL_TYPE_AXIS;
		szControlName = "AXIS_X";
		pGUID = &GUID_XAxis;
	}
	else if ( instance.guidType == GUID_YAxis ) 
	{
		eControlType = CONTROL_TYPE_AXIS;
		szControlName = "AXIS_Y";
		pGUID = &GUID_YAxis;
	}
	else if ( instance.guidType == GUID_ZAxis ) 
	{
		eControlType = CONTROL_TYPE_AXIS;
		szControlName = "AXIS_Z";
		pGUID = &GUID_ZAxis;
	}
	else if ( instance.guidType == GUID_RxAxis ) 
	{
		eControlType = CONTROL_TYPE_RAXIS;
		szControlName = "AXIS_RX";
		pGUID = &GUID_RxAxis;
	}
	else if ( instance.guidType == GUID_RyAxis ) 
	{
		eControlType = CONTROL_TYPE_RAXIS;
		szControlName = "AXIS_RY";
		pGUID = &GUID_RyAxis;
	}
	else if ( instance.guidType == GUID_RzAxis ) 
	{
		eControlType = CONTROL_TYPE_RAXIS;
		szControlName = "AXIS_RZ";
		pGUID = &GUID_RzAxis;
	}
	else if ( instance.guidType == GUID_Slider ) 
	{
		eControlType = CONTROL_TYPE_SLIDER;
		szControlName = NStr::Format( "SLIDER%d", pDesc->cntrltypes[eControlType] );
		pGUID = &GUID_Slider;
	}
	else if ( instance.guidType == GUID_Button ) 
	{
		eControlType = CONTROL_TYPE_KEY;
		szControlName = NStr::Format( "BUTTON%d", pDesc->cntrltypes[eControlType] );
		pGUID = &GUID_Button;
	}
	else if ( instance.guidType == GUID_Key ) 
	{
		eControlType = CONTROL_TYPE_KEY;
		if ( pDesc->eType == DEVICE_TYPE_KEYBOARD ) 
			szControlName = GetKeyName( instance.dwOfs );
		else
			szControlName = NStr::Format( "KEY%d", pDesc->cntrltypes[eControlType] );
		pGUID = &GUID_Key;
	}
	else if ( instance.guidType == GUID_POV ) 
	{
		eControlType = CONTROL_TYPE_POV;
		szControlName = NStr::Format( "POV%d", pDesc->cntrltypes[eControlType] );
		pGUID = &GUID_POV;
	}
	if ( eControlType == CONTROL_TYPE_UNKNOWN ) 
		return DIENUM_CONTINUE;
	if ( pDesc->eType != DEVICE_TYPE_KEYBOARD ) 
		szControlName = pDesc->szName + "_" + szControlName;
	SControlDesc control;
	control.szFriendlyName = instance.tszName;
	control.szName = szControlName;
	control.eType = eControlType;
	control.nID = instance.dwOfs;
	control.nGranularity = 1;
	pDesc->controls.push_back( control );

	DIOBJECTDATAFORMAT object;
	object.pguid = pGUID;
	object.dwType = instance.dwType;
	object.dwOfs = instance.dwOfs;
	object.dwFlags = 0;
	pDesc->objects.push_back( object );
	pDesc->cntrltypes[eControlType]++;

	NStr::DebugTrace( "\t--- New Control \"%s\" of type %s with offset %d found\n", control.szFriendlyName.c_str(), control.szName.c_str(), object.dwOfs );
	return DIENUM_CONTINUE;
}
#endif
CInputAPI::CInputAPI()
{
	eTextMode = INPUT_TEXT_MODE_NOTEXT;
	nCodePage = 1251;
	bInitialized = false;
	bCoopLevelSet = false;
	bFocusCaptured = false;
	hWindow = 0;
	#if defined(BK_INPUT_EVENT_ONLY)
	dwLastPumpingTime = static_cast<DWORD>( NPlatform::MonotonicMilliseconds() );
	#else
	dwLastPumpingTime = timeGetTime();
	#endif
	pLastControlKey = 0;
	dwRepeatDelay = 500;
	dwRepeatPeriod = 30;
#if defined(BK_INPUT_EVENT_ONLY)
	nextControllerID = 3;
#endif
}
CInputAPI::~CInputAPI()
{
	Done();
}
bool CInputAPI::Init()
{
#if defined(BK_INPUT_EVENT_ONLY)
	// The default runtime owns input through normalized SDL events.  Preserve
	// the legacy device/control IDs by creating two private virtual devices;
	// ConsumePlatformEvent feeds the same control state and binding machinery
	// used by the historical backend.
	devices.clear();
	controlIDs.clear();
	controlNames.clear();
	controllerDeviceIDs.clear();
	nextControllerID = 3;
	SDevice keyboard;
	keyboard.nID = 1;
	keyboard.eType = DEVICE_TYPE_KEYBOARD;
	keyboard.szName = "KEYBOARD";
	keyboard.szFriendlyName = "Keyboard";
	keyboard.bEmulated = true;
	std::size_t keyboardCodeCount = 0;
	const NInput::KeyCodeEntry *keyboardCodes = NInput::KeyboardCodes( &keyboardCodeCount );
	for ( std::size_t i = 0; i != keyboardCodeCount; ++i )
	{
		const NInput::KeyCodeEntry &entry = keyboardCodes[i];
		SControlDesc desc = {};
		desc.eType = CONTROL_TYPE_KEY;
		const int legacyID = static_cast<int>( entry.code );
		desc.nID = ( keyboard.nID << 16 ) | legacyID;
		desc.szName = entry.name;
		desc.szFriendlyName = entry.name;
		desc.nGranularity = 1;
		keyboard.controls.push_back( new CControlKey( desc ) );
		controlIDs[desc.nID] = keyboard.controls.back();
		controlNames[desc.szName] = keyboard.controls.back();
	}
	devices.push_back( std::move( keyboard ) );
	SDevice mouse;
	mouse.nID = 2;
	mouse.eType = DEVICE_TYPE_MOUSE;
	mouse.szName = "MOUSE";
	mouse.szFriendlyName = "Mouse";
	mouse.bEmulated = true;
	const struct { const char *name; int id; EControlType type; } mouseControls[] = {
		{ "AXIS_X", INPUT_CONTROL_MOUSE_AXIS_X, CONTROL_TYPE_AXIS },
		{ "AXIS_Y", INPUT_CONTROL_MOUSE_AXIS_Y, CONTROL_TYPE_AXIS },
		{ "AXIS_Z", INPUT_CONTROL_MOUSE_AXIS_Z, CONTROL_TYPE_AXIS },
		{ "BUTTON0", INPUT_CONTROL_MOUSE_BUTTON0, CONTROL_TYPE_KEY },
		{ "BUTTON1", INPUT_CONTROL_MOUSE_BUTTON1, CONTROL_TYPE_KEY },
		{ "BUTTON2", INPUT_CONTROL_MOUSE_BUTTON2, CONTROL_TYPE_KEY },
	};
	for ( const auto &entry : mouseControls )
	{
		SControlDesc desc = {};
		desc.eType = entry.type;
		desc.nID = ( mouse.nID << 16 ) | entry.id;
		desc.szName = entry.name;
		desc.szFriendlyName = entry.name;
		desc.nGranularity = 1;
		CControl *control = entry.type == CONTROL_TYPE_KEY ? static_cast<CControl *>( new CControlKey( desc ) ) : static_cast<CControl *>( new CControlAxis( desc ) );
		mouse.controls.push_back( control );
		controlIDs[desc.nID] = control;
		controlNames[desc.szName] = control;
	}
	devices.push_back( std::move( mouse ) );
	hWindow = 0;
	bInitialized = true;
	bCoopLevelSet = true;
	bFocusCaptured = false;
	dwLastPumpingTime = static_cast<DWORD>( NPlatform::MonotonicMilliseconds() );
	TIME_DIFF_DBL_CLK = 500;
	dwRepeatDelay = 500;
	dwRepeatPeriod = 30;
	return true;
#else
	{
		IDirectInput8 *pTemp = 0;
		HRESULT dxrval = DirectInput8Create( GetModuleHandle(0), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&pTemp, 0 );
		NI_ASSERTHR_TF( dxrval, "Can't create direct input object", return false );
		pInput = pTemp;
		pTemp->Release();
	}
	SDevicesEnumDesc devdesc;
	pInput->EnumDevices( DI8DEVCLASS_ALL, DIEnumDevicesCallback, &devdesc, DIEDFL_ATTACHEDONLY );
	int nDeviceCounter = 0;
	devices.reserve( devdesc.devices.size() );
	for ( std::vector<SDeviceEnumDesc>::iterator device = devdesc.devices.begin(); device != devdesc.devices.end(); ++device )
		AddDevice( &(*device), ++nDeviceCounter );
	hWindow = 0;
	// SDL owns acquisition and focus.  Keep the legacy device/control model for
	// bindings, but consume all runtime state through platform events.
	for ( CDevicesList::iterator device = devices.begin(); device != devices.end(); ++device )
		device->bEmulated = true;
	bInitialized = true;
	TIME_DIFF_DBL_CLK = GetDoubleClickTime();
	AREA_DBL_CLK_CX = GetSystemMetrics( SM_CXDOUBLECLK );
	AREA_DBL_CLK_CY = GetSystemMetrics( SM_CYDOUBLECLK );
	if ( GetGlobalVar("norepeats", 0) == 0 )
	{
		int nDelay = 0;			// first input repeat delay. [0..3], 0 = 250ms, 3 = 1000ms (1sec)
		DWORD dwSpeed = 0;	// repetition speed [0..31], 0 = 2.5 repetitions per sec, 31 = 30 repetitions per sec
		SystemParametersInfo( SPI_GETKEYBOARDDELAY, 0, &nDelay, 0 );
		SystemParametersInfo( SPI_GETKEYBOARDSPEED, 0, &dwSpeed, 0 );
		dwRepeatDelay = (nDelay + 1) * 250;
		dwRepeatPeriod = 1000.0f / ( 2.5f + (float(dwSpeed) / 31.0f)*( 30.0f - 2.5f ) );
	}
	else
	{
		dwRepeatDelay = 0x7fffffff;
		dwRepeatPeriod = 0x7fffffff;
	}
	return bInitialized;
#endif
}
bool CInputAPI::Done()
{
	bInitialized = false;
	bCoopLevelSet = false;
	bFocusCaptured = false;
	chars.clear();
	controlIDs.clear();
	controlNames.clear();
	devices.clear();
#if defined(BK_INPUT_EVENT_ONLY)
	controllerDeviceIDs.clear();
	nextControllerID = 3;
#endif
#if !defined(BK_INPUT_EVENT_ONLY)
	pInput = 0;
#endif
	hWindow = 0;
	return true;
}
#if !defined(BK_INPUT_EVENT_ONLY)
#if defined(__clang__)
__attribute__((no_sanitize("alignment")))
#endif
void CInputAPI::AddDevice( SDeviceEnumDesc *pDesc, const int nID )
{
	IDirectInputDevice8 *pTempDevice = 0;
	HRESULT dxrval = pInput->CreateDevice( pDesc->guid, &pTempDevice, 0 );
	NI_ASSERTHR_TF( dxrval, "Can't create device \"%s\". Skipping...", return );
	com_ptr<IDirectInputDevice8> pDevice = pTempDevice;
	pTempDevice->Release();
	DIDEVCAPS devcaps;
	Zero( devcaps );
	devcaps.dwSize = sizeof( devcaps );
	dxrval = pDevice->GetCapabilities( &devcaps );
	NI_ASSERTHR_T( dxrval, NStr::Format("Can't get caps from device \"%s\"", pDesc->szFriendlyName.c_str()) );
	if ( FAILED(dxrval) )
		return;
	pDesc->bPoll = ( devcaps.dwFlags & (DIDC_POLLEDDATAFORMAT | DIDC_POLLEDDEVICE) ) ? true : false;
	NStr::DebugTrace( "*** New Input Device \"%s\" of type %s found\n", pDesc->szFriendlyName.c_str(), pDesc->szName.c_str() );
	dxrval = pDevice->EnumObjects( DIEnumDeviceObjectsCallback, pDesc, DIDFT_ALL );
	NI_ASSERTHR_T( dxrval, NStr::Format("Can't enum controls for device \"%s\"", pDesc->szFriendlyName.c_str()) );
	if ( FAILED(dxrval) )
		return;
	DWORD dwDataSize = 0;
	for ( std::vector<DIOBJECTDATAFORMAT>::iterator object = pDesc->objects.begin(); object != pDesc->objects.end(); ++object )
		dwDataSize = Max( dwDataSize, object->dwOfs );
	dwDataSize += 4;
	if ( dwDataSize & 3 ) 
		dwDataSize = ( dwDataSize & (~3UL) ) + 4;
	DIDATAFORMAT format;
	format.dwSize = sizeof( DIDATAFORMAT );
	format.dwObjSize = sizeof( DIOBJECTDATAFORMAT );
	format.dwDataSize = dwDataSize;
	format.dwFlags = DIDF_ABSAXIS;
	format.dwNumObjs = pDesc->objects.size();
	format.rgodf = &( pDesc->objects[0] );
	dxrval = pDevice->SetDataFormat( &format );
	NI_ASSERTHR_T( dxrval, NStr::Format("Can't set data format for device \"%s\"", pDesc->szFriendlyName.c_str()) );
	if ( FAILED(dxrval) )
		return;
	DIPROPDWORD dipdw;
	dipdw.diph.dwSize = sizeof( DIPROPDWORD );
	dipdw.diph.dwHeaderSize = sizeof( DIPROPHEADER );
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = SAMPLE_BUFFER_SIZE;
	dxrval = pDevice->SetProperty( DIPROP_BUFFERSIZE, &dipdw.diph );
	NI_ASSERTHR_T( dxrval, NStr::Format("Can't set sample buffer size (%d) for device \"%s\"", SAMPLE_BUFFER_SIZE, pDesc->szFriendlyName.c_str()) );
	if ( FAILED(dxrval) )
		return;
	for ( int i = 0; i != pDesc->objects.size(); ++i )
	{
		DIOBJECTDATAFORMAT &object = pDesc->objects[i];
		if ( (object.dwType & DIDFT_AXIS) == 0 )
			continue;
		SControlDesc &control = pDesc->controls[i];
		{
			DIPROPRANGE dipRange;
			dipRange.diph.dwSize = sizeof( DIPROPRANGE );
			dipRange.diph.dwHeaderSize = sizeof( DIPROPHEADER );
			dipRange.diph.dwHow = DIPH_BYOFFSET; 
			dipRange.diph.dwObj = object.dwOfs;
			dipRange.lMin = -AXIS_RANGE_VALUE;
			dipRange.lMax = AXIS_RANGE_VALUE;
			pDevice->SetProperty( DIPROP_RANGE, &dipRange.diph );
		}
		{
			DIPROPDWORD prop;
			prop.diph.dwSize = sizeof( DIPROPDWORD );
			prop.diph.dwHeaderSize = sizeof( DIPROPHEADER );
			prop.diph.dwHow = DIPH_BYOFFSET;
			prop.diph.dwObj = object.dwOfs;
			HRESULT dxrval = pDevice->GetProperty( DIPROP_GRANULARITY, (DIPROPHEADER*)&prop );
			control.nGranularity = SUCCEEDED(dxrval) ? prop.dwData : 1;
		}
	}
	devices.push_back( SDevice() );
	SDevice &device = devices.back();
	device.guid = pDesc->guid;
	device.nID = devices.size();
	device.eType = pDesc->eType;
	device.szName = pDesc->szName;
	device.szFriendlyName = pDesc->szFriendlyName;
	device.bPoll = pDesc->bPoll;
	device.pDevice = pDevice;
	device.dwBufferSize = dwDataSize;
	device.controls.reserve( pDesc->controls.size() + pDesc->cntrltypes[CONTROL_TYPE_POV] );
	for ( std::vector<SControlDesc>::const_iterator it = pDesc->controls.begin(); it != pDesc->controls.end(); ++it )
	{
		if ( it->eType == CONTROL_TYPE_POV ) 
		{
			{
				SControlDesc desc = *it;
				desc.szName += "_X";
				desc.nID = ( device.nID << 16 ) | it->nID;
				device.controls.push_back( new CControlAxis( desc ) );
				controlIDs[desc.nID] = device.controls.back();
				controlNames[desc.szName] = device.controls.back();
			}
			{
				SControlDesc desc = *it;
				desc.szName += "_Y";
				desc.nID = ( device.nID << 16 ) | it->nID | 0x8000;
				device.controls.push_back( new CControlAxis( desc ) );
				controlIDs[desc.nID] = device.controls.back();
				controlNames[desc.szName] = device.controls.back();
			}
		}
		else
		{
			SControlDesc desc = *it;
			desc.nID = ( device.nID << 16 ) | it->nID;
			switch ( desc.eType )
			{
				case CONTROL_TYPE_KEY:
					device.controls.push_back( new CControlKey( desc ) );
					break;
				case CONTROL_TYPE_AXIS:
				case CONTROL_TYPE_SLIDER:
					device.controls.push_back( new CControlAxis( desc ) );
					break;
				case CONTROL_TYPE_RAXIS:
					device.controls.push_back( new CControlAxisR( desc ) );
					break;
			}
			controlIDs[desc.nID] = device.controls.back();
			controlNames[desc.szName] = device.controls.back();
		}
	}
}
#endif
SDevice* CInputAPI::GetDevice( const int nID )
{
	for ( CDevicesList::iterator it = devices.begin(); it != devices.end(); ++it )
	{
		if ( it->nID == nID )
			return &( *it );
	}
	return 0;
}
#if defined(BK_INPUT_EVENT_ONLY)
static int NormalizeControllerAxis( const int rawValue, const int axis )
{
	if ( axis >= 4 )
	{
		const int clamped = Max( 0, Min( 32767, rawValue ) );
		return (clamped * AXIS_RANGE_VALUE) / 32767;
	}
	const int clamped = Max( -32768, Min( 32767, rawValue ) );
	const int magnitude = abs( clamped );
	const int deadZone = 8000;
	if ( magnitude <= deadZone ) return 0;
	const int scaled = ((magnitude - deadZone) * AXIS_RANGE_VALUE) / (32767 - deadZone);
	return (clamped < 0) ? -Min( AXIS_RANGE_VALUE, scaled ) : Min( AXIS_RANGE_VALUE, scaled );
}
static int ControllerAxisControlID( const int axis ) { return axis * 4; }
static int ControllerButtonControlID( const int button ) { return 0x100 + button; }
void CInputAPI::HandleControllerAdded( const NPlatform::PlatformEvent &event )
{
	if ( controllerDeviceIDs.find( event.deviceId ) != controllerDeviceIDs.end() ) return;
	int runtimeID = nextControllerID;
	while ( GetDevice( runtimeID ) != 0 ) ++runtimeID;
	nextControllerID = runtimeID + 1;
	SDevice device;
	device.guid.fill( 0 );
	device.nID = runtimeID;
	device.eType = DEVICE_TYPE_GAMEPAD;
	device.bEmulated = true;
	device.szName = "GAMEPAD_" + std::to_string( runtimeID );
	device.szFriendlyName = event.text[0] != 0 ? event.text : "Gamepad";
	static const char *axisNames[] = { "LEFT_X", "LEFT_Y", "RIGHT_X", "RIGHT_Y", "LEFT_TRIGGER", "RIGHT_TRIGGER" };
	for ( int axis = 0; axis != 6; ++axis )
	{
		SControlDesc desc = {};
		desc.eType = CONTROL_TYPE_AXIS;
		desc.nID = ( runtimeID << 16 ) | ControllerAxisControlID( axis );
		desc.szName = device.szName + "_" + axisNames[axis];
		desc.szFriendlyName = desc.szName;
		desc.nGranularity = 1;
		device.controls.push_back( new CControlAxis( desc ) );
		controlIDs[desc.nID] = device.controls.back();
		controlNames[desc.szName] = device.controls.back();
	}
	for ( int button = 0; button != 16; ++button )
	{
		SControlDesc desc = {};
		desc.eType = CONTROL_TYPE_KEY;
		desc.nID = ( runtimeID << 16 ) | ControllerButtonControlID( button );
		desc.szName = device.szName + "_BUTTON" + std::to_string( button );
		desc.szFriendlyName = desc.szName;
		desc.nGranularity = 1;
		device.controls.push_back( new CControlKey( desc ) );
		controlIDs[desc.nID] = device.controls.back();
		controlNames[desc.szName] = device.controls.back();
	}
	devices.push_back( std::move( device ) );
	controllerDeviceIDs[event.deviceId] = runtimeID;
}
void CInputAPI::HandleControllerRemoved( const NPlatform::PlatformEvent &event )
{
	std::unordered_map<int, int>::iterator host = controllerDeviceIDs.find( event.deviceId );
	if ( host == controllerDeviceIDs.end() ) return;
	const int runtimeID = host->second;
	for ( CDevicesList::iterator device = devices.begin(); device != devices.end(); ++device )
	{
		if ( device->nID != runtimeID ) continue;
		for ( std::vector<CControl*>::iterator control = device->controls.begin(); control != device->controls.end(); ++control )
		{
			if ( (*control)->IsActive() )
			{
				(*control)->ChangeState( 0, static_cast<DWORD>( event.timestamp ), 0, pLastControlKey );
				(*control)->Deactivate( static_cast<DWORD>( event.timestamp ) );
			}
			controlIDs.erase( (runtimeID << 16) | ((*control)->GetID() & 0xffff) );
			controlNames.erase( (*control)->GetDesc().szName );
			for ( CStoredControlsList::iterator active = activecontrols.begin(); active != activecontrols.end(); )
			{
				if ( active->pControl == *control ) active = activecontrols.erase( active );
				else ++active;
			}
		}
		devices.erase( device );
		break;
	}
	controllerDeviceIDs.erase( host );
}
#endif
bool CInputAPI::AddDoubleClick( const std::string &szControlName )
{
	CControlNamesMap::iterator pos = controlNames.find( szControlName );
	if ( (pos == controlNames.end()) || (pos->second->GetType() != CONTROL_TYPE_KEY) ) 
		return false;
	CControl *pControl = pos->second;
	SControlDesc desc = pControl->GetDesc();
	if ( SDevice *pDevice = GetDevice((desc.nID >> 16) & 0xffff) )
	{
		desc.szName += "_DBLCLK";
		desc.nID |= 0x4000;
		CControl *pDBLCLK = new CControlKey( desc );
		pDevice->controls.push_back( pDBLCLK );
		controlIDs[desc.nID] = pDBLCLK;
		controlNames[desc.szName] = pDBLCLK;
		static_cast<CControlKey*>(pControl)->SetDoubleClickControl( pDBLCLK );
		return true;
	}
	return false;
}
bool CInputAPI::SetPower( const std::string &szControlName, const float fPower )
{
	CControlNamesMap::iterator pos = controlNames.find( szControlName );
	if ( pos == controlNames.end() ) 
		return false;
	pos->second->SetPower( fPower );
	return true;
}
bool CInputAPI::SetCoopLevel()
{
#if defined(BK_INPUT_EVENT_ONLY)
	bCoopLevelSet = true;
	return true;
#else
	if ( bCoopLevelSet )
		return true;
	for ( CDevicesList::iterator it = devices.begin(); it != devices.end(); ++it )
	{
		if ( it->bEmulated ) 
			continue;
		HRESULT dxrval;
		if ( it->eType == DEVICE_TYPE_KEYBOARD || it->eType == DEVICE_TYPE_MOUSE )
			dxrval = it->pDevice->SetCooperativeLevel( hWindow, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND );
		else
			dxrval = it->pDevice->SetCooperativeLevel( hWindow, DISCL_EXCLUSIVE | DISCL_FOREGROUND );
		NI_ASSERTHR_TF( dxrval, NStr::Format("Can't set cooperative level for the device \"%s\"", it->szFriendlyName.c_str()), return false );
	}
	bCoopLevelSet = true;
	return true;
#endif
}
bool CInputAPI::SetFocus( bool bFocus )
{
	if ( !bInitialized )
		return false;
	if ( bFocusCaptured == bFocus )
		return true;
#if defined(BK_INPUT_EVENT_ONLY)
	bFocusCaptured = bFocus;
	if ( bFocus )
		GetSingleton<IInput>()->AddMessage( SGameMessage( UI_CLEAR_KEYBOARD_STATE ) );
	return true;
#else
	if ( bFocus )
	{
		// The SDL owner has already established focus; there is no native window
		// handle or DirectInput cooperative level involved in this path.
		for ( CDevicesList::iterator it = devices.begin(); it != devices.end(); ++it )
		{
			if ( it->bEmulated ) 
				continue;
			HRESULT dxrval = it->pDevice->Acquire();
			if ( dxrval == DIERR_OTHERAPPHASPRIO  )
				return false;
			NI_ASSERTHR_TF( dxrval, "Can't acquire on of the devices", return false );
			std::vector<BYTE> data;
			if ( GetDeviceState(*it, data) )
			{
				for ( std::vector<CControl*>::iterator control = it->controls.begin(); control != it->controls.end(); ++control )
				{
					if ( ((*control)->GetDesc().eType != CONTROL_TYPE_KEY) && ((*control)->GetID() & 0xf000) == 0 )
					{
						const DWORD dwData = (*control)->GetDataFromBuffer( &(data[0]) );
						(*control)->InitState( dwData );
					}
				}
			}
		}

		GetSingleton<IInput>()->AddMessage( SGameMessage( UI_CLEAR_KEYBOARD_STATE ) );
	}
	else
	{
		for ( CDevicesList::const_iterator it = devices.begin(); it != devices.end(); ++it )
			it->pDevice->Unacquire();
	}
	bFocusCaptured = bFocus;
	return true;
#endif
}
void CInputAPI::SetDeviceEmulationStatus( const enum EDeviceType eDeviceType, const bool bEmulate )
{
#if !defined(BK_INPUT_EVENT_ONLY)
	for ( CDevicesList::iterator it = devices.begin(); it != devices.end(); ++it )
		it->pDevice->Unacquire();
#endif
	for ( CDevicesList::iterator it = devices.begin(); it != devices.end(); ++it )
	{
		if ( it->eType == eDeviceType ) 
		{
			it->bEmulated = bEmulate;
			bCoopLevelSet = false;
			const bool bOldFocusCaptured = bFocusCaptured;
			bFocusCaptured = false;
			SetFocus( bOldFocusCaptured );
			break;
		}
	}
}
bool CInputAPI::IsEmulated( const enum EDeviceType eDeviceType ) const
{
	for ( CDevicesList::const_iterator it = devices.begin(); it != devices.end(); ++it )
	{
		if ( it->eType == eDeviceType ) 
			return it->bEmulated;
	}
	return false;
}
void CInputAPI::EmulateInput( const enum EDeviceType eDeviceType, const int nControlID, 
														  const int nValue, const DWORD time, const int nParam )
{
	for ( CDevicesList::const_iterator it = devices.begin(); it != devices.end(); ++it )
	{
		if ( it->eType == eDeviceType ) 
		{
#if defined(BK_INPUT_EVENT_ONLY)
			emulatedMessages.push_back( SInputEvent {
				static_cast<std::uint32_t>( it->nID ), static_cast<std::uint32_t>( nControlID ),
				static_cast<std::int32_t>( nValue ), static_cast<std::uint32_t>( time ),
				static_cast<std::uint32_t>( nParam ) } );
#else
			DIDEVICEOBJECTDATA didod;
			didod.dwOfs = ( it->nID << 16 ) | nControlID;
			didod.dwData = nValue;
			didod.dwTimeStamp = time;
			didod.dwSequence = nParam;
			emulatedMessages.push_back( didod );
#endif
			break;
		}
	}
}
static void AppendUtf8AsUtf16( const char *text, std::list<STextMessage> &chars )
{
	if ( !text ) return;
	uint16_t decoded[128] = {};
	const std::size_t count = NInput::DecodeUtf8( text, decoded, sizeof(decoded) / sizeof(decoded[0]) );
	for ( std::size_t i = 0; i != count; ++i )
	{
		STextMessage message = {};
		message.wChars[0] = decoded[i];
		message.bPressed = true;
		chars.push_back( message );
	}
}
void CInputAPI::ConsumePlatformEvent( const NPlatform::PlatformEvent &event )
{
	const DWORD time = static_cast<DWORD>( event.timestamp );
	const int packedPosition = ((event.x & 0x7fff) | ((event.y & 0x7fff) << 15) | 0x40000000);
	switch ( event.type )
	{
		case NPlatform::EventType::keyDown:
		case NPlatform::EventType::keyUp:
		{
			if ( eTextMode == INPUT_TEXT_MODE_TEXTONLY )
				break;
			const uint32_t legacy = NInput::SDLScancodeToLegacy( static_cast<uint32_t>( event.scancode ) );
			if ( legacy != 0 )
				EmulateInput( DEVICE_TYPE_KEYBOARD, static_cast<int>( legacy ), event.type == NPlatform::EventType::keyDown ? 0x80 : 0, time, event.modifiers );
			break;
		}
		case NPlatform::EventType::textInput:
			if ( eTextMode != INPUT_TEXT_MODE_NOTEXT ) AppendUtf8AsUtf16( event.text, chars );
			break;
		case NPlatform::EventType::focusLost:
			for ( CDevicesList::const_iterator device = devices.begin(); device != devices.end(); ++device )
				if ( device->eType == DEVICE_TYPE_KEYBOARD || device->eType == DEVICE_TYPE_MOUSE )
					for ( std::vector<CControl*>::const_iterator control = device->controls.begin(); control != device->controls.end(); ++control )
						if ( (*control)->IsActive() )
							EmulateInput( device->eType, (*control)->GetID() & 0x0fff, 0, time, 0 );
			bFocusCaptured = false;
			break;
		case NPlatform::EventType::focusGained:
			bFocusCaptured = true;
			break;
		case NPlatform::EventType::mouseMotion:
			EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_AXIS_X, event.x, time, event.x );
			EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_AXIS_Y, event.y, time, event.y );
			break;
		case NPlatform::EventType::mouseButtonDown:
		case NPlatform::EventType::mouseButtonUp:
		{
			const int control = INPUT_CONTROL_MOUSE_BUTTON0 + Max( 0, event.button - 1 );
			EmulateInput( DEVICE_TYPE_MOUSE, control, event.type == NPlatform::EventType::mouseButtonDown ? 0x80 : 0, time, packedPosition );
			break;
		}
		case NPlatform::EventType::mouseWheel:
			EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_AXIS_Z, event.y, time, 0 );
			break;
		case NPlatform::EventType::controllerAdded:
#if defined(BK_INPUT_EVENT_ONLY)
			HandleControllerAdded( event );
#endif
			break;
		case NPlatform::EventType::controllerRemoved:
#if defined(BK_INPUT_EVENT_ONLY)
			HandleControllerRemoved( event );
#endif
			break;
		case NPlatform::EventType::controllerButtonDown:
		case NPlatform::EventType::controllerButtonUp:
#if defined(BK_INPUT_EVENT_ONLY)
		{
			const std::unordered_map<int, int>::const_iterator device = controllerDeviceIDs.find( event.deviceId );
			if ( device != controllerDeviceIDs.end() && event.control >= 0 && event.control < 16 )
				emulatedMessages.push_back( SInputEvent {
					static_cast<std::uint32_t>( device->second ),
					static_cast<std::uint32_t>( ControllerButtonControlID( event.control )),
					static_cast<std::int32_t>( event.type == NPlatform::EventType::controllerButtonDown ? 0x80 : 0 ),
					static_cast<std::uint32_t>( time ), 0 } );
			break;
		}
#endif
			break;
		case NPlatform::EventType::controllerAxis:
#if defined(BK_INPUT_EVENT_ONLY)
		{
			const std::unordered_map<int, int>::const_iterator device = controllerDeviceIDs.find( event.deviceId );
			if ( device != controllerDeviceIDs.end() && event.control >= 0 && event.control < 6 )
				emulatedMessages.push_back( SInputEvent {
					static_cast<std::uint32_t>( device->second ),
					static_cast<std::uint32_t>( ControllerAxisControlID( event.control )),
					static_cast<std::int32_t>( NormalizeControllerAxis( event.value, event.control )),
					static_cast<std::uint32_t>( time ), 0 } );
			break;
		}
#endif
			break;
		default: break;
	}
}
template <class TYPE>
inline bool GetMessage( std::list<TYPE> &messages, TYPE *pMsg )
{
	if ( messages.empty() ) 
		return false;
	*pMsg = messages.front();
	messages.pop_front();
	return true;
}
bool CInputAPI::GetMessage( SGameMessage *pMsg ) 
{ 
	return ::GetMessage( messages, pMsg ); 
}
bool CInputAPI::GetTextMessage( STextMessage *pMsg ) 
{ 
	return ::GetMessage( chars, pMsg ); 
}
void CInputAPI::ClearMessages()
{
	PumpMessagesLocal( bFocusCaptured );
	messages.clear();
	chars.clear();
}
#if !defined(BK_INPUT_EVENT_ONLY)
struct SSeqNumberLessThenFunctional
{
	bool operator()( const DIDEVICEOBJECTDATA &dido1, const DIDEVICEOBJECTDATA &dido2 ) const 
	{ 
		return ( dido1.dwSequence < dido2.dwSequence ); 
	}
};
#endif
bool CInputAPI::PumpMessagesLocal( bool bFocus )
{
	if ( !bInitialized )
		return false;
	const bool bForceGetState = bFocus && !bFocusCaptured;
	SetFocus( bFocus );
	#if !defined(BK_INPUT_EVENT_ONLY)
	if ( !bFocusCaptured )
		return false;
	#else
	if ( !bFocusCaptured && emulatedMessages.empty() )
		return false;
	#endif
	for ( CStoredControlsList::iterator it = activecontrols.begin(); it != activecontrols.end(); ++it )
		it->bActive = false;
#if defined(BK_INPUT_EVENT_ONLY)
	chars.clear();
	dwLastPumpingTime = static_cast<DWORD>( NPlatform::MonotonicMilliseconds() );
	// Platform events are already ordered by the SDL owner.  sequence remains
	// the legacy notification parameter (modifiers, position, or caller data),
	// so sorting on it would change same-frame input semantics.
	for ( std::deque<SInputEvent>::const_iterator it = emulatedMessages.begin(); it != emulatedMessages.end(); ++it )
		EventCame( *it );
	emulatedMessages.clear();
#else
	std::vector<DIDEVICEOBJECTDATA> events( SAMPLE_BUFFER_SIZE * devices.size() );
	chars.clear();
	int nNumEvents = 0;
	for ( CDevicesList::iterator it = devices.begin(); it != devices.end(); ++it )
	{
		if ( it->bEmulated ) 
			continue;
		const SDevice &device = *it;
		DIDEVICEOBJECTDATA *pDIDOs = &( events[nNumEvents] );
		DWORD dwElements = SAMPLE_BUFFER_SIZE;
		if ( it->bPoll ) 
		{
			HRESULT dxrval = it->pDevice->Poll();
			if ( FAILED(dxrval) ) 
				it->pDevice->Acquire();
		}
		
		HRESULT dxrval = it->pDevice->GetDeviceData( sizeof(DIDEVICEOBJECTDATA), pDIDOs, &dwElements, 0 );
		if ( dxrval != DI_OK ) 
		{
			dxrval = it->pDevice->Acquire();
			while ( dxrval == DIERR_INPUTLOST ) 
				dxrval = it->pDevice->Acquire();
			it->bNeedResync = true;
			continue;
		}
		if ( dxrval == DIERR_OTHERAPPHASPRIO ) 
			continue;
		NI_ASSERTHR_SLOW_TF( dxrval, "Can't get bufferred data from the device", return false );
		if ( it->eType == DEVICE_TYPE_KEYBOARD ) 
		{
			if ( eTextMode != INPUT_TEXT_MODE_NOTEXT ) 
				Convert2Text( pDIDOs, dwElements );
			if ( eTextMode == INPUT_TEXT_MODE_TEXTONLY ) 
				dwElements = 0;
		}
		for ( int i = 0; i != dwElements; ++i )
			pDIDOs[i].dwOfs |= ( it->nID << 16 );
		nNumEvents += dwElements;
		pDIDOs += dwElements;
		
	}
	events.resize( nNumEvents );
	std::sort( events.begin(), events.end(), SSeqNumberLessThenFunctional() );
	dwLastPumpingTime = timeGetTime();
	DWORD dwSequence = 1000000;
	for ( CDevicesList::iterator it = devices.begin(); it != devices.end(); ++it )
	{
		if ( it->bEmulated ) 
			continue;
		if ( it->bNeedResync || bForceGetState ) 
		{
			events.reserve( events.size() + it->controls.size() );
			std::vector<BYTE> data;
			if ( GetDeviceState(*it, data) )
			{
				for ( std::vector<CControl*>::iterator control = it->controls.begin(); control != it->controls.end(); ++control )
				{
					if ( ((*control)->GetID() & 0xf000) == 0 )
					{
						DIDEVICEOBJECTDATA didod;
						didod.dwOfs = (*control)->GetID();
						didod.dwSequence = dwSequence++;
						didod.dwData = (*control)->GetDataFromBuffer( &(data[0]) );
						didod.dwTimeStamp = dwLastPumpingTime;
						events.push_back( didod );
					}
				}
			}
			it->bNeedResync = false;
		}
	}
	for ( std::vector<DIDEVICEOBJECTDATA>::const_iterator it = events.begin(); it != events.end(); ++it )
		EventCame( &(*it), 0 );
	for ( std::deque<DIDEVICEOBJECTDATA>::const_iterator it = emulatedMessages.begin(); it != emulatedMessages.end(); ++it )
		EventCame( &(*it), it->dwSequence );
	emulatedMessages.clear();
#endif
	for ( CStoredControlsList::iterator it = activecontrols.begin(); it != activecontrols.end();  )
	{
		if ( !it->bActive ) 
		{
			it->pControl->Deactivate( GetCurrentTime() );
			if ( !it->pControl->IsActive() ) 
				it = activecontrols.erase( it );
			else
			{
				GenerateRepeats( it->pControl );
				++it;
			}
		}
		else
		{
			GenerateRepeats( it->pControl );
			++it;
		}
	}
	return true;
}
#if !defined(BK_INPUT_EVENT_ONLY)
void CInputAPI::Convert2Text( const DIDEVICEOBJECTDATA *pData, int nNumElements )
{
	BYTE keys[256];
	if ( GetKeyboardState( keys ) == 0 )
		memset( keys, 0, sizeof(keys) );
	HKL hkl = GetKeyboardLayout( 0 );	// get current keyboard layout
	for ( int i=0; i<nNumElements; ++i )
	{
		STextMessage message;
		message.bPressed = ( pData[i].dwData & 0x80 ) != 0;
		message.nScanCode = pData[i].dwOfs;
		message.wChars[0] = message.wChars[1] = 0;
		message.nVirtualKey = MapVirtualKeyEx( message.nScanCode, 1, hkl );
		if ( message.nVirtualKey == 0 )
		{
			switch ( message.nScanCode )
			{
				case DIK_HOME:
					message.nVirtualKey = VK_HOME;
					break;
				case DIK_END:
					message.nVirtualKey = VK_END;
					break;
				case DIK_UP:
					message.nVirtualKey = VK_UP;
					break;
				case DIK_DOWN:
					message.nVirtualKey = VK_DOWN;
					break;
				case DIK_LEFT:
					message.nVirtualKey = VK_LEFT;
					break;
				case DIK_RIGHT:
					message.nVirtualKey = VK_RIGHT;
					break;
				case DIK_PRIOR:
					message.nVirtualKey = VK_PRIOR;
					break;
				case DIK_NEXT:
					message.nVirtualKey = VK_NEXT;
					break;
				case DIK_INSERT:
					message.nVirtualKey = VK_INSERT;
					break;
				case DIK_DELETE:
					message.nVirtualKey = VK_DELETE;
					break;
				default:
					continue;
			}
		}
		else
		{
			ToAsciiEx( message.nVirtualKey, message.nScanCode, keys, message.wChars, 0, hkl );
			WCHAR wUnicode = 0;
			const char cCharCode = message.wChars[0];
			const int nNumChar = MultiByteToWideChar( nCodePage, 0, &cCharCode, 1, &wUnicode, 1 );
			message.wChars[0] = nNumChar == 1 ? wUnicode : 0;
		}
		chars.push_back( message );
	}
}
#else
void CInputAPI::EventCame( const SInputEvent &event )
{
	const int controlID = static_cast<int>( (event.device_id << 16) | (event.control_id & 0xffffu) );
	CControl *pControl = controlIDs[controlID];
	if ( pControl == 0 )
		return;
	if ( pControl->GetType() == CONTROL_TYPE_POV_X )
	{
		const float fAngle = float( event.value ) / 36000.0f * FP_2PI;
		const float fAxisX = event.value == -1 ? 0 : AXIS_RANGE_VALUE * sin( fAngle );
		const float fAxisY = event.value == -1 ? 0 : AXIS_RANGE_VALUE * cos( fAngle );
		if ( pControl->ChangeState( fAxisX, event.timestamp, event.sequence, pLastControlKey ) )
			AddActiveControl( pControl );
		if ( CControl *pControlY = controlIDs[pControl->GetID() | 0x8000] )
		{
			if ( pControlY->ChangeState( fAxisY, event.timestamp, event.sequence, pLastControlKey ) )
				AddActiveControl( pControlY );
		}
	}
	else if ( pControl->ChangeState( event.value, event.timestamp, event.sequence, pLastControlKey ) )
	{
		AddActiveControl( pControl );
		if ( pControl->GetType() == CONTROL_TYPE_KEY )
			pLastControlKey = pControl;
	}
}
#endif
#if !defined(BK_INPUT_EVENT_ONLY)
void CInputAPI::EventCame( const DIDEVICEOBJECTDATA *pEvent, const int nParam )
{
	CControl *pControl = controlIDs[pEvent->dwOfs];
	if ( pControl == 0 )
		return;
	if ( pControl->GetType() == CONTROL_TYPE_POV_X ) 
	{
		const float fAngle = float( pEvent->dwData ) / 36000.0f * FP_2PI;
		const float fAxisX = pEvent->dwData == -1 ? 0 : AXIS_RANGE_VALUE * sin( fAngle );
		const float fAxisY = pEvent->dwData == -1 ? 0 : AXIS_RANGE_VALUE * cos( fAngle );
		if ( pControl->ChangeState( fAxisX, pEvent->dwTimeStamp, nParam, pLastControlKey ) )
			AddActiveControl( pControl );
		if ( CControl *pControlY = controlIDs[pControl->GetID() | 0x8000] )
		{
			if ( pControlY->ChangeState(fAxisY, pEvent->dwTimeStamp, nParam, pLastControlKey) )
				AddActiveControl( pControlY );
		}
	}
	else if ( pControl->ChangeState(pEvent->dwData, pEvent->dwTimeStamp, nParam, pLastControlKey) )
	{
		AddActiveControl( pControl );
		if ( pControl->GetType() == CONTROL_TYPE_KEY ) 
			pLastControlKey = pControl;
	}
}
#endif
void CInputAPI::AddActiveControl( CControl *pControl )
{
	for ( CStoredControlsList::iterator it = activecontrols.begin(); it != activecontrols.end(); ++it )
	{
		if ( it->pControl == pControl ) 
		{
			it->bActive = true;
			return;
		}
	}
	activecontrols.push_back( pControl );
}
void CInputAPI::GenerateRepeats( CControl *pControl )
{
#if defined(BK_INPUT_EVENT_ONLY)
	(void)pControl;
	return;
#else
	if ( (eTextMode == INPUT_TEXT_MODE_NOTEXT) ) 
		return;
	const SDevice *pDevice = GetDevice( (pControl->GetID() >> 16) & 0xffff );
	if ( pDevice->eType == DEVICE_TYPE_KEYBOARD )
	{
		const int nRepeats = pControl->GenerateRepeats( dwLastPumpingTime, dwRepeatDelay, dwRepeatPeriod );
		if ( nRepeats == 1 ) 
		{
			DIDEVICEOBJECTDATA didod;
			didod.dwOfs = pControl->GetID() & 0x0fff;
			didod.dwData = 128;
			didod.dwSequence = 0;
			didod.dwTimeStamp = dwLastPumpingTime;
			Convert2Text( &didod, 1 );
		}
		else if ( nRepeats > 1 ) 
		{
			DIDEVICEOBJECTDATA didod;
			didod.dwOfs = pControl->GetID() & 0x0fff;
			didod.dwData = 128;
			didod.dwSequence = 0;
			didod.dwTimeStamp = dwLastPumpingTime;

			std::vector<DIDEVICEOBJECTDATA> didods( nRepeats, didod );
			Convert2Text( &(didods[0]), didods.size() );
		}
	}
#endif
}
void CInputAPI::VisitControls( IInputVisitor *pVisitor )
{
	for ( CControlIDsMap::iterator it = controlIDs.begin(); it != controlIDs.end(); ++it )
	{
		if ( it->second ) 
			pVisitor->VisitControl( it->second );
	}
}
#if !defined(BK_INPUT_EVENT_ONLY)
bool CInputAPI::GetDeviceState( SDevice &device, std::vector<BYTE> &data )
{
	data.resize( device.dwBufferSize );
	HRESULT dxrval = device.pDevice->GetDeviceState( device.dwBufferSize, &(data[0]) );
	NI_ASSERTHR_T( dxrval, NStr::Format("Can't get device state for device \"%s\" of type \"%s\"", device.szFriendlyName.c_str(), device.szName.c_str()) );
	return SUCCEEDED( dxrval );
}
#endif
int CInputAPI::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &messages );
	saver.Add( 2, &chars );
	return 0;
}
