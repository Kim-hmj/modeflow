/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */
#include "devpower.h"
#include "modeflow.h"

DevicePower::DevicePower(PlayerStates *pactManStreaming):ModeFlow("devpwr")
{
    mPlayerStates = pactManStreaming;

    msgProc();

    //revert to previous solution ConnectorSource();

    string recoverInfo = modeRecovery("Shutdown", sizeof(struct DevicePowerSt), 16, (void *)&mconst_DevicePowerSt, true);

    pstDevicePower = (struct DevicePowerSt*)getShmDataAddr();

    std::thread(&ModeFlow::init_actions,this, [this, recoverInfo] {return make_shared<string>(recoverInfo);}).detach();
};

void DevicePower::IfConnector()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    //from stanby to On
    emecMap_re("trigger::wake_word_heard","Standby","devpwr::SetOnMode");

    //test mode support (2021106)
    emecMap_re("Test::enter_test_mode", "(?!(Semi)?Off|Docked).+", "devpwr::EnterTestMode");
    emecMap_re("Test::exit_test_mode", "Test", "devpwr::ExitTestMode");

    emecMap_re("playerstate::recover2On", "On", "devpwr::SetStreamChannel");

    //TargetOn SeqStartStartup
    emecMap_re("trigger::led_init_complete" ,"Shutdown", "devpwr::SetTransonMode");
    emecMap_re("playerstate::Init2Idle" ,"Transon", "devpwr::SetOnMode");

    //TargetOn SeqGroupPowerOn
    emecMap_re("button::.+[^V]L(H|P)","(Semi)?Off","devpwr::SetTransonMode");
    emecMap_re("button::PLAYPAUSE_(DP|TP)","(Semi)?Off","devpwr::SetTransonMode");
    emecMap_re("playerstate::TransIdle2Idle","Transon","devpwr::SetOnMode");
    emecMap_re("gpio::115rising","DockStart","devpwr::SetTransonMode");
    //TargetOn
    //FlowPlayPauseLHExitStandby
    emecMap_re("button::PLAYPAUSE_LH","Standby","playpause::button_SP");//button::PLAYPAUSE_LH == button::PLAYPAUSE_SP
    //FlowUnDock
    emecMap_re("gpio::115rising","Docked","devpwr::SetTransonMode"); //2021/0106
    emecMap_re("playerstate::(?!recover).+2(Idle|On)","Standby","devpwr::SetOnMode");

    //TargetDocked SeqStartStartupDock
    emecMap_re("gpio::115falling","(?!Shutdown|(Semi)?Off)","devpwr::SetDockedMode");
    emecMap_re("playerstate::Init2Idle" ,"DockStart", "devpwr::SetDockedMode"); //20210106

    //TargetDocked SeqStartStartupDock
    emecMap_re("gpio::115falling", "Shutdown", "devpwr::SetDockStartMode");

    //TargetOff
    emecMap_re("button::ACTION_LH", "(On|Standby|Transon)","devpwr::SetTransOffMode");
    emecMap_re("gpio::115rising", "VehicleSleep","devpwr::SetTransOffMode");

    //TargetOff SeqStartUSBWakeUp
    emecMap_re("gpio::1275rising", "Off", "devpwr::SetSemiOffMode");//Docked2Stanby

    emecMap_re("PwrDlvr::(?!recover).+2ConnHost.+", "Off", "devpwr::SetSemiOffMode");

    //Target VehicleSleep
    emecMap_re("ble::command:5","Docked","devpwr::SetVehicleSleepMode");

}

