#include "gpio.h"
GPIOD::GPIOD():ModeFlow("gpio")
{
    e2aMap("gpio::Init", [this] {
        std::thread (&ModeFlow::long_actions, this, [this]{
            return initGPIO(argc, argv);
        }).detach();

        std::thread (&ModeFlow::long_actions, this, [this]{
            return detectKey();
        }).detach();

        return (shared_ptr<string>)nullptr;
    });
    e2aMap("gpio::detectTimer", [this] {
        std::thread (&ModeFlow::suspendable_long_actions,this, [this]{
            return detectGPIO(argc, argv);
        }).detach();
        return (shared_ptr<string>)nullptr;
    });

    e2aMap_re("key::.+", [this] {
        std::thread (&ModeFlow::suspendable_long_actions,this, [this]{
            return detectKey();
        }).detach();
        return (shared_ptr<string>)nullptr;
    });

    Output();

    modeRecovery("Off");

    std::thread (&ModeFlow::init_actions,this, [this] {return make_shared<string>("gpio::Init");}).detach();
}


/****************************************************************
 * gpio_export
 ****************************************************************/
int GPIOD::gpio_export(unsigned int gpio)
{
    int fd, len;
    char buf[MAX_BUF];

    fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
    if (fd < 0) {
        perror("gpio/export");
        return fd;
    }

    len = snprintf(buf, sizeof(buf), "%d", gpio);
    write(fd, buf, len);
    close(fd);

    return 0;
}

/****************************************************************
 * gpio_unexport
 ****************************************************************/
int GPIOD::gpio_unexport(unsigned int gpio)
{
    int fd, len;
    char buf[MAX_BUF];

    fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
    if (fd < 0) {
        perror("gpio/export");
        return fd;
    }

    len = snprintf(buf, sizeof(buf), "%d", gpio);
    write(fd, buf, len);
    close(fd);
    return 0;
}

/****************************************************************
 * gpio_set_dir
 ****************************************************************/
int GPIOD::gpio_set_dir(unsigned int gpio, unsigned int out_flag)
{
    int fd, len;
    char buf[MAX_BUF];

    len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR  "/gpio%d/direction", gpio);

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        perror("gpio/direction");
        return fd;
    }

    if (out_flag)
        write(fd, "out", 4);
    else
        write(fd, "in", 3);

    close(fd);
    return 0;
}

/****************************************************************
 * gpio_set_value
 ****************************************************************/
int GPIOD::gpio_set_value(unsigned int gpio, unsigned int value)
{
    int fd, len;
    char buf[MAX_BUF];

    len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        perror("gpio/set-value");
        return fd;
    }

    if (value)
        write(fd, "1", 2);
    else
        write(fd, "0", 2);

    close(fd);
    return 0;
}

/****************************************************************
 * gpio_get_value
 ****************************************************************/
int GPIOD::gpio_get_value(unsigned int gpio, unsigned int *value)
{
    int fd, len;
    char buf[MAX_BUF];
    char ch;

    len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

    fd = open(buf, O_RDONLY);
    if (fd < 0) {
        perror("gpio/get-value");
        return fd;
    }

    read(fd, &ch, 1);

    if (ch != '0') {
        *value = 1;
    } else {
        *value = 0;
    }

    close(fd);
    return 0;
}


/****************************************************************
 * gpio_set_edge
 ****************************************************************/

int GPIOD::gpio_set_edge(unsigned int gpio, char *edge)
{
    int fd, len;
    char buf[MAX_BUF];

    len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/edge", gpio);

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        perror("gpio/set-edge");
        return fd;
    }

    write(fd, edge, strlen(edge) + 1);
    close(fd);
    return 0;
}

/****************************************************************
 * gpio_fd_open
 ****************************************************************/

int GPIOD::gpio_fd_open(unsigned int gpio)
{
    int fd, len;
    char buf[MAX_BUF];

    len = snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

    fd = open(buf, O_RDONLY | O_NONBLOCK );
    if (fd < 0) {
        perror("gpio/fd_open");
    }
    return fd;
}

/****************************************************************
 * gpio_fd_close
 ****************************************************************/

int GPIOD::gpio_fd_close(int fd)
{
    return close(fd);
}

