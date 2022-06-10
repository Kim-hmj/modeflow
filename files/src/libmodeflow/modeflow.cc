/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Mode management for all modes management
 *
 */
#include "modeflow.h"
#include <fstream>
#include <string>
#include <memory>
#include <iostream>
#include <sys/mman.h>
#include <sys/file.h>
#include <fcntl.h>           /* For O_* constants */
#include <cassert>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "external.h"
#ifdef X86_DEBUG
#define DEVELOPER_NAME string("ryder")
#else
#define DEVELOPER_NAME string()
#endif

map<string, ModeFlow*> ModeFlow::modesmap;
list<ModeFlow *> ModeFlow::modeslist;
map<string, list<ModeFlow *>> ModeFlow::event2smList;
map<string, list<ModeFlow *>> ModeFlow::event2smList_alt;

#ifdef X86_DEBUG
std::unique_ptr<adk::config::Config> ModeFlow::config_ = adk::config::Config::Create("../src/adk.modeflow.db");
#else
std::unique_ptr<adk::config::Config> ModeFlow::config_ = adk::config::Config::Create(CONFIG_FILENAME);
#endif
uint32_t ModeFlow::thread_count = 0;
pthread_mutex_t ModeFlow::mtx = PTHREAD_MUTEX_INITIALIZER;

const std::string kWakelockThread = "adk.modeflow.thread";

std::mutex modeflow_condvar_mutex;
bool ready = false;
std::condition_variable modeflow_condvar;
//Application API
ModeFlow::ModeFlow(const std::string module_name):mModulename(module_name)
{
    ModeFlow::modesmap[module_name] = this;
    modeslist.push_back(this);
#ifndef MFL_NDEBUG
    e2aMap_re(module_name + "::setmode:.+", [this] {
        string mode = getEvent().substr(getEvent().find("setmode:") + 8);
        return setMode(mode);
    });//used for debug to set mode

    e2aMap_re(module_name + "::getmode:.+", [this, module_name] {
        string targetmodule = getEvent().substr(getEvent().find("getmode:") + 8);
        return make_shared<string>(targetmodule + "::" + module_name + ":" + current_mode);
    });//used for debug to set mode

    e2aMap_re(module_name + "::getcache", [this] {
        map<string,unsigned int>::iterator it;
        for(it = hashmap.begin(); it!=hashmap.end(); it++) {
            mflog(MFLOG_STATISTICS,"cache:%u\t%s", it->second,it->first.c_str());
        }
        return (shared_ptr<string>)nullptr;
    });//used for debug to set mode

    e2aMap_re(module_name + "::getfallthrough", [this] {
        map<string,unsigned int>::iterator it;
        for(it = hashmapfallthrough.begin(); it!=hashmapfallthrough.end(); it++) {
            mflog(MFLOG_STATISTICS,"cache:%u\t%s", it->second,it->first.c_str());
        }
        return (shared_ptr<string>)nullptr;
    });//used for debug to set mode

    e2aMap_re(module_name + "::getmodefallthrough", [this] {
        map<string,unsigned int>::iterator it;
        for(it = hashmapmodefallthrough.begin(); it!=hashmapmodefallthrough.end(); it++) {
            mflog(MFLOG_STATISTICS,"cache:%u\t%s", it->second,it->first.c_str());
        }
        return (shared_ptr<string>)nullptr;
    });//used for debug to set mode

    mflog(MFLOG_STATISTICS,"modeflow:%s constructed", module_name.c_str());
#endif
}

void ModeFlow::e2mMap(string in_event, string target_mode)
{
    assert(hasfield(in_event));
    assert(!hasregex(in_event));
    assert(!hasfield(target_mode));

    registerfields(in_event);
    registermodes(target_mode);

    event2mode[in_event] = target_mode;
}

void ModeFlow::em2eMap(string in_event, string based_mode, string out_event)
{
    assert(hasfield(in_event));
    assert(!hasregex(in_event + based_mode));
    registerfields(in_event);
    eventmode2event[in_event.append(":").append(based_mode)] = out_event;
}

void ModeFlow::em2mMap(string in_event, string based_mode, string target_mode)
{
    assert(hasfield(in_event));
    assert(!hasregex(in_event + based_mode));
    assert(!hasfield(target_mode));
    registerfields(in_event);
    registermodes(target_mode);

    event2mode[in_event.append(":").append(based_mode)] = target_mode;
}

void ModeFlow::e2aMap(string in_event,std::function<shared_ptr<string>()> func)
{
    assert(hasfield(in_event));
    assert(!hasregex(in_event ));
    registerfields(in_event);
    event2action[in_event] = func;
}

void ModeFlow::e2mMap_re(string in_event, string target_mode)
{
    assert(hasfield(in_event));
    assert(!hasfield(target_mode));

    registerfields(in_event);
    registermodes(target_mode);
    event2mode_regex.push_back(make_tuple(std::regex(in_event),target_mode, in_event));
}
void ModeFlow::em2mMap_re(string in_event, string based_mode, string target_mode)
{
    assert(hasfield(in_event));
    assert(!hasfield(target_mode));

    registerfields(in_event);
    registermodes(target_mode);
    string emstring = in_event.append(":").append(based_mode);
    event2mode_regex.push_back(make_tuple(std::regex(emstring),target_mode, emstring));
}

