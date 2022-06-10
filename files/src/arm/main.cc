/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */



#include <adk/config/config.h>
#include <adk/log.h>
#include <unistd.h>
#include <csignal>
#include "adk_adaptor.h"
#include <systemd/sd-daemon.h>
#include <condition_variable>
#include <syslog.h>

#define DBUS_SESSION_FILE "/tmp/dbus-session"


extern std::mutex modeflow_condvar_mutex;
extern bool ready ;
extern std::condition_variable modeflow_condvar;


static void RemoveAllWakelocks() {
    system_wake_lock_toggle(false, adk::mode::kWakelockMain, 0);
    extern const std::string kWakelockThread;
    system_wake_lock_toggle(false, kWakelockThread, 0);
}

using adk::mode::ADKAdaptor;
void DispatchInputString(std::string event, map<string, list<ModeFlow *>> &event2smList, list<ModeFlow *> &sm);

int main(int argc, char* argv[]) {
    system_wake_lock_toggle(true, adk::mode::kWakelockMain, 0);

    ifstream dBusfile(DBUS_SESSION_FILE);
    string file_data;
    if (dBusfile.is_open())
    {
        while (getline(dBusfile, file_data))
        {
            string var = file_data.substr(0, file_data.find("="));
            string value = file_data.substr(file_data.find("=") + 1);
            const char *env = var.c_str();
            const char *set = value.c_str();
            setenv(env, set, 0);
        }
        dBusfile.close();
    }
    else
    {
        std::cerr << "Unable to open file - " << DBUS_SESSION_FILE << ", export DBUS_SESSION_BUS_ADDRESS manually" << endl;
    }
//    bool fflag = false;
//    std::string foptions;
//    int options_char;
    /*    while ((options_char = getopt(argc, argv, "f:")) != -1) {
            switch (options_char) {
                case 'f':
                    fflag = true;
                    foptions = std::string(optarg);
                    break;

                default:
                    break;
            }
        }
    */
    sigset_t signal_set;
    if (sigemptyset(&signal_set) < 0) {
        ADK_LOG_ERROR("Unable to create process signal mask");
        RemoveAllWakelocks();
        std::exit(EXIT_FAILURE);
    }
    if (sigaddset(&signal_set, SIGINT) < 0) {
        ADK_LOG_ERROR("Unable to add to process signal mask");
        RemoveAllWakelocks();
        std::exit(EXIT_FAILURE);
    }
    if (sigaddset(&signal_set, SIGTERM) < 0) {
        ADK_LOG_ERROR("Unable to add to process signal mask");
        RemoveAllWakelocks();
        std::exit(EXIT_FAILURE);
    }

    if (sigaddset(&signal_set, SIGPIPE) < 0) {
        ADK_LOG_ERROR("Unable to add to process signal mask");
        RemoveAllWakelocks();
        std::exit(EXIT_FAILURE);
    }

    if (sigprocmask(SIG_BLOCK, &signal_set, nullptr) < 0) {
        ADK_LOG_ERROR("ERROR: Unable to set process signal mask");
        RemoveAllWakelocks();
        std::exit(EXIT_FAILURE);
    }

    setlogmask (LOG_UPTO (LOG_NOTICE));

    openlog ("mfl", LOG_CONS | LOG_NDELAY, LOG_LOCAL0);

    // Create our application object
    ADKAdaptor adk_adaptor;
    /*    if (fflag) {
            adk_adaptor.SetDatabaseFile(foptions);
        }*/

    if (!adk_adaptor.Start()) {
        ADK_LOG_ERROR("Could not start");
        return 0;
    }

    sd_notifyf(0, "READY=1\n"
               "STATUS=Processing requests...\n"
               "MAINPID=%lu",
               (unsigned long) getpid());

    {   // this brace is neccesary for convar
        std::lock_guard<std::mutex> lk(modeflow_condvar_mutex);
        ready = true;
        //std::cout << "notifying..." << std::endl;
    }
    modeflow_condvar.notify_all();
    // Main loop waiting for signals
    bool waiting_for_signals = true;
    while (waiting_for_signals) {
        // Block until a signal arrives
        int signal_number;
        system_wake_lock_toggle(false, adk::mode::kWakelockMain, 0);

        int error = sigwait(&signal_set, &signal_number);
        extern bool app_terminate;
        app_terminate = true;
        system_wake_lock_toggle(true, adk::mode::kWakelockMain, 0);
        // Check there was no error
        if (error) {
            ADK_LOG_ERROR("%d while waiting for process signals", error);
            break;
        }

        // Check which signal it was
        switch (signal_number) {
        case SIGPIPE:
            break;
        case SIGINT:
        case SIGTERM:
            // Exit loop, terminate gracefully
            waiting_for_signals = false;
            break;

        default:
            ADK_LOG_ERROR("Received unexpected process signal");
            waiting_for_signals = false;
            break;
        }
    }

    adk_adaptor.Stop();
    RemoveAllWakelocks();
    return 0;
}
