/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */


#ifndef ADKADPTOR_H_
#define ADKADPTOR_H_
#include <string>
#include <vector>
#include <list>
#include "external.h"
#include "modeflow.h"
#include "msg_proc_thread.h"

using adk::msg::AdkMessageService;
using namespace std;
namespace adk {
namespace mode {
// Wakelock name
const std::string kWakelockMain = "adk.modeflow.main";

using adk::msg::AdkMessage;

class ADKAdaptor {
public:
    ADKAdaptor();
    bool Start();
    void Stop();
//    void SetDatabaseFile(std::string database_file);
    ~ADKAdaptor();

private:
    struct Private;
    std::unique_ptr<Private> self_;


    void HandleSystemMode(const AdkMessage&);
    void HandleSystemTimerEvent(const AdkMessage& message);
    void HandleButtonGroupMsg(const AdkMessage& message);
    void HandleAudioGroupMsg(const AdkMessage& message);
    void HandleConnectivityGroupMsg(const AdkMessage& message);
    void HandleVoiceUIGroupMsg(const AdkMessage& message);

    //wrapper for simple mode act man
    void enqueue(shared_ptr<string> pevent);
    list<ModeFlow *> sm;
    map<int, string> msg2str, msg2strconn, msg2strvoiceui;
    adk::msg::SubscriptionToken subscription_token;
    vector<adk::msg::SubscriptionToken> handler_id_list;
    MsgProc *pmsgproc;
    MainMsgProc *pmainmsgproc;
#define MAX_QUE_NUM 2
    unique_ptr<MFListQue> pmsgProcMfList[MAX_QUE_NUM];
    uint8_t current_que_num = 0;
};
}  // namespace Mode
}  // namespace adk
void ProcessInputString(std::string event, map<string, list<ModeFlow *>> &event2smList, list<ModeFlow *> &sm);

#endif  // MODE_FLOW_H_