void ModeFlow::em2eMap_re(string in_event, string based_mode, string out_event)
{
    assert(hasfield(in_event));
    registerfields(in_event);

    string emstring = in_event.append(":").append(based_mode);
    eventmode2event_regex.push_back(make_tuple(std::regex(emstring),out_event, emstring));
}

void ModeFlow::e2aMap_re(string in_event, std::function<shared_ptr<string>()> func)
{
    assert(hasfield(in_event));
    registerfields(in_event);
    event2action_regex.push_back(make_tuple(std::regex(in_event),func, in_event));
}

void ModeFlow::e2taMap_re(string in_event, string ret, std::function<shared_ptr<string>()> func)
{
    assert(hasfield(in_event));
    registerfields(in_event);
    event2action_regex.push_back(make_tuple(std::regex(in_event),[=] {
        std::thread(&ModeFlow::long_actions, this, func).detach();
        return make_shared<string>(ret);
    }, in_event));
}

void ModeFlow::em2aMap(string in_event, string based_mode, std::function<shared_ptr<string>()> func)
{
    assert(hasfield(in_event));
    assert(!hasregex(in_event + based_mode));
    registerfields(in_event);
    eventmode2action[in_event.append(":").append(based_mode)] = func;
}

void ModeFlow::em2aMap_re(string in_event, string based_mode, std::function<shared_ptr<string>()> func)
{
    assert(hasfield(in_event));
    registerfields(in_event);
    string emstring = in_event.append(":").append(based_mode);
    eventmode2action_regex.push_back(make_tuple(std::regex(emstring),func,emstring));
}

void ModeFlow::em2taMap_re(string in_event, string based_mode, string ret, std::function<shared_ptr<string>()> func)
{
    assert(hasfield(in_event));
    registerfields(in_event);

    string emstring = in_event.append(":").append(based_mode);
    eventmode2action_regex.push_back(make_tuple(std::regex(emstring),[=] {
        std::thread(&ModeFlow::long_actions, this, func).detach();
        return make_shared<string>(ret);
    },emstring));
}

void ModeFlow::em2ta_susMap_re(string in_event, string based_mode, string ret, std::function<shared_ptr<string>()> func)
{
    assert(hasfield(in_event));
    registerfields(in_event);

    string emstring = in_event.append(":").append(based_mode);
    eventmode2action_regex.push_back(make_tuple(std::regex(emstring),[=] {
        std::thread(&ModeFlow::suspendable_long_actions, this, func).detach();
        return make_shared<string>(ret);
    },emstring));
}

void ModeFlow::e2deMap_re(string in_event, std::string out_event, uint64_t secs, uint64_t milisecs)//event to delayed event
{
    assert(hasfield(in_event));
    assert(hasfield(out_event));
    registerfields(in_event);
    event2action_regex.push_back(make_tuple(std::regex(in_event),[=] {
        std::thread(&ModeFlow::delayed_e, this, out_event, secs, milisecs).detach();
        return make_shared<string>("mfl::delay_e_set");
    },in_event));
}

void ModeFlow::em2deMap_re(string in_event, string based_mode, std::string out_event, uint64_t secs, uint64_t milisecs)//event mode to delayed event
{
    assert(hasfield(in_event));
    assert(hasfield(out_event));
    registerfields(in_event);

    string emstring = in_event.append(":").append(based_mode);
    eventmode2action_regex.push_back(make_tuple(std::regex(emstring),[=] {
        std::thread(&ModeFlow::delayed_e, this, out_event, secs, milisecs).detach();
        return make_shared<string>("mfl::delay_e_set");
    },emstring));
}

void ModeFlow::e2dmeMap_re(string in_event, std::string out_event, std::string regex_mode, uint64_t secs, uint64_t milisecs)//event to delayed event
{
    assert(hasfield(in_event));
    assert(hasfield(out_event));
    registerfields(in_event);

    event2action_regex.push_back(make_tuple(std::regex(in_event),[=] {
        std::thread(&ModeFlow::delayed_me, this, out_event, regex_mode,secs, milisecs).detach();
        return make_shared<string>("mfl::delay_me_set");
    },in_event));
}

void ModeFlow::em2dmeMap_re(string in_event, string based_mode, std::string out_event, std::string regex_mode, uint64_t secs, uint64_t milisecs)//event mode to delayed event
{
    assert(hasfield(in_event));
    assert(hasfield(out_event));
    registerfields(in_event);

    string emstring = in_event.append(":").append(based_mode);
    eventmode2action_regex.push_back(make_tuple(std::regex(emstring),[=] {
        std::thread(&ModeFlow::delayed_me, this, out_event, regex_mode,secs, milisecs).detach();
        return make_shared<string>("mfl::delay_me_set");
    },emstring));
}

string ModeFlow::getCurrentMode()
{
    return current_mode;
}

shared_ptr<string> ModeFlow::setMode(string mode)
{
    registermodes(mode);
    old_mode = current_mode;
    current_mode = mode;
    std::stringstream ps;

    assert(mode.size() < mShmDataOffset  - 1);
    snprintf((char*)getShmAddr(), mShmDataOffset, "%s\n", current_mode.c_str());

    //set history mode
    e2mMap("Thread::" + mModulename + ":history", old_mode);

    // output adk message
    ps << mModulename << "::" << getPreviousMode() << "2" <<  getCurrentMode();
    //end of mode change adk message
    return make_shared<string>(ps.str());
}

