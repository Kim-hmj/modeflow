/**
*  @file      tps65987-drv.c
*  @brief     tps65987 i2c drv
*  @author    Link Lin
*  @date      11 -2019
*  @copyright
*/

#include<stdio.h>
#include<fcntl.h>
#include <error.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include<string.h>
#include<stdlib.h>

#include<linux/i2c.h>
#include<linux/i2c-dev.h>

#include "tps65987_interface.h"
#include "external.h"

#define I2C_FILE_NAME   "/dev/i2c-5"
//#define I2C_ADDR        0x38
//#define I2C_ADDR        0x20

//the I2C addr will change, 0x38 or 0x20
static unsigned int I2C_ADDR = 0x38;

//supposed to be Little-endian
int check_endian(void)
{
    unsigned int x;
    unsigned char x0,x1,x2,x3;

    x=0x12345678;

    x0=((unsigned char *)&x)[0];  //low byte
    x1=((unsigned char *)&x)[1];
    x2=((unsigned char *)&x)[2];
    x3=((unsigned char *)&x)[3];  //high byte

    if(x0 == 0x12)
    {
        syslog(LOG_DEBUG, "charging, Big-endian, x0=0x%x,x1=0x%x,x2=0x%x,x3=0x%x\n",x0,x1,x2,x3);
    }

    if(x0 == 0x78)
    {
        syslog(LOG_DEBUG, "charging, Little-endian, x0=0x%x,x1=0x%x,x2=0x%x,x3=0x%x\n",x0,x1,x2,x3);
    }

    return 0;
}


static int fd;


int i2c_open_tps65987(void)
{
    int ret;

    int val;
    unsigned char i2c_addr = I2C_ADDR;

    fd = i2c_open(I2C_FILE_NAME, i2c_addr);

    if(fd < 0)
    {
        perror("Unable to open tps65987 i2c control file");

        return -1;
    }

    syslog(LOG_DEBUG, "charging, open tps65987 i2c file success, fd is %d\n",fd);

    return 0;
}

int tps65987_i2c_write(unsigned char dev_addr, unsigned char reg, unsigned char *val, unsigned char data_len)
{
    unsigned char buf[80] = {0};
    int i;

    int ret;

    if(data_len + 2 >= 80)
    {
        syslog(LOG_DEBUG, "charging, data_len_exceed\n");
        return -1;
    }

    buf[0] = reg;
    buf[1] = data_len;

    for(i = 0; i<data_len; i++)
    {
        buf[2+i] = val[i];
    }

    if(i2c_write(fd, dev_addr, buf, data_len+2) == 0)
    {
        return 0;
    }

    return -1;
}


int tps65987_i2c_read(unsigned char addr, unsigned char reg, unsigned char *val, unsigned char data_len)
{
    unsigned char buf[80] = {0};
    int i;

    if(data_len + 1 >= 80)
    {
        syslog(LOG_DEBUG, "charging, data_len_exceed\n");
        return -1;
    }

    if(i2c_read(fd, addr, reg, buf, data_len+1) == 0)
    {
        syslog(LOG_DEBUG, "charging, read tps65987 reg 0x%x = ",reg);
        for(i = 0; i < data_len; i++)
        {
            val[i] = buf[1+i];
            syslog(LOG_DEBUG, "charging, %02x ",val[i]);
        }
        syslog(LOG_DEBUG, "charging, \n");

        return 0;
    }

    return -1;
}


static int tps65987_send_4CC_Cmd(unsigned char *cmd_ptr, unsigned char *cmd_data_ptr, unsigned char cmd_data_length)
{
    int ret;
    int i;

    unsigned char val[4] = {0};

    //first write 4CC Cmd Used Data(if any)
    if(cmd_data_ptr != NULL)
    {
        ret = tps65987_i2c_write(I2C_ADDR, 0x09, cmd_data_ptr, cmd_data_length);

        if(ret != 0)
        {
            syslog(LOG_DEBUG, "charging, write 4CC Cmd Used Data err \n");
            return -1;
        }
    }

    val[0] = cmd_ptr[0];
    val[1] = cmd_ptr[1];
    val[2] = cmd_ptr[2];
    val[3] = cmd_ptr[3];

    syslog(LOG_DEBUG, "charging, send 4CC Cmd : ");
    for(i=0; i<4; i++)
    {
        syslog(LOG_DEBUG, "charging, %c",val[i]);
    }
    syslog(LOG_DEBUG, "charging, \n");

    //write 4CC Cmd
    return tps65987_i2c_write(I2C_ADDR, 0x08, val, 4);
}


