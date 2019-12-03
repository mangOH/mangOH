#include <legato.h>
#include <interfaces.h>
#include "periodicSensor.h"

#include <termios.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/signal.h>

static bool data_available = false;
static bool is_fetched = false;
static char RFID_buffer[64];

void signal_handler_IO(int status);

struct sigaction saio;
struct termios oldtio, newtio;

int open_uart(char *dev) {
	int     fd;

	fd = open (dev, O_RDWR | O_NOCTTY | O_NONBLOCK);

	/* install the signal handler before making the device asynchronous */
	saio.sa_handler = signal_handler_IO;
	// saio.sa_mask = 0;
	saio.sa_flags = 0;
	saio.sa_restorer = NULL;
	sigaction(SIGIO, &saio, NULL);

	/* allow the process to receive SIGIO */
	fcntl(fd, F_SETOWN, getpid());
	fcntl(fd, F_SETFL, FASYNC);

	// fcntl (fd, F_SETFL, 0);
	
	// get the parameters
	tcgetattr (fd, &oldtio);
	tcgetattr (fd, &newtio);

	// Set the baud rates to 9600...
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
	tcsetattr (fd, TCSANOW, &newtio);

	return fd;
}

void read_uart(int fd)
{
	char read_buffer[64];   /* Buffer to store the data received              */
	int  bytes_read = 0;    /* Number of bytes read by the read() system call */
	int i = 0;
	int start_byte_index = -1;
	int stop_byte_index = -1;
	bool is_valid_id = false;

	bytes_read = read(fd, &read_buffer, 64);

	for(i=0 ; i < bytes_read; i++)
	{
#if defined (DEBUG)
		LE_INFO("%2X", read_buffer[i]);
#endif
		if (read_buffer[i] == 0x2) {
			// start byte
			start_byte_index = i;
			if (stop_byte_index != -1) {
				is_valid_id = false;
				start_byte_index = -1;
				stop_byte_index = -1;
			}
		}
		else if (read_buffer[i] == 0x3) {
			// stop byte
			stop_byte_index = i;
			if (start_byte_index >= 0) {
				is_valid_id = true;
			} else {
				is_valid_id = false;
				start_byte_index = -1;
				stop_byte_index = -1;
			}
		}
		if (is_valid_id) {
			memcpy(RFID_buffer,
			       &read_buffer[start_byte_index + 1],
			       stop_byte_index - start_byte_index - 1);
			RFID_buffer[stop_byte_index - start_byte_index] = 0;
			is_fetched = false;
#if defined (DEBUG)
			LE_INFO(RFID_buffer);
#endif
		}
	}
	if (bytes_read <= 0) {
		data_available = false;
	}
}

void write_uart(int fd, char *cmd)
{
	int     wrote = 0;
	wrote = write (fd, cmd, strlen (cmd));
	LE_INFO("write %s", cmd);
	LE_INFO("wrote %d", wrote);
}

void signal_handler_IO(int status)
{
	data_available = true;
}

void RFIDReader_getID
(
    char* cmd_buffer,
        ///< [OUT]
    size_t cmd_bufferSize
        ///< [IN]
);

// please disable periodic sensor sample if you want to use this function manually
void RFIDReader_getID(char* cmd_buffer, size_t cmd_bufferSize)
{
	if (is_fetched) {
		cmd_buffer[0] = 0;
	} else {
		if (cmd_bufferSize <= strlen(RFID_buffer)) {
			LE_ERROR("Input buffer did not have enough size");
		} else {
			is_fetched = true;
			memcpy(cmd_buffer,
			       RFID_buffer,
			       strlen(RFID_buffer));
			cmd_buffer[strlen(RFID_buffer)] = 0;
		}
	}
}

void periodicSample(
	psensor_Ref_t ref,
	void *context
	)
{
	char cmd[64];
	RFIDReader_getID(cmd, 63);
	if (strlen(cmd) > 0) {
		psensor_PushString(ref, 0, cmd);
	}
}

COMPONENT_INIT
{
	LE_INFO("Grove 125KHz RFID reader start");

	int serial_fd;	
	serial_fd = open_uart("/dev/ttyHS0");

	psensor_Create("RFID125KHz", DHUBIO_DATA_TYPE_STRING, "", periodicSample, NULL);

	while(1)
	{
		if (data_available) {
			read_uart(serial_fd);
		} else {
			usleep(1000);
		}
	}

	// restore old serial configuration
	tcsetattr (serial_fd, TCSANOW, &oldtio);
	close(serial_fd);
}
