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
#include "modeflow.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fstream>


using adk::msg::AdkMessageService;
//using adk::mode;

Source::Source():ModeFlow("source")
{
    msgProc();

    string recoverInfo = modeRecovery("Init", sizeof(struct SourceSt), 16, (void *)&mconst_SourceSt);
    pstSource = (struct SourceSt*)getShmDataAddr();

    std::thread(&ModeFlow::init_actions,this, [this, recoverInfo] {return make_shared<string>(recoverInfo);}).detach();
}

void Source::IfConnector()
{
//bt enabled
    emecMap_re("bt::enabled","(Init|initbt|lostbt|offbt)","source::setTransBt");
    ecMap_re("bt::connected","source::setBt");

//bt disconnected
    ecMap_re("bt::disconnected","source::setLostBt");

//keytotransmode
    ecMap_re("connect::button_SP","source::modeTrans");//wifi/bt trans mode_trans_key status change;

//review	ecMap_re("audio::source_select_bt", "source::setbt");

//record sources states
    ecMap_re("bt::enabled", "source::bt_enabled");
    ecMap_re("bt::disabled", "source::bt_disabled");
    ecMap_re("wifi::enabled", "source::wifi_enabled");
    ecMap_re("wifi::disabled", "source::wifi_disabled");



//wifi enabled
    emecMap_re("wifi::enabled","(Init|initwifi|lostwifi|offwifi)","source::setTransWifi");

//wifi connected
    em2deMap_re("wifi::connected", "(transwifi|lostwifi|initwifi|wifipart|wififull)", "source::confirmWifiStatus", 1, 0);

//wifi onboarding
    em2deMap_re("wifi::onboarding", "(transwifi|lostwifi|initwifi|wifipart|wififull)", "source::confirmWifiOnboarding", 5, 0);
//wifi completeonboarding
    emecMap_re("wifi::completeonboarding", "(transwifi|lostwifi|initwifi|wifipart|wififull)","source::confirmWifiOnboarding");

    ecMap_re("vu::StatusIdle", "source::setWifiFull");

    emecMap_re("btman::gotoStandby", "(transbt|initbt|lostbt)", "source::setBtStandby");

//FlowPLAYPAUSE2Idle
    /*    em2aMap_re("button::(?!PLAYPAUSE).+_(S|L)P","Standby",
        [this] {
            if(enabledSources) {
                return setMode("Idle");
            }
            return (shared_ptr<string>)nullptr;
        });*/

//TBD	em2mMap("bt::disconnected", "bt", "source::btdisconnected")

}

