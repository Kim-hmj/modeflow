/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#include <string>
#include <iostream>
#include "external.h"
#include "msg_proc_thread.h"
#include "modeflow.h"
#include <syslog.h>
#include <time.h>
#include <sys/time.h>

extern SafeQueue<shared_ptr<string>> MsgQue;
int loglevel = MFLOG_DEBUG;

#ifdef X86_DEBUG
using namespace std;

void external_init(AdkMessageService *pms) {}
void externl_sendAdkMsg(std::string payload) {
    mflog(MFLOG_DEBUG,"sendAdkMsg:%s", payload.c_str());
}

void externl_sendAdkPayload(std::string payload) {
    mflog(MFLOG_DEBUG,"sendAdkPayload:%s", payload.c_str());
}

void externl_sendPayload(shared_ptr<string> ppayload) {
    MsgQue.enqueue(ppayload);
}

void system_wake_lock_toggle(bool x, const std::string &y, int z) {
    //std::cout << "wake lock toggle." << y << x << std::endl;
}
#else
#include "adk_adaptor.h"
static AdkMessageService *pmessage_service = nullptr;
const std::string kWakelockMsg = "adk.modeflow.msg";

void external_init(AdkMessageService *pms)
{
    pmessage_service = pms;
}

void externl_sendAdkMsg(std::string payload)
{
    system_wake_lock_toggle(true, kWakelockMsg, 0);

    AdkMessage send_message;
    google::protobuf::TextFormat::ParseFromString(payload, &send_message);

    if (send_message.IsInitialized())
    {
        string filename(__FILE__);
        //syslog(LOG_NOTICE,"%s:%s:%d",filename.substr(filename.find_last_of("/") == string::npos ? 0 : filename.find_last_of("/") + 1).c_str(), __func__, __LINE__);
        send_message.PrintDebugString();
        //cout.rdbuf(NULL);

        if (!pmessage_service->Send(send_message))
        {
            std::cerr << "Failed to send" << std::endl;
        }

        //cout.rdbuf(orig_buf);
    }
    else
    {
        std::cerr << "ERROR: Message not initialized" << std::endl;
    }

    system_wake_lock_toggle(false, kWakelockMsg, 0);

}

void externl_sendAdkPayload(std::string payload)
{
    system_wake_lock_toggle(true, kWakelockMsg, 0);

    AdkMessage send_message;
    auto out_message = send_message.mutable_system_mode_management();
    out_message->set_name(payload);

    if (send_message.IsInitialized())
    {
        if (!pmessage_service->Send(send_message))
        {
            std::cerr << "Failed to send" << std::endl;
        }

        //cout.rdbuf(orig_buf);
    }
    else
    {
        std::cout << "ERROR: Message not initialized" << std::endl;
    }

    system_wake_lock_toggle(false, kWakelockMsg, 0);
}

void externl_sendPayload(shared_ptr<string> ppayload)
{
    system_wake_lock_toggle(true, kWakelockMsg, 0);

    MsgQue.enqueue(ppayload);

    system_wake_lock_toggle(false, kWakelockMsg, 0);
}

void modeflow_wake_lock_toggle(bool x, const std::string &y, int z)
{
    std::cout << "wake lock toggle." << y << x << std::endl;
    system_wake_lock_toggle(x, y, z);
}

#endif


pthread_rwlock_t  e2sm_mutex = PTHREAD_RWLOCK_INITIALIZER;
pthread_rwlock_t  e2sm_mutex_alt = PTHREAD_RWLOCK_INITIALIZER;

