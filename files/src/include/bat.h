/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MODEFLOW_BAT_H
#define MODEFLOW_BAT_H
#include  "modeflow.h"
#include "external.h"
#include <string>
#include <unistd.h>


#include "flowdspsocket.h"

#include <fstream>
#include <iostream>
#include <cstdint>
#include <syslog.h>
#include <sys/time.h>
#include "charging/tps65987_interface.h"
#include "charging/bq40z50_interface.h"
#include "charging/bq25703_drv.h"
#include "external.h"
#include <cstdint>

int bq25703_set_InputVoltageLimit(unsigned int input_voltage_limit_set);
int bq25703_set_InputVoltageLimit(unsigned int input_voltage_limit_set);
int bq25703a_otg_function_init();
int batteryTemperature_is_overstep_ChargeStopThreshold(int battery_temperature);
int batteryTemperature_is_in_ChargeAllowThreshold(int battery_temperature);
int batteryTemperature_is_overstep_DischargeStopThreshold(int battery_temperature);
int bq25703_init_ChargeOption_0(void);
int bq25703_disable_OTG();
int bq25703_set_InputCurrentLimit(unsigned int input_current_limit_set);
int bq25703_set_ChargeCurrent(unsigned int charge_current_set);
unsigned char decide_the_ChargeLevel(int voltage, int temp);
int bq25703a_charge_function_init(uint16_t registers[], int size);
int decide_the_ChargeCurrent_v2(int vol, int temp);
int bq25703a_get_ChargeCurrentSetting(void);
int bq25703a_get_PSYS_and_VBUS(unsigned int *p_PSYS_vol, unsigned int *p_VBUS_vol);
int bq25703a_get_CMPINVol_and_InputCurrent(unsigned int *p_CMPIN_vol, unsigned int *p_input_current);
extern uint16_t CHARGE_REGISTER_Init_Without_Current[];



class PwrDlvr:public ModeFlow
{
public:
    PwrDlvr();
    virtual void IfConnector();
    virtual void Input();
    virtual void Internal();
    virtual void Output();
    virtual void IOConnector();
    shared_ptr<string> detectPDEvents();
    shared_ptr<string> check_status();
private:
    s_TPS_status oldStatus{};
    bool startup = 0;
    bool sysEn = true;
};

class Battery:public ModeFlow
{
public:
    Battery();
    virtual void IfConnector();
    virtual void Input();
    virtual void Internal();
    virtual void Output();
    virtual void IOConnector();
    shared_ptr<string> detectBatInfo();

private:
    struct faultST
    {
        unsigned int hightem : 1;
        unsigned int  lowtem :1;
        unsigned int  highvol :1;
        unsigned int  lowvol :1;
        unsigned int  highcurrent :1;
        unsigned int  lowcurrent :1;
        unsigned int  lowSOC :1;
    };

    enum faultStatus
    {
        No_Fault = 0,
        Overheat1,
        Overheat2,
        Overcool1,
        Overcool2
    };

    int OVERHEAT1 = 50;
    int OVERHEAT2 = 60;
    int OVERCOOL1 = 0;
    int OVERCOOL2 = -20;

    int vol = 0;
    int current = 100000000;//10000A charge current is impossible
    int SOC = -1;
    int tem = Temperature_UNVALID;
    int maxcellvol = 0;
    struct faultST faultWarning {};
    struct faultST faultStop {};
    struct faultST faultShutdown {};
    struct BatSt {
        bool fullyCharged  = false;
        int chargerPlugged = 0;
        faultStatus fault = No_Fault;
        bool factory_shipment_charge_complete_flag = false;
        unsigned int charge_limit = 100;
        int outcharge = 0;//bit 0: out/not out charging connection; bit 1: outcharging/not outcharging
        ///Todo: shipment shutdown, low bat warning
        struct timeval last_time = {.tv_sec = 0, .tv_usec = 0};
        BatSt() {
        }
    } * pstBat;
    const struct BatSt mconst_BatSt;

};

extern uint16_t	USB_TYPEA_VALUE_BUF[];
extern uint16_t	CHARGE_REGISTER_Init_Without_Current[];
extern uint16_t	OTG_REGISTER_DDR_VALUE_BUF[];
extern uint16_t   CHARGE_REGISTER_DDR_VALUE_BUF[];


extern uint16_t	USB_TYPEA_VALUE_BUF_size ;
extern uint16_t	CHARGE_REGISTER_Init_Without_Current_size;
extern uint16_t	OTG_REGISTER_DDR_VALUE_BUF_size;
extern uint16_t   CHARGE_REGISTER_DDR_VALUE_BUF_size;

class Charger:public ModeFlow
{
public:
    Charger();
    virtual void IfConnector();//external input signal converting interface signal. correspondent to each flow. Such connecting releation can be difined in Output or IfConnector.
    virtual void Input();//interface
    virtual void Internal();//internal processing
    virtual void Output();//Connecting to external module's input signal. Such connecting releation can be difined in Output or IfConnector.
    virtual void IOConnector();//Processing exteral signal and output external signals without internal state change;A simplfiled way ignored Input and Internal definition; focused on flow.
    shared_ptr<string> set_current_USBHostBC(string event);
private:
    int charge_current_set = 0;
    unsigned int input_current = 0;
};

#endif


