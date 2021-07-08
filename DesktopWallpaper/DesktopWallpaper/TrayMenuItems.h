#pragma once

// Following file contgains definition for Tray menu items
// Is is used because default VisualStudio resource editor loves 
//  removing comments and deleting unused definitions

// Separator ID
#define IDM_SEP                                44444

// Tray menu "icon" ID
#define IDR_SYSTRAYMENU                        40000
										       
#define ID_SYSTRAYMENU_MOVETONEXTDISPLAY       40001
#define ID_SYSTRAYMENU_MOVETOPREVDISPLAY       40002
#define ID_SYSTRAYMENU_FULLSCREEN              40003
#define ID_SYSTRAYMENU_RESCAN_DISPLAYS         40003
										       
#define ID_SYSTRAYMENU_PAUSE                   40004
#define ID_SYSTRAYMENU_RESET_TIME              40005
#define ID_SYSTRAYMENU_RELOAD_INPUTS           40006
#define ID_SYSTRAYMENU_RELOAD_PACK             40007
										       
#define ID_SYSTRAYMENU_FPS_SUBMENU             40008 // 10 free
										       
#define ID_SYSTRAYMENU_SOUND_MODE              40019
#define ID_SYSTRAYMENU_SOUND_SOURCE            40032 // 256 different input sources, [ID_SYSTRAYMENU_SOUND_SOURCE_ITEM+i]
#define ID_SYSTRAYMENU_MOUSE_MODE              40020
										       
#define ID_SYSTRAYMENU_OPEN_PACK               40021
#define ID_SYSTRAYMENU_SAVE_PACK               40022
#define ID_SYSTRAYMENU_SAVE_PACK_AS            40023
#define ID_SYSTRAYMENU_NEW_PACK                40024
										       
#define ID_SYSTRAYMENU_MAIN_SHADER             40025
#define ID_SYSTRAYMENU_BUFFER_A_SHADER         40026
#define ID_SYSTRAYMENU_BUFFER_B_SHADER         40027
#define ID_SYSTRAYMENU_BUFFER_C_SHADER         40028
#define ID_SYSTRAYMENU_BUFFER_D_SHADER         40029

#define ID_SYSTRAYMENU_DEBUG_WARNINGS          40030
#define ID_SYSTRAYMENU_EXIT                    40031
										       
// Submenu for Shader config			       
#define IDS_SHADER_OPEN		        40501
#define IDS_SHADER_NEW		        40502 // Buffers only
#define IDS_SHADER_REMOVE	        40503
#define IDS_SHADER_INPUT_0	        40504
#define IDS_SHADER_INPUT_1	        40505
#define IDS_SHADER_INPUT_2	        40506
#define IDS_SHADER_INPUT_3	        40507

// Submenu for Shader input config
#define IDS_SHADER_INPUT_BUFFER_A   41001
#define IDS_SHADER_INPUT_BUFFER_B   41002
#define IDS_SHADER_INPUT_BUFFER_C   41003
#define IDS_SHADER_INPUT_BUFFER_D   41004
#define IDS_SHADER_INPUT_IMAGE	    41005
#define IDS_SHADER_INPUT_MICROPHONE 41006
#define IDS_SHADER_INPUT_AUDIO	    41007
#define IDS_SHADER_INPUT_VIDEO	    41008
#define IDS_SHADER_INPUT_WEBCAM	    41009

// TODO:
// Get item selected in parent menu and use submenu as standalone menu with same ID's