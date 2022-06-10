/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MODEFLOW_BT_H
#define MODEFLOW_BT_H
#include "modeflow.h"
#include "external.h"
#include <string>


using adk::msg::AdkMessage;
using adk::msg::AdkMessageService;

class Bluetooth:public ModeFlow
{
public:
    Bluetooth();
    virtual void IfConnector();
    virtual void Input();
    virtual void Internal();
    virtual void Output();
    virtual void IOConnector();
    void BtIndication();
#if 0
    void BtInterface();
#endif
    void ConnectorDevpwrAudio();
    shared_ptr<string>  silentClassic();
    shared_ptr<string>  resumeClassic();
private:
    uint8_t mDiscovWaitThreadCounter = 0;
    uint8_t mConnectWaitThreadCounter = 0;
    uint8_t power_st = 0; //0:init, 1:off, 2:on
};

class HFP:public ModeFlow
{
public:
    HFP();
    virtual void IfConnector();
    virtual void Input();
    virtual void Internal();
    virtual void Output();
    virtual void IOConnector();
};

class Source:public ModeFlow
{
public:
    Source();
    virtual void IfConnector();
    virtual void Input();
    virtual void Internal();
    virtual void Output();
    virtual void IOConnector();
private:
    shared_ptr<string>  gotoWifiMode();
    shared_ptr<string>  gotoBtMode();
    shared_ptr<string>  checkWifiConnected();
    shared_ptr<string>  WifiDisconnected();
    shared_ptr<string>  checkPowerOffBt();
    shared_ptr<string>  checkPowerOffWifi();
    shared_ptr<string>  checkTransBt();
    bool if_wlan0_connected();
    bool check_wpa_config();
    string Multiroom_main1();
    string Multiroom_main2();
    string Multiroom_slave1();
    string Multiroom_slave2();
    struct SourceSt {
        bool wifi_connected = false;
        bool wifi_fail = false;
        bool bt_fail = false;
        uint8_t power_st = 0; //0:init, 1:off,  2:on
        uint8_t enabledSources = 0;//bit 1: bt, bit2:wifi
        uint8_t checkbt_num = 0;//check bit 1:wifi	 bit 2:bt	bit 3:backwifi	 bit 4:backbt
        uint8_t checkwifi_num = 0;
        uint8_t checkbackbt_num = 0;
        uint8_t checkbackwifi_num = 0;
        uint8_t idle_connection = 0x03;//bit 1: bt, bit2:wifi , bit3 alexa//it's the startup value, wether you start up with them on?
        bool rauc_downloading = false;
        bool mode_trans_key = 0; //true: wait 5 sec
        SourceSt() {
        }
    } * pstSource;
    const struct SourceSt mconst_SourceSt;
};

class FlowBLE:public ModeFlow
{
public:
    FlowBLE();
private:
    shared_ptr<string>  restartAdvertising();
    const string s_uuid_custom = "52495649-414E-2052-5320-535441545553";
    const string c_uuid_conn = "434f4e4e-4543-5420-5253-205749464900";
    const string c_uuid_token_set = "554e4956-4552-5341-4c20-444154410000";
};

#endif


