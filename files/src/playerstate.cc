/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 * This module will be in Idle mode once bluetooth is enabled, On mode once streaming
 */
#include "playerstate.h"

const std::string kWakelockShutDown = "adk.modeflow.shutdown";


PlayerStates::PlayerStates():ModeFlow("playerstate")
{
    msgProc();
    string recoverInfo =  modeRecovery("Init");

    std::thread(&ModeFlow::init_actions,this, [this, recoverInfo] {return make_shared<string>(recoverInfo);}).detach();

}

void PlayerStates::IfConnector()
{
//flow target idle
//recovery
    emecMap_re("devpwr::StreamStopped", "On","playerstate::setIdle");

//stream start
    emecMap_re("devpwr::StreamStarted", "(Standby|Idle|TransIdle)","playerstate::setOn");

//PowerUp
    emecMap_re("playerstate::startupIdle","Init","playerstate::setIdle");
//SeqRestartPowerOnReconnect
    emecMap_re("bt::enabled","Init","playerstate::setIdle");
//PowerOn
    //in case of recover to Transon, shall power on again.
    emecMap_re("devpwr::.+2Transon","(TransOff|Off)","playerstate::setTransIdle");
//TransIdle to Idle
    emecMap_re("source::.+2(bt|wifi).*","TransIdle","playerstate::setIdle");
//Standby to Idle
    emecMap_re("source::.+2(wifi|bt).*","Standby" ,"playerstate::setIdle");
    emecMap_re("source::(bt_enabled|wifi_enabled)","TransIdle" ,"playerstate::setIdle");

//flow target off
//PowerOff
    emecMap_re("devpwr::(?!recover|Docked|Standby).+2Standby",  "Off", "playerstate::setOff");//indicate devpwr to go to Off
    emecMap_re("devpwr::(?!recover).+2Docked",  "(?!TransOff|Off).+", "playerstate::setOff");//indicate devpwr to go to Off without bt/wifi disabled //??

//rtcwakeup
    emecMap_re("trigger::rtc_wakeup_shutpwr","Off","playerstate::setShutPower");
    emecMap_re("trigger::rtc_wakeup_offtimer","(?!wifi).+","playerstate::setOffTimer");

//FlowPowerOffUSB
    e2aMap_re("PwrDlvr::(?!recover).+2ConnHost.+", [this] {usbhost_present = true; return (shared_ptr<string>)nullptr;});
    e2aMap_re("PwrDlvr::(?!recover).+2DisConn", [this] {usbhost_present = false; return (shared_ptr<string>)nullptr;});

//BatteryLimpCharging
//flow target on
//TargetOn
    emecMap_re("audio::bt:Playing", "(TransIdle|Idle|Standby)","playerstate::setOn");
    emecMap_re("HFPman::Off2Call.+","(TransIdle|Idle|Standby)", "playerstate::setOn");
//FlowConnectFlowDsp
    emecMap_re("volume::(flowdsp_connect_failed|send_failed).+","On","playerstate::setflowreconn");

//flow statndby
//TargetStandby
    emecMap_re("trigger::standby_timer_out",  "Idle" ,"playerstate::setStandby");
    emecMap_re("source::.+2lost.+", "(Init|On|Idle)" ,"playerstate::setStandby");
    emecMap_re("source::.+(2initbt|2initwifi)", "(Init|On|Idle)" ,"playerstate::setStandby");

//Can't successfully recover, because bt status need to be checked.
//FlowBtDisconnected
    em2eMap_re("btman::(?!recover|Connecting).+2Disconnected", "(?!TransOff|Off)",
               "disconnect_inidication");
//standby rtcwake
    emecMap_re("trigger::rtc_wakeup_standbytimer", "Idle", "playerstate::setStandbyRtc");
}

void PlayerStates::Input()
{
//flow idle
    em2mMap_re("playerstate::setIdle", "(On|Init|TransIdle|Standby)", "Idle");
    em2mMap_re("playerstate::setTransIdle","(TransOff|Off)","TransIdle");

//flow off
//power off
    em2mMap_re("playerstate::setOff", "(?!Init).+", "Off");

//NormalPowerDown
    em2mMap_re("playerstate::setTransOff",  "(?!TransOff|Off).+", "TransOff");


//flow target on
    em2mMap_re("playerstate::setOn", "(TransIdle|Idle|Standby)","On");

//TargetStandby
    em2mMap_re("playerstate::setStandby", "(Init|On|Idle)" ,"Standby");

}


