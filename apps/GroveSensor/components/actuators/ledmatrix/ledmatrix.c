#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/io.h>
#include <stdio.h>
#include <errno.h>

#include "i2cUtils.h"
#include "legato.h"
#include "interfaces.h"
#include "le_mutex.h"
#include "json.h"

#define LEDMATRIX_DATAHUB_STRING    "ledmatrix/string"
#define LEDMATRIX_DATAHUB_NUMBER    "ledmatrix/number"
#define LEDMATRIX_DATAHUB_EMOJI     "ledmatrix/emoji"

le_mutex_Ref_t ledmatrix_lock = NULL;


// private variables
uint8_t devAddr;
uint8_t offsetAddress;
uint8_t baseAddress;
uint32_t deviceId[3]; // Unique device ID(96 bits: Low, Middle, High)


const char *led_matrix_i2c_bus = "/dev/i2c-5";


void i2cSendContinueBytes(uint8_t address, uint8_t *data, uint8_t len)
{
    uint8_t *buffer = malloc(len + 1);
    if (buffer == NULL) {
        return;
    }
    buffer[0] = LEDMATRIX_I2C_CMD_CONTINUE_DATA;
    memcpy(buffer + 1, data, len);
    i2cSendBytes(led_matrix_i2c_bus, devAddr, buffer, len + 1);
    free(buffer);
}

/**
 * Description
 *  Get vendor ID of device.
 * Parameter
 *  Null.
 * Return
 *  Return vendor ID of device.
 **/
uint16_t ledmatrix_getDeviceVID(void)
{
    uint8_t data[4] = {0, };
    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_GET_DEV_ID);
    i2cReceiveBytes(led_matrix_i2c_bus, devAddr, data, 4);
    return (uint16_t)(data[0] + data[1] * 256);
}

/**
 * Description
 *  Get product ID of device.
 * Parameter
 *  Null.
 * Return
 *  Return product ID of device.
 **/
uint16_t ledmatrix_getDevicePID(void)
{
    uint8_t data[4] = {0, };
    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_GET_DEV_ID);
    i2cReceiveBytes(led_matrix_i2c_bus, devAddr, data, 4);
    return (uint16_t)(data[2] + data[3] * 256);
}

/**
 * Description
 *  Change i2c base address of device.
 * Parameter
 *  newAddress: 0x10-0x70, The new i2c base address of device.
 * Return
 *  Null.
 **/
void ledmatrix_changeDeviceBaseAddress(uint8_t newAddress)
{
    uint8_t data[2] = {0, };
    if (!(newAddress >= 0x10 && newAddress <= 0x70)) {
        newAddress = LEDMATRIX_GROVE_TWO_RGB_LED_MATRIX_DEF_I2C_ADDR;
    }
    data[0] = LEDMATRIX_I2C_CMD_SET_ADDR;
    data[1] = newAddress;
    baseAddress = newAddress;
    i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 2);
    devAddr = baseAddress + offsetAddress;
    sleep(0.2);
}

/**
 * Description
 *  Restore the i2c address of device to default.
 * Parameter
 *  Null.
 * Return
 *  Null.
 **/
void ledmatrix_defaultDeviceAddress(void)
{
    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_RST_ADDR);
    baseAddress = LEDMATRIX_GROVE_TWO_RGB_LED_MATRIX_DEF_I2C_ADDR;
    devAddr = baseAddress + offsetAddress;
    sleep(0.2);
}

/**
 * Description
 *  Turn on the indicator LED flash mode.
 * Parameter
 *  Null.
 * Return
 *  Null.
 **/
void ledmatrix_turnOnLEDFlash(void)
{
    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_LED_ON);
}

/**
 * Description
 *  Turn off the indicator LED flash mode.
 * Parameter
 *  Null.
 * Return
 *  Null.
 **/
void ledmatrix_turnOffLEDFlash(void)
{
    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_LED_OFF);
}

/**
 * Description
 *  Enable device auto sleep mode. Send any I2C commands will 
 * wake up all the sleepy devices.
 * Parameter
 *  Null.
 * Return
 *  Null.
 **/
void ledmatrix_enableAutoSleep(void)
{
    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_AUTO_SLEEP_ON);
}

/**
 * Description
 *  Don't need this function anymore.
 *  (Wake device from sleep mode. It takes about 0.15ms.)
 * Parameter
 *  Null.
 * Return
 *  Null.
 **/