string ModeFlow::modeRecovery(const std::string &defaultMode, size_t datasize, unsigned char dataoffset, void* iniData, bool has_thread)
{
    /*do recovery*/
    string buffer;
    recoveryFile = "r1_" + mModulename;
    string absolutePath = "/dev/shm/"  + DEVELOPER_NAME + recoveryFile;
    if( access(absolutePath.c_str(), F_OK ) == 0 ) {
        // file exists
        mShmAddres = resumeShmAddr(datasize,dataoffset);
        std::ifstream fin(absolutePath.c_str());
        getline(fin, buffer);
        fin.close();
        assert(!buffer.empty());
        initMode(buffer);
        return string(mModulename) + "::recover2" + string(buffer);
    }

    // file doesn't exist, create it.
    mShmAddres = getShmAddr(datasize,dataoffset);
    assert(mShmAddres != nullptr);
    if(iniData != nullptr && datasize > 0) {
        memcpy((void*)((unsigned long)mShmAddres + getShmDataOffset()), iniData, datasize);
    }

    initMode(defaultMode);

    seperate_thread = has_thread;
    return string(mModulename) + string("::Init");
}

void ModeFlow::init_actions(std::function<shared_ptr<string>()> func) //shall run in a thread:std::thread (long_actions,func).detach();
{
    incThreadCounter();
	shared_ptr<string> pretS;

    std::unique_lock<std::mutex> lk(modeflow_condvar_mutex);
    modeflow_condvar.wait(lk, [] {return ready;});
	pretS = func();

	if(pretS)
    sendPayload(pretS);

    decThreadCounter();

}

void ModeFlow::sendPayload(shared_ptr<string> ppayload)
{
    if(ppayload)
    {
        mflog(MFLOG_DEBUG,"%03lu:%s:sndp:%s", get_ms(), getModuleName().c_str(), ppayload->c_str());
        externl_sendPayload(ppayload);
    }
}

void ModeFlow::sendTimerPayload(shared_ptr<string> ppayload)
{
    if(ppayload)
    {
        mflog(MFLOG_DEBUG,"%03lu:%s:sndtm:%s", get_ms(), getModuleName().c_str(), ppayload->c_str());
        externl_sendPayload(ppayload);
    }
}

void ModeFlow::sendAdkMsg(std::string payload)
{
    if(!payload.empty())
    {
        mflog(MFLOG_DEBUG,"%03lu:%s:sndadk:%s", get_ms(), getModuleName().c_str(), payload.c_str());
        externl_sendAdkMsg(payload);
    }
}

void ModeFlow::sendAdkPayload(std::string payload)
{
    if(!payload.empty())
    {
        mflog(MFLOG_DEBUG,"%03lu:%s:sndadk:%s", get_ms(), getModuleName().c_str(), payload.c_str());
        externl_sendAdkPayload(payload);
    }

}

void ModeFlow::delayed_e(std::string outString, uint64_t secs, uint64_t milisecs)
{
#ifdef X86_DEBUG
    mflog(MFLOG_DEBUG,"delay_e:%s,delay %lu secs %lu milisecs", outString.c_str(), secs, milisecs);
    return;
#endif

    std::this_thread::sleep_for (std::chrono::seconds(secs));
    std::this_thread::sleep_for (std::chrono::milliseconds(milisecs));

    incThreadCounter();
    sendTimerPayload(make_shared<string>(outString));

    decThreadCounter();
}

//secs is at least 35 bit, milisecs is at least 45 bits unsigned value
void ModeFlow::delayed_me(std::string outString, std::string regex_mode, uint64_t secs, uint64_t milisecs)
{
#ifdef X86_DEBUG
    mflog(MFLOG_DEBUG,"delayed_me:%s,delay %lu secs %lu milisecs", outString.c_str(), secs, milisecs);
    return;
#endif

    std::this_thread::sleep_for (std::chrono::seconds(secs));
    std::this_thread::sleep_for (std::chrono::milliseconds(milisecs));
    incThreadCounter();
    regex matchString = std::regex(regex_mode);
    string mode = getCurrentMode();
    if(std::regex_search(mode,matchString)) {
        sendTimerPayload(make_shared<string>(outString));
    }
    decThreadCounter();
}

int ModeFlow::exec_cmd(string cmd)
{
    mflog(MFLOG_DEBUG,"%03lu:%s:exec:%s", get_ms(),  getModuleName().c_str(),cmd.c_str());
#ifdef X86_DEBUG
    //cout << cmd << endl;
    return 1;
#endif
    return system(cmd.c_str());
}

int ModeFlow::isAppRunning(string app_name)
{
    mflog(MFLOG_DEBUG,"%03lu:%s:check %s is running?", get_ms(),  getModuleName().c_str(),app_name.c_str());

    char PIDGet[10] = {0};
    string PgrepStr = "pgrep " + app_name;

    FILE *fp = popen(PgrepStr.c_str(), "r");
    if (fp == NULL) {
        return 0;
    } else {
        fgets(PIDGet, sizeof(PIDGet), fp);
        pclose(fp);
        return atoi(PIDGet);
    }
}

