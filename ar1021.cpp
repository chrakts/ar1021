

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

/******************************************************************************
 * Includes
 *****************************************************************************/

#include "ar1021.h"


/*
AR1021::AR1021(volatile TIMER *timeoutTimer,)
{
  //_cs = 1; // active low
  _spiAR1021 = spiAR1021;
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

    _x = 0;
    _y = 0;
    _pen = 0;

    _initialized = false;
}
*/

void AR1021::setDebug(Communication *debugCom)
{
  _debugCom = debugCom;
}

void AR1021::debug(const char *text)
{
  if(_debugCom!=NULL)
    _debugCom->sendInfo(text,"BR");
}

void AR1021::debug(const char *text,int16_t zahl)
{
  if(_debugCom!=NULL)
  {
    char gesamtText[60];
    sprintf(gesamtText,text,zahl);
    _debugCom->sendInfo(gesamtText,"BR");
  }
}

bool AR1021::read(touchCoordinate_t &coord)
{

  if (!_initialized) return false;

  //coord.x = (actual.x * _width)/4095;
  //coord.y = (actual.y * _height)/4095;
  coord.x = actual.x;
  coord.y = actual.y;
  coord.touched = actual.touched;
  if( compareCoord(actual,lastActual) )
    return false;
  else
  {
    lastActual.x = actual.x;
    lastActual.y = actual.y;
    lastActual.touched = actual.touched;
    return true;
  }
}


bool AR1021::init(uint16_t width, uint16_t height, bool rotated = false)
{
    int result = 0;
    bool ok = false;
    int attempts = 0;

    _width = width;
    _height = height;
    _rotated = rotated;
    while (1) {

        do {
            // disable touch
            result = cmd(AR1021_CMD_DISABLE_TOUCH, NULL, 0, NULL, 0);
            if (result == -AR1021_RESP_STAT_CANCEL_CALIB) {
                debug("calibration was cancelled, short delay and try again");
                _delay_us(50);
                result = cmd(AR1021_CMD_DISABLE_TOUCH, NULL, 0, NULL, 0);
            }
            if (result != 0) {
                debug("disable touch failed (%d)", result);
                break;
            }
            _delay_us(50);

            char regOffset = 0;
            int regOffLen = 1;
            result = cmd(AR1021_CMD_REGISTER_START_ADDR_REQUEST, NULL, 0,
                    &regOffset, &regOffLen);
            if (result != 0) {
                debug("register offset request failed (%d)", result);
                break;
            }
            // enable calibrated coordinates
            //                  high, low address,                        len,  value
            // char toptions[4] = {0x00, AR1021_REG_TOUCH_OPTIONS+regOffset, 0x01, 0x01};
            setRegister(AR1021_REG_TOUCH_OPTIONS,0,regOffset);
            setRegister(AR1021_REG_SENS_FILTER,0x04,regOffset);
            setRegister(AR1021_REG_TOUCH_THRESHOLD,0xc5,regOffset);

            // save registers to eeprom
            result = cmd(AR1021_CMD_REGISTER_WRITE_TO_EEPROM, NULL, 0, NULL, 0);
            if (result != 0) {
                debug("register write to eeprom failed (%d)", result);
                break;
            }

            // enable touch
            result = cmd(AR1021_CMD_ENABLE_TOUCH, NULL, 0, NULL, 0);
            if (result != 0) {
                debug("enable touch failed (%d)", result);
                break;
            }

            //_siqIrq.rise(this, &AR1021::readTouchIrq);

            _initialized = true;
            ok = true;

        } while(0);

        if (ok) break;

        // try to run the initialize sequence at most 2 times
        if(++attempts >= 2) break;
    }


    return ok;
}

void  AR1021::setRegister(uint8_t reg,uint8_t val,uint8_t offset)
{
char toptions[4] = {0x00, reg+offset, 0x01, val};
  int result = cmd(AR1021_CMD_REGISTER_WRITE, toptions, 4, NULL, 0);
  if (result != 0)
      debug("register write request failed (%d)", result);
}

void  AR1021::registerDump()
{
int result = 0;
char myResp[4];
int myNum;
char regOffset = 0;
int regOffLen = 1;

  result = cmd(AR1021_CMD_REGISTER_START_ADDR_REQUEST, NULL, 0,&regOffset, &regOffLen);
  if (result != 0)
    debug("register offset request failed (%d)", result);
  debug("offset: %x",regOffset);

  myNum = 3;
  result = cmd(AR1021_CMD_GET_VERSION,NULL,0,myResp,&myNum);
  if (result != 0)
    debug("version request failed (%d)", result);
  debug("version: %x", myResp[0]);
  debug("version: %x", myResp[1]);
  debug("version: %x", myResp[2]);


  for(uint8_t i=0;i<0x12;i++)
  {
    myNum = 1;
    char myOptions[3] = {0x00, regOffset+i, 1};
    result = cmd(AR1021_CMD_REGISTER_READ,myOptions,3,myResp,&myNum);
      debug("reg: %x",myResp[0]);
  }

}

