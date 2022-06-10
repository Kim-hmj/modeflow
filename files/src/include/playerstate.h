/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MODEFLOW_STREAMING_H
#define MODEFLOW_STREAMING_H
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
#include "gpio.h"
#include "devpower.h"
#include <sys/time.h>

using adk::msg::AdkMessage;
using adk::msg::AdkMessageService;


class DevicePower;
class PlayerStates:public ModeFlow
{
public:
    PlayerStates();
    virtual void IfConnector();
    virtual void Input();
    virtual void Internal();
    virtual void Output();
    virtual void IOConnector();
private:
    uint16_t OFF_TIMER = 360;//decrease every 5 seconds
    uint16_t STANDBY_TIMER = 240;//decrease every 5 seconds

    const uint32_t SECONDS_IN_DAY = 86400;
    int elapsed =0;
    uint8_t standby_flag = 0;
//    uint16_t OffTimer = OFF_TIMER;
    bool usbhost_present = false;
    struct timeval standby_time, idle_time;
    friend DevicePower;
};
#endif