void DevicePower::Input()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    //from stanby to On
    em2mMap("devpwr::SetOnMode", "Standby", "On");

    //test mode support (2021106)
    em2mMap_re("devpwr::EnterTestMode", "(?!(Semi)?Off|Docked).+", "Test");
    em2mMap_re("devpwr::ExitTestMode", "Test", "On");

    em2aMap("devpwr::SetStreamChannel", "On", [this] {
        pstDevicePower->mStreamingChannels = 1;
        return (shared_ptr<string>)nullptr;
    });

    //TargetOn SeqStartStartup & SeqGroupPowerOn
    em2mMap_re("devpwr::SetTransonMode", "(Shutdown|(Semi)?Off|DockStart|Docked)", "Transon");
    em2mMap_re("devpwr::SetOnMode", "(Transon|Standby)", "On");

    //TargetStandby
    em2mMap("devpwr::gotoStandby", "On", "Standby");

    //TargetDocked SeqStartStartupDock
    em2mMap_re("devpwr::SetDockedMode", "(?!Shutdown|(Semi)?Off)", "Docked");
    em2mMap_re("devpwr::SetDockedMode", "DockStart", "Docked");

    //TargetDocked SeqStartStartupDock
    em2mMap_re("devpwr::SetDockStartMode", "Shutdown", "DockStart");

    //TargetOff
    em2mMap_re("devpwr::SetTransOffMode", "(On|Standby|Transon|VehicleSleep)", "TransOff");
    em2mMap_re("devpwr::powerOff", "(On|Transon|Standby)", "TransOff");
    //including recover2
    e2aMap_re("devpwr::shutpower", [this] {
        exec_cmd("echo 1 > /sys/class/gpio/gpio114/value");
        if(!mPlayerStates->usbhost_present) {
            exec_cmd("echo 0 > /sys/class/gpio/gpio34/value");//light USB icon for charging indication
            exec_cmd("echo 0 > /sys/class/gpio/gpio113/value");
            sendPayload(make_shared<string>("devpwr::SysDisabled"));
            return setMode("Off");
        }
        return setMode("SemiOff");
    });
    //TargetOff SeqStartUSBWakeUp
    em2mMap("devpwr::SetSemiOffMode", "Off", "SemiOff");

    em2mMap("devpwr::SetVehicleSleepMode","Docked","VehicleSleep");
}

void DevicePower::Internal()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    e2aMap_re("playerstate::recover2(?!On).+", [this] {pstDevicePower->mStreamingChannels = 0; return (shared_ptr<string>)nullptr;});

    e2aMap_re("devpwr::Docked2(?!(Docked|VehicleSleep)).+", [this] {
        std::thread (&ModeFlow::long_actions,this, [this]{return SysEn();}).detach();
        return (shared_ptr<string>)nullptr;
    });

    //TargetOn SeqStartStartup
    e2taMap_re("devpwr::Shutdown2Transon","devpwr::startup_connection_enable",
               bind(&DevicePower::connection_enable_startup,this));

    //TargetOn SeqGroupPowerOn
    e2aMap_re("devpwr::Transon2On",[this] {
        if(pstDevicePower->docked) {
            return setMode("Docked");
        }
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("devpwr::.+2Docked",[this] {
        if(!pstDevicePower->docked) {
            return setMode("Transon");
        }
        return (shared_ptr<string>)nullptr;
    });

    //TargetOn
    //FlowUnDock
    emecMap_re("devpwr::Docked2Transon","Transon","devpwr::SetOnMode"); //2021/0106

    //TargetDocked SeqDockedBatCheck BatteryCheck
    em2aMap("devpwr::Docked:startBatCheckTimer", "Docked",[this] {
        std::thread (&ModeFlow::delay_thread_timer, this, "DevicePower:BatSatusDetect",3, 0).detach();
        return (shared_ptr<string>)nullptr;
    });

    //TargetOff SeqStartUSBWakeUp

    e2dmeMap_re("devpwr::Off2SemiOff", "devpwr::EnableIconDriver", "SemiOff", 0, 100);
    em2aMap("devpwr::EnableIconDriver", "SemiOff", [this] {
        exec_cmd("echo 1 > /sys/class/gpio/gpio34/value");
        return make_shared<string>("devpwr::IconDriverEnabled");
    });
}

void DevicePower::Output()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    //TargetOn SeqStartStartup
    ecMap_re("devpwr::Shutdown2Transon","led::9");


    //TargetOn SeqGroupPowerOn
    e2aMap_re("devpwr::((Semi)?Off|Docked|recover)2Transon",
              bind(&DevicePower::sysAndLed_enable,this));
    e2aMap_re("devpwr::((Semi)?Off|Docked|recover)2Transon",
              bind(&DevicePower::connection_enable,this));
    //Undock: reconnect, if failed, go to disacoverable
    e2aMap_re("devpwr::Docked2Transon",[this] {
        if( access("/data/bt_last_connected_address.txt", F_OK ) == 0 ) {
            sendAdkMsg("connectivity_bt_connect{}");
        }

        return (shared_ptr<string>)nullptr;
    });
    ecMap_re("devpwr::Transon2On","amp::PowerOn");
    e2deMap_re("devpwr::Transon2On", "indication::gotoPowerOn", 1,500);
    //TargetDocked FlowSysDis
    e2aMap_re("devpwr::(?!recover).+2Docked", [this] {return SysDis();});
    //TargetDocked
    ecMap_re("devpwr::Docked2(?!TransOff|Docked|VehicleSleep).+","btman::resume"); //Docked has already been transfered

    //TargetOff
    ecMap_re("devpwr::.+2TransOff", "playerstate::setTransOff","source::poweroff");
    e2aMap_re("devpwr::.+2(Semi)?Off", [this] {
        if(pstDevicePower->docked) {
            return setMode("TransOutDocked");
        }
        return (shared_ptr<string>)nullptr;
    });
    //TargetOff SeqStartUSBWakeUp
    e2aMap("devpwr::Off2SemiOff", [this] {
        exec_cmd("echo 1 > /sys/class/gpio/gpio113/value");
        return make_shared<string>("devpwr::SysEnabled");
    });

    //ConnectorAMP
    ecMap_re("devpwr::StreamStarted", "amp::PowerOn");
    emecMap_re("devpwr::StreamStopped","On", "amp::Sleep");
    emecMap_re("devpwr::StreamStopped","(Standby|(Trans|Semi)?Off)", "amp::PowerOff");
    ecMap_re("devpwr::.+2Docked", "amp::PowerOff");
    //ecMap_re("devpwr::.+2(Semi)?Off", "amp::PowerOff");//Will automatically power off atfer power off tone.

    e2dmeMap_re("devpwr::.+2Standby", "amp::OffIfNotOn", "Standby", 6,0);
    em2aMap_re("devpwr::(Semi)?Off2TransOutDocked", "TransOutDocked", [this] {
        sendPayload(make_shared<string>("source::establish"));
        return (shared_ptr<string>)nullptr;
    });

    e2dmeMap_re("devpwr::Docked2Transon","devpwr::On:bt_check_UndockConnect", "On",10,0);
}