int ProcessInputString(shared_ptr<string>      pevent, map<string, list<ModeFlow *>> &event2smList, list<ModeFlow *> &sm, ModeFlow *pmodeflow)
{
    int ret = 0;

#ifdef MULTI_THREAD
    list<ModeFlow *>::iterator it;
    list<ModeFlow *> *used_sm;
    string field;

    field = ModeFlow::getfield(pevent);
    if(field.empty()) return 0;
    pthread_rwlock_rdlock (&e2sm_mutex);
    if(event2smList.count(field) == 0) {

        event2smList[ModeFlow::getfield(pevent)] =  sm;
    }

    used_sm = &event2smList[ModeFlow::getfield(pevent)];
    pthread_rwlock_unlock(&e2sm_mutex);
#endif
#ifndef MFL_NDEBUG
    unsigned long long tmp_ms = get_ms();
#endif
    vector<shared_ptr<string>>* payloads = pmodeflow->EventSet(pevent);
#ifndef MFL_NDEBUG
    unsigned long long tmp_ms1 = get_ms();
    if(tmp_ms1 - tmp_ms > 10) {
        mflog(MFLOG_STATISTICS,"%09llu#_perf_ms#%s#%s", tmp_ms1 - tmp_ms, pmodeflow->getModuleName().c_str(),pevent->c_str());
    }
#endif
    if(payloads->size() != 0) {
        vector<shared_ptr<string>>::iterator itp;
        for(itp= payloads->begin(); itp != payloads->end(); itp++) {
            shared_ptr<string> ppayload = *itp;
            if(ppayload)
                externl_sendPayload(ppayload);
        }
    } else {
        string field = ModeFlow::getfield(pevent);
        if(!(pmodeflow->isregistered(field))) {
#ifdef MULTI_THREAD
            pthread_rwlock_rdlock(&e2sm_mutex_alt);
            used_sm = &ModeFlow::event2smList_alt[ModeFlow::getfield(pevent)];
            it = std::find(used_sm->begin(), used_sm->end(),pmodeflow);

            if(it != used_sm->end()) {
                cout << pmodeflow->getModuleName() << "start erasing..." << endl;
                pthread_rwlock_unlock(&e2sm_mutex_alt);
                pthread_rwlock_wrlock(&e2sm_mutex_alt);
                used_sm->erase(it);
                pthread_rwlock_unlock(&e2sm_mutex_alt);
                if(0 == pthread_rwlock_trywrlock(&e2sm_mutex)) {
                    pthread_rwlock_rdlock(&e2sm_mutex_alt);
                    event2smList = ModeFlow::event2smList_alt;
                    pthread_rwlock_unlock(&e2sm_mutex_alt);
                    pthread_rwlock_unlock(&e2sm_mutex);
                }
            } else {
                pthread_rwlock_unlock(&e2sm_mutex_alt);
            }
            cout << pmodeflow->getModuleName() << "finished erasing..." << endl;

#else
            ret = -1;// needs delet this element
#endif
        }
    }
    delete payloads;
    return ret;
}

void DispatchInputString(shared_ptr<string> pevent, map<string, list<ModeFlow *>> &event2smList, list<ModeFlow *> &sm)
{
    list<ModeFlow *>::iterator it;
    list<ModeFlow *> *used_sm;
    mflog(MFLOG_DEBUG,"%03lu:%s", get_ms(), pevent->c_str());
    string field = ModeFlow::getfield(pevent);
    list<ModeFlow *> dirty_sm;
    if(field.empty()) return;
#ifdef MULTI_THREAD
    pthread_rwlock_rdlock (&e2sm_mutex);
#endif
    if(event2smList.count(field) == 0) {

        event2smList[ModeFlow::getfield(pevent)] =  sm;
        ModeFlow::event2smList_alt[ModeFlow::getfield(pevent)] =  sm;
    }

    used_sm = &event2smList[ModeFlow::getfield(pevent)];
    for(it = used_sm->begin(); it != used_sm->end(); it++)
    {

        string filename(__FILE__);
#ifdef MULTI_THREAD
        if((*it)->hasThread()) {
            (*it)->pushevent(pevent);
        } else {
            (*it)->pushMFListQue(pevent);
            dirty_sm.push_back(*it);
        }
#else
        if(-1 == ProcessInputString(pevent, event2smList, ModeFlow::modeslist, *it)) {
            it = used_sm->erase(it);
            it--;
        }
#endif
    }
#ifdef MULTI_THREAD
    pthread_rwlock_unlock(&e2sm_mutex);
    for(it = dirty_sm.begin(); it != dirty_sm.end(); it++) {
        (*it)->resetMfListQueDirtyFlag();
    }

#endif
}

unsigned long get_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_nsec/1000000;
}

unsigned long get_us() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_nsec/1000;
}

