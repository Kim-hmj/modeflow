/**
*  @file      bq40z50-drv.c
*  @brief     bq40z50 SMBUS(i2c) drv
*  @author    Link Lin
*  @date      12 -2019
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

#include "bq40z50_interface.h"
#include "external.h"
#include <algorithm>
#define I2C_FILE_NAME   "/dev/i2c-5"

static unsigned int I2C_ADDR = 0x0B; //7 bit address, (the 8 bit address is 0x16)

static int fd;

static unsigned char disable_communication_flag = 0;

#define  ManufacturerAccess_REG         0x00
#define  ManufacturerData_REG           0x23


//just for test
static int i2c_read_count = 0;
static int i2c_read_err_count = 0;


//supposed to be Little-endian
static int check_endian(void)
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


int i2c_open_fuelgauge(void)
{
    int ret;

    int val;

    unsigned char i2c_addr = I2C_ADDR;

    fd = i2c_open(I2C_FILE_NAME, i2c_addr);

    if(fd < 0)
    {
        perror("Unable to open fuelgauge i2c control file");

        return -1;
    }

    syslog(LOG_DEBUG, "charging, open fuelgauge i2c file success, fd is %d\n",fd);

    return 0;
}

static int bq40z50_i2c_write(unsigned char dev_addr, unsigned char reg, unsigned char *val, unsigned char data_len)
{
    unsigned char buf[80] = {0};
    int i;

    int ret;

    if(disable_communication_flag)
    {
        syslog(LOG_DEBUG, "charging, fuelgauge communication disabled\n");
        return -1;
    }

    if(data_len + 2 >= 80)
    {
        syslog(LOG_DEBUG, "charging, data_len_exceed\n");
        return -1;
    }

    buf[0] = reg;

    for(i = 0; i<data_len; i++)
    {
        buf[1+i] = val[i];
    }

    if(i2c_write(fd, dev_addr, buf, data_len+1) == 0)
    {
        syslog(LOG_DEBUG, "charging, bq40z50 write reg 0x%02x = ",buf[0]);
        for(i = 1; i < data_len+1; i++)
        {
            syslog(LOG_DEBUG, "charging, %02x ",buf[i]);
        }
        syslog(LOG_DEBUG, "charging, \n");

        return 0;
    }

    return -1;
}


static int bq40z50_i2c_read(unsigned char addr, unsigned char *reg_w_list, unsigned char reg_w_len, unsigned char *val, unsigned char data_len)
{
    unsigned char buf[80] = {0};
    int i;
#ifdef X86_DEBUG
    return 0;
#endif
    if(disable_communication_flag)
    {
        syslog(LOG_DEBUG, "charging, fuelgauge communication disabled\n");
        return -1;
    }

    if(data_len + 1 >= 80)
    {
        syslog(LOG_DEBUG, "charging, data_len_exceed\n");
        return -1;
    }

    if(i2c_readwrite(fd, addr, reg_w_list, reg_w_len, buf, data_len) == 0)
    {
        //syslog(LOG_DEBUG, "charging, bq40z50 read reg 0x%x = ",reg_w_list[0]);
        for(i = 0; i < data_len; i++)
        {
            val[i] = buf[i];
            //syslog(LOG_DEBUG, "charging, %02x ",val[i]);
        }
        return 0;
    }

    return -1;
}


static int bq40z50_ManufacturerAccess_Send(unsigned char* w_data, unsigned int len)
{
    syslog(LOG_DEBUG, "charging, bq40z50 ManufacturerAccess Send\n");

    if(bq40z50_i2c_write(I2C_ADDR, ManufacturerAccess_REG, w_data, len) != 0)
    {
        syslog(LOG_DEBUG, "charging, bq40z50 ManufacturerAccess Send err\n");
        return -1;
    }

    return 0;
}


static int bq40z50_ManufacturerAccess_Read(unsigned char *r_data, unsigned int len)
{
    unsigned char write_val[1] = {0};

    write_val[0] = ManufacturerData_REG;

    syslog(LOG_DEBUG, "charging, bq40z50 ManufacturerAccess Read\n");

    if(bq40z50_i2c_read(I2C_ADDR, write_val, 1, r_data, len) != 0)
    {
        syslog(LOG_DEBUG, "charging, bq40z50 ManufacturerAccess Read err\n");
        return -1;
    }

    return 0;
}


void fuelgauge_read_FirmwareVersion(void)
{
    unsigned char w_buf[16] = {0};
    unsigned char r_buf[16] = {0};

    int i;

    w_buf[0] = 0x02;
    w_buf[1] = 0x00;

    bq40z50_ManufacturerAccess_Send(w_buf, 2);
    bq40z50_ManufacturerAccess_Read(r_buf, 1 + 11); //the first byte is length

    syslog(LOG_DEBUG, "charging, bq40z50 read FirmwareVersion:");
    for(i=0; i<16; i++)
    {
        syslog(LOG_DEBUG, "charging, %02x",r_buf[i]);
    }
    syslog(LOG_DEBUG, "charging, \n\n");
}

void fuelgauge_read_Chemical_ID(void)
{
    unsigned char w_buf[16] = {0};
    unsigned char r_buf[16] = {0};

    int i;

    w_buf[0] = 0x06;
    w_buf[1] = 0x00;

    bq40z50_ManufacturerAccess_Send(w_buf, 2);
    bq40z50_ManufacturerAccess_Read(r_buf, 1 + 2); //the first byte is length

    syslog(LOG_DEBUG, "charging, bq40z50 read Chemical_ID:");
    for(i=0; i<16; i++)
    {
        syslog(LOG_DEBUG, "charging, %02x",r_buf[i]);
    }
    syslog(LOG_DEBUG, "charging, \n\n");
}


void fuelgauge_disable_communication(void)
{
    disable_communication_flag = 1;
    syslog(LOG_DEBUG, "charging, fuelgauge disable_communication\n");
}

void fuelgauge_enable_communication(void)
{
    disable_communication_flag = 0;
    syslog(LOG_DEBUG, "charging, fuelgauge enable_communication\n");
}

int fuelgauge_battery_enter_shutdown_mode(void)
{
    unsigned char w_buf[16] = {0};

    w_buf[0] = 0x10;
    w_buf[1] = 0x00;

    syslog(LOG_DEBUG, "charging, fuelgauge_battery_enter_shutdown_mode\n");

    //write cmd twice in 4 seconds
    if(bq40z50_ManufacturerAccess_Send(w_buf, 2) != 0)
    {
        syslog(LOG_DEBUG, "charging, first write err\n");
        return -1;
    }

    sleep(1);

    if(bq40z50_ManufacturerAccess_Send(w_buf, 2) != 0)
    {
        syslog(LOG_DEBUG, "charging, second write err\n");
        return -1;
    }

    return 0;
}


void check_fuelgauge_iic_readErrCnt(void)
{
    syslog(LOG_DEBUG, "charging, read fuelgauge iic err times: %d / %d\n", i2c_read_err_count, i2c_read_count);
}


int fuelgauge_get_Battery_Temperature(void)
{
    unsigned char buf[16];
    unsigned char reg;

    unsigned short temp = 0;

    signed short battery_temperature = 0;

    //Temperature
    reg = 0x08;
    if(bq40z50_i2c_read(I2C_ADDR, &reg, 1, buf, 2) != 0)
    {
        return Temperature_UNVALID;
    }

    temp = (buf[1]<<8) | buf[0];

    //convent to °C
    battery_temperature = temp/10 - 273;

    syslog(LOG_DEBUG, "charging, get battery Temperature %d * 0.1K, %dC\n\n", temp, battery_temperature);

    return battery_temperature;


    //just fot debug use
    /*FILE *fp;

    char t_buf[16];

    int ret;
    int i;

    fp = fopen("/tmp/batt_temp","r");
    if(fp == NULL)
    {
        //no test file, return normal temperature
        return 30;
    }

    ret = fread(t_buf,1,16,fp);

    syslog(LOG_DEBUG, "charging, read %d data from file:", ret);
    for(i=0; i<ret; i++)
    {
        syslog(LOG_DEBUG, "charging, %c",t_buf[i]);
    }
    syslog(LOG_DEBUG, "charging, \n");

    battery_temperature = atoi(t_buf);
    syslog(LOG_DEBUG, "charging, get batt temp: %d\n", battery_temperature);

    return battery_temperature;*/
}


