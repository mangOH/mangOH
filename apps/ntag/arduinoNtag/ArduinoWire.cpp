/*
 * ArduinoWire.cpp
 *
 * Implementation of the ArduinoWire class
 *
 *  Created on: Apr 4, 2016
 *      Author: mario
 *	Modified (mostly rewrote): SWI for mangOH Yellow
 */

#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include "legato.h"
#include "ArduinoWire.h"
#include "ntag.h"
#include "NDEF/Ndef.h"

ArduinoWire::ArduinoWire(): fd(0), txAddress(0), txBufferIndex(0), txBufferLength(0), rxBufferIndex(0), rxBufferLength(0), releaseBus(true) {

	/* Let's see what functionality our I2c adapter has
	unsigned long funcs; */

	// TODO: Parameterize. On a mangOH Yellow the ntag chip is on i2c bus 8
        if(access("/dev/i2c-8", O_RDWR) == -1) {
                LE_INFO("Cannot access /dev/i2c-8 for O_RDWR error: %s", strerror(errno));
                exit(1);
        }
        /* else
                LE_INFO("Can access /dev/i2c-8"); */

	this->fd = open("/dev/i2c-8", O_RDWR);

	if (this->fd == -1) {
                LE_INFO("Cannot open /dev/i2c-8 O_RDWR ret: %d %s", this->fd, strerror(errno));
                exit(2);
	}

	/* if(ioctl(this->fd, I2C_FUNCS, &funcs) < 0) {
		LE_INFO("I2C_FUNCS returned error");
		exit(3);
  	}
  	LE_INFO("I2c Funcs of adapter: %ld", funcs); */
}

void ArduinoWire::becomeBusMaster() {
	if (txAddress == 0) {
		LE_INFO("Could not communicate with slave device at address: %x", txAddress);
		exit(1);
	}
	/* SWI: We aren't doing SMBus transactions we are using the combined interface
	if (ioctl(this->fd, I2C_SLAVE_FORCE, txAddress) < 0) {
		LE_INFO("Could not I2C_SLAVE_FORCE slave device at address: %x", txAddress);
		exit(1);
	}
	*/

}
void ArduinoWire::begin(){
	txBufferIndex = 0;
	txBufferLength = 0;

	rxBufferIndex = 0;
	rxBufferLength = 0;
}

//void ArduinoWire::begin(uint8_t address) {
//	txAddress = address;
//
//	begin();
//}

void ArduinoWire::beginTransmission(const uint8_t address){
	/* SWI: We aren't doing SMBus transactions we are using the combined interface
	int ret;
        if ((ret = ioctl(fd, I2C_SLAVE_FORCE, address)) < 0) {
        	LE_INFO("ArduinoWire::beginTransmission Error: %s", strerror(errno));
        	LE_INFO("I2C_SLAVE address: %x ret: %d",address, ret);
                exit(1);
        }
	*/
	txAddress = address;
}

size_t ArduinoWire::write(const uint8_t data){
	if (txBufferLength >= BUFFER_LENGTH) {
		LE_INFO("txBuffer is full");
		return 0;
	}

	txBuffer[txBufferIndex] = data;
	txBufferIndex++;
	txBufferLength = txBufferIndex;

	// PrintHex(txBuffer, txBufferLength);
	return 1;
}

size_t ArduinoWire::write(const uint8_t * data, size_t size){
	int res = 0;
	for (uint8_t i=0; i<size; i++) {
		res += this->write(data[i]);
	}

	return res;
}

//FIXME not really sending the STOP bit.check if there are bugs in i2c device...
// We simply restart the connection as master next time
// this function is called
int ArduinoWire::endTransmission(uint8_t sendStop){
	int res = 0;

	/* SWI: We don't need this as we are doing combined transactions across I2c
	if (releaseBus) {
		becomeBusMaster();
		releaseBus = false;
	}
	*/

	if(txBufferLength == 0) {
		LE_INFO("Error txBufferLength == 0, nothing to transmit");
		return 1;
	}

	// PrintHex(txBuffer, txBufferLength);

	/* For the NXP NT3H2111_2211 we are allowed to only write NTAG_BLOCK_SIZE  */
	res = i2c_write();
	if (res < 0 )
		LE_INFO("Transmission fail: txBufferLength: %d", txBufferLength);

	txBufferIndex = 0;
	txBufferLength = 0;

	//releaseBus = sendStop;

	return res;
}

