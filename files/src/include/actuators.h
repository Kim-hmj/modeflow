/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MODEFLOW_RULES_H
#define MODEFLOW_RULES_H
#include  "modeflow.h"
#include "external.h"
#include <string>
#include <cstdint>
#include "led.h"
class Actuators:public ModeFlow
{
public:
    Actuators();
    shared_ptr<string> replace(string input, string match, string replace);//replace is a regex string
    void led_actuators();
    void audio_actuators();
    void internal_actuators();
    void exec_actuators();
    void ledPattern(ELEDPattern patternID);
    void ledStopSignal(ELEDPattern patternID);
    void avs_actuators();
};
#endif