/****************************************************************
 * Main
 ****************************************************************/
shared_ptr<string> GPIOD::initGPIO(int argc, string (&argv)[ARGC] ) {
    memset((void*)fdset, 0, sizeof(fdset));
    memset((void*)status,2,sizeof(status)); //2 means not determined.

    for(int i =0; i < argc -1; i++)
    {
        gpio[i] = std::stoi(argv[i + 1]);

        gpio_export(gpio[i]);
        gpio_set_dir(gpio[i], 0);
        gpio_set_edge(gpio[i], (char *)"both");//argv[2] shalll be falling/rising, if both, need revise logic.
        fdset[i].fd = gpio_fd_open(gpio[i]);
        fdset[i].events = POLLPRI;
    }

    timeout = POLL_TIMEOUT;
    return make_shared<string>("gpio::detectTimer");
}
shared_ptr<string> GPIOD::detectGPIO(int argc, string (&argv)[ARGC])
{

    rc = poll(fdset, nfds, timeout);

    if (rc < 0) {
        printf("\npoll() failed!\n");
        return make_shared<string>("poll_failed");
    }

    if (rc == 0) {
        printf(".");
    }


    for(int i =0; i < argc -1; i++)
    {
        if (fdset[i].revents & POLLPRI) {
            lseek(fdset[i].fd, 0, SEEK_SET);
            len = read(fdset[i].fd, buf, MAX_BUF);

            usleep(30000);
            lseek(fdset[i].fd, 0, SEEK_SET);
            len = read(fdset[i].fd, buf1, MAX_BUF);
            //std::cout <<"before compare:" << (unsigned int)*buf << "+" << (unsigned int)*buf1 << std::endl;
            if(buf[0] =! buf1[0]) {
                std::cout <<"boucing:" << (unsigned int)*buf << "+" << (unsigned int)*buf1 << std::endl;
                continue;
            }
            std::stringstream ps;
            //std::cout <<"before send:"<<(unsigned int)*buf << "+" << (unsigned int)*buf1 << std::endl;

            if(buf1[0] & 0x01) {
                status[i] = 1;
                ps << "gpio::" << gpio[i] << "rising";
            } else {
                status[i] = 0;
                ps << "gpio::" << gpio[i] << "falling";
            }

            sendPayload(make_shared<string>(ps.str()));

            //test again to prevent lost change
            lseek(fdset[i].fd, 0, SEEK_SET);
            len = read(fdset[i].fd, buf, MAX_BUF);
            if(buf[0] =! buf1[0]) {
                i = i -1;
            }
        } else {
            lseek(fdset[i].fd, 0, SEEK_SET);
            len = read(fdset[i].fd, buf, MAX_BUF);
            if((buf[0] & 0x01) != status[i] && status[i] < 2)
            {
                lseek(fdset[i].fd, 0, SEEK_SET);
                len = read(fdset[i].fd, buf1, MAX_BUF);
                int newstatus = buf1[0] & 0x01 ;
                if(newstatus!= status[i])
                {
                    std::stringstream ps;
                    if(newstatus) {
                        status[i] = 1;
                        ps << "gpio::" << gpio[i] << "rising";
                    } else {
                        status[i] = 0;
                        ps << "gpio::" << gpio[i] << "falling";
                    }

                    sendPayload(make_shared<string>(ps.str()));

                }
            }
        }
    }

    sendTimerPayload(make_shared<string>("gpio::detectTimer"));

    return (shared_ptr<string>)nullptr;
}

shared_ptr<string> GPIOD::detectKey()
{
    int i;
    FILE * input;
    uint8_t data[EVENT_LEN];

    input = fopen(INPUT_QUEUE, "r+");

    for(i = 0; i < EVENT_LEN; i++) { //each key press will trigger 16 characters of data, describing the event
        data[i] = (char) fgetc(input);
    }
    fclose(input);
    uint16_t keycode = (uint16_t)data[18]  + ((uint16_t)data[19] << 8);
    return make_shared<string>(std::string("key::") + keyboard.at(keycode) + ":" + keydir.at(data[20]));
}

void GPIOD::Output()
{
    ecMap_re("gpio::35rising", "avs::micmute");
    ecMap_re("gpio::35falling", "avs::micunmute");
}

