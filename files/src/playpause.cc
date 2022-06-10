/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 PlayPause Mode management
 *
 */

#include <sstream>
#include <cstring>
#include "playpause.h"
PlayPause::PlayPause():ModeFlow("playpause")
{
    /*
    Register relation ship: input event -> target mode.
    */

    //State transitons
    e2mMap("playpause::button_DP","SkipNextTrack");
    e2mMap("playpause::button_TP","SkipPreviousTrack");
    e2mMap("trigger::playpause_skip_timer_out","PlayPause");
    e2mMap_re("HFPman::(?!recover).+2(CallIn|CallOut|Accepted)","HFP");
    e2mMap_re("HFPman::(?!recover).+2(Disconnected|Connected|Off)","PlayPause");

    //filtered in events
    em2eMap("playpause::button_SP","PlayPause", "PlayPauseToggle");
    em2eMap("playpause::button_SP","SkipNextTrack", "SkipNext");
    em2eMap("playpause::button_SP","SkipPreviousTrack", "SkipPrevious");

//Connector
    e2aMap_re("source::(?!recover).+2bt", [this] {
        /*
          R1-199. Comment the following line if we treat R1-199 as a bug
        */
        std::thread (&ModeFlow::delay_thread, this, "playpause:play",2, 0).detach();
        return make_shared<string>("volume::synOutVolume");
    });
//Interface/Acuator
    em2aMap_re("Thread::playpause:play", "PlayPause",[this] {
        sendAdkMsg("audio_track_play{}");
        return (shared_ptr<string>)nullptr;
    });

    /*
        Do recovery, PlayPause shall have a Init mode to differentiate with Off.
    */
    modeRecovery("PlayPause");
}


