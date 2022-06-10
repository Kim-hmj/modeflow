/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MODEFLOW_AVS_H
#define MODEFLOW_AVS_H
#include "modeflow.h"
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
#include "led.h"
using adk::msg::AdkMessage;
using adk::msg::AdkMessageService;

//for engineering samples special build
//#define ENGINEERING
class AVS:public ModeFlow
{
public:
    AVS();
    virtual void IfConnector();
    virtual void Input();
    virtual void Internal();
    virtual void Output();
    virtual void IOConnector();
    void setLanternDirection(int dirNum);
    int getLanternDirection();

private:
    shared_ptr<string> tapAVS();
    shared_ptr<string> shutdown();
    shared_ptr<string> start();
    shared_ptr<string> micmute();
    shared_ptr<string> micunmute();

    bool m_alert_dule = false;
    bool m_dnd = false;
    bool m_mute = false;
    int direction = 30;
};
#endif