void ledmatrix_wakeDevice(void)
{
    usleep(200);
}

/**
 * Description
 *  Disable device auto sleep mode.
 * Parameter
 *  Null.
 * Return
 *  Null.
 **/
void ledmatrix_disableAutoSleep(void)
{
    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_AUTO_SLEEP_OFF);
}

/**
 * Description
 *  Setting the display orientation.
 *  This function can be used before or after display.
 *  DO NOT WORK with displayColorWave(), displayClockwise(), displayColorAnimation()
 * Parameter
 *  orientation: DISPLAY_ROTATE_0, DISPLAY_ROTATE_90, DISPLAY_ROTATE_180,
 * DISPLAY_ROTATE_270, which means the display will rotate 0°, 90°,180° or 270°.
 * Return
 *  Null.
 **/
void ledmatrix_setDisplayOrientation(ledmatrix_orientation_type_t orientation)
{
    uint8_t data[2] = {0, };
    data[0] = LEDMATRIX_I2C_CMD_DISP_ROTATE;
    data[1] = (uint8_t)orientation;
    i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 2);
}

/**
 * Description
 *  Setting the display offset of x-axis and y-axis.
 *  This function can be used before or after display.
 *  DO NOT WORK with displayColorWave(), displayClockwise(), displayColorAnimation(),
 *  displayNumber(when number<0 or number>=10), displayString(when more than one character)
 * Parameter
 *  offset_x: The display offset value of horizontal x-axis, range from -8 to 8.
 *  offset_y: The display offset value of horizontal y-axis, range from -8 to 8.
 * Return
 *  Null.
 **/
void ledmatrix_setDisplayOffset(int32_t offset_x, int32_t offset_y)
{
    // convert to positive
    offset_x +=8;
    offset_y +=8;

    if (offset_x < 0)
        offset_x = 0;
    else if (offset_x > 16)
        offset_x = 16;

    if (offset_y < 0)
        offset_y = 0;
    else if (offset_y > 16)
        offset_y = 16;

    uint8_t data[3] = {0, };
    data[0] = LEDMATRIX_I2C_CMD_DISP_OFFSET;
    data[1] = (uint8_t)offset_x;
    data[2] = (uint8_t)offset_y;
    i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 3);
}

/**
 * Description
 *  Display a bar on RGB LED Matrix.
 * Parameter
 *  bar: 0 - 32. 0 is blank and 32 is full.
 *  duration_time: Set the display time(ms) duration. Set it to 0 to not display.
 *  forever_flag: Set it to true to display forever, and the duration_time will not work.
 *            Or set it to false to display one time.
 *  color: Set the color of the display, range from 0 to 255. See COLORS for more details.
 * Return
 *  Null.
 **/
void ledmatrix_displayBar(uint8_t bar,
              uint16_t duration_time,
              bool forever_flag,
              uint8_t color)
{
    uint8_t data[6] = {0, };

    data[0] = LEDMATRIX_I2C_CMD_DISP_BAR;
    if (bar > 32) 
        bar = 32;
    data[1] = bar;
    data[2] = (uint8_t)(duration_time & 0xff);
    data[3] = (uint8_t)((duration_time >> 8) & 0xff);
    data[4] = forever_flag;
    data[5] = color;
    i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 6);
}

/**
 * Description
 *  Display a colorful bar on RGB LED Matrix.
 * Parameter
 *  bar: 0 - 32. 0 is blank and 32 is full.
 *  duration_time: Set the display time(ms) duration. Set it to 0 to not display.
 *  forever_flag: Set it to true to display forever, and the duration_time will not work.
 *            Or set it to false to display one time.
 * Return
 *    Null.
 **/
void ledmatrix_displayColorBar(uint8_t bar,
                   uint16_t duration_time,
                   bool forever_flag)
{
    uint8_t data[5] = {0, };
    data[0] = LEDMATRIX_I2C_CMD_DISP_COLOR_BAR;
    if (bar > 32)
        bar = 32;
    data[1] = bar;
    data[2] = (uint8_t)(duration_time & 0xff);
    data[3] = (uint8_t)((duration_time >> 8) & 0xff);
    data[4] = forever_flag;
    i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 5);
}

/**
 * Description
 *  Display a wave on RGB LED Matrix.
 * Parameter
 *  color: Set the color of the display, range from 0 to 255. See COLORS for more details.
 *  duration_time: Set the display time(ms) duration. Set it to 0 to not display.
 *  forever_flag: Set it to true to display forever, and the duration_time will not work.
 *            Or set it to false to display one time.
 * Return
 *    Null.
 **/
