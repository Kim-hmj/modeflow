/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#include "adk_adaptor.h"
#include <unistd.h>
#include <string>
#include <cstdlib>
#include "adk/log.h"
#include <iostream>
#include <fstream>
#include <memory>
#include <algorithm>
#include <functional>
#include <sstream>
#include "lantern.h"
#include "playpause.h"
#include "otamode.h"
#include "fautM.h"
#include "misc.h"
#include "avs.h"
#include "ota.h"
#include "bt.h"
#include "bat.h"
#include "playerstate.h"
#include "devpower.h"
#include "amptemp.h"
#include "volume.h"
#include "actuators.h"
#include "indication.h"

#include "external.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>



extern SafeQueue<shared_ptr<string>> MsgQue;
namespace adk {
namespace mode {

struct ADKAdaptor::Private {
    Private()
        :message_service_(std::string("action_message_service")) {}
    AdkMessageService message_service_;
};

ADKAdaptor::~ADKAdaptor() = default;

ADKAdaptor::ADKAdaptor(): self_(new Private())
{
    /*
    * Ryder if multipul  sm is defined, the must not listen to the messages  what is sent by another sm, or need to used in different process.
    */
    self_->message_service_.Initialise();
    external_init(&self_->message_service_);
    pmsgProcMfList[current_que_num] = make_unique<MFListQue>(ModeFlow::event2smList,ModeFlow::modeslist,ModeFlow::event2smList_alt);

//Sensors
    new GPIOD();
    new PwrDlvr();
    new Battery();
    new AMPTemp();

//Controllers
    //Ealier Flow
    new Bluetooth(); //Revise class name to use differrent state machine logic
    new HFP(); //Revise class name to use differrent state machine logic
    new Source();

    new ModeOta(); //Revise class name to use differrent state machine logic
    new OTA();
    new FlowBLE();
    pmsgProcMfList[current_que_num]->addmf(new Misc()); //Revise class name to use differrent state machine logic
    new AVS();

    PlayerStates* pactManStreaming = new PlayerStates();
    new DevicePower(pactManStreaming);

    new PlayPause(); //Revise class name to use differrent state machine logic
    new Volume();
    pmsgProcMfList[current_que_num]->addmf(new Indication());

    new Lantern();//Revise class name to use differrent state machine logic
    pmsgProcMfList[current_que_num]->addmf(new Icon());
    pmsgProcMfList[current_que_num]->addmf(new ConnectIcon());
    current_que_num++;
    //Later Flow

//Actuators
    new FaultM(); //Revise class name to use differrent state machine logic
    new AMP();
    new Charger();
    new Actuators(); //Actuators must be the final one so that
}

bool ADKAdaptor::Start() {
    ADK_LOG_INFO("Start()");

    // Subscribe to all the Mode group messages
    if (!self_->message_service_.Subscribe(AdkMessage::kSystemModeManagement,
                                           std::bind(&ADKAdaptor::HandleSystemMode, this, std::placeholders::_1),&subscription_token)) {
        return false;
    }
    handler_id_list.push_back(subscription_token);

    if (!self_->message_service_.Subscribe(AdkMessage::kSystemTimerEvent,
                                           std::bind(&ADKAdaptor::HandleSystemTimerEvent, this, std::placeholders::_1),&subscription_token)) {
        return false;
    }
    handler_id_list.push_back(subscription_token);
    if (!self_->message_service_.Subscribe(adk::msg::kGroupButton,
                                           std::bind(&ADKAdaptor::HandleButtonGroupMsg, this, std::placeholders::_1),&subscription_token)) {
        return false;
    } else
    {
        //register msg to string map
        msg2str[AdkMessage::kButtonPressed] = "SP";
        msg2str[AdkMessage::kButtonLongPressed] = "LP";
        msg2str[AdkMessage::kButtonVeryLongPressed] = "VLP";
        msg2str[AdkMessage::kButtonExtraVeryLongPressed] = "ELP";
        msg2str[AdkMessage::kButtonLongHold] = "LH";
        msg2str[AdkMessage::kButtonVeryLongHold] = "VLH";
        msg2str[AdkMessage::kButtonExtraVeryLongHold] = "ELH";
        msg2str[AdkMessage::kButtonDoublePressed] = "DP";
        msg2str[AdkMessage::kButtonTriplePressed] = "TP";

        handler_id_list.push_back(subscription_token);

    }

    if (!self_->message_service_.Subscribe(adk::msg::kGroupAudio,
                                           std::bind(&ADKAdaptor::HandleAudioGroupMsg, this, std::placeholders::_1),&subscription_token)) {
        return false;
    }
    handler_id_list.push_back(subscription_token);

    if (!self_->message_service_.Subscribe(adk::msg::kGroupConnectivity,
                                           std::bind(&ADKAdaptor::HandleConnectivityGroupMsg, this, std::placeholders::_1),&subscription_token)) {
        return false;
    } else
    {
        //register msg to string map
        msg2strconn[AdkMessage::kConnectivityBtEnabled] = "bt::enabled";
        msg2strconn[AdkMessage::kConnectivityBtDisabled] = "bt::disabled";
        msg2strconn[AdkMessage::kConnectivityBtConnected] = "bt::connected";
        msg2strconn[AdkMessage::kConnectivityBtConnect] = "bt::connect";
        msg2strconn[AdkMessage::kConnectivityBtDisconnected] = "bt::disconnected";
        msg2strconn[AdkMessage::kConnectivityBtIncomingcall] = "bt::incomingcall";
        msg2strconn[AdkMessage::kConnectivityBtOutgoingcall] = "bt::outgoingcall";
        msg2strconn[AdkMessage::kConnectivityBtActivecall] = "bt::activecall";
        msg2strconn[AdkMessage::kConnectivityBtEndcall] = "bt::endcall";
        msg2strconn[AdkMessage::kConnectivityBtEndedcall] = "bt::endedcall";

        msg2strconn[AdkMessage::kConnectivityBtSetdiscoverable] = "bt::setdiscoverable";
        msg2strconn[AdkMessage::kConnectivityBtDiscoverable] = "bt::discoverable";
        msg2strconn[AdkMessage::kConnectivityBtExitdiscoverable] = "bt::exitdiscoverable";
        msg2strconn[AdkMessage::kConnectivityBtErrorresponse] = "bt::errorresponse";
        msg2strconn[AdkMessage::kConnectivityBtBleconnected] = "bt::bleconnected";

        msg2strconn[AdkMessage::kConnectivityWifiConnecting] = "wifi::connecting";
        msg2strconn[AdkMessage::kConnectivityWifiConnected] = "wifi::connected";
        msg2strconn[AdkMessage::kConnectivityWifiOnboarding] = "wifi::onboarding";
        msg2strconn[AdkMessage::kConnectivityWifiDisabled] = "wifi::disabled";
        msg2strconn[AdkMessage::kConnectivityWifiError] = "wifi::error";
        msg2strconn[AdkMessage::kConnectivityWifiEnabled] = "wifi::enabled";
        msg2strconn[AdkMessage::kConnectivityWifiCompleteonboarding] = "wifi::completeonboarding";

        handler_id_list.push_back(subscription_token);

    }

    if (!self_->message_service_.Subscribe(adk::msg::kGroupVoiceui,
                                           std::bind(&ADKAdaptor::HandleVoiceUIGroupMsg, this, std::placeholders::_1),&subscription_token)) {
        return false;
    } else
    {
        //register msg to string map
        msg2strvoiceui[AdkMessage::kVoiceuiStatusIdle] = "vu::StatusIdle";
        msg2strvoiceui[AdkMessage::kVoiceuiStatusListening] = "vu::StatusListening";
        msg2strvoiceui[AdkMessage::kVoiceuiStatusThinking] = "vu::StatusThinking";
        msg2strvoiceui[AdkMessage::kVoiceuiStatusSpeaking] = "vu::StatusSpeaking";
        msg2strvoiceui[AdkMessage::kVoiceuiStatusSpeechDone] = "vu::StatusSpeechDone";
        msg2strvoiceui[AdkMessage::kVoiceuiStatusAlert] = "vu::StatusAlert";
        msg2strvoiceui[AdkMessage::kVoiceuiNotification] = "vu::Notification";
        msg2strvoiceui[AdkMessage::kVoiceuiStatusDoNotDisturb] = "vu::StatusDoNotDisturb";
        msg2strvoiceui[AdkMessage::kVoiceuiStatusDirectionOfArrival] = "vu::StatusDirectionOfArrival";
        msg2strvoiceui[AdkMessage::kVoiceuiSetZigbeeSecurity] = "vu::SetZigbeeSecurity";
        msg2strvoiceui[AdkMessage::kVoiceuiDatabaseUpdated] = "vu::DatabaseUpdated";
        msg2strvoiceui[AdkMessage::kVoiceuiAuthenticateAvs] = "vu::";
        msg2strvoiceui[AdkMessage::kVoiceuiAvsOnboardingError] = "vu::AvsOnboardingError";
        msg2strvoiceui[AdkMessage::kVoiceuiTapAvs] = "vu::TapAvs";
        msg2strvoiceui[AdkMessage::kVoiceuiTapQcasr] = "vu::TapQcasr";
        msg2strvoiceui[AdkMessage::kVoiceuiStartOnboarding] = "vu::StartOnboarding";
        msg2strvoiceui[AdkMessage::kVoiceuiDeleteCredential] = "vu::DeleteCredential";
        msg2strvoiceui[AdkMessage::kVoiceuiStatus] = "vu::Status";
        msg2strvoiceui[AdkMessage::kVoiceuiSetDefaultClient] = "vu::SetDefaultClient";
        msg2strvoiceui[AdkMessage::kVoiceuiSetAvsLocale] = "vu::SetAvsLocale";
        msg2strvoiceui[AdkMessage::kVoiceuiSetModularClientProps] = "vu::SetModularClientProps";

        handler_id_list.push_back(subscription_token);

    }
#ifdef MULTI_THREAD
    std::for_each(ModeFlow::modeslist.begin(),ModeFlow::modeslist.end(), [this](ModeFlow * mf) {
        mf->setMsgProc(ModeFlow::event2smList);
    });

    for(int i = 0; i< current_que_num; i++) {
        pmsgProcMfList[i]->StartThread();
    }
#endif
    pmainmsgproc = new MainMsgProc(ModeFlow::event2smList, ModeFlow::modeslist, MsgQue);

    return true;
}

// handle the ADK message "Mode" group messages
void ADKAdaptor::HandleSystemMode(const AdkMessage& message) {
    modeflow_wake_lock_toggle(true, kWakelockMain, 0);

    // get the type of the message, and handle accordingly
    std::string value = message.system_mode_management().name();
	enqueue(make_shared<string>(value));
    modeflow_wake_lock_toggle(false, kWakelockMain, 0);

}

void ADKAdaptor::HandleSystemTimerEvent(const AdkMessage& message) {
    modeflow_wake_lock_toggle(true, kWakelockMain, 0);

    // get the type of the message, and handle accordingly
    std::string value = message.system_timer_event().name();
	enqueue(make_shared<string>(value));

    modeflow_wake_lock_toggle(false, kWakelockMain, 0);

}

void ADKAdaptor::HandleButtonGroupMsg(const AdkMessage& message) {
    modeflow_wake_lock_toggle(true, kWakelockMain, 0);

    // get the type of the message, and handle accordingly
    std::string value =string();
    switch(message.adk_msg_case())
    {
    case AdkMessage::kButtonPressed:
        value = message.button_pressed().button();
        break;
    case AdkMessage::kButtonLongPressed:
        value = message.button_long_pressed().button();
        break;
    case AdkMessage::kButtonVeryLongPressed:
        value = message.button_very_long_pressed().button();
        break;
    case AdkMessage::kButtonLongHold:
        value = message.button_long_hold().button();
        break;
    case AdkMessage::kButtonVeryLongHold:
        value = message.button_very_long_hold().button();
        break;
    case AdkMessage::kButtonDoublePressed:
        value = message.button_double_pressed().button();
        break;
    case AdkMessage::kButtonTriplePressed:
        value = message.button_triple_pressed().button();
        break;
    default:
        modeflow_wake_lock_toggle(false, kWakelockMain, 0);
        return;
    }

    value = "button::" + value + "_" + msg2str[message.adk_msg_case()];
    std::cout << value <<  std::endl;
	enqueue(make_shared<string>(value));
    modeflow_wake_lock_toggle(false, kWakelockMain, 0);

}

void ADKAdaptor::HandleAudioGroupMsg(const AdkMessage& message) {
    modeflow_wake_lock_toggle(true, kWakelockMain, 0);

    std::string value =string();
    switch(message.adk_msg_case())
    {
    // get the type of the message, and handle accordingly
    case AdkMessage::kAudioPlayerStatusUpdate:
        if(message.has_audio_player_status_update()) {
            value = message.audio_player_status_update().status();
            value = "audio::" + value;
        }
        break;
    case AdkMessage::kAudioSourceSet:
        if (message.audio_source_set().has_name()) {
            value = "audio::source_set" + message.audio_source_set().name();
        }
        break;
    case AdkMessage::kAudioSourceSelect:
        if (message.audio_source_select().has_source_name()) {
            value = "audio::source_select_" + message.audio_source_select().source_name();
        }
        break;
    case AdkMessage::kAudioVolumeUpdated:
        if (message.audio_volume_updated().has_volume()) {
            value = "audio::volume:"+ std::to_string(message.audio_volume_updated().volume());
        }
        break;

    default:
        modeflow_wake_lock_toggle(false, kWakelockMain, 0);
        return;
    }
	enqueue(make_shared<string>(value));

    modeflow_wake_lock_toggle(false, kWakelockMain, 0);
}

void ADKAdaptor::HandleConnectivityGroupMsg(const AdkMessage& message) {
    modeflow_wake_lock_toggle(true, kWakelockMain, 0);

    // get the type of the message, and handle accordingly

    std::string value = string();
    /*    switch(message.adk_msg_case())
        {
        case AdkMessage::kConnectivityWifiOnboarding:
            value = ":" + message.connectivity_wifi_onboarding().status();
            break;
        case AdkMessage::kConnectivityWifiConnected:
            value = ":" + message.connectivity_wifi_connected().status();
            break;
        default:
            return;
        }
    */

    switch(message.adk_msg_case())
    {
    case AdkMessage::kConnectivityBtBleconnected:
        value = ":" + message.connectivity_bt_bleconnected().status();
        break;
    case AdkMessage::kConnectivityBtErrorresponse:
        value = ":" + message.connectivity_bt_errorresponse().status();
        if (message.connectivity_bt_errorresponse().has_address()) {
            value = value + ":" + message.connectivity_bt_errorresponse().address();
        }
        if (message.connectivity_bt_errorresponse().has_command()) {
            value = value + ":" + message.connectivity_bt_errorresponse().command();
        }
        break;

    default:
        break;
    }

    value = msg2strconn[message.adk_msg_case()] + value;

    if(!value.empty())
    {
        enqueue(make_shared<string>(value));
    }

    modeflow_wake_lock_toggle(false, kWakelockMain, 0);
}

void ADKAdaptor::HandleVoiceUIGroupMsg(const AdkMessage& message) {
    modeflow_wake_lock_toggle(true, kWakelockMain, 0);

    std::string value = string();

    switch(message.adk_msg_case())
    {
    case AdkMessage::kVoiceuiDatabaseUpdated:
        value = ":" + message.voiceui_database_updated ().key();
        break;
    case AdkMessage::kVoiceuiStatusListening:
    case AdkMessage::kVoiceuiStatusIdle:
    case AdkMessage::kVoiceuiStatusThinking:
    case AdkMessage::kVoiceuiStatusSpeaking:
    case AdkMessage::kVoiceuiStatusSpeechDone:
    case AdkMessage::kVoiceuiStatusAlert:
    case AdkMessage::kVoiceuiNotification:
    case AdkMessage::kVoiceuiStatusDoNotDisturb:
        break;
    case AdkMessage::kVoiceuiStatusDirectionOfArrival:
        value = ":" + std::to_string(message.voiceui_status_direction_of_arrival ().angle());
        break;
    case AdkMessage::kVoiceuiSetZigbeeSecurity:
    case AdkMessage::kVoiceuiAuthenticateAvs:
    case AdkMessage::kVoiceuiAvsOnboardingError:
    case AdkMessage::kVoiceuiTapAvs:
    case AdkMessage::kVoiceuiTapQcasr:
    case AdkMessage::kVoiceuiStartOnboarding:
    case AdkMessage::kVoiceuiDeleteCredential:
    case AdkMessage::kVoiceuiStatus:
    case AdkMessage::kVoiceuiSetDefaultClient:
    case AdkMessage::kVoiceuiSetAvsLocale:
    case AdkMessage::kVoiceuiSetModularClientProps:
        break;
    default:
        break;
    }

    value = msg2strvoiceui[message.adk_msg_case()] + value;

    if(!value.empty())
    {
        enqueue(make_shared<string>(value));
    }

    modeflow_wake_lock_toggle(false, kWakelockMain, 0);
}

void ADKAdaptor::enqueue(shared_ptr<string> pevent)
{
    mflog(MFLOG_DEBUG,"%03lu:rcvadk:%s", get_ms(), pevent->c_str());
    MsgQue.enqueue(pevent);
}

void ADKAdaptor::Stop() {
    // Do a clean shutdown of anything
    for(auto token:handler_id_list) {
        self_->message_service_.Unsubscribe(token);
    }
    return;
}
}  // namespace Mode
}  // namespace adk