void* ModeFlow::getShmDataAddr() {
    assert(getShmDataSize()!=0);//ensure this function is called after modeRecovery
    void *addr = getShmAddr();

    assert(addr != nullptr);
    return (void*)((unsigned long)addr + getShmDataOffset());
}

void ModeFlow::long_actions(std::function<shared_ptr<string>()> func) //shall run in a thread:std::thread (long_actions,func).detach();
{
    incThreadCounter();
	shared_ptr<string> pretS;

	if(pretS = func())sendPayload(pretS);

    decThreadCounter();
}
void ModeFlow::suspendable_long_actions(std::function<shared_ptr<string>()> func) //can suspend
{
	shared_ptr<string> pretS;
	if(pretS = func())sendPayload(pretS);
}

//Formework API
string ModeFlow::getPreviousMode()
{
    return old_mode;
}

void ModeFlow::initMode(string mode)
{
    current_mode = mode;
    old_mode = mode;
    // write to recovery file;

    assert(mode.size() < mShmDataOffset  - 1);

    snprintf((char*)getShmAddr(), mShmDataOffset, "%s\n", current_mode.c_str());

    registerfields(mModulename);
}

vector<shared_ptr<string>>* ModeFlow::EventSet(shared_ptr<string> pevent)
{
    string keystring = *pevent;
    string keymodestring = keystring;
    std::map<std::string, std::string>::iterator it;
    std::map<std::string, std::function<shared_ptr<string>()>>::iterator it_action;
    std::vector<tuple<regex,string,string>>::iterator it_regex;
    std::vector<tuple<regex,std::function<shared_ptr<string>()>,string>>::iterator it_action_regex;
    std::vector<tuple<regex,std::function<vector<shared_ptr<string>>()>,string>>::iterator it_evconverter_regex;
    vector<shared_ptr<string>> *retVs =  new vector<shared_ptr<string>>;
    std::stringstream ps;
    bool keyfound = false, keymodefound = false;
    mCurrentEvent = *pevent;
//check whether event is registered;
    string filed = getfield(pevent);
    assert(filed.empty() != true);

    if(m_fields.count(filed) == 0) {

        return retVs;
    }

    keymodestring.append(":");
    keymodestring.append(current_mode);

    it = event2mode.find(keystring);
    if (it != event2mode.end())
    {
        old_mode = current_mode;
        current_mode = it->second;
        keyfound = true;
        goto exit;
    }

    it = event2mode.find(keymodestring);
    if (it != event2mode.end())
    {
        old_mode = current_mode;
        current_mode = it->second;

        keymodefound = true;
        goto exit;
    }

    for (auto it_regex : event2mode_regex)
    {
        if(keystring[0] != std::get<2>(it_regex)[0]
                || keystring[1] != std::get<2>(it_regex)[1]
                || keystring[2] != std::get<2>(it_regex)[2]
          ) continue; //if field is different no need to check
#ifndef MFL_NDEBUG
        unsigned long tmp_us = get_us();
#endif
        if(std::regex_search(keystring,std::get<0>(it_regex)))
        {

#ifndef MFL_NDEBUG
            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSet_perf_us#%s#%s", tmp_us1 - tmp_us, keystring.c_str(), std::get<2>(it_regex).c_str());
#endif
            old_mode = current_mode;
            current_mode = std::get<1>(it_regex);
            keyfound = true;
            goto exit;
        }
#ifndef MFL_NDEBUG
        else {
            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSetNmatch_perf_us#%s#%s", tmp_us1 - tmp_us, keystring.c_str(), std::get<2>(it_regex).c_str());
        }
#endif
    }

    for (auto it_regex : event2mode_regex)
    {
        if(keystring[0] != std::get<2>(it_regex)[0]
                || keystring[1] != std::get<2>(it_regex)[1]
                || keystring[2] != std::get<2>(it_regex)[2]
          ) continue; //if field is different no need to check
#ifndef MFL_NDEBUG
        unsigned long tmp_us = get_us();
#endif
        if(std::regex_search(keymodestring,std::get<0>(it_regex)))
        {
#ifndef MFL_NDEBUG
            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSet_perf_us#%s#%s", tmp_us1 - tmp_us, keystring.c_str(), std::get<2>(it_regex).c_str());
#endif

            old_mode = current_mode;
            if(current_mode[0] == '_' && current_mode[1] == 'm') {
                string regmodstr = std::get<2>(it_regex);
                string regmodstr2 = regmodstr.substr(regmodstr.find_last_of(":") + 1);
                std::regex regmode(regmodstr2);
                current_mode = replacemode(regmode, std::get<1>(it_regex));
            } else {
                current_mode = std::get<1>(it_regex);
            }
            keymodefound = true;
            goto exit;
        }
#ifndef MFL_NDEBUG
        else {
            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSetNmatch_perf_us#%s#%s", tmp_us1 - tmp_us, keystring.c_str(), std::get<2>(it_regex).c_str());
        }
#endif
    }

exit:
    if(keyfound || keymodefound)
    {
        // write to recovery file;
        assert(current_mode.size() < mShmDataOffset  - 1);

        snprintf((char*)getShmAddr(), mShmDataOffset, "%s\n", current_mode.c_str());

        //set history mode
        e2mMap("Thread::" + mModulename + ":history", old_mode);

        // output adk message
        ps  << mModulename << "::" << getPreviousMode() << "2" <<  getCurrentMode();
        //end of mode change adk message
        shared_ptr<string> spretS = make_shared<string>(ps.str()) ;
        retVs->push_back(spretS);
    }
#ifndef MFL_NDEBUG
    if(keyfound) {
        unsigned int hashvalue = myhash(keystring.c_str());
        hashmap[keystring]=hashvalue;
    } else if(keymodefound) {
        unsigned int hashvalue = myhash(keymodestring.c_str());
        hashmap[keymodestring]=hashvalue;
    } else {
        if(isfallthrough(keystring)) {
            unsigned int hashvalue = myhash(keystring.c_str());
            hashmapfallthrough[keystring]=hashvalue;
        } else {
            unsigned int hashvalue = myhash(keystring.c_str());
            hashmapmodefallthrough[keymodestring]=hashvalue;
        }
    }
#endif
    keymodestring = keystring;
    keymodestring.append(":");
    keymodestring.append(current_mode);

#ifndef MFL_NDEBUG
    keyfound = false;
    keymodefound = false;
#endif
    it = eventmode2event.find(keymodestring);
    if (it != eventmode2event.end())
    {
        //output event adk message
        mEvent = it->second;
        ps = std::stringstream();
        ps << mModulename << "::" <<  getCurrentMode() << ":" <<  it->second;
        shared_ptr<string> spretS = make_shared<string>(ps.str()) ;
        retVs->push_back(spretS);
#ifndef MFL_NDEBUG
        keymodefound = true;
#endif
    }

    for (auto it_regex : eventmode2event_regex)
    {

        if(keystring[0] != std::get<2>(it_regex)[0]
                || keystring[1] != std::get<2>(it_regex)[1]
                || keystring[2] != std::get<2>(it_regex)[2]
          ) continue; //if field is different no need to check
#ifndef MFL_NDEBUG
        unsigned long tmp_us = get_us();
#endif
        if(std::regex_search(keymodestring,std::get<0>(it_regex)))
        {
#ifndef MFL_NDEBUG
            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSet_perf_us#%s#%s", tmp_us1 - tmp_us, keymodestring.c_str(), std::get<2>(it_regex).c_str());
#endif

            mEvent = std::get<1>(it_regex);
            ps = std::stringstream();
            ps << mModulename << "::" << getCurrentMode() << ":" <<  std::get<1>(it_regex);
            shared_ptr<string> spretS = make_shared<string>(ps.str()) ;
            retVs->push_back(spretS);
#ifndef MFL_NDEBUG
            keymodefound = true;
#endif
        }
#ifndef MFL_NDEBUG
        else {
            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSetNmatch_perf_us#%s#%s", tmp_us1 - tmp_us, keymodestring.c_str(), std::get<2>(it_regex).c_str());
        }
#endif
    }

    it_action = event2action.find(keystring);
    if (it_action != event2action.end())
    {
        mAction = it_action->second;
        shared_ptr<string> spretS = mAction();
        retVs->push_back(spretS);
#ifndef MFL_NDEBUG
        keyfound = true;
#endif

    }

    it_action = eventmode2action.find(keymodestring);
    if (it_action != eventmode2action.end())
    {
        mAction = it_action->second;
        shared_ptr<string> spretS = mAction() ;
        retVs->push_back(spretS);
#ifndef MFL_NDEBUG
        keymodefound = true;
#endif

    }

//For regular expression match, all actions will be done
    for (auto it_action_regex : event2action_regex)
    {

        if(keystring[0] != std::get<2>(it_action_regex)[0]
                || keystring[1] != std::get<2>(it_action_regex)[1]
                || keystring[2] != std::get<2>(it_action_regex)[2]
          ) continue; //if field is different no need to check
#ifndef MFL_NDEBUG
        unsigned long tmp_us = get_us();
#endif
        if(std::regex_search(keystring,std::get<0>(it_action_regex)))
        {

#ifndef MFL_NDEBUG
            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSet_perf_us#%s#%s", tmp_us1 - tmp_us, keystring.c_str(), std::get<2>(it_action_regex).c_str());
#endif
            mAction = std::get<1>(it_action_regex);
            shared_ptr<string> spretS = mAction() ;
            retVs->push_back(spretS);
#ifndef MFL_NDEBUG
            keyfound = true;
#endif

        }
#ifndef MFL_NDEBUG
        else {
            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSetNmatch_perf_us#%s#%s", tmp_us1 - tmp_us, keystring.c_str(), std::get<2>(it_action_regex).c_str());
        }
#endif
    }

    for (auto it_action_regex : eventmode2action_regex)
    {
        if(keystring[0] != std::get<2>(it_action_regex)[0]
                || keystring[1] != std::get<2>(it_action_regex)[1]
                || keystring[2] != std::get<2>(it_action_regex)[2]
          ) continue; //if field is different no need to check
#ifndef MFL_NDEBUG
        unsigned long tmp_us = get_us();
#endif
        if(std::regex_search(keymodestring,std::get<0>(it_action_regex)))
        {
#ifndef MFL_NDEBUG
            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSet_perf_us#%s#%s", tmp_us1 - tmp_us, keymodestring.c_str(), std::get<2>(it_action_regex).c_str());
#endif
            mAction = std::get<1>(it_action_regex);
            shared_ptr<string> spretS = mAction() ;
            retVs->push_back(spretS);
#ifndef MFL_NDEBUG
            keymodefound = true;
#endif

        }
#ifndef MFL_NDEBUG
        else {
            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSetNmatch_perf_us#%s#%s", tmp_us1 - tmp_us, keymodestring.c_str(), std::get<2>(it_action_regex).c_str());
        }
#endif

    }

    //For regular expression match, all actions will be done
    for (auto it_evconverter_regex : econverter_regex)
    {
        if(keystring[0] != std::get<2>(it_evconverter_regex)[0]
                || keystring[1] != std::get<2>(it_evconverter_regex)[1]
                || keystring[2] != std::get<2>(it_evconverter_regex)[2]
          ) continue; //if field is different no need to check
#ifndef MFL_NDEBUG
        unsigned long tmp_us = get_us();
#endif
        if(std::regex_search(keystring,std::get<0>(it_evconverter_regex)))
        {
#ifndef MFL_NDEBUG
            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSet_perf_us#%s#%s", tmp_us1 - tmp_us, keystring.c_str(), std::get<2>(it_evconverter_regex).c_str());
#endif
            mecAction = std::get<1>(it_evconverter_regex);
            vector<shared_ptr<string>> evts = mecAction();
            std::for_each(evts.begin(),evts.end(),
            [&retVs, keyfound](const shared_ptr<string> &pretS) mutable{
                shared_ptr<string> spretS = pretS ;
                retVs->push_back(spretS);
#ifndef MFL_NDEBUG
                keyfound = true;
#endif
            });
        }
#ifndef MFL_NDEBUG
        else {

            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSetNmatch_perf_us#%s#%s", tmp_us1 - tmp_us, keystring.c_str(), std::get<2>(it_evconverter_regex).c_str());
        }
#endif
    }

    for (auto it_evconverter_regex : emconverter_regex)
    {
        if(keystring[0] != std::get<2>(it_evconverter_regex)[0]
                || keystring[1] != std::get<2>(it_evconverter_regex)[1]
                || keystring[2] != std::get<2>(it_evconverter_regex)[2]
          ) continue; //if field is different no need to check
#ifndef MFL_NDEBUG
        unsigned long tmp_us = get_us();
#endif
        if(std::regex_search(keymodestring,std::get<0>(it_evconverter_regex)))
        {
#ifndef MFL_NDEBUG
            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSet_perf_us#%s#%s", tmp_us1 - tmp_us, keymodestring.c_str(), std::get<2>(it_evconverter_regex).c_str());
#endif
            mecAction = std::get<1>(it_evconverter_regex);
            vector<shared_ptr<string>> evts = mecAction();
            std::for_each(evts.begin(),evts.end(),
            [&retVs,keymodefound](const shared_ptr<string> &pretS) mutable{
                shared_ptr<string> spretS = pretS ;
                retVs->push_back(spretS);
#ifndef MFL_NDEBUG
                keymodefound = true;
#endif
            });
        }
#ifndef MFL_NDEBUG
        else {
            unsigned long tmp_us1 = get_us();
            mflog(MFLOG_STATISTICS,"%06lu#_eventSetNmatch_perf_us#%s#%s", tmp_us1 - tmp_us, keymodestring.c_str(), std::get<2>(it_evconverter_regex).c_str());
        }
#endif

    }
//cout << "before calling retF:"<< event << endl;
#ifndef MFL_NDEBUG
    if(keyfound) {
        unsigned int hashvalue = myhash(keystring.c_str());
        hashmap[keystring]=hashvalue;
    } else if(keymodefound) {
        unsigned int hashvalue = myhash(keymodestring.c_str());
        hashmap[keymodestring]=hashvalue;
    } else {
        if(isfallthrough(keystring)) {
            unsigned int hashvalue = myhash(keystring.c_str());
            hashmapfallthrough[keystring]=hashvalue;
        } else {
            unsigned int hashvalue = myhash(keystring.c_str());
            hashmapmodefallthrough[keymodestring]=hashvalue;
        }
    }
#endif
    return retVs;
}

