/*************************************************************************
 * pca9685.c
 *
 * This software is a devLib extension to wiringPi <http://wiringpi.com/>
 * and enables it to control the Adafruit PCA9685 16-Channel 12-bit
 * PWM/Servo Driver <http://www.adafruit.com/products/815> via I2C interface.
 *
 * Copyright (c) 2014 Reinhard Sprung
 *
 * If you have questions or improvements email me at
 * reinhard.sprung[at]gmail.com
 *
 * This software is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The given code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You can view the contents of the licence at <http://www.gnu.org/licenses/>.
 **************************************************************************
 */

#include "config.h"
#ifdef HAVE_WIRINGPI_H
#include <wiringPi.h>
#include <wiringPiI2C.h>

#include "pca9685.h"

// Setup registers
#define PCA9685_MODE1 0x0
#define PCA9685_PRESCALE 0xFE

// Define first LED and all LED. We calculate the rest
#define LED0_ON_L 0x6
#define LEDALL_ON_L 0xFA

#define PIN_ALL 16


// Declare
static void myPwmWrite(struct wiringPiNodeStruct *node, int pin, int value);
static void myOnOffWrite(struct wiringPiNodeStruct *node, int pin, int value);
static int myOffRead(struct wiringPiNodeStruct *node, int pin);
static int myOnRead(struct wiringPiNodeStruct *node, int pin);
int baseReg(int pin);


/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 S E T U P                                                                                           *
 *  =======================                                                                                           *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief .
 *  \param pinBase .
 *  \param i2cAddress .
 *  \param freq .
 *  \result .
 */
