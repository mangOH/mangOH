#ifndef __GROVE_HUMAN_PRESENCE_SENSOR_H__
#define __GROVE_HUMAN_PRESENCE_SENSOR_H__

#include "legato.h"
#include "interfaces.h"
#include "i2cUtils.h"

#define AK975X_DEFAULT_ADDRESS 0x64

//Register addresses
#define REG_WIA1       0x00
#define REG_WIA2       0x01
#define REG_INFO1      0x02
#define REG_INFO2      0x03
#define REG_INTST      0x04
#define REG_ST1        0x05
#define REG_IR1L       0x06
#define REG_IR1H       0x07
#define REG_IR2L       0x08
#define REG_IR2H       0x09
#define REG_IR3L       0x0A
#define REG_IR3H       0x0B
#define REG_IR4L       0x0C
#define REG_IR4H       0x0D
#define REG_TMPL       0x0E
#define REG_TMPH       0x0F
#define REG_ST2        0x10
#define REG_ETH13H_LSB 0x11
#define REG_ETH13H_MSB 0x12
#define REG_ETH13L_LSB 0x13
#define REG_ETH13L_MSB 0x14
#define REG_ETH24H_LSB 0x15
#define REG_ETH24H_MSB 0x16
#define REG_ETH24L_LSB 0x17
#define REG_ETH24L_MSB 0x18
#define REG_EHYS13     0x19
#define REG_EHYS24     0x1A
#define REG_EINTEN     0x1B
#define REG_ECNTL1     0x1C
#define REG_CNTL2      0x1D
 
/* EEPROM */
#define REG_EKEY          0x50
#define EEPROM_ETH13H_LSB 0x51
#define EEPROM_ETH13H_MSB 0x52
#define EEPROM_ETH13L_LSB 0x53
#define EEPROM_ETH13L_MSB 0x54
#define EEPROM_ETH24H_LSB 0x55
#define EEPROM_ETH24H_MSB 0x56
#define EEPROM_ETH24L_LSB 0x57
#define EEPROM_ETH24L_MSB 0x58
#define EEPROM_EHYS13     0x59
#define EEPROM_EHYS24     0x5A
#define EEPROM_EINTEN     0x5B
#define EEPROM_ECNTL1     0x5C

//Valid sensor modes - Register ECNTL1
#define AK975X_MODE_STANDBY 0b000
#define AK975X_MODE_EEPROM_ACCESS 0b001
#define AK975X_MODE_SINGLE_SHOT 0b010
#define AK975X_MODE_0 0b100
#define AK975X_MODE_1 0b101
#define AK975X_MODE_2 0b110
#define AK975X_MODE_3 0b111

//Valid digital filter cutoff frequencies
#define AK975X_FREQ_0_3HZ 0b000
#define AK975X_FREQ_0_6HZ 0b001
#define AK975X_FREQ_1_1HZ 0b010
#define AK975X_FREQ_2_2HZ 0b011
#define AK975X_FREQ_4_4HZ 0b100
#define AK975X_FREQ_8_8HZ 0b101

//Movement
#define MOVEMENT_NONE        0b0000
#define MOVEMENT_FROM_1_TO_3 0b0001
#define MOVEMENT_FROM_3_TO_1 0b0010
#define MOVEMENT_FROM_2_TO_4 0b0100
#define MOVEMENT_FROM_4_TO_2 0b1000

class AK9753
{
public:
  AK9753(uint8_t i2c_addr = AK975X_DEFAULT_ADDRESS);

  bool initialize(void) ;
  