void Source::Input()
{
//interface
//Init
    ecMap_re("source::Init","indication::InitSource");

//record power stat
    e2aMap_re("devpwr::.+2Transon",[this]{
            pstSource->power_st = 2;
            return make_shared<string>("Log::source:power_st:2");
        });

    e2aMap_re("source::poweroff",[this]{
            pstSource->power_st = 1;
            sendPayload(make_shared<string>("source::disable_wifi"));
            sendPayload(make_shared<string>("btman::stopBtApp"));
            return make_shared<string>("Log::source:power_st:1");
        });

//setTransBt
    e2aMap_re("source::setTransBt", std::bind(&Source::checkTransBt, this));
    
//setBt    
    em2mMap_re("source::setBt","(transbt|lostbt|initbt)","bt");

//setTransWifi
    em2mMap_re("source::setTransWifi","(Init|initwifi|transwifi|lostwifi|offwifi)","transwifi");

//setLostBt
    em2mMap_re("source::setLostBt","(transbt|lostbt|initbt|bt)","lostbt");

//setInitBt
    em2mMap_re("source::setInitBt","(transbt|lostbt|initbt|bt)","initbt");

//setBtStandby
    em2aMap_re("source::setBtStandby", "(transbt|initbt|lostbt)", [this] {
        if( access("/data/bt_last_connected_address.txt", F_OK ) == 0 ) {
            return make_shared<string>("source::setLostBt");
        } else {
            return make_shared<string>("source::setInitBt");
        }
        return (shared_ptr<string>)nullptr;
    });

//keytotransmode
    em2aMap_re("source::modeTrans","(transbt|lostbt|initbt|bt|Init)",[this] {
        if(pstSource->mode_trans_key) {
            pstSource->mode_trans_key = false;
            return make_shared<string>("source::setModeTransWifi");
        }
        pstSource->mode_trans_key = true;
        return make_shared<string>(string("source::modeTrans:") + std::to_string(pstSource->mode_trans_key));
    });

    em2aMap_re("source::modeTrans","(transwifi|lostwifi|initwifi|wifipart|wififull)",[this] {

        if(pstSource->mode_trans_key) {
            pstSource->mode_trans_key = false;
            return make_shared<string>("source::setModeTransBt");
        }
        pstSource->mode_trans_key = true;
        return make_shared<string>(string("source::modeTrans:") +  std::to_string(pstSource->mode_trans_key));
    });

    e2aMap_re("source::clearmodeTrans",[this] {
        if(pstSource->mode_trans_key) {
            pstSource->mode_trans_key = false;
        }
        return make_shared<string>(string("source::modeTrans:") +  std::to_string(pstSource->mode_trans_key));
    });

    em2mMap_re("source::setModeTransBt","(transwifi|lostwifi|initwifi|wifipart|wififull)","transbt");

    //record source enabled states
    e2aMap_re("source::bt_(enabled|connected)",
    [this] {
        pstSource->enabledSources |= 0x01;
        return make_shared<string>(string("Log::source:enabledSources:" + std::to_string(pstSource->enabledSources)));
    });
    e2aMap_re("source::wifi_(enabled|connected)",
    [this] {
        pstSource->enabledSources |= 0x02;
        return make_shared<string>(string("Log::source:enabledSources:" + std::to_string(pstSource->enabledSources)));
    });

    //record source disabled states
    e2aMap_re("source::bt_disabled",
    [this] {
        pstSource->enabledSources &= ~0x01;
        return make_shared<string>(string("source::enabledSources:" + std::to_string(pstSource->enabledSources)));
    });
    e2aMap_re("source::wifi_disabled",
    [this] {
        pstSource->enabledSources &= ~0x02;
        pstSource->wifi_connected = false;
        return make_shared<string>(string("source::enabledSources:" + std::to_string(pstSource->enabledSources)));
    });


//set transwifi
    e2mMap("source::setModeTransWifi", "transwifi");

//set lostwifi or initwifi
    em2mMap_re("source::setLostWifi", "(transwifi|lostwifi|initwifi|wifipart|wififull)", "lostwifi");
    em2mMap_re("source::setInitWifi", "(transwifi|lostwifi|initwifi|wifipart|wififull)", "initwifi");

//set wifipart or wififull
    em2mMap_re("source::setWifiPart", "(transwifi|lostwifi|initwifi|wifipart|wififull)","wifipart");
    em2mMap_re("source::setWifiFull", "wifipart","wififull");
    
//establish/poweron
    emecMap_re("source::establish", "(transbt|lostbt|initbt|offbt|bt)", "btman::startBtApp");
    emecMap_re("source::establish", "(transwifi|lostwifi|initwifi|offwifi|wifipart|wififull)", "source::enable_wifi");



//confirmWifiStatus
    e2aMap_re("source::confirmWifiStatus", std::bind(&Source::checkWifiConnected, this));
    e2aMap_re("source::confirmWifiOnboarding", std::bind(&Source::checkWifiConnected, this));
}

void Source::Internal()
{
//transwifi
    e2deMap_re("source::.+2transwifi", "source::i:checkwifi", 30, 0);
    em2aMap_re("source::i:checkwifi", "transwifi", std::bind(&Source::checkWifiConnected, this));

//transbt
    e2deMap_re("source::.+2transbt", "source::i:checkbt", 30, 0);
    em2aMap_re("source::i:checkbt", "transbt", [this] {
        if( access("/data/bt_last_connected_address.txt", F_OK ) == 0 ) {
            return make_shared<string>("source::setLostBt");
        } else {
            return make_shared<string>("source::setInitBt");
        }
        return (shared_ptr<string>)nullptr;
    });

//keytotransmode
    e2deMap_re("source::modeTrans:1", "source::clearmodeTrans", 5, 0);
}

