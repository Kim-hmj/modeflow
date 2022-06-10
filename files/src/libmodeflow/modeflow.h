/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Mode management core logic interface
 * Descrition:
 * It is a interface for mode management core logic. Specific mode management logic shall not use this interface.
 *
 */

#ifndef MODEFLOW_H
#define MODEFLOW_H
#include <map>
#include <regex>
#include <vector>
#include <list>
#include <tuple>
#include <functional>
#include <iostream>       // std::cout
#include <sstream>
#include <cstring>
#include <string>
#include <thread>         // std::thread, std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <condition_variable>
#include <mutex>
#include <sys/mman.h>
#include <fcntl.h>           /* For O_* constants */
#include <cassert>
#include <syslog.h>
#include <adk/config/config.h>
#include "msg_proc_thread.h"

using namespace std;
extern const std::string kWakelockThread;
class MsgProc;

class  ModeFlow
{
public:
//Application API
    ModeFlow(std::string module_name);
    void e2mMap(string in_event, string target_mode);
    void e2mMap_re(string in_event, string target_mode);

    void e2aMap(string in_event, std::function<shared_ptr<string>()> func);
    void e2aMap_re(string in_event, std::function<shared_ptr<string>()> func);
    void e2taMap_re(string in_event, string ret, std::function<shared_ptr<string>()> func);

    void em2mMap(string in_event, string based_mode, string target_mode);
    void em2mMap_re(string event, string based_mode, string target_mode);

    void em2eMap(string in_event, string based_mode, string out_event);
    void em2eMap_re(string in_event, string based_mode, string out_event);

    void em2aMap(string in_event, string based_mode, std::function<shared_ptr<string>()> func);
    void em2aMap_re(string in_event, string based_mode, std::function<shared_ptr<string>()> func);
    void em2taMap_re(string in_event, string based_mode, string ret, std::function<shared_ptr<string>()> func);
    void em2ta_susMap_re(string in_event, string based_mode, string ret, std::function<shared_ptr<string>()> func);
    void e2deMap_re(string in_event, std::string out_event, uint64_t secs, uint64_t milisecs);//event to delayed event
    void em2deMap_re(string in_event, string based_mode, std::string out_event, uint64_t secs, uint64_t milisecs);//event mode to delayed event
    void e2dmeMap_re(string in_event, std::string out_event, std::string regex_mode, uint64_t secs, uint64_t milisecs);//event to delayed mode event
    void em2dmeMap_re(string in_event, string based_mode, std::string out_event, std::string regex_mode, uint64_t secs, uint64_t milisecs);//event mode to delayed mode event
    template<typename... Args>
    void ecMap_re(string in_event, Args... args);
    template<typename... Args>
    void emecMap_re(string in_event, string mode, Args... args);
    string getCurrentMode();
    shared_ptr<string> setMode(string mode);
    string modeRecovery(const std::string &defaultMode  = "Off", size_t datasize = 0, unsigned char dataoffset = 32, void* iniData = nullptr, bool has_thread = true);
    std::string getModuleName() {
        return mModulename;
    };

    void init_actions(std::function<shared_ptr<string>()> func); //shall run in a thread:std::thread (long_actions,func).detach();
    void sendPayload(shared_ptr<string> ppayload);
    void sendTimerPayload(shared_ptr<string> ppayload);
    void sendAdkMsg(std::string payload);
    void sendAdkPayload(std::string payload);

    //Application API low level
    void delayed_e(std::string outString, uint64_t secs, uint64_t milisecs);
    void delayed_me(std::string outString, std::string regex_mode, uint64_t secs, uint64_t milisecs);
    int exec_cmd(string cmd);
    int isAppRunning(string app_name);
    void* getShmDataAddr();//offset: must be bigger than (modename len + 1) , size:data size
    void long_actions(std::function<shared_ptr<string>()> func); //shall run in a thread:std::thread (long_actions,func).detach();
    void suspendable_long_actions(std::function<shared_ptr<string>()> func); //can suspend

//Virtual functions that need to be implemented.
    virtual void IfConnector() {}; //external input signal converting interface signal. correspondent to each flow. Such connecting releation can be difined in Output or IfConnector.
    virtual void Input() {}; //interface
    virtual void Internal() {}; //internal processing
    virtual void Output() {}; //Connecting to external module's input signal. Such connecting releation can be difined in Output or IfConnector.
    virtual void IOConnector() {}; //Processing exteral signal and output external signals without internal state change;A simplfiled way ignored Input and Internal definition; focused on flow.
    void msgProc() {
        IfConnector();
        Input();
        Internal();
        Output();
        IOConnector();
    };
//framework API
    string getPreviousMode();
    void initMode(string mode);
    vector<shared_ptr<string>>* EventSet(shared_ptr<string> pevent); // return current mode;
    void* getShmAddr(size_t datasize = 0, unsigned char dataoffset = 32);
    void* resumeShmAddr(size_t datasize = 0, unsigned char dataoffset = 32);

