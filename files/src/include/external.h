/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef EXTERNAL_H
#define EXTERNAL_H
#include <list>
#include <map>
#include <string>
#include <syslog.h>
#include <memory>
using namespace std;

#ifdef X86_DEBUG
namespace adk {
namespace mode {
// Wakelock name
const std::string kWakelockMain = "adk.modeflow.main";

}
namespace msg {
typedef char* AdkMessage;
typedef char* AdkMessageService;
}
}
void system_wake_lock_toggle(bool x, const std::string &y, int z);
#else
#include <adk/wakelock.h>
#include <google/protobuf/text_format.h>
#include "adk/message-service/adk-message-service.h"
#endif
using adk::msg::AdkMessage;
using adk::msg::AdkMessageService;
void external_init(AdkMessageService *pms);
void externl_sendAdkMsg(std::string payload);
void externl_sendAdkPayload(std::string payload);
void externl_sendPayload(shared_ptr<string> ppayload);
int i2c_open(const std::string &devpath, unsigned char addr);
int i2c_write(int fd_i2c, unsigned char dev_addr, void *val, unsigned char len);
int i2c_read(int fd_i2c, unsigned char addr, unsigned char reg, unsigned char *val, unsigned char len);
int i2c_readwrite(int fd, unsigned char addr, unsigned char *reg_w_list, unsigned char reg_w_len, unsigned char *val, unsigned char len);
void modeflow_wake_lock_toggle(bool x, const std::string &y, int z);
class ModeFlow;
int  ProcessInputString(shared_ptr<string>      pevent, map<string, list<ModeFlow *>> &event2smList, list<ModeFlow *> &sm, ModeFlow *pmodeflow);
void DispatchInputString(shared_ptr<string>      pevent, map<string, list<ModeFlow *>> &event2smList, list<ModeFlow *> &sm);
unsigned long get_ms();
unsigned long get_us();
#define MFLOG_RELEASE 0
#define MFLOG_DEBUG 1
#define MFLOG_CHARGE 2
#define MFLOG_STATISTICS 3


extern int loglevel;
template<typename... Args>
void mflog(int level, const char* fmt, Args... args) {
    if(level == loglevel) {
        syslog(LOG_NOTICE, fmt, args ...);
    } else {
        return;
    }
}

#else
#endif
