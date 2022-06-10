/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */
#include <iostream>       // std::cout
#include <sstream>
#include <cstring>
#include <string>
#include <thread>         // std::thread, std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds
#include "bt.h"
#include "external.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>


using adk::msg::AdkMessageService;
//using adk::mode;

Bluetooth::Bluetooth():ModeFlow("btman")
{
    msgProc();
#if 0
    em2mMap_re("bt::enabled","(?!Disconnected).+","Enabled");
    em2mMap_re("bt::setdiscoverable","(?!DiscoverableWait|Discoverable).*","Discoverable");
    e2mMap("bt::discoverable", "Discoverable");
    em2aMap_re("devpwr::(?!recover).+2Docked","Connected", [this] {sendAdkMsg("connectivity_bt_disconnect{}"); return make_shared<string>("Log::btman:DisconnectAsDocked");});

    e2mMap_re("bt::connected", "Connected");
    em2mMap_re("audio::bt:Playing","(?!Connected).+","Connected");//sometimes streaming, not bt connected msg not received

    em2mMap_re("bt::disconnected", "(Connected|Connecting)", "Disconnected");

    em2mMap("bt::paired", "Discoverable", "Disconnected");

    em2aMap_re("bt::setdiscoverablewait","(Disconnected|Enabled|Connected)",[this] {
        mDiscovWaitThreadCounter++;

        std::thread (&ModeFlow::delay_thread, this, "btman:check_TimeoutState",30, 0).detach();
        return make_shared<string>("btman::discovWait");
    });
    e2mMap("btman::discovWait","DiscoverableWait");

    em2aMap_re("btman::(?!recover).+2DiscoverableWait","DiscoverableWait",[this] {
        sendAdkMsg("connectivity_bt_setdiscoverable {timeout:0}");
        return (shared_ptr<string>)nullptr;
    });
//    em2mMap_re("devpwr::(Docked|Standby):bt_check_TimeoutState","DiscoverableWait", "Discoverable");

    e2aMap_re("devpwr::(On|Standby):bt_check_TimeoutState",[this] {
        --mDiscovWaitThreadCounter;
        return (shared_ptr<string>)nullptr;
    });

    em2aMap_re("devpwr::(On|Standby):bt_check_TimeoutState","DiscoverableWait", [this] {
        if(mDiscovWaitThreadCounter == 0) {
            if( access("/data/bt_last_connected_address.txt", F_OK ) == 0 ) {
                sendAdkMsg("connectivity_bt_connect{}");
                sendAdkMsg("connectivity_bt_exitdiscoverable {}");
            }
        }
        return setMode("Disconnected");;
    });

    em2aMap_re("connect::button_(LH|SP|DP)","Discoverable.*", [this] {
        if( access("/data/bt_last_connected_address.txt", F_OK ) == 0 ) {
            sendAdkMsg("connectivity_bt_connect{}");
            sendAdkMsg("connectivity_bt_exitdiscoverable {}");
        }
        return (shared_ptr<string>)nullptr;
    });

    em2aMap_re("devpwr::On:bt_check_UndockConnect","(?!Connected).+", [this] {
        //sendAdkMsg("audio::prompt_play{type:\"tone\" name:\"r1-ErrorSound\"}");
//		sendAdkMsg("connectivity_bt_setdiscoverable {timeout:0}");
        return make_shared<string>("btman::gotoStandby");
    });

    em2aMap_re("btman::(?!recover).+2Enabled","Enabled", [this] {
        std::thread (&ModeFlow::delay_thread, this, "btman:StartupConnectCheck",30, 0).detach();
        return (shared_ptr<string>)nullptr;
    });

    em2aMap_re("playerstate::Standby2Idle","(?!Connected).+", [this] {
        std::thread (&ModeFlow::delay_thread, this, "btman:MaunalOnCheck",30, 0).detach();
        return (shared_ptr<string>)nullptr;
    });


    em2aMap_re("Thread::btman:StartupConnectCheck","Enabled", [this] {
        //sendAdkMsg("audio::prompt_play{type:\"tone\" name:\"r1-ErrorSound\"}");
        //sendAdkMsg("connectivity_bt_setdiscoverable {timeout:0}");
        return make_shared<string>("btman::gotoStandby");
    });

    em2aMap_re("Thread::btman:MaunalOnCheck","(?!Connected).+", [this] {
        //sendAdkMsg("audio::prompt_play{type:\"tone\" name:\"r1-ErrorSound\"}");
        //sendAdkMsg("connectivity_bt_setdiscoverable {timeout:0}");
        return make_shared<string>("btman::gotoStandby");
    });

    em2mMap_re("bt::connect","(Enabled|Disconnected)", "Connecting");
    em2mMap("btman::gotoDisconnected","Connecting", "Disconnected");

    em2aMap_re("btman::(?!recover).+2Connecting","Connecting", [this] {
        std::thread (&ModeFlow::delay_thread, this, "btman:check_SPConnect",10, 0).detach();
        return (shared_ptr<string>)nullptr;

    });//If 5 seconds later, bt is not connected, goto standby

    em2aMap_re("Thread::btman:check_SPConnect","Connecting", [this] {

        return make_shared<string>("btman::gotoDisconnected");
    });


    em2mMap_re("bt::exitdiscoverable","Discoverable.*", "Disconnected");
    em2aMap_re("bt::exitdiscoverable","Disconnected", [this] {
        mConnectWaitThreadCounter++;
        std::thread (&ModeFlow::delay_thread, this, "btman:check_state",10, 0).detach();
        return (shared_ptr<string>)nullptr;
    });//If 10 seconds later, bt is not connected, goto standby


    em2aMap_re("Thread::btman:check_state","Disconnected", [this] {
        if(--mConnectWaitThreadCounter == 0) {
            sendPayload(make_shared<string>("Tone::sound:r1-PowerDown"));
            return make_shared<string>("btman::gotoStandby");
        }
        return (shared_ptr<string>)nullptr;
    });

    em2aMap_re("Thread::btman:check_state","Discoverable", [this] {
        if(--mConnectWaitThreadCounter == 0) {
            sendPayload(make_shared<string>("Tone::sound:r1-PowerDown"));
            //sendAdkMsg("connectivity_bt_setdiscoverable {timeout:0}");
            return make_shared<string>("btman::gotoStandby");
        }
        return (shared_ptr<string>)nullptr;
    });

    e2mMap("bt::disabled", "Disabled");
    BtIndication();
    BtInterface();
    IOConnector();
#endif

    string recoverInfo = modeRecovery("Disabled");
    std::thread(&ModeFlow::init_actions,this, [this, recoverInfo] {return make_shared<string>(recoverInfo);}).detach();

}
void Bluetooth::IfConnector() {
    emecMap_re("bt::enabled","(?!Disconnected).+","btman::SetEnabled");
    emecMap_re("bt::(setdiscoverable|discoverable)","(?!DiscoverableWait|Discoverable).+","btman::SetDiscoverable");
    emecMap_re("bt::disconnected", "(Connected|Connecting)", "btman::SetDisconnected");
    emecMap_re("bt::paired", "Discoverable", "btman::SetPaired");
    emecMap_re("bt::connect","(Enabled|Disconnected)", "btman::SetConnecting");
    emecMap_re("bt::exitdiscoverable","Discoverable.*", "btman::SetExitDiscoverable");
    ecMap_re("bt::disabled", "btman::SetDisabled");
    ecMap_re("bt::connected", "btman::SetConnected");
    emecMap_re("connect::button_LH","(Disconnected|Enabled|Connected)","bt::setdiscoverablewait");
}