bool AR1021::calibrateStart() {
    bool ok = false;
    int result = 0;
    int attempts = 0;

    if (!_initialized) return false;

    //_siqIrq.rise(NULL);

    while(1) {

        do {
            // disable touch
            result = cmd(AR1021_CMD_DISABLE_TOUCH, NULL, 0, NULL, 0);
            if (result != 0) {
                debug("disable touch failed (%d)", result);
                break;
            }

            char regOffset = 0;
            int regOffLen = 1;
            result = cmd(AR1021_CMD_REGISTER_START_ADDR_REQUEST, NULL, 0,
                    &regOffset, &regOffLen);
            if (result != 0) {
                debug("register offset request failed (%d)", result);
                break;
            }

            // set insets
            // enable calibrated coordinates
            //                high, low address,                       len,  value
            char insets[4] = {0x00, AR1021_REG_CALIB_INSETS+regOffset, 0x01, _inset};
            result = cmd(AR1021_CMD_REGISTER_WRITE, insets, 4, NULL, 0);
            if (result != 0) {
                debug("register write request failed (%d)", result);
                break;
            }

            // calibration mode
            char calibType = 4;
            result = cmd(AR1021_CMD_CALIBRATE_MODE, &calibType, 1, NULL, 0, false);
            if (result != 0) {
                debug("calibration mode failed (%d)", result);
                break;
            }

            _calibPoint = 0;
            ok = true;

        } while(0);

        if (ok) break;

        // try to run the calibrate mode sequence at most 2 times
        if (++attempts >= 2) break;
    }

    return ok;
}

bool AR1021::getNextCalibratePoint(uint16_t* x, uint16_t* y) {

    if (!_initialized) return false;
    if (x == NULL || y == NULL) return false;

    int xInset = (_width * _inset / 100) / 2;
    int yInset = (_height * _inset / 100) / 2;

    switch(_calibPoint) {
    case 0:
        *x = xInset;
        *y = yInset;
        break;
    case 1:
        *x = _width - xInset;
        *y = yInset;
        break;
    case 2:
        *x = _width - xInset;
        *y = _height - yInset;
        break;
    case 3:
        *x = xInset;
        *y = _height - yInset;
        break;
    default:
        return false;
    }

    return true;
}

bool AR1021::waitForCalibratePoint(bool* morePoints, uint32_t timeout) {
    int result = 0;
    bool ret = false;

    if (!_initialized) return false;

    do {
        if (morePoints == NULL || _calibPoint >= AR1021_NUM_CALIB_POINTS) {
            break;
        }

        // wait for response
        result = waitForCalibResponse(timeout);
        if (result != 0) {
            debug("wait for calibration response failed (%d)\n", result);
            break;
        }

        _calibPoint++;
        *morePoints = (_calibPoint < AR1021_NUM_CALIB_POINTS);


        // no more points -> enable touch
        if (!(*morePoints)) {

            // wait for calibration data to be written to eeprom
            // before enabling touch
            result = waitForCalibResponse(timeout);
            if (result != 0) {
                debug("wait for calibration response failed (%d)", result);
                break;
            }


            // clear chip-select since calibration is done;
            spiDevice::unselect(); //_cs = 1;

            result = cmd(AR1021_CMD_ENABLE_TOUCH, NULL, 0, NULL, 0);
            if (result != 0) {
                debug("enable touch failed (%d)", result);
                break;
            }

            //_siqIrq.rise(this, &AR1021::readTouchIrq);
        }

        ret = true;

    } while (0);



    if (!ret) {
        // make sure to set chip-select off in case of an error
        spiDevice::unselect(); // _cs = 1;
        // calibration must restart if an error occurred
        _calibPoint = AR1021_NUM_CALIB_POINTS+1;
    }



    return ret;
}

