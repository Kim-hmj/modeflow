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
#include "ota.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <random>
#include <sys/time.h>
#include <fstream>

OTA::OTA():ModeFlow("actmanota")
{
    e2mMap("ota::raucStarted","Polling");
    e2mMap("ota::raucFinished","WantToPoll");

    e2aMap("ota::cycling",                    [this] {std::thread (&ModeFlow::suspendable_long_actions, this, [this]{return cycling();}).detach(); return (shared_ptr<string>)nullptr;});
    //e2aMap("ota::FullyDone",                  [this] {return make_shared<string>("ota::disableVehicleAP");});
    e2aMap("Thread::OTA:reconnectWifi", [this] {system("wpa_cli reconfigure"); return (shared_ptr<string>)nullptr;});

    e2aMap_re("devpwr::(?!recover).+2(Docked|On)" ,[this] {
        shared_ptr<string> pretS;

        if ((getCurrentMode().find("WantToPoll") != string::npos) && (!(if_time() || if_last_step() || if_need_to_retry()))) {
            pretS = setMode("Init");
        } else if ((getCurrentMode().find("Init") != string::npos) && (if_time() || if_last_step() || if_need_to_retry())) {
            pretS = setMode("WantToPoll");
        } else  if ((getCurrentMode().find("Polling") != string::npos) && if_polling_too_long()) {
            pretS = setMode("Init");
        }

        return pretS;
    });
    emecMap_re("actmanota::(?!recover).+2WantToPoll", "WantToPoll","actmanota::startChecking");
    emecMap_re("Thread::OTA:checkforOTA", "WantToPoll","actmanota::startChecking");
    em2aMap_re("actmanota::startChecking", "WantToPoll",[this] {
        shared_ptr<string> pretS;

        if (!(if_time() || if_last_step() || if_need_to_retry())) {
            return setMode("Init");
        }

        cycling_period = CYCLING_PERIOD_SHORT_SEC;

        if (!if_wlan_up()) {
            pretS  = make_shared<string>("ota::wantStartButWlanNotUp");
#ifdef RAUC_CONTROLS_WIFI
            sendAdkMsg("connectivity_wifi_enable{}");
#endif
        } else if (!if_wlan_connected()) {
            if (if_can_try_connect_now()) {
                attempts_connect_wifi = (attempts_connect_wifi >= CONNECT_ATTEMPTS_LIMIT) ? 1 : ++attempts_connect_wifi;
                gettimeofday(&request_to_vehicle, NULL);

                if ((if_have_credentials()) && (!if_already_sent_ap_request())) {
                    gettimeofday(&request_ap, NULL);
                    sendPayload(make_shared<string>("ota::wantStartButWlanNotConnected"));
                    std::thread(&ModeFlow::delay_thread, this, "OTA:reconnectWifi", 30, 0).detach();
                } else {
                    sendPayload(make_shared<string>("ota::wantStartButWlanNotConnected"));
                    sendPayload(make_shared<string>("ota::wantStartButWlanNotConnectedNoCredentials"));
                }
            }
        } else if (if_can_try_poll_now()) {
            std::thread (&ModeFlow::long_actions,this,[this] {return run_rauc();}).detach();
            cycling_period = CYCLING_PERIOD_STOP;
        } else {

        }
        sendTimerPayload(make_shared<string>("ota::cycling"));

        return pretS;
    });

    modeRecovery("Init");
}

int OTA::getRandValue(int from, int to)
{
    const int range_from	= from;
    const int range_to		= to;
    std::random_device									rand_dev;
    std::mt19937												generator(rand_dev());
    std::uniform_int_distribution<int>	distr(range_from, range_to);

    return distr(generator);
}

shared_ptr<string> OTA::run_rauc()
{
    sendPayload(make_shared<string>("ota::raucStarted"));
    gettimeofday(&start_polling, NULL);
    exec_cmd("rauc-hawkbit-updater -c /etc/rauc-hawkbit-updater/config.conf");
    sendPayload(make_shared<string>("ota::raucFinished"));

#ifdef RAUC_CONTROLS_WIFI
    sendAdkMsg("connectivity_wifi_disable{}");
#endif

    sendAdkMsg("ota::disableVehicleAP");

    return (shared_ptr<string>)nullptr;
}

bool OTA::if_need_stop()
{

    if( access("/data/stop_ota", 0 ) == 0 ) {
        return true;
    } else {
        return false;
    }
}

