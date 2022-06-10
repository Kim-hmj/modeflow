/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#include "volume.h"
#include <math.h>
#include <adk/config/config.h>
#include <iterator>
#include<bitset>

#define CUE_CONST_VOLUME

Volume::Volume():ModeFlow("volume")
{
    msgProc();
    string recoverInfo = modeRecovery("Normal", sizeof(struct VolumeSt), 16, (void *)&mconst_VolumeSt);
    pstVolumes = (struct VolumeSt*)getShmDataAddr();

    std::thread(&ModeFlow::init_actions,this, [this, recoverInfo] {return make_shared<string>(recoverInfo);}).detach();
}

void Volume::IfConnector()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

//start flowdsp connection
    connectorVolumeConnFlowDsp();

//reset features
    connectorVolumeReset();

//volume sync from btapp
    e2aMap_re("btapp::volIndexSync:.+",  [this] {
        return make_shared<string>(string("volume::setBtvol:") + getEvent().substr(getEvent().find_last_of(":") + 1));
    });

//volume configure from audioman
    e2aMap_re("audioman::setvol:.+",  [this] {
        return make_shared<string>(string("volume::setWifivol:") + getEvent().substr(getEvent().find_last_of(":") + 1));
    });

//HFPStartStop
    ecMap_re("btman::(?!recover).+2CallIn","volume::EnterCall");
    ecMap_re("btman::CallIn2(?!CallIn).+","volume::ExitCall");

//Bass Toggle
    emecMap_re("button::BASSTOGGLE_LH",   "Normal", "volume::loopBassTable");


//FlowAudioStop
    ecMap_re("devpwr::StreamStopped", "volume::stopflowdsp");

}

