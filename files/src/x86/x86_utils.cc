/*
 *
 * Author:Ryder Lee<ryder.lee@tymphany.com>
 *
 * Rivian R1 modeflow
 *
 */

#ifdef X86_DEBUG
#include <stdio.h>
#include <fcntl.h>
#include <error.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <poll.h>
#include <stdint.h>
#include <time.h>
#include <linux/input.h>
#include <sys/inotify.h>
#include <syslog.h>
#include <iostream>
#include <map>
using namespace std;
static unsigned int fdCount = 1;
map<unsigned int, string> fdmap;
int i2c_open(const std::string &devpath, unsigned char addr)
{
    int ret;
    int val;
    fdmap[fdCount] = devpath;
    return fdCount++;
}

int i2c_write(int fd_i2c, unsigned char dev_addr, void *val, unsigned char len)
{
#ifdef TEST
#else
    cout << fdmap[fd_i2c] << " write:";
    printf("%#2x ", dev_addr);
    for(int i = 0; i < len; i++) {
        printf("%#04x ",((__u8*)val)[i]);
    }

    cout << endl;
#endif
    return 0;
}


int i2c_read(int fd_i2c, unsigned char addr, unsigned char reg, unsigned char *val, unsigned char len)
{
#ifdef TEST
#else

    cout << fdmap[fd_i2c] << " read "  << ":";
    printf("%#2x len: %d", reg, len);
    cout << endl;
#endif

    return 0;
}

int i2c_readwrite(int fd, unsigned char addr, unsigned char *reg_w_list, unsigned char reg_w_len, unsigned char *val, unsigned char len)
{
#ifdef TEST
#else
    cout << fdmap[fd] << "readwrite:" << addr << ":...";

    cout << endl;
#endif
    return 0;
}

#endif
