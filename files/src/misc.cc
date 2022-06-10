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
#include "misc.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

using adk::msg::AdkMessageService;
//using adk::mode;
extern int loglevel;
Misc::Misc():ModeFlow("misc")
{
//modeflow interface
    e2aMap(getModuleName() + "::getthreads", [this] {
        return make_shared<string>("modeflow::threads:" + std::to_string(getThreadCounter()));
    });//used for debug to set mode

    e2aMap(getModuleName() + "::logrelease", [this] {
        loglevel = MFLOG_RELEASE;
        return make_shared<string>("modeflow::loglevel:" + std::to_string(loglevel));
    });

    e2aMap(getModuleName() + "::logdebug", [this] {
        loglevel = MFLOG_DEBUG;
        return make_shared<string>("modeflow::loglevel:" + std::to_string(loglevel));
    });

    e2aMap(getModuleName() + "::logstatistics", [this] {
        loglevel = MFLOG_STATISTICS;
        return make_shared<string>("modeflow::loglevel:" + std::to_string(loglevel));
    });

    e2aMap(getModuleName() + "::logcharge", [this] {
        loglevel = MFLOG_CHARGE;
        return make_shared<string>("modeflow::loglevel:" + std::to_string(loglevel));
    });

//modeflow interface
    e2aMap(getModuleName() + "::getsmlist", [this] {
        syslog(LOG_NOTICE,"---------getting sm list-----------\n");
        for(auto tmp:ModeFlow::event2smList) {

            cout << tmp.first << std::endl;
            syslog(LOG_NOTICE,"field:%s", tmp.first.c_str());
            for(ModeFlow * tmps:tmp.second) {
                syslog(LOG_NOTICE,"\t%s", tmps->getModuleName().c_str());
                cout << "\t" << tmps->getModuleName() << std::endl;

            }
        }
        return (shared_ptr<string>)nullptr;
    });//used for debug to set mode

//fatory reset
    e2taMap_re("button::FACTORYRESET_VLH", "", bind(&Misc::factory_reset, this));
    ecMap_re("misc::reboot" , "actions::/etc/exitscripts/board-script/reboot.sh 0 factory_reset &");

//Tone Controller
    ecMap_re("audio::cue:Playing","devpwr::playtone","volume::setPortcue","volume::connectFlowDsp");
    ecMap_re("audio::cue:Stopped","devpwr::stopcue");

//Airplay Controller
    ecMap_re("audio::airplay:Playing","devpwr::playairplay","volume::setPortairplay","volume::connectFlowDsp");
    ecMap_re("audio::airplay:Stopped","devpwr::stopairplay");

//spotify Controller
    ecMap_re("audio::spotify:Playing","devpwr::playspotify","volume::setPortspotify","volume::connectFlowDsp");
    ecMap_re("audio::spotify:Stopped","devpwr::stopspotify");

//snapclient Controller
    ecMap_re("audio::snapclient:Playing","devpwr::playsnapclient","volume::setPortsnapclient","volume::connectFlowDsp");
    //ecMap_re("audio::snapclient:Stopped","devpwr::stopsnapclient");

//e2de test interface
    e2dmeMap_re("misc::teste2de", "misc::OK", ".+", 1, 0);
//e2ta_re test
    e2taMap_re("misc::teste2ta", "e2taRet", bind(&Misc::e2tateste, this));
    em2mMap_re("misc::testmultimode", "Active","_m#modeX#modeY");
    em2mMap_re("misc::testmultimode", "_m#modeX#(.+)","_m#modeA#$1");
    modeRecovery("Active", 0, 32, nullptr, false);
}

shared_ptr<string> Misc::factory_reset()
{
    sendAdkMsg("led_start_pattern {pattern:61}");
    exec_cmd("sh /etc/factory-test/r1/factory_reset.sh");
    std::thread (&ModeFlow::delayed_e, this, "misc::reboot",3, 0).detach();
    return (shared_ptr<string>)nullptr;
}

shared_ptr<string> Misc::e2tateste()
{
    return make_shared<string>("misc::e2taOK");
}

