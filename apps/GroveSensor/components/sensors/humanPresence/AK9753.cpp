/*    
 * A library for Grove - Human Presence Sensor
 *   
 * Copyright (c) 2018 seeed technology co., ltd.  
 * Author      : Jack Shao  
 * Create Time: June 2018
 * Change Log : 
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "AK9753.h"

/*Global variable */
static const char ak9753_i2c_bus[20] = "/dev/i2c-5"; //< I2C bus
uint8_t buf[32] = {
    0,
};                            //< Buffer store data
size_t buf_len = sizeof(buf); //< Lenght of this buffer

void delay(uint32_t ms){
  usleep(ms*1000);
}

static uint32_t millis(){
  uint32_t millisecond = 0;
    timeval tv;
    gettimeofday(&tv,NULL);
    millisecond = tv.tv_sec *1000000 + tv.tv_usec/1000;
    return millisecond;
}

/*Class AK9753 */
AK9753::AK9753(uint8_t i2c_addr)
{
    m_addr = i2c_addr;
}

bool AK9753::initialize(void)
{
    softReset();

    //check chip id
    if (getDeviceID() != 0x13)
        return false;

    //set mode and filter freq
    setECNTL1((AK975X_FREQ_8_8HZ << 3) | AK975X_MODE_0);

    //enable interrupt
    setEINTEN(0x1f); //enable all interrupts

    return true;
}

int AK9753::readRegs(int reg_addr, uint8_t *reg_data, int len)
{
    int8_t result;
	result = i2cReceiveBytes_v2(ak9753_i2c_bus, m_addr, reg_addr, buf, len);
    if(result != LE_OK)
        LE_ERROR("Read Reg failed:");
    for (int i = 0; i < len; i++)
    {
        reg_data[i] = (uint8_t)buf[i];
    }
    return 0;
}

int AK9753::writeRegs(uint8_t *reg_data, int len)
{
    int8_t result;

    result = i2cSendBytes(ak9753_i2c_bus, m_addr,reg_data,len);
    if(result != LE_OK)
        LE_ERROR("Send byte failed");
    return 0;
}

/**
 * getCompanyCode
 * the code is expected to be 0x48
 */
uint8_t AK9753::getCompanyCode(void)
{
    uint8_t data;

    readRegs(REG_WIA1, &data, 1);
    return (data);
}

/**
 * getDeviceID
 * the ID is expected to be 0x13
 */
uint8_t AK9753::getDeviceID(void)
{
    uint8_t data;
    readRegs(REG_WIA2, &data, 1);
    return (data);
}

bool AK9753::dataReady(void) /* returns ST1[0], read ST2 to clear */
{
    uint8_t data;
    readRegs(REG_ST1, &data, 1);
    return ((data & 0x01) == 0x01);
}

bool AK9753::dataOverRun(void)
{
    uint8_t data;
    readRegs(REG_ST2, &data, 1);
    return ((data & 0x02) == 0x02);
}

uint8_t AK9753::getINTST(void) /** return REG_INTST */
{
    uint8_t data;
    readRegs(REG_INTST, &data, 1);
    return (data);
}

uint8_t AK9753::getST1(void)
{
    uint8_t data;
    readRegs(REG_ST1, &data, 1);
    return (data);
}

int16_t AK9753::getRawIR1(void)
{
    uint8_t data[2];
    int16_t IR;
    readRegs(REG_IR1L, data, 2);
    IR = (data[1] << 8) | data[0];
    return (IR);
}

float AK9753::getIR1(void)
{
    int16_t iValue;
    float fValue;
    iValue = getRawIR1();
    fValue = 14286.8 * iValue / 32768.0;
    return (fValue);
}

int16_t AK9753::getRawIR2(void)
{
    uint8_t data[2];
    int16_t IR;
    readRegs(REG_IR2L, data, 2);
    IR = (data[1] << 8) | data[0];
    return (IR);
}

float AK9753::getIR2(void)
{
    int16_t iValue;
    float fValue;
    iValue = getRawIR2();
    fValue = 14286.8 * iValue / 32768.0;
    return (fValue);
}

int16_t AK9753::getRawIR3(void)
{
    uint8_t data[2];
    int16_t IR;
    readRegs(REG_IR3L, data, 2);
    IR = (data[1] << 8) | data[0];
    return (IR);
}

float AK9753::getIR3(void)
{
    int16_t iValue;
    float fValue;
    iValue = getRawIR3();
    fValue = 14286.8 * iValue / 32768.0;
    return (fValue);
}

int16_t AK9753::getRawIR4(void)
{
    uint8_t data[2];
    int16_t IR;
    readRegs(REG_IR4L, data, 2);
    IR = (data[1] << 8) | data[0];
    return (IR);
}

