/*
 *  Copyright 2013 Embedded Artists AB
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef AR1021_H
#define AR1021_H

#include <avr/io.h>
#include <stdbool.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "ar1021Hardware.h"
#include "spi_driver.h"
#include "Communication.h"
#include "timer.h"
#include "spiDevice.h"
#include "ledHardware.h"


/******************************************************************************
 * Defines and typedefs
 *****************************************************************************/

#define AR1021_REG_TOUCH_THRESHOLD        (0x02)
#define AR1021_REG_SENS_FILTER            (0x03)
#define AR1021_REG_SAMPLING_FAST          (0x04)
#define AR1021_REG_SAMPLING_SLOW          (0x05)
#define AR1021_REG_ACC_FILTER_FAST        (0x06)
#define AR1021_REG_ACC_FILTER_SLOW        (0x07)
#define AR1021_REG_SPEED_THRESHOLD        (0x08)
#define AR1021_REG_SLEEP_DELAY            (0x0A)
#define AR1021_REG_PEN_UP_DELAY           (0x0B)
#define AR1021_REG_TOUCH_MODE             (0x0C)
#define AR1021_REG_TOUCH_OPTIONS          (0x0D)
#define AR1021_REG_CALIB_INSETS           (0x0E)
#define AR1021_REG_PEN_STATE_REPORT_DELAY (0x0F)
#define AR1021_REG_TOUCH_REPORT_DELAY     (0x11)


#define AR1021_CMD_GET_VERSION                 (0x10)
#define AR1021_CMD_ENABLE_TOUCH                (0x12)
#define AR1021_CMD_DISABLE_TOUCH               (0x13)
#define AR1021_CMD_CALIBRATE_MODE              (0x14)
#define AR1021_CMD_REGISTER_READ               (0x20)
#define AR1021_CMD_REGISTER_WRITE              (0x21)
#define AR1021_CMD_REGISTER_START_ADDR_REQUEST (0x22)
#define AR1021_CMD_REGISTER_WRITE_TO_EEPROM    (0x23)
#define AR1021_CMD_EEPROM_READ                 (0x28)
#define AR1021_CMD_EEPROM_WRITE                (0x29)
#define AR1021_CMD_EEPROM_WRITE_TO_REGISTERS   (0x2B)

#define AR1021_RESP_STAT_OK           (0x00)
#define AR1021_RESP_STAT_CMD_UNREC    (0x01)
#define AR1021_RESP_STAT_HDR_UNREC    (0x03)
#define AR1021_RESP_STAT_TIMEOUT      (0x04)
#define AR1021_RESP_STAT_CANCEL_CALIB (0xFC)


#define AR1021_ERR_NO_HDR      (-1000)
#define AR1021_ERR_INV_LEN     (-1001)
#define AR1021_ERR_INV_RESP    (-1002)
#define AR1021_ERR_INV_RESPLEN (-1003)
#define AR1021_ERR_TIMEOUT     (-1004)

// bit 7 is always 1 and bit 0 defines pen up or down
#define AR1021_PEN_MASK (0x81)

#define AR1021_NUM_CALIB_POINTS (4)


/**
 * Microchip Touch Screen Controller (AR1021).
 *
 * Please note that this touch panel has an on-board storage for
 * calibration data. Once a successful calibration has been performed
 * it is not needed to do additional calibrations since the stored
 * calibration data will be used.
 */
class AR1021 : public spiDevice
{
public:

    typedef struct
    {
        int16_t x;
        int16_t y;
        bool    touched;
    } touchCoordinate_t;


    /**
     * Constructor
     *
     * @param mosi SPI MOSI pin
     * @param miso SPI MISO pin
     * @param sck SPI SCK pin
     * @param cs chip-select pin
     * @param siq interrupt pin
     */
    AR1021(volatile TIMER *timeoutTimer,SPI_INTLVL_t intLevel,bool clk2x,SPI_PRESCALER_t clockDivision)
                          :spiDevice(&AR1021_SPI,&AR1021_SPI_PORT,&AR1021_CS_PORT,AR1021_CS,false,SPI_MODE_1_gc,intLevel,clk2x,clockDivision)
    {
      //_cs = 1; // active low
      _timeoutTimer = timeoutTimer;
      AR1021_INT_PORT.DIRCLR = AR1021_INT_PIN;
      AR1021_CS_PORT.DIRSET = AR1021_CS;
      AR1021_CS_PORT.OUTSET = AR1021_CS;

      //_spi.format(8, 1);
      //_spi.frequency(500000);

      // default calibration inset is 25 -> (25/2 = 12.5%)
      _inset = 25;

      // make sure _calibPoint has an invalid value to begin with
      // correct value is set in calibrateStart()
      _calibPoint = AR1021_NUM_CALIB_POINTS+1;

      actual.x = 0;
      actual.y = 0;
      actual.touched = false;
      _initialized = false;
    }


    void setDebug(Communication *debugCom);
    void debug(const char *text);
    void debug(const char *text,int16_t zahl);

    bool init(uint16_t width, uint16_t height, bool rotated );
    bool read(touchCoordinate_t &coord);
    bool calibrateStart();
    bool getNextCalibratePoint(uint16_t* x, uint16_t* y);
    bool waitForCalibratePoint(bool* morePoints, uint32_t timeout);
    void registerDump();
    void setRegister(uint8_t reg,uint8_t val,uint8_t offset);

    void readTouchIrq(); // war private
    void readTouch();
    bool compareCoord(const touchCoordinate_t& a, const touchCoordinate_t& b);

    touchCoordinate_t actual,lastActual;

private:


    Communication *_debugCom=NULL;
    volatile TIMER *_timeoutTimer;
    //DigitalOut _cs;
    //DigitalIn _siq;
    //InterruptIn _siqIrq;
    bool _initialized;

    uint16_t _width;
    uint16_t _height;
    bool     _rotated=false;
    uint8_t _inset;

    int _calibPoint;


    int cmd(char cmd, char* data, int len, char* respBuf, int* respLen, bool setCsOff=true);
    int waitForCalibResponse(uint32_t timeout);


};

#endif