static int tps65987_check_4CC_Cmd_executed()
{
    int i;

    unsigned char buf[4] = {0,1,2,3}; //just a random value

    unsigned char Cmd_exec_success[4] = {0,0,0,0};

    unsigned char Cmd_exec_fail[4] = {'C','M','D',' '};
    unsigned char Cmd_unrecognized[4] = {'!','C','M','D'};

    for(i = 0; i< 50; i++)
    {
        usleep(10000);

        tps65987_i2c_read(I2C_ADDR, 0x08, buf, 4);

        if(memcmp(buf,Cmd_exec_success,4) == 0)
        {
            syslog(LOG_DEBUG, "charging, 4CC Cmd executed, %d\n", i);
            return 0;
        }

        if(memcmp(buf,Cmd_exec_fail,4) == 0)
        {
            syslog(LOG_DEBUG, "charging, 4CC Cmd exec fail, %d\n", i);
            return 1;
        }

        if(memcmp(buf,Cmd_unrecognized,4) == 0)
        {
            syslog(LOG_DEBUG, "charging, 4CC Cmd unrecognized, %d\n", i);
            return -1;
        }
    }

    syslog(LOG_DEBUG, "charging, 4CC Cmd exec timeout, %d\n", i);
    return -1;

}


static int tps65987_read_4CC_Cmd_exec_output(unsigned char *cmd_data_ptr, unsigned char cmd_data_length)
{
    return tps65987_i2c_read(I2C_ADDR, 0x09, cmd_data_ptr, cmd_data_length);
}


int tps65987_exec_4CC_Cmd(unsigned char *cmd_ptr, unsigned char *cmd_data_in_ptr, unsigned char cmd_data_in_length, unsigned char *cmd_data_out_ptr, unsigned char cmd_data_out_length)
{

    if(tps65987_send_4CC_Cmd(cmd_ptr, cmd_data_in_ptr, cmd_data_in_length) != 0)
    {
        syslog(LOG_DEBUG, "charging, send_4CC_Cmd err\n");
        return -1;
    }

    if( strcmp((const char*)cmd_ptr,"Gaid") == 0 || strcmp((const char*)cmd_ptr,"GAID") == 0 )
    {
        //Technically this command never completes since the processor restarts
    }
    else
    {
        if(tps65987_check_4CC_Cmd_executed() != 0)
        {
            syslog(LOG_DEBUG, "charging, 4CC_Cmd exec err\n");
            return -1;
        }
    }


    if(cmd_data_out_ptr != NULL)
    {
        if(tps65987_read_4CC_Cmd_exec_output(cmd_data_out_ptr, cmd_data_out_length) != 0)
        {
            syslog(LOG_DEBUG, "charging, read 4CC_Cmd exec output err\n");
            return -1;
        }
    }

    return 0;
}


int ResetPDController()
{
    unsigned char buf[64] = {0};

    /*
    * Execute GAID, and wait for reset to complete
    */
    syslog(LOG_DEBUG, "charging, Send GAID and Waiting for device to reset\n\r");
    unsigned char cmd[] = "GAID";
    tps65987_exec_4CC_Cmd(cmd, NULL, 0, NULL, 0);

    usleep(1000000);

    //read Mode
    tps65987_i2c_read(I2C_ADDR, REG_MODE, buf, 4);

    tps65987_i2c_read(I2C_ADDR, REG_Version, buf, 4);

    tps65987_i2c_read(I2C_ADDR, REG_BootFlags, buf, 12);

    return 0;
}

int tps65987_get_Intevents(char tps_IntEvents[11])
{
    unsigned char buf[64] = {0};

    if(tps65987_i2c_read(I2C_ADDR, 0x14, (unsigned char*)tps_IntEvents, 11) == 0)
    {

        return 0;
    }

    return -1;
}

int tps65987_clear_Intevents()
{
    unsigned char tps_IntEvents[11]= {0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff, 0xff,0xff, 0xff};

    if(tps65987_i2c_write(I2C_ADDR, 0x18, tps_IntEvents, 11) == 0)
    {

        return 0;
    }

    return -1;
}


