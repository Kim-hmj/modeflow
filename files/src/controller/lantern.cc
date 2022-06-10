/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 Lantern Mode management
 *
 */

#include <sstream>
#include <cstring>
#include "modeflow.h"
#include "lantern.h"
#include "led.h"
Lantern::Lantern():ModeFlow("lantern")
{
    msgProc();

    campfirePause_ = true;
    std::thread(&Lantern::CampfireLEDAnimation, this).detach();
    /*
        Do recovery, Lantern shall have a Init mode to differentiate with Off.
    */
    string recoverInfo = modeRecovery("Init", 0, 32, nullptr, true);

    std::thread(&ModeFlow::init_actions,this, [this, recoverInfo] {return make_shared<string>(recoverInfo);}).detach();
}

void Lantern::IfConnector()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    //SeqPowerOn
    ecMap_re("devpwr::Transon2On" , "lantern::LedsOff");
    ecMap_re("devpwr::Docked2Transon", "lantern::LedsOff");
    ecMap_re("devpwr::DockStart2On", "lantern::LedsOff");

    //SeqStandby
    //Entering Standby
    emecMap_re("playerstate::(?!recover).+2Standby", "(?!Off|Init|RGB|Campfire|Night|Classic).+", "lantern::LedsStandby");
    //Exiting Standby
    ecMap_re("playerstate::Standby2Idle", "lantern::LedsExitStandby");
    ecMap_re("playerstate::Standby2On", "lantern::LedsExitStandby");

    //TargetInit
    //Fade to off when docked
    ecMap_re("devpwr::(Standby|On)2Docked",   "lantern::LedsInit");
    //Fade to off when system Off
    ecMap_re("devpwr::(On|Standby)2TransOff",   "lantern::LedsInit");

    //For Testing purpose, enter test from any, exit to Low state.
    ecMap_re("devpwr::ForceOffLeds",   "lantern::LedsInit");
    ecMap_re("devpwr::ForceOnLeds",   "lantern::LedsLow");
}

void Lantern::Input()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    //SeqPowerOn
    em2mMap_re("lantern::LedsOff", "(?!Off).+", "Off");

    //SeqStandby
    em2mMap_re("lantern::LedsStandby", "(?!Off|Init|RGB|Campfire|Night|Classic).+", "Standby");
    em2mMap("lantern::LedsExitStandby", "Standby", "Low");
    em2mMap_re("lantern::LedsInit", "(?!Init).+", "Init");
    em2mMap_re("lantern::LedsLow", "(?!Low).+", "Low");

    em2taMap_re("lantern::Leds.+", "RGB.+", "", [this] {
        string delayed_event = getEvent().substr(getEvent().find_last_of(":") + 1);
        sleep(1);
        return make_shared<string>(string("lantern::") + delayed_event);
    });
    //SeqRGB
    em2mMap_re("lantern::fault", "(?!RGB|Init).+", "RGB_err");
    em2dmeMap_re("lantern::fault", "RGB(?!_err).+", "lantern::fault", "(?!RGB_err|Init).+", 5, 0);
    em2mMap_re("lantern::BtPairingRun", "(?!RGB|Init).+","RGB_BtPairingRun");
    emecMap_re("lantern::Blue3secFadeoff", "RGB_BtPairingRun", "led::" + std::to_string(Pattern_LanternBlue3secFadeoff));
    em2mMap_re("lantern::Blue3secFadeoff", "(?!RGB|Init|Campfire|Night|Classic).+","RGB_BtConnectedBlue");
    em2mMap_re("lantern::Amber3secFadeoff", "(?!RGB|Init|Campfire|Night|Classic).+","RGB_WifiConnectedAmber");
    emecMap_re("lantern::Blue3secFadeoff", "(Campfire|Night|Classic)", "led::" + std::to_string(Pattern_LanternBlue3secFadeoff));
    emecMap_re("lantern::Amber3secFadeoff", "(Campfire|Night|Classic)", "led::" + std::to_string(Pattern_LanternAmber3secFadeoff));
    emecMap_re("actuators::led_stop:"+std::to_string(Pattern_LanternBlue3secFadeoff), "RGB_BtConnectedBlue", "Thread::lantern:history");
    emecMap_re("actuators::led_stop:"+std::to_string(Pattern_LanternAmber3secFadeoff), "RGB_WifiConnectedAmber", "Thread::lantern:history");

    //SeqIllumination
    //Register relation ship: input event + current mdoe -> target mode.
    em2mMap("lantern::button_SP", "Low"     ,"Medium");
    em2mMap("lantern::button_SP", "Medium"  ,"High");
    em2mMap("lantern::button_SP", "High"    ,"Off");
    em2mMap("lantern::button_SP", "Off"     ,"Low");
    em2mMap("lantern::button_LH", "Off"     ,"High");
    em2mMap("lantern::button_LH", "Low"     ,"Off");
    em2mMap("lantern::button_LH", "Medium"  ,"Off");
    em2mMap("lantern::button_LH", "High"    ,"Off");

    //CampfireMode
    em2mMap_re("lantern::campfirebtn_LH", "(?!RGB|Campfire|Night|Classic).+", "Campfire");
    //Exit CampfireMode
    emecMap_re("lantern::button_(SP|LH)", "Campfire", "lantern::ExitCampfireMode");
    emecMap_re("connect::button_(SP|DP|LH)", "Campfire", "lantern::ExitCampfireMode");

    //NightMode
    em2mMap_re("lantern::EnterNightMode", "(?!RGB|Campfire|Night|Classic).+", "Night");
    //Exit NightMode
    emecMap_re("lantern::ExitNightMode", "Night", "lantern::ExitSpecificMode");

    //ClassicMode
    em2mMap_re("lantern::classic_TP", "(?!RGB|Campfire|Night|Classic).+", "Classic");
    //Exit ClassicMode
    emecMap_re("lantern::button_(SP|LH)", "Classic", "lantern::ExitSpecificMode");

    //SeqRGB
    em2mMap_re("lantern::button_DP", "(?!RGB|Init|Campfire|Night|Classic).+", "RGB_batlevel");

    //SeqStandby
    em2mMap_re("lantern::confirmStandby", "(?!Standby|Off|Init|Campfire|Night|Classic).+","Standby");
}

