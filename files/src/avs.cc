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
#include "avs.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

using adk::msg::AdkMessageService;

AVS::AVS():ModeFlow("avs")
{
    msgProc();
    modeRecovery("Shutdown", 0, 32, nullptr, true);
}

void AVS::IfConnector()
{
}

void AVS::Input() {
    e2mMap("vu::StatusIdle", "Idle");
    e2mMap("vu::StatusDirectionOfArrival", "Listening");
    e2mMap("vu::StatusListening", "Listening");
    e2mMap("vu::StatusThinking", "ListeningEnd");
    e2mMap("vu::StatusSpeaking", "Speaking");

    e2mMap("avs::shutdown", "TransShutdown");
    e2mMap("avs::start", "TransInit");
    e2mMap("avs::started", "Init");

    em2mMap("avs::micmute", "Idle", "Mute");
    e2aMap("avs::micmute", [this] {m_mute = true; return (shared_ptr<string>)nullptr;});
    e2aMap("avs::micunmute", [this] {m_mute = false; return (shared_ptr<string>)nullptr;});
    em2mMap("avs::micunmute", "Mute", "Idle");

    e2deMap_re("avs::pauseAVSAudioPlayer", "actions::/bin/echo 2 > /dev/shm/avsin",0,100);
    e2deMap_re("avs::resumeAVSAudioPlayer", "actions::/bin/echo 1 > /dev/shm/avsin",0,100);

    em2mMap("avs::avsdown", "TransShutdown","Shutdown");

}

void AVS::Internal() {
    e2aMap_re("avs::.+2Idle", [this] {
        if(m_mute) {
            setMode("Mute");
            return make_shared<string>("avs::Idle2Mute");
        }
        return (shared_ptr<string>)nullptr;
    });

    e2taMap_re("avs::.+2TransShutdown", "avs::startShutting", bind(&AVS::shutdown, this));
    e2taMap_re("avs::Shutdown2TransInit", "avs::starting", bind(&AVS::start, this));

    /*	ecMap_re("avs::.+2(Speaking|Thinking)", "audio::avs:Playing:AudioMediaPlayer");
    	ecMap_re("avs::Speaking2Idle", "audio::avs:Stopped:AudioMediaPlayer");*/
}
void AVS::Output() {
    ecMap_re("avs::.+2Mute", "indication::avsmicmute");
    ecMap_re("avs::Mute2.+", "indication::avsmicunmute");
}
void AVS::IOConnector() {
//AVS Controller
    ecMap_re("audio::avs:Playing:SpeakMediaPlayer", "devpwr::playavs", "volume::setPortAVSSpeakMediaPlayer", "volume::connectFlowDsp");//tmp AVS connector
    ecMap_re("audio::avs:Playing:AudioMediaPlayer", "devpwr::playavs", "volume::setPortAVSAudioMediaPlayer", "volume::connectFlowDsp");//tmp AVS connector
    ecMap_re("audio::avs:Playing:NotificationsMediaPlayer","devpwr::playavs", "volume::setPortAVSNotificationsMediaPlayer", "volume::connectFlowDsp");//tmp AVS connector
    ecMap_re("audio::avs:Playing:BluetoothMediaPlayer", "devpwr::playavs", "volume::setPortAVSBluetoothMediaPlayer", "volume::connectFlowDsp");//tmp AVS connector
    ecMap_re("audio::avs:Playing:RingtoneMediaPlayer", "devpwr::playavs", "volume::setPortRingtoneMediaPlayer", "volume::connectFlowDsp");//tmp AVS connector
    ecMap_re("audio::avs:Playing:AlertsMediaPlayer", "devpwr::playavs", "volume::setPortAVSAlertsMediaPlayer", "volume::connectFlowDsp");//tmp AVS connector
    ecMap_re("audio::avs:Playing:SystemSoundMediaPlayer","devpwr::playavs", "volume::setPortAVSSystemSoundMediaPlayer", "volume::connectFlowDsp");//tmp AVS connector


    /*
    	em2aMap_re("button::VOL.+","Idle",[this]{
    		if(m_volume > 95){
    			string adkmsg = "indication::AlexaVolume:100";
    			return make_shared<string>(adkmsg);
    		}else if(m_volume <= 5){
    			string adkmsg = "indication::AlexaVolume:5";
    			return make_shared<string>(adkmsg);
    		}
    		return (shared_ptr<string>)nullptr;
    	});
    	em2aMap_re("audio::volume:.+", "Idle",[this]{
    			string volume_str = getEvent().substr(getEvent().find_last_of(":") + 1);
    			float volume_float = std::atof(volume_str.c_str());
    			int volume_int = (int)(volume_float*100);
    			if(volume_int == 0)
    				volume_int = 5;
    			m_volume = volume_int;
    			string adkmsg = "indication::AlexaVolume:" + to_string(volume_int);
    			return make_shared<string>(adkmsg);
    	});
    */
    e2aMap_re("audio::avs:(Stopped|Paused).+",[this] {
        string playername = getEvent().substr(getEvent().find_last_of(":") + 1);
        string volumecmd = string("volume::") + "close:" + playername;
        return make_shared<string>(volumecmd);
    });

    e2aMap_re("avs::(Speaking|Listening|ListeningEnd)2Idle",[this] {
        string angle = to_string(direction);
        string adkmsg = "indication::AlexaSpeakingListening2Idle:" + angle ;
        return make_shared<string>(adkmsg);
    });

    e2aMap_re("vu::StatusDirectionOfArrival:.+", [this] {
        string angle = getEvent().substr(getEvent().find_last_of(":") + 1);
        direction = std::stoi(angle);
        string adkmsg = "indication::AlexaListeningStart:" + angle ;
        return make_shared<string>(adkmsg);
    });

    ecMap_re("vu::StatusThinking", "indication::StatusThinking");
    ecMap_re("vu::StatusSpeaking", "indication::StatusSpeaking");
    ecMap_re("vu::StatusAlert", "indication::StatusAlert");
//???To check
    ecMap_re("vu::Notification", "indication::Notification");
    ecMap_re("vu::StatusDoNotDisturb", "indication::StatusDoNotDisturb");


    emecMap_re("vu::StatusAlert", "Speaking","indication::avsSpeakingAlert");
    emecMap_re("vu::StatusAlert", "(?!Speaking).+","indication::avsNotSpeakingAlert");

    em2taMap_re("button::ACTION_SP", "(?!(Init|Shutdown)).+", "avs::tapAVS", bind(&AVS::tapAVS, this));

    e2aMap_re("devpwr::(?!Off).+2.*Off", std::bind(&AVS::shutdown, this));

}

shared_ptr<string> AVS::tapAVS() {
    sendAdkMsg("voiceui_tap_avs {}");
    return (shared_ptr<string>)nullptr;
}

shared_ptr<string> AVS::shutdown() {
    sendPayload(make_shared<string>("actions::systemctl stop voiceUI&"));
    sendPayload(make_shared<string>("actions::systemctl stop avs&"));
    return make_shared<string>("avs::avsdown");
}

shared_ptr<string> AVS::start() {
    sendPayload(make_shared<string>("actions::systemctl start voiceUI&"));
    sendPayload(make_shared<string>("actions::systemctl start avs&"));
    return make_shared<string>("avs::started");
}