bool ModeFlow::isregistered(string &field) {
    return m_fields.count(field);
}
bool ModeFlow::isfallthrough(string &event)
{
#ifndef MFL_NDEBUG
    for(auto it:modenumbermap)
    {
        if(checkmap(event, it.first)) {
            return false;
        }
    }
    return true;
#else
    return false;
#endif
}
size_t ModeFlow::registerfields(string event) {
    int startPos = 0, lastStartPos = 0;
    int startTag = 0, startTag1 = 0;
    size_t count = 0;
    while((startPos = event.find("::", lastStartPos)) != string::npos) {
        string sub = event.substr(lastStartPos, startPos - lastStartPos);
        //(((xxx::)|))
        startTag = sub.find_last_of("(");
        startTag1 = sub.find_last_of("|");
        startTag = max(startTag, startTag1); //string::npos == -1

        if(startTag == string::npos) {
            m_fields[sub] = true;
        } else {
            m_fields[sub.substr(startTag + 1)] = true;
        }

        count++;
        startPos += 2;
        lastStartPos = startPos;
    }
    return count;
}

size_t ModeFlow::registermodes(string mode) {

    assert(checkmultimode(mode));

    if(modenumbermap.find(mode) == modenumbermap.end())
    {
        modenumbermap[mode] = max_mode_nubmer;
        return ++max_mode_nubmer;
    } else {
        return max_mode_nubmer;
    }
}