void ledmatrix_displayColorWave(uint8_t color,
                uint16_t duration_time,
                bool forever_flag)
{
    uint8_t data[5] = {0, };
    data[0] = LEDMATRIX_I2C_CMD_DISP_COLOR_WAVE;
    data[1] = color;
    data[2] = (uint8_t)(duration_time & 0xff);
    data[3] = (uint8_t)((duration_time >> 8) & 0xff);
    data[4] = forever_flag;
    i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 5);
}

/**
 * Description
 *  Display a clockwise(or anti-clockwise) animation on RGB LED Matrix.
 * Parameter
 *  is_cw: Set it true to display a clockwise animation, while set it false to display a anti-clockwise
 *  is_big: Set it true to display a 8*8 animation, while set it false to display a 4*4 animation
 *  duration_time: Set the display time(ms) duration. Set it to 0 to not display.
 *  forever_flag: Set it to true to display forever, and the duration_time will not work.
 *            Or set it to false to display one time.
 * Return
 *  Null.
 **/
void ledmatrix_displayClockwise(bool is_cw,
                bool is_big,
                uint16_t duration_time,
                bool forever_flag)
{
    uint8_t data[6] = {0, };
    data[0] = LEDMATRIX_I2C_CMD_DISP_COLOR_CLOCKWISE;
    data[1] = is_cw;
    data[2] = is_big;
    data[3] = (uint8_t)(duration_time & 0xff);
    data[4] = (uint8_t)((duration_time >> 8) & 0xff);
    data[5] = forever_flag;
    i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 6);
}

/**
 * Description
 *  Display other built-in animations on RGB LED Matrix.
 * Parameter
 *  index: the index of animations, 
 *          0. big clockwise 
 *          1. small clockwise
 *          2. rainbow cycle
 *          3. fire
 *          4. walking child
 *          5. broken heart
 *    duration_time: Set the display time(ms) duration. Set it to 0 to not display.
 *  forever_flag: Set it to true to display forever, and the duration_time will not work.
 *            Or set it to false to display one time.
 * Return
 *  Null.
 **/
void ledmatrix_displayColorAnimation(uint8_t index,
                     uint16_t duration_time,
                     bool forever_flag)
{
    uint8_t data[6] = {0, };
    uint8_t from = 0;
    uint8_t to = 0;
    data[0] = LEDMATRIX_I2C_CMD_DISP_COLOR_ANIMATION;
    switch(index) {
    case 0:
        from = 0;
        to = 28;
        break;
    case 1:
        from = 29;
        to = 41;
        break;
    case 2:             // rainbow cycle
        from = 255;
        to = 255;
        break;
    case 3:             // fire 
        from = 254;
        to = 254;
        break;
    case 4:             // walking
        from = 42;
        to = 43;
        break;
    case 5:             // broken heart
        from = 44;
        to = 52;
        break;
    default:
        break;
    }
    data[1] = from;
    data[2] = to;
    data[3] = (uint8_t)(duration_time & 0xff);
    data[4] = (uint8_t)((duration_time >> 8) & 0xff);
    data[5] = forever_flag;
    i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 6);
}

/**
 * Description
 *  Display emoji on LED matrix.
 * Parameter
 *  emoji: Set a number from 0 to 29 for different emoji.   
 *      0   smile   10  heart       20  house
 *      1   laugh   11  small heart 21  tree
 *      2   sad 12  broken heart    22  flower
 *      3   mad 13  waterdrop   23  umbrella
 *      4   angry   14  flame       24  rain
 *      5   cry 15  creeper     25  monster
 *      6   greedy  16  mad creeper 26  crab
 *      7   cool    17  sword       27  duck
 *      8   shy 18  wooden sword    28  rabbit
 *      9   awkward 19  crystal sword   29  cat
 *      30  up  31  down        32 left
 *      33  right   34  smile face 3
 *  duration_time: Set the display time(ms) duration. Set it to 0 to not display.
 *  forever_flag: Set it to true to display forever, and the duration_time will not work. 
 *            Or set it to false to display one time.
 * Return
 *  Null.
 **/
