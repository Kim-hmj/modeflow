/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#include "amptemp.h"
AMPTemp::AMPTemp():ModeFlow("ampt")
{
    e2mMap_re("devpwr::(?!recover).+2(Docked|Standby)", "Inactive");
    em2mMap_re("devpwr::(?!recover).+2On", "Inactive", "Active");
    e2aMap("ampt::Inactive2Active", [this] {
        string filename(__FILE__);
        sendTimerPayload(make_shared<string>("ampt::checkAMPTTimer"));
        return (shared_ptr<string>)nullptr;
    });
    e2aMap_re("setval::AmpProtV:.+", [this] {return SetThresh(getEvent());});

    em2aMap("ampt::checkAMPTTimer", "Active",[this] {
        std::thread(&ModeFlow::suspendable_long_actions,this, [this]{return AmptDetect();}).detach();
        return (shared_ptr<string>)nullptr;
    });
    modeRecovery("Inactive");
};

shared_ptr<string> AMPTemp::AmptDetect()
{
    union U_Temp {
        short value;
        unsigned char buf[2];
    };

    struct S_AMPFault {
        union U_fault {
            char value;
            struct S_which {
                char amp1:1;
                char amp2:1;
            } which;
        } fault;
    };

    //periodic i2c register check
    int amp1_temp_fd = i2c_open("/dev/i2c-5", 0x48);//twitter woofer
    int amp2_temp_fd = i2c_open("/dev/i2c-5", 0x49);//full range
    union U_Temp temp1 = {0x0000};
    union U_Temp temp2 = {0x0000};
    struct S_AMPFault amp_fault= {.fault= {0}};

    if(0 == i2c_read(amp1_temp_fd, 0x48, 0x00, temp1.buf, 2))
    {
        if(temp1.value > 0 && (temp1.value >> 4) > temp_protect_thres && amp_fault.fault.which.amp1 == 1)
        {
            amp_fault.fault.which.amp1 = 1;
            sendPayload(make_shared<string>("ampfault::twprotect"));
        } else if(temp1.value < temp_unprotect_thres && amp_fault.fault.which.amp2 == 1)
        {
            amp_fault.fault.which.amp1 = 0;

            sendPayload(make_shared<string>("ampfault::twtgood"));
        }
    }

    if(0 == i2c_read(amp2_temp_fd, 0x49, 0x00, temp2.buf, 2))
    {
        if(temp2.value > 0 && (temp2.value >> 4) > temp_protect_thres && amp_fault.fault.which.amp2 == 1)
        {
            amp_fault.fault.which.amp2 == 1;
            sendPayload(make_shared<string>("ampfault::lrprotect"));
        } else if(temp2.value < temp_unprotect_thres && amp_fault.fault.which.amp2 == 1)
        {
            amp_fault.fault.which.amp2 = 0;
            sendPayload(make_shared<string>("ampfault::lrtgood"));

        }
    }

    close(amp1_temp_fd);
    close(amp2_temp_fd);

    std::thread(&ModeFlow::delayed_e, this, "ampt::checkAMPTTimer", 3, 0).detach();
    return (shared_ptr<string>)nullptr;
}

shared_ptr<string> AMPTemp::SetThresh(std::string event)
{
    std::stringstream ps;
    temp_protect_thres = std::stoi(event.substr(17,4));
    temp_unprotect_thres = std::stoi(event.substr(22,4));
    ps << "fault thresh changed to" << " " << __func__ << " " << temp_protect_thres << " " << temp_unprotect_thres;

    return make_shared<string>(ps.str());
}