void Bluetooth::Input() {
    em2mMap_re("btman::SetEnabled", "(?!Disconnected).+", "Enabled");
    em2mMap_re("btman::SetDiscoverable", "(?!DiscoverableWait|Discoverable).+", "Discoverable");
    em2mMap_re("btman::SetDisconnected", "(Connected|Connecting)", "Disconnected");
    em2mMap_re("btman::SetPaired", "Discoverable", "Disconnected");
    em2mMap_re("btman::gotoDisconnected", "(Connecting|Connected)", "Disconnected");
    em2mMap_re("btman::SetConnecting", "(Enabled|Disconnected)", "Connecting");
    em2mMap_re("btman::SetExitDiscoverable","Discoverable.*", "Disconnected");
    e2mMap_re("btman::SetDisabled","Disabled");
    e2mMap_re("btman::discovWait", "DiscoverableWait");
    e2mMap_re("btman::SetConnected", "Connected");

//record power stat
    e2aMap_re("devpwr::.+2Transon",[this]{
        power_st = 2;
        return make_shared<string>("Log::btman:source:power_st:2");
    });

    e2aMap_re("source::poweroff",[this]{
        power_st = 1;
        return make_shared<string>("Log::btman:power_st:1");
    });

    em2aMap_re("Thread::btman:StartupConnectCheck", "Enabled", [this] {
        //sendAdkMsg("audio::prompt_play{type:\"tone\" name:\"r1-ErrorSound\"}");
        //sendAdkMsg("connectivity_bt_setdiscoverable {timeout:0}");
        return make_shared<string>("btman::gotoStandby");
    });
    em2aMap_re("Thread::btman:MaunalOnCheck","(?!Connected).+", [this] {
        //sendAdkMsg("audio::prompt_play{type:\"tone\" name:\"r1-ErrorSound\"}");
        //sendAdkMsg("connectivity_bt_setdiscoverable {timeout:0}");
        return make_shared<string>("btman::gotoStandby");
    });
    em2aMap_re("Thread::btman:check_SPConnect","Connecting", [this] {

        return make_shared<string>("btman::gotoDisconnected");
    });
    em2aMap_re("Thread::btman:check_state","(Disconnected|Discoverable)", [this] {
        if(--mConnectWaitThreadCounter == 0) {
            sendPayload(make_shared<string>("Tone::sound:r1-PowerDown"));
            return make_shared<string>("btman::gotoStandby");
        }
        return (shared_ptr<string>)nullptr;
    });
    // em2aMap_re("Thread::btman:check_state","Discoverable", [this] {
    //     if(--mConnectWaitThreadCounter == 0) {
    // 		sendPayload(make_shared<string>("Tone::sound:r1-PowerDown"));
    //         //sendAdkMsg("connectivity_bt_setdiscoverable {timeout:0}");
    //         return make_shared<string>("btman::gotoStandby");
    //     }
    //     return (shared_ptr<string>)nullptr;
    // });

    em2aMap_re("btman::startBtApp", "Disabled",[this] {
        if(power_st != 1)
        {
            return make_shared<string>("actions::systemctl restart btapp");
        }
        return (shared_ptr<string>)nullptr;
    });
    em2aMap_re("btman::startBtApp", "Disconnected",[this] {
        if( access("/data/bt_last_connected_address.txt", F_OK ) == 0 ) {
            if(power_st != 1)
            {
                sendAdkMsg("connectivity_bt_connect{}");
            }
        }
        return (shared_ptr<string>)nullptr;
    });
    em2aMap_re("btman::stopBtApp", "(?!Disabled).+",[this] {
        sendAdkMsg("connectivity_bt_disable{}");
        return (shared_ptr<string>)nullptr;
    });
    em2aMap_re("btman::silent", "(?!Connected).+", std::bind(&Bluetooth::silentClassic, this));
    e2aMap("btman::resume",  std::bind(&Bluetooth::resumeClassic, this));

}