void ledmatrix_displayEmoji(uint8_t emoji,
                uint16_t duration_time,
                bool forever_flag)
{
    uint8_t data[5] = {0, };

    data[0] = LEDMATRIX_I2C_CMD_DISP_EMOJI;
    data[1] = emoji;
    data[2] = (uint8_t)(duration_time & 0xff);
    data[3] = (uint8_t)((duration_time >> 8) & 0xff);
    data[4] = forever_flag;
    i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 5);
}

/**
 * Description
 *  Display a number(-32768 ~ 32767) on LED matrix.
 * Parameter
 *  number: Set the number you want to display on LED matrix. Number(except 0-9)
 *      will scroll horizontally, the shorter you set the duration time, 
 *      the faster it scrolls. The number range from -32768 to +32767, if 
 *      you want to display larger number, please use displayString().
 *  duration_time: Set the display time(ms) duration. Set it to 0 to not display.
 *  forever_flag: Set it to true to display forever, or set it to false to display one time.
 *  color: Set the color of the display, range from 0 to 255. See COLORS for more details.
 * Return
 *  Null.
 **/
void ledmatrix_displayNumber(int16_t number,
                 uint16_t duration_time,
                 bool forever_flag,
                 uint8_t color)
{
    uint8_t data[7] = {0, };

    data[0] = LEDMATRIX_I2C_CMD_DISP_NUM;
    data[1] = (uint8_t)((uint16_t)number & 0xff);
    data[2] = (uint8_t)(((uint16_t)number >> 8) & 0xff);
    data[3] = (uint8_t)(duration_time & 0xff);
    data[4] = (uint8_t)((duration_time >> 8) & 0xff);
    data[5] = forever_flag;
    data[6] = color;
    i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 7);
}

/**
 * Description
 *  Display a string on LED matrix.
 * Parameter
 *  str: The string pointer, the maximum length is 28 bytes. String will 
 *       scroll horizontally when its length is more than 1. The shorter 
 *       you set the duration time, the faster it scrolls.
 *  duration_time: Set the display time(ms) duration. Set it to 0 to not display.
 *  forever_flag: Set it to true to display forever, or set it to false to display one time.
 *  color: Set the color of the display, range from 0 to 255. See COLORS for more details.
 * Return
 *  Null.
 **/
void ledmatrix_displayString(const char* LE_NONNULL str,
                 uint16_t duration_time,
                 bool forever_flag,
                 uint8_t color)
{
    uint8_t data[36] = {0, };
    uint8_t len = strlen(str);

    if (len >= 28)
        len = 28;
    for (uint8_t i = 0; i < len; i++)
        data[i + 6] = str[i];
    data[0] = LEDMATRIX_I2C_CMD_DISP_STR;
    data[1] = forever_flag;
    data[2] = (uint8_t)(duration_time & 0xff);
    data[3] = (uint8_t)((duration_time >> 8) & 0xff);
    data[4] = len;
    data[5] = color;
    if (len > 25) {
        i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 31);
        usleep(1000); // 1ms
        i2cSendContinueBytes(devAddr, data+31, (len - 25));
    } else {
        i2cSendBytes(led_matrix_i2c_bus, devAddr, data, (len + 6));
    }
}

/**
 * Description
 *  Display user-defined frames on LED matrix.
 * Parameter
 *  buffer: The data pointer. 1 frame needs 64bytes data. Frames will switch
 *      automatically when the frames_number is larger than 1. The shorter 
 *      you set the duration_time, the faster it switches.
 *  duration_time: Set the display time(ms) duration. Set it to 0 to not display.
 *  forever_flag: Set it to true to display forever, or set it to false to display one time.
 *  frames_number: the number of frames in your buffer. Range from 1 to 5.
 * Return
 *  Null.
 **/