void DevicePower::IOConnector()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    ButtonFilter();
    ConnectorSource();

    em2eMap_re("faultm::amp_unprotected","On","amp::PowerOn");//2021106


    emecMap_re("bt::disabled", "TransOutDocked", "source::establish");

    em2eMap_re("Thread::btman:check_TimeoutState",".+","bt_check_TimeoutState");

    ///Charging LED display
#ifdef ENGINEERING
    em2aMap_re("Bat::led:[0-9]+","(?!Test).+",[this] {
        std::string event = getEvent();
        std::string msg = std::string("led_start_pattern{pattern:") + event.substr(event.find_last_of(":")+1) + std::string("}");

        if(getCurrentMode().find("Docked") != string::npos) {
            if(event.find("30") != string::npos) {
                sendAdkMsg("led_start_pattern{pattern:56}");
            } else if(event.find("31") != string::npos) {
                sendAdkMsg("led_start_pattern{pattern:57}");
            } else if(event.find("32") != string::npos) {
                sendAdkMsg("led_start_pattern{pattern:27}");
            } else if(event.find("33") != string::npos) {
                sendAdkMsg("led_start_pattern{pattern:59}");
            }
        }
        return (shared_ptr<string>)nullptr;
    });
#else
    em2aMap_re("Bat::led:[0-9]+","(?!Docked|Test).+",[this] {
        std::string event = getEvent();
        std::string msg = std::string("led::") + event.substr(event.find_last_of(":")+1);
        sendPayload(make_shared<string>(msg));
        return (shared_ptr<string>)nullptr;
    });