void Bluetooth::Internal() {
    em2mMap_re("audio::bt:Playing","(?!Connected).+","Connected");//sometimes streaming, not bt connected msg not received
    em2aMap_re("bt::setdiscoverablewait","(Disconnected|Enabled|Connected)",[this] {
        mDiscovWaitThreadCounter++;

        std::thread (&ModeFlow::delay_thread, this, "btman:check_TimeoutState",30, 0).detach();
        return make_shared<string>("btman::discovWait");
    });
    e2aMap_re("devpwr::(On|Standby):bt_check_TimeoutState",[this] {
        --mDiscovWaitThreadCounter;
        return (shared_ptr<string>)nullptr;
    });
    em2aMap_re("btman::(?!recover).+2Enabled","Enabled", [this] {
        std::thread (&ModeFlow::delay_thread, this, "btman:StartupConnectCheck",30, 0).detach();
        return (shared_ptr<string>)nullptr;
    });
    em2aMap_re("playerstate::Standby2Idle","(?!Connected).+", [this] {
        std::thread (&ModeFlow::delay_thread, this, "btman:MaunalOnCheck",30, 0).detach();
        return (shared_ptr<string>)nullptr;
    });
    em2aMap_re("btman::(?!recover).+2Connecting","Connecting", [this] {
        std::thread (&ModeFlow::delay_thread, this, "btman:check_SPConnect",10, 0).detach();
        return (shared_ptr<string>)nullptr;

    });//If 5 seconds later, bt is not connected, goto standby
    em2aMap_re("bt::exitdiscoverable","Disconnected", [this] {
        mConnectWaitThreadCounter++;
        std::thread (&ModeFlow::delay_thread, this, "btman:check_state",10, 0).detach();
        return (shared_ptr<string>)nullptr;
    });//If 10 seconds later, bt is not connected, goto standby

}