void PlayerStates::Internal()
{

//connectorIcon
    ecMap_re("playerstate::Standby2(Idle|On)", "icon::IconDimToFull");

//timeout shutdown
    e2dmeMap_re("playerstate::.+2TransOff", "playerstate::setOff", "TransOff", 14, 0);//wifi timeout

    ecMap_re("playerstate::TransOff2Off","actions::/etc/initscripts/board-script/rtcwake.sh 2 shutpwr&");

//SeqOffTimer
    e2aMap_re("playerstate::.+2Standby", [this] {
        elapsed = 0;
        gettimeofday(&standby_time, NULL);
        return make_shared<string>(string("actions::/etc/initscripts/board-script/rtcwake.sh ") + std::to_string(OFF_TIMER*5) + " offtimer &");
    });

    em2aMap_re("playerstate::setOffTimer", "Standby",[this] {
        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        // sudden jump in time is a sign of just synced RTC via wifi or ble
        if((elapsed = current_time.tv_sec - standby_time.tv_sec) >= SECONDS_IN_DAY) {
            gettimeofday(&standby_time, NULL);
        }
        if((elapsed = current_time.tv_sec - standby_time.tv_sec) >= OFF_TIMER*5) {
            elapsed = 0;
            return make_shared<string>("playerstate::Standby:OffTimerOut");
        } else{
            return make_shared<string>(string("actions::/etc/initscripts/board-script/rtcwake.sh ") + std::to_string(OFF_TIMER*5 - elapsed) + " offtimer &");
        }

        return (shared_ptr<string>)nullptr;
    });


//SeqStandbyTimer

    e2aMap_re("playerstate::.+2Idle", [this] {
        elapsed = 0;
        gettimeofday(&idle_time, NULL);
        return make_shared<string>(string("actions::/etc/initscripts/board-script/rtcwake.sh ") + std::to_string(STANDBY_TIMER*5) + " standbytimer &");
    });

    em2aMap_re("playerstate::setStandbyRtc", "Idle",[this] {
        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        // sudden jump in time is a sign of just synced RTC via wifi or ble
        if((elapsed = current_time.tv_sec - idle_time.tv_sec) >= SECONDS_IN_DAY) {
            gettimeofday(&idle_time, NULL);
        }
        if((elapsed = current_time.tv_sec - idle_time.tv_sec) >= STANDBY_TIMER*5) {
            elapsed = 0;
            return make_shared<string>("trigger::standby_timer_out");
        } else{
            return make_shared<string>(string("actions::/etc/initscripts/board-script/rtcwake.sh ") + std::to_string(STANDBY_TIMER*5 - elapsed) + " standbytimer &");
        }
        return (shared_ptr<string>)nullptr;
    });
}

void PlayerStates::Output()
{
//connect to icon
    ecMap_re("playerstate::Standby2(Idle|On)", "icon::IconDimToFull");

    em2aMap_re("playerstate::.+2TransOff", "(?!Off).+", [this] {
        sendPayload(make_shared<string>("Tone::sound:r1-PowerDown"));
        sendAdkMsg("led_start_pattern {pattern:27}");
        sendAdkMsg("led_start_pattern {pattern:12}");
        sendAdkMsg("led_start_pattern {pattern:37}");

        return (shared_ptr<string>)nullptr;
    });

    emecMap_re("playerstate::setShutPower","Off","devpwr::shutpower");

    ecMap_re("playerstate::Standby:OffTimerOut", "devpwr::powerOff");
    e2dmeMap_re("playerstate::.+2Standby", "devpwr::gotoStandby", "Standby",2, 0);

    e2aMap_re("playerstate::(?!TransOff).+2TransOff", [this] {
        system_wake_lock_toggle(true, kWakelockShutDown, 0);
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("playerstate::TransOff2(?!TransOff).+", [this] {
        system_wake_lock_toggle(false, kWakelockShutDown, 0);
        return (shared_ptr<string>)nullptr;
    });


    e2aMap_re("playerstate::(Init|TransIdle)2TransOff", [this] {
        sendAdkMsg("led_start_pattern {pattern:27}");
        sendAdkMsg("led_start_pattern {pattern:12}");
        sendAdkMsg("led_start_pattern {pattern:37}");
        return (shared_ptr<string>)nullptr;
    });

//FlowConnectFlowDsp
    em2eMap_re("playerstatte::setflowreconn","On","flowreconn");

//flow standby
//Indication Interface
    ecMap_re("playerstate::.+:disconnect_inidication", "indication::btDisconnected");


}

void PlayerStates::IOConnector()
{
    // During cold boot wifi::enabled is not received, this is why this work around
    e2aMap_re("wifi::connecting",
    [this] {
        exec_cmd("/etc/initscripts/board-script/set_country_code.sh");
        return (shared_ptr<string>)nullptr;
    });

    em2aMap("devpwr::SysEnabled", "TransIdle",[this] {
        sendAdkMsg("led_start_pattern {pattern:9}");
        return (shared_ptr<string>)nullptr;
    });

//StartupTimeoutCheck
    e2dmeMap_re("devpwr::.+2Transon", "actions::/etc/exitscripts/board-script/reboot.sh 3 startFailure &", "Transon", 20,0);
    //inform lantern to go to standby after lanter goes to history mode.
    emecMap_re( "playerstate::checkStandby", "Standby","lantern::confirmStandby");

    //Can't successfully recover, because bt status need to be checked.
    //FlowBtDisconnected
    em2aMap("btman::Connected2Disconnected", "Idle",[this] {
        sendAdkMsg("led_start_pattern {pattern:27}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap("playerstate::setStandbyTimer", [this] {
        this->STANDBY_TIMER = 4;
        return (shared_ptr<string>)nullptr;
    });
}