void ledmatrix_displayFrames(const uint8_t* bufferPtr,
                 size_t bufferSize,
                 uint16_t duration_time,
                 bool forever_flag,
                 uint8_t frames_number)
{
    uint8_t data[72] = {0, };
    // max 5 frames in storage
    if (frames_number > 5)
        frames_number = 5;
    else if (frames_number == 0)
        return;
    if (bufferSize < 64 * frames_number) {
        LE_ERROR("ledmatrix_displayFrames: buffer is too small\n");
        return;
    }
    data[0] = LEDMATRIX_I2C_CMD_DISP_CUSTOM;
    data[1] = 0x0;
    data[2] = 0x0;
    data[3] = 0x0;
    data[4] = frames_number;
    for (int i = frames_number-1; i >= 0; i--) {
        data[5] = i;
        for (int j = 0; j < 64; j++)
            data[8+j] = bufferPtr[j+i*64];
        if (i == 0) {
            // display when everything is finished.
            data[1] = (uint8_t)(duration_time & 0xff);
            data[2] = (uint8_t)((duration_time >> 8) & 0xff);
            data[3] = forever_flag;
        }
        i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 24);
        usleep(1000);
        i2cSendContinueBytes(devAddr, data+24, 24);
        usleep(1000);
        i2cSendContinueBytes(devAddr, data+48, 24);
    }
}
void ledmatrix_displayFrames64(const uint64_t* bufferPtr,
                   size_t bufferSize,
                   uint16_t duration_time,
                   bool forever_flag,
                   uint8_t frames_number)
{
    uint8_t data[72] = {0, };
    // max 5 frames in storage

    if (frames_number > 5)
        frames_number = 5;
    else if (frames_number == 0)
        return;
    if (bufferSize < 8 * frames_number) {
        LE_ERROR("ledmatrix_displayFrames64: buffer is too small\n");
        return;
    }
    data[0] = LEDMATRIX_I2C_CMD_DISP_CUSTOM;
    data[1] = 0x0;
    data[2] = 0x0;
    data[3] = 0x0;
    data[4] = frames_number;

    for (int i = frames_number - 1; i >= 0; i--) {
        data[5] = i;
        // different from uint8_t buffer
        for (int j = 0; j < 8; j++) {
            for (int k = 7; k >= 0; k--) {
                data[8+j*8+(7-k)] = ((uint8_t *)bufferPtr)[j*8+k+i*64];
            }
        }
        if (i == 0) {
            // display when everything is finished.
            data[1] = (uint8_t)(duration_time & 0xff);
            data[2] = (uint8_t)((duration_time >> 8) & 0xff);
            data[3] = forever_flag;
        }
        i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 24);
        usleep(1000);
        i2cSendContinueBytes(devAddr, data+24, 24);
        usleep(1000);
        i2cSendContinueBytes(devAddr, data+48, 24);
    }
}

/**
 * Description
 *  Display color block on LED matrix with a given uint32_t rgb color.
 * Parameter
 *  rgb: uint32_t rgb color, such as 0xff0000(red), 0x0000ff(blue)
 *  duration_time: Set the display time(ms) duration. Set it to 0 to not display.
 *  forever_flag: Set it to true to display forever, or set it to false to display one time.
 * Return
 *  Null.
 **/
void ledmatrix_displayColorBlock(uint32_t rgb,
                 uint16_t duration_time,
                 bool forever_flag)
{
    uint8_t data[7] = {0, };
    data[0] = LEDMATRIX_I2C_CMD_DISP_COLOR_BLOCK;
    // red green blue
    data[1] = (uint8_t)((rgb >> 16) & 0xff);
    data[2] = (uint8_t)((rgb >> 8) & 0xff);
    data[3] = (uint8_t)(rgb & 0xff);
    data[4] = (uint8_t)(duration_time & 0xff);
    data[5] = (uint8_t)((duration_time >> 8) & 0xff);
    data[6] = forever_flag;
    i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 7);
}

/**
 * Description
 *  Display nothing on LED Matrix.
 * Parameter
 *  Null.
 * Return
 *  Null.
 **/
void ledmatrix_stopDisplay(void)
{
    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_DISP_OFF);
}

/**
 * Description
 *  Store the frames(you send to the device) to flash. It takes about 200ms.
 * Parameter
 *  Null.
 * Return
 *  Null.
 **/
void ledmatrix_storeFrames(void)
{
    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_STORE_FLASH);
    usleep(200000); // 200ms
}

/**
 * Description
 *  Delete all the frames in the flash of device. It takes about 200ms.
 * Parameter
 *  Null.
 * Return
 *  Null.
 **/
void ledmatrix_deleteFrames(void)
{
    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_DELETE_FLASH);
    usleep(200000); // 200ms
}

/**
 * Description
 *  Display frames which is stored in the flash of device.
 * Parameter
 *  duration_time: Set the display time(ms) duration. Set it to 0 to not display. If there
 *             are more than 1 frame to display, frames will switch automatically. The  
 *             shorter you set the duration_time, the faster it switches.
 *  forever_flag: Set it to true to display forever, or set it to false to display one time.
 *  from: the index of frames in your flash. Range from 1 to 5. 
 *  to: the index of frames in your flash. Range from 1 to 5.
 * Return
 *  Null.
 **/
