/*
 * ArduinoWire.h
 *
 *  Created on: Apr 4, 2016
 *      Author: mario
 */

#ifndef ARDUINOWIRE_H_
#define ARDUINOWIRE_H_

// On Linux I2c is limited to this size by I2C_SMBUS_BLOCK_MAX
#define BUFFER_LENGTH 32

#include <stdint.h>
#include <cstddef>


class ArduinoWire {
private:
	int fd; //File descriptor used to access i2c device

	uint8_t txAddress;
	uint8_t txBuffer[BUFFER_LENGTH];
	int txBufferIndex;
	int txBufferLength;

	uint8_t rxBuffer[BUFFER_LENGTH];
	int rxBufferIndex;
	int rxBufferLength;

	bool releaseBus;

	void becomeBusMaster();

	/* SWI: We have added private member functions for reading
	 * across the I2c that use the low-level message-based
	 * interface of the Linux I2c stack
	 */
	int i2c_read();
	int i2c_write();

public:

	ArduinoWire();

	/**
	 * Opens the i2c device
	 */
	void begin();

	/**
	 * Begin a transmission to the I2C slave device with the given address.
	 * Subsequently, queue bytes for transmission with the write() function and
	 * transmit them by calling endTransmission().
	 *
	 * Input values:
	 * 		@param deviceAddress the address of the i2c slave device
	 */
	void beginTransmission(const uint8_t deviceAddress);

	/**
	 *
	 */
	size_t write(const uint8_t registerAddress);
	size_t write(const uint8_t * registerAddress, size_t size);

	/**
	 * Ends a transmission to the slave device
	 *
	 * Input values:
	 * 		@param releaseDevice True, sends stop message, releasing the bus after transmission. False, sends a restart
	 * 		       keeping the connection active.
	 *
	 * @return
	 * 		byte, which indicates the status of the transmission:
	 *			0:success
	 *			1:data too long to fit in transmit buffer
	 *			2:received NACK on transmit of address
	 *			3:received NACK on transmit of data
	 *			4:other error
	 */
	int endTransmission(uint8_t releaseDevice = true);

	/**
	 * Used to request bytes from a slave device, given by . The bytes may then be retrieved with the available() and read() functions.
	 */
	int requestFrom(int address, int read_len, bool releaseBus = true);
	int available();
	uint8_t read();

	uint8_t* toString() {
		return txBuffer;
	}
};




#endif /* ARDUINOWIRE_H_ */
