/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MODEFLOW_SYSTEM_H
#define MODEFLOW_SYSTEM_H

#include "modeflow.h"
#include "external.h"
#include <string>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <cstdint>
#include <syslog.h>
#include <thread>
#include "playerstate.h"
//system
/*
In docked mode, trigger battery check here.
first silent i2c5, then check with interval of 3 seconds.
*/

class PlayerStates;
class DevicePower:public ModeFlow
{
public:
    DevicePower(PlayerStates *pactManStreaming);
    virtual void IfConnector();
    virtual void Input();
    virtual void Internal();
    virtual void Output();
    virtual void IOConnector();

private:
    void InterfaceAudio();
    void ConnectorSource();
    void ButtonFilter();

    shared_ptr<string> SysDis();
    shared_ptr<string> SysEn();
    shared_ptr<string> BatSatusDetect();
    shared_ptr<string> connection_enable();
    shared_ptr<string> connection_enable_startup();

    shared_ptr<string> sysAndLed_enable();

    int8_t mPromptChannels =0;
    const int8_t mHalchannelID[2]= {28,15};
    class PlayerStates* mPlayerStates;
    struct DevicePowerSt {
        uint8_t mStreamingChannels =0; //bit 0: bt_music, bit1:bt_phone, bit2,avs :bit3,airplay,bit4,spotify,  bit5 snapclient, bit7:Tone
        bool docked = false;
        DevicePowerSt() {
        }
    } * pstDevicePower;
    const struct DevicePowerSt mconst_DevicePowerSt;

};
#endif
