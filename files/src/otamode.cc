/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 Ota Mode management
 *
 */

#include <sstream>
#include <cstring>
#include "otamode.h"
ModeOta::ModeOta():ModeFlow("ota")
{
    /*
    Register relation ship: input event -> target mode.
    */
    em2mMap_re("devpwr::(?!Docked).+2(Docked)",  "Init",          "ReadyToReboot");
    em2mMap_re("devpwr::(?!Docked).+2(Docked)",  "Wait4Reboot",   "Rebooting");

    em2mMap_re("devpwr::(Docked)2(?!Docked).+",  "ReadyToReboot", "Init");

    em2mMap_re("ota::Wait4Reboot", "(?!ReadyToReboot).+", "Wait4Reboot");

    em2mMap("ota::Wait4Reboot", "ReadyToReboot",       "Rebooting");

    /*
        Do recovery, Ota shall have a Init mode to differentiate with Off.
    */

    modeRecovery("Init");
}

