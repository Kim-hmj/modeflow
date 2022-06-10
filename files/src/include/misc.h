/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MODEFLOW_MISC_H
#define MODEFLOW_MISC_H
#include "modeflow.h"
#include "external.h"
#include <string>
#include <unistd.h>
#include "flowdspsocket.h"
#include <fstream>
#include <iostream>
#include <cstdint>
#include <syslog.h>
#include "external.h"
#include "gpio.h"
#include "led.h"
using adk::msg::AdkMessage;
using adk::msg::AdkMessageService;

//for engineering samples special build
//#define ENGINEERING
class Misc:public ModeFlow
{
public:
    Misc();
    shared_ptr<string> factory_reset();
    shared_ptr<string> e2tateste();
};

class Icon:public ModeFlow
{
public:
    Icon():ModeFlow("icon")
    {
        //icon
        ecMap_re("icon::TopButtonFadeOn","led::" + std::to_string(Pattern_TopButtonFadeOn));
        ecMap_re("icon::FadetoFull","led::" + std::to_string(Pattern_IconsDim2Full));
        ecMap_re("icon::DimTo30",
                 "led::" + std::to_string(Pattern_ButtonPlayPauseFullWhite),
                 "led::" + std::to_string(Pattern_IconsFull2Dim));
        //lantern
        ecMap_re("icon::LanternButtonOn","led::" + std::to_string(Pattern_LanternButtonOn));
        ecMap_re("icon::LanternButtonOff","led::" + std::to_string(Pattern_LanternButtonOff));

        ecMap_re("icon::PlayPausePulsing","led::" + std::to_string(Pattern_PlayPauseBlinkCycle));
        ecMap_re("icon::PlayPauseBlink","led::" + std::to_string(Pattern_PlayPauseBlink));
        ecMap_re("lantern::(Low|Medium|High)2Standby", "led::" + std::to_string(Pattern_LanternButtonDim));
        //ecMap_re("icon::lanternIconDim","led::" + std::to_string(Pattern_LanternButtonDim));

        modeRecovery("Init", 0, 32, nullptr, false);
    };
};

class ConnectIcon:public ModeFlow
{
public:
    ConnectIcon():ModeFlow("ConnectIcon")
    {
        e2mMap_re("source::.+2bt","Blue");
        e2mMap_re("source::.+(2wifipart|2wififull)","Amber");
        e2mMap_re("source::.+2lostbt","White");
        e2mMap_re("source::.+2lostwifi","White");
        e2mMap_re("source::.+2initwifi","Off");
        e2mMap_re("source::.+2initbt","Off");
        e2mMap_re("source::(?!Init).+2transbt","BPuls");
        e2mMap_re("source::.+2transwifi","APuls");
        e2mMap_re("ConnectIcon::ConnectFadeToOff","Off");
        e2mMap_re("ConnectIcon::BtPairingRun", "BPairingRun");
        e2mMap_re("ConnectIcon::BtPairingFailed","White");

        //dim
        emecMap_re("ConnectIcon::ConnectDimTo30","Blue","led::" + std::to_string(Pattern_ConnectDimTo30Blue));
        emecMap_re("ConnectIcon::ConnectDimTo30","Amber","led::" + std::to_string(Pattern_ConnectDimTo30Amber));
        emecMap_re("ConnectIcon::ConnectDimTo30","White","led::" + std::to_string(Pattern_ConnectDimTo30White));
        emecMap_re("ConnectIcon::ConnectDimTo30","Off","led::" + std::to_string(Pattern_ConnectIconOff));

        //pulsing
        emecMap_re("ConnectIcon::ConnectPulsing","BPuls","led::" + std::to_string(Pattern_ConnectBluePulsing));
        emecMap_re("ConnectIcon::ConnectPulsing","APuls","led::" + std::to_string(Pattern_ConnectAmberPulsing));

        //pairing or pairing failed
        ecMap_re("ConnectIcon::BtPairingRun", "led::" + std::to_string(Pattern_ConnectBluePulsing));
        ecMap_re("ConnectIcon::BtPairingFailed", "led::" + std::to_string(Pattern_ConnectFade2Off));

        //initsource
        ecMap_re("ConnectIcon::InitSource","led::" + std::to_string(Pattern_ConnectIconOff));
		
        //initbt
        ecMap_re("ConnectIcon::InitBt","led::" + std::to_string(Pattern_ConnectIconOff));
        //bt connect
        emecMap_re("ConnectIcon::BtConnected","Blue","led::" + std::to_string(Pattern_ConnectFadetoBlue));
        //bt disconnect
        emecMap_re("ConnectIcon::BtDisconnected", "(?!BPairingRun)", "led::" + std::to_string(Pattern_ConnectDimTo30White));
        //offbt
        ecMap_re("ConnectIcon::OffBt","led::" + std::to_string(Pattern_ConnectIconOff));


        //InitWifi
        ecMap_re("ConnectIcon::InitWifi","led::" + std::to_string(Pattern_ConnectIconOff));
        //wifi connect
        emecMap_re("ConnectIcon::WifiConnected","Amber","led::" + std::to_string(Pattern_ConnectFadeToAmber));
        //wifi disconnect
        ecMap_re("ConnectIcon::WifiDisconnected","led::" + std::to_string(Pattern_ConnectDimTo30White));
        //offwifi
        ecMap_re("ConnectIcon::OffWifi","led::" + std::to_string(Pattern_ConnectIconOff));


        //full
        emecMap_re("ConnectIcon::ConnectFadetoOn","Blue","led::" + std::to_string(Pattern_ConnectFadetoBlue));
        emecMap_re("ConnectIcon::ConnectFadetoOn","Amber","led::" + std::to_string(Pattern_ConnectFadeToAmber));
        emecMap_re("ConnectIcon::ConnectFadetoOn","White","led::" + std::to_string(Pattern_ConnectDimTo30White));

        //Off
        ecMap_re("ConnectIcon::.*2Off","led::" + std::to_string(Pattern_ConnectFade2Off));

        modeRecovery("Init", 0, 32, nullptr, false);
    };
};