    unsigned char getShmDataOffset() {
        return mShmDataOffset;
    };
    size_t getShmDataSize() {
        return mShmDataSize;
    };
    std::string getEvent() {
        return mCurrentEvent;
    };
    static string getfield(shared_ptr<string> pevent);
    static string getfield(string &event);
    bool isregistered(string &field);
    bool isfallthrough(string &event);
    string getRecoveryFile() {
        return recoveryFile;
    };

    string  replacemode(std::regex e, string target_mode);
#ifndef MFL_NDEBUG
    bool checkmap(string &event, string mode);
    bool hasregex(string str);
    bool hasfield(string str);
    bool checkmultimode(string target_mode);
#endif

    string getModuleMode(string moduleName);//obsolete
    ModeFlow* getModeModule(string moduleName);//obsolete
    static map<std::string, ModeFlow*> modesmap;
    static list<ModeFlow *> modeslist;
    static map<string, list<ModeFlow *>> event2smList;
    static map<string, list<ModeFlow *>> event2smList_alt;
    void delay_thread(std::string outString, uint64_t secs, uint64_t milisecs);

    void delay_thread_timer(std::string outString, uint64_t secs, uint64_t milisecs);
    void setMsgProc(map<string, list<ModeFlow *>> &event2smList);
    void pushevent(shared_ptr<string> pevent) {
        mfQue.enqueue(pevent);
        return;
    };
    bool hasThread() {
        return seperate_thread;
    }

    void pushMFListQue(shared_ptr<string> pevent) {
        if(mfListQue->isDirty()) {
            return;
        } else {
            mfListQue->setDirty();
            mfListQue->push_event(pevent);
        }
    }
    void setMFListQue(MFListQue* pque) {
        mfListQue = pque;
    }

    MFListQue* getMFListQue(MFListQue* pque) {
        return mfListQue;
    }

    void resetMfListQueDirtyFlag() {
        mfListQue->cleanDirty();
    }

#ifndef MFL_NDEBUG
    static constexpr unsigned int myhash(const char *s, int off = 0) {
        return !s[off] ? 5381 : (myhash(s, off + 1) * 33) ^ s[off];
    }
#endif
protected:
    static std::unique_ptr<adk::config::Config> config_;
    uint32_t getThreadCounter() {
        return thread_count;
    };
private:
    size_t registerfields(string event);
    size_t registermodes(string mode);
    map<string,string> event2mode, eventmode2event;
    map<string, std::function<shared_ptr<string>()>> event2action, eventmode2action;
    vector<tuple<regex,string, string>> event2mode_regex, eventmode2event_regex;
    vector<tuple<regex,std::function<shared_ptr<string>()>, string>> event2action_regex, eventmode2action_regex;
    vector<tuple<regex,std::function<vector<shared_ptr<string>>()>, string>> econverter_regex, emconverter_regex;//event converter
    string current_mode = "Off";
    string old_mode = "Off";
    string recoveryFile;
    string mEvent, mCurrentEvent;
    std::function<shared_ptr<string>()> mAction  = NULL;
    std::function<vector<shared_ptr<string>>()> mecAction  = NULL;
    const std::string mModulename;
    map<string, bool> m_fields;
    unsigned char mShmDataOffset = 32;//a relatively larger offset, the max mode string (length +1), it is used to define the data start point in shm file
    void* mShmAddres = nullptr;
    size_t mShmDataSize = 0;

    void incThreadCounter();
    void decThreadCounter();
    static uint32_t thread_count;
    static pthread_mutex_t mtx;
    MsgProc *pproc=nullptr;
    SafeQueue<shared_ptr<string>> mfQue;
    MFListQue* mfListQue=nullptr;
    map<string, uint8_t> modenumbermap;//give each mode a number
    uint8_t max_mode_nubmer = 0;
    bool seperate_thread = true; //indicate that this module runs a seperate thread in multithread configuration.
#ifndef MFL_NDEBUG
    map<string,unsigned int> hashmap;
    map<string,unsigned int> hashmapfallthrough;
    map<string,unsigned int> hashmapmodefallthrough;
#endif
};

template<typename... Args>
void ModeFlow::ecMap_re(string event, Args... args) {
    const string converted_event[] = {args...};
    assert(hasfield(event));
    registerfields(event);
    std::function<vector<shared_ptr<string>>()> func = [this, converted_event] {
        vector<shared_ptr<string>> retVs;
        for (auto it = std::begin(converted_event);
        it != std::end(converted_event);
        it++) {
            assert(hasfield(*it));
            retVs.push_back(make_shared<string>(static_cast<string>(*it)));
        }
        return retVs;
    };
    econverter_regex.push_back(make_tuple(std::regex(event),func,event));
}

template<typename... Args>
void ModeFlow::emecMap_re(string event, string mode, Args... args) {
    const string converted_event[] = {args...};
    assert(hasfield(event));
    registerfields(event);
    std::function<vector<shared_ptr<string>>()> func = [this, converted_event] {
        vector<shared_ptr<string>> retVs;
        for (auto it = std::begin(converted_event);
        it != std::end(converted_event);
        it++) {
            assert(hasfield(*it));
            retVs.push_back(make_shared<string>(static_cast<string>(*it)));
        }
        return retVs;
    };
    emconverter_regex.push_back(make_tuple(std::regex(event + ":" + mode),func, event + ":" + mode));
}
#else
#endif