int ArduinoWire::requestFrom(int address, int read_len, bool releaseBus){
	int read = 0;

	rxBufferIndex = 0;
	rxBufferLength = read_len;

	if(txBufferLength == 0) {
		LE_INFO("Error txBufferLength == 0, no block/register address");
		return 0;
	}

	// LE_INFO("Block/Register addressing:");
	// PrintHex(txBuffer, txBufferLength);

	read = i2c_read();
	// LE_INFO("Reading from address: %d read_len: %d", address, read_len);
	/* if (read == read_len) {
		LE_INFO("read OK");
		PrintHex(rxBuffer, read);
	} else {
		LE_INFO("read not OK");
	} */

	//this->releaseBus = releaseBus;

	/* We assume that a write register/block calls are not
	 * happening simultaneously, or things will break - need a mutex.
	 */
	txBufferIndex = 0;
	txBufferLength = 0;

	return read;
}

int ArduinoWire::available(){
	if (rxBufferLength - rxBufferIndex >= 1) {
		return 1;
	} else {
		return 0;
	}
}

uint8_t ArduinoWire::read(){
	if (rxBufferIndex >= rxBufferLength) {
		LE_INFO("No more data.");
		return 0;
	}
	return rxBuffer[rxBufferIndex++];
}

// Read a NTAG block/register and return the read value in inbuf
int ArduinoWire::i2c_read() {
    int ret;
    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data msgset[1];

    if(txAddress == 0) {
        LE_INFO("I2c address not set");
        return -1;
    }

    if(txBufferLength == 0 || rxBufferLength == 0) {
        LE_INFO("Error txBufferLength: %d rxBufferLength: %d", txBufferLength, rxBufferLength);
        return -2;
    }

    msgs[0].addr = txAddress;
    msgs[0].flags = 0;
    msgs[0].len = txBufferLength;
    msgs[0].buf = txBuffer;

    msgs[1].addr = txAddress;
    msgs[1].flags = I2C_M_RD ;
    msgs[1].len = rxBufferLength;
    msgs[1].buf = rxBuffer;

    msgset[0].msgs = msgs;
    msgset[0].nmsgs = 2;

    if ((ret = ioctl(fd, I2C_RDWR, &msgset)) < 0) {
        LE_INFO("Error: %s", strerror(errno));
        LE_INFO("I2C_RDWR in i2c_read, address: %x ret: %d", txAddress, ret);
        return -1;
    }

    if(msgs[1].len != rxBufferLength)
    	LE_INFO("i2c_read FAILED, rxBufferLength: %d msgs[1].len: %d",
		rxBufferLength, (int) msgs[1].len);
    // else
    	// LE_INFO("i2c_read SUCCESS, rxBufferLength: %d msgs[1].len: %d",
		 // rxBufferLength, (int) msgs[1].len);

    return (int) msgs[1].len;
}

// Write a block/register to the NTAG
int ArduinoWire::i2c_write() {
    int ret;
    struct i2c_msg msgs[1];
    struct i2c_rdwr_ioctl_data msgset[1];

    if(txAddress == 0) {
	LE_INFO("I2c address not set");
	return -1;
    }

    msgs[0].addr = txAddress;
    msgs[0].flags = 0;

    // Could remove as endTransmission has the same check
    if(txBufferLength == 0) {
	LE_INFO("Error txBufferLength == 0, nothing to transmit");
	return -2;
    }

    msgs[0].len = txBufferLength;
    msgs[0].buf = txBuffer;

    msgset[0].msgs = msgs;
    msgset[0].nmsgs = 1;

    if ((ret = ioctl(fd, I2C_RDWR, &msgset)) < 0) {
        LE_INFO("Error: %s", strerror(errno));
        LE_INFO("I2C_RDWR in i2c_write, address: %x ret: %d", txAddress, ret);
        return -3;
    }
    // LE_INFO("i2c_write ret returned: %d", ret);
    return 0;
}