void Bluetooth::Output() {
    BtIndication();
    em2aMap_re("btman::(?!recover).+2DiscoverableWait","DiscoverableWait",[this] {
        sendAdkMsg("connectivity_bt_setdiscoverable {timeout:0}");
        return (shared_ptr<string>)nullptr;
    });
    em2aMap_re("devpwr::(On|Standby):bt_check_TimeoutState","DiscoverableWait", [this] {
        if(mDiscovWaitThreadCounter == 0) {
            if( access("/data/bt_last_connected_address.txt", F_OK ) == 0 ) {
                if(power_st == 2)
                {
                    sendAdkMsg("connectivity_bt_connect{}");
                }
            }
            sendAdkMsg("connectivity_bt_exitdiscoverable {}");
        }
        return setMode("Disconnected");;
    });
    em2aMap_re("connect::button_(LH|SP|DP)","Discoverable.*", [this] {
        if( access("/data/bt_last_connected_address.txt", F_OK ) == 0 ) {
            if(power_st == 2)
            {
                sendAdkMsg("connectivity_bt_connect{}");
            }
        }
        sendAdkMsg("connectivity_bt_exitdiscoverable {}");
        return (shared_ptr<string>)nullptr;
    });
    em2aMap_re("devpwr::On:bt_check_UndockConnect","(?!Connected).+", [this] {
        //sendAdkMsg("audio::prompt_play{type:\"tone\" name:\"r1-ErrorSound\"}");
//		sendAdkMsg("connectivity_bt_setdiscoverable {timeout:0}");
        return make_shared<string>("btman::gotoStandby");
    });

    e2aMap_re("btman::Disabled2Enabled", [this] {
        if( access("/data/bt_last_connected_address.txt", F_OK ) == 0 ) {
            if(power_st != 1)
            {
                sendAdkMsg("connectivity_bt_connect{}");
            }
        }

        return make_shared<string>("Log::btman:reconnect");
    });


}
void Bluetooth::IOConnector()
{
    em2aMap_re("devpwr::(?!recover).+2Docked","Connected", [this] {sendAdkMsg("connectivity_bt_disconnect{}"); return make_shared<string>("Log::btman:DisconnectAsDocked");});
    ecMap_re("bt::disconnected","devpwr::stopbt");
    ConnectorDevpwrAudio();

}
void Bluetooth::BtIndication()
{
//Interface definication(port connection)
    e2deMap_re("btman::(?!Discoverable).+2Discoverable.*", "indication::BtPairing", 2, 0);
    ecMap_re("btman::(?!recover).+2Connected", "indication::BtConnected");
    ecMap_re("btman::Discoverable.*2(?!Connected|Discoverable).+", "indication::BtPairingFailed");
}

#if 0
void Bluetooth::BtInterface()
{
//Interface definication(start stop bt)
    em2aMap_re("btman::startBtApp", "Disabled",[this] {
        return make_shared<string>("actions::systemctl restart btapp");
    });

    em2aMap_re("btman::startBtApp", "Disconnected",[this] {
        if( access("/data/bt_last_connected_address.txt", F_OK ) == 0 ) {
            sendAdkMsg("connectivity_bt_connect{}");
        }
        return (shared_ptr<string>)nullptr;
    });

    em2aMap_re("btman::stopBtApp", "(?!Disabled).+",[this] {
        sendAdkMsg("connectivity_bt_disable{}");
        return (shared_ptr<string>)nullptr;
    });

    em2aMap_re("devpwr::.+2(Semi)?Off", "Connected", [this] {
        sendAdkMsg("connectivity_bt_disconnect{}");
        return make_shared<string>("bt::enabled");
    });

    em2aMap_re("btman::silent", "(?!Connected).+", std::bind(&Bluetooth::silentClassic, this));
    e2aMap("btman::resume",  std::bind(&Bluetooth::resumeClassic, this));

}
#endif