#endif

    //TargetOn
    InterfaceAudio();

    //TargetOn SeqGroupPowerOn
    e2aMap_re("gpio::115falling",[this] {
        pstDevicePower->docked = true;
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("gpio::115rising",[this] {
        pstDevicePower->docked = false;
        return (shared_ptr<string>)nullptr;
    });
    //FlowPowerOnReconnect
    em2aMap_re("playerstate::(?!TransOff).+:bt_enabled","(?!TransOutDocked|Docked|(Trans|Semi)?Off).+",
    //em2aMap_re("bt::enabled","(?!TransOutDocked|Docked|(Trans|Semi)?Off).+",
    [this] {
        if( access("/data/bt_last_connected_address.txt", F_OK ) == 0 ) {
            sendAdkMsg("connectivity_bt_connect{}");
        }
        return (shared_ptr<string>)nullptr;
    });

    //TargetStandby connectorIcon
    emecMap_re("playerstate::(?!recover).+2Standby","(?!TransOutDocked|Docked).+","icon::IconDim");

    //TargetDocked FlowDockedConnectionRefuse
    em2aMap_re("bt::connected","(TransOutDocked|Docked)",
    [this] {
        sendAdkMsg("connectivity_bt_disconnect{}");
        return (shared_ptr<string>)nullptr;
    });
    //TargetDocked
    em2deMap_re("btman::Connected2Disconnected", "Docked", "btman::silent",2,0);//delay is a must, or else bt mode can't be changed.

    //TargetDocked SeqDockedBatCheck BatteryCheck
    em2aMap_re("Timer::DevicePower:BatSatusDetect", "(Docked|VehicleSleep)", [this] {return BatSatusDetect();});
    //system enable and issue command to check battery
    em2aMap_re("Timer::checkBat", "(Docked|VehicleSleep)", [this] {
        exec_cmd("/etc/initscripts/board-script/DockEnI2C5.sh");
        return make_shared<string>("devpwr::Docked:checkBat");
    });
    em2aMap_re("devpwr::setTimertoCheckBat", "(Docked|VehicleSleep)",[this] {
        //This is for Engineering build to run on EVT hw
#ifdef ENGINEERING
#else
        // exec_cmd("echo 0 > /sys/class/gpio/gpio34/value");
        //exec_cmd("echo 0 > /sys/class/gpio/gpio113/value");
#endif

        //exec_cmd("echo \"Off\" > /dev/shm/AMPInitStatus");
        std::thread (&ModeFlow::delay_thread_timer, this, "DevicePower:BatSatusDetect",3, 0).detach();
        //return make_shared<string>("devpwr::sys_disabled");
        return (shared_ptr<string>)nullptr;
    });

    //TargetOff
    //including recovery
    emecMap_re("PwrDlvr::.+2DisConn", "SemiOff",
               "actions::echo 0 > /sys/class/gpio/gpio34/value");//light USB icon for charging indication
    //including recovery
    em2deMap_re("PwrDlvr::.+2DisConn","SemiOff", "PwrDlvr::DisableSys",0, 100);
    em2aMap("PwrDlvr::DisableSys", "SemiOff", [this] {
        exec_cmd("echo 0 > /sys/class/gpio/gpio113/value");
        sendPayload(make_shared<string>("devpwr::SysDisabled"));
        return setMode("Off");
    });
    //SeqStartUSBWakeUp
    //remove strange wakelock
    emecMap_re("gpio::detectTimer", "Off", "actions::echo 110 > /sys/power/wake_unlock"); //remove unkown wakelock created by led-manager for unkown reason in unkown way

    e2aMap_re("wifi::disconnected", [this] {//mark all wifi channels closed.
        pstDevicePower->mStreamingChannels &= ~0xfc;
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

}

void DevicePower::ConnectorSource()
{
    //Connector
    emecMap_re("ota::wantStartButWlanNotUp","Docked", "source::enable_wifi");
    emecMap_re("ota::raucFinished","(Docked|Off)", "source::disable_wifi");
}

void DevicePower::InterfaceAudio()
{
    em2aMap_re("devpwr::playbt", "(?!(Trans|Semi)?Off).+", [this] {
        pstDevicePower->mStreamingChannels |= 1;
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

    em2aMap_re("devpwr::playphone", "(?!(Trans|Semi)?Off).+", [this] {
        pstDevicePower->mStreamingChannels |= 1 << 1;
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

    em2aMap_re("devpwr::playavs", "(?!(Trans|Semi)?Off).+", [this] {
        pstDevicePower->mStreamingChannels |= 1 << 2;
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

    em2aMap_re("devpwr::playairplay", "(?!(Semi)?Off).+", [this] {
        pstDevicePower->mStreamingChannels |= 1 << 3;
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

    em2aMap_re("devpwr::playspotify", "(?!(Semi)?Off).+", [this] {
        pstDevicePower->mStreamingChannels |= 1 << 4;
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

    em2aMap_re("devpwr::playsnapclient", "(?!(Semi)?Off).+", [this] {
        pstDevicePower->mStreamingChannels |= 1 << 5;
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

    em2aMap_re("devpwr::playtone", "(?!(Semi)?Off).+", [this] {
        pstDevicePower->mStreamingChannels |= 1 << 7;
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

    em2aMap_re("devpwr::play(bt|avs|tone|phone|airplay|spotify|snapclient)", "(On|Standby)", [this] {
        return make_shared<string>("devpwr::StreamStarted");
    });

    em2aMap_re("devpwr::stopbt", "(?!(Semi)?Off).+", [this] {
        pstDevicePower->mStreamingChannels &= ~1;
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

    em2aMap_re("devpwr::stopphone", "(?!(Semi)?Off).+", [this] {
        pstDevicePower->mStreamingChannels &= ~(1 << 1);
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

    em2aMap_re("devpwr::stopavs", "(?!(Semi)?Off).+", [this] {
        pstDevicePower->mStreamingChannels &= ~(1 << 2);
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

    em2aMap_re("devpwr::stopairplay", "(?!(Semi)?Off).+", [this] {
        pstDevicePower->mStreamingChannels &= ~(1 << 3);
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

    em2aMap_re("devpwr::stopspotify", "(?!(Semi)?Off).+", [this] {
        pstDevicePower->mStreamingChannels &= ~(1 << 4);
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

    em2aMap_re("devpwr::stopsnapclient", "(?!(Semi)?Off).+", [this] {
        pstDevicePower->mStreamingChannels &= ~(1 << 5);
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

    e2aMap_re("devpwr::stopcue", [this] {
        pstDevicePower->mStreamingChannels &= ~(1 << 7);
        return make_shared<string>(string("devpwr::streaming:") + std::to_string(pstDevicePower->mStreamingChannels >> 4) + std::to_string(pstDevicePower->mStreamingChannels & 0x0f ));
    });

//seq dependenent
    em2dmeMap_re("devpwr::streaming:00", "(?!Standby).+","devpwr::checkStream","(?!Standby).+",1,0);
    e2aMap("devpwr::checkStream", [this]() {
        if(pstDevicePower->mStreamingChannels == 0){
            return make_shared<string>("devpwr::StreamStopped");
        }else{
            return (shared_ptr<string>)nullptr;
        }
    });
}

void DevicePower::ButtonFilter()
{
//lantern:in lantern.cc
    emecMap_re("button::LANTERN_SP","(On|Standby)","lantern::button_SP");
    emecMap_re("button::LANTERN_DP","(On|Standby)","lantern::button_DP");
    emecMap_re("button::LANTERN_LH","(On|Standby)","lantern::button_LH");
    emecMap_re("button::LANTERN_TP","(On|Standby)","lantern::classic_TP");
    emecMap_re("button::CAMPFIRE_LH","(On|Standby)","lantern::campfirebtn_LH");

//connect: in bt.cc
    emecMap_re("button::CONNECT_SP","(On|Standby)","connect::button_SP");
    emecMap_re("button::CONNECT_DP","(On|Standby)","connect::button_DP");
    emecMap_re("button::CONNECT_LH","(On|Standby)","connect::button_LH");

//volume: in volume.cc
    emecMap_re("button::VOLDOWN_SP","(On|Standby)","volume::bdn_SP");
    emecMap_re("button::VOLDOWN_LP","(On|Standby)","volume::bdn_LP");
    emecMap_re("button::VOLDOWN_LH","(On|Standby)","volume::bdn_LH");
    emecMap_re("button::VOLUP_SP","(On|Standby)","volume::bup_SP");
    emecMap_re("button::VOLUP_LP","(On|Standby)","volume::bup_LP");
    emecMap_re("button::VOLUP_LH","(On|Standby)","volume::bup_LH");

//playpause in playpause.cc
    emecMap_re("button::PLAYPAUSE_SP","(On|Standby)","playpause::button_SP");
    emecMap_re("button::PLAYPAUSE_DP","(On|Standby)","playpause::button_DP");
    emecMap_re("button::PLAYPAUSE_TP","(On|Standby)","playpause::button_TP");

}

shared_ptr<string> DevicePower::SysDis()
{
    //exec_cmd("adk-message-send  'button_pressed {button: \"PLAYPAUSE\"}'");

    //This is for Engineering build to run on EVT hw
#ifdef ENGINEERING
#else
//In docked mode, keep sys enabled
//exec_cmd("echo 0 > /sys/class/gpio/gpio113/value");
    sendPayload(make_shared<string>("SysDis"));
#endif

    return make_shared<string>("devpwr::Docked:startBatCheckTimer");
}

shared_ptr<string> DevicePower::SysEn()
{
    //exec_cmd("echo 1 > /sys/class/gpio/gpio113/value");
    return make_shared<string>("SysEn");
}

shared_ptr<string> DevicePower::BatSatusDetect()
{
    sendTimerPayload(make_shared<string>("Timer::checkBat"));
    return (shared_ptr<string>)nullptr;
}

shared_ptr<string> DevicePower::connection_enable()
{
    return make_shared<string>("source::establish");
}

shared_ptr<string> DevicePower::connection_enable_startup()
{

    return make_shared<string>("");
}

shared_ptr<string> DevicePower::sysAndLed_enable()
{
    exec_cmd("echo 1 > /sys/class/gpio/gpio113/value");
    usleep(100000);
    exec_cmd("echo 1 > /sys/class/gpio/gpio34/value &");
    exec_cmd("echo 0 > /sys/class/gpio/gpio114/value &");

    return make_shared<string>("devpwr::SysEnabled");
}