string ModeFlow::getfield(shared_ptr<string> pevent) {
    int startPos = 0, lastStartPos = 0;
    int startTag = 0, startTag1 = 0;
    if((startPos = pevent->find("::", lastStartPos)) != string::npos) {
        string sub = pevent->substr(lastStartPos, startPos - lastStartPos);
        return sub;
    } else {
        return string();
    }
}

string ModeFlow::getfield(string &event){
    int startPos = 0, lastStartPos = 0;
    int startTag = 0, startTag1 = 0;
    if((startPos = event.find("::", lastStartPos)) != string::npos) {
        string sub = event.substr(lastStartPos, startPos - lastStartPos);
        return sub;
    } else {
        return string();
    }
}
/*
offset: the data offset,
size: the data size. the wholesize is offset + size;
return: ptr to the shm file.
*/
void* ModeFlow::getShmAddr(size_t datasize, unsigned char dataoffset) {
    int fd;
    size_t len;                 /* Size of shared memory object */
    void *addr;

    if(recoveryFile.empty()) {
        return nullptr;
    }

    if(mShmAddres != nullptr) {
//        assert(mShmDataOffset == offset);
//        assert(mShmDataSize == size);
        return mShmAddres;
    }

    mShmDataOffset = dataoffset;
    mShmDataSize = datasize;

//    string shm_dir = "/dev/shm/r1";
//    mkdir(shm_dir.c_str(), 0777);

    string shm_file = "/" + DEVELOPER_NAME + recoveryFile; /* works */
    //string shm_path = "/r1/" + recoveryFile;
    fd = shm_open(shm_file.c_str(), O_RDWR|O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1)
        return nullptr;

    len = mShmDataOffset + mShmDataSize;
    if (ftruncate(fd, len) == -1)           /* Resize object to hold string */
        return nullptr;

    /*FIXME: above: should use %zu here, and remove (long) cast */
    addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (addr == MAP_FAILED)
        return nullptr;

    if (close(fd) == -1)                    /* 'fd' is no longer needed */
        return nullptr;
    assert(addr != nullptr);
    return addr;
}

