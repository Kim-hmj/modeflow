/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#include "actuators.h"
#include <syslog.h>

Actuators::Actuators():ModeFlow("actuators")
{
    led_actuators();
    audio_actuators();
    exec_actuators();
    internal_actuators();
    avs_actuators();
    e2aMap_re("actions::.+", [this] {
        string value = getEvent();
        if(value.find("actions::") != std::string::npos)
        {
            std::string cmd = value.substr(9, value.length()-9);
            if (!exec_cmd(cmd)) {
                mflog(MFLOG_CHARGE, "%s","actions execution err\n");
            }
        }
        return (shared_ptr<string>)nullptr;
    });
}

shared_ptr<string> Actuators::replace(string input, string match, string replace)//replace is a regex string
{
    regex e(match);
    return make_shared<string>(regex_replace(input,e,replace));
}
void Actuators::avs_actuators()
{
    e2aMap_re("lantern::RGB_AlexaSpeaking", [this] {
        string adkmsg = "led_start_pattern {pattern:" +
        std::to_string(Pattern_AlexaSpeaking) +"}";
        sendAdkMsg(adkmsg);
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("lantern::RGB_AlexaThinking", [this] {
        string adkmsg = "led_start_pattern {pattern:" +
        std::to_string(Pattern_AlexaThinking) +"}";
        sendAdkMsg(adkmsg);
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("lantern::RGB_AlexasListeningStart:.+", [this] {
        string angle = getEvent().substr(getEvent().find_last_of(":") + 1);
        string adkmsg = "led_indicate_direction_pattern {pattern:" +
        std::to_string(Pattern_AlexaListeningStart) + " direction :" + angle +"}";
        sendAdkMsg(adkmsg);
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("lantern::RGB_AlexaListeningEnd:.+", [this] {
        string angle = getEvent().substr(getEvent().find_last_of(":") + 1);
        string adkmsg = "led_indicate_direction_pattern {pattern:" +
        std::to_string(Pattern_AlexaListeningSpeakingEnd) + " direction :" + angle +"}";
        sendAdkMsg(adkmsg);
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("lantern::RGB_AlexaMicmute", [this] {
        string adkmsg = "led_start_pattern {pattern:" +
        std::to_string(Pattern_MicOnToOff) +"}";
        sendAdkMsg(adkmsg);
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("lantern::RGB_AlexaMicunmute", [this] {
        string adkmsg = "led_start_pattern {pattern:" +
        std::to_string(Pattern_MicOffToOn) +"}";
        sendAdkMsg(adkmsg);
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("lantern::RGB_AlexaAlarm", [this] {
        string adkmsg = "led_start_pattern {pattern:" +
        std::to_string(Pattern_TimerAlarmReminder) +"}";
        sendAdkMsg(adkmsg);
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("lantern::RGB_AlexaIncomingNotification", [this] {
        string adkmsg = "led_start_pattern {pattern:" +
        std::to_string(Pattern_IncomingNotification) +"}";
        sendAdkMsg(adkmsg);
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("lantern::RGB_AlexaStatusDoNotDisturb", [this] {
        string adkmsg = "led_start_pattern {pattern:" +
        std::to_string(Pattern_EnableDoNotDisturb) +"}";
        sendAdkMsg(adkmsg);
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("lantern::AlexaVolume:.+", [this] {
        string percent = getEvent().substr(getEvent().find_last_of(":") + 1);
        string adkmsg = "led_indicate_percent_pattern {pattern:" +
        std::to_string(Pattern_AlexaVolume) + " percent :" + percent +"}";
        sendAdkMsg(adkmsg);
        return (shared_ptr<string>)nullptr;
    });
}
void Actuators::led_actuators()
{

    e2aMap_re("lantern::(Off|Init|Campfire|Night|Classic|RGB.+)2Low", [this] {
        sendAdkMsg("led_start_pattern {pattern:0}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::(Low|Campfire|Night|Classic)2Medium", [this] {
        sendAdkMsg("led_start_pattern {pattern:1}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::(Medium|Campfire|Night|Classic)2High", [this] {
        sendAdkMsg("led_start_pattern {pattern:2}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::High2(Off|Init|RGB.+)", [this] {
        sendAdkMsg("led_start_pattern {pattern:3}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::(Off|Init|RGB.+)2High", [this] {
        sendAdkMsg("led_start_pattern {pattern:4}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::Low2(Off|Init|RGB.+)", [this] {
        sendAdkMsg("led_start_pattern {pattern:5}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::Medium2(Off|Init|RGB.+)", [this] {
        sendAdkMsg("led_start_pattern {pattern:6}");
        return (shared_ptr<string>)nullptr;
    });

    //devpwr changed Docked Mode, thus replace led::89 with Adkmsg(89)
    e2aMap_re("lantern::(Low|Medium|High|Campfire|Night|Classic)2Init", [this] {
        sendAdkMsg("led_start_pattern {pattern:89}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::(Off|Init|RGB.+)2Medium", [this] {
        sendAdkMsg("led_start_pattern {pattern:45}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::High2Init", [this] {
        sendAdkMsg("led_start_pattern {pattern:7}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::Low2High", [this] {
        sendAdkMsg("led_start_pattern {pattern:8}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::High2Low", [this] {
        sendAdkMsg("led_start_pattern {pattern:23}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::Medium2Low", [this] {
        sendAdkMsg("led_start_pattern {pattern:24}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::Standby2High", [this] {
        sendAdkMsg("led_start_pattern {pattern:39}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::High2Standby", [this] {
        sendAdkMsg("led_start_pattern {pattern:38}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::Medium2Standby", [this] {
        sendAdkMsg("led_start_pattern {pattern:40}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::Low2Standby", [this] {
        sendAdkMsg("led_start_pattern {pattern:41}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::Standby2(Init|Off)", [this] {
        sendAdkMsg("led_start_pattern {pattern:42}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::Standby2Low", [this] {
        sendAdkMsg("led_start_pattern {pattern:54}");
        return (shared_ptr<string>)nullptr;
    });

    ecMap_re("lantern::(?!recover).+2RGB_BtPairingRun","led::" + std::to_string(Pattern_LanternBluePulsing));
    ecMap_re("lantern::(?!recover).+2RGB_BtConnectedBlue", "led::" + std::to_string(Pattern_LanternBlue3secFadeoff));
    ecMap_re("lantern::(?!recover).+2RGB_WifiConnectedAmber", "led::" + std::to_string(Pattern_LanternAmber3secFadeoff));
    ecMap_re("lantern::RGB_BtPairingRun2Off", "led::" + std::to_string(Pattern_Lantern2Off));

    e2aMap_re("Test::ForceOffLeds", [this] {
        sendAdkMsg("led_start_pattern {pattern:16}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("Test::ForceOnLeds", [this] {
        sendAdkMsg("led_start_pattern {pattern:14}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("devpwr::(?!recover).+2Dock(ed|Start|Test)", [this] {
        sendAdkMsg("led_start_pattern {pattern:27}");
        sendAdkMsg("led_start_pattern {pattern:37}");
        sendAdkMsg("led_start_pattern {pattern:12}");
        sendAdkMsg("led_start_pattern {pattern:33}");
        sendAdkMsg("led_start_pattern {pattern:68}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("devpwr::TransPowerOn2DockStart", [this] {
        sendAdkMsg("led_start_pattern {pattern:15}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::(?!recover).+2RGB_err", [this] {
        sendAdkMsg("led_start_pattern {pattern:46}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("btapp::volIndexSync.+", [this] {
        sendAdkMsg("led_start_pattern {pattern:53}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("volume::setWifivol:.+", [this] {
        sendAdkMsg("led_start_pattern {pattern:53}");
        return (shared_ptr<string>)nullptr;
    });

	e2aMap_re("led::.+", [this] {
		if(getModuleMode("devpwr").find("Docked") == string::npos){
			ELEDPattern patternID = (ELEDPattern)std::stoi(getEvent().substr((getEvent().find_last_of(":")+1)));
            if (regex_match(getModuleMode("lantern"), regex(R"(Campfire|Night|Classic)"))) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
			ledPattern(patternID);
            ledStopSignal(patternID);
		}
		return (shared_ptr<string>)nullptr;
	});
}

void Actuators::audio_actuators()
{
    e2aMap_re("playpause::PlayPause:PlayPauseToggle", [this] {
        sendAdkMsg("audio_track_play_pause_toggle{}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("playpause::(?!recover).+2SkipNextTrack", [this] {
        sendAdkMsg("audio_track_next {}");
        return (shared_ptr<string>)nullptr;
    });
    //next next
    e2aMap_re("playpause::SkipNextTrack:SkipNext", [this] {
        sendAdkMsg("audio_track_next {}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("playpause::(?!recover).+2SkipPreviousTrack", [this] {
        sendAdkMsg("audio_track_previous {}");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("playpause::SkipPreviousTrack:SkipPrevious", [this] {
        sendAdkMsg("audio_track_previous {}");
        return (shared_ptr<string>)nullptr;
    });

    /*
        e2aMap_re("playerstate::On:pauseaudio", [this] {
            sendAdkMsg("audio_track_play_pause_toggle{}");
            return (shared_ptr<string>)nullptr;
        });
    */
    e2aMap_re("Thread::tone:.+", [this] {
        string toneName = getEvent().substr((getEvent().find_last_of(":")+1));
        sendAdkMsg("audio_prompt_play {type : \"tone\" name : \"" + toneName + "\" }");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("Tone::sound:.+", [this] {
        string toneName = getEvent().substr((getEvent().find_last_of(":")+1));
        if(getModuleMode("devpwr").find("Docked") == string::npos)
        {
            if(getModuleMode("amp").find("On") != string::npos)
            {
                sendAdkMsg("audio_prompt_play {type : \"tone\" name : \"" + toneName +"\"}");//In devpwr On mode
            } else if(getModuleMode("amp").find("Slept") != string::npos)
            {
                sendPayload(make_shared<string>("amp::PowerOn"));
                sendAdkMsg("audio_prompt_play {type : \"tone\" name : \"" + toneName +"\"}");//In devpwr On mode
            } else {
                //soundToneStandby
                sendPayload(make_shared<string>("amp::PowerOn"));
                std::thread (&ModeFlow::delay_thread, this, "tone:" + toneName,1,500).detach();//In other mode
            }
        }
        return (shared_ptr<string>)nullptr;
    });

}

void Actuators::internal_actuators()
{
    e2aMap_re("Test::ForceOffLeds", [this] {
        return make_shared<string>("devpwr::ForceOffLeds");
    });

    e2aMap_re("Test::ForceOnLeds", [this] {
        return make_shared<string>("devpwr::ForceOnLeds");
    });
}

void Actuators::exec_actuators()
{

    e2aMap_re("playpause::(?!recover).+2SkipNextTrack", [this] {
        exec_cmd("/bin/systemctl restart r1-playpause.timer &");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("playpause::SkipNextTrack:SkipNext", [this] {
        exec_cmd("/bin/systemctl restart r1-playpause.timer &");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("playpause::(?!recover).+2SkipPreviousTrack", [this] {
        exec_cmd("/bin/systemctl restart r1-playpause.timer &");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("playpause::SkipPreviousTrack:SkipPrevious", [this] {
        exec_cmd("/bin/systemctl restart r1-playpause.timer &");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("devpwr::On2Docked", [this] {
        exec_cmd("/etc/exitscripts/led-board-disable.sh &");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("devpwr::On2Docked", [this] {
        exec_cmd("/etc/exitscripts/led-board-disable.sh &");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("playerstate::(?!recover).+2Idle", [this] {
        exec_cmd("/bin/systemctl restart r1-standby.timer &");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("playerstate::Idle2.+", [this] {
        exec_cmd("/bin/systemctl stop r1-standby.timer &");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("trigger::factory_charge_complete", [this] {
        exec_cmd("/etc/factory-test/r1/set_shipping_mode.sh &");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::(?!recover).+2RGB_batlevel", [this] {
        exec_cmd("/bin/systemctl restart r1-displaybatlevel.timer &");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("lantern::(?!recover).+2RGB_err", [this] {
        exec_cmd("/bin/systemctl restart r1-displayerr.timer &");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("ota::(?!recover).+2Rebooting", [this] {
        exec_cmd("/etc/exitscripts/board-script/reboot.sh 0 otareboot&");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("faultm::amp_protect_failed", [this] {
        return make_shared<string>("actions::/etc/exitscripts/board-script/shutdown.sh 4 faultm::amp_protect_failed &");
    });

    e2aMap_re("trigger::lowbattery_power_off", [this] {
        exec_cmd("/etc/exitscripts/board-script/shutdown.sh 4 &");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("faultm::fault_happened", [this] {
        exec_cmd("echo 1 > /sys/class/gpio/gpio116/value");
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("faultm::fault_dispeared", [this] {
        exec_cmd("echo 0 > /sys/class/gpio/gpio116/value");
        return (shared_ptr<string>)nullptr;
    });

}
void Actuators::ledPattern(ELEDPattern patternID)
{
    sendAdkMsg("led_start_pattern {pattern:" + std::to_string(patternID) + "}");
}

void Actuators::ledStopSignal(ELEDPattern patternID)
{
    std::thread([this](ELEDPattern patternID) {
        bool success;
        int pattern_time;
        success = config_->Read(&pattern_time, "modeflow.pattern_time." + std::to_string((int)patternID));
        if (pattern_time && success) {
            std::this_thread::sleep_for(std::chrono::milliseconds(pattern_time));
            sendPayload(make_shared<string>("actuators::led_stop:"+std::to_string((int)patternID)));
        }
    }, patternID).detach();
}


