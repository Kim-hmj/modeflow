/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifndef MODEFLOW_AMPT_H
#define MODEFLOW_AMPT_H
#include "modeflow.h"
#include <string>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <syslog.h>

int i2c_open(const std::string &devpath, unsigned char addr);
int i2c_write(int fd_i2c, unsigned char dev_addr, unsigned char *val, unsigned char len);
int i2c_read(int fd_i2c, unsigned char addr, unsigned char reg, unsigned char *val, unsigned char len);

class AMPTemp:public ModeFlow
{
public:
    AMPTemp();
    shared_ptr<string> AmptDetect();
    shared_ptr<string> SetThresh(std::string event);

private:
    int temp_protect_thres = 100, temp_unprotect_thres = 90;
};
#endif