void Lantern::Internal()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);
    em2aMap("lantern::ExitCampfireMode", "Campfire", [this] {
        if (getPreviousMode().find("RGB") != string::npos) {
            return setMode(GetRealHistoryMode());
        } else {
            if (getPreviousMode().find("Campfire") != string::npos) {
                return setMode("Off");
            } else {
                return setMode(getPreviousMode());
            }
        }
    });

    em2aMap_re("lantern::ExitSpecificMode", "(Night|Classic)", [this] {
        if (getPreviousMode().find("RGB") != string::npos) {
            return setMode(GetRealHistoryMode());
        } else {
            if (regex_match(getPreviousMode(), regex(R"(Night|Classic)"))) {
                return setMode("Off");
            } else {
                return setMode(getPreviousMode());
            }
        }
    });
}

void Lantern::Output()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);
    e2deMap_re("Thread::lantern:history","playerstate::checkStandby",1,0);

    //Enter CampfireMode
    e2aMap_re("lantern::(?!RGB|Campfire|Night|Classic).+2Campfire", [this] {
        if (!regex_match(getPreviousMode(), regex(R"(RGB.+)"))) {
            SetRealHistoryMode(getPreviousMode());
        }
        if(regex_match(getModuleMode("playerstate"), regex(R"(Idle|Standby)"))) {
	        exec_cmd("/etc/initscripts/start_campfire_voice.sh /data/cue/r1-Campfire_60s.wav -5 &");
        }
        CampfireStart();
        return (shared_ptr<string>)nullptr;
    });
    //Exit CampfireMode
    e2aMap_re("lantern::Campfire2(?!RGB|Campfire|Night|Classic).+", [this] {
        CampfireStop();

        exec_cmd("/etc/initscripts/stop_campfire_voice.sh &");
        return (shared_ptr<string>)nullptr;
    });

    //Enter NightMode
    e2aMap_re("lantern::(?!RGB|Campfire|Night|Classic).+2Night", [this] {
        if (!regex_match(getPreviousMode(), regex(R"(RGB.+)"))) {
            SetRealHistoryMode(getPreviousMode());
        }
        sendAdkMsg("led_start_pattern {pattern:109}");
        return (shared_ptr<string>)nullptr;
    });
    //Exit NightMode
    e2aMap_re("lantern::Night2(?!RGB|Campfire|Night|Classic).+", [this] {
        sendAdkMsg("led_start_pattern {pattern:68}");
        return (shared_ptr<string>)nullptr;
    });

    //Enter ClassicMode
    e2aMap_re("lantern::(?!RGB|Campfire|Night|Classic).+2Classic", [this] {
        if (!regex_match(getPreviousMode(), regex(R"(RGB.+)"))) {
            SetRealHistoryMode(getPreviousMode());
        }
        sendAdkMsg("led_start_pattern {pattern:108}");
        return (shared_ptr<string>)nullptr;
    });
    //Exit ClassicMode
    e2aMap_re("lantern::Classic2(?!RGB|Campfire|Night|Classic).+", [this] {
        sendAdkMsg("led_start_pattern {pattern:68}");
        return (shared_ptr<string>)nullptr;
    });
}

