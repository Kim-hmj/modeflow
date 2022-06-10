/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MSG_PROC_H
#define MSG_PROC_H
#include <list>
#include <thread>
#include <iostream>
#include "que.h"
#include <map>
using namespace std;
class ModeFlow;
template <typename TYPE, void (TYPE::*_RunThread)() >
void* _thread_t(void* param)
{
    TYPE* This = (TYPE*)param;
    This->ThreadProc();
    return NULL;
}

class MsgProc
{
public:
    MsgProc(map<string, list<ModeFlow *>> &event2smList, list<ModeFlow *> &sm, SafeQueue<shared_ptr<string>> &que,ModeFlow *pmf);
    void ThreadProc();

protected:
    SafeQueue<shared_ptr<string>> &mmsgQue;
    map<string, list<ModeFlow *>> &event2smList;
    list<ModeFlow *> &m_sm;

private:
    ModeFlow *pmodeflow=nullptr;

};

class MainMsgProc
{
public:
    MainMsgProc(map<string, list<ModeFlow *>> &event2smList, list<ModeFlow *> &sm, SafeQueue<shared_ptr<string>> &que);
    void ThreadProc();

protected:
    SafeQueue<shared_ptr<string>> &mmsgQue;
    map<string, list<ModeFlow *>> &event2smList;
    list<ModeFlow *> &m_sm;

private:
    ModeFlow *pmodeflow=nullptr;

};

//In progress
class MFListQue
{
public:
    MFListQue(map<string, list<ModeFlow *>> &event2smList, list<ModeFlow *> &sm,map<string, list<ModeFlow *>> &event2smList_alt);
    void ThreadProc();
    void addmf(ModeFlow *mf);
    void StartThread();
    void setDirty() {
        dirty = true;
    }
    bool isDirty() {
        return dirty;
    }
    void cleanDirty() {
        dirty = false;
    }
    void push_event(shared_ptr<string> pevent) {
        mQue.enqueue(pevent);
    }
protected:
    SafeQueue<shared_ptr<string>> mQue;
    map<string, list<ModeFlow *>> &event2smList;
    map<string, list<ModeFlow *>> &event2smList_alt;
    list<ModeFlow *> &m_sm;

private:
    list<ModeFlow *> mflist;//mode flow list related to mQue;
    bool dirty = false;
};
#endif


