/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef ACT_MAN_GPIOD_H
#define ACT_MAN_GPIOD_H
#include  "modeflow.h"
#include "external.h"
#include <string>
#include <unistd.h>


#include "flowdspsocket.h"

#include <fstream>
#include <iostream>
#include <cstdint>
#include <syslog.h>
#include "external.h"
using adk::msg::AdkMessage;
using adk::msg::AdkMessageService;

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <systemd/sd-daemon.h>
#include <pthread.h>
#include <string>
/****************************************************************
* Constants
****************************************************************/

#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define POLL_TIMEOUT (3 * 1000) /* 3 seconds */
#define MAX_BUF 64
#define INPUT_QUEUE "/dev/input/event1"
#define EVENT_LEN 48

class GPIOD:public ModeFlow
{
public:
    GPIOD();

    /****************************************************************
     * gpio_export
     ****************************************************************/
    int gpio_export(unsigned int gpio);

    /****************************************************************
     * gpio_unexport
     ****************************************************************/
    int gpio_unexport(unsigned int gpio);


    /****************************************************************
     * gpio_set_dir
     ****************************************************************/
    int gpio_set_dir(unsigned int gpio, unsigned int out_flag);

    /****************************************************************
     * gpio_set_value
     ****************************************************************/
    int gpio_set_value(unsigned int gpio, unsigned int value);

    /****************************************************************
     * gpio_get_value
     ****************************************************************/
    int gpio_get_value(unsigned int gpio, unsigned int *value);


    /****************************************************************
     * gpio_set_edge
     ****************************************************************/

    int gpio_set_edge(unsigned int gpio, char *edge);

    /****************************************************************
     * gpio_fd_open
     ****************************************************************/

    int gpio_fd_open(unsigned int gpio);


    /****************************************************************
     * gpio_fd_close
     ****************************************************************/

    int gpio_fd_close(int fd);


#define ARGC 9
    shared_ptr<string> initGPIO(int argc, string (&argv)[ARGC] ) ;

    shared_ptr<string> detectGPIO(int argc, string (&argv)[ARGC]);
    shared_ptr<string> detectKey();
    void Output();

private:
    int timeout, rc;
    char buf[MAX_BUF];
    char buf1[MAX_BUF];

    int len;
    bool startup = true;

    int argc = ARGC;
    /*
    #31 is PD_INT from PD,
    #32 is  PROCHOT from BQ,
    #33 is CHG_OK from USB connector,
    #35 is MIC MUTE
    #43 is TMP_Alert from BQ,
    #105 us AMP_WARN, 0 means warning, amp internal temperature.
    #106 is AMP_FAULT, low if AMP fault, AMP out HiZ then, 1 is FAULT(over temprature or others),0 is Good()
    #112 is BST_PG, amplifier booster power good indicator(1 is good)
    #115 is POGO_PIN,
    #1275 pms405_gpio6(No. 4)
    */
    string argv[ARGC]= {"gpio", "30","32", "35", "43", "105", "106", "115", "1275"};
    char status[ARGC - 1];
    int nfds = ARGC - 1;
    unsigned int gpio[ARGC -1];
    struct pollfd fdset[ARGC - 1];
    map<int,string> keyboard = {{0x021e,"lantern"},{0x72,"voldn"},{0x73,"volup"},{0xa4,"playpause"},{0x66,"action"},{0xda,"connect"},{0x6e,"insert"}};
    map<int,string> keydir = {{0,"release"},{1,"press"}};
};

#endif
