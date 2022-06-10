/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MODEFLOW_INDICATION_H
#define MODEFLOW_INDICATION_H
#include  "modeflow.h"

class Indication:public ModeFlow
{
public:
    Indication();
    virtual void IfConnector();
    virtual void Input();
    virtual void Internal();
    virtual void Output();
    virtual void IOConnector();
    uint8_t power_st = 0; //0:init, 1: off, 2:on
private:
//void SeqPowerOnIndication();
//void SeqConnection();
//void SeqAlexa();
};
#endif

