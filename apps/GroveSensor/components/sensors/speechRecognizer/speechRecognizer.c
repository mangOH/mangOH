#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"

#include <termios.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/signal.h>

static bool data_available = false;
static bool is_fetched_cmd = false;
static int cmd_index = -1;
void signal_handler_IO(int status);

struct sigaction saio;
struct termios oldtio, newtio;

const char *voiceBuffer[] =
{
    "Turn on the light",
    "Turn off the light",
    "Play music",
    "Pause",
    "Next",
    "Previous",
    "Up",
    "Down",
    "Turn on the TV",
    "Turn off the TV",
    "Increase temperature",
    "Decrease temperature",
    "What's the time",
    "Open the door",
    "Close the door",
    "Left",
    "Right",
    "Stop",
    "Start",
    "Mode 1",
    "Mode 2",
    "Go",
};

int open_uart1(char *dev)
{
    int fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);

    /* install the signal handler before making the device asynchronous */
    saio.sa_handler = signal_handler_IO;
    // saio.sa_mask = 0;
    saio.sa_flags = 0;
    saio.sa_restorer = NULL;
    sigaction(SIGIO, &saio, NULL);

    /* allow the process to receive SIGIO */
    fcntl(fd, F_SETOWN, getpid());
    fcntl(fd, F_SETFL, FASYNC);

    // fcntl(fd, F_SETFL, 0);

    // get the parameters
    tcgetattr(fd, &oldtio);
    tcgetattr(fd, &newtio);

    // Set the baud rates to 115200...
    cfsetispeed(&newtio, B9600);
    cfsetospeed(&newtio, B9600);
    // Enable the receiver and set local mode...
    newtio.c_cflag |= (CLOCAL | CREAD);
    // No parity (8N1):
    newtio.c_cflag &= ~PARENB;
    newtio.c_cflag &= ~CSTOPB;
    newtio.c_cflag &= ~CSIZE;
    newtio.c_cflag |= CS8;
    // enable hardware flow control (CNEW_RTCCTS)
    // newtio.c_cflag |= CRTSCTS;
    // if(hw_handshake)
    // Disable the hardware flow control for use with mangOH RED
    newtio.c_cflag &= ~CRTSCTS;

    // set raw input
    newtio.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    newtio.c_iflag &= ~(INLCR | ICRNL | IGNCR);

    // set raw output
    newtio.c_oflag &= ~OPOST;
    newtio.c_oflag &= ~OLCUC;
    newtio.c_oflag &= ~ONLRET;
    newtio.c_oflag &= ~ONOCR;
    newtio.c_oflag &= ~OCRNL;

    newtio.c_cc[VMIN] = 0;
    newtio.c_cc[VTIME] = 0;

    // Set the new newtio for the port...
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &newtio);

    return fd;
}

void read_uart1(int fd)
{
    char read_buffer[32];   /* Buffer to store the data received              */
    int  bytes_read = 0;    /* Number of bytes read by the read() system call */
    int i;

    bytes_read = read(fd, &read_buffer, 10); /* Read the data                 */

    for(i=0 ; i < bytes_read; i++)
    {
        if (read_buffer[i] >= 1 && read_buffer[i] <= sizeof(voiceBuffer))
        {
            cmd_index = read_buffer[i] - 1;
            LE_INFO(voiceBuffer[cmd_index]);
            is_fetched_cmd = false;
        }
        else
        {
            LE_ERROR("Unknown Command '%d'\n", read_buffer[i]);
        }
    }

    if (bytes_read <= 0)
    {
        data_available = false;
    }
}

void signal_handler_IO(int status)
{
    data_available = true;
}

// please disable periodic sensor sample if you want to use this function manually
void recognizer_getCmd(char* cmd_buffer, size_t cmd_bufferSize)
{
    if (is_fetched_cmd)
    {
        cmd_buffer[0] = 0;
    }
    else
    {
        if (cmd_bufferSize <= strlen(voiceBuffer[cmd_index])) {
            LE_ERROR("Input buffer is too small to hold the command string");
        }
        else
        {
            is_fetched_cmd = true;
            memcpy(cmd_buffer, voiceBuffer[cmd_index], strlen(voiceBuffer[cmd_index]));
            cmd_buffer[strlen(voiceBuffer[cmd_index])] = 0;
        }
    }
}

void periodicSample(psensor_Ref_t ref, void *context)
{
    char cmd[32];
    recognizer_getCmd(cmd, 31);
    if (strlen(cmd) > 0)
    {
        psensor_PushString(ref, 0, cmd);
    }
}

COMPONENT_INIT
{
    LE_INFO("Speech Recognizer start.");

    int serial_fd = open_uart1("/dev/ttyHS0");

    psensor_Create("recognizer", DHUBIO_DATA_TYPE_STRING, "", periodicSample, NULL);

    while(1)
    {
        if (data_available)
        {
            read_uart1(serial_fd);
        }
        else
        {
            usleep(1000);
        }
    }

    // restore old serial configuration
    tcsetattr(serial_fd, TCSANOW, &oldtio);
    close(serial_fd);
}
