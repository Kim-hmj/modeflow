/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#include <vector>
#include <string>
#include <iostream>
#include <iostream>
#include <fstream>
#include <memory>
#include <algorithm>
#include <functional>
#include <sstream>
#include "lantern.h"
#include "playpause.h"
#include "fautM.h"
#include "ota.h"
#include "otamode.h"
#include "misc.h"
#include "avs.h"
#include "bt.h"
#include "bat.h"
#include "playerstate.h"
#include "devpower.h"
#include "amptemp.h"
#include "volume.h"
#include "actuators.h"
#include "indication.h"
#include "modeflow.h"
#include "unistd.h"
#include <readline/readline.h>
#include <readline/history.h>
#include <condition_variable>
#include <list>
#include <sys/stat.h>
#include "msg_proc_thread.h"
#include <syslog.h>

using namespace std;

extern std::mutex modeflow_condvar_mutex;
extern bool ready ;
extern std::condition_variable modeflow_condvar;
extern SafeQueue<shared_ptr<string>> MsgQue;
vector<string>    getvs()
{
    vector<string> vs;
    string s{"test1"};
    string s1{"test2"};
    vs.push_back(s);
    vs.push_back(s1);
    return vs;
}
MainMsgProc *pmainmsgproc;

#define MAX_QUE_NUM 2
unique_ptr<MFListQue> pmsgProcMfList[MAX_QUE_NUM];
int main(int argc, char** argv)
{
    list<ModeFlow *> sm;
    uint8_t current_que_num = 0;
    pmsgProcMfList[current_que_num] = make_unique<MFListQue>(ModeFlow::event2smList,ModeFlow::modeslist,ModeFlow::event2smList_alt);

    //Mode *pmodeManSys = new ModeSystem();
    //pmodeManSys); //Revise class name to use differrent state machine logic

    //Sensors
    //new GPIOD();
    new PwrDlvr();
    new Battery();
    new AMPTemp();

    //Controllers
    //Ealier Flow

    new Bluetooth(); //Revise class name to use differrent state machine logic
    new HFP(); //Revise class name to use differrent state machine logic
    new Source();

    new ModeOta(); //Revise class name to use differrent state machine logic
    new OTA();
    new FlowBLE();
    pmsgProcMfList[current_que_num]->addmf(new Misc()); //Revise class name to use differrent state machine logic
    new AVS();

    PlayerStates* pactManStreaming = new PlayerStates();
    new DevicePower(pactManStreaming);

    new PlayPause(); //Revise class name to use differrent state machine logic
    new Volume();
    new Indication();

    pmsgProcMfList[current_que_num]->addmf(new Lantern());//Revise class name to use differrent state machine logic
    pmsgProcMfList[current_que_num]->addmf(new Icon());
    pmsgProcMfList[current_que_num]->addmf(new ConnectIcon());
    current_que_num++;

    //Later Flow

    //Actuators
    new FaultM(); //Revise class name to use differrent state machine logic
    new AMP();
    new Charger();
    new Actuators(); //Actuators must be the final one so that

    {
        std::lock_guard<std::mutex> lk(modeflow_condvar_mutex);
        ready = true;
        //std::cout << "notifying..." << std::endl;
    }
    modeflow_condvar.notify_all();

    setlogmask (LOG_UPTO (LOG_NOTICE));

    openlog ("mfl", LOG_CONS | LOG_NDELAY, LOG_USER);

#ifdef MULTI_THREAD
    std::for_each(ModeFlow::modeslist.begin(),ModeFlow::modeslist.end(), [](ModeFlow * mf) {
        mf->setMsgProc(ModeFlow::event2smList);
    });
    for(int i = 0; i< current_que_num; i++) {
        pmsgProcMfList[i]->StartThread();
    }
#endif
    pmainmsgproc = new MainMsgProc(ModeFlow::event2smList, ModeFlow::modeslist, MsgQue);


    while(1) {
        char cevent[256];
        string value;

        std::getline(std::cin, value);
        if(value.empty()) continue;
        usleep(1000);
        mflog(MFLOG_DEBUG,"receivedAdkMsg:%s", value.c_str());
        MsgQue.enqueue(make_shared<string>(value));
    }
    return 0;
}


