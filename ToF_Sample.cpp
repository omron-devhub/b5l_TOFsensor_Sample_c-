/*---------------------------------------------------------------------------*/
/* Copyright(C)  2019-2021  OMRON Corporation                                */
/*                                                                           */
/* Licensed under the Apache License, Version 2.0 (the "License");           */
/* you may not use this file except in compliance with the License.          */
/* You may obtain a copy of the License at                                   */
/*                                                                           */
/*     http://www.apache.org/licenses/LICENSE-2.0                            */
/*                                                                           */
/* Unless required by applicable law or agreed to in writing, software       */
/* distributed under the License is distributed on an "AS IS" BASIS,         */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  */
/* See the License for the specific language governing permissions and       */
/* limitations under the License.                                            */
/*---------------------------------------------------------------------------*/

#ifndef _WIN32
#include	<termios.h>
#include	<sys/time.h>

#else
#define		_CRT_SECURE_NO_WARNINGS

#include	<windows.h>
#endif // _WIN32


#include	<ctype.h>
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/stat.h>
#include	<time.h>
#include	"TOFApiZ.h"
#include	"uart.h"


/* [CODE:  Data Type:                                                              ] */
/* 0x000:  distance data (polar coordinates)                                         */
/* 0x001:  distance data (rectangle coordinates without rotation)                    */
/* 0x002:  distance data (rectangle coordinates with rotation)                       */
/* 0x100:  distance data (polar coordinates) and amplitude data                      */
/* 0x101:  distance data (rectangle coordinates without rotation) and amplitude data */
/* 0x102:  distance data (rectangle coordinates with rotation) and amplitude data    */
/* 0x1FF:  amplitude data                                                            */
#define	RAW_OUTPUT				0

#define	BMP_HEADDER_SIZE		54
#define	TOF_TIMEOUT				1000	/* ms (Minimum: 500ms) */
#define	PARAMETER_RESET_TIMEOUT	2000
#define	MAX_CUBOID				10