void Source::Output()
{
//Init2bt
    //startup
    ecMap_re("source::Init2transbt", "playerstate::startupIdle");

    e2aMap_re("source::.+2transwifi", std::bind(&Source::gotoWifiMode, this));
    e2aMap_re("source::.+2transbt", std::bind(&Source::gotoBtMode, this));

    ecMap_re("source::.+2initwifi", "indication::InitWifi");
    ecMap_re("source::.+2lostwifi", "indication::WifiDisconnected");


    ecMap_re("source::.+2initbt", "indication::InitBt");
    ecMap_re("source::.+2lostbt", "indication::BtDisconnected");
    ecMap_re("source::.+2offbt", "indication::OffBt");
    ecMap_re("source::.+2offwifi", "indication::OffWifi");

    e2aMap_re("source::.+2wifipart", [this] {
        sendPayload(make_shared<string>("actions::systemctl restart voiceUI&"));
        sendPayload(make_shared<string>("actions::systemctl restart avs&"));
        sendPayload(make_shared<string>("actions::systemctl restart spotify&"));
        sendPayload(make_shared<string>("actions::systemctl restart airplay&"));
        return make_shared<string>("Log::source:WifiServicesStarted");
    });
    e2aMap_re("source::wifi(part|full)2(?!wifi).+", [this] {
        sendPayload(make_shared<string>("actions::systemctl stop voiceUI&"));
        sendPayload(make_shared<string>("actions::systemctl stop avs&"));
        sendPayload(make_shared<string>("actions::systemctl stop spotify&"));
        sendPayload(make_shared<string>("actions::systemctl stop airplay&"));
        return make_shared<string>("Log::source:WifiServicesStopped");
    });

}