// void Bluetooth::IOConnector()
// {
// 	ecMap_re("bt::disconnected","devpwr::stopbt");
// 	ConnectorDevpwrAudio();

// }

void Bluetooth::ConnectorDevpwrAudio()
{
    ecMap_re("audio::bt:Playing", "devpwr::playbt", "volume::setPortbt", "volume::connectFlowDsp");
    ecMap_re("audio::bt:Stopped","devpwr::stopbt");
}


shared_ptr<string> Bluetooth::silentClassic()
{
    sendAdkMsg("connectivity_bt_setmode{mode:0}"); //It must be issued when is disconnected status
    return make_shared<string>("btman::btsiliented");
}

shared_ptr<string> Bluetooth::resumeClassic()
{
    sendAdkMsg("connectivity_bt_setmode{mode:1}"); //1 - Connectable, 2 - Discoverable
    return make_shared<string>("btman::btresumed");
}

HFP::HFP():ModeFlow("HFPman")
{
    msgProc();

    string recoverInfo = modeRecovery("Off");
    std::thread(&ModeFlow::init_actions,this, [this, recoverInfo] {return make_shared<string>(recoverInfo);}).detach();

}

void HFP::IfConnector()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    ecMap_re("bt::incomingcall", "HFPman::SetCallIn");
    ecMap_re("bt::outgoingcall", "HFPman::SetCallOut");
    ecMap_re("bt::activecall", "HFPman::SetAccepted");
    ecMap_re("bt::endedcall", "HFPman::SetOff");

    emecMap_re("button::PLAYPAUSE_SP", "CallIn", "HFPman::AcceptCall");
    emecMap_re("button::PLAYPAUSE_SP", "(Accepted|CallOut)", "HFPman::EndCall");

    emecMap_re("button::VOLDOWN_SP", "CallIn", "HFPman::RejectCall");
}

void HFP::Input()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    e2mMap("HFPman::SetCallIn", "CallIn");
    e2mMap("HFPman::SetCallOut", "CallOut");
    e2mMap("HFPman::SetAccepted", "Accepted");
    e2mMap("HFPman::SetOff", "Off");

    em2aMap("HFPman::AcceptCall", "CallIn", [this] {sendAdkMsg("connectivity_bt_acceptcall {}"); return (shared_ptr<string>)nullptr;});
    em2aMap_re("HFPman::EndCall", "(Accepted|CallOut)", [this] {sendAdkMsg("connectivity_bt_endcall {}"); return (shared_ptr<string>)nullptr;});

    em2aMap("HFPman::RejectCall", "CallIn",  [this] {sendAdkMsg("connectivity_bt_rejectcall {}"); return (shared_ptr<string>)nullptr;});
}

void HFP::Internal()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);
}

void HFP::Output()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    ecMap_re("HFPman::Off2(Call.+|Accepted)", "devpwr::playphone", "volume::setPortphone", "volume::connectFlowDsp");
    ecMap_re("HFPman::(?!recover).+2Off","devpwr::stopphone");
}

void HFP::IOConnector()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    ecMap_re("btapp::HFPAudioStart", "volume::connectFlowDsp");
}

shared_ptr<string> FlowBLE::restartAdvertising()
{
    sendAdkMsg("example_wifi_ble_stop_advertising{server: 1}");
    usleep(300000);
    sendAdkMsg("example_wifi_ble_start_advertising{server: 1}");
    return make_shared<string>("ble::restartadv");
}