/*
offset: the data offset,
size: the data size. the wholesize is offset + size;
return: ptr to the shm file.
*/
void* ModeFlow::resumeShmAddr(size_t datasize, unsigned char dataoffset) {
    int fd;
    size_t len;                 /* Size of shared memory object */
    void *addr;

    if(recoveryFile.empty()) {
        return nullptr;
    }

    mShmDataOffset = dataoffset;
    mShmDataSize = datasize;

    string shm_file = "/" + DEVELOPER_NAME + recoveryFile; /* works */
    //string shm_path = "/r1/" + recoveryFile;
    fd = shm_open(shm_file.c_str(), O_RDWR|O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1)
        return nullptr;

    len = mShmDataOffset + mShmDataSize;

    /*FIXME: above: should use %zu here, and remove (long) cast */
    addr = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (addr == MAP_FAILED)
        return nullptr;

    if (close(fd) == -1)                    /* 'fd' is no longer needed */
        return nullptr;
    assert(addr != nullptr);
    return mShmAddres = addr;
}

string ModeFlow::getModuleMode(string moduleName)
{
    if(ModeFlow::modesmap.find(moduleName) != ModeFlow::modesmap.end())
    {
        return ModeFlow::modesmap.at(moduleName)->getCurrentMode();
    } else {
        syslog(LOG_ERR,"%s:%s", __func__, moduleName.c_str());
        return "";
    }
}

ModeFlow* ModeFlow::getModeModule(string moduleName)
{
    if(ModeFlow::modesmap.find(moduleName) != ModeFlow::modesmap.end())
    {
        return ModeFlow::modesmap.at(moduleName);
    } else {
        syslog(LOG_ERR,"%s:%s", __func__, moduleName.c_str());
        return nullptr;
    }
}

//secs is at least 35 bit, milisecs is at least 45 bits unsigned value
void ModeFlow::delay_thread(std::string outString, uint64_t secs, uint64_t milisecs)
{
#ifdef X86_DEBUG
    mflog(MFLOG_DEBUG,"delay_thread:%s,delay %lu secs %lu milisecs", outString.c_str(), secs, milisecs);
    return;
#endif

    std::this_thread::sleep_for (std::chrono::seconds(secs));
    std::this_thread::sleep_for (std::chrono::milliseconds(milisecs));

    incThreadCounter();
    sendTimerPayload(make_shared<string>("Thread::" + outString));

    decThreadCounter();
}

void ModeFlow::delay_thread_timer(std::string outString, uint64_t secs, uint64_t milisecs)
{
#ifdef X86_DEBUG
    mflog(MFLOG_DEBUG,"delay_thread_timer:%s,delay %lu secs %lu milisecs", outString.c_str(), secs, milisecs);
    return;
#endif
    std::this_thread::sleep_for (std::chrono::seconds(secs));
    std::this_thread::sleep_for (std::chrono::milliseconds(milisecs));

    incThreadCounter();

    sendTimerPayload(make_shared<string>("Timer::" + outString));

    decThreadCounter();
}

