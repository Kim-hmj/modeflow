
/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */
#include "indication.h"
Indication::Indication():ModeFlow("indication")
{
    msgProc();
    //SeqPowerOnIndication();
    //SeqConnection();
    //SeqAlexa();

    modeRecovery("Init", 0, 32, nullptr, false);
}

void Indication::IfConnector()
{
    //lantern
    ecMap_re("lantern::Off2(?!Init|RGB|Campfire|Night|Classic|Standby).+","indication::lanternIconOn");
    ecMap_re("lantern::(?!recover|RGB|Campfire|Night|Classic).+2(Off|Init)","indication::lanternIconOff");
    ecMap_re("lantern::Standby2(Low|Medium|High)","indication::lanternIconOn");
    //source sound
    ecMap_re("source::(?!recover).+2wifipart", "indication::wifiConnected");
    ecMap_re("source::(?!wifipart).+2wififull", "indication::wifiConnected");

    ecMap_re("source::(?!recover).+2bt", "indication::btConnected");

//	ecMap_re("btman::(?!recover).+2Disconnected","indication::btDisconnected");//Shall use btman state
    ecMap_re("source::wifi2lostwifi","indication::WifiDisconnected");
    //power off
    ecMap_re("devpwr::(?!TransOff).+2TransOff","indication::setOff");
    ecMap_re("devpwr::(?!recover).+2Standby","indication::setDim");
    ecMap_re("devpwr::(?!recover).+2On","indication::setOn");

}
void Indication::Input()
{
    e2mMap_re("indication::setOff","Off");
    e2mMap_re("indication::setDim","Dim");
    e2mMap_re("indication::setOn","On");
}
void Indication::Internal()
{
    e2aMap_re("indication::setOn", [this] {
            power_st = 2;
            return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("indication::setOff", [this] {
            power_st = 1;
            return (shared_ptr<string>)nullptr;
    });
    e2deMap_re("indication::btConnected","indication::bt1sConnected",1,0);
    e2deMap_re("indication::wifiConnected","indication::wifi1sConnected",1,0);
}
void Indication::Output()
{
    //icon and ConnectIcon
    ecMap_re("indication::.+2Off","icon::FadeToOff","ConnectIcon::ConnectFadeToOff");
    ecMap_re("indication::.+2Dim","icon::DimTo30","ConnectIcon::ConnectDimTo30","icon::lanternIconDim");
    ecMap_re("indication::.+2On","icon::FadetoFull","ConnectIcon::ConnectFadetoOn","icon::lanternIconFadeOn");

    //connected
    ecMap_re("indication::bt1sConnected","Tone::sound:r1-Connected","lantern::Blue3secFadeoff", "ConnectIcon::BtConnected");
    ecMap_re("indication::wifi1sConnected","Tone::sound:r1-Connected","lantern::Amber3secFadeoff", "ConnectIcon::WifiConnected");

    //disconnected
    ecMap_re("indication::BtDisconnected", "Tone::sound:r1-Disconnected","ConnectIcon::BtDisconnected");
    ecMap_re("indication::WifiDisconnected", "Tone::sound:r1-Disconnected","ConnectIcon::WifiDisconnected");
    
    ecMap_re("indication::gotoPowerOn", "Tone::sound:r1-PowerUp","icon::TopButtonFadeOn");

    ecMap_re("indication::BtPairing", "Tone::sound:r1-Pairing","ConnectIcon::BtPairingRun","lantern::BtPairingRun");

    ecMap_re("indication::BtPairingFailed", "ConnectIcon::BtPairingFailed","Thread::lantern:history");


    //lantern
    ecMap_re("indication::lanternIconOn", "icon::LanternButtonOn");
    ecMap_re("indication::lanternIconOff", "icon::LanternButtonOff");

    e2aMap_re("audio::(bt|spotify|airplay):Stopped", [this] {
        if (power_st == 2)
            return make_shared<string>("icon::PlayPausePulsing");
        else
            return (shared_ptr<string>)nullptr;
    });
	
    e2aMap_re("audio::(bt|spotify|airplay):Playing", [this] {
        if (power_st == 2)
            return make_shared<string>("icon::PlayPauseBlink");
        else
            return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("indication::(BtConnected|WifiConnected)", [this] {
        std::thread (&ModeFlow::delay_thread, this, "lantern:history",4,0).detach();
        if(getModuleMode("lantern").find("Standby") != string::npos) {
            return make_shared<string>("Thread::lantern:history");
        }
        return (shared_ptr<string>)nullptr;
    });

    ecMap_re("indication::AlexaListeningStart:.+","Tone::sound:r1-AlexaListeningStartVoice");
    e2aMap_re("indication::AlexaListeningStart:.+",[this] {
        string angle = getEvent().substr(getEvent().find_last_of(":") + 1);
        string adkmsg = "lantern::RGB_AlexasListeningStart:" + angle;
        return make_shared<string>(adkmsg);
    });

    ecMap_re("indication::AlexaSpeakingListening2Idle:.+","Tone::sound:r1-AlexaListeningEnd");
    e2aMap_re("indication::AlexaSpeakingListening2Idle:.+",[this] {
        string angle = getEvent().substr(getEvent().find_last_of(":") + 1);
        string adkmsg = "lantern::RGB_AlexaListeningEnd:" + angle;
        return make_shared<string>(adkmsg);
    });

    e2aMap_re("indication::AlexaVolume:.+",[this] {
        string angle = getEvent().substr(getEvent().find_last_of(":") + 1);
        string adkmsg = "lantern::AlexaVolume:" + angle;
        return make_shared<string>(adkmsg);
    });

    ecMap_re("indication::StatusSpeaking", "lantern::RGB_AlexaSpeaking");
    ecMap_re("indication::StatusThinking", "lantern::RGB_AlexaThinking");
    ecMap_re("indication::avsmicmute", "Tone::sound:r1-AlexaMicOnToMicOff","lantern::RGB_AlexaMicmute");
    ecMap_re("indication::avsmicunmute", "Tone::sound:r1-AlexaMicOffToMicOn","lantern::RGB_AlexaMicunmute");

    ecMap_re("indication::StatusAlert", "Tone::sound:r1-AlexaAlarm","lantern::RGB_AlexaAlarm");
    ecMap_re("indication::Notification", "Tone::sound:r1-AlexaIncomingNotification","lantern::RGB_AlexaIncomingNotification");
    ecMap_re("indication::StatusDoNotDisturb", "lantern::RGB_AlexaStatusDoNotDisturb");
}

void Indication::IOConnector()
{
    //Init
    em2deMap_re("devpwr::Transon2On","Init","icon::ConnectFadeToWhite",1,0);    
    em2deMap_re("source::.+(2bt|2wifi)","(On|Dim)","ConnectIcon::ConnectFadetoOn",1,0);
    e2deMap_re("source::.+(2transbt|2transwifi)", "ConnectIcon::ConnectPulsing",1,0);

    ecMap_re("indication::InitWifi", "ConnectIcon::InitWifi");
    ecMap_re("indication::OffWifi", "ConnectIcon::OffWifi");
    ecMap_re("indication::InitBt", "ConnectIcon::InitBt");
    ecMap_re("indication::OffBt", "ConnectIcon::OffBt");
    ecMap_re("indication::InitSource", "ConnectIcon::InitSource");
}
