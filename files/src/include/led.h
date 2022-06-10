/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MODEFLOW_LED_H
#define MODEFLOW_LED_H

enum ELEDPattern {
    Pattern_LanternOff2Low,   		// 0,
    Pattern_LanternLow2LMedium,     // 1,
    Pattern_LanternMedium_Hight,    // 2,
    Pattern_LanternHigh2Off,     	// 3,
    Pattern_LanternOff2High,     	// 4,
    Pattern_LanternLow2Off,     	// 5,
    Pattern_LanternMedium2Off,     	// 6,
    Pattern_LanternHigh2Off2,     	// 7,  repeat with 3,almost.
    Pattern_LanternLow2High,     	// 8,
    Pattern_ButtonsFadeIn2FullCycle,     // 9,   Group 2, Action&Vol+&Playpause&Vol-&Mute&Lantern
    Pattern_LanternSwellHE,     	// 10,
    Pattern_LanternFadeOut2Off,     // 11,
    Pattern_ButtonsFadeOut2Off,    	// 12,
    Pattern_LanternFullWhite,     	// 13,
    Pattern_ButtonsFullwhite,     	// 14,
    Pattern_LanternFullBlack,     	// 15,
    Pattern_ButtonsFullBlack,     	// 16,
    Pattern_PlayPauseBlink,    		// 17,  white(0ms)2black(200ms)2white(0ms), once.
    Pattern_PlayPauseBlinkCycle,     // 18,  //white2black2white
    Pattern_ConnectIconUsbIconFadeOff,     // 19,
    Pattern_IconsFull2Dim,    // 20,  lantern+usb+connect+connect_dn+vol++vol-+Action+mute+playpause
    Pattern_IconsDim2Full,   // 21, lantern+usb+connect+connect_dn+vol++vol-+Action+mute+playpause
    Pattern_PlayPauseTo70White,     	// 22,
    Pattern_LanternHigh2Low,     		// 23,
    Pattern_LanternMedium2Low,     		// 24,
    Pattern_LanternWhiteBlink,     		// 25,  black2white2black cycle 5.
    Pattern_WifiAmberBlink,     		// 26, connect icon, cycle 3
    Pattern_ButtonPlayPauseFullWhite,   // 27,
    Pattern_TopButtonFadeOn,     		// 28,   "Action&Vol+&Playpause&Vol-&Mute&Lantern"
    Pattern_ButtonActionAmberFade2Black,     // 29, Action Group9 cycle2
    Pattern_USBChargingSlowPulse,     	// 30,
    Pattern_USBChargingFastPulse,     	// 31,
    Pattern_USBChargingComplete,     	// 32,
    Pattern_USBChargingOff,     		// 33,
    Pattern_USBChargingGreen,     		// 34,
    Pattern_USBChargingAmber,     		// 35,
    Pattern_USBChargingRed,     		// 36,
    Pattern_ConnectFade2Off,     		// 37,
    Pattern_LanternStandby2High,     	// 38,
    Pattern_LanternHigh2Standby,     	// 39,
    Pattern_LanternMedium2Standby,     	// 40,
    Pattern_LanternLow2StandBy,     	// 41,
    Pattern_LanternStandBy2Init,     	// 42,
    Pattern_LanternTest1,     			// 43,
    Pattern_LanternTest2,     			// 44,
    Pattern_LanternOff2Medium,     		// 45,
    Pattern_LanternErr,     			// 46,
    Pattern_MuteOff,     				// 47,
    Pattern_MuteOn,     				// 48,
    Pattern_ConnectBluePulsing,     	// 49,
    Pattern_ConnectFadetoBlue,     		// 50,
    Pattern_ConnectFadeToAmber,     	// 51,
    Pattern_ConnectFadeToWhite,     	// 52,
    Pattern_ButtonsFlowFlashForVolumeChange,     // 53,
    Pattern_LanternStandBy2Low,     	// 54,
    Pattern_MuteStandBy,     			// 55,
    Pattern_PlayPauseBlink2slow,     	// 56,  balck2white2black
    Pattern_PlayPauseBlink2fast,     	// 57,  balck2white2black
    Pattern_PlayPauseFullWhite2,     	// 58,  repeat with 27!!!
    Pattern_PlayPauseOff,     			// 59,
    Pattern_PlayPauseFadein30ToFullWhite,     // 60,
    Pattern_LanternGrayBlinkCycle2,     // 61, black2gray2black cycle2
    Pattern_2VolIconChange,     		// 62,
    Pattern_MuteActionChange,     	// 63,
    Pattern_2VolIconChange2,     	// 64,
    Pattern_MuteActionChange2,     	// 65,
    Pattern_LanternBluePulsing,     	// 66,
    Pattern_LanternBlue3secFadeoff,  // 67,
    Pattern_Lantern2Off,     		// 68,
    Pattern_ConnectDimTo30White,     // 69,
    Pattern_ConnectDimTo30Blue,     	// 70,

    Pattern_AlexaListeningStart,			//71
    Pattern_AlexaListeningSpeakingEnd,		//72
    Pattern_AlexaThinking,					//73
    Pattern_AlexaSpeaking,					//74
    Pattern_MicOnToOff,						//75
    Pattern_MicOffToOn,						//76
    Pattern_TimerAlarmReminder,				//77
    Pattern_TimerAlarmReminderShort,		//78
    Pattern_IncomingNotification,			//79
    Pattern_QueuedNotification,				//80
    Pattern_EnableDoNotDisturb,				//81
    Pattern_BluetoothConnectedDisconnected,  //82
    Pattern_Error,							//83
    Pattern_campfire,						//84
    Pattern_LanternAmber3secFadeoff,		//85
    Pattern_ConnectAmberPulsing,     	// 86,
    Pattern_ConnectDimTo30Amber,     	// 87,
    Pattern_campfireOff,				//88
    Pattern_LanternButtonOff,				//89
    Pattern_LanternButtonOn,				//90
    Pattern_AlexaVolume,					//91
    Pattern_CampfireOccupy1,								//92
    Pattern_CampfireOccupy2,								//93
    Pattern_CampfireOccupy3,								//94
    Pattern_CampfireOccupy4,								//95
    Pattern_CampfireOccupy5,								//96
    Pattern_CampfireOccupy6,								//97
    Pattern_CampfireOccupy7,								//98
    Pattern_CampfireOccupy8,								//99
    Pattern_CampfireOccupy9,								//100
    Pattern_CampfireOccupy10,								//101
    Pattern_CampfireOccupy11,								//102
    Pattern_CampfireOccupy12,								//103
    Pattern_CampfireOccupy13,								//104
    Pattern_CampfireOccupy14,								//105
    Pattern_CampfireOccupy15,								//106
    Pattern_CampfireOccupy16,								//107
    Pattern_LanternClass,									//108
    Pattern_LanternNight,									//109
    Pattern_CampfireOccupy17,								//110
    Pattern_CampfireOccupy18,								//111
    Pattern_CampfireOccupy19,								//112
    Pattern_LanternButtonDim,                               //113
    Pattern_ConnectIconOff,                                 //114
};
#endif