void ledmatrix_displayFramesFromFlash(uint16_t duration_time,
                      bool forever_flag,
                      uint8_t from,
                      uint8_t to)
{
    uint8_t data[6] = {0, };
    uint8_t temp = 0;

    // 1 <= from <= to <= 5
    if (from < 1)
        from = 1;
    else if (from > 5)
        from =5;
    if (to < 1)
        to = 1;
    else if (to > 5)
        to = 5;

    if (from > to) {
        temp = from;
        from = to;
        to = temp;
    }
    data[0] = LEDMATRIX_I2C_CMD_DISP_FLASH;
    data[1] = (uint8_t)(duration_time & 0xff);
    data[2] = (uint8_t)((duration_time >> 8) & 0xff);
    data[3] = forever_flag;
    data[4] = from-1;
    data[5] = to-1;
    i2cSendBytes(led_matrix_i2c_bus, devAddr, data, 6);

    usleep(200000); // 200ms
}

/**
 * Description
 *  Enable TX and RX pin test mode.
 * Parameter
 *  Null.
 * Return
 *  Null.
 **/
void ledmatrix_enableTestMode(void)
{
    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_TEST_TX_RX_ON);
}

/**
 * Description
 *  Enable TX and RX pin test mode.
 * Parameter
 *  Null.
 * Return
 *  Null.
 **/
void ledmatrix_disableTestMode(void)
{
    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_TEST_TX_RX_OFF);
}

/**
 * Description
 *  Get software vresion.
 * Parameter
 *  Null.
 * Return
 *  Return software version.
 **/
uint32_t ledmatrix_getTestVersion(void)
{
    uint8_t data[3] = {0, };

    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_TEST_GET_VER);
    i2cReceiveBytes(led_matrix_i2c_bus, devAddr, data, 3);
    return (uint32_t)(data[2] + (uint32_t)data[1] * 256 + (uint32_t)data[0] * 256 * 256);
}

/**
 * Description
 *  Reset device.
 * Parameter
 *  Null.
 * Return
 *  Null.
 **/
void ledmatrix_resetDevice(void)
{

}

/**
 * Description
 *  Get device id.
 * Parameter
 *  Null.
 * Return
 *  Null.

 **/
void ledmatrix_getDeviceId(uint8_t* deviceIdPtr, size_t* deviceIdSizePtr)
{
    i2cSendByte(led_matrix_i2c_bus, devAddr, LEDMATRIX_I2C_CMD_GET_DEVICE_UID);
    i2cReceiveBytes(led_matrix_i2c_bus, devAddr, (uint8_t *)deviceId, 12);
    memcpy(deviceIdPtr, deviceId, 12);
    *deviceIdSizePtr = 12;
}

void ledmatrix_init(uint8_t base, uint8_t screenNumber)
{
    if (!(screenNumber >= 1 && screenNumber <= 16))
        screenNumber = 1;
    if (!(base >= 0x10 && base <= 0x70))
        base = LEDMATRIX_GROVE_TWO_RGB_LED_MATRIX_DEF_I2C_ADDR;

    offsetAddress = screenNumber - 1;
    baseAddress = base;
    devAddr = offsetAddress + baseAddress;
}

void ledmatrix_deinit(void)
{
}

