/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MODEFLOW_VOLUME_H
#define MODEFLOW_VOLUME_H
#include  "modeflow.h"
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

class Volume:public ModeFlow
{
    struct VolumeSt;
public:
    Volume();
    virtual void IfConnector();
    virtual void Input();
    virtual void Internal();
    virtual void Output();
    virtual void IOConnector();

    void InterfaceVolumeConnFlowDsp();
    void connectorVolumeConnFlowDsp();
    void InterfaceVolumeReset();
    void connectorVolumeReset();
    void FlowRetriveStoreVolume();
    int  CurrentVol(int db);

    shared_ptr<string> setVolume(int8_t volindex, int volumedb, bool syncIn = false, string playername = "");
    shared_ptr<string> setVolumeRam(int8_t volindex, string playername = "");
    shared_ptr<string> ajustVolume(int volume, bool syncIn = false, string playername = "");
    shared_ptr<string> sendfunc(std::shared_ptr<FlowdspSocket> &sptmpsock, char* msg);
    shared_ptr<string> establishConn();
    shared_ptr<string> setAbsVolume(int absVol, string playername = "", bool synIn = true);
    bool  CheckAVSPlayer();
    shared_ptr<string> retreiveDB();
    shared_ptr<string> SavetoDB();
    shared_ptr<string> SetBaseGain(int gain);//gain: -120 ~ +9
    uint8_t getActiveportIndex();
private:
    const int volume_db[22] = {-120, -54, -50,-46,-42,-38,-34,-31,-28,-25,-22,-20,-18,-16,-14,-12,-10,-8,-6,-4,-2,0};
    const int speed_table[22] = {5000,5000,4500,4000,3500,3000,2500,2000,1500,1000,500,500,500,500,500,500,500,500,500,500,500,500};
    bool Player_flag = false;
    bool cur_db = false;
    bool AVS_WakeUp = 0;
    bool T_t = false;
    bool Alerting = false;
    int setvol_bak = 0;
    int I_i = 0;
    int num_cur = 0;
    int volume_step = 0;
    bool button_up = false;
#define SPEAKER_MAX_VOLUME_STEPS (sizeof(volume_db)/sizeof(volume_db[0]) -1)
#define ABS_VOL_BASE 127
//    std::shared_ptr<FlowdspSocket> mpflowsock=nullptr,tmppsock=nullptr;
    int8_t bass_table[2] = {0, -6};
    bool syncing_out = false;

    const map<string, int> portmap = {{"bt", 50013},{"phone", 50021},
        {"AVSAudioMediaPlayer",         20012},
        {"AVSSpeakMediaPlayer",         20013},
        {"AVSNotificationsMediaPlayer", 20014},
        {"AVSBluetoothMediaPlayer",     20015},
        {"AVSRingtoneMediaPlayer",      20016},
        {"AVSAlertsMediaPlayer",        20017},
        {"AVSSystemSoundMediaPlayer",   20018},{"avsReserveMediaPlayer", 20019},{"cue", 50020},{"airplay", 50010},{"spotify", 50012},{"snapclient", 50014}
    };

    const map<string, int> portIndexMap = {{"bt", 0},{"phone", 1},
        {"AVSAudioMediaPlayer",             2},
        {"AVSSpeakMediaPlayer",         3},
        {"AVSNotificationsMediaPlayer",4},
        {"AVSBluetoothMediaPlayer",     5},
        {"AVSRingtoneMediaPlayer",     6},
        {"AVSAlertsMediaPlayer",        7},
        {"AVSSystemSoundMediaPlayer",   8},{"avsReserveMediaPlayer", 9},{"cue", 10},{"airplay", 11},{"spotify", 12},{"snapclient", 13}
    };

    const vector<string> players = {{"bt"},{"phone"},
        {"AVSAudioMediaPlayer"},
        {"AVSSpeakMediaPlayer"},
        {"AVSNotificationsMediaPlayer"},
        {"AVSBluetoothMediaPlayer"},
        {"AVSRingtoneMediaPlayer"},
        {"AVSAlertsMediaPlayer"},
        {"AVSSystemSoundMediaPlayer"},{"avsReserveMediaPlayer"},{"cue"},{"airplay"},{"spotify"},{"snapclient"}
    };

    map<string, std::shared_ptr<FlowdspSocket>> portSocketMap = {{"bt", nullptr},{"phone", nullptr},
        {"AVSAudioMediaPlayer",             nullptr},
        {"AVSSpeakMediaPlayer",         nullptr},
        {"AVSNotificationsMediaPlayer",nullptr},
        {"AVSBluetoothMediaPlayer",     nullptr},
        {"AVSRingtoneMediaPlayer",     nullptr},
        {"AVSAlertsMediaPlayer",        nullptr},
        {"AVSSystemSoundMediaPlayer",   nullptr},{"avsReserveMediaPlayer", nullptr},{"cue", nullptr},{"airplay", nullptr},{"spotify", nullptr},{"snapclient", nullptr}
    };

    uint32_t mDecodingDelayus = 0;
    int tmpPort = 0;
    string tmpPortName;
    bool new_flowdsp = false, resetlowVolume = false;
    uint32_t avssyncing = 0;

    struct VolumeSt {
        int targetPort = 0;
        int8_t mVolumeIndex[14] = {7,7,11,11,11,11,11,11,11,11,11,7,7,7}; //-35db
        int8_t speedIndex = 0, bass_index = 0, portIndex = 0;
        bool speedIncreasing = true;
        bool flowdsp_state = false;
        int deltaDB[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
        char currentPortname[32];
        int32_t playingChannels = 0;
        bool avsReady = false;
        VolumeSt() {
        }
    } * pstVolumes;
    const struct VolumeSt mconst_VolumeSt;
};

#endif
