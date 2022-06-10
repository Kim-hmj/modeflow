/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MODEFLOW_FAULT_H
#define MODEFLOW_FAULT_H
#include "modeflow.h"
#include "external.h"
#include <string>
#include <syslog.h>
class FaultM:public ModeFlow
{
public:
    FaultM();

    enum Enum_PRODUCT_ERROR
    {
        No_Fault	= 0,
        Overheat1	= 1 << 0,
        Overheat2	= 1 << 1,
        Overcool1	= 1 << 2,
        Overcool2	= 1 << 3,
        AMPfrfault	= 1 << 4,
        AMPtwfault	= 1 << 5
    };

    shared_ptr<string> shutdown();
    shared_ptr<string> protect_amp(bool sw);//true to mute, false to unmute
    shared_ptr<string> fault_man(Enum_PRODUCT_ERROR fault, bool status);
};
#endif