FlowBLE::FlowBLE():ModeFlow("ble")
{
    e2aMap("ota::wantStartButWlanNotConnected", [this] {
        int instance = (0x02 << 8) + 0x01;//size is encoded in instance in byte 2
        string msg = "connectivity_bt_blecmd {instance:" + std::to_string(instance) + ", cmd:0x0102, suuid:\"" + s_uuid_custom
        + "\", cuuid:\"" + c_uuid_conn + "\"}";
        sendAdkMsg(msg);
        return (shared_ptr<string>)nullptr;
    });

    e2aMap("ota::disableVehicleAP", [this] {
        int instance = (0x02 << 8) + 0x01;//size is encoded in instance in byte 2
        string msg = "connectivity_bt_blecmd {instance:" + std::to_string(instance) + ", cmd:0x0202, suuid:\"" + s_uuid_custom
        + "\", cuuid:\"" + c_uuid_conn + "\"}";
        sendAdkMsg(msg);
        return (shared_ptr<string>)nullptr;
    });

    e2aMap("ota::wantStartButWlanNotConnectedNoCredentials", [this] {
        int instance = (0x02 << 8) + 0x01;//size is encoded in instance in byte 2
        string msg = "connectivity_bt_blecmd {instance:" + std::to_string(instance) + ", cmd:0x0302, suuid:\"" + s_uuid_custom
        + "\", cuuid:\"" + c_uuid_conn + "\"}";
        sendAdkMsg(msg);
        return (shared_ptr<string>)nullptr;
    });

    e2aMap("gpio::115falling", [this] {
        int instance = (0x02 << 8) + 0x01;//size is encoded in instance in byte 2
        string msg = "connectivity_bt_blecmd {instance:" + std::to_string(instance) + ", cmd:0x0106, suuid:\"" + s_uuid_custom
        + "\", cuuid:\"" + c_uuid_token_set + "\"}";
        sendAdkMsg(msg);
        return (shared_ptr<string>)nullptr;
    });

    e2aMap("gpio::115rising", [this] {
        int instance = (0x02 << 8) + 0x01;//size is encoded in instance in byte 2
        string msg = "connectivity_bt_blecmd {instance:" + std::to_string(instance) + ", cmd:0x0006, suuid:\"" + s_uuid_custom
        + "\", cuuid:\"" + c_uuid_token_set + "\"}";
        sendAdkMsg(msg);
        return (shared_ptr<string>)nullptr;
    });

    em2aMap("connect::button_DP", "Shy", [this] {

        std::thread (&ModeFlow::delay_thread, this, "ble:switch2Shy",60, 0).detach();

        return setMode("Connectable");
    });

    em2mMap("Thread::ble:switch2Shy", "Connectable", "Shy");

    e2aMap_re("btman::((?!recover).+2DiscoverableWait|DiscoverableWait2.+)",[this] {

        return restartAdvertising();
    });

    e2aMap_re("ble::.+2.+",[this] {
        return restartAdvertising();
    });

    e2aMap_re("Charger::.+2.+",[this] {

        int instance = (0x02 << 8) + 0x01;//size is encoded in instance in byte 2
        string msg = "connectivity_bt_blecmd {instance:" + std::to_string(instance) + ", cmd:0x0, suuid:\"" + s_uuid_custom
        + "\", cuuid:\"" + c_uuid_token_set + "\", log:\"" + std::string(1, (char)0x08) + getEvent().substr(getEvent().find("2") + 1) + "\"}";
        sendAdkMsg(msg);
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("bt::errorresponse:.+", [this] {

        int instance = (0x02 << 8) + 0x01;//size is encoded in instance in byte 2
        string msg = "connectivity_bt_blecmd {instance:" + std::to_string(instance) + ", cmd:0x0, suuid:\"" + s_uuid_custom
        + "\", cuuid:\"" + c_uuid_token_set + "\", log:\"" + std::string(1, (char)0x0B) + getEvent() + "\"}";
        sendAdkMsg(msg);
        return (shared_ptr<string>)nullptr;
    });

    e2aMap("bt::bleconnected:success", [this] {
        FILE *fp;
        char onboarded = '0';

        fp = popen("adkcfg -f /data/adk.connectivity.states.db  list | grep connectivity.states.wifi_onboarded | awk -F '|' '{print $2}'", "r");
        if(fp != NULL) {
            fread(&onboarded, 1, 1, fp);
            pclose(fp);
        }

        if ('0' == onboarded) {
            int instance = (0x02 << 8) + 0x01;
            string msg = "connectivity_bt_blecmd {instance:" + std::to_string(instance) + ", cmd:0x0302, suuid:\"" + s_uuid_custom
            + "\", cuuid:\"" + c_uuid_conn + "\"}";
            sendAdkMsg(msg);
        }
        return (shared_ptr<string>)nullptr;
    });

    modeRecovery("Shy");

}