const unsigned char	ucBmp24Headder[BMP_HEADDER_SIZE] = {
	0x42, 0x4d, 0x36, 0x84, 0x03, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
	0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0xf0, 0x00,
	0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x84, 0x03, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
					ucBmp8Headder[BMP_HEADDER_SIZE] = {
	0x42, 0x4d, 0x36, 0x30, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x36, 0x04, 0x00, 0x00, 0x28, 0x00,
	0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0xf0, 0x00,
	0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x2C, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

const char	*ucParameters[] = {
	"IMAGE_PATH",
	"FOV_LIMITATION",
	"DETECTION_CUBOID",
	"OUTPUT_FORMAT",
	"OPERATION_MODE",
	"EXPOSURE_FPS",
	"T3D_XYZ",
	"LED_FREQUENCY_ID",
	"MIN_AMP_ALL",
	"MIN_AMP_NEAR",
	"OPERATION_CHECK_LED",
	"RESPONSE_SPEED",
	"ENR_TRESHOLD" };


INT32	ToF_Format;
char	ImagePath[256] = "";
int		TreminationFlag = 0,
		FOV_Limitation = 0,
		PointNum = 76800;

int		DetectionCuboid[MAX_CUBOID][7],	/* x1, y1, z1, x2, y2, z2, Th */
		CuboidCount[MAX_CUBOID],
		InFOV[76800];
char	CuboidCommand[MAX_CUBOID][256];

#ifndef _WIN32
#define	TRAP_START()	signal(SIGINT, CtrlC_Handler)
#define	TRAP_END()		signal(SIGINT, SIG_DFL)
#define	TIMEVAL timeval
#define	TV_USEC tv_usec
#define	TO_MSEC 1000.0
#define	GETTIMEOFDAY(ptv, pt)   gettimeofday(ptv, pt)

void CtrlC_Handler(int tmp)
{
	TreminationFlag = 1;
	fprintf(stderr, "\rNow terminating...\n");
	signal(SIGINT, CtrlC_Handler);
}

#else
#define	TRAP_START()	SetConsoleCtrlHandler(Ctrl_Handler, TRUE)
#define	TRAP_END()		SetConsoleCtrlHandler(Ctrl_Handler, FALSE)
#define	TIMEVAL timespec
#define	TV_USEC tv_nsec
#define	TO_MSEC 1000000.0
#define	GETTIMEOFDAY(ptv, pt)   timespec_get(ptv, TIME_UTC)

BOOL WINAPI Ctrl_Handler(DWORD dwCtrlType)
{
	TreminationFlag = 1;
	fprintf(stderr, "\rNow terminating...\n");
	return TRUE;
}
#endif


int BMPimage24(char *pcFileName, unsigned char *puImage)
{
	FILE	*fp;
	int		x, y, z, c, r, g, b;

	if (pcFileName == NULL) {
		return 0;
	}

	/* Open BMP file for output */
	if ((fp = fopen(pcFileName, "wb")) == NULL) {
		return -1;
	}

	/* Write a BMP headder of 24bit-color */
	if (fwrite(ucBmp24Headder, BMP_HEADDER_SIZE, 1, fp) != 1) {
		return -2;
	}

	/* Output an image data */
	for(y = 0; y < 240; y++) {
		for(x = 0; x < 320; x++) {

			/* Color converting */
			if (InFOV[y * 320 + x]) {
				z = puImage[(y * 320 + x) * 2] + (int)puImage[(y * 320 + x) * 2 + 1] * 256;
				c = z / 6;
				if (c < 128) {
					// 0-767:0-127
					r = 128 - c,  g = b = 0;
				} else if (c < 383) {
					// 768-2297:128-382
					r = 255,  g = c - 127,  b = 0;
				} else if (c < 638) {
					// 2298-3827:383-637
					r = 637 - c,  g = 255,  b = c - 382;
				} else if (c < 893) {
					// 3828-5357:638-892
					r = 0,  g = 892 - c,  b = 255;
				} else if (c < 1020) {
					r = g = 0,  b = 1147-c;
					// 5358-6119:893-1019
				} else if (z < TOF_SATULATION_DISTANCE) {
					// Too far
					r = g = b = 0;
				} else {
					// Error
					if (z == TOF_LOWAMP_DISTANCE) {
						// Low Amplitude
						r = g = b = 0;
					}
					else {
						// Satulation or Overflow
						r = g = b = 255;
					}
				}
			}
			else {
				r = g = b = 128;
			}

			/* Output a pixel */
			if (fputc(b, fp) == EOF) {
				return -2;
			}
			if (fputc(g, fp) == EOF) {
				return -2;
			}
			if (fputc(r, fp) == EOF) {
				return -2;
			}
		}
	}

	/* Close the file */
	fclose(fp);
	return 0;
}


int BMPimage8(char *pcFileName, unsigned char *puImage)
{
	FILE	*fp;
	int		x, y, a;

	if (pcFileName == NULL) {
		return 0;
	}

	/* Open BMP file for output */
	if ((fp = fopen(pcFileName, "wb")) == NULL) {
		return -1;
	}

	/* Write a BMP headder of 8bit-gray */
	if (fwrite(ucBmp8Headder, BMP_HEADDER_SIZE, 1, fp) != 1) {
		return -2;
	}

	/* write a palette table */
	for(a = 0; a < 256; a++) {
		if (fputc(a, fp) == EOF) {
			return -2;
		}
		if (fputc(a, fp) == EOF) {
			return -2;
		}
		if (fputc(a, fp) == EOF) {
			return -2;
		}
		if (fputc(0, fp) == EOF) {
			return -2;
		}
	}

	/* Output an image data */
	for(y = 0; y < 240; y++) {
		for(x = 0; x < 320; x++) {

			/* Read an Amplitude data */
			a = puImage[(y * 320 + x) * 2] + (int)puImage[(y * 320 + x) * 2 + 1] * 256;

			/* Error check */
			if (a > 255) {
				if (a < TOF_OVERFLOW_AMPLITUDE) {
					// Low Amplitude
					a = 0;
				}
				else {
					// Satulation or Overflow
					a = 255;
				}
			}

			/* Output a pixel */
			if (fputc(a, fp) == EOF) {
				return -2;
			}
		}
	}

	/* Close the file */
	fclose(fp);
	return 0;
}


int PCDfile(char *pcFileName, unsigned char *puImage, int nFormat)
{
	FILE	*fp = NULL;
	float	f;
	short	*p;
	int		i, j;
	char	buff[180];
	const char* PCDheaderXYZ = "# .PCD v.7 - Point Cloud Data file format\x0a"
							   "VERSION .7\x0a"
							   "FIELDS x y z\x0a"
							   "SIZE 4 4 4\x0a"
							   "TYPE F F F\x0a"
							   "COUNT 1 1 1\x0a"
							   "WIDTH %d\x0a"
							   "HEIGHT %d\x0a"
							   "VIEWPOINT 0 0 0 1 0 0 0\x0a"
							   "POINTS %d\x0a"
							   "DATA binary\x0a";

	/* Open PCD file for output */
	if (pcFileName != NULL) {
		if ((fp = fopen(pcFileName, "wb")) == NULL) {
			return -1;
		}
	}

	/* Output a PCD data */
	p = (short*)(puImage + 170);
	if (nFormat == 0) {	// Integer
		if (fp != NULL) {
			if (fwrite(puImage, 0x708AA, 1, fp) != 1) {
				fprintf(stderr, "File output error. [%s]\n", pcFileName);
				fclose(fp);
				return -2;
			}
		}

		/* Cuboid Check */
		for (i = 0; i < 76800; i++, p += 3) {
			for (j = 0; j < MAX_CUBOID; j++) {
				if (DetectionCuboid[j][6] != 0) {
					if (DetectionCuboid[j][0] <= p[0] && p[0] <= DetectionCuboid[j][3] &&
						DetectionCuboid[j][1] <= p[1] && p[1] <= DetectionCuboid[j][4] &&
						DetectionCuboid[j][2] <= p[2] && p[2] <= DetectionCuboid[j][5]) {
						CuboidCount[j]++;
					}
				}
			}
		}
	}
	else {				// Float
		/* PCD Header */
		if (fp != NULL) {
			if (FOV_Limitation) {
				sprintf(buff, PCDheaderXYZ, PointNum, 1, PointNum);
			}
			else {
				sprintf(buff, PCDheaderXYZ, 320, 240, 76800);
			}
			if (fwrite(buff, strlen(buff), 1, fp) != 1) {
				fprintf(stderr, "File output error. [%s]\n", pcFileName);
				fclose(fp);
				return -2;
			}
		}

		/* Data Loop */
		for(i = 0; i < 76800; i++) {
			if (InFOV[i]) {

				/* Cuboid Check */
				for (j = 0; j < MAX_CUBOID; j++) {
					if (DetectionCuboid[j][6] != 0) {
						if (DetectionCuboid[j][0] <= p[0] && p[0] <= DetectionCuboid[j][3] &&
							DetectionCuboid[j][1] <= p[1] && p[1] <= DetectionCuboid[j][4] &&
							DetectionCuboid[j][2] <= p[2] && p[2] <= DetectionCuboid[j][5] ) {
							CuboidCount[j]++;
						}
					}
				}

				/* Output XYZ */
				if (fp != NULL) {
					for(j = 0; j < 3; j++) {
						f = (float)*p++ / 1000.0F;
						if (fwrite(&f, sizeof(f), 1, fp) != 1) {
							fprintf(stderr, "File output error. [%s]\n", pcFileName);
							fclose(fp);
							return -2;
						}
					}
				}
				else {
					p += 3;
				}
			}
		}
	}

	/* Close the file */
	if (pcFileName != NULL) {
		fclose(fp);
	}
	return 0;
}


int PCD_xyzi_file(char *pcFileName, unsigned char *puImage)
{
    /* The result format should be 0x101 or 0x102 (PCD + Amplitude) */

    FILE             *fp = NULL;
    float			 f, distance;
    short            *pXYZ, *pAmplitude;
    int              i, j;
    unsigned int     a;
    unsigned char    uc;
	char			 buff[190];
	const char       *PCDheaderXYZI = "# .PCD v.7 - Point Cloud Data file format\x0a"
									  "VERSION .7\x0a"
	                                  "FIELDS x y z i\x0a"
	                                  "SIZE 4 4 4 1\x0a"
	                                  "TYPE F F F U\x0a"
	                                  "COUNT 1 1 1 1\x0a"
	                                  "WIDTH 320\x0a"
	                                  "HEIGHT 240\x0a"
	                                  "VIEWPOINT 0 0 0 1 0 0 0\x0a"
	                                  "POINTS 76800\x0a"
	                                  "DATA binary\x0a";

	if (pcFileName != NULL) {
		/* Open PCD file for output */
		if ((fp = fopen(pcFileName, "wb")) == NULL) {
			return -1;
		}

		/* Output a PCD data */
		if (FOV_Limitation) {
			sprintf(buff, PCDheaderXYZI, PointNum, 1, PointNum);
		}
		else {
			sprintf(buff, PCDheaderXYZI, 320, 240, 76800);
		}
		if (fwrite(buff, strlen(buff), 1, fp) != 1) {
			fprintf(stderr, "File output error. [%s]\n", pcFileName);
			fclose(fp);
			return -2;
		}
    }
    pXYZ       = (short *)(puImage + 170);
    pAmplitude = (short *)(puImage + 170 + 6 * 76800);

    /* QVGA Loop */
    for(i = 0; i < 76800; i++) {
		if (InFOV[i]) {

			/* Cuboid Check */
			for (j = 0; j < MAX_CUBOID; j++) {
				if (DetectionCuboid[j][6] != 0) {
					if (DetectionCuboid[j][0] <= pXYZ[0] && pXYZ[0] <= DetectionCuboid[j][3] &&
						DetectionCuboid[j][1] <= pXYZ[1] && pXYZ[1] <= DetectionCuboid[j][4] &&
						DetectionCuboid[j][2] <= pXYZ[2] && pXYZ[2] <= DetectionCuboid[j][5]) {
						CuboidCount[j]++;
					}
				}
			}

			if (fp != NULL) {
				/* Output XYZ */
				distance = 0.0;
				for (j = 0; j < 3; j++) {
					f = (float)*pXYZ++ / 1000.0F;
					distance += f * f;
					if (fwrite(&f, sizeof(f), 1, fp) != 1) {
						fprintf(stderr, "File output error. [%s]\n", pcFileName);
						fclose(fp);
						return -2;
					}
				}

				/* Prepare Amplitude data */
				a = (unsigned int)*pAmplitude++;

				/* Error check */
				if (a > 255) {
					if (a < TOF_OVERFLOW_AMPLITUDE) {
						// Low Amplitude
						a = 0;
					}
					else {
						// Satulation or Overflow
						a = 255;
					}
				}
				else {
					if (a * distance > 255.0) {
						a = 255;
					}
					else {
						a = (unsigned int)(a * distance);
					}
				}
				uc = (unsigned char)a;

				/* Output Intensity */
				if (fwrite(&uc, 1, 1, fp) != 1) {
					fprintf(stderr, "File output error. [%s]\n", pcFileName);
					fclose(fp);
					return -2;
				}
			}
			else {
				pXYZ += 3;
				pAmplitude++;
			}
		}
	}

    /* Close the file */
    if (pcFileName != NULL) {
		fclose(fp);
	}
	return 0;
}


int RAWfile(char *pcFileName, unsigned char *puImage, int outSize)
{
	FILE	*fp;

	if (pcFileName == NULL) {
		return 0;
	}

	/* Open BMP file for output */
	if ((fp = fopen(pcFileName, "wb")) == NULL) {
		return -1;
	}

	/* Output a PCD data */
	if (fwrite(puImage, outSize, 1, fp) != 1) {
		fprintf(stderr, "File output error. [%s]\n", pcFileName);
		fclose(fp);
		return -2;
	}

	/* Close the file */
	fclose(fp);
	return 0;
}


int ImageOutput(INT32 ToF_Format, char *pcFileName, unsigned char *puImage, int nSize)
{
/* [CODE     - Data Type:                       Data Size (PCD) ] */
/* 000h      - polar coordinates:               0x25800           */
/* 001h,002h - rectangle coordinates:           0x708AA           */
/* 100h      - polar coordinates+amplitude:     0x4B000 (0x25800) */
/* 101h,102h - rectangle coordinates+amplitude: 0x960AA (0x708AA) */
/* 1FFh      - amplitude:                       0x25800           */

	char	fname[1024], *p;
	int		ret = 0;

	if (pcFileName == NULL) {
		p = NULL;
	}
	else {
		p = fname;
	}
	switch(ToF_Format) {
	case 0x000:	/* distance data (polar coordinates) */
	case 0x100:	/* distance data (polar coordinates) and amplitude data */
		/* Output Colored BMP Data File (distance) */
		sprintf(fname, "%s.bmp", pcFileName);
		if (BMPimage24(p, puImage) < 0) {
			ret = -1;
		}
		sprintf(fname, "%s.dpt", pcFileName);
		if (RAWfile(p, puImage, 0x25800) < 0) {
			ret = -1;
		}
		puImage += 0x25800;
		break;
	case 0x001:	/* distance data (rectangle coordinates without rotation) */
	case 0x002:	/* distance data (rectangle coordinates with rotation) */
		/* Output PCD Data File (rectangle coordinates, xyz) */
		sprintf(fname, "%s.pcd", pcFileName);
		if (PCDfile(p, puImage, 1) < 0) {
			ret = -1;
		}
		puImage += 0x708AA;
		break;
	case 0x101:	/* distance data (rectangle coordinates without rotation) and amplitude data */
	case 0x102:	/* distance data (rectangle coordinates with rotation) and amplitude data */
		/* Output PCD Data File (rectangle coordinates, xyzi) */
		sprintf(fname, "%s.pcd", pcFileName);
		if (PCD_xyzi_file(p, puImage) < 0) {
			ret = -1;
		}
		puImage += 0x708AA;
		break;
	case 0x1FF:	/* amplitude data */
		break;
	default:	/* RAW data */
		/* Output RAW Data File (any type format) */
		sprintf(fname, "%s.raw", pcFileName);
		if (RAWfile(p, puImage, nSize) < 0) {
			ret = -1;
		}
		else {
			ret = 1;
		}
		break;
	}
	if (ret == 0) {
		if (ToF_Format > 0xff) {
			/* Output Gray-Scale BMP Data File (amplitude)  */
			sprintf(fname, "%s_amp.bmp", pcFileName);
			if (BMPimage8(p, puImage) < 0) {
				ret = -1;
			}
		}
	}
	if (ret == -1) {
		fprintf(stderr, "File output error. [%s]\n", pcFileName);
	}
	return ret;
}


int Interactive(INT32 *cmd)
{
	INT32		outSize, format, counter;
	UINT8		*outData, outStatus;
	char		buff[256], fname[256], *p;
	const char	*cp;
	int			ret, v1, v2, v3;

	while(1) {
		printf("\n\n"
			   "****************************************\n"
			   "  Command Menu\n"
			   "****************************************\n"
			   "00: Get Version\n"
   			   "80: Start Mesurement         81: Stop Mesurement\n"
			   "82: Get Mesurement Result\n"
			   "84: Set Output Format        85: Get Output Format\n"
			   "86: Set Operation Mode       87: Get Operation Mode\n"
			   "88: Set Exposure Time        89: Get Exposure Time\n"
			   "8A: Set Rotation Angle       8B: Get Rotation Angle\n"
			   "8E: Set LED Frequency ID     8F: Get LED Frequency ID\n"
			   "90: Set Min AMP (All)        91: Get Min AMP (All)\n"
			   "92: Set Min AMP (Near)       93: Get Min AMP (Near)\n"
			   "94: Get THETA-PHI Table\n"
			   "95: Set Operation Check LED  96: Get Operation Check LED\n"
			   "97: Set Response Speed       98: Get Response Speed\n"
			   "99: Set ENR Treshold         9A: Get ENR Treshold\n"
			   "9B: Get Imager Temparature   9C: Get LED Temparature\n"
			   "9E: Initialize Parameters\n"
			   "9F: Software RESET\n\n"
			   "Input Command Code [HEX] or just Enter to END: ");

		/* Input Command Code */
		if(fgets(buff, 256, stdin) == NULL) return 2;

		/* Skip leading whitespace */
		for(p = buff; *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'; p++);
		if (*p == '\0') return 1;

		/* Test Command Code */
		sscanf(p, "%x", cmd);
		switch(*cmd) {
			case 0x00: /* Get Version */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get Version\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::getVersion(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				printf("OMRON ToF Sensor:  %.11s %d.%d.%d Revision:%d Serial:%.11s\n",
					   outData, outData[11], outData[12], outData[13], (outData[14] << 24) + (outData[15] << 16) + (outData[16] << 8) + outData[17], outData + 18);
				break;
			case 0x80: /* Start Mesurement */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Start Mesurement\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::startMeasurement(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				printf("ToF Measurement was started.\n");
				break;
			case 0x81: /* Stop Mesurement */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Stop Mesurement\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::stopMeasurement(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				printf("ToF Measurement was stopped.\n");
				break;
			case 0x82: /* Get Mesurement Result */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get Mesurement Result\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "File Name (with path, without extension): ");
				if (fgets(buff, 256, stdin) != NULL) {
					if (buff[strlen(buff) - 1] == '\n') buff[strlen(buff) - 1] = '\0';
					if (*buff != '\0') {
						TRAP_START();
						printf("\nRecording ToF Measurement was started.\n"
							   "When you want to stop, please press Ctrl.+C.\n\n");

						for(counter = 1; TreminationFlag == 0; counter++) {
							if ((ret = CTOFApiZ::getMesurementResult(TOF_TIMEOUT, 0, &outStatus, &outSize, &outData)) != TOF_NO_ERROR) {
								return ret;
							}
							if (outStatus != 0) {
								return outStatus;
							}

							/* Output Data File */
							format = ToF_Format;
							if (RAW_OUTPUT) {
								format = -format;
							}
							sprintf(fname, "%s_%04d", buff, counter);
							if (ImageOutput(format, fname, outData, outSize) < 0) {
								ret = -99;
								return ret;
							}
						}

						TRAP_END();
						TreminationFlag = 0;
						printf("Termination Complete!\n");
					}
				}
				break;
			case 0x84: /* Set Output Format */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Set Output Format\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "000-Polar Coordinates Distance\n"
					   "001-Rectangle Coordinates without Rotation\n"
					   "002-Rectangle Coordinates with Rotation\n"
					   "100-Polar Coordinates Distance + Amplitude\n"
					   "101-Rectangle Coordinates without Rot.+Amp.\n"
					   "102-Rectangle Coordinates with Rot.+Amp.\n"
					   "1FF-Amplitude\n\n"
					   "Output Format [HEX]: ");
				if (fgets(buff, 256, stdin) != NULL) {
					if (sscanf(buff, "%x", &v1) == 1) {
						if (v1 == 0 || v1 == 1 || v1 == 2 ||
							v1 == 0x100 || v1 == 0x101 || v1 == 0x102 || v1 == 0x1ff) {
							if ((ret = CTOFApiZ::setOutputFormat(TOF_TIMEOUT, v1, &outStatus)) != TOF_NO_ERROR) {
								return ret;
							}
							if (outStatus != 0) {
								return outStatus;
							}
							ToF_Format = v1;
							printf("Complete!\n");
						}
					}
				}
				break;
			case 0x85: /* Get Output Format */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get Output Format\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::getOutputFormat(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				v1 = outData[0] * 0x100 + outData[1];
				cp = "Invalid Format Code!";
				if (v1 == 0x000) cp = "Polar Coordinates Distance";
				if (v1 == 0x001) cp = "Rectangle Coordinates without Rotation";
				if (v1 == 0x002) cp = "Rectangle Coordinates with Rotation";
				if (v1 == 0x100) cp = "Polar Coordinates Distance + Amplitude";
				if (v1 == 0x101) cp = "Rectangle Coordinates without Rot. + Amplitude";
				if (v1 == 0x102) cp = "Rectangle Coordinates witht Rot. + Amplitude";
				if (v1 == 0x1FF) cp = "Amplitude";
				printf("Output Format setting is %03X <%s>.\n\n", v1, cp);
				break;
			case 0x86: /* Set Operation Mode */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Set Operation Mode\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "Operation Mode <0: Normal(with HDR), 1: High Speed (without HDR)>: ");
				if (fgets(buff, 256, stdin) != NULL) {
					if (sscanf(buff, "%i", &v1) == 1) {
						if (v1 == 0 || v1 == 1) {
							if ((ret = CTOFApiZ::setOperationMode(TOF_TIMEOUT, v1, &outStatus)) != TOF_NO_ERROR) {
								return ret;
							}
							if (outStatus != 0) {
								return outStatus;
							}
							printf("Complete!\n");
						}
					}
				}
				break;
			case 0x87: /* Get Operation Mode */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get Operation Mode\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::getOperationMode(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				v1 = *outData;
				cp = "Invalid Operation Mode!";
				if (v1 == 0) cp = "Normal(with HDR)";
				if (v1 == 1) cp = "High Speed (without HDR)";
				printf("Operation Mode setting is %d <%s>.\n\n", v1, cp);
				break;
			case 0x88: /* Set Exposure Time */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Set Exposure Time\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "Exposure Time <Normal Mode:170~5312, High Speed Mode:20~10000> [ms]: ");
				if (fgets(buff, 256, stdin) != NULL) {
					if (sscanf(buff, "%i", &v1) == 1) {
						if (20 <= v1 && v1 <= 10000) {
							printf("FPS <0~20>: ");
							if (fgets(buff, 256, stdin) != NULL) {
								if (sscanf(buff, "%i", &v2) == 1) {
									if (0 <= v2 && v2 <= 20) {
										if ((ret = CTOFApiZ::setExposureTime(TOF_TIMEOUT, v1, v2, &outStatus)) != TOF_NO_ERROR) {
											return ret;
										}
										if (outStatus != 0) {
											return outStatus;
										}
										printf("Complete!\n");
									}
								}
							}
						}
					}
				}
				break;
			case 0x89: /* Get Exposure Time */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get Exposure Time\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::getExposureTime(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				v1 = outData[0] * 0x100 + outData[1];
				v2 = outData[6];
				printf("Exposure Time is %dms.\n"
					   "FPS is %d.\n\n", v1, v2);
				break;
			case 0x8A: /* Set Rotation Angle */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Set Rotation Angle\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "Rotation Angle of X-axis <0~359> [degree]: ");
				if (fgets(buff, 256, stdin) != NULL) {
					if (sscanf(buff, "%i", &v1) == 1) {
						if (0 <= v1 && v1 <= 359) {
							printf("Rotation Angle of Y-axis <0~359> [degree]: ");
							if (scanf("%d", &v2) == 1) {
								if (0 <= v2 && v2 <= 359) {
									printf("Rotation Angle of Z-axis <0~359> [degree]: ");
									if (scanf("%d", &v3) == 1) {
										if (0 <= v3 && v3 <= 359) {
											if ((ret = CTOFApiZ::setRotationAngle(TOF_TIMEOUT, v1, v2, v3, &outStatus)) != TOF_NO_ERROR) {
												return ret;
											}
											if (outStatus != 0) {
												return outStatus;
											}
										}
									}
								}
							}
							printf("Complete!\n");
						}
					}
				}
				break;
			case 0x8B: /* Get Rotation Angle */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get Rotation Angle\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::getRotationAngle(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				v1 = outData[0] * 0x100 + outData[1];
				v2 = outData[2] * 0x100 + outData[3];
				v3 = outData[4] * 0x100 + outData[5];
				printf("Rotation Angles are: X-%ddegree, Y-%ddegree, Z-%ddegree\n\n", v1, v2, v3);
				break;
			case 0x8E: /* Set LED Frequency ID */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Set LED Frequency ID\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "LED Frequency ID <0~16>: ");
				if (fgets(buff, 256, stdin) != NULL) {
					if (sscanf(buff, "%i", &v1) == 1) {
						if (0 <= v1 || v1 <= 16) {
							if ((ret = CTOFApiZ::setLEDfrequencyID(TOF_TIMEOUT, v1, &outStatus)) != TOF_NO_ERROR) {
								return ret;
							}
							if (outStatus != 0) {
								return outStatus;
							}
							printf("Complete!\n");
						}
					}
				}
				break;
			case 0x8F: /* Get LED Frequency ID */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get LED Frequency ID\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::getLEDfrequencyID(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				printf("LED Frequency ID is %d.\n\n", *outData);
				break;
			case 0x90: /* Set Min AMP (All) */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Set Min AMP (All)\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "Minimum Amplitude for All Range <0~200>: ");
				if (fgets(buff, 256, stdin) != NULL) {
					if (sscanf(buff, "%i", &v1) == 1) {
						if (0 <= v1 || v1 <= 200) {
							if ((ret = CTOFApiZ::setMinAmpAll(TOF_TIMEOUT, v1, &outStatus)) != TOF_NO_ERROR) {
								return ret;
							}
							if (outStatus != 0) {
								return outStatus;
							}
							printf("Complete!\n");
						}
					}
				}
				break;
			case 0x91: /* Get Min AMP (All) */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get Min AMP (All)\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::getMinAmpAll(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				printf("Minimum Amplitude for All Range is %d.\n\n", *outData);
				break;
			case 0x92: /* Set Min AMP (Near) */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Set Min AMP (Near)\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "Minimum Amplitude for Near Range <0~200>: ");
				if (fgets(buff, 256, stdin) != NULL) {
					if (sscanf(buff, "%i", &v1) == 1) {
						if (0 <= v1 || v1 <= 200) {
							if ((ret = CTOFApiZ::setMinAmpNear(TOF_TIMEOUT, v1, &outStatus)) != TOF_NO_ERROR) {
								return ret;
							}
							if (outStatus != 0) {
								return outStatus;
							}
							printf("Complete!\n");
						}
					}
				}
				break;
			case 0x93: /* Get Min AMP (Near) */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get Min AMP (Near)\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::getMinAmpNear(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				printf("Minimum Amplitude for Near Range is %d.\n\n", *outData);
				break;
			case 0x94: /* Get THETA-PHI Table */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get THETA-PHI Table\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "Output File Name: ");
				if (fgets(buff, 256, stdin) != NULL) {
					if (buff[strlen(buff) - 1] == '\n') buff[strlen(buff) - 1] = '\0';
					if (*buff != '\0') {
						if ((ret = CTOFApiZ::getThetaPhiTable(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
							return ret;
						}
						if (outStatus != 0) {
							return outStatus;
						}

						/* Output THETA-PHI Table */
						if (RAWfile(buff, outData, 0x4B000) < 0) {
							ret = -99;
							return ret;
						}

						printf("Complete!\n");
					}
				}
				break;
			case 0x95: /* Set Operation Check LED */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Set Operation Check LED\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "Operation Check LED Enable/Disable Flag <0:Enable or 1:Disable>: ");
				if (fgets(buff, 256, stdin) != NULL) {
					if (sscanf(buff, "%i", &v1) == 1) {
						if (v1 == 0 || v1 == 1) {
							if ((ret = CTOFApiZ::setOperationCheckLED(TOF_TIMEOUT, v1, &outStatus)) != TOF_NO_ERROR) {
								return ret;
							}
							if (outStatus != 0) {
								return outStatus;
							}
							printf("Complete!\n");
						}
					}
				}
				break;
			case 0x96: /* Get Operation Check LED */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get Operation Check LED\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::getOperationCheckLED(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				v1 = *outData;
				cp = "Invalid Operation Check LED Flag!";
				if (v1 == 0) cp = "Enable";
				if (v1 == 1) cp = "Disable";
				printf("Operation Check LED Enable/Disable Flag is %d <%s>.\n\n", v1, cp);
				break;
			case 0x97: /* Set Response Speed */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Set Response Speed\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "Transmission size <1, 2, 4, 8 or 16> [KB]: ");
				if (fgets(buff, 256, stdin) != NULL) {
					if (sscanf(buff, "%i", &v1) == 1) {
						if (v1 == 1 || v1 == 2 || v1 == 4 || v1 == 8 || v1 == 16) {
							printf("Transmission interval (0~10000) [us]: ");
							if (scanf("%i", &v2) == 1) {
								if (0 <= v2 && v2 <= 10000) {
									if ((ret = CTOFApiZ::setResponseSpeed(TOF_TIMEOUT, v1, v2, &outStatus)) != TOF_NO_ERROR) {
										return ret;
									}
									if (outStatus != 0) {
										return outStatus;
									}
									printf("Complete!\n");
								}
							}
						}
					}
				}
				break;
			case 0x98: /* Get Response Speed */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get Response Speed\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::getResponseSpeed(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				v1 = outData[0];
				v2 = outData[1] * 0x100 + outData[2];
				printf("Transmission size is %dKB.\n"
					   "Transmission interval is %dus.\n\n", v1, v2);
				break;
			case 0x99: /* Set ENR Treshold */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Set ENR Treshold\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "ENR(Edge Noise Removal) Threshold <0~12499> [mm]: ");
				if (fgets(buff, 256, stdin) != NULL) {
					if (sscanf(buff, "%i", &v1) == 1) {
						if (0 <= v1 || v1 <= 12499) {
							if ((ret = CTOFApiZ::setENRthreshold(TOF_TIMEOUT, v1, &outStatus)) != TOF_NO_ERROR) {
								return ret;
							}
							if (outStatus != 0) {
								return outStatus;
							}
							printf("Complete!\n");
						}
					}
				}
				break;
			case 0x9A: /* Get ENR Treshold */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get ENR Treshold\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::getENRthreshold(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				printf("ENR(Edge Noise Removal) Threshold is %dmm.\n\n", outData[0] * 0x100 + outData[1]);
				break;
			case 0x9B: /* Get Imager Temparature */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get Imager Temparature\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::getImagerTemparature(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				printf("Imager Temparature <Upper-Left>  is %.1fdegree-celsius.\n",   (outData[0] * 0x100 + outData[1]) / 10.0);
				printf("Imager Temparature <Upper-Right> is %.1fdegree-celsius.\n",   (outData[2] * 0x100 + outData[3]) / 10.0);
				printf("Imager Temparature <Lower-Left>  is %.1fdegree-celsius.\n",   (outData[4] * 0x100 + outData[5]) / 10.0);
				printf("Imager Temparature <Lower-Right> is %.1fdegree-celsius.\n\n", (outData[6] * 0x100 + outData[7]) / 10.0);
				break;
			case 0x9C: /* Get LED Temparature */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Get LED Temparature\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::getLEDTemparature(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				printf("LED Temparature is %.1fdegree-celsius.\n\n", (outData[0] * 0x100 + outData[1]) / 10.0);
				break;
			case 0x9E: /* Initialize Parameters */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Initialize Parameters\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::initializeParameters(PARAMETER_RESET_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				printf("Complete!\n");
				break;
			case 0x9F: /* Software RESET */
				printf("\n\n"
					   "++++++++++++++++++++++++++++++++++++++++\n"
					   "  Software RESET\n"
					   "++++++++++++++++++++++++++++++++++++++++\n");
				if ((ret = CTOFApiZ::softwareReset(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
					return ret;
				}
				if (outStatus != 0) {
					return outStatus;
				}
				printf("Complete!\n"
					   "The USB connection will be disconnected.\n"
					   "Please re-connect after 30 seconds.\n\n");
				return 1;
			default:
				printf("No such Command.\n\n");
		}
	}

	return 0;
}


int SetParameter(char *pcFileName)
{
	UINT8	outStatus, *outData;
	FILE	*fp;
	int		c, x, y, z, x1, y1, z1, n, th, sw, ret = 0;
	char	buff[256], cmd[256], value[256], *p;

	/* Open Parameter File */
	if ((fp = fopen(pcFileName, "rt")) == NULL) {
		fprintf(stderr, "Cannot read the Parameter File: [%s]\n", pcFileName);
		return -1;
	}

	/* Parse Parameter File */
	while(fgets(buff, 256, fp) != NULL) {

		/* Determine EOL */
		for(p = buff; *p != '\0' && *p != '#' && *p != ';'; p++);
		*p = '\0';

		/* Skip leading whitespace */
		for(p = buff; *p == ' ' || *p == '\t'; p++);

		/* Parse Key and Value */
		c = sscanf(p, "%[^= \t]=%[^\n]", cmd, value);
		if (c == EOF || c < 2) continue;

		/* Find Key */
		for(p = cmd; *p != '\0'; p++) {
			*p = toupper(*p);
		}
		for(c = 0; c < 13; c++) {
			if (strcmp(ucParameters[c], cmd) == 0) break;
		}
		ret = 0;

		switch(c) {
		case 0: /* IMAGE_PATH */
			if (value[strlen(value) - 1] != '/') {
				sprintf(ImagePath, "%s/", value);
			}
			else {
				sprintf(ImagePath, "%s", value);
			}
			break;
		case 1: /* FOV_LIMITATION */
			if (sscanf(value, "%i", &x) != 1) {
				ret = 1;
				break;
			}
			if (x != 0 && x != 1) {
				ret = 1;
				break;
			}
			FOV_Limitation = x;
			break;
		case 2: /* DETECTION_CUBOID */
			if (sscanf(value, "%i,%i,%i,%i,%i,%i,%i,%i,%n", &n, &x, &y, &z, &x1, &y1, &z1, &th, &sw) != 8) {
				ret = 1;
				break;
			}
			if (n < 0 || n >= MAX_CUBOID) {
				ret = 1;
				break;
			}
			if (x > x1) {
				sw = x;
				x = x1;
				x1 = sw;
			}
			if (y > y1) {
				sw = y;
				y = y1;
				y1 = sw;
			}
			if (z > z1) {
				sw = z;
				z = z1;
				z1 = sw;
			}
			DetectionCuboid[n][0] = x;
			DetectionCuboid[n][1] = y;
			DetectionCuboid[n][2] = z;
			DetectionCuboid[n][3] = x1;
			DetectionCuboid[n][4] = y1;
			DetectionCuboid[n][5] = z1;
			DetectionCuboid[n][6] = th;
			strncpy(CuboidCommand[n], value + sw, 255);
			break;
		case 3: /* OUTPUT_FORMAT */
			if (sscanf(value, "%i", &x) != 1) {
				ret = 1;
				break;
			}
			if (x != ToF_Format) {
				if ((ret = CTOFApiZ::setOutputFormat(TOF_TIMEOUT, x, &outStatus)) != TOF_NO_ERROR) {
					fprintf(stderr, "CTOFApiZ::setOutputFormat() has been terminated with error code %d.\n", ret);
				}
				ToF_Format = x;
			}
			break;
		case 4: /* OPERATION_MODE */
			if (sscanf(value, "%i", &x) != 1) {
				ret = 1;
				break;
			}
			if ((ret = CTOFApiZ::getOperationMode(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
				fprintf(stderr, "CTOFApiZ::getOperationMode() has been terminated with error code %d.\n", ret);
				break;
			}
			if (outStatus != 0) {
				fprintf(stderr, "ToF Sensor responded with error code %d for GetOperationMode(0x87) command.\n", outStatus);
				break;
			}
			if (x != outData[0]) {
				if ((ret = CTOFApiZ::setOperationMode(TOF_TIMEOUT, x, &outStatus)) != TOF_NO_ERROR) {
					fprintf(stderr, "CTOFApiZ::setOperationMode() has been terminated with error code %d.\n", ret);
				}
			}
			break;
		case 5: /* EXPOSURE_FPS */
			if (sscanf(value, "%i,%i", &x, &y) != 2) {
				ret = 1;
				break;
			}
			if ((ret = CTOFApiZ::getExposureTime(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
				fprintf(stderr, "CTOFApiZ::getExposureTime() has been terminated with error code %d.\n", ret);
				break;
			}
			if (outStatus != 0) {
				fprintf(stderr, "ToF Sensor responded with error code %d for GetExposureTime(0x89) command.\n", outStatus);
				break;
			}
			if (x != outData[0] * 256 + outData[1] || y != outData[6]) {
				if ((ret = CTOFApiZ::setExposureTime(TOF_TIMEOUT, x, y, &outStatus)) != TOF_NO_ERROR) {
					fprintf(stderr, "CTOFApiZ::setExposureTime() has been terminated with error code %d.\n", ret);
				}
			}
			break;
		case 6: /* T3D_XYZ */
			if (sscanf(value, "%i,%i,%i", &x, &y, &z) != 3) {
				ret = 1;
				break;
			}
			if ((ret = CTOFApiZ::getRotationAngle(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
				fprintf(stderr, "CTOFApiZ::getRotationAngle() has been terminated with error code %d.\n", ret);
				break;
			}
			if (outStatus != 0) {
				fprintf(stderr, "ToF Sensor responded with error code %d for GetRotationAngle(0x8B) command.\n", outStatus);
				break;
			}
			if (x != outData[0] * 256 + outData[1] || y != outData[2] * 256 + outData[3] || z != outData[4] * 256 + outData[5]) {
				if ((ret = CTOFApiZ::setRotationAngle(TOF_TIMEOUT, x, y, z, &outStatus)) != TOF_NO_ERROR) {
					fprintf(stderr, "CTOFApiZsetRotationAngle() has been terminated with error code %d.\n", ret);
				}
			}
			break;
		case 7: /* LED_FREQUENCY_ID */
			if (sscanf(value, "%i", &x) != 1) {
				ret = 1;
				break;
			}
			if ((ret = CTOFApiZ::getLEDfrequencyID(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
				fprintf(stderr, "CTOFApiZ::getLEDfrequencyID() has been terminated with error code %d.\n", ret);
				break;
			}
			if (outStatus != 0) {
				fprintf(stderr, "ToF Sensor responded with error code %d for GetLEDfrequencyID(0x8F) command.\n", outStatus);
				break;
			}
			if (x != outData[0]) {
				if ((ret = CTOFApiZ::setLEDfrequencyID(TOF_TIMEOUT, x, &outStatus)) != TOF_NO_ERROR) {
					fprintf(stderr, "CTOFApiZ::setLEDfrequencyID() has been terminated with error code %d.\n", ret);
				}
			}
			break;
		case 8: /* MIN_AMP_ALL */
			if (sscanf(value, "%i", &x) != 1) {
				ret = 1;
				break;
			}
			if ((ret = CTOFApiZ::getMinAmpAll(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
				fprintf(stderr, "CTOFApiZ::getMinAmpAll() has been terminated with error code %d.\n", ret);
				break;
			}
			if (outStatus != 0) {
				fprintf(stderr, "ToF Sensor responded with error code %d for GetMinAmpAll(0x91) command.\n", outStatus);
				break;
			}
			if (x != outData[0]) {
				if ((ret = CTOFApiZ::setMinAmpAll(TOF_TIMEOUT, x, &outStatus)) != TOF_NO_ERROR) {
					fprintf(stderr, "CTOFApiZ::setMinAmpAll() has been terminated with error code %d.\n", ret);
				}
			}
			break;
		case 9: /* MIN_AMP_NEAR */
			if (sscanf(value, "%i", &x) != 1) {
				ret = 1;
				break;
			}
			if ((ret = CTOFApiZ::getMinAmpNear(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
				fprintf(stderr, "CTOFApiZ::getMinAmpNear() has been terminated with error code %d.\n", ret);
				break;
			}
			if (outStatus != 0) {
				fprintf(stderr, "ToF Sensor responded with error code %d for GetMinAmpNear(0x93) command.\n", outStatus);
				break;
			}
			if (x != outData[0]) {
				if ((ret = CTOFApiZ::setMinAmpNear(TOF_TIMEOUT, x, &outStatus)) != TOF_NO_ERROR) {
					fprintf(stderr, "CTOFApiZ::setMinAmpNear() has been terminated with error code %d.\n", ret);
				}
			}
			break;
		case 10: /* OPERATION_CHECK_LED */
			if (sscanf(value, "%i", &x) != 1) {
				ret = 1;
				break;
			}
			if ((ret = CTOFApiZ::getOperationCheckLED(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
				fprintf(stderr, "CTOFApiZ::getOperationCheckLED() has been terminated with error code %d.\n", ret);
				break;
			}
			if (outStatus != 0) {
				fprintf(stderr, "ToF Sensor responded with error code %d for GetOperationCheckLED(0x96) command.\n", outStatus);
				break;
			}
			if (x != outData[0]) {
				if ((ret = CTOFApiZ::setOperationCheckLED(TOF_TIMEOUT, x, &outStatus)) != TOF_NO_ERROR) {
					fprintf(stderr, "CTOFApiZ::setOperationCheckLED() has been terminated with error code %d.\n", ret);
				}
			}
			break;
		case 11: /* RESPONSE_SPEED */
			if (sscanf(value, "%i,%i", &x, &y) != 2) {
				ret = 1;
				break;
			}
			if ((ret = CTOFApiZ::getResponseSpeed(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
				fprintf(stderr, "CTOFApiZ::getResponseSpeed() has been terminated with error code %d.\n", ret);
				break;
			}
			if (outStatus != 0) {
				fprintf(stderr, "ToF Sensor responded with error code %d for GetResponseSpeed(0x98) command.\n", outStatus);
				break;
			}
			if (x != outData[0] || y != outData[1] * 256 + outData[2]) {
				if ((ret = CTOFApiZ::setResponseSpeed(TOF_TIMEOUT, x, y, &outStatus)) != TOF_NO_ERROR) {
					fprintf(stderr, "CTOFApiZ::setResponseSpeed() has been terminated with error code %d.\n", ret);
				}
			}
			break;
		case 12: /* ENR_TRESHOLD */
			if (sscanf(value, "%i", &x) != 1) {
				ret = 1;
				break;
			}
			if ((ret = CTOFApiZ::getENRthreshold(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
				fprintf(stderr, "CTOFApiZ::getENRthreshold() has been terminated with error code %d.\n", ret);
				break;
			}
			if (outStatus != 0) {
				fprintf(stderr, "ToF Sensor responded with error code %d for GetENRthreshold(0x9A) command.\n", outStatus);
				break;
			}
			if (x != outData[0] * 256 + outData[1]) {
				if ((ret = CTOFApiZ::setENRthreshold(TOF_TIMEOUT, x, &outStatus)) != TOF_NO_ERROR) {
					fprintf(stderr, "CTOFApiZ::setENRthreshold() has been terminated with error code %d.\n", ret);
				}
			}
			break;
		default: /* ELSE */
			ret = 1;
		}
	}
	if (ret > 0) {
		fprintf(stderr, "Parameter File Read Error: %s\n", buff);
	}
	fclose(fp);
	return ret;
}


int main(int argc, char *argv[])
{
	S_STAT		stat;
	UINT8		outStatus, *outData;
	char		fname[1024], *p;
	INT32		ret, i, cmd, outSize, counter = 0, format, frames;
	double		finishTime = 0.0;
	struct TIMEVAL	sStartTime, sEndTime;


	/* Usage */
	fprintf(stderr, "[ToF_Sample Code]\n"
					"  Usage:\n"
					"    Interactive Mode     - ToF_Sample <Port No.>\n"
					"    Non-Interactive Mode - ToF_Sample <Port No.> <Parameter File> [Number of Frames (0: No Limit)]\n\n");
	if (argc < 2 || argc > 4) return 0;

	/* Open Serial Port */
#ifndef	_WIN32
	system("sudo modprobe usbserial vendor=0x0590 product=0x00ca");
#endif
	stat.com_num = atoi(argv[1]);
	stat.BaudRate = 921600;
	if (com_init(&stat) == 0) {
		fprintf(stderr, "Cannot open Deveice\n");
		return 1;
	}
#ifndef _WIN32
	fprintf(stderr, "/dev/ttyUSB%d is opened successfully.\n", stat.com_num);
#else
	fprintf(stderr, "\\\\.\\COM%d is opened successfully.\n", stat.com_num);
#endif

	/* Initialize InFOV */
	for (i = 0; i < 76800; InFOV[i++] = 1);

	/* Interactive Mode */
	if (argc == 2) {
		while((ret = Interactive(&cmd)) == 0);
		if (ret != 1) {
			fprintf(stderr, "ToF Command 0x%02X has been terminated with error code %d.\n", cmd, ret);
		}
	}
	/* Non-Interactive Mode */
	else {

		/* Set Out of FOV flag when FOV_Limitation is active */
		if (FOV_Limitation) {
			if ((ret = CTOFApiZ::getThetaPhiTable(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
				fprintf(stderr, "CTOFApiZ::getThetaPhiTable() has been terminated with error code %d.\n", ret);
				com_close();
				return 2;
			}
			if (outStatus != 0) {
				fprintf(stderr, "ToF Sensor responded with error code %d for GetThetaPhiTable(0x94) command.\n", outStatus);
				com_close();
				return 3;
			}

			/* Set In-FOV flags */
			for (i = PointNum = 0; i < 76800; i++) {
				if (InFOV[i] = ((outData[i * 2 + 1] & 0xf0) != 0xf0)) PointNum++;
			}
		}

		/* Get Version */
		if ((ret = CTOFApiZ::getVersion(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
			fprintf(stderr, "CTOFApiZ::getVersion() has been terminated with error code %d.\n", ret);
			com_close();
			return 4;
		}
		if (outStatus != 0) {
			fprintf(stderr, "ToF Sensor responded with error code %d for GetVersion(0x00) command.\n", outStatus);
			com_close();
			return 5;
		}
		fprintf(stderr, "OMRON ToF Sensor:  %.11s %d.%d.%d Revision:%d Serial:%.11s\n",
			outData, outData[11], outData[12], outData[13], (outData[14] << 24) + (outData[15] << 16) + (outData[16] << 8) + outData[17], outData + 18);

		/* Get Format setting */
		if ((ret = CTOFApiZ::getOutputFormat(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
			fprintf(stderr, "CTOFApiZ::getOutputFormat() has been terminated with error code %d.\n", ret);
			com_close();
			return 6;
		}
		if (outStatus != 0) {
			fprintf(stderr, "ToF Sensor responded with error code %d for GetOutputFormat(0x85) command.\n", outStatus);
			com_close();
			return 7;
		}
		ToF_Format = outData[0] * 256 + outData[1];

		/* Set Number of Frames */
		if (argc < 4) frames = -1;
		else {
			if ((frames = atoi(argv[3])) == 0) frames = -1;
		}

		/* Parameter Setting */
		if ((ret = SetParameter(argv[2])) != 0) {
			return ret;
		}

		/* Start Measurement */
		if ((ret = CTOFApiZ::startMeasurement(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
			fprintf(stderr, "CTOFApiZ::startMeasurement() has been terminated with error code %d.\n", ret);
			com_close();
			return 8;
		}
		if (outStatus != 0) {
			fprintf(stderr, "ToF Sensor responded with error code %d for StartMeasurement(0x80) command.\n", outStatus);
			com_close();
			return 9;
		}

		/* Set Ctrl.+C Handler */
		TRAP_START();
		printf("ToF Measurement was started.\n"
			   "When you want to stop, please press Ctrl.+C.\n");

		/* Set Format */
		format = ToF_Format;
		if (RAW_OUTPUT) {
			format = -format;
		}

		/* Main Loop */
		GETTIMEOFDAY(&sStartTime, NULL);
		while(TreminationFlag == 0 && counter != frames) {

			/* Get Measurement Result */
			if ((ret = CTOFApiZ::getMesurementResult(TOF_TIMEOUT, 0, &outStatus, &outSize, &outData)) != TOF_NO_ERROR) {
				fprintf(stderr, "CTOFApiZ::getMesurementResult() has been terminated with error code %d.\n", ret);
				ret = 10;
				break;
			}
			if (outStatus != 0) {
				fprintf(stderr, "ToF Sensor responded with error code %d for GetMesurementResult(0x82) command.\n", outStatus);
				ret = 11;
				break;
			}
			if (TreminationFlag == 1) {
				break;
			}

			/* Reset Cuboid Counter */
			for (i = 0; i < MAX_CUBOID; CuboidCount[i++] = 0);

			/* Output Data File */
			counter++;
			if (*ImagePath == '\0') {
				p = NULL;
			}
			else {
				sprintf(p = fname, "%sToF_Output_%04d", ImagePath, counter);
			}
			if (ImageOutput(format, p, outData, outSize) < 0) {
				fprintf(stderr, "File output error. [%s]\n", fname);
				ret = 12;
				break;
			}

			/* Check Detection Cuboid */
			for (i = 0; i < MAX_CUBOID; i++) {
				if (DetectionCuboid[i][6] != 0) {
					if (DetectionCuboid[i][6] > 0) {
						if (CuboidCount[i] >= DetectionCuboid[i][6]) {
							system(CuboidCommand[i]);
						}
					}
					else {
						if (CuboidCount[i] < -DetectionCuboid[i][6]) {
							system(CuboidCommand[i]);
						}
					}
				}
			}
		}

		/* Termination */
		GETTIMEOFDAY(&sEndTime, NULL);
		finishTime = difftime(sEndTime.tv_sec, sStartTime.tv_sec) * 1000.0 + (sEndTime.TV_USEC - sStartTime.TV_USEC) / TO_MSEC;
		printf("FPS: %.1f\n", counter / finishTime * 1000.0);
		if ((ret = CTOFApiZ::stopMeasurement(TOF_TIMEOUT, &outStatus, &outData)) != TOF_NO_ERROR) {
			fprintf(stderr, "CTOFApiZ::stopMeasurement() has been terminated with error code %d.\n", ret);
		}
		else if (outStatus != 0) {
			fprintf(stderr, "ToF Sensor responded with error code %d for StopMeasurement(0x00) command.\n", outStatus);
		}
	}

	/* Close Com. Port */
	com_close();
	TRAP_END();
	printf("\rTermination Complete!\n");
	return ret;
}

