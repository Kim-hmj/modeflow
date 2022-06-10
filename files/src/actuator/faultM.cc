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
#include "fautM.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>

FaultM::FaultM():ModeFlow("faultm")
{
    e2aMap("batfault::overheat2" , [this] {return fault_man(Overheat2, true);}); //Enable
    e2aMap("batfault::overheat1" , [this] {return fault_man(Overheat1, true);}); //Enable
    e2aMap("batfault::overcool2" , [this] {return fault_man(Overcool2, true);}); //Enable
    e2aMap("batfault::overcool1" , [this] {return fault_man(Overcool1, true);}); //Enable
    e2aMap("batfault::nofault" , [this] {
        shared_ptr<string> pretS = fault_man(Overheat2, false);
        if(pretS->empty()) pretS = fault_man(Overheat1, false); else return pretS;
        if(pretS->empty()) pretS = fault_man(Overcool2, false); else return pretS;
        if(pretS->empty())  pretS = fault_man(Overcool1, false); else return pretS;
        return (shared_ptr<string>)nullptr;
    });

    e2aMap("ampfault::lrprotect" , [this] {return fault_man(AMPfrfault, true);}); //Enable
    e2aMap("ampfault::twprotect" , [this] {return fault_man(AMPtwfault, true);}); //Enable
    e2aMap_re("ampfault::twtgood.*" , [this] {return fault_man(AMPtwfault, false);}); //disable
    e2aMap_re("ampfault::lrtgood.*" , [this] {return fault_man(AMPfrfault, false);}); //disable

//connector to lantern
    ecMap_re("faultm::fault_happened", "lantern::fault");
}

shared_ptr<string> FaultM::protect_amp(bool sw) //true to mute, false to unmute
{
    static int fd1, fd2, fd3;
    static char path[] = "/dev/shm/ampEnablelock";
    static char path2[] = "/dev/shm/ampInitLock";
    static char path3[] = "/dev/shm/ampProtectLock";
	shared_ptr<string> pretS;
    if(sw == true) {
        pretS  = make_shared<string>("faultm::amp_protect_failed");
        fd1 = open(path, O_RDWR | O_CREAT,S_IRUSR | S_IWUSR);
        fd2 = open(path2, O_RDWR | O_CREAT,S_IRUSR | S_IWUSR);
        fd3 = open(path3, O_RDWR | O_CREAT,S_IRUSR | S_IWUSR);
        if (fd1 != -1) {
            if (flock(fd1, LOCK_EX) == 0) {
                if (fd2 != -1) {
                    if (flock(fd2, LOCK_EX) == 0) {
                        if (flock(fd3, LOCK_EX) == 0) {
                            std::cout << "Protect file was locked " << std::endl;
                            exec_cmd("/etc/exitscripts/board-script/disable-amplifier-without-lock.sh poweroff");
                            pretS = make_shared<string>("faultm::amp_protected");
                        } else {
                            std::cout << "ampInitLock not locked " << std::endl;
                        }
                    } else {
                        std::cout << "ampInitLock not locked " << std::endl;
                    }
                } else {
                    std::cout << "ampInitLock Cannot open file " << path << std::endl;
                }

            } else {
                std::cout << "ampEnablelock not locked " << std::endl;
            }
        } else {
            std::cout << "ampEnablelock Cannot open file " << path << std::endl;
        }

        if(fd2 != -1) {
            if (flock(fd2, LOCK_UN) == 0) {
                std::cout << "ampInitLock The file was unlocked. " << std::endl;
            } else {
                std::cout << "ampInitLock The file was no unlocked. " << std::endl;
            }
            close(fd2);
        }

        if(fd1 != -1) {
            if (flock(fd1, LOCK_UN) == 0) {
                string filename(__FILE__);
                //syslog(LOG_NOTICE,"%s:%s:%d,ampEnablelock The file was unlocked.",filename.substr(filename.find_last_of("/") == string::npos ? 0 : filename.find_last_of("/") + 1).c_str(), __func__, __LINE__);
            } else {
                string filename(__FILE__);
                mflog(MFLOG_RELEASE,"%s:%s:%d,ampEnablelock The file was not unlocked.",filename.substr(filename.find_last_of("/") == string::npos ? 0 : filename.find_last_of("/") + 1).c_str(), __func__, __LINE__);

            }
            close(fd1);
        }

    } else
    {
        pretS  = make_shared<string>("faultm::amp_unprotect_failed");
        int err = flock(fd3, LOCK_UN);
        if ( err == 0) {
            std::cout << "Protect file was unlocked " << std::endl;
            pretS = make_shared<string>("faultm::amp_unprotected");
        } else {
            string filename(__FILE__);
            mflog(MFLOG_RELEASE,"%s:%s:%d,Protect file unlocked failed",filename.substr(filename.find_last_of("/") == string::npos ? 0 : filename.find_last_of("/") + 1).c_str(), __func__, __LINE__);

            pretS  = make_shared<string>(std::string("faultm::amp_unprotect_failed") + std::string("errno:") + std::to_string(err));
        }
        close(fd3);
    }

    return pretS;
}

shared_ptr<string> FaultM::shutdown()
{
    exec_cmd("/etc/exitscripts/board-script/shutdown.sh 0 &");

    return (shared_ptr<string>)nullptr;
}

shared_ptr<string> FaultM::fault_man(Enum_PRODUCT_ERROR fault, bool status)
{
    static Enum_PRODUCT_ERROR fault_status = No_Fault;
    static bool fault_indi = false;
    static bool amp_fault_toggle = false;
    shared_ptr<string> pretS;
    if(status == true)
    {
        fault_status =(Enum_PRODUCT_ERROR) ((int)fault_status | (int)fault);

        if(((int)fault_status & (AMPfrfault | AMPtwfault)) && false == amp_fault_toggle)
        {
            amp_fault_toggle = true;
            std::thread (&ModeFlow::long_actions,this, [this] {return protect_amp(true);}).detach();
        }

        if((int)fault_status & (Overcool2 | Overheat2))
        {
            std::thread (&ModeFlow::long_actions,this,[this] {return shutdown();}).detach();
        }

        if(fault_indi == false)
        {
            fault_indi = true;
            pretS = make_shared<string>("faultm::fault_happened");
        }
    } else
    {
        fault_status = (Enum_PRODUCT_ERROR) ((int)fault_status & ~(int)fault);

        if(0 == ((int)fault_status & (AMPfrfault | AMPtwfault)) && true == amp_fault_toggle)
        {
            amp_fault_toggle = false;
            std::thread (&ModeFlow::long_actions, this, [this] {return protect_amp(false);}).detach();
        }

        if(fault_indi == true && fault_status == 0)
        {
            fault_indi = false;
            pretS =  make_shared<string>("faultm::fault_dispeared");
        }
    }

    return pretS;

}

