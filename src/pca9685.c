/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 . C                                                                                                 *
 *  =================                                                                                                 *
 *                                                                                                                    *
 *  This is free software; you can redistribute it and/or modify it under the terms of the GNU General Public         *
 *  License version 2 as published by the Free Software Foundation.  Note that I am not granting permission to        *
 *  redistribute or modify this under the terms of any later version of the General Public License.                   *
 *                                                                                                                    *
 *  This is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied        *
 *  warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more     *
 *  details.                                                                                                          *
 *                                                                                                                    *
 *  You should have received a copy of the GNU General Public License along with this program (in the file            *
 *  "COPYING"); if not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111,   *
 *  USA.                                                                                                              *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \file
 *  \brief Functions to control a servo.
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
 *  \brief Inital setup of the pca9685 interface.
 *  \param pinBase Base number i2c interface.
 *  \param i2cAddress Address of the i2c interface.
 *  \param freq Frequency to use.
 *  \result File hadle of the i2c.
 */
int pca9685Setup(const int pinBase, const int i2cAddress, float freq)
{
	int fd = -1;

	// Create a node with 16 pins [0..15] + [16] for all
	struct wiringPiNodeStruct *node = wiringPiNewNode(pinBase, PIN_ALL + 1);

	// Check if pinBase is available
	if (node)
	{
		// Check i2c address
		if ((fd = wiringPiI2CSetup (i2cAddress)) >= 0)
		{
			// Setup the chip. Enable auto-increment of registers.
			int settings = wiringPiI2CReadReg8 (fd, PCA9685_MODE1) & 0x7F;
			int autoInc = settings | 0x20;

			wiringPiI2CWriteReg8(fd, PCA9685_MODE1, autoInc);

			// Set frequency of PWM signals. Also ends sleep mode and starts PWM output.
			if (freq > 0)
			{
				pca9685PWMFreq(fd, freq);
			}
			node->fd = fd;
			node->pwmWrite		= myPwmWrite;
			node->digitalWrite	= myOnOffWrite;
			node->digitalRead	= myOffRead;
			node->analogRead	= myOnRead;
		}
	}
	return fd;
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 P W M F R E Q                                                                                       *
 *  ===========================                                                                                       *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Set the frequecy.
 *  \param fd File handle of the interface.
 *  \param freq Frequency to set.
 *  \result None.
 */
void pca9685PWMFreq(int fd, float freq)
{
	if (fd != -1)
	{
		// Cap at min and max
		freq = (freq > 1000 ? 1000 : (freq < 40 ? 40 : freq));

		// -----------------------------------------------------------------------------------------------------
		// To set pwm frequency we have to set the prescale register. The formula is:
		// prescale = round(osc_clock / (4096 * frequency))) - 1 where osc_clock = 25 MHz
		// Further info here: http://www.nxp.com/documents/data_sheet/PCA9685.pdf Page 25
		// -----------------------------------------------------------------------------------------------------
		int prescale = (int)(25000000.0f / (4096 * freq) - 0.5f);

		// Get settings and calc bytes for the different states.
		int settings = wiringPiI2CReadReg8(fd, PCA9685_MODE1) & 0x7F;	// Set restart bit to 0
		int sleep	= settings | 0x10;									// Set sleep bit to 1
		int wake	= settings & 0xEF;									// Set sleep bit to 0
		int restart = wake | 0x80;										// Set restart bit to 1

		// Go to sleep, set prescale and wake up again.
		wiringPiI2CWriteReg8(fd, PCA9685_MODE1, sleep);
		wiringPiI2CWriteReg8(fd, PCA9685_PRESCALE, prescale);
		wiringPiI2CWriteReg8(fd, PCA9685_MODE1, wake);

		// Now wait a millisecond until oscillator finished stabilizing and restart PWM.
		delay(1);
		wiringPiI2CWriteReg8(fd, PCA9685_MODE1, restart);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 P W M R E S E T                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Reset the i2c interface.
 *  \param fd File handle of the interface.
 *  \result None.
 */
void pca9685PWMReset(int fd)
{
	wiringPiI2CWriteReg16 (fd, LEDALL_ON_L, 0x0);
	wiringPiI2CWriteReg16 (fd, LEDALL_ON_L + 2, 0x1000);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 P W M W R I T E                                                                                     *
 *  =============================                                                                                     *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Write to a particular pin.
 *  \param fd File handle of the i2c interface.
 *  \param pin Pin to write to.
 *  \param on Value for on.
 *  \param off Vlaue for off.
 *  \result .
 */
void pca9685PWMWrite(int fd, int pin, int on, int off)
{
	// Write to on and off registers and mask the 12 lowest bits of data to overwrite full-on and off
	wiringPiI2CWriteReg16 (fd, baseReg (pin), on & 0x0FFF);
	wiringPiI2CWriteReg16 (fd, baseReg (pin) + 2, off & 0x0FFF);
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 P W M R E A D                                                                                       *
 *  ===========================                                                                                       *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Read from the interace.
 *  \param fd File handle of the interface.
 *  \param pin Pin to read from.
 *  \param on Return on value.
 *  \param off Return off value.
 *  \result None.
 */
void pca9685PWMRead(int fd, int pin, int *on, int *off)
{
	if (on)
	{
		*on	 = wiringPiI2CReadReg16 (fd, baseReg(pin));
	}
	if (off)
	{
		*off = wiringPiI2CReadReg16 (fd, baseReg(pin) + 2);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 F U L L  O N                                                                                        *
 *  ==========================                                                                                        *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Switch all on.
 *  \param fd File handle of the interface.
 *  \param pin Pin to write too.
 *  \param tf Set true false.
 *  \result None.
 */
void pca9685FullOn(int fd, int pin, int tf)
{
	int reg = baseReg(pin) + 1;		// LEDX_ON_H
	int state = wiringPiI2CReadReg8(fd, reg);

	// Set bit 4 to 1 or 0 accordingly
	state = tf ? (state | 0x10) : (state & 0xEF);
	wiringPiI2CWriteReg8 (fd, reg, state);

	// For simplicity, we set full-off to 0 because it has priority over full-on
	if (tf)
	{
		pca9685FullOff (fd, pin, 0);
	}
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  P C A 9 6 8 5 F U L L  O F F                                                                                      *
 *  ============================                                                                                      *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Switch all off.
 *  \param fd File handle of the interface.
 *  \param pin Pin to write too.
 *  \param tf Set true false.
 *  \result None.
 */
void pca9685FullOff(int fd, int pin, int tf)
{
	int reg = baseReg(pin) + 3;		// LEDX_OFF_H
	int state = wiringPiI2CReadReg8 (fd, reg);

	// Set bit 4 to 1 or 0 accordingly
	state = tf ? (state | 0x10) : (state & 0xEF);
	wiringPiI2CWriteReg8 (fd, reg, state);
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
	if (value >= 4096)
		pca9685FullOn (node->fd, pin - node->pinBase, 1);
	else if (value > 0)
		pca9685PWMWrite (node->fd, pin - node->pinBase, 0, value);	// (Deactivates full-on and off by itself)
	else
		pca9685FullOff (node->fd, pin - node->pinBase, 1);
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
	value ? pca9685FullOn (node->fd, pin - node->pinBase, 1) :
			pca9685FullOff (node->fd, pin - node->pinBase, 1);
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
	int off = 0;
	pca9685PWMRead (node->fd, pin - node->pinBase, 0, &off);
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
	int on = 0;
	pca9685PWMRead (node->fd, pin - node->pinBase, &on, 0);
	return on;
}

#endif