void Volume::Input() {
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);
    InterfaceVolumeConnFlowDsp();
    InterfaceVolumeReset();

    emecMap_re("volume::b(dn|up)_LP", "Continue.+", "volume::continousVolumeChange");
    e2mMap("volume::bdn_LH", "ContinueDown");
    e2mMap("volume::bup_LH",   "ContinueUp");
    em2aMap("volume::bdn_SP", "Normal", [this] {
        shared_ptr<string> pretS;
         button_up = false;
        int8_t portIndex = pstVolumes->portIndex;
        if(pstVolumes->deltaDB[portIndex] != 0) {
            pstVolumes->deltaDB[portIndex] = 0;
            pretS = setVolume(pstVolumes->mVolumeIndex[portIndex], volume_db[pstVolumes->mVolumeIndex[portIndex]],false);
        } else if(--pstVolumes->mVolumeIndex[portIndex] >= 0) {
            pretS = setVolume(pstVolumes->mVolumeIndex[portIndex], volume_db[pstVolumes->mVolumeIndex[portIndex]],false);
        } else
        {
            pstVolumes->mVolumeIndex[portIndex] = 0;
            pretS = make_shared<string>("log::volume:gettomin");
        }
        return pretS;
    });

    em2aMap("volume::bup_SP",   "Normal", [this] {
        shared_ptr<string> pretS;
         button_up = true;
        int8_t portIndex = pstVolumes->portIndex;
        if(pstVolumes->deltaDB[portIndex] != 0)
        {
            pstVolumes->deltaDB[portIndex] = 0;
        }

        if(++pstVolumes->mVolumeIndex[portIndex] < sizeof(volume_db)/sizeof(volume_db[0])) {
            pretS  = setVolume(pstVolumes->mVolumeIndex[portIndex], volume_db[pstVolumes->mVolumeIndex[portIndex]],false);
        } else{
            pstVolumes->mVolumeIndex[portIndex] = sizeof(volume_db)/sizeof(volume_db[0]) -1;
            pretS = make_shared<string>("log::volume:gettomax");
        }

        return pretS;
    });

    e2aMap_re("volume::setBtvol:.+",  [this] {
        int absVol = std::stod(getEvent().substr(getEvent().find_last_of(":") + 1));
		shared_ptr<string> pretS = setAbsVolume(absVol, "bt",false);

		if(pretS)sendPayload(pretS);
		
        return setAbsVolume(absVol, "phone",false);
    });

    e2aMap_re("volume::setWifivol:.+",  [this] {
        int absVol = std::stod(getEvent().substr(getEvent().find_last_of(":") + 1));
		shared_ptr<string> pretS = setAbsVolume(absVol, "airplay",false);
		if(pretS) sendPayload(pretS);
		 pretS = setAbsVolume(absVol, "spotify",false);
		if(pretS) sendPayload(pretS);
        return setAbsVolume(absVol, "snapclient",false);
    });

    e2mMap_re("volume::EnterCall","CallIn");
    e2mMap_re("volume::ExitCall","Normal");
    em2aMap_re("volume::continousVolumeChange", "Continue.+", [this] {
        pstVolumes->speedIncreasing = false;
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("volume::setbass:.+",   [this] {
        int gain = std::stoi(getEvent().substr(getEvent().find_last_of(":") + 1));
        return SetBaseGain(gain);
    });

    e2aMap("volume::loopBassTable",  [this] {
        if(++pstVolumes->bass_index == sizeof bass_table)
            pstVolumes->bass_index = 0;

        return SetBaseGain(bass_table[pstVolumes->bass_index]);
    });

//FlowPowerOff
    e2aMap_re("volume::powerOffReset", [this] {
        pstVolumes->bass_index = 0;
        return make_shared<string>("Log::Volume:base_index_reset");
    });

    e2aMap("volume::stopflowdsp",   [this] {
        pstVolumes->flowdsp_state = false;
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("source::.+2wififull", [this] {pstVolumes->avsReady = true; return (shared_ptr<string>)nullptr;});
    e2aMap_re("source::wififull2(?!wififull).+", [this] {pstVolumes->avsReady = false; return (shared_ptr<string>)nullptr;});
    e2aMap_re("volume::reduceSync", [this] {avssyncing--; return (shared_ptr<string>)nullptr;});
    e2aMap_re("volume::syncToBt.+", [this] {
        string volIndex = getEvent().substr(getEvent().find_last_of(":") + 1);
        sendAdkMsg(("connectivity_bt_syncvol {volume:") + volIndex + std::string("}"));
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("volume::syncToWifi.+", [this] {
        string volIndex = getEvent().substr(getEvent().find_last_of(":") + 1);
        sendAdkMsg(std::string("audio_volume_updated {volume:") + volIndex + std::string("}"));//phone consider 1 less
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("vu::TapAvs",[this]{
        if(AVS_WakeUp){
            system("echo s > /dev/shm/avsin");
            return (shared_ptr<string>)nullptr;
        }
        if(CheckAVSPlayer()){
            Player_flag = false;
            if(Alerting){Alerting = false;}
            system("echo s > /dev/shm/avsin");
            return (shared_ptr<string>)nullptr;
        }
        AVS_WakeUp = true;
        cur_db = true;
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("vu::StatusListening",[this]{
        AVS_WakeUp = true;
        cur_db = true;
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("vu::StatusIdle",[this]{
        AVS_WakeUp = false;
        return (shared_ptr<string>)nullptr;
    });

}

void Volume::Internal()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    FlowRetriveStoreVolume();

    em2aMap_re("volume::(?!recover).+2Normal",   "Normal", [this] {
        pstVolumes->speedIndex = 0;
        pstVolumes->speedIncreasing = true;
        return (shared_ptr<string>)nullptr;
    });

    em2mMap_re("volume::send_failed.*",  "Continue.+" ,"Normal");


}

void Volume::Output()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);
    //VolumeChange
    em2aMap_re("volume::(.+2ContinueUp|nextUp)", "ContinueUp",[this] {
        std::thread (&ModeFlow::long_actions,this, [this]{
            shared_ptr<string> pretS;
            int8_t portIndex = pstVolumes->portIndex;
            if(pstVolumes->deltaDB[portIndex] != 0)
            {
                pstVolumes->deltaDB[portIndex] = 0;
            }

            if(++pstVolumes->mVolumeIndex[portIndex] < (sizeof(volume_db)/sizeof(volume_db[0])))
            {
                pretS = setVolume(pstVolumes->mVolumeIndex[portIndex],volume_db[pstVolumes->mVolumeIndex[portIndex]],false);
            }
            else
            {
                pstVolumes->mVolumeIndex[portIndex]--;
                return setMode("Normal");
            }

            if(pretS && pretS->find("failed") != string::npos)
            {
                return pretS;
            }
            usleep(speed_table[pstVolumes->speedIndex]*100);
            if(!pstVolumes->speedIncreasing) return make_shared<string>("volume::nextUp");
            if(++pstVolumes->speedIndex >=  sizeof(speed_table)/sizeof(speed_table[0])) {
                return setMode("Normal");
            }
            else {return make_shared<string>("volume::nextUp");}
        }).detach();
        return (shared_ptr<string>)nullptr;
    });

//VolumeChange
    em2aMap_re("volume::(.+2ContinueDown|nextDown)", "ContinueDown",[this] {
        std::thread (&ModeFlow::long_actions,this, [this]{
			shared_ptr<string> pretS;
            int8_t portIndex = pstVolumes->portIndex;
            if(pstVolumes->deltaDB[portIndex] != 0) {
                pstVolumes->deltaDB[portIndex] = 0;
                pretS = setVolume(pstVolumes->mVolumeIndex[portIndex], volume_db[pstVolumes->mVolumeIndex[portIndex]],false);
            } else if(--pstVolumes->mVolumeIndex[portIndex] >= 0) {
                pretS = setVolume(pstVolumes->mVolumeIndex[portIndex], volume_db[pstVolumes->mVolumeIndex[portIndex]],false);
            } else {
                pstVolumes->mVolumeIndex[portIndex]++;
                return setMode("Normal");
            }

            if(pretS && pretS->find("failed") != string::npos)
            {
                return pretS;
            }
            usleep(speed_table[pstVolumes->speedIndex]*100);
            if(!pstVolumes->speedIncreasing) return make_shared<string>("volume::nextDown");
            if(++pstVolumes->speedIndex >= sizeof(speed_table)/sizeof(speed_table[0])) {
                return setMode("Normal");
            }
            else {return make_shared<string>("volume::nextDown");}
        }).detach();
        return (shared_ptr<string>)nullptr;
    });

//VolumeSyncOut
    e2aMap_re("Thread::Volume:syncing_out_complete:.+", [this] {
        string tmpS = getEvent();

        int8_t portIndex = pstVolumes->portIndex;
        int syncVolumeIndex = std::stoi(tmpS.substr((tmpS.find_last_of(":")+1)));
        int volumeIndex = pstVolumes->mVolumeIndex[portIndex];
        if(pstVolumes->mVolumeIndex[portIndex] != syncVolumeIndex) { //Vol is still changing, do not sync
            int volumeIndex = pstVolumes->mVolumeIndex[portIndex];
            std::thread(&ModeFlow::delay_thread, this, "Volume:syncing_out_complete:" + std::to_string(volumeIndex), 0, 100).detach();
        } else{
            //Vol is stable, sync
            double deltaIndex = (double)pstVolumes->deltaDB[portIndex]/(double)(volume_db[volumeIndex+1] - volume_db[volumeIndex]);

            if(volumeIndex < sizeof(volume_db)/sizeof(volume_db[0]) - 2) {
                sendPayload(make_shared<string>(string("volume::FilterBtSyncOut:") + std::to_string((int)round((double)volumeIndex + deltaIndex)) ));
                sendPayload(make_shared<string>(string("volume::FilterWifiSyncOut:") + std::to_string((double)(volumeIndex + deltaIndex)/((sizeof(volume_db)/sizeof(volume_db[0]) - 1)))));
            }

            syncing_out = false;
        }
        return (shared_ptr<string>)nullptr;
    });

}

void Volume::IOConnector()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    e2aMap_re("vu::spk:vol:[0-9]+:mute:[01]",
    [this] {
        string vol_str = getEvent().substr(getEvent().find("vol:") + 4, (getEvent().find("mute:") - 1) - (getEvent().find("vol:") + 4));
        int vol = std::stoi(vol_str);

        setAbsVolume((int)((float)vol/100*127));
        return make_shared<string>("Log::volume:setLocalVolume");
    });

    e2aMap_re("vu::alert:vol:[0-9]+:mute:[01]",
    [this] {
        string vol_str = getEvent().substr(getEvent().find("vol:") + 4, (getEvent().find("mute:") - 1) - (getEvent().find("vol:") + 4));
        int vol = std::stoi(vol_str);

        pstVolumes->mVolumeIndex[8] = (int)((float)vol*SPEAKER_MAX_VOLUME_STEPS/100);

        setAbsVolume((int)((float)vol/100*127), "AVSAlertsMediaPlayer");
        return make_shared<string>("Log::volume:setAlertVolume");
    });

    e2aMap_re("vu::setvol:.+",
    [this] {
        if(avssyncing) return make_shared<string>("Log::volume:avssyncing");
        string vol_str = getEvent().substr(getEvent().find_last_of(":") + 1);
        int vol = std::stoi(vol_str);
        string playername = string("AVS") + getEvent().substr(getEvent().find("setvol:") + 7, (getEvent().find_last_of(":")) - (getEvent().find("setvol:") + 7));

        pstVolumes->mVolumeIndex[8] = (int)((float)vol*SPEAKER_MAX_VOLUME_STEPS/100);

        setAbsVolume((int)((float)vol/100*127), playername, true);
        return make_shared<string>(string("Log::volume:Volumeset:") + playername);
    });
}

//refactor in progress
void Volume::InterfaceVolumeConnFlowDsp()
{
//FlowAudioOn
//FlowConnectFlowDsp
//AVS  20012-20018
//SeqNo.1
    e2aMap_re("volume::setPort.+",  [this] {
        string event = getEvent();
        string tag = "setPort";
        if(event.find(tag) != string::npos) {
            string tmp_playerName = event.substr(event.find(tag) + tag.length());
            snprintf(pstVolumes->currentPortname,32, "%s", tmp_playerName.c_str());

            if(portIndexMap.find(tmp_playerName) == portIndexMap.end()) return make_shared<string>("Log::volume:setPort:notfound");
            pstVolumes->targetPort = portmap.at(tmp_playerName);
            if(pstVolumes->targetPort == 20012) {
                mDecodingDelayus =0;
            } else {
            }

            sendPayload(make_shared<string>("log::volume:" + tmp_playerName));

            pstVolumes->portIndex = portIndexMap.at(tmp_playerName);
            pstVolumes->playingChannels |= (1 << pstVolumes->portIndex);
            std::stringstream stream;
            std::bitset<sizeof(int32_t)*8> bits(pstVolumes->playingChannels);
            stream << bits;
            sendPayload(make_shared<string>(string("log::volume:Channel:") + stream.str()));
        } else{
            return make_shared<string>("log::portNotFound");
        }
        return make_shared<string>(string("log::port:") + std::to_string(pstVolumes->targetPort));
    });
//SeqNo.2
    e2aMap_re("volume::connectFlowDsp",  [this] {
        std::thread (&ModeFlow::long_actions,this, [this]() {
            return establishConn();
        }).detach();
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("volume::close.+",  [this] {
        string playername = getEvent().substr(getEvent().find_last_of(":") + 1);

        sendPayload(make_shared<string>("log::volume:" + string("AVS") + playername));
        if(portIndexMap.find(string("AVS") + playername) == portIndexMap.end()) return (shared_ptr<string>)nullptr;
        int portIndex = portIndexMap.at(string("AVS") + playername);
        pstVolumes->playingChannels &= ~(1 << portIndex);
        pstVolumes->portIndex = getActiveportIndex();
        portSocketMap.at(string("AVS") + playername) = nullptr;

        std::stringstream stream;
        std::bitset<sizeof(int32_t)*8> bits(pstVolumes->playingChannels);
        stream << bits;

        if(pstVolumes->playingChannels & (1 << portIndexMap.at("AVSAudioMediaPlayer"))) {//AVSAudio Playing
            return make_shared<string>(string("log::volume:Channel:") + stream.str());
        } else if(pstVolumes->playingChannels & 0x03fc) {
            return make_shared<string>(string("log::volume:Channel:") + stream.str());
        } else{
            sendPayload(make_shared<string>(string("log::volume:Channel:") + stream.str()));
            return make_shared<string>("devpwr::stopavs");
        }
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("devpwr::play.+", [this] {

        string playername = getEvent().substr(getEvent().find("stop") + 4);
        if(playername.find("AVS") != string::npos) return (shared_ptr<string>)nullptr;

        if(portIndexMap.find(playername) == portIndexMap.end()) return (shared_ptr<string>)nullptr;

        int portIndex = portIndexMap.at(playername);
        pstVolumes->playingChannels |= 1 << portIndex;
        std::stringstream stream;
        std::bitset<sizeof(int32_t)*8> bits(pstVolumes->playingChannels);
        stream << bits;
        string result( stream.str() );
        return make_shared<string>(string("log::volume:Channel:") + result);
    });

    e2aMap_re("devpwr::stop.+", [this] {

        string playername = getEvent().substr(getEvent().find("stop") + 4);
        if(playername.find("AVS") != string::npos) return (shared_ptr<string>)nullptr;

        if(portIndexMap.find(playername) == portIndexMap.end()) return (shared_ptr<string>)nullptr;

        int portIndex = portIndexMap.at(playername);
        pstVolumes->playingChannels &= ~(1 << portIndex);
        pstVolumes->portIndex = getActiveportIndex();

        portSocketMap.at(playername) = nullptr;
        std::stringstream stream;
        std::bitset<sizeof(int32_t)*8> bits(pstVolumes->playingChannels);
        stream << bits;
        string result( stream.str() );
        return make_shared<string>(string("log::volume:Channel:") + result);
    });

}

void Volume::connectorVolumeConnFlowDsp()
{
    ecMap_re("playerstate::On:flowreconn", "volume::connectFlowDsp");
    ecMap_re("volume::sock_nullptr[0-9]+", "volume::connectFlowDsp");
}

void Volume::InterfaceVolumeReset()
{
    e2aMap("volume::reset", [this] {
        for(int i = 0; i < players.size(); i++) {
            setVolumeRam(mconst_VolumeSt.mVolumeIndex[i],players[i]);
        }
        return make_shared<string>("volume::reset_finished");
    });

    e2aMap("volume::synOutVolume", [this] { //meaningless if connected as phone will tell speaker the vomlue.

        int8_t portIndex = pstVolumes->portIndex;
        sendAdkMsg(std::string("connectivity_bt_syncvol {volume:") + std::to_string(pstVolumes->mVolumeIndex[portIndex]) + std::string("}"));//phone consider 1 less
        sendAdkMsg(std::string("audio_volume_updated {volume:") + std::to_string((double)pstVolumes->mVolumeIndex[portIndex]/((sizeof(volume_db)/sizeof(volume_db[0]) - 1))) + std::string("}"));//phone consider 1 less
        return make_shared<string>("volume::synOutVolume_finished");
    });

    e2aMap_re("volume::resetlowVolume",[this] {
        resetlowVolume = true;
        return make_shared<string>("volume::readyToResetLowVolume");
    });

    em2mMap_re("volume::ExitContinueChange", "Continue.+","Normal");

}

void Volume::connectorVolumeReset()
{
    ecMap_re("playerstate::.+2Standby", "volume::reset");
    ecMap_re("devpwr::.+2Docked", "volume::reset");

//FlowPowerOff
    ecMap_re("playerstate::(?!recover).+2TransOff", "volume::powerOffReset");

    emecMap_re("audio::(bt:)?Stopped",  "Continue.+", "volume::ExitContinueChange");
    emecMap_re("HFPman::(?!recover).+2Off",  "Continue.+", "volume::ExitContinueChange");

    emecMap_re("button::.+_SP", "Continue.+",  "volume::ExitContinueChange");
}

shared_ptr<string> Volume::establishConn()
{
    int try_times = 80;
    string buffer;
    bool myflowdsp;
    string playername;
    int8_t portIndex = pstVolumes->portIndex;
    std::shared_ptr<FlowdspSocket> mpflowsock =  std::unique_ptr<FlowdspSocket>(new FlowdspSocket("127.0.0.1", pstVolumes->targetPort));
    if(new_flowdsp == true) {
        new_flowdsp = false;
    } else {
        new_flowdsp = true;
    }
    myflowdsp = new_flowdsp;
    while(0 != mpflowsock->Connect() && (--try_times > 0) && myflowdsp == new_flowdsp) {
        string filename(__FILE__);
        mflog(MFLOG_DEBUG,"%03lu:%s:%s:%d:connect failed", get_ms(),filename.substr(filename.find_last_of("/") == string::npos ? 0 : filename.find_last_of("/") + 1).c_str(), __func__, __LINE__);
        usleep(50000);
        mpflowsock = std::unique_ptr<FlowdspSocket>(new FlowdspSocket("127.0.0.1", pstVolumes->targetPort));
    }
    if(try_times > 0) {
        pstVolumes->flowdsp_state = true;

        if(portIndexMap.find(pstVolumes->currentPortname) == portIndexMap.end()) return make_shared<string>("volume::currentPortnameNotFound");
        portSocketMap.at(pstVolumes->currentPortname) = mpflowsock;
        char data_received[128];
        mpflowsock->Receive(data_received);
        string filename(__FILE__);
        //syslog(LOG_NOTICE,"%s:%s:%d:%s",filename.substr(filename.find_last_of("/") == string::npos ? 0 : filename.find_last_of("/") + 1).c_str(), __func__, __LINE__, data_received);
        //Initialise volume and base control
#ifdef CUE_CONST_VOLUME
        if(pstVolumes->targetPort == 50020)
        {
            sendfunc(mpflowsock,(char*)"set/MUX_2/select/0/");
            sendfunc(mpflowsock,(char*)"set/MUX_3/select/0/");
            return make_shared<string>("Log::volume:muxConnected");
        }
#endif
        char msg[32];
        //EQ pre setting
        snprintf(msg, 32, "set/GAIN_ST_2/gain/%d/", bass_table[pstVolumes->bass_index]);
        sendfunc(mpflowsock,msg);
        //set first volume
        if(pstVolumes->mVolumeIndex[portIndex] == 0 && resetlowVolume) {
            if(portIndex != portmap.at("cue")) {

                pstVolumes->mVolumeIndex[portIndex] = 3;
                pstVolumes->deltaDB[portIndex] = 0;
                resetlowVolume = false;
            }
        }
        playername = players[portIndex];
        int volume = volume_db[pstVolumes->mVolumeIndex[portIndex]] + pstVolumes->deltaDB[portIndex];
        setVolume(pstVolumes->mVolumeIndex[portIndex], volume, true, playername);
        usleep(mDecodingDelayus);
        mDecodingDelayus = 0;
        sendfunc(mpflowsock,(char*)"set/MUX_2/select/0/");
        sendfunc(mpflowsock,(char*)"set/MUX_3/select/0/");
        return make_shared<string>("volume::flowdsp_connect_success"); //setVolume(volume_db[mVolumeIndex[portIndex]]);
    }
    else {
        std::this_thread::sleep_for (std::chrono::seconds(1));
    }
    return make_shared<string>(std::string("volume::flowdsp_connect_failed:")+std::to_string(pstVolumes->targetPort));
}

shared_ptr<string> Volume::setVolume(int8_t volindex, int volumedb, bool syncIn, string playername)
{
    uint8_t portIndex;
    if(playername.empty()) {
        portIndex  = pstVolumes->portIndex; //recent port
    } else {
        portIndex  = portIndexMap.at(playername); //AVS player port
    }
    sendAdkPayload(string("Log::volume:portIndex:") + std::to_string(portIndex));

    if(!syncIn) {
        avssyncing++;
        std::thread (&ModeFlow::delayed_e, this, "volume::reduceSync",1, 0).detach();
        if(portIndex == 20017) {
            sendAdkPayload(string("volume::syncToAvsAlert:") + std::to_string((unsigned char)((float)volindex/21*100)));
        } else if(portIndex > 1 && portIndex < 10) {
            sendAdkPayload(string("volume::syncToAvsSpeaker:") + std::to_string((unsigned char)((float)volindex/21*100)));
            sendAdkPayload(string("Log::volume:deltaDB:") + std::to_string(pstVolumes->deltaDB[portIndex]) + ":index:" + std::to_string(portIndex));
        }
    }

    //??? not useful  Kim:can use it DEBUG
    //sendAdkPayload(string("volume::setvolume:") + playername + ":" + std::to_string(volindex) + ":" + std::to_string(volumedb));

    if(playername.compare("AVSAudioMediaPlayer") == 0 && AVS_WakeUp && I_i == 0){
        I_i = 1;//in order not get the volumedb twice times
        volume_step = CurrentVol(volumedb);
    }

    setVolumeRam(volindex, playername);
    return ajustVolume(volumedb,syncIn,playername);
}

int Volume::CurrentVol(int db){
    int db_bak = db;
    usleep(5000);//in order not get the volumedb twice times
    while (cur_db)
    {
        //sendAdkPayload(string("Kim get vol : ")+std::to_string(db_bak));//debug
        if( db_bak <= volume_db[1]){
            db_bak = volume_db[0];
            cur_db = false;
            return num_cur = 0;
            break;
        }
        for(num_cur = 0; num_cur <=21; num_cur++){
            if( db_bak == volume_db[num_cur]){
                //sendAdkPayload(string("Kim::setvolume:") + std::to_string(db_bak) + " : " + std::to_string(num_cur));//debug
                I_i = 0;
                cur_db = false;
                return num_cur;
            }
        }
            db_bak++;
            if( db_bak > 0 ) cur_db = false;
    }

    I_i = 0;
    return num_cur = 2;
}
bool Volume::CheckAVSPlayer(){

        map<string, std::shared_ptr<FlowdspSocket>>:: iterator iter;
        for(iter = portSocketMap.begin(); iter != portSocketMap.end(); iter++) {
             if(iter->second != nullptr){
                 if(iter->first.compare("AVSAlertsMediaPlayer") == 0)
                     Alerting = true;
             }
        }
        for(iter = portSocketMap.begin(); iter != portSocketMap.end(); iter++) {
            if(iter->second != nullptr){
                if(iter->first.compare("AVSSpeakMediaPlayer") == 0         ||
                   iter->first.compare("AVSNotificationsMediaPlayer") == 0 ||
                   iter->first.compare("AVSBluetoothMediaPlayer") == 0     ||
                   iter->first.compare("AVSRingtoneMediaPlayer") == 0      ||
                   iter->first.compare("AVSAlertsMediaPlayer") == 0        ||
                   iter->first.compare("AVSSystemSoundMediaPlayer") == 0) {
                        Player_flag = true;
                        return Player_flag;
                   }
            }else Player_flag = false;
        }
        return Player_flag;
}
shared_ptr<string> Volume::setVolumeRam(int8_t volindex, string playername)
{

    if(volindex < 0 || volindex >= sizeof(volume_db)/sizeof(volume_db[0]))
    {
        return make_shared<string>("log::volume:wrong_volindex");
    }

    if(!playername.empty()) { //set a specific volume
        if (portIndexMap.find(playername) != portIndexMap.end())
            pstVolumes->mVolumeIndex[portIndexMap.at(playername)] = volindex;
        return (shared_ptr<string>)nullptr;
    }

    for (std::pair<string, std::shared_ptr<FlowdspSocket>> element : portSocketMap) {//set all volume except tone and alertmediaplayer
        // Accessing KEY from element
        std::string word = element.first;
        // Accessing VALUE from element.
        std::shared_ptr<FlowdspSocket> &sptmpsock = element.second;
#ifdef CUE_CONST_VOLUME
        if(word.compare("cue") == 0 && playername.compare("cue") !=0 ) continue;
#endif
        if(word.compare("AVSAlertsMediaPlayer") == 0 && playername.compare("AVSAlertsMediaPlayer") !=0 ) continue;

        pstVolumes->mVolumeIndex[portIndexMap.at(word)] = volindex;

    }

    return make_shared<string>("log::volume:ramVoumeSet");
}

shared_ptr<string> Volume::ajustVolume(int volume, bool syncIn, string playername)
{
    char msg[32];
    char msg_Aud[32];
    shared_ptr<string> pretS = make_shared<string>("volume::setVolume");
    int8_t portIndex = pstVolumes->portIndex;

    if(!syncing_out && !syncIn && playername.empty()) {
        syncing_out = true;
        int volumeIndex = pstVolumes->mVolumeIndex[portIndex];
        std::thread(&ModeFlow::delay_thread, this, "Volume:syncing_out_complete:" + std::to_string(volumeIndex), 0, 100).detach();
    }

    if(pstVolumes->flowdsp_state == true) {
        string filename(__FILE__);
        //syslog(LOG_NOTICE,"%s:%s:%d:ivian R1 FlowDsp Volume change",filename.substr(filename.find_last_of("/") == string::npos ? 0 : filename.find_last_of("/") + 1).c_str(), __func__, __LINE__);

        snprintf(msg, 32, "set/GAIN_ST_1/gain/%d/", volume);
        //syslog(LOG_NOTICE,"msg is %s", msg);
        if(!playername.empty()) { //set a specific volume
            // Accessing VALUE from element.

            if(portIndexMap.find(playername) == portIndexMap.end()) return make_shared<string>("ajustVolume::notfound");
            std::shared_ptr<FlowdspSocket> &sptmpsock =  portSocketMap.at(playername);

            if(nullptr == sptmpsock) return (shared_ptr<string>)nullptr;

            pretS = sendfunc(sptmpsock, msg);
			if(pretS != nullptr){
	            if(pretS->find("volume::send_failed") != string::npos)
	            {
	                return make_shared<string>(*pretS + ":" + std::to_string(pstVolumes->portIndex) + ":" + std::to_string(pstVolumes->mVolumeIndex[portIndex]));
	            }else{
		            return make_shared<string>(*pretS + std::to_string(pstVolumes->mVolumeIndex[portIndex]) + ":" + std::to_string(pstVolumes->deltaDB[portIndex]) + ":" + std::to_string(volume));
	            }
			}else{
				return pretS;
			}
        }

        for (std::pair<string, std::shared_ptr<FlowdspSocket>> element : portSocketMap) {//set all volume except tone and alertmediaplayer
            // Accessing KEY from element
            std::string word = element.first;
            // Accessing VALUE from element.
            std::shared_ptr<FlowdspSocket> &sptmpsock = element.second;
#ifdef CUE_CONST_VOLUME
            if(word.compare("cue") == 0 && playername.compare("cue") !=0 ) continue;
#endif

            if(word.compare("AVSAlertsMediaPlayer") == 0 && playername.compare("AVSAlertsMediaPlayer") !=0 ) continue;

            if(nullptr == sptmpsock) continue;
            if(word.compare("AVSAudioMediaPlayer") == 0 && sptmpsock != nullptr) {
                if(CheckAVSPlayer() || AVS_WakeUp){
                    Player_flag = false;
                    if(volume == 0)continue;
                    if (Alerting) {Alerting = false; continue;}
                    if(button_up)volume_step++;
                    else volume_step--;
                    setvol_bak = volume_db[volume_step];
                    snprintf(msg_Aud, 32, "set/GAIN_ST_1/gain/%d/", setvol_bak);
                    //std::string log_g = msg_Aud;
                    //sendAdkPayload("Kim Change volume :"+ word +" "+log_g);//debug
                    T_t = true;
                }
            }

            if(T_t){
                std::string log_t = msg_Aud;
                pretS = sendfunc(sptmpsock, msg_Aud);
                T_t = false;
                //sendAdkPayload("Kim success_1 :"+ word + log_t + "step "+std::to_string(volume_step));//debug
            }else{
                std::string log_t = msg;
                pretS = sendfunc(sptmpsock, msg);
                //sendAdkPayload("Kim success_2 :"+ word + log_t);//debug
            }
			if(pretS){
	            if(pretS->find("volume::send_failed") != string::npos)
	            {
	                sendPayload(make_shared<string>(*pretS + ":" + std::to_string(pstVolumes->portIndex) + ":" + std::to_string(pstVolumes->mVolumeIndex[portIndex])));
	            }
	            sendPayload(make_shared<string>(*pretS + std::to_string(pstVolumes->mVolumeIndex[portIndex]) + ":" + std::to_string(pstVolumes->deltaDB[portIndex]) + ":" + std::to_string(volume) + word));
			}else{
				return pretS;
			}
        }
    } else {

        return make_shared<string>("volume::flowdsp_state_err");
    }

    return (shared_ptr<string>)nullptr;
}

shared_ptr<string> Volume::sendfunc(std::shared_ptr<FlowdspSocket> &sptmpsock, char* msg) {
    if(!sptmpsock)
        return make_shared<string>("volume::sock_nullptr");

    if(!sptmpsock->Send(msg)) {
        char data_receive[128] = {0};
        int success = sptmpsock->Receive(data_receive);
        return make_shared<string>("volume::send_success");
    } else {
        sptmpsock = nullptr;
        return make_shared<string>("volume::send_failed");
    }

    return (shared_ptr<string>)nullptr;

}

shared_ptr<string> Volume::setAbsVolume(int absVol, string playername, bool syncIn)
{
    float findex = (float)absVol*SPEAKER_MAX_VOLUME_STEPS/ABS_VOL_BASE;

    int8_t index = (int8_t)findex;
    float deltaIndex = findex - index;
    int portIndex = 0;

    if(playername.empty()) {
        portIndex  = pstVolumes->portIndex; //recent port
    } else {
        portIndex  = portIndexMap.at(playername); //AVS player port
    }
    //count DeltaDB
    if(index < sizeof(volume_db)/sizeof(volume_db[0]) -2 ) {
        pstVolumes->deltaDB[portIndex] = (int)(round(deltaIndex*((float)volume_db[index + 1] - (float)volume_db[index])));
    } else {
        pstVolumes->deltaDB[portIndex] = 0;
    }
    //count mVolumeIndex[portIndex]
    if(index >=0 && index < sizeof(volume_db)/sizeof(volume_db[0])) {
        // Ram volume is set in setVolumeRam pstVolumes->mVolumeIndex[portIndex] = index;
        return setVolume(index, volume_db[index] + pstVolumes->deltaDB[portIndex], syncIn, playername);
    } else {
        return make_shared<string>("btVolOutofRange");
    }
}

void Volume::FlowRetriveStoreVolume()
{
    //FlowPowerUp
    e2aMap("devpwr::Shutdown2Transon", bind(&Volume::retreiveDB, this));

    //FlowShutdownReboot
    e2aMap_re("actions::.+(shutdown|reboot).+", bind(&Volume::SavetoDB, this));
    //FlowShutdownReboot
    e2aMap_re("devpwr::.+2(Semi)?Off", bind(&Volume::SavetoDB, this));

}

shared_ptr<string> Volume::retreiveDB()
{
    for(int i =0; i < sizeof(pstVolumes->mVolumeIndex)/sizeof(pstVolumes->mVolumeIndex[0]); i++)
    {
        int volume;
        config_->Read(&volume, "modeflow.volume." + std::to_string(i));
        pstVolumes->mVolumeIndex[i] = volume;
    }

    return make_shared<string>("volume::retreiveDB");
}

shared_ptr<string> Volume::SavetoDB()
{
    for(int i =0; i < sizeof(pstVolumes->mVolumeIndex)/sizeof(pstVolumes->mVolumeIndex[0]); i++)
    {
        config_->Write(pstVolumes->mVolumeIndex[i], "modeflow.volume." + std::to_string(i));
    }

    return make_shared<string>("volume::saveDB:");
}

shared_ptr<string> Volume::SetBaseGain(int gain)//gain: -120 ~ +9
{

    if(pstVolumes->flowdsp_state == true) {
        char msg[32];
        shared_ptr<string> pretS =  make_shared<string>("bass_set_failed");
        string filename(__FILE__);
        //syslog(LOG_NOTICE,"%s:%s:%d:Rivian R1 FlowDsp BASS Toggle",filename.substr(filename.find_last_of("/") == string::npos ? 0 : filename.find_last_of("/") + 1).c_str(), __func__, __LINE__);
        snprintf(msg, 32, "set/GAIN_ST_2/gain/%d/", gain);
        //syslog(LOG_NOTICE,"msg is %s", msg);

        for (std::pair<string, std::shared_ptr<FlowdspSocket>> element : portSocketMap) {
            // Accessing KEY from element
            std::string word = element.first;
            // Accessing VALUE from element.
            std::shared_ptr<FlowdspSocket> &sptmpsock = element.second;

            if(word.compare("cue") == 0) continue;
            if(nullptr == sptmpsock) continue;
            // Change bass table
            pstVolumes->bass_index = 1;
            bass_table[1] = gain;

            pretS = sendfunc(sptmpsock, msg);
            if(pretS && pretS->find("volume::send_failed") !=string::npos)
            {
                sendPayload(make_shared<string>(std::string("bass_gain:") + std::to_string(bass_table[pstVolumes->bass_index])));
            }
        }

    } else {
        return make_shared<string>("flowdsp_Not_ready");
    }

    return make_shared<string>("volume::bassGainSet");
}

uint8_t Volume::getActiveportIndex()
{
    for(uint8_t i = 0; i < portIndexMap.size(); i++) {
        if(pstVolumes->playingChannels & (1 << i)) {
            return i;
        }
    }

    return 0;
}