  uint8_t getCompanyCode(void) ;
  uint8_t getDeviceID(void) ;
  bool    dataReady(void) ; /* returns ST1[0], read ST2 to clear */
  bool    dataOverRun(void) ; /* return ST1[1], read ST2, etc, to clear */
  uint8_t getINTST(void) ; /** return REG_INTST */
  uint8_t getST1(void) ;
  int16_t getRawIR1(void) ;
  float   getIR1(void) ;
  int16_t getRawIR2(void) ;
  float   getIR2(void) ;
  int16_t getRawIR3(void) ;
  float   getIR3(void) ;
  int16_t getRawIR4(void) ;
  float   getIR4(void) ;
  int16_t getRawTMP(void) ;
  float   getTMP(void) ;
  float   getTMP_F(void) ;
  uint8_t getST2(void) ;
  int16_t getETH13H(void) ;
  int16_t getETH13L(void) ;
  int16_t getETH24H(void) ;
  int16_t getETH24L(void) ;
  uint8_t getEHYS13(void) ;
  uint8_t getEHYS24(void) ;
  uint8_t getEINTEN(void) ;
  uint8_t getECNTL1(void) ;
  uint8_t getCNTL2(void) ;
  
  int16_t ETHpAtoRaw(float pA) ;
  void    setETH13H(int16_t value) ;
  void    setETH13L(int16_t value) ;
  void    setETH24H(int16_t value) ;
  void    setETH24L(int16_t value) ;
  uint8_t EHYSpAtoRaw(float pA) ;
  void    setEHYS13(uint8_t value) ;
  void    setEHYS24(uint8_t value) ;
  void    setEINTEN(uint8_t value) ;
  void    setECNTL1(uint8_t value) ;

  void    softReset(void) ;

  /**
   * This is an alias of getST2(), just for friendly name
   */
  void    startNextSample(void) ;
  
 
private:
  uint8_t m_addr;
  int readRegs(int addr, uint8_t *data, int len);
  int writeRegs(uint8_t *data, int len);
};

class Smoother;

#define NUM_SMOOTHER    6

class PresenceDetector
{
public:
  /**
   * @param sensor - the ref of AK9753 class instance
   * @param threshold_presence - compares with the derivative of the readings of a specific IR sensor (1/2/3/4)
   * @param threshold_movement - compares with the derivative of the difference value between IR sensor 1-3 or 2-4
   * @param detect_interval - the interval of the presence detection, unit: millisecond
   */
  PresenceDetector(
    AK9753 &sensor, 
    float threshold_presence = 10, 
    float threshold_movement = 10, 
    int detect_interval = 30);
  ~PresenceDetector();

  /**
   * This is the driven loop of the detector, should call this as fast as possible
   */
  void loop();

  /**
   * if an IR object is in the view of a specific sensor, clear after read
   * @return - true: an IR object is in view (entrance event)
   */
  bool presentField1();
  bool presentField2();
  bool presentField3();
  bool presentField4();

  /**
   * @param clear - clear after read or not
   * @return - true: if an IR object enters any view section of this sensor
   */
  bool presentFullField(bool clear = true);

  float getDerivativeOfIR1();
  float getDerivativeOfIR2();
  float getDerivativeOfIR3();
  float getDerivativeOfIR4();

  /**
   * Read the movement flags, clear after read
   * @return - one/OR of the MOVEMENT_FROM_X_TO_X macro
   */
  uint8_t getMovement();

  float getDerivativeOfDiff13();
  float getDerivativeOfDiff24();


private:
  AK9753 *m_sensor;
  Smoother *m_smoothers[NUM_SMOOTHER];
  int m_interval;
  uint32_t m_last_time;
  
  bool m_presences[4];
  uint8_t m_movement;

  float m_threshold_presence, m_threshold_movement;
  float m_ders[4];
  float m_der13, m_der24;
};


class Smoother
{
public:
    Smoother(float average_weight)
    {
        m_average_weight = average_weight;
        m_last_marked_value = 0;
        m_average = 0;
    }

    void addDataPoint(float data)
    {
        m_average = m_average_weight * data + (1 - m_average_weight) * m_average;
    }

    float getSmoothValue()
    {
        return m_average;
    }

    float getDerivative()
    {
        float d = m_average - m_last_marked_value;
        m_last_marked_value = m_average;
        return d;
    }

private:
    float m_last_marked_value;
    float m_average_weight;
    float m_average;
};
#endif