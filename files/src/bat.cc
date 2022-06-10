/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#include "bat.h"
#include "led.h"
PwrDlvr::PwrDlvr():ModeFlow("PwrDlvr")
{
    msgProc();

    if(i2c_open_tps65987() == 0) {
        modeRecovery("On");
    } else {
        modeRecovery("I2cOpenFail");
    }

    //Before start modeflow, message won't be receive, must wait for some time;
    std::thread(&ModeFlow::init_actions,this, [this] {return make_shared<string>("PwrDlvr::Init");}).detach();
}
void PwrDlvr::IfConnector()
{
    emecMap_re("devpwr::SysDisabled", "(?!I2cOpenFail|Off).+", "PwrDlvr::SwitchOff");

    emecMap_re("devpwr::SysEnabled", "(?!I2cOpenFail).+", "PwrDlvr::SwitchOn");
    emecMap_re("gpio::1275rising", "(?!I2cOpenFail).+", "PwrDlvr::SwitchOn");

    em2deMap_re("key::insert:press", "(?!I2cOpenFail).+", "PwrDlvr::detectPDEvents", 0, 500);
    em2deMap_re("PwrDlvr::Init", "(?!I2cOpenFail).+", "PwrDlvr::detectPDEvents", 0, 500);

//This is a prevention in case something wrong happes
    em2aMap_re("devpwr::StreamStarted", "Off", [this] {
        sysEn = true;
        return make_shared<string>("PwrDlvr::SwitchOn");
    });

    em2deMap_re("devpwr::StreamStarted", "Off", "PwrDlvr::detectPDEvents", 0, 500);
    em2deMap_re("gpio::1275rising", "Off", "PwrDlvr::checkStatus", 0, 500);//PD reset will have no interrupt events
    em2deMap_re("trigger::USB_DISCONNECTED", "ConnHostBC:.+", "PwrDlvr::detectPDEvents", 0, 500);
    em2mMap_re("trigger::USB_DISCONNECTED", "ConnHostBC:.+","DisConn");//For PC case, PD can't detect for fast usb out/in action
    em2mMap_re("gpio::1275falling.+", "ConnHost.+","DisConn");//1275falling after sys disabled, this is a rundundant logic
    em2mMap_re("key::insert:press.+", "ConnHost.+","DisConn");//1275falling after sys disabled, this is a rundundant logic
}