static void json_extract_dump(le_result_t res)
{
    if (res == LE_OK) {
        LE_INFO("json_Extract: successful");
    }
    if (res == LE_FORMAT_ERROR) {
        LE_ERROR("json_Extract: there's something wrong with the input JSON string.");
    }
    if (res == LE_BAD_PARAMETER) {
        LE_ERROR("json_Extract: there's something wrong with the extraction specification");
    }
    if (res == LE_NOT_FOUND) {
        LE_ERROR("json_Extract: the thing we are trying to extract doesn't exist in the JSON input");
    }
    if (res == LE_OVERFLOW) {
        LE_ERROR("json_Extract: the provided result buffer isn't big enough");
    }
    // LE_ASSERT_OK(res);
}
static void ledmatrixStringHandle(double timestamp,
                  const char* LE_NONNULL value,
                  void* contextPtr)
{
    char buffer[IO_MAX_STRING_VALUE_LEN];
    int duration;
    bool forever;
    int color;
    le_result_t le_res;
    json_DataType_t json_data_type;

    LE_INFO("ledmatrixStringHandle: timestamp %lf", timestamp);
    LE_INFO("ledmatrixStringHandle: value %s", value);

    // JSON message looks like this:
    // "
    // {
    //  \"str\": \"Hello\",
    //  \"duration\": 20000,
    //  \"forever\": true,
    //  \"color\": green
    // }
    // "
    if (!json_IsValid(value)) {
        LE_ERROR("INVALID JSON string");
        return;
    }

    // duration
    memset(buffer, 0, IO_MAX_STRING_VALUE_LEN);
    le_res = json_Extract(buffer,
                  IO_MAX_STRING_VALUE_LEN,
                  value,
                  "duration", 
                  &json_data_type);
    json_extract_dump(le_res);
    if (json_data_type != JSON_TYPE_NUMBER) {
        LE_ERROR("WRONG data type for duration");
        return;
    }
    duration = (int)json_ConvertToNumber(buffer);
    
    // forever
    memset(buffer, 0, IO_MAX_STRING_VALUE_LEN);
    le_res = json_Extract(buffer,
                  IO_MAX_STRING_VALUE_LEN,
                  value,
                  "forever", 
                  &json_data_type);
    json_extract_dump(le_res);
    if (json_data_type != JSON_TYPE_BOOLEAN) {
        LE_ERROR("WRONG data type for forever");
        return;
    }
    forever = json_ConvertToBoolean(buffer);

    // color
    memset(buffer, 0, IO_MAX_STRING_VALUE_LEN);
    le_res = json_Extract(buffer,
                  IO_MAX_STRING_VALUE_LEN,
                  value,
                  "color", 
                  &json_data_type);
    json_extract_dump(le_res);
    if (json_data_type != JSON_TYPE_NUMBER) {
        LE_ERROR("WRONG data type for color");
        return;
    }
    color = (int)json_ConvertToNumber(buffer);

    // str
    memset(buffer, 0, IO_MAX_STRING_VALUE_LEN);
    le_res = json_Extract(buffer,
                  IO_MAX_STRING_VALUE_LEN,
                  value,
                  "str", 
                  &json_data_type);
    json_extract_dump(le_res);
    if (json_data_type != JSON_TYPE_STRING) {
        LE_ERROR("WRONG data type for string");
        return;
    }

    le_mutex_Lock(ledmatrix_lock);
    ledmatrix_displayString(buffer, duration, forever, color);
    le_mutex_Unlock(ledmatrix_lock);
}
static void ledmatrixNumberHandle(double timestamp,
                  const char* LE_NONNULL value,
                  void* contextPtr)
{
    char buffer[IO_MAX_STRING_VALUE_LEN];
    int number;
    int duration;
    bool forever;
    int color;
    le_result_t le_res;
    json_DataType_t json_data_type;

    LE_INFO("ledmatrixNumberHandle: timestamp %lf", timestamp);
    LE_INFO("ledmatrixNumberHandle: value %s", value);

    // JSON message looks like this:
    // "
    //  {
    //      \"number\": number,
    //      \"duration\": 1000,
    //      \"forever\": true,
    //      \"color\": red
    //  }
    // ",
    if (!json_IsValid(value)) {
        LE_ERROR("INVALID JSON string");
        return;
    }

    // duration
    memset(buffer, 0, IO_MAX_STRING_VALUE_LEN);
    le_res = json_Extract(buffer,
                  IO_MAX_STRING_VALUE_LEN,
                  value,
                  "duration", 
                  &json_data_type);
    json_extract_dump(le_res);
    if (json_data_type != JSON_TYPE_NUMBER) {
        LE_ERROR("WRONG data type for duration");
        return;
    }
    duration = (int)json_ConvertToNumber(buffer);
    
    // forever
    memset(buffer, 0, IO_MAX_STRING_VALUE_LEN);
    le_res = json_Extract(buffer,
                  IO_MAX_STRING_VALUE_LEN,
                  value,
                  "forever", 
                  &json_data_type);
    json_extract_dump(le_res);
    if (json_data_type != JSON_TYPE_BOOLEAN) {
        LE_ERROR("WRONG data type for forever");
        return;
    }
    forever = json_ConvertToBoolean(buffer);

    // color
    memset(buffer, 0, IO_MAX_STRING_VALUE_LEN);
    le_res = json_Extract(buffer,
                  IO_MAX_STRING_VALUE_LEN,
                  value,
                  "color", 
                  &json_data_type);
    json_extract_dump(le_res);
    if (json_data_type != JSON_TYPE_NUMBER) {
        LE_ERROR("WRONG data type for color");
        return;
    }
    color = (int)json_ConvertToNumber(buffer);

    // number
    memset(buffer, 0, IO_MAX_STRING_VALUE_LEN);
    le_res = json_Extract(buffer,
                  IO_MAX_STRING_VALUE_LEN,
                  value,
                  "number", 
                  &json_data_type);
    json_extract_dump(le_res);
    if (json_data_type != JSON_TYPE_NUMBER) {
        LE_ERROR("WRONG data type for number");
        return;
    }
    number = (int) json_ConvertToNumber(buffer);

    le_mutex_Lock(ledmatrix_lock);
    ledmatrix_displayNumber(number, duration, forever, color);
    le_mutex_Unlock(ledmatrix_lock);
}
static void ledmatrixEmojiHandle(double timestamp,
                  const char* LE_NONNULL value,
                  void* contextPtr)
{
    char buffer[IO_MAX_STRING_VALUE_LEN];
    int emoji;
    int duration;
    bool forever;
    le_result_t le_res;
    json_DataType_t json_data_type;

    LE_INFO("ledmatrixEmojiHandle: timestamp %lf", timestamp);
    LE_INFO("ledmatrixEmojiHandle: value %s", value);

    // JSON message looks like this:
    // "
    //  {
    //      \"emoji\": emoji,
    //      \"duration\": 1000,
    //      \"forever\": true
    //  }
    // "
    if (!json_IsValid(value)) {
        LE_ERROR("INVALID JSON string");
        return;
    }

    // duration
    memset(buffer, 0, IO_MAX_STRING_VALUE_LEN);
    le_res = json_Extract(buffer,
                  IO_MAX_STRING_VALUE_LEN,
                  value,
                  "duration", 
                  &json_data_type);
    json_extract_dump(le_res);
    if (json_data_type != JSON_TYPE_NUMBER) {
        LE_ERROR("WRONG data type for duration");
        return;
    }
    duration = (int)json_ConvertToNumber(buffer);
    
    // forever
    memset(buffer, 0, IO_MAX_STRING_VALUE_LEN);
    le_res = json_Extract(buffer,
                  IO_MAX_STRING_VALUE_LEN,
                  value,
                  "forever", 
                  &json_data_type);
    json_extract_dump(le_res);
    if (json_data_type != JSON_TYPE_BOOLEAN) {
        LE_ERROR("WRONG data type for forever");
        return;
    }
    forever = json_ConvertToBoolean(buffer);

    // emoji
    memset(buffer, 0, IO_MAX_STRING_VALUE_LEN);
    le_res = json_Extract(buffer,
                  IO_MAX_STRING_VALUE_LEN,
                  value,
                  "emoji", 
                  &json_data_type);
    json_extract_dump(le_res);
    if (json_data_type != JSON_TYPE_NUMBER) {
        LE_ERROR("WRONG data type for number");
        return;
    }
    emoji = (int) json_ConvertToNumber(buffer);

    le_mutex_Lock(ledmatrix_lock);
    ledmatrix_displayEmoji(emoji, duration, forever);
    le_mutex_Unlock(ledmatrix_lock);
}