int fuelgauge_get_Battery_Voltage(void)
{
    unsigned char buf[16];
    unsigned char reg;

    unsigned short battery_voltage = 0;

    //Voltage
    reg = 0x09;
    if(bq40z50_i2c_read(I2C_ADDR, &reg, 1, buf, 2) != 0)
    {
        return -1;
    }

    battery_voltage = (buf[1]<<8) | buf[0];

    syslog(LOG_DEBUG, "charging, get battery Voltage %dmV\n\n", battery_voltage);

    return battery_voltage;
}

int fuelgauge_get_maxcell_Voltage(void)
{
    unsigned char buf[16];
    unsigned char reg;

    unsigned short cell1_voltage = 0, cell2_voltage = 0, cell3_voltage = 0;

    //cell Voltage
    reg = 0x3c;
    if(bq40z50_i2c_read(I2C_ADDR, &reg, 1, buf, 2) != 0)
    {
        return -1;
    }

    cell1_voltage = (buf[1]<<8) | buf[0];

    //cell Voltage
    reg = 0x3d;
    if(bq40z50_i2c_read(I2C_ADDR, &reg, 1, buf, 2) != 0)
    {
        return -1;
    }

    cell2_voltage = (buf[1]<<8) | buf[0];

    reg = 0x3e;
    if(bq40z50_i2c_read(I2C_ADDR, &reg, 1, buf, 2) != 0)
    {
        return -1;
    }

    cell3_voltage = (buf[1]<<8) | buf[0];


    syslog(LOG_DEBUG, "charging, max cell Voltage %dmV\n\n", max(max(cell1_voltage, cell2_voltage),cell3_voltage));

    return max(max(cell1_voltage, cell2_voltage),cell3_voltage);
}

