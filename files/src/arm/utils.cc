
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

int i2c_open(const std::string &devpath, unsigned char addr)
{
    int ret;
    int val;

    int fd_i2c = open(devpath.c_str(), O_RDWR);

    if(fd_i2c < 0)
    {
        perror("Unable to open i2c control file");

        return -1;
    }

    printf("open %s i2c file success,fd is %d\n", devpath.c_str(), fd_i2c);

    ret = ioctl(fd_i2c, I2C_SLAVE_FORCE, addr);
    if (ret < 0)
    {
        perror("i2c: Failed to set i2c device address\n");
        return -1;
    }

    printf("i2c: set i2c device address success\n");

    val = 3;
    ret = ioctl(fd_i2c, I2C_RETRIES, val);
    if(ret < 0)
    {
        printf("i2c: set i2c retry times err\n");
    }

    printf("i2c: set i2c retry times %d\n",val);

    return fd_i2c;
}

int i2c_write(int fd_i2c, unsigned char dev_addr, void *val, unsigned char len)
{
    int ret;
    int i;

    struct i2c_rdwr_ioctl_data data;

    struct i2c_msg messages;


    messages.addr = dev_addr;  //device address
    messages.flags = 0;    //write
    messages.len = len;
    messages.buf = (__u8*)val;  //data

    data.msgs = &messages;
    data.nmsgs = 1;

    if(ioctl(fd_i2c, I2C_RDWR, &data) < 0)
    {
        printf("write ioctl err %d\n",fd_i2c);
        return -1;
    }

    printf("i2c write buf = ");
    for(i=0; i< len; i++)
    {
        printf("%02x ",((unsigned char *)val)[i]);
    }
    printf("\n");

    return 0;
}


int i2c_read(int fd_i2c, unsigned char addr, unsigned char reg, unsigned char *val, unsigned char len)
{
    int ret;
    int i;

    struct i2c_rdwr_ioctl_data data;
    struct i2c_msg messages[2];

    messages[0].addr = addr;  //device address
    messages[0].flags = 0;    //write
    messages[0].len = 1;
    messages[0].buf = &reg;  //reg address

    messages[1].addr = addr;       //device address
    messages[1].flags = I2C_M_RD;  //read
    messages[1].len = len;
    messages[1].buf = (__u8*)val;

    data.msgs = messages;
    data.nmsgs = 2;

    if(ioctl(fd_i2c, I2C_RDWR, &data) < 0)
    {
        perror("---");
        printf("read ioctl err %d\n",fd_i2c);

        return -1;
    }

    printf("i2c read buf = ");
    for(i = 0; i < len; i++)
    {
        printf("%02x ",val[i]);
    }
    printf("\n");

    return 0;
}

int i2c_readwrite(int fd, unsigned char addr, unsigned char *reg_w_list, unsigned char reg_w_len, unsigned char *val, unsigned char len)
{
    int ret;
    int i;

    struct i2c_rdwr_ioctl_data data;
    struct i2c_msg messages[2];

    messages[0].addr = addr;  //device address
    messages[0].flags = 0;    //write
    messages[0].len = reg_w_len;
    messages[0].buf = reg_w_list;  //reg address

    messages[1].addr = addr;       //device address
    messages[1].flags = I2C_M_RD;  //read
    messages[1].len = len;
    messages[1].buf = val;

    data.msgs = messages;
    data.nmsgs = 2;

    //i2c_read_count++;

    ret = ioctl(fd, I2C_RDWR, &data);

    if(ret < 0)
    {
        syslog(LOG_DEBUG, "read ioctl err %d\n",ret);
        //i2c_read_err_count++;
        return ret;
    }

    /*syslog(LOG_DEBUG, "i2c read buf = ");
    for(i = 0; i < len; i++)
    {
        syslog(LOG_DEBUG, "%02x ",val[i]);
    }
    syslog(LOG_DEBUG, "\n");*/

    return 0;
}