COMPONENT_INIT
{
    le_result_t result;
    ledmatrix_lock = le_mutex_CreateNonRecursive("ledmatrix");

    // LED matrix initialization
    ledmatrix_init(0, 0);

    // This will be received from the Data Hub.
    result = io_CreateOutput(LEDMATRIX_DATAHUB_STRING,
                 IO_DATA_TYPE_JSON,
                 "string");
    LE_ASSERT(result == LE_OK);
    result = io_CreateOutput(LEDMATRIX_DATAHUB_NUMBER,
                 IO_DATA_TYPE_JSON,
                 "number");
    LE_ASSERT(result == LE_OK);
    result = io_CreateOutput(LEDMATRIX_DATAHUB_EMOJI,
                 IO_DATA_TYPE_JSON,
                 "emoji");
    LE_ASSERT(result == LE_OK);

    // Register for notification of updates to the counter value.
    io_AddJsonPushHandler(LEDMATRIX_DATAHUB_STRING,
                  ledmatrixStringHandle,
                  NULL);
    io_AddJsonPushHandler(LEDMATRIX_DATAHUB_NUMBER,
                  ledmatrixNumberHandle,
                  NULL);
    io_AddJsonPushHandler(LEDMATRIX_DATAHUB_EMOJI,
                  ledmatrixEmojiHandle,
                  NULL);
}
