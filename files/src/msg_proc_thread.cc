/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#include "msg_proc_thread.h"
#include "external.h"
#include "modeflow.h"
SafeQueue<shared_ptr<string>> MsgQue;
bool app_terminate = false;

MsgProc::MsgProc(map<string, list<ModeFlow *>> &event2smList, list<ModeFlow *> &sm, SafeQueue<shared_ptr<string>> &que,ModeFlow *pmf):event2smList(event2smList),m_sm(sm),mmsgQue(que),pmodeflow(pmf)
{

    pthread_t thread_id;

    int ret=0 ,stacksize = 1024*128;

    pthread_attr_t attr;
    ret = pthread_attr_init(&attr);
    assert(-1 != ret);

    ret = pthread_attr_setstacksize(&attr, stacksize);
    pthread_attr_setguardsize(&attr, 0);
    assert(-1 != ret);

    ret = pthread_create (&thread_id, NULL, _thread_t<MsgProc,&MsgProc::ThreadProc>, this);
    pthread_detach(thread_id);
    assert(-1 != ret);

    ret = pthread_attr_destroy(&attr);
    assert(-1 != ret);
}

void MsgProc::ThreadProc()
{
    if(pmodeflow == nullptr) {
//		cout << "null_pmodeflow" << endl << flush;
        while(true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        return;
    }
//		cout << "msgproc: thread_proc:"<< pmodeflow->getModuleName() << endl  << flush;
    while(true)
    {
        shared_ptr<string> pevent = mmsgQue.dequeue();
        if(app_terminate) {
            break;
        }
//			cout << "processing:" << pmodeflow->getModuleName() <<":"<< event << endl << flush;
        ProcessInputString(pevent, event2smList, m_sm, pmodeflow);
    }
}

pthread_attr_t attr;
MainMsgProc::MainMsgProc(map<string, list<ModeFlow *>> &event2smList, list<ModeFlow *> &sm, SafeQueue<shared_ptr<string>> &que):event2smList(event2smList),m_sm(sm),mmsgQue(que)
{

    pthread_t thread_id;

    int ret=0 ,stacksize = 1024*128;


    assert(-1 != pthread_attr_init(&attr));

    ret = pthread_attr_setstacksize(&attr, stacksize);
    pthread_attr_setguardsize(&attr, 0);
    assert(-1 != ret);


    ret = pthread_create (&thread_id, &attr, _thread_t<MainMsgProc,&MainMsgProc::ThreadProc>, this);
    pthread_detach(thread_id);
    assert(-1 != ret);

    ret = pthread_attr_destroy(&attr);
    assert(-1 != ret);

}

void MainMsgProc::ThreadProc()
{
//    cout << "mainmsgproc: thread_proc:" << endl  << flush;

    while(true)
    {
        shared_ptr<string> pevent = mmsgQue.dequeue();
        if(app_terminate) {
            break;
        }
//        cout << "dispatching:" << event << endl << flush;
        DispatchInputString(pevent, event2smList, m_sm);
//        cout << "endofdispatching:" << event << endl << flush;
    }
}

extern pthread_rwlock_t  e2sm_mutex;
extern pthread_rwlock_t  e2sm_mutex_alt;
MFListQue::MFListQue(map<string, list<ModeFlow *>> &event2smList, list<ModeFlow *> &sm, map<string, list<ModeFlow *>> &event2smList_alt):event2smList(event2smList),m_sm(sm),event2smList_alt(event2smList_alt)
{

}

void MFListQue::StartThread()
{
    pthread_t thread_id;

    int ret=0 ,stacksize = 1024*128;

    pthread_attr_t attr;
    ret = pthread_attr_init(&attr);
    assert(-1 != ret);

    ret = pthread_attr_setstacksize(&attr, stacksize);
    pthread_attr_setguardsize(&attr, 0);
    assert(-1 != ret);

    ret = pthread_create (&thread_id, NULL, _thread_t<MFListQue,&MFListQue::ThreadProc>, this);
    pthread_detach(thread_id);
    assert(-1 != ret);

    ret = pthread_attr_destroy(&attr);
    assert(-1 != ret);
}


void MFListQue::ThreadProc()
{
    while(true)
    {
        shared_ptr<string> pevent = mQue.dequeue();
        if(app_terminate) {
            break;
        }
        list<ModeFlow *>::iterator it;
        list<ModeFlow *> *used_sm;
        pthread_rwlock_rdlock(&e2sm_mutex_alt);
        used_sm = &event2smList_alt[ModeFlow::getfield(pevent)];
        pthread_rwlock_unlock(&e2sm_mutex_alt);

        for(it = mflist.begin(); it != mflist.end(); it++)
        {
            if(-1 == ProcessInputString(pevent, event2smList, m_sm, *it)) {
                pthread_rwlock_rdlock(&e2sm_mutex_alt);
                it = std::find(used_sm->begin(), used_sm->end(),*it);
                if(it != used_sm->end()) {
                    pthread_rwlock_unlock(&e2sm_mutex_alt);
                    pthread_rwlock_wrlock(&e2sm_mutex_alt);
                    it = used_sm->erase(it);
                    pthread_rwlock_unlock(&e2sm_mutex_alt);
                    pthread_rwlock_rdlock(&e2sm_mutex_alt);
                    it--;
                }
                pthread_rwlock_unlock(&e2sm_mutex_alt);

                if(0 == pthread_rwlock_trywrlock(&e2sm_mutex)) {
                    pthread_rwlock_rdlock(&e2sm_mutex_alt);
                    event2smList = ModeFlow::event2smList_alt;
                    pthread_rwlock_unlock(&e2sm_mutex_alt);
                    pthread_rwlock_unlock(&e2sm_mutex);
                }
            }
        }
    }
}

void MFListQue::addmf(ModeFlow *mf)
{
    if(mf->hasThread()) {
        return;
    }

    mflist.push_back(mf);
    mf->setMFListQue(this);
}