int AR1021::cmd(char cmd, char* data, int len, char* respBuf, int* respLen,bool setCsOff)
{

    int ret = 0;

    // command request
    // ---------------
    // 0x55 len cmd data
    // 0x55 = header
    // len = data length + cmd (1)
    // data = data to send

    spiDevice::select(); //_cs = 0;

    transceiveByte(0x55); // _spi.write(0x55);
    _delay_us(50); // according to data sheet there must be an inter-byte delay of ~50us
    transceiveByte(len+1); // _spi.write(len+1);
    _delay_us(50);
    transceiveByte(cmd); // _spi.write(cmd);
    _delay_us(50);

    for(int i = 0; i < len; i++) {
        transceiveByte(data[i]); // __spi.write(data[i]);
        _delay_us(50);
    }


    // wait for response (siq goes high when response is available)
    // Timer t;
    // t.start();
    _timeoutTimer->value= 101;
    _timeoutTimer->state=TM_START;
    //while(_siq.read() != 1 && t.read_ms() < 1000);
    while( ((AR1021_INT_PORT.IN & AR1021_INT_PIN)==0) && (_timeoutTimer->state != TM_STOP));


    // command response
    // ---------------
    // 0x55 len status cmd data
    // 0x55 = header
    // len = number of bytes following the len byte
    // status = status
    // cmd = command ID
    // data = data to receive


    do {

        //if (t.read_ms() >= 1000) {
        if (_timeoutTimer->state == TM_STOP)
        {
            ret = AR1021_ERR_TIMEOUT;
            break;
        }

        int head = transceiveByte(0); // __spi.write(0);
        if (head != 0x55) {
            ret = AR1021_ERR_NO_HDR;
            debug("wrong head: %d",head);
            break;
        }

        _delay_us(50);
        int len = transceiveByte(0); // _spi.write(0);
        if (len < 2) {
            ret = AR1021_ERR_INV_LEN;
            break;
        }

        _delay_us(50);
        int status = transceiveByte(0); // _spi.write(0);
        if (status != AR1021_RESP_STAT_OK) {
            ret = -status;
            break;
        }

        _delay_us(50);
        int cmdId = transceiveByte(0); // _spi.write(0);
        if (cmdId != cmd) {
            ret = AR1021_ERR_INV_RESP;
            break;
        }

        if ( ((len-2) > 0 && respLen  == NULL)
                || ((len-2) > 0 && respLen != NULL && *respLen < (len-2))) {
            ret = AR1021_ERR_INV_RESPLEN;
            break;
        }

        for (int i = 0; i < len-2;i++) {
            _delay_us(50);
            respBuf[i] = transceiveByte(0); // _spi.write(0);
        }
        if (respLen != NULL) {
            *respLen = len-2;
        }

        // make sure we wait 50us before issuing a new cmd
        _delay_us(50);

    } while (0);



    // disable chip-select if setCsOff is true or if an error occurred
    if (setCsOff || ret != 0) {
        spiDevice::unselect(); // _cs = 1;
    }



    return ret;
}

int AR1021::waitForCalibResponse(uint32_t timeout) {
    //Timer t;
    int ret = 0;

    _timeoutTimer->value = timeout;
    _timeoutTimer->state = TM_START; // t.start();

    // wait for siq
    // while (_siq.read() != 1 && (timeout == 0 || (uint32_t)t.read_ms() < (int)timeout));
    while ( ((AR1021_INT_PORT.IN & AR1021_INT_PIN)==0) && (timeout == 0 || _timeoutTimer->state != TM_STOP ) );


    do {

        // if (timeout >  0 && (uint32_t)t.read_ms() >= timeout)
        if (timeout >  0 && _timeoutTimer->state == TM_STOP )
        {
            ret = AR1021_ERR_TIMEOUT;
            break;
        }

        int head = transceiveByte(0); // _spi.write(0);
        if (head != 0x55) {
            ret = AR1021_ERR_NO_HDR;
            break;
        }

        _delay_us(50);
        int len = transceiveByte(0); // _spi.write(0);
        if (len != 2) {
            ret = AR1021_ERR_INV_LEN;
            break;
        }

        _delay_us(50);
        int status = transceiveByte(0); // _spi.write(0);
        if (status != AR1021_RESP_STAT_OK) {
            ret = -status;
            break;
        }

        _delay_us(50);
        int cmdId = transceiveByte(0); // _spi.write(0);
        if (cmdId != 0x14) {
            ret = AR1021_ERR_INV_RESP;
            break;
        }


        // make sure we wait 50us before issuing a new cmd
        _delay_us(50);

    } while (0);


    return ret;
}


void AR1021::readTouchIrq()
{
    //while(_siq.read() == 1)
    while(AR1021_INT_PORT.IN & AR1021_INT_PIN)
    {

        spiDevice::select(); // _cs = 0;
        _delay_us(50);

        // touch coordinates are sent in a 5-byte data packet

        int pen = transceiveByte(0); // _spi.write(0);
        _delay_us(50);

        int xlo = transceiveByte(0); // _spi.write(0);
        _delay_us(50);

        int xhi = transceiveByte(0); // _spi.write(0);
        _delay_us(50);

        int ylo = transceiveByte(0); // _spi.write(0);
        _delay_us(50);

        int yhi = transceiveByte(0); // _spi.write(0);
        _delay_us(50);

        spiDevice::unselect(); //_cs = 1;


        // pen down
        if ((pen&AR1021_PEN_MASK) == (1<<7|1<<0)) {
            actual.touched = true;
        }
        // pen up
        else if ((pen&AR1021_PEN_MASK) == (1<<7)){
            actual.touched = false;
        }
        // invalid value
        else {
            continue;
        }
        if(_rotated)
        {
          actual.y = ( ((xhi<<7)|xlo)* _height )>>12;
          actual.x = ( ((yhi<<7)|ylo)* _width )>>12;
        }
        else
        {
          actual.x = ( ((xhi<<7)|xlo)* _width )>>12;
          actual.y = ( ((yhi<<7)|ylo)* _height )>>12;
        }
    }
}


bool AR1021::compareCoord(const touchCoordinate_t& a, const touchCoordinate_t& b)
{
  return( (a.x==b.x) && (a.y==b.y) && (a.touched==b.touched) );
}