int tps65987_get_Status(s_TPS_status *p_tps_status)
{
    unsigned char buf[64] = {0};

    if(tps65987_i2c_read(I2C_ADDR, REG_Status, (unsigned char*)p_tps_status, 8) == 0)
    {
        syslog(LOG_DEBUG, "charging, get tps65987 Status: \n");
        syslog(LOG_DEBUG, "charging, PlugPresent: %d\n", p_tps_status->PlugPresent);
        syslog(LOG_DEBUG, "charging, ConnState: %d\n", p_tps_status->ConnState);
        syslog(LOG_DEBUG, "charging, PlugOrientation: %d\n", p_tps_status->PlugOrientation);
        syslog(LOG_DEBUG, "charging, PortRole: ");
        switch(p_tps_status->PortRole)
        {
        case 0:
            syslog(LOG_DEBUG, "charging, Sink, %d\n", p_tps_status->PortRole);
            break;

        case 1:
            syslog(LOG_DEBUG, "charging, Source, %d\n", p_tps_status->PortRole);
            break;
        }

        syslog(LOG_DEBUG, "charging, DataRole: %d\n", p_tps_status->DataRole);
        switch(p_tps_status->DataRole)
        {
        case 0:
            syslog(LOG_DEBUG, "charging, UFP, %d\n", p_tps_status->DataRole);
            break;

        case 1:
            syslog(LOG_DEBUG, "charging, DFP, %d\n", p_tps_status->DataRole);
            break;
        }

        syslog(LOG_DEBUG, "charging, VbusStatus: %d\n", p_tps_status->VbusStatus);
        syslog(LOG_DEBUG, "charging, UsbHostPresent: %d\n", p_tps_status->UsbHostPresent);
        syslog(LOG_DEBUG, "charging, HighVoltageWarning: %d\n", p_tps_status->HighVoltageWarning);
        syslog(LOG_DEBUG, "charging, LowVoltageWarning: %d\n", p_tps_status->LowVoltageWarning);

        return 0;
    }

    return -1;
}


int tps65987_get_PortRole(void)
{
    s_TPS_status tps_status = {0};

    s_TPS_status *p_tps_status = NULL;

    p_tps_status = &tps_status;

    if(tps65987_i2c_read(I2C_ADDR, REG_Status, (unsigned char*)p_tps_status, 8) == 0)
    {
        syslog(LOG_DEBUG, "charging, get tps65987 Status: \n");
        syslog(LOG_DEBUG, "charging, PlugPresent: %d\n", p_tps_status->PlugPresent);
        syslog(LOG_DEBUG, "charging, ConnState: %d\n", p_tps_status->ConnState);
        syslog(LOG_DEBUG, "charging, PlugOrientation: %d\n", p_tps_status->PlugOrientation);
        syslog(LOG_DEBUG, "charging, PortRole: ");
        switch(p_tps_status->PortRole)
        {
        case 0:
            syslog(LOG_DEBUG, "charging, Sink, %d\n", p_tps_status->PortRole);
            break;

        case 1:
            syslog(LOG_DEBUG, "charging, Source, %d\n", p_tps_status->PortRole);
            break;
        }

        syslog(LOG_DEBUG, "charging, DataRole: %d\n", p_tps_status->DataRole);
        switch(p_tps_status->DataRole)
        {
        case 0:
            syslog(LOG_DEBUG, "charging, UFP, %d\n", p_tps_status->DataRole);
            break;

        case 1:
            syslog(LOG_DEBUG, "charging, DFP, %d\n", p_tps_status->DataRole);
            break;
        }

        syslog(LOG_DEBUG, "charging, VbusStatus: %d\n", p_tps_status->VbusStatus);
        syslog(LOG_DEBUG, "charging, UsbHostPresent: %d\n", p_tps_status->UsbHostPresent);
        syslog(LOG_DEBUG, "charging, HighVoltageWarning: %d\n", p_tps_status->HighVoltageWarning);
        syslog(LOG_DEBUG, "charging, LowVoltageWarning: %d\n", p_tps_status->LowVoltageWarning);

        //return PortRole
        return p_tps_status->PortRole;

    }

    return -1;
}


int tps65987_get_RXSourceNumValidPDOs(void)
{
    unsigned char buf[64] = {0};

    unsigned char rx_valid_PDO_num = 0;

    if(tps65987_i2c_read(I2C_ADDR, REG_RX_Source_Capabilities, buf, 29) != 0)
    {
        syslog(LOG_DEBUG, "charging, get RXSourceNumValidPDOs err \n");
        return -1;
    }

    rx_valid_PDO_num = buf[0] & 0x07;

    syslog(LOG_DEBUG, "charging, get RXSourceNumValidPDOs = %d\n\n", rx_valid_PDO_num);

    return rx_valid_PDO_num;
}