int pca9685Setup(const int pinBase, const int i2cAddress, float freq)
{
	// Create a node with 16 pins [0..15] + [16] for all
	struct wiringPiNodeStruct *node = wiringPiNewNode(pinBase, PIN_ALL + 1);

	// Check if pinBase is available
	if (!node)
		return -1;

	// Check i2c address
	int fd = wiringPiI2CSetup(i2cAddress);
	if (fd < 0)
		return fd;

	// Setup the chip. Enable auto-increment of registers.
	int settings = wiringPiI2CReadReg8(fd, PCA9685_MODE1) & 0x7F;
	int autoInc = settings | 0x20;

	wiringPiI2CWriteReg8(fd, PCA9685_MODE1, autoInc);
	
	// Set frequency of PWM signals. Also ends sleep mode and starts PWM output.
	if (freq > 0)
		pca9685PWMFreq(fd, freq);
	

	node->fd			= fd;
	node->pwmWrite		= myPwmWrite;
	node->digitalWrite	= myOnOffWrite;
	node->digitalRead	= myOffRead;
	node->analogRead	= myOnRead;

	return fd;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 P W M F R E Q                                                                                       *
 *  ===========================                                                                                       *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief .
 *  \param fd .
 *  \param freq .
 *  \result .
 */
void pca9685PWMFreq(int fd, float freq)
{
	// Cap at min and max
	freq = (freq > 1000 ? 1000 : (freq < 40 ? 40 : freq));

	// To set pwm frequency we have to set the prescale register. The formula is:
	// prescale = round(osc_clock / (4096 * frequency))) - 1 where osc_clock = 25 MHz
	// Further info here: http://www.nxp.com/documents/data_sheet/PCA9685.pdf Page 24
	int prescale = (int)(25000000.0f / (4096 * freq) - 0.5f);

	// Get settings and calc bytes for the different states.
	int settings = wiringPiI2CReadReg8(fd, PCA9685_MODE1) & 0x7F;	// Set restart bit to 0
	int sleep	= settings | 0x10;									// Set sleep bit to 1
	int wake 	= settings & 0xEF;									// Set sleep bit to 0
	int restart = wake | 0x80;										// Set restart bit to 1

	// Go to sleep, set prescale and wake up again.
	wiringPiI2CWriteReg8(fd, PCA9685_MODE1, sleep);
	wiringPiI2CWriteReg8(fd, PCA9685_PRESCALE, prescale);
	wiringPiI2CWriteReg8(fd, PCA9685_MODE1, wake);

	// Now wait a millisecond until oscillator finished stabilizing and restart PWM.
	delay(1);
	wiringPiI2CWriteReg8(fd, PCA9685_MODE1, restart);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 P W M R E S E T                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief .
 *  \param fd .
 *  \result .
 */
void pca9685PWMReset(int fd)
{
	wiringPiI2CWriteReg16(fd, LEDALL_ON_L	 , 0x0);
	wiringPiI2CWriteReg16(fd, LEDALL_ON_L + 2, 0x1000);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 P W M W R I T E                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief .
 *  \param fd .
 *  \param pin .
 *  \param on .
 *  \param off .
 *  \result .
 */
void pca9685PWMWrite(int fd, int pin, int on, int off)
{
	int reg = baseReg(pin);

	// Write to on and off registers and mask the 12 lowest bits of data to overwrite full-on and off
	wiringPiI2CWriteReg16(fd, reg	 , on  & 0x0FFF);
	wiringPiI2CWriteReg16(fd, reg + 2, off & 0x0FFF);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 P W M R E A D                                                                                       *
 *  ===========================                                                                                       *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief .
 *  \param fd .
 *  \param pin .
 *  \param on .
 *  \param off .
 *  \result .
 */
void pca9685PWMRead(int fd, int pin, int *on, int *off)
{
	int reg = baseReg(pin);

	if (on)
		*on  = wiringPiI2CReadReg16(fd, reg);
	if (off)
		*off = wiringPiI2CReadReg16(fd, reg + 2);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 F U L L  O N                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief .
 *  \param fd .
 *  \param pin .
 *  \param tf .
 *  \result .
 */
void pca9685FullOn(int fd, int pin, int tf)
{
	int reg = baseReg(pin) + 1;		// LEDX_ON_H
	int state = wiringPiI2CReadReg8(fd, reg);

	// Set bit 4 to 1 or 0 accordingly
	state = tf ? (state | 0x10) : (state & 0xEF);

	wiringPiI2CWriteReg8(fd, reg, state);

	// For simplicity, we set full-off to 0 because it has priority over full-on
	if (tf)
		pca9685FullOff(fd, pin, 0);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 F U L L  O F F                                                                                      *
 *  ============================                                                                                      *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief .
 *  \param fd .
 *  \param pin .
 *  \param tf .
 *  \result .
 */
void pca9685FullOff(int fd, int pin, int tf)
{
	int reg = baseReg(pin) + 3;		// LEDX_OFF_H
	int state = wiringPiI2CReadReg8(fd, reg);

	// Set bit 4 to 1 or 0 accordingly
	state = tf ? (state | 0x10) : (state & 0xEF);

	wiringPiI2CWriteReg8(fd, reg, state);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  B A S E  R E G                                                                                                    *
 *  ==============                                                                                                    *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief .
 *  \param pin .
 *  \result .
 */
int baseReg(int pin)
{
	return (pin >= PIN_ALL ? LEDALL_ON_L : LED0_ON_L + 4 * pin);
}




/**********************************************************************************************************************
 *                                                                                                                    *
 *  M Y  P W M  W R I T E                                                                                             *
 *  =====================                                                                                             *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief .
 *  \param node .
 *  \param pin .
 *  \param value .
 *  \result .
 */
static void myPwmWrite(struct wiringPiNodeStruct *node, int pin, int value)
{
	int fd   = node->fd;
	int ipin = pin - node->pinBase;

	if (value >= 4096)
		pca9685FullOn(fd, ipin, 1);
	else if (value > 0)
		pca9685PWMWrite(fd, ipin, 0, value);	// (Deactivates full-on and off by itself)
	else
		pca9685FullOff(fd, ipin, 1);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  M Y  O N  O F F  W R I T E                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief .
 *  \param node .
 *  \param pin .
 *  \param value .
 *  \result .
 */
static void myOnOffWrite(struct wiringPiNodeStruct *node, int pin, int value)
{
	int fd   = node->fd;
	int ipin = pin - node->pinBase;

	if (value)
		pca9685FullOn(fd, ipin, 1);
	else
		pca9685FullOff(fd, ipin, 1);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  M Y  O F F  R E A D                                                                                               *
 *  ===================                                                                                               *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief .
 *  \param node .
 *  \param pin .
 *  \result .
 */
static int myOffRead(struct wiringPiNodeStruct *node, int pin)
{
	int fd   = node->fd;
	int ipin = pin - node->pinBase;

	int off;
	pca9685PWMRead(fd, ipin, 0, &off);

	return off;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  M Y  O N  R E A D                                                                                                 *
 *  =================                                                                                                 *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief .
 *  \param node .
 *  \param pin .
 *  \result .
 */
static int myOnRead(struct wiringPiNodeStruct *node, int pin)
{
	int fd   = node->fd;
	int ipin = pin - node->pinBase;

	int on;
	pca9685PWMRead(fd, ipin, &on, 0);

	return on;
}

#endif