//secs is at least 35 bit, milisecs is at least 45 bits unsigned value
/*
void ModeFlow::delayed_action(std::function<shared_ptr<string>()> func, uint64_t secs, uint64_t milisecs, string mode_regex)
{
    std::this_thread::sleep_for (std::chrono::seconds(secs));
    std::this_thread::sleep_for (std::chrono::milliseconds(milisecs));

    if(++thread_count == 1) {
        system_wake_lock_toggle(true, kWakelockThread, 0);
    }

    if(mode_regex.empty() ==true || std::regex_search(getCurrentMode(),std::regex(mode_regex))) {
        externl_sendPayload(make_shared<string>(func())));
    }

    if(--thread_count == 0) {
        system_wake_lock_toggle(false, kWakelockThread, 0);
    }
}
*/

void ModeFlow::incThreadCounter() {
    int s = 0;
    s = pthread_mutex_lock(&mtx);
    if (s != 0)
    {
    }

    if(++thread_count == 1) {
        system_wake_lock_toggle(true, kWakelockThread, 0);
    }
    s = pthread_mutex_unlock(&mtx);
    if (s != 0)
    {
    }
    return;
}

void ModeFlow::setMsgProc(map<string, list<ModeFlow *>> &event2smList)
{
    if(hasThread()) {
        pproc = new MsgProc(event2smList, ModeFlow::modeslist, mfQue,this);
    }
}

void ModeFlow::decThreadCounter() {
    int s = 0;
    s = pthread_mutex_lock(&mtx);
    if (s != 0)
    {
    }

    if(--thread_count == 0) {
        system_wake_lock_toggle(false, kWakelockThread, 0);
    }
    s = pthread_mutex_unlock(&mtx);
    if (s != 0)
    {
    }
    return;
}

string ModeFlow::replacemode(regex e, string target_mode)
{
    return std::regex_replace (current_mode, e,target_mode);
}

#ifndef MFL_NDEBUG
//check whether there is already a match for map
bool  ModeFlow::checkmap(string &event, string mode)
{
    string keystring = event;
    string keymodestring = keystring;
    std::map<std::string, std::string>::iterator it;
    std::map<std::string, std::function<shared_ptr<string>()>>::iterator it_action;
    std::vector<tuple<regex,string>>::iterator it_regex;
    std::vector<tuple<regex,std::function<shared_ptr<string>()>>>::iterator it_action_regex;
    vector<string> retVs;
    int ret = false;
    std::stringstream ps;
//check whether event is registered;
    string filed = getfield(event);

    if(m_fields.count(filed) == 0) {

        return true;
    }

    keymodestring.append(":");
    keymodestring.append(mode);

    it = event2mode.find(keystring);
    if (it != event2mode.end())
    {
        return true;
        goto exit;
    }

    it = event2mode.find(keymodestring);
    if (it != event2mode.end())
    {
        return true;
        goto exit;
    }

    for (auto it_regex : event2mode_regex)
    {
        if(std::regex_search(keystring,std::get<0>(it_regex)))
        {
            return true;
            goto exit;
        }
    }

    for (auto it_regex : event2mode_regex)
    {
        if(std::regex_search(keymodestring,std::get<0>(it_regex)))
        {
            return true;
            goto exit;
        }
    }

    it = eventmode2event.find(keymodestring);
    if (it != eventmode2event.end())
    {
        return true;
        goto exit;
    }

    for (auto it_regex : eventmode2event_regex)
    {
        if(std::regex_search(keymodestring,std::get<0>(it_regex)))
        {
            return true;
            goto exit;
        }
    }

    it_action = event2action.find(keystring);
    if (it_action != event2action.end())
    {
        return true;
        goto exit;
    }

    it_action = eventmode2action.find(keymodestring);
    if (it_action != eventmode2action.end())
    {
        return true;
        goto exit;
    }

    //For regular expression match, all actions will be done
    for (auto it_action_regex : event2action_regex)
    {
        if(std::regex_search(keystring,std::get<0>(it_action_regex)))
        {
            return true;
            goto exit;
        }
    }

    for (auto it_action_regex : eventmode2action_regex)
    {
        if(std::regex_search(keymodestring,std::get<0>(it_action_regex)))
        {
            return true;
            goto exit;
        }
    }

    //For regular expression match, all actions will be done
    for (auto it_evconverter_regex : econverter_regex)
    {
        if(std::regex_search(keystring,std::get<0>(it_evconverter_regex)))
        {
            return true;
            goto exit;
        }
    }

    for (auto it_evconverter_regex : emconverter_regex)
    {
        if(std::regex_search(keymodestring,std::get<0>(it_evconverter_regex)))
        {
            return true;
            goto exit;
        }
    }

exit:
    return ret;
}

bool ModeFlow::hasregex(string str)
{
    if (regex_match (str, regex(R"(.*[\.\+\(\*\?]+.*)"))) {
        return true;
    } else {
        return false;
    }
}

bool ModeFlow::hasfield(string str)//str shall start with a field name, only on filed name is supported
{
    if (regex_match (str, regex(R"(^[a-zA-Z|]+::.+)"))) {
        return true;
    } else {
        return false;
    }
}

bool ModeFlow::checkmultimode(string target_mode)
{
    if(target_mode[0] == '_' && target_mode[1] == 'm') {
        //multimode: _m#mode1#mode2#mode3
        if(target_mode[2] == '#') {
            return true;
        } else {
            return false;
        }
    } else {
        return true;
    }

}

#endif