int fuelgauge_get_Battery_Current(void)
{
    unsigned char buf[16];
    unsigned char reg;

    signed short battery_current = 0;

    //Current
    reg = 0x0A;
    if(bq40z50_i2c_read(I2C_ADDR, &reg, 1, buf, 2) != 0)
    {
        return 100000000;//10000A is impossible
    }

    battery_current = (signed short)((buf[1]<<8) | buf[0]);

    syslog(LOG_DEBUG, "charging, get battery Current %dmA\n\n", battery_current);

    return battery_current;
}


int fuelgauge_get_RelativeStateOfCharge(void)
{
    unsigned char buf[16];
    unsigned char reg;

    unsigned short relative_state_of_charge = 0;

    //RelativeStateOfCharge
    reg = 0x0D;
    if(bq40z50_i2c_read(I2C_ADDR, &reg, 1, buf, 2) != 0)
    {
        return -1;
    }

    relative_state_of_charge = (buf[1]<<8) | buf[0];

    syslog(LOG_DEBUG, "charging, get RelativeStateOfCharge %d%%\n\n", relative_state_of_charge);

    return relative_state_of_charge;
}


int fuelgauge_get_AbsoluteStateOfCharge(void)
{
    unsigned char buf[16];
    unsigned char reg;

    unsigned short absolute_state_of_charge = 0;

    //AbsoluteStateOfCharge
    reg = 0x0E;
    if(bq40z50_i2c_read(I2C_ADDR, &reg, 1, buf, 2) != 0)
    {
        return -1;
    }

    absolute_state_of_charge = (buf[1]<<8) | buf[0];

    syslog(LOG_DEBUG, "charging, get Absolute State Of Charge %d%%\n\n", absolute_state_of_charge);

    return absolute_state_of_charge;
}


int fuelgauge_get_Battery_ChargingCurrent(void)
{
    unsigned char buf[16];
    unsigned char reg;

    signed short battery_charge_current = 0;

    //Charging Current
    reg = 0x14;
    if(bq40z50_i2c_read(I2C_ADDR, &reg, 1, buf, 2) != 0)
    {
        return -1;
    }

    battery_charge_current = (signed short)((buf[1]<<8) | buf[0]);

    syslog(LOG_DEBUG, "charging, get battery charge Current %dmA\n\n", battery_charge_current);

    return battery_charge_current;
}


int fuelgauge_get_Battery_ChargingVoltage(void)
{
    unsigned char buf[16];
    unsigned char reg;

    signed short battery_charge_voltage = 0;

    //Charging Voltage
    reg = 0x15;
    if(bq40z50_i2c_read(I2C_ADDR, &reg, 1, buf, 2) != 0)
    {
        return -1;
    }

    battery_charge_voltage = (signed short)((buf[1]<<8) | buf[0]);

    syslog(LOG_DEBUG, "charging, get battery charge Voltage %dmV\n\n", battery_charge_voltage);

    return battery_charge_voltage;
}


int fuelgauge_get_BatteryStatus(void)
{
    unsigned char buf[16];
    unsigned char reg;

    unsigned short battery_status = 0;

    //BatteryStatus
    reg = 0x16;
    if(bq40z50_i2c_read(I2C_ADDR, &reg, 1, buf, 2) != 0)
    {
        syslog(LOG_DEBUG, "charging, get BatteryStatus err\n");
        return -1;
    }

    battery_status = (buf[1]<<8) | buf[0];

    syslog(LOG_DEBUG, "charging, get BatteryStatus %04x.", battery_status);

    return battery_status;
}


int fuelgauge_check_BatteryFullyCharged(void)
{
    int battery_status = 0;

    battery_status = fuelgauge_get_BatteryStatus();
    if( battery_status < 0)
    {
        return -1;
    }

    if(battery_status & 0x0020)
    {
        syslog(LOG_DEBUG, "charging, Battery fully charged\n\n");
        return 1;
    }

    return 0;
}


