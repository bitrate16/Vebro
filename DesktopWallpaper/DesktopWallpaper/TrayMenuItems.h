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
										       
#define ID_SYSTRAYMENU_FPS_UP                  40008
#define ID_SYSTRAYMENU_FPS_DOWN                40009
										       
#define ID_SYSTRAYMENU_SOUND_MODE              40010
#define ID_SYSTRAYMENU_SOUND_SOURCE            40011
#define ID_SYSTRAYMENU_SOUND_SOURCE_ITEM       40012 // 256 different input sources
#define ID_SYSTRAYMENU_MOUSE_MODE              40268
										       
#define ID_SYSTRAYMENU_OPEN_PACK               40269
#define ID_SYSTRAYMENU_SAVE_PACK               40270
#define ID_SYSTRAYMENU_SAVE_PACK_AS            40271
#define ID_SYSTRAYMENU_NEW_PACK                40272
										       
#define ID_SYSTRAYMENU_MAIN_SHADER             40273
#define ID_SYSTRAYMENU_BUFFER_A_SHADER         40274
#define ID_SYSTRAYMENU_BUFFER_B_SHADER         40275
#define ID_SYSTRAYMENU_BUFFER_C_SHADER         40276
#define ID_SYSTRAYMENU_BUFFER_D_SHADER         40277

#define ID_SYSTRAYMENU_DEBUG_WARNINGS          40294
#define ID_SYSTRAYMENU_EXIT                    40295
										       
// Submenu for Shader config			       
#define ID_SYSTRAYMENU_SHADER_OPEN		       40278
#define ID_SYSTRAYMENU_SHADER_NEW		       40279 // Buffers only
#define ID_SYSTRAYMENU_SHADER_REMOVE	       40280
#define ID_SYSTRAYMENU_SHADER_INPUT_0	       40281
#define ID_SYSTRAYMENU_SHADER_INPUT_1	       40282
#define ID_SYSTRAYMENU_SHADER_INPUT_2	       40283
#define ID_SYSTRAYMENU_SHADER_INPUT_3	       40284

// Submenu for Shader input config
#define ID_SYSTRAYMENU_SHADER_INPUT_BUFFER_A   40285
#define ID_SYSTRAYMENU_SHADER_INPUT_BUFFER_B   40286
#define ID_SYSTRAYMENU_SHADER_INPUT_BUFFER_C   40287
#define ID_SYSTRAYMENU_SHADER_INPUT_BUFFER_D   40288
#define ID_SYSTRAYMENU_SHADER_INPUT_IMAGE	   40289
#define ID_SYSTRAYMENU_SHADER_INPUT_MICROPHONE 40290
#define ID_SYSTRAYMENU_SHADER_INPUT_AUDIO	   40291
#define ID_SYSTRAYMENU_SHADER_INPUT_VIDEO	   40292
#define ID_SYSTRAYMENU_SHADER_INPUT_WEBCAM	   40293