/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MODEFLOW_OTA_H
#define MODEFLOW_OTA_H
#include  "modeflow.h"
#include "external.h"
#include <string>

using adk::msg::AdkMessage;
using adk::msg::AdkMessageService;

#define TIME_RANDOMIZATION
//#define RAUC_CONTROLS_WIFI

#ifdef TIME_RANDOMIZATION
#define OTA_CHECK_INTERVAL_SEC      (60 * 60 * (24 + getRandValue(0, 5)))
#else
#define OTA_CHECK_INTERVAL_SEC      (60 * 60 * 24)
#endif

#define CYCLING_PERIOD_SHORT_SEC    (60 *  1)

#define CYCLING_PERIOD_STOP     (0)

#define DOWNLOAD_ATTEMPTS_LIMIT (3)

#define CONNECT_ATTEMPTS_LIMIT (6)

#define POLL_ATTEMPTS_LIMIT (6)

#define REQUESTS_PAUSE_SEC (60 * 60)

#define POLLING_TOO_LONG_SEC   (60 * 60 * 2)

class OTA:public ModeFlow
{
public:
    OTA();
    shared_ptr<string> cycling();

private:
    int  getRandValue(int from, int to);
    shared_ptr<string> run_rauc();
    bool if_need_stop();
    bool if_wlan_up();
    bool if_wlan_connected();
    bool if_time();
    bool if_last_step();
    bool if_need_to_retry();
    bool if_have_credentials();
    bool if_already_sent_ap_request();
    bool if_polling_too_long();
    bool if_can_try_connect_now();
    bool if_can_try_poll_now();
    int cycling_period = CYCLING_PERIOD_SHORT_SEC;
    uint8_t attempts_connect_wifi = 0;
    uint8_t attempts_poll_be = 0;
    struct timeval request_ap = (struct timeval) {
        0
    };
    struct timeval request_to_vehicle = (struct timeval) {
        0
    };
    struct timeval start_polling = (struct timeval) {
        0
    };
};
#endif