float AK9753::getIR4(void)
{
    int16_t iValue;
    float fValue;
    iValue = getRawIR4();
    fValue = 14286.8 * iValue / 32768.0;
    return (fValue);
}

int16_t AK9753::getRawTMP(void)
{
    uint8_t data[2];
    int16_t temp;
    readRegs(REG_TMPL, data, 2);
    temp = (data[1] << 8) | data[0];
    return (temp);
}

float AK9753::getTMP(void)
{
    int16_t iValue;
    float temperature;

    iValue = getRawTMP();
    iValue >>= 6; // Temp is 10-bit. TMPL0:5 fixed at 0

    temperature = 26.75 + (iValue * 0.125);

    return (temperature);
}

float AK9753::getTMP_F(void)
{
    float temperature = getTMP();
    temperature = temperature * 1.8 + 32.0;
    return (temperature);
}

uint8_t AK9753::getST2(void)
{
    uint8_t data;
    readRegs(REG_ST2, &data, 1);
    return (data);
}

int16_t AK9753::getETH13H(void)
{
    int16_t value;
    uint8_t data[2];
    readRegs(REG_ETH13H_LSB, data, 2);
    value = (data[1] << 8) | data[0];
    return (value);
}

int16_t AK9753::getETH13L(void)
{
    int16_t value;
    uint8_t data[2];
    readRegs(REG_ETH13L_LSB, data, 2);
    value = (data[1] << 8) | data[0];
    return (value);
}

int16_t AK9753::getETH24H(void)
{
    int16_t value;
    uint8_t data[2];
    readRegs(REG_ETH24H_LSB, data, 2);
    value = (data[1] << 8) | data[0];
    return (value);
}

int16_t AK9753::getETH24L(void)
{
    int16_t value;
    uint8_t data[2];
    readRegs(REG_ETH24L_LSB, data, 2);
    value = (data[1] << 8) | data[0];
    return (value);
}

uint8_t AK9753::getEHYS13(void)
{
    uint8_t data;
    readRegs(REG_EHYS13, &data, 1);
    return (data);
}

uint8_t AK9753::getEHYS24(void)
{
    uint8_t data;
    readRegs(REG_EHYS24, &data, 1);
    return (data);
}

uint8_t AK9753::getEINTEN(void)
{
    uint8_t data;
    readRegs(REG_EINTEN, &data, 1);
    return (data);
}

uint8_t AK9753::getECNTL1(void)
{
    uint8_t data;
    readRegs(REG_ECNTL1, &data, 1);
    return (data);
}

uint8_t AK9753::getCNTL2(void)
{
    uint8_t data;
    readRegs(REG_CNTL2, &data, 1);
    return (data);
}

int16_t AK9753::ETHpAtoRaw(float pA)
{
    int16_t raw = (int16_t)(pA / 3.4877);
    if (raw > 2047)
        raw = 2047;
    if (raw < -2048)
        raw = -2048;
    return raw;
}

void AK9753::setETH13H(int16_t value)
{
    uint8_t data[3];
    data[0] = REG_ETH13H_LSB;
    value <<= 3;
    data[1] = value & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    writeRegs(data, 3);
}

void AK9753::setETH13L(int16_t value)
{
    uint8_t data[3];
    data[0] = REG_ETH13L_LSB;
    value <<= 3;
    data[1] = value & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    writeRegs(data, 3);
}

void AK9753::setETH24H(int16_t value)
{
    uint8_t data[3];
    data[0] = REG_ETH24H_LSB;
    value <<= 3;
    data[1] = value & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    writeRegs(data, 3);
}

void AK9753::setETH24L(int16_t value)
{
    uint8_t data[3];
    data[0] = REG_ETH24L_LSB;
    value <<= 3;
    data[1] = value & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    writeRegs(data, 3);
}

uint8_t AK9753::EHYSpAtoRaw(float pA)
{
    uint16_t raw = (uint16_t)(pA / 3.4877);
    if (raw > 31)
        raw = 31;
    return (uint8_t)raw;
}

void AK9753::setEHYS13(uint8_t value)
{
    uint8_t data[2];
    data[0] = REG_EHYS13;
    data[1] = value;
    writeRegs(data, 2);
}

void AK9753::setEHYS24(uint8_t value)
{
    uint8_t data[2];
    data[0] = REG_EHYS24;
    data[1] = value;
    writeRegs(data, 2);
}

void AK9753::setEINTEN(uint8_t value)
{
    uint8_t data[2];
    data[0] = REG_EINTEN;
    data[1] = value;
    writeRegs(data, 2);
}