class AMP:public ModeFlow
{
public:
    AMP():ModeFlow("amp")
    {
        AMPStateDetector();
        AMPInterface();
        AMPController();
        AMPActuator();

        modeRecovery("Off");
    };

    void AMPStateDetector()
    {
        //Sensor
        em2mMap_re("trigger::amplifier_initiated",  "(?!Trans(Off|Sleep)).+", "On");
        em2mMap_re("trigger::amplifier_slept", "(?!Transon).+", "Slept");
        em2mMap_re("trigger::amplifier_off", "(?!Transon).+","Off");
        em2mMap_re("trigger::amplifier_wakened", "(?!Trans(Off|Sleep)).+", "On");
    }

    void AMPInterface()
    {
        //interface
        em2mMap_re("amp::PowerOn", "(?!Transon|On).+","Transon");//garantee amp can be powered
        em2mMap_re("amp::Sleep", "(On|TransSleep)", "TransSleep");//can't go to sleep from Off
        em2mMap_re("amp::PowerOff", "(?!(Trans)?Off).+","TransOff");
        //derived interface, when devpwr goes to standby...
        emecMap_re("amp::OffIfNotOn", "(?!On|Transon)","amp::PowerOff");
    }

    void AMPController()
    {
        //FlowDebouncing
        emecMap_re("trigger::amplifier_(slept|off)","Transon","amp::PowerOn");
        emecMap_re("trigger::amplifier_(initated|wakened)","TransOff","amp::PowerOff");
        emecMap_re("trigger::amplifier_(initated|wakened)","TransSleep","amp::Sleep");
    }

    const std::string kWakelockAmp = "adk.modeflow.amp";
    void AMPActuator()
    {
        //Actuator
        //inlcuding recover
        e2aMap_re("amp::.+2Transon", [this] {
            exec_cmd("/etc/initscripts/board-script/enable-amplifier.sh &");
            return (shared_ptr<string>)nullptr;
        });
        e2aMap_re("amp::.+2TransSleep", [this] {
            exec_cmd("/etc/exitscripts/board-script/disable-amplifier.sh sleep &");
            return (shared_ptr<string>)nullptr;
        });
        e2aMap_re("amp::.+2TransOff", [this] {
            exec_cmd("/etc/exitscripts/board-script/disable-amplifier.sh poweroff &");
            return (shared_ptr<string>)nullptr;
        });

        e2aMap_re("amp::PowerOn", [this] {
            system_wake_lock_toggle(true, kWakelockAmp, 0);
            return make_shared<string>("amp::wakelock_on");
        });

        e2aMap_re("amp::.+2(Slept|Off)", [this] {
            system_wake_lock_toggle(false, kWakelockAmp, 0);
            return make_shared<string>("amp::wakelock_off");
        });
    }
};

#endif

