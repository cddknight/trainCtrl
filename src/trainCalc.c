/**********************************************************************************************************************
 *                                                                                                                    *
 *  T R A I N  C A L C . C                                                                                            *
 *  ======================                                                                                            *
 *                                                                                                                    *
 *  Copyright (c) 2023 Chris Knight                                                                                   *
 *                                                                                                                    *
 *  File trainCalc.c part of TrainControl is free software: you can redistribute it and/or modify it under the terms  *
 *  of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License,  *
 *  or (at your option) any later version.                                                                            *
 *                                                                                                                    *
 *  TrainControl is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the        *
 *  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for  *
 *  more details.                                                                                                     *
 *                                                                                                                    *
 *  You should have received a copy of the GNU General Public License along with this program. If not, see:           *
 *  <http://www.gnu.org/licenses/>                                                                                    *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \file
 *  \brief Calculate registers for train ID.
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

/**********************************************************************************************************************
 *                                                                                                                    *
 *  H E L P  T H E M                                                                                                  *
 *  ================                                                                                                  *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Output the help message.
 *  \result None.
 */
void helpThem ()
{
	printf ("traincalc [-rsdac] <train cab>\n");
	printf ("    -r . . . Reverse directions.\n");
	printf ("    -s . . . 28/128 Speed steps.\n");
	printf ("    -d . . . DC operation.\n");
	printf ("    -a . . . Railcom.\n");
	printf ("    -c . . . Complex speed curve.\n");
	printf ("    -o . . . Other CVs to set.\n");
}

/**********************************************************************************************************************
 *                                                                                                                    *
 *  S H O W  S E P                                                                                                    *
 *  ==============                                                                                                    *
 *                                                                                                                    *
 **********************************************************************************************************************/
/**
 *  \brief Show the separator.
 *  \param id ID being processed.
 *  \result None.
 */
void showSep (int id)
{
	printf ("------------- %04d -------------\n", id);
}

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
	int i, trainID = 3, cv29 = 0, done = 0;

	while ((i = getopt(argc, argv, "rsdaco")) != -1)
	{
		switch (i)
		{
		case 'r':
			cv29 |= 1;
			break;
		case 's':
			cv29 |= 2;
			break;
		case 'd':
			cv29 |= 4;
			break;
		case 'a':
			cv29 |= 8;
			break;
		case 'c':
			cv29 |= 16;
			break;
		case 'o':
			printf ("My setup for Steam Locos:\n");
			printf ("CV 03: %3d (0x%02X)\n"
				"CV 04: %3d (0x%02X)\n"
				"CV 05: %3d (0x%02X)\n"
				"CV 06: %3d (0x%02X)\n"
				"CV 61: %3d (0x%02X)\n",
					20, 20, 24, 24, 188, 188, 60, 60, 1, 1);
			return 0;
		case '?':
			helpThem();
			exit (1);
		}
	}
	for (; optind < argc; ++optind)
	{
		trainID = atoi (argv[optind]);
		showSep (trainID);
		if (trainID > 0 && trainID < 100)
		{
			printf ("CV  1: %3d (0x%02X)\nCV 29: %3d (0x%02X)\n", trainID, trainID, cv29, cv29);
		}
		else if (trainID > 123 && trainID < 10000)
		{
			int cv17 = 0, cv18 = 0;

			cv17 = (trainID >> 8) + 0xC0;
			cv18 = trainID & 0xFF;
			cv29 |= 32;

			printf ("CV 17: %3d (0x%02X)\nCV 18: %3d (0x%02X)\nCV 29: %3d (0x%02X)\n",
					cv17, cv17, cv18, cv18, cv29, cv29);
		}
		else
		{
			printf ("%d is not a recommended address\n", trainID);
		}
		++done;
	}
	if (done == 0)
		helpThem();
	else
		printf ("--------------------------------\n");

	return 0;
}