void AK9753::setECNTL1(uint8_t value)
{
    uint8_t data[2];
    data[0] = REG_ECNTL1;
    data[1] = value;
    writeRegs(data, 2);
}

void AK9753::softReset(void)
{
    uint8_t data[2] = {REG_CNTL2, 0xFF};
    writeRegs(data, 2);
}

void AK9753::startNextSample(void)
{
    getST2();
}

/* Class PresenceDetector */
PresenceDetector::PresenceDetector(
    AK9753 &sensor,
    float threshold_presence,
    float threshold_movement,
    int detect_interval) : m_interval(detect_interval),
                           m_threshold_presence(threshold_presence),
                           m_threshold_movement(threshold_movement)
{
    m_sensor = &sensor;

    for (int i = 0; i < NUM_SMOOTHER; i++)
    {
        m_smoothers[i] = new Smoother(0.05); //0.3 very steep, 0.1 less steep, 0.05 less steep
    }
    m_last_time = millis();
    memset(m_presences, 0, sizeof(m_presences));
    m_movement = MOVEMENT_NONE;
}

PresenceDetector::~PresenceDetector()
{
    //delete[] m_smoothers;
}


void PresenceDetector::loop()
{
    float ir1, ir2, ir3, ir4, diff13, diff24;
    uint32_t now = millis();

    if (!m_sensor->dataReady())
        return;

    ir1 = m_sensor->getIR1();
    ir2 = m_sensor->getIR2();
    ir3 = m_sensor->getIR3();
    ir4 = m_sensor->getIR4();
    diff13 = ir1 - ir3;
    diff24 = ir2 - ir4;
    m_sensor->startNextSample();

    m_smoothers[0]->addDataPoint(ir1);
    m_smoothers[1]->addDataPoint(ir2);
    m_smoothers[2]->addDataPoint(ir3);
    m_smoothers[3]->addDataPoint(ir4);
    m_smoothers[4]->addDataPoint(diff13);
    m_smoothers[5]->addDataPoint(diff24);

    if (now - m_last_time > (uint32_t)m_interval)
    {
        float d;
        for (int i = 0; i < 4; i++)
        {
            d = m_ders[i] = m_smoothers[i]->getDerivative();

            if (d > m_threshold_presence)
            {
                m_presences[i] = true;
            }
            else if (d < (-m_threshold_presence))
            {
                m_presences[i] = false;
            }
        }

        d = m_der13 = m_smoothers[4]->getDerivative();

        if (d > m_threshold_movement)
        {
            m_movement &= 0b11111100;
            m_movement |= MOVEMENT_FROM_3_TO_1;
        }
        else if (d < (-m_threshold_movement))
        {
            m_movement &= 0b11111100;
            m_movement |= MOVEMENT_FROM_1_TO_3;
        }

        d = m_der24 = m_smoothers[5]->getDerivative();
        if (d > m_threshold_movement)
        {
            m_movement &= 0b11110011;
            m_movement |= MOVEMENT_FROM_4_TO_2;
        }
        else if (d < (-m_threshold_movement))
        {
            m_movement &= 0b11110011;
            m_movement |= MOVEMENT_FROM_2_TO_4;
        }

        m_last_time = now;
    }
}

bool PresenceDetector::presentField1()
{
    bool r = m_presences[0];
    m_presences[0] = false;
    return r;
}

bool PresenceDetector::presentField2()
{
    bool r = m_presences[1];
    m_presences[1] = false;
    return r;
}

bool PresenceDetector::presentField3()
{
    bool r = m_presences[2];
    m_presences[2] = false;
    return r;
}

bool PresenceDetector::presentField4()
{
    bool r = m_presences[3];
    m_presences[3] = false;
    return r;
}
bool PresenceDetector::presentFullField(bool clear)
{
    if (clear)
    {
        return presentField1() || presentField2() || presentField3() || presentField4();
    }
    else
    {
        return m_presences[0] || m_presences[1] || m_presences[2] || m_presences[3];
    }
}

float PresenceDetector::getDerivativeOfIR1()
{
    return m_ders[0];
}

float PresenceDetector::getDerivativeOfIR2()
{
    return m_ders[1];
}

float PresenceDetector::getDerivativeOfIR3()
{
    return m_ders[2];
}

float PresenceDetector::getDerivativeOfIR4()
{
    return m_ders[3];
}

uint8_t PresenceDetector::getMovement()
{
    uint8_t r = m_movement;
    m_movement = MOVEMENT_NONE;
    return r;
}

float PresenceDetector::getDerivativeOfDiff13()
{
    return m_der13;
}

float PresenceDetector::getDerivativeOfDiff24()
{
    return m_der24;
}