int tps65987_get_ActiveContractPDO(void)
{
    int i;

    unsigned char PDO_15V_3A[4] = {0x2c, 0xb1, 0x04, 0x00}; //PDO: 15V/3A
    unsigned char PDO_20V_2d25A[4] = {0xe1, 0x40, 0x06, 0x00}; //PDO: 20V/2.25A
    unsigned char PDO_20V_3d25A[4] = {0x45, 0x41, 0x16, 0x00}; //PDO: 20V/3.25A

    unsigned char buf[64] = {0};

    if(tps65987_i2c_read(I2C_ADDR, REG_ActiveContractPDO, buf, 6) != 0)
    {
        syslog(LOG_DEBUG, "charging, \nget ActiveContractPDO err \n");
        return -1;
    }

    syslog(LOG_DEBUG, "charging, \nget ActiveContractPDO = ");
    for(i = 0; i < 6; i++)
    {
        syslog(LOG_DEBUG, "charging, %02x ",buf[i]);
    }
    syslog(LOG_DEBUG, "charging, \n");

    if(memcmp(buf, PDO_15V_3A, 4) == 0)
    {
        syslog(LOG_DEBUG, "charging, ActiveContractPDO is 15V/3A\n\n");
        return INPUT_CURRENT__USB_3A_Limit;
    }
    else if(memcmp(buf, PDO_20V_2d25A, 4) == 0)
    {
        syslog(LOG_DEBUG, "charging, ActiveContractPDO is 20V/2.25A\n\n");
        return INPUT_CURRENT__USB_2d25A_Limit;
    }
    else if(memcmp(buf, PDO_20V_3d25A, 4) == 0)
    {
        syslog(LOG_DEBUG, "charging, ActiveContractPDO is 20V/3.25A\n\n");
        return INPUT_CURRENT__USB_3d25A_Limit;
    }

    return INPUT_CURRENT__USB_3A_Limit;
}


int tps65987_get_TypeC_Current(void)
{
    s_TPS_Power_Status tps_power_status = {0};

    s_TPS_Power_Status *p_tps_power_status = NULL;

    p_tps_power_status = &tps_power_status;

    if(tps65987_i2c_read(I2C_ADDR, REG_Power_Status, (unsigned char*)p_tps_power_status, 2) == 0)
    {
        syslog(LOG_DEBUG, "charging, get tps65987 Power Status: \n");
        syslog(LOG_DEBUG, "charging, PowerConnection: %d\n", p_tps_power_status->PowerConnection);
        syslog(LOG_DEBUG, "charging, SourceSink: ");
        switch(p_tps_power_status->SourceSink)
        {
        case 0:
            syslog(LOG_DEBUG, "charging, PD Controller as source, %d\n", p_tps_power_status->SourceSink);
            break;

        case 1:
            syslog(LOG_DEBUG, "charging, PD Controller as sink, %d\n", p_tps_power_status->SourceSink);
            break;
        }

        syslog(LOG_DEBUG, "charging, TypeC_Current: ");
        switch(p_tps_power_status->TypeC_Current)
        {
        case USB_Default_Current:
            syslog(LOG_DEBUG, "charging, USB Default Current, %d\n", p_tps_power_status->TypeC_Current);
            break;

        case C_1d5A_Current:
            syslog(LOG_DEBUG, "charging, 1.5A, %d\n", p_tps_power_status->TypeC_Current);
            break;

        case C_3A_Current:
            syslog(LOG_DEBUG, "charging, 3A, %d\n", p_tps_power_status->TypeC_Current);
            break;

        case PD_contract_negotiated:
            syslog(LOG_DEBUG, "charging, PD contract negotiated, %d\n", p_tps_power_status->TypeC_Current);
            break;
        }

        syslog(LOG_DEBUG, "charging, Charger Detect Status: %d\n", p_tps_power_status->Charger_Detect_Status);
        syslog(LOG_DEBUG, "charging, Charger_AdvertiseStatus: %d\n", p_tps_power_status->Charger_AdvertiseStatus);

        //return TypeC_Current
        return p_tps_power_status->TypeC_Current;

    }

    return -1;
}
