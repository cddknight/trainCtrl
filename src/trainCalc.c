/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  C A L C . C                                                                                            *
 *  ======================                                                                                            *
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
 *  \brief Calculate registers for train ID.
 */
#include <stdio.h>
#include <stdlib.h>

/**********************************************************************************************************************
 *                                                                                                                    *
 *  M A I N                                                                                                           *
 *  =======                                                                                                           *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief The program starts here.
 *  \param argc The number of arguments passed to the program.
 *  \param argv Pointers to the arguments passed to the program.
 *  \result 0 (zero) if all processed OK.
 */
int main (int argc, char *argv[])
{
	int i, trainID = 3;

	for (i = 1; i < argc; ++i)
	{
	
		trainID = atoi (argv[i]);
		if (trainID > 0 && trainID < 100)
		{
			printf ("CV 1: %d (0x%X)\nCV 29-5: 0\n", trainID, trainID);
		}
		else if (trainID > 123 && trainID < 10000)
		{
			int reg17 = 0, reg18 = 0;

			reg17 = (trainID >> 8) + 0xC0;
			reg18 = trainID & 0xFF;
			
			printf ("CV 17: %3d (0x%02X)\nCV 18: %3d (0x%02X)\nCV 29-5: 1\n",
					reg17, reg17, reg18, reg18);
		}
		else
		{
			printf ("%d is not a recomened address\n", trainID);
		}
	}
	return 0;
}