void Source::IOConnector()
{
    e2aMap("source::disable_wifi", [this] {
            sendAdkMsg("connectivity_wifi_disable{}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("source::enable_wifi", [this] {
        sendAdkMsg("connectivity_wifi_enable{}");
        return (shared_ptr<string>)nullptr;
    });

//abnormal bt/wifi response
    // em2deMap_re("bt::disabled", "transbt", "btman::startBtApp",1,0);//owen???
    // em2deMap_re("wifi::disabled", "transwifi", "source::enable_wifi",1,0);//owen???

    //Seq enabledSources disabled 1
    em2aMap_re("source::enabledSources:.+","(lost.+|Init)",
    [this] {
        if(!(pstSource->enabledSources & (pstSource->idle_connection & ~(pstSource->rauc_downloading << 1) ))) {//if downloading, turn off with wifi ON
            return make_shared<string>("playerstate::setOff");
        }
        return (shared_ptr<string>)nullptr;
    });


    e2aMap_re("ota::raucStarted", [this] {
        pstSource->rauc_downloading = true;
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("ota::raucFinished", [this] {
        pstSource->rauc_downloading = false;
        return (shared_ptr<string>)nullptr;
    });

    em2aMap("bt::enabled", "transbt", [this] {
        sendAdkMsg("audio_source_set{name:\"bt\"}");
        return (shared_ptr<string>)nullptr;
    });

    //???Tocheck where normal poweron needs same logic
    ecMap_re("source::wifi_enabled","actions::/etc/initscripts/board-script/set_country_code.sh&");

//poweroff
    em2mMap_re("source::poweroff", "(lostbt|initbt|bt)", "offbt");
    em2mMap_re("source::poweroff", "(lostwifi|initwifi|wifipart|wififull)", "offwifi");

    em2deMap_re("source::poweroff", "transbt", "source::i:checkPowerOffBt",10,0);
    e2aMap_re("source::i:checkPowerOffBt", std::bind(&Source::checkPowerOffBt, this));

    em2deMap_re("source::poweroff", "transwifi", "source::i:checkPowerOffWifi",10,0);
    e2aMap_re("source::i:checkPowerOffWifi", std::bind(&Source::checkPowerOffWifi, this));

    e2aMap_re("source::poweroff", [this] {
        if(pstSource->enabledSources & 0x02) {
            if(!pstSource->rauc_downloading)
            {
                sendPayload(make_shared<string>("source::disable_wifi"));
            }
        }
        
        sendPayload(make_shared<string>("btman::stopBtApp"));
        sendPayload(make_shared<string>("actions::systemctl stop spotify&"));
        sendPayload(make_shared<string>("actions::systemctl stop airplay&"));
        sendPayload(make_shared<string>("actions::systemctl stop avs&"));
        return make_shared<string>("actions::systemctl stop voiceUI&");
    });

    em2aMap_re("volume::FilterBtSyncOut:.+", "bt", [this] {
        string volIndex = getEvent().substr(getEvent().find_last_of(":") + 1);
        return make_shared<string>(string("volume::syncToBt:") + volIndex);
    });
    em2aMap_re("volume::FilterWifiSyncOut:.+", "wifi.+", [this] {
        string volIndex = getEvent().substr(getEvent().find_last_of(":") + 1);
        return make_shared<string>(string("volume::syncToWifi:") + volIndex);
    });
}

shared_ptr<string> Source::gotoWifiMode()
{
    sendPayload(make_shared<string>("btman::stopBtApp"));
    sendAdkMsg("audio_source_set{name:\"wifi\"}");

    if(if_wlan0_connected()) {
        return make_shared<string>("source::setWifiPart");
    } else {
        return make_shared<string>("source::enable_wifi");
    }
    return (shared_ptr<string>)nullptr;;
}

shared_ptr<string> Source::gotoBtMode()
{
    sendPayload(make_shared<string>("source::disable_wifi"));
    if(pstSource->enabledSources & 0x01) {
        sendAdkMsg("audio_source_set{name:\"bt\"}");
    } else {
        if(pstSource->power_st == 2)
        {
            return make_shared<string>("btman::startBtApp");
        }
    }
    return (shared_ptr<string>)nullptr;
}

shared_ptr<string> Source::checkTransBt()
{
    if(pstSource->power_st == 2)
    {
        return setMode("transbt");
    }
    return (shared_ptr<string>)nullptr;
}

shared_ptr<string> Source::checkWifiConnected()
{
    if(if_wlan0_connected()) {
        if(pstSource->wifi_connected == false)
        {
            pstSource->wifi_connected = true;
            return make_shared<string>("source::setWifiPart");
        }
    } else {
        pstSource->wifi_connected = false;
        if( access("/etc/misc/wifi/wpa_supplicant.conf", F_OK ) == 0 ) {
            if(check_wpa_config() == false ) {
                return make_shared<string>("source::setInitWifi");
            } else {
                return make_shared<string>("source::setLostWifi");
            }
        } else {
            return make_shared<string>("source::setLostWifi");
        }
    }
    return (shared_ptr<string>)nullptr;
}


bool Source::check_wpa_config()
{
    FILE *fptr;
    int carrier = 0;
    char str[512] = {0};
    fptr = fopen("/etc/misc/wifi/wpa_supplicant.conf", "r");
    if (NULL != fptr) {
        fread(str, 1, sizeof(str), fptr);  
        fclose(fptr);
    }

    return (strstr(str, "psk") != NULL) ? true : false;
}

bool Source::if_wlan0_connected()
{
    FILE *fptr;
    int carrier = 0;

    fptr = fopen("/sys/class/net/wlan0/carrier", "r");
    if (NULL != fptr) {
        char str[8];
        fscanf(fptr,"%s",str);
        carrier = atoi(str);
        fclose(fptr);
    }

    return (1 == carrier);

}

shared_ptr<string> Source::checkPowerOffWifi()
{
    setMode("offwifi");
    sendPayload(make_shared<string>("source::disable_wifi"));
    sendPayload(make_shared<string>("indication::OffWifi"));
    return (shared_ptr<string>)nullptr;
}

shared_ptr<string> Source::checkPowerOffBt()
{
    setMode("offbt");
    sendPayload(make_shared<string>("btman::stopBtApp"));
    sendPayload(make_shared<string>("indication::OffBt"));
    return (shared_ptr<string>)nullptr;
}