bool OTA::if_have_credentials()
{
    FILE *fp;
    char onboarded;

    fp = popen("adkcfg -f /data/adk.connectivity.states.db  list | grep connectivity.states.wifi_onboarded | awk -F '|' '{print $2}'", "r");
    if(fp != NULL) {
        fread(&onboarded, 1, 1, fp);
        pclose(fp);
    }

    if ('1' == onboarded) {
        return true;
    } else {
        return false;
    }
}

bool OTA::if_need_to_retry()
{
    string attempt;

    if( access("/persist/rauc-hawkbit-updater/temp/downloading", 0 ) == 0 ) {

        ifstream in("/persist/rauc-hawkbit-updater/temp/downloading");
        std::getline (in,attempt);
        std::getline (in,attempt);

        if (stoi(attempt) < DOWNLOAD_ATTEMPTS_LIMIT) {
            return true;
        }
    }
    return false;
}

bool OTA::if_last_step()
{

    string version_now, version_expected;

    if( access("/persist/rauc-hawkbit-updater/temp/inprogress", 0 ) == 0 ) {
        ifstream in1("/persist/rauc-hawkbit-updater/temp/inprogress");
        std::getline (in1,version_expected);

        ifstream in2("/etc/sw-version.conf");
        std::getline (in2,version_now);

        if (0 == version_now.compare(version_expected)) {
            return true;
        }
    }
    return false;
}

bool OTA::if_can_try_connect_now()
{

    struct timeval current_time;

    gettimeofday(&current_time, NULL);

    if (((current_time.tv_sec - request_to_vehicle.tv_sec) > REQUESTS_PAUSE_SEC) || (attempts_connect_wifi < CONNECT_ATTEMPTS_LIMIT)) {
        return true;
    } else {
        return false;
    }
}

bool OTA::if_can_try_poll_now()
{
    FILE *fptr;
    int epochSec = 0;
    struct timeval current_time;

    gettimeofday(&current_time, NULL);

    fptr = fopen("/persist/rauc-hawkbit-updater/temp/lastCouldNotConnectToBE", "r");
    if (NULL != fptr) {
        char str[16];
        fscanf(fptr,"%s",str);
        epochSec = atoi(str);
        fclose(fptr);
    }

    if ((current_time.tv_sec - epochSec) > REQUESTS_PAUSE_SEC) {
        attempts_poll_be = 0;
        return true;
    } else if (attempts_poll_be < POLL_ATTEMPTS_LIMIT) {
        ++attempts_poll_be;
        return true;
    } else {
        return false;
    }
}

bool OTA::if_wlan_connected()
{
    FILE *fptr;
    int carrier = 0;

    fptr = fopen("/sys/class/net/wlan0/carrier", "r");
    if (NULL != fptr) {
        char str[8];
        fscanf(fptr,"%s",str);
        carrier = atoi(str);
        fclose(fptr);
    }

    return (1 == carrier);
}

bool OTA::if_wlan_up()
{
    if( access("/sys/class/net/wlan0", 0 ) == 0 ) {
        return true;
    } else {
        return false;
    }
}

bool OTA::if_time()
{
    FILE *fptr;
    int epochSec = 0;
    struct timeval current_time;

    gettimeofday(&current_time, NULL);

    fptr = fopen("/persist/rauc-hawkbit-updater/temp/lastCheck", "r");
    if (NULL != fptr) {
        char str[16];
        fscanf(fptr,"%s",str);
        epochSec = atoi(str);
        fclose(fptr);
    }

    if ((current_time.tv_sec - epochSec) > OTA_CHECK_INTERVAL_SEC) {
        return true;
    } else {
        return false;
    }
}

bool OTA::if_already_sent_ap_request()
{
    struct timeval current_time;

    gettimeofday(&current_time, NULL);

    if ((current_time.tv_sec - request_ap.tv_sec) < ((CYCLING_PERIOD_SHORT_SEC * 3)/2)) {
        return true;
    } else {
        return false;
    }
}

bool OTA::if_polling_too_long()
{
    struct timeval current_time;

    gettimeofday(&current_time, NULL);

    if ((current_time.tv_sec - start_polling.tv_sec) > (POLLING_TOO_LONG_SEC)) {
        return true;
    } else {
        return false;
    }
}

shared_ptr<string> OTA::cycling()
{
    shared_ptr<string> pretS;

    if (if_need_stop()) {
        pretS  = make_shared<string>("ota::cyclingManuallyStopped");
        return pretS;
    }

    if (0 != cycling_period) {
        std::thread(&ModeFlow::delay_thread, this, "OTA:checkforOTA", cycling_period, 0).detach();
    }
    return pretS;
}