void PwrDlvr::Input()
{
    em2aMap_re("PwrDlvr::SwitchOff", "(?!I2cOpenFail|Off).+", [this] {
        memset(&oldStatus,0, sizeof(oldStatus));
        sysEn = false;
        return setMode("Off");
    });

    em2aMap_re("PwrDlvr::SwitchOn", "(?!I2cOpenFail|On).+", [this] {
        sysEn = true;
        if(getCurrentMode().compare("Off") == 0) {//only set to On in Off mode
            return setMode("On");
        } else{
        }
        return (shared_ptr<string>)nullptr;
    });

//interface
    e2aMap_re("PwrDlvr::detectPDEvents", [this] {
        std::thread(&ModeFlow::long_actions, this, [this]{
            return detectPDEvents();
        }).detach();
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("PwrDlvr::checkStatus", [this] {
        std::thread(&ModeFlow::long_actions, this, [this]{
            shared_ptr<string> pretS = check_status();
            return pretS;
        }).detach();
        return (shared_ptr<string>)nullptr;
    });


}

void PwrDlvr::Internal()
{
}

void PwrDlvr::Output()
{
    e2aMap_re("PwrDlvr::(?!recover).+2ConnHostPD:.+",  [this] {
        string para = getEvent().substr(getEvent().find_last_of(":") + 1);
        std::thread(&ModeFlow::delay_thread, this, "Charger:configurePD:" + para, 0, 150).detach();
        return (shared_ptr<string>)nullptr;
    });


    e2aMap_re("PwrDlvr::(?!recover).+2ConnHostBC:.+",  [this] {
        string para = getEvent().substr(getEvent().find_last_of(":") + 1);
        std::thread(&ModeFlow::delay_thread, this, "Charger:configureHostBC:" + para, 0, 150).detach();
        return (shared_ptr<string>)nullptr;
    });
}

void PwrDlvr::IOConnector()
{
    emecMap_re("Bat::BatLimpCharging", "ConnHostBC:0.*", "devpwr::powerOff");
}

shared_ptr<string> PwrDlvr::detectPDEvents() {

    char buf[11];
	shared_ptr<string> pretS;

    if(0 == tps65987_get_Intevents(buf))
    {
        pretS = check_status();
        if(buf[0] & 0x02) {
            sendPayload(make_shared<string>("Charger::Reset"));
        }
    } else
    {
        mflog(MFLOG_CHARGE, "%s", "Error get intstatus");
    }

    if(0 == tps65987_clear_Intevents())
    {
        mflog(MFLOG_CHARGE, "%s", "Clearing PD interrupts.");
    } else {
        mflog(MFLOG_CHARGE, "%s", "Error clear intstatus");
    }

    if(pretS) {
        return pretS;
    } else {
        return make_shared<string>("Log::PwrDlvr:detectedPDEvents");
    }
}

shared_ptr<string> PwrDlvr::check_status() {
    s_TPS_status tpStatus;
	shared_ptr<string> pretS;
    if(0 == tps65987_get_Status(&tpStatus)) {
        //VBus status

        mflog(MFLOG_CHARGE, "tpStatus ConnState %d, oldPlus Status is %d", tpStatus.ConnState, oldStatus.ConnState);
        if(oldStatus.ConnState ^ tpStatus.ConnState) {
            //Host/Device/Connection Judge
            string tmpMode{};
            //tpStatus.ConnState	== 4:No connection, Ra detected (Ra but no Rd)
            if ((tpStatus.ConnState ^ oldStatus.ConnState) && (tpStatus.ConnState  == 0 || tpStatus.ConnState  == 4) )
            {
                //Disconnected
                //syslog(LOG_DEBUG, "set Mode: DisConn");
                //if(getCurrentMode().find("DisConn") == string::npos){
                pretS = setMode("DisConn");
                //}
            } else if(tpStatus.PortRole) {
                //USB device
                //if(getCurrentMode().find("ConnDevice") == string::npos){
                pretS = setMode("ConnDevice");
                //}
            } else {
                //usb Host
                //detect in another way as in below function.
                //if(getCurrentMode().find("ConnHost") == string::npos){
                tmpMode = string("ConnHost");
                //}
            }

            //Check PD or BC1.2
            if(tmpMode.find("ConnHost") != string::npos) {
                if(tpStatus.ConnState^oldStatus.ConnState) {
                    if(tpStatus.UsbHostPresent & 0x01) {
                        //Is a PD device
                        string newmode = string("ConnHostPD:") + std::to_string(tps65987_get_ActiveContractPDO());
                        pretS  = setMode(newmode);
                    } else {
                        int currentConfig = tps65987_get_TypeC_Current();
                        if(currentConfig < 3) {
                            string newmode = string("ConnHostBC:") + std::to_string(currentConfig);
                            pretS = setMode(newmode);
                        }
                    }
                } else {

                }
            }
        }
        oldStatus = tpStatus;
        startup = 1;
    } else {
        mflog(MFLOG_CHARGE, "%s", "Error get pdstatus");
    }
    return pretS;
}

Battery::Battery():ModeFlow("Bat")
{
    msgProc();


    if(i2c_open_fuelgauge() == 0) {
        modeRecovery("Init", sizeof(struct BatSt), 32, (void *)&mconst_BatSt, true);

    } else {
        modeRecovery("I2cOpenFail", sizeof(struct BatSt), 32, (void *)&mconst_BatSt, true);
    }
    pstBat = (struct BatSt*)getShmDataAddr();

    std::thread(&ModeFlow::init_actions,this, [this] {return make_shared<string>("Bat::Init");}).detach();

}

void Battery::IfConnector()
{
    emecMap_re("Charger::chargeCurrent:.+", "(?!I2cOpenFail).+", "Bat::detectBatInfo");
    emecMap_re("Bat::(OutCharge|Init)", "(?!I2cOpenFail).+", "Bat::detectBatInfo");
    emecMap_re("gpio::detectTimer", "(?!I2cOpenFail).+", "Bat::detectBatInfoTimer");

    emecMap_re("devpwr::Docked:checkBat", "(?!I2cOpenFail).+", "Bat::detectBatInfoTimer","devpwr::setTimertoCheckBat");

    emecMap_re("Charger::(?!recover).+2SetInput:.+mA", "Chargeable.*", "Bat::decideChageCurrent");

    emecMap_re("Charger::POGOInputCurrentSet", "Chargeable.*", "Bat::decideChageCurrent");

    ecMap_re("PwrDlvr::(?!recover).+2ConnDevice","Bat::OutCharge");

    ecMap_re("PwrDlvr::(?!recover).+2(DisConn|Off)","Bat::setUSBDisconnected");

    ecMap_re("PwrDlvr::(?!recover).+2ConnHost(BC|PD):.+", "Bat::indicateCharge");

    ecMap_re("gpio::115falling", "Bat::setDocked");

    ecMap_re("gpio::115rising","Bat::setUndocked");

    ecMap_re("Bat::FullyCharged", "Bat::checkOutCharge");

    emecMap_re("gpio::detectTimer", "(?!ChargeableCharging).+","Bat::checkLowBatTimer");

    emecMap_re("Charger::lowCapability", "ChargeableNotCharging", "ChargeableLowInput");

    ecMap_re("lantern::button_DP", "Bat::indicateBatlevel");

}
void Battery::Input()
{
    e2aMap_re("Bat::detectBatInfo.+", [this] {
        return detectBatInfo();
    });

    em2aMap_re("Bat::decideChageCurrent", "Chargeable.*", [this] {
        int chargeCurrent = decide_the_ChargeCurrent_v2(vol,tem);
        shared_ptr<string> pretS = make_shared<string>("Bat::Charge:" + std::to_string(chargeCurrent));
        return pretS;
    });

    e2aMap_re("Bat::OutCharge",[this] {
        pstBat->outcharge |= 0x01;
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("Bat::setUSBDisconnected",[this] {
        std::thread(&ModeFlow::delay_thread, this, "Bat:gotoInit", 0, 500).detach();
        pstBat->chargerPlugged &= ~0x01;
        pstBat->outcharge = 0x00;
        return make_shared<string>("Bat::led:33");//USB Led Off
    });

    e2aMap_re("Bat::indicateCharge", [this] {
        pstBat->chargerPlugged |= 0x01;
        if(pstBat->fullyCharged) {
            return make_shared<string>("Bat::led:" + std::to_string(Pattern_USBChargingComplete));
        } else if(getCurrentMode().find("ChargeableCharging")!=string::npos) {
            return make_shared<string>("Bat::led:" + std::to_string(Pattern_USBChargingSlowPulse));
        } else{
            return make_shared<string>("Bat::led:" + std::to_string(Pattern_USBChargingFastPulse));
        }

        return make_shared<string>("Bat::USBHostPlugged");
    });

    e2aMap_re("Bat::setDocked", [this] {
        pstBat->chargerPlugged |= 0x02;
        return make_shared<string>("Bat::POGODocked");
    });

    e2aMap_re("Bat::setUndocked",[this] {
        pstBat->chargerPlugged &= ~0x02;
        return make_shared<string>("Bat::charger:POGOUnDocked");
    });


    e2aMap_re("Bat::checkOutCharge", [this] {
        if(pstBat->chargerPlugged || pstBat->outcharge) {
            return make_shared<string>("Bat::led:32");
        }
        return (shared_ptr<string>)nullptr;
    });

    em2aMap_re("Bat::checkLowBat.+", "(?!ChargeableCharging).+",[this] {
        //Check SOC for low battery warning
        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        //syslog(LOG_DEBUG, "seconds : %ld\nmicro seconds : %ld",
        //		 current_time.tv_sec, current_time.tv_usec);
        if(pstBat->last_time.tv_sec == 0) {
            pstBat->last_time = current_time;
        } else if(faultShutdown.lowSOC) {
            //simulate action key power off
            if(current_time.tv_sec - pstBat->last_time.tv_sec > 20) {
                sendPayload(make_shared<string>("Tone::sound:r1-PowerDown"));
                sendPayload(make_shared<string>("trigger::lowbattery_power_off"));
                pstBat->last_time = current_time;
            }
        } else if(faultStop.lowSOC) {
            if(current_time.tv_sec - pstBat->last_time.tv_sec > 120) {
                sendPayload(make_shared<string>("Tone::sound:r1-BatteryWarning"));
                pstBat->last_time = current_time;
            }
        } else if(faultWarning.lowSOC) {
            if(current_time.tv_sec - pstBat->last_time.tv_sec > 300) {
                sendPayload(make_shared<string>("Tone::sound:r1-BatteryWarning"));
                pstBat->last_time = current_time;
            }
        }

        return (shared_ptr<string>)nullptr;
    });


    em2mMap("Bat::setLowCharger", "ChargeableNotCharging", "ChargeableLowInput");

    e2aMap("Bat::indicateBatlevel", [this] {
        shared_ptr<string> pretS;
        if(SOC >= 0) {
            if(SOC >= 75)
            {
                pretS = make_shared<string>("Bat::led:34");
            } else if(SOC >= 35) {
                pretS = make_shared<string>("Bat::led:35");

            } else {
                pretS = make_shared<string>("Bat::led:36");
            }
        }
        return pretS;
    });



    e2aMap_re("Bat::setChargeLimit:.+", [this] {
        pstBat->charge_limit = std::stoi(getEvent().substr(getEvent().find_last_of(":") + 1));
        return (shared_ptr<string>)nullptr;
    });

    e2aMap("Bat::getChargeLimit", [this] {
        return make_shared<string>(string("Bat::ChargeLimit:") + std::to_string(pstBat->charge_limit));
    });

    //ported function, need to be compatible with old interface
    e2aMap_re("setval::FaultVal:.+", [this] {
        OVERHEAT2 = std::stoi(getEvent().substr(17,3));
        OVERHEAT1 = std::stoi(getEvent().substr(21,3));
        OVERCOOL2 = std::stoi(getEvent().substr(25,3));
        OVERCOOL1 = std::stoi(getEvent().substr(29,3));
        return (shared_ptr<string>)nullptr;
    });

    //ported function, need to be compatible with old interface
    e2aMap("getval::battery", [this] {
        sendAdkMsg("system_power_data {battery:\"" + std::to_string(current) + ":" + std::to_string(vol) + ":"
        + std::to_string(tem) + ":" + std::to_string(SOC) + "\"}");
        return (shared_ptr<string>)nullptr;
    });

}
void Battery::Internal()
{
    e2mMap("Thread::Bat:gotoInit","Init");
    e2aMap_re("Bat::(?!recover).+2NotChargeable", [this] {
        shared_ptr<string> pretS = make_shared<string>("Bat::StopCharge");
        return pretS;
    });

    em2aMap_re("Bat::(?!ChargeableCharging).+2ChargeableCharging", "",[this] {
        if(pstBat->last_time.tv_sec != 0) {
            pstBat->last_time.tv_sec == 0;
        }
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("Bat::(((?!recover).+2(NotChargable|ChargeableNotCharging|ChargeableLowInput))|StartOutCharging)", [this] {
        if(pstBat->chargerPlugged || pstBat->outcharge) {
            /*
              Workaround for PD unplug is not reported from PD at first time
              must delay longer to skip pd register change during out charging
            */
            std::thread(&ModeFlow::delayed_me, this, "PwrDlvr::detectPDEvents", ".+", 3, 0).detach();
            return make_shared<string>("Bat::led:31");//Disacharging
        }
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("Bat::(?!recover).+2ChargeableCharging", [this] {
        if(pstBat->chargerPlugged || pstBat->outcharge) {
            return make_shared<string>("Bat::led:30");//Charging
        }
        return (shared_ptr<string>)nullptr;
    });

}
void Battery::Output()
{
    e2aMap_re("Bat::Charge:.+",  [this] {
        string para = getEvent().substr(getEvent().find_last_of(":") + 1);
        std::thread(&ModeFlow::delay_thread, this, "Charger:configureCharge:" + para, 0, 150).detach();
        return (shared_ptr<string>)nullptr;
    });


    e2aMap_re("Bat::StopCharge",  [this] {
        std::thread(&ModeFlow::delay_thread, this, "Charger:stopCharge", 0, 150).detach();
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("Bat::StartOutCharging",  [this] {
        std::thread(&ModeFlow::delay_thread, this, "Charger:StartOutCharging", 0, 150).detach();
        return (shared_ptr<string>)nullptr;
    });

}

void Battery::IOConnector()
{
}

shared_ptr<string> Battery::detectBatInfo() {
#ifdef X86_DEBUG
    vol = 10000;
    current = 0;
    SOC = 1;
    tem = 23;
    maxcellvol = 0;
#else
    maxcellvol = fuelgauge_get_maxcell_Voltage();
    if(maxcellvol == -1) {
        return make_shared<string>("Bat::ConnectionErr");
    }
    vol = fuelgauge_get_Battery_Voltage();
    if(vol == -1)
    {
        return make_shared<string>("Bat::ConnectionErr");
    }

    current = fuelgauge_get_Battery_Current();
    if(current == 100000000)
    {
        return make_shared<string>("Bat::ConnectionErr");
    }
    SOC = fuelgauge_get_RelativeStateOfCharge();
    if(SOC == -1)
    {
        return make_shared<string>("Bat::ConnectionErr");
    }
    tem = fuelgauge_get_Battery_Temperature();
    if(tem == Temperature_UNVALID)
    {
        return make_shared<string>("Bat::ConnectionErr");
    }
#endif

    if(batteryTemperature_is_overstep_ChargeStopThreshold(tem) && current > 0) {
        faultStop.hightem = 1;//Neet to Stop Charge
    }

    if(batteryTemperature_is_in_ChargeAllowThreshold(tem) && current > 0) {
        faultStop.hightem = 0;
    }

    if(batteryTemperature_is_overstep_DischargeStopThreshold(tem) && current < 0) {
        faultShutdown.hightem = 1;//Need to shutdown
    } else {
        faultShutdown.hightem = 0;
    }

    if(SOC < 5) faultWarning.lowSOC = 1;
    else faultWarning.lowSOC = 0;

    if(SOC < 2) faultStop.lowSOC = 1;
    else faultStop.lowSOC = 0;

    if(SOC <= 1) faultShutdown.lowSOC = 1;
    else faultShutdown.lowSOC = 0;

    if(maxcellvol >=3650) faultStop.highvol = 1;
    else faultStop.highvol = 0;

    if(getCurrentMode().find("ChargeableCharging")!= string::npos) {
        if(SOC >= pstBat->charge_limit) {
            sendPayload(make_shared<string>("Bat::ChargeLimitReached"));
        }
    }
    {
        FILE *fp;
        char cmd[128];

        snprintf(cmd, 128, "echo %d > /dev/shm/bq-drv-r1-SOC", SOC);
        system(cmd);
    }

    int battery_status = fuelgauge_get_BatteryStatus();
    if(battery_status != -1) {
        if(battery_status & 0x0020)
        {
            if(getCurrentMode().find("ChargeableCharging") != string::npos ||
                    (pstBat->fullyCharged == false && pstBat->chargerPlugged) || getCurrentMode().find("Init") != string::npos) {
                sendPayload(make_shared<string>("Bat::FullyCharged"));
            }

            pstBat->fullyCharged = true;

            {
                FILE *fp;
                char cmd[128];

                snprintf(cmd, 128, "echo %d > /dev/shm/bq-drv-r1-SOC", 100);
                system(cmd);
            }
        } else {
            pstBat->fullyCharged = false;
        }
    } else {
        return make_shared<string>("Bat::ConnectionErr");
    }

    if(tem > OVERHEAT2 && pstBat->fault != Overheat2)
    {
        pstBat->fault = Overheat2;
        sendPayload(make_shared<string>("batfault::overheat2"));
    } else if(tem > OVERHEAT1 && pstBat->fault != Overheat1) {
        pstBat->fault = Overheat1;
        sendPayload(make_shared<string>("batfault::overheat1"));
    } else if(tem < OVERCOOL2 && pstBat->fault != Overcool2) {
        pstBat->fault = Overcool2;
        sendPayload(make_shared<string>("batfault::overcool2"));
    } else if(tem < OVERCOOL1 && pstBat->fault != Overcool1) {
        pstBat->fault = Overcool1;
        sendPayload(make_shared<string>("batfault::overcool1"));
    } else if(tem < OVERHEAT1 && tem > OVERCOOL1 && pstBat->fault != No_Fault) {
        pstBat->fault = No_Fault;
        sendPayload(make_shared<string>("batfault::nofault"));
    } else {
        mflog(MFLOG_CHARGE, "%s", "fault status not changed.\n");
    }


    if((pstBat->outcharge & 0x03) == 0x03 && (faultStop.hightem ||faultWarning.lowSOC)) {
        sendPayload(make_shared<string>("Bat::StopOutCharging"));
        pstBat->outcharge &= ~0x02;
    } else if((pstBat->outcharge & 0x03) == 0x01 && !faultStop.hightem && !faultWarning.lowSOC) {
        sendPayload(make_shared<string>("Bat::StartOutCharging"));
        pstBat->outcharge |= 0x02;
    }

    //Temprature and fullyCharged affect Chargeable
    if(faultStop.highvol || faultStop.hightem || pstBat->fullyCharged || SOC >= pstBat->charge_limit) {
        if(getCurrentMode().find("NotChargeable") == string::npos) {
            return setMode("NotChargeable");
        }
    } else {
        if(current > 0 && current < 100000000) {
            //0 ~ 10mA as threshold to debounce
            if(current < 10 && getCurrentMode().find("ChargeableLowInput") != string::npos) {
                return (shared_ptr<string>)nullptr;
            }
            if(getCurrentMode().find("ChargeableCharging") == string::npos) {
                if(faultShutdown.lowSOC && current < 100) {
                    sendPayload(make_shared<string>("Bat::BatLimpCharging"));
                }
                return setMode("ChargeableCharging");
            }
        } else if(current <= 0) { //0 ~ 10mA as threshold to debounce
            //Not Charging
            //This is abnormal, need to send each time found.
            if(pstBat->chargerPlugged) {
                if(bq25703a_get_ChargeCurrentSetting() == 0) {
                    return setMode("ChargeableNotCharging");
                } else {
                    if(getCurrentMode().find("ChargeableLowInput") == string::npos) {
                        if(faultShutdown.lowSOC) {
                            sendPayload(make_shared<string>("Bat::BatLimpCharging"));
                        }
                        return setMode("ChargeableLowInput");
                    }
                }

            } else {
                //Charge Not plugged
                if(getCurrentMode().find("ChargeableNoInput") == string::npos) {
                    return setMode("ChargeableNoInput");
                }
            }
        } else {
            mflog(MFLOG_CHARGE, "%s", "current error.\n");
        }
    }

    return (shared_ptr<string>)nullptr;
}

Charger::Charger():ModeFlow("Charger")
{
    msgProc();


    /*
            e2aMap_re("PwrDlvr::(?!recover).+2DisConn",[this] {

                if(0 == bq25703a_otg_function_init())
                {
                    return make_shared<string>("Charger::setOTG");
                } else{

                    return make_shared<string>("Charger::setOTGerr");
                }
                return (shared_ptr<string>)nullptr;
            });
    Disable to save power
    */


    if(i2c_open_bq25703() == 0) {
        modeRecovery("Off");
    } else {
        modeRecovery("I2cOpenFail");
    }
}

void Charger::IfConnector()
{
    e2deMap_re("Bat::POGODocked", "Charger::configurePOGO", 0, 150);

    emecMap_re("Bat::Charge:.+", "ToSettingSDPCurrent", "Charger::GetreadyToSetSDPCurrent");

    emecMap_re("PwrDlvr::(?!recover).+2DisConn", "OTG","Charger::DisableOTG");
    ecMap_re("PwrDlvr::(?!recover).+2DisConn", "Charger::SwitchOff");

    ecMap_re("Bat::StopOutCharging","Charger::DisableOTG");

    ecMap_re("Bat::(?!recover|NotChargeable).+2ChargeableNotCharging","Charger::recharge");
    ecMap_re("Bat::NotChargeable2ChargeableNotCharging","Bat::decideChageCurrent");
};
void Charger::Input()
{
    e2aMap("Charger::Reset", [this] {
        extern uint16_t USB_TYPEA_VALUE_BUF[];
        bq25703a_charge_function_init(USB_TYPEA_VALUE_BUF, USB_TYPEA_VALUE_BUF_size);
        return make_shared<string>("Charge::charger_init");
    });

    e2aMap_re("Charger::configurePOGO", [this] {
        std::thread(&ModeFlow::long_actions, this, [this]{
            input_current = 0x1e00;
            if(-1 != bq25703a_charge_function_init(CHARGE_REGISTER_Init_Without_Current, CHARGE_REGISTER_Init_Without_Current_size)
            && -1 != bq25703_set_InputCurrentLimit(input_current)) {
                //ensure relative registers are set
                return make_shared<string>("Charger::POGOInputCurrentSet");
            } else{
                return make_shared<string>("Charger::POGOInputCurrentSetErr");
            }
        }).detach();
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("Thread::Charger:configurePD:.+", [this] {
        input_current = std::stoi(getEvent().substr(getEvent().find_last_of(":") + 1));
        std::thread(&ModeFlow::long_actions, this, [this]{
            if(bq25703_init_ChargeOption_0() != 0)
            {
                return make_shared<string>("Charger::init_err");
            }

            if(bq25703_disable_OTG()) {
                return make_shared<string>("Charger::disotg_err");
            }

            bq25703a_charge_function_init(CHARGE_REGISTER_Init_Without_Current, CHARGE_REGISTER_Init_Without_Current_size);//ensure relative registers are set
            if( 0 == bq25703_set_InputCurrentLimit(input_current)) {
                string chargerMode = string();
                return this->setMode(chargerMode + "SetInput:" + std::to_string(( input_current >>8 )*50)+ "mA," + std::to_string(charge_current_set) + "mA");
                //return make_shared<string>(string("Charger::inputCurrent:") + std::to_string(( input_current >>8 )*50)+ "mA");
            }

            return make_shared<string>("Charger::set_err");
        }).detach();
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("Thread::Charger:configureHostBC:.+", [this] {
        string event = getEvent();
        std::thread(&ModeFlow::long_actions, this, [this, event]{
            return set_current_USBHostBC(event);
        }).detach();
        return (shared_ptr<string>)nullptr;
    });

    em2aMap_re("Charger::GetreadyToSetSDPCurrent", "ToSettingSDPCurrent", [this] {
        return setMode("SettingSDPCurrent");
    });

    em2aMap("Charger::setSDPCurrent", "SettingSDPCurrent", [this] {
        //Configure input current
        unsigned int VBus_vol = 0;
        unsigned int PSys_vol = 0;


        if(0 == bq25703a_get_PSYS_and_VBUS(&PSys_vol, &VBus_vol)) {
            if(VBus_vol >= 4900) {
                input_current += 0x0200 ;
                if(0 == bq25703_set_InputCurrentLimit(input_current)) {
                    if(input_current < 0x2a00)
                        return make_shared<string>(string("Charger::inputCurrent:") + std::to_string(( input_current >>8 )*50)+ "mA");
                }
            }
        }

        string chargerMode = string();
        return setMode(chargerMode + "SetInput:" + std::to_string(( input_current >>8 )*50)+ "mA," + std::to_string(charge_current_set) + "mA");
        //return make_shared<string>(string("Charger::inputCurrent:") + std::to_string(( input_current >>8 )*50)+ "mA");
    });

    em2aMap_re("Charger::DisableOTG", "OTG",[this] {
        //Configure input current
        bq25703_disable_OTG();
        return make_shared<string>("Log::Charger:OTG_Disabled");
    });

    e2aMap_re("Charger::SwitchOff", [this] {
        //Configure input current
        return setMode("Off");
    });

    e2aMap_re("Thread::Charger:configureCharge:.+", [this] {
        string event = getEvent();
        std::thread(&ModeFlow::long_actions, this, [this, event]{
            int chargeCurrent = std::stoi(event.substr(event.find_last_of(":") + 1));
            bq25703a_charge_function_init(CHARGE_REGISTER_Init_Without_Current, CHARGE_REGISTER_Init_Without_Current_size);//ensure relative registers are set
            if(0 == bq25703_set_InputCurrentLimit(input_current)) {
                if(bq25703_set_ChargeCurrent(chargeCurrent) == 0) {
                    charge_current_set = chargeCurrent;
                    shared_ptr<string> pretS = make_shared<string>("Charger::chargeCurrent:" + std::to_string(charge_current_set) + "mA");

                    if(getCurrentMode().find("SettingSDPCurrent") == string::npos) {
                        string chargerMode = string();
                        return this->setMode(chargerMode + "SetCharge:" + std::to_string(( input_current >>8 )*50)+ "mA,"+ std::to_string(charge_current_set) + "mA");
                    }
                    return pretS;
                }
                return make_shared<string>("Charger::set_err");
            }
            return (shared_ptr<string>)nullptr;
        }).detach();
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("Charger::recharge",[this] {
        bq25703a_charge_function_init(CHARGE_REGISTER_Init_Without_Current, CHARGE_REGISTER_Init_Without_Current_size);//ensure relative registers are set
        if(0 == bq25703_set_InputCurrentLimit(input_current)) {
            if(charge_current_set != bq25703a_get_ChargeCurrentSetting() || charge_current_set == 0) {
                bq25703_set_ChargeCurrent(charge_current_set);
                if(getCurrentMode().find("SettingSDPCurrent") == string::npos) {
                    string chargerMode = string();
                    return setMode(chargerMode + "SetCharge:" + std::to_string(( input_current >>8 )*50)+ "mA,"+ std::to_string(charge_current_set) + "mA");
                }
                return (shared_ptr<string>)nullptr;
            } else {
                return make_shared<string>("Charger::lowCapability");
            }
        }
        return make_shared<string>("Charger::set_err");
    });

    e2aMap_re("Thread::Charger:stopCharge", [this] {
        std::thread(&ModeFlow::long_actions, this, [this]{
            if(bq25703_set_ChargeCurrent(CHARGE_CURRENT_0) == 0) {

                shared_ptr<string> pretS = make_shared<string>("Charger::chargeCurrent:" + std::to_string(CHARGE_CURRENT_0)+ "mA");
                charge_current_set = CHARGE_CURRENT_0;
                string chargerMode = string();
                if(getCurrentMode().find("Off") == string::npos) {
                    return setMode(chargerMode + "SetCharge:" + std::to_string(( input_current >>8 )*50)+ "mA," + std::to_string(charge_current_set) + "mA");
                } else {
                    return make_shared<string>("Charger::" + chargerMode + "SetCharge:" + std::to_string(( input_current >>8 )*50)+ "mA," + std::to_string(charge_current_set) + "mA");
                }
            }

            return make_shared<string>("Charger::set_err");
        }).detach();
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("Thread::Charger:StartOutCharging", [this] {
        std::thread(&ModeFlow::long_actions, this, [this]{
            if(0 == bq25703a_otg_function_init())
            {

                string chargerMode = string();
                return setMode(chargerMode + "OTG");
            } else{

                return make_shared<string>("Charger::setOTGerr");
            }
        }).detach();
        return (shared_ptr<string>)nullptr;
    });

};

void Charger::Internal()
{
    e2dmeMap_re("Charger::.+2SettingSDPCurrent", "Charger::setSDPCurrent", "SettingSDPCurrent", 1, 0);
};

void Charger::Output()
{
};

void Charger::IOConnector()
{

};


shared_ptr<string> Charger::set_current_USBHostBC(string event) {
    TPS_TypeC_Current_Type inputCurrentLevel;
    inputCurrentLevel = TPS_TypeC_Current_Type (std::stoi(event.substr(event.find_last_of(":") + 1)));

    switch (inputCurrentLevel)
    {
    case USB_Default_Current:
        input_current = INPUT_CURRENT__USB_Default_Limit;
        break;
    case C_1d5A_Current:
        input_current = INPUT_CURRENT__USB_1d5A_Limit;
        break;
    case C_3A_Current:
        input_current = INPUT_CURRENT__USB_3A_Limit;
        break;
    default:
        input_current = INPUT_CURRENT__USB_Default_Limit;
        break;
    }
    /*
    	 if(bq25703_init_ChargeOption_0() != 0)
    	 {
    		 return make_shared<string>("Charger::init_err");
    	 }

    	if(bq25703_disable_OTG()) {
    		  return make_shared<string>("Charger::disotg_err");
    	}
    */

    bq25703a_charge_function_init(CHARGE_REGISTER_Init_Without_Current, CHARGE_REGISTER_Init_Without_Current_size);//ensure relative registers are set
    if( 0 == bq25703_set_InputCurrentLimit(input_current)) {
        string chargerMode = string();
        /*
        For safe reason, automatic current increase is commented
        		if(inputCurrentLevel == USB_Default_Current){

        			return setMode("ToSettingSDPCurrent");
        		}else{
        */
        return setMode(chargerMode + "SetInput:" + std::to_string(( input_current >>8 )*50)+ "mA," + std::to_string(charge_current_set) + "mA");
        /*
        For safe reason, automatic current increase is commented

        		}
        */
//            return make_shared<string>(string("Charger::inputCurrent:") + std::to_string(( input_current >>8 )*50)+ "mA");
    }

    return (shared_ptr<string>)nullptr;

}