void Lantern::IOConnector()
{
    mflog(MFLOG_STATISTICS,"modeflow:%s %s constructed", getModuleName().c_str(), __FUNCTION__);

    em2aMap_re("audio::(?!cue).+:Play.+", "Campfire", [this]{
        exec_cmd("/etc/initscripts/stop_campfire_voice.sh &");
        return (shared_ptr<string>)nullptr;
    });

    em2aMap_re("audio::(?!cue).+:Stop.+", "Campfire", [this]{
        if (!isAppRunning("start_campfire_voice.sh"))
            exec_cmd("/etc/initscripts/start_campfire_voice.sh /data/cue/r1-Campfire_60s.wav -5 &");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("led::.+", [this] {
		if(getModuleMode("devpwr").find("Docked") == string::npos){
			ELEDPattern patternID = (ELEDPattern)std::stoi(getEvent().substr((getEvent().find_last_of(":")+1)));
            if (regex_match(getCurrentMode(), regex(R"(Campfire|Night|Classic)"))) {
                int pattern_group;
                bool success = config_->Read(&pattern_group, "modeflow.pattern_group." + std::to_string((int)patternID));
                if (success) {
                    if (pattern_group == 0 || pattern_group == 1 || pattern_group == 13) {
                        if (getCurrentMode() == "Campfire")
                            CampfireStop();
                        else
                            sendAdkMsg("led_start_pattern {pattern:68}");
                    }
                }
            }
		}
		return (shared_ptr<string>)nullptr;
	});

    e2aMap_re("actuators::led_stop:.+", [this] {
        ELEDPattern patternID = std::stoi(getEvent().substr((getEvent().find_last_of(":")+1)));
        if (regex_match(getCurrentMode(), regex(R"(Campfire|Night|Classic)")))  {
            int pattern_group;
            bool success = config_->Read(&pattern_group, "modeflow.pattern_group." + std::to_string((int)patternID));
            if (success) {
                if (pattern_group == 0 || pattern_group == 1 || pattern_group == 13) {
                    if (getCurrentMode() == "Campfire")
                        CampfireStart();
                    else if (getCurrentMode() == "Night")
                        sendAdkMsg("led_start_pattern {pattern:109}");
                    else
                        sendAdkMsg("led_start_pattern {pattern:108}");
                }
            }
        }
        return (shared_ptr<string>)nullptr;
    });
}

void Lantern::CampfireLEDAnimation()
{
    int Stime = 0;
    int Freq = 0;
    while(true) {
        if (campfirePause_) {
            unique_lock<mutex> lock(cv_m_);
            cv_.wait(lock);
            lock.unlock();
        }
        sendAdkMsg("led_start_pattern {pattern:15}");
        sendAdkMsg("led_start_pattern {pattern:98}");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        sendAdkMsg("led_start_pattern {pattern:99}");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        sendAdkMsg("led_start_pattern {pattern:98}");
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        while(!campfirePause_) {
            srand((unsigned int)time(0));
            Stime = (rand() % 3) + 2;
            sendAdkMsg("led_start_pattern {pattern:84}");
            for (int i = 0; i < (Stime * 100); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                if (campfirePause_)
                    break;
            }
            if (campfirePause_)
                break;
            Freq = Stime % 3;
            if (!Freq) {
                // Fast
                sendAdkMsg("led_start_pattern {pattern:105}");
                for (int i = 0; i < 30; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    if (campfirePause_)
                        break;
                }
                if (campfirePause_)
                    break;
            } else if (Freq == 1) {
                // Medium
                sendAdkMsg("led_start_pattern {pattern:105}");
                for (int i = 0; i < 30; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    if (campfirePause_)
                        break;
                }
                if (campfirePause_)
                    break;
                sendAdkMsg("led_start_pattern {pattern:98}");
                for (int i = 0; i < 30; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    if (campfirePause_)
                        break;
                }
                if (campfirePause_)
                    break;
            } else {
                // Slow
                //sendAdkMsg("led_start_pattern {pattern:103}");
                sendAdkMsg("led_start_pattern {pattern:101}");
                for (int i = 0; i < 30; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    if (campfirePause_)
                        break;
                }
                if (campfirePause_)
                    break;
                sendAdkMsg("led_start_pattern {pattern:98}");
                for (int i = 0; i < 30; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    if (campfirePause_)
                        break;
                }
                if (campfirePause_)
                    break;
                sendAdkMsg("led_start_pattern {pattern:101}");
                for (int i = 0; i < 30; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    if (campfirePause_)
                        break;
                }
                if (campfirePause_)
                    break;
                sendAdkMsg("led_start_pattern {pattern:98}");
                for (int i = 0; i < 30; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    if (campfirePause_)
                        break;
                }
                if (campfirePause_)
                    break;
                sendAdkMsg("led_start_pattern {pattern:105}");
                for (int i = 0; i < 30; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    if (campfirePause_)
                        break;
                }
                if (campfirePause_)
                    break;
                sendAdkMsg("led_start_pattern {pattern:98}");
                for (int i = 0; i < 30; ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    if (campfirePause_)
                        break;
                }
                if (campfirePause_)
                    break;
            }
        }
        sendAdkMsg("led_start_pattern {pattern:68}");
        sendAdkMsg("led_start_pattern {pattern:88}");
    }
}

