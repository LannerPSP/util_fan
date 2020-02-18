
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/io.h>
#include <fcntl.h>

#define TYPE_256B	0
#define TYPE_512B	1
#define TYPE_1KB	2
#define TYPE_2KB	3
#define TYPE_4KB	4
#define TYPE_8KB	5
#define TYPE_16KB	6
#define TYPE_32KB	7
#define TYPE_64KB	8
#define TYPE_128KB	9

#include "smbus.c"

uint8_t ubI2cAddr=0;

int ascii_to_hex(char ch) 
{ 
char ch_tmp; 
int hex_val = -1; 

	ch_tmp = tolower(ch); 

	if ((ch_tmp >= '0') && (ch_tmp <= '9')) { 
		hex_val = ch_tmp - '0'; 
	} 
	else if ((ch_tmp >= 'a') && (ch_tmp <= 'f')) { 
		hex_val = ch_tmp - 'a' + 10; 
	} 

	return hex_val; 
} 
/********/
int str_to_hex(char *hex_str) 
{
int i, len; 
int hex_tmp, hex_val; 
char *bptr;
int fHEX=0;
	bptr = strstr(hex_str, "0x");
	if ( bptr != NULL ) {
		bptr+=2;
		fHEX=1;
	}
	else 	bptr=hex_str;

	if (fHEX==1 ){
		len = (int)strlen(bptr); 
		hex_val = 0; 
	 	for (i=0; i<len;i++) { 
			hex_tmp = ascii_to_hex(bptr[i]); 
			if (hex_tmp == -1) { return -1; } 
			hex_val = (hex_val) * 16 + hex_tmp; 
		}
	}
	else {
		hex_val = atoi(bptr);
	} 
	return hex_val; 
} 
void __printf_usage(char *argv0)
{
	printf("Usage: %s -nct6776 -smart 			--> enable smart-fan function\n", argv0);
	printf("       %s -nct6776 -duty num#			--> disable smart fan and set pwm duty\n", argv0);
	printf("       %s -nct6776 -cpufan/-sysfan/-auxfan0..2	--> reading fan speed rpm\n", argv0);
	printf("       %s -nct6776 -speed rpm#			--> disable smart fan and set fan speed\n", argv0);
	printf("       %s -nct7904 -tech pin# (1~12)		--> reading tech pin speed rpm (only NCT-7904)\n", argv0);
	printf("	prarmeter:\n");
	printf("	  -nct6766: SuperIO NCT-6766\n");
	printf("	  -ft81866: SuperIO FT-81866\n");
	printf("	  -ft81865: SuperIO FT-81865\n");
	printf("	  -nct6116: SuperIO NCT-6116\n");
	printf("	  -nct7904: SMBus   NCT-7904D\n");
}


void _err_printf(char * pbStirng)
{
	printf("\e[1;31m%s\e[m\n",pbStirng);

}
void __error_exit(char *argv0)
{
	printf("\e[1;31m<Error> command or function doen't input !!!\e[m\n");
	__printf_usage(argv0);
	exit(-1);
}

uint8_t SIO_INDEX =0;
uint8_t SIO_DATA =0;
uint16_t wHWM_INDEX =0;
uint16_t wHWM_DATA =0;

#define DEV_NCT6776	0
#define DEV_FT81866	1
#define DEV_FT81865	2
#define DEV_NCT6116	3
#define DEV_NCT7904	4
#define DEV_TOGTAL	DEV_NCT7904+1
#define DEV_SKIP	0xFF

#define SEL_DISABLE	1
#define SEL_ENABLE	2
#define SEL_CPUFAN	3
#define SEL_SYSFAN	4
#define SEL_AUXFAN0	5
#define SEL_AUXFAN1	6
#define SEL_AUXFAN2	7
#define SEL_FANSPEED	8
#define SEL_READFAN	9

char dev_name[DEV_TOGTAL][30]= {"NCT-6776", "FT-81866", "FT-81865", "NCT-6116"};
uint16_t DEV_VENDOR[DEV_TOGTAL]={0x5ca3, 0x1934, 0x1934, 0x5ca3};
uint16_t DEV_CHIPID[DEV_TOGTAL]={0x00C1, 0x1010, 0x0704, 0x00C1};

typedef struct DEF_SIO_LDNLIST {
	int	iLDN;
	char	strLdnName[30];

}SIO_LDNLIST;

uint8_t SIO_HWM_LDN[]={
0x0B, //NCT-6776
0x04, //FT-81866
0x04, //FT-81865
0x0B, //NCT-61166
0xFF, //NCT-7904
};


int __check_hardware(int iDevice);

/********************************************************************/
/***** SuperIO access functions *************************************/
void __sio_unlock(void)
{
	ioperm(SIO_INDEX, 2, 1);
	outb(0x87 , SIO_INDEX);
	outb(0x87 , SIO_INDEX);
}
/***********/
void __sio_lock(void)
{
	outb(0xaa , SIO_INDEX);
	ioperm(SIO_INDEX, 2, 0);
}
/***********/
void __sio_logic_device(char num)
{
	outb(0x7 , SIO_INDEX);
	outb(num , SIO_DATA);
}
/************/
uint8_t read_sio_reg(uint8_t LDN, uint8_t reg)
{
        outb(0x07, SIO_INDEX); //LDN register
        outb(LDN, SIO_DATA);
        outb(reg , SIO_INDEX);
        return inb(SIO_DATA);
}
/************/
uint8_t write_sio_reg(uint8_t LDN, uint8_t reg, uint8_t value)
{	
        outb(0x07, SIO_INDEX); //LDN register
        outb(LDN, SIO_DATA);
        outb(reg, SIO_INDEX);
        outb(value, SIO_DATA);
        return 0;
}

uint8_t _write_hwm_data(uint8_t ubBank, uint8_t ubReg, uint8_t ubData)
{
	//set bank
	ioperm(wHWM_INDEX, 2, 1);
	outb(0x4E, wHWM_INDEX);
	outb(ubBank, wHWM_DATA);
	outb(ubReg, wHWM_INDEX);
	outb(ubData, wHWM_DATA);
	ioperm(wHWM_INDEX, 2, 1);
	return 0;
	
}

uint8_t _read_hwm_data(uint8_t ubBank, uint8_t ubReg)
{
uint8_t ubdata;
	//set bank
	ioperm(wHWM_INDEX, 2, 1);
	outb(0x4E, wHWM_INDEX);
	outb(ubBank, wHWM_DATA);
	outb(ubReg, wHWM_INDEX);
	ubdata = (uint8_t)inb(wHWM_DATA);
	ioperm(wHWM_INDEX, 2, 1);
	return ubdata;
	
}
#define BANK0	0
#define BANK1	1
#define BANK2	2
#define BANK3	3
#define BANK4	4
#define BANK5	5
#define BANK6	6
#define BANK7	7

typedef struct DEF_SIO_FANREG {
	uint8_t	ubBank;
	uint8_t	ubIndex;
	uint8_t ubSmart;
	uint8_t ubManaul;	
	uint8_t ubPwmBank;
	uint8_t ubPwmIndex;
}SIO_FANREG;

SIO_FANREG strFanCtl[]= {
{BANK2, 0x02, 0x40,0x00, BANK2,0x09 },//NCT-6776
{BANK2, 0x02, 0x40,0x00, BANK2,0x09 },//FT-81866
{BANK2, 0x02, 0x40,0x00, BANK2,0x09 },//FT-81865
{BANK2, 0x02, 0x40,0x00, BANK2,0x09 },//NCT-6116
{BANK3, 0x00, 0x01,0x00, BANK3,0x10 },//NCT-7904
};

typedef struct DEF_SIO_FANIN_REG {
	uint8_t	bCPU_Bank;
	uint8_t	bCPU_High;
	uint8_t	bCPU_Low;
	uint8_t	bSYS_Bank;
	uint8_t	bSYS_High;
	uint8_t	bSYS_Low;
	uint8_t	bAUX0_Bank;
	uint8_t	bAUX0_High;
	uint8_t	bAUX0_Low;
	uint8_t	bAUX1_Bank;
	uint8_t	bAUX1_High;
	uint8_t	bAUX1_Low;
	uint8_t	bAUX2_Bank;
	uint8_t	bAUX2_High;
	uint8_t	bAUX2_Low;
	uint8_t bDuty_Bank;
	uint8_t bCPU_DutyReg;
	uint8_t bSYS_DutyReg;
	uint8_t bAUX_DutyReg;
}SIO_FANIN_REG;

SIO_FANIN_REG strFanIn[] = {
{BANK6, 0x58, 0x59,BANK6, 0x56, 0x57, BANK6, 0x5A, 0x5B, BANK6, 0x5C, 0x5D,  BANK6, 0x5E, 0x5F, BANK0, 0x03, 0x09, 0x11 },//NCT-6776
{BANK6, 0x58, 0x59,BANK6, 0x56, 0x57, BANK6, 0x5A, 0x5B, BANK0, 0x5C, 0x5D,  BANK6, 0x5E, 0x5F, BANK0, 0x03, 0x09, 0x11 },//FT-81866
{BANK6, 0x58, 0x59,BANK6, 0x56, 0x57, BANK6, 0x5A, 0x5B, BANK0, 0x5C, 0x5D,  BANK6, 0x5E, 0x5F, BANK0, 0x03, 0x09, 0x11 },//FT-81865
{BANK6, 0x58, 0x59,BANK6, 0x56, 0x57, BANK6, 0x5A, 0x5B, BANK0, 0x5C, 0x5D,  BANK6, 0x5E, 0x5F, BANK0, 0x03, 0x09, 0x11 },//NCT-6116
{BANK0, 0x80, 0x81,BANK0, 0x82, 0x83, BANK0, 0x84, 0x85, BANK0, 0x86, 0x87,  BANK0, 0x88, 0x89, BANK3, 0x10, 0x11, 0x12 },//NCT-7904
};


int main(int argc, char **argv) 
{
int iRet =0, xi,xj, iDevice=-1, iLDN=-1, iSelect=-1, iIndex=-1;
int dwData=0x00;
uint8_t ubReg=0xFF, ubData, ubDuty=0x80;
int fRegInput=0, fSkip=0;
uint16_t wFanRpm, uwSpeedRpm;
uint8_t bAddr, bReg;


	if ( getuid() != 0 ) {
		_err_printf("<Warning> Please uses root user !!!");
		return -1;
	}
	for ( xi= 1; xi< argc ; xi++ ) {
		if 	( strcmp("-nct6776", argv[xi]) == 0 ) iDevice = DEV_NCT6776;
		else if ( strcmp("-ft81866", argv[xi]) == 0 ) iDevice = DEV_FT81866;
		else if ( strcmp("-ft81865", argv[xi]) == 0 ) iDevice = DEV_FT81865;
		else if ( strcmp("-nct6116", argv[xi]) == 0 ) iDevice = DEV_NCT6116;
		else if ( strcmp("-nct7904", argv[xi]) == 0 ) iDevice = DEV_NCT7904;
		//else if ( strcmp("-disable", argv[xi]) == 0 ) iSelect = SEL_DISABLE;
		else if ( strcmp("-smart", argv[xi]) == 0 ) iSelect = SEL_ENABLE;
		else if ( strcmp("-cpufan", argv[xi]) == 0 ) iSelect = SEL_CPUFAN;
		else if ( strcmp("-sysfan", argv[xi]) == 0 ) iSelect = SEL_SYSFAN;
		else if ( strcmp("-auxfan0", argv[xi]) == 0 ) iSelect = SEL_AUXFAN0;
		else if ( strcmp("-auxfan1", argv[xi]) == 0 ) iSelect = SEL_AUXFAN1;
		else if ( strcmp("-auxfan2", argv[xi]) == 0 ) iSelect = SEL_AUXFAN2;
		else if ( strcmp("-tech", argv[xi]) == 0 ) { 
			iSelect = SEL_READFAN;
			if ( xi == argc-1 ) {
				_err_printf("<Warning> not input fan number !!!");
				__printf_usage(argv[0]);
				return -1;
			}
			uwSpeedRpm =  (uint16_t)str_to_hex(argv[xi+1]);
			xi++;
		}
		else if ( strcmp("-speed", argv[xi]) == 0 ) { 
			iSelect = SEL_FANSPEED;
			if ( xi == argc-1 ) {
				_err_printf("<Warning> not input fan rpm value !!!");
				__printf_usage(argv[0]);
				return -1;
			}
			uwSpeedRpm =  (uint16_t)str_to_hex(argv[xi+1]);
			xi++;
		}
		else if ( strcmp("-duty", argv[xi]) == 0 ) { 
			iSelect = SEL_DISABLE;
			if ( xi == argc-1 ) {
				_err_printf("<Warning> not input PWM duty 0 ~ 255 !!!");
				__printf_usage(argv[0]);
				return -1;
			}
			ubDuty =  (uint8_t)(str_to_hex(argv[xi+1]) & 0xFF);
			xi++;
		}

		else {
			_err_printf("<Warning> invalid command or parameter input");
			__printf_usage(argv[0]);
			return -1;
		}
	}

	if ( iDevice==-1 && fSkip==0 ) { //if no assigned device
		__printf_usage(argv[0]);
		return -1;
	}

if ( iDevice !=  DEV_NCT7904 ) {

	/*****************************************************************/
	switch (iSelect) {
	case SEL_DISABLE: //disable smart-fan function
		iRet = __check_hardware(iDevice);
		if ( !iRet ) { 
			printf("\e[1;31m<Error> device %s not found !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		//printf("Hardware Monitor Address is 0x%X\n", wHWM_INDEX);
		//disable smart fan
		_write_hwm_data(strFanCtl[iDevice].ubBank, strFanCtl[iDevice].ubIndex,  strFanCtl[iDevice].ubManaul);

		//ubData = _read_hwm_data(strFanCtl[iDevice].ubBank, strFanCtl[iDevice].ubIndex);
		//printf("Fan Control Mode = 0x%X\n", ubData );

		//write PWM duty
		_write_hwm_data(strFanCtl[iDevice].ubPwmBank, strFanCtl[iDevice].ubPwmIndex, ubDuty);
		//ubData = _read_hwm_data(0, 0x03);
		//printf("CPU Fanout value= 0x%X\n", ubData);
		printf("Disbaled smart-fan, set duty is OK.\n");
		break;
	case SEL_ENABLE: //enable smart-fan function
		iRet = __check_hardware(iDevice);
		if ( !iRet ) { 
			printf("\e[1;31m<Error> device %s not found !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		//printf("Hardware Monitor Address is 0x%X\n", wHWM_INDEX);
		//enable smart fan
		_write_hwm_data(strFanCtl[iDevice].ubBank, strFanCtl[iDevice].ubIndex,  strFanCtl[iDevice].ubSmart);

		//ubData = _read_hwm_data(strFanCtl[iDevice].ubBank, strFanCtl[iDevice].ubIndex);
		//printf("Fan Control Mode = 0x%X\n", ubData);

		//ubData = _read_hwm_data(0, 0x03);
		//printf("CPU Fanout value= 0x%X\n", ubData);
		printf("Enabled smart fan OK.\n");		

		break;
	case SEL_CPUFAN: //reading CPUFAN speed
		iRet = __check_hardware(iDevice);
		if ( !iRet ) { 
			printf("\e[1;31m<Error> device %s not found !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		wFanRpm = (uint16_t)_read_hwm_data(strFanIn[iDevice].bCPU_Bank, strFanIn[iDevice].bCPU_High) << 8;
		wFanRpm += (uint16_t)_read_hwm_data(strFanIn[iDevice].bCPU_Bank, strFanIn[iDevice].bCPU_Low) ;
		ubDuty = _read_hwm_data(strFanIn[iDevice].bDuty_Bank, strFanIn[iDevice].bCPU_DutyReg);
		printf("SIO %s CPUFAN speed = %d, Duty=%d\n", dev_name[iDevice],wFanRpm , ubDuty);
		break;
	case SEL_SYSFAN: //reading SYSFAN speed
		iRet = __check_hardware(iDevice);
		if ( !iRet ) { 
			printf("\e[1;31m<Error> device %s not found !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		wFanRpm = (uint16_t)_read_hwm_data(strFanIn[iDevice].bSYS_Bank, strFanIn[iDevice].bSYS_High) << 8;
		wFanRpm += (uint16_t)_read_hwm_data(strFanIn[iDevice].bSYS_Bank, strFanIn[iDevice].bSYS_Low) ;
		printf("SIO %s SYSFAN speed = %d\n", dev_name[iDevice],wFanRpm);
		break;
	case SEL_AUXFAN0: //reading AUXFAN0 speed
		iRet = __check_hardware(iDevice);
		if ( !iRet ) { 
			printf("\e[1;31m<Error> device %s not found !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		wFanRpm = (uint16_t)_read_hwm_data(strFanIn[iDevice].bAUX0_Bank, strFanIn[iDevice].bAUX0_High) << 8;
		wFanRpm += (uint16_t)_read_hwm_data(strFanIn[iDevice].bAUX0_Bank, strFanIn[iDevice].bAUX0_Low) ;
		printf("SIO %s AUXFAN0 speed = %d\n", dev_name[iDevice],wFanRpm);
		break;
	case SEL_AUXFAN1: //reading AUXFAN1 speed
		iRet = __check_hardware(iDevice);
		if ( !iRet ) { 
			printf("\e[1;31m<Error> device %s not found !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		wFanRpm = (uint16_t)_read_hwm_data(strFanIn[iDevice].bAUX1_Bank, strFanIn[iDevice].bAUX1_High) << 8;
		wFanRpm += (uint16_t)_read_hwm_data(strFanIn[iDevice].bAUX1_Bank, strFanIn[iDevice].bAUX1_Low) ;
		printf("SIO %s AUXFAN1 speed = %d\n", dev_name[iDevice],wFanRpm);
		break;
	case SEL_AUXFAN2: //reading AUXFAN1 speed
		iRet = __check_hardware(iDevice);
		if ( !iRet ) { 
			printf("\e[1;31m<Error> device %s not found !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		wFanRpm = (uint16_t)_read_hwm_data(strFanIn[iDevice].bAUX2_Bank, strFanIn[iDevice].bAUX2_High) << 8;
		wFanRpm += (uint16_t)_read_hwm_data(strFanIn[iDevice].bAUX2_Bank, strFanIn[iDevice].bAUX2_Low) ;
		printf("SIO %s AUXFAN2 speed = %d\n", dev_name[iDevice],wFanRpm);
		break;
	case SEL_FANSPEED:
		iRet = __check_hardware(iDevice);
		if ( !iRet ) { 
			printf("\e[1;31m<Error> device %s not found !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		ubDuty = _read_hwm_data(0, 0x03);//_read_hwm_data(strFanCtl[iDevice].ubPwmBank, strFanCtl[iDevice].ubPwmIndex);
		_write_hwm_data(strFanCtl[iDevice].ubBank, strFanCtl[iDevice].ubIndex,  strFanCtl[iDevice].ubManaul);
		printf("Disabled smart fan OK.\n");

		while(1) {
			wFanRpm = (uint16_t)_read_hwm_data(strFanIn[iDevice].bCPU_Bank, strFanIn[iDevice].bCPU_High) << 8;
			wFanRpm += (uint16_t)_read_hwm_data(strFanIn[iDevice].bCPU_Bank, strFanIn[iDevice].bCPU_Low) ;
			
			printf("SIO %s CPUFAN speed = %d, Duty=%d\n", dev_name[iDevice],wFanRpm , ubDuty);
//printf("ubDuty=%d\n", ubDuty);
//printf("uwSpeedRpm=%d\n", uwSpeedRpm);
			if ( wFanRpm  > (uwSpeedRpm * 101 / 100) ) {
//printf("wFanRpm  > (uwSpeedRpm *1.1)\n");
				
				if ( ubDuty  >= 1 ) ubDuty -= 1;
				else 	break;
				_write_hwm_data(strFanCtl[iDevice].ubPwmBank, strFanCtl[iDevice].ubPwmIndex, ubDuty);
			}
			else if ( wFanRpm  < (uwSpeedRpm * 99 / 100) ) {
//printf("wFanRpm  < (uwSpeedRpm * 0.9)\n");
				if ( ubDuty  <= 254 ) ubDuty += 1;
				else 	break;
				_write_hwm_data(strFanCtl[iDevice].ubPwmBank, strFanCtl[iDevice].ubPwmIndex, ubDuty);
			}
			else {
				break;
			}
			usleep(500000);
		}
		break;

	default:
		__error_exit(argv[0]);
		break;
	}
}
else {
	if((i2cfd = open_i2c_adapter()) < 0) 	{
		_err_printf("<Warning> I2C/SMBus driver not found !!!");
		return -1;
	}
	ioctl(i2cfd, I2C_TIMEOUT,1);
	iRet = __check_hardware(iDevice);
	if ( !iRet ) { 
		printf("\e[1;31m<Error> device %s not found !!!\e[m\n", dev_name[iDevice]);
		return -1;
	}
	printf("Sensors Device SMBus Address = 0x%02X\n", ubI2cAddr >> 1);

	switch (iSelect) {
	case SEL_DISABLE: //disable smart-fan function
		ioctl(i2cfd, I2C_SLAVE, ubI2cAddr>>1); 
		bAddr = NCT7904_REG_BANK ; bReg=strFanCtl[iDevice].ubBank;
		iRet = i2c_smbus_write_byte_data(i2cfd, bAddr, bReg);
		if ( iRet < 0 ) { 
			printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		bAddr = strFanCtl[iDevice].ubIndex; bReg=strFanCtl[iDevice].ubManaul;
		iRet = i2c_smbus_write_byte_data(i2cfd, bAddr, bReg);
		if ( iRet < 0) { 
			printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		//write PWM duty
		bAddr = strFanCtl[iDevice].ubPwmIndex; bReg=ubDuty;
		iRet = i2c_smbus_write_byte_data(i2cfd, bAddr, bReg);
		if ( iRet < 0) { 
			printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		
		printf("Disbaled smart-fan, set duty is OK.\n");
		break;
	case SEL_ENABLE: //enable smart-fan function
		ioctl(i2cfd, I2C_SLAVE, ubI2cAddr>>1); 
		bAddr = NCT7904_REG_BANK ; bReg=strFanCtl[iDevice].ubBank;
		iRet = i2c_smbus_write_byte_data(i2cfd, bAddr, bReg);
		if ( iRet < 0 ) { 
			printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		bAddr = strFanCtl[iDevice].ubIndex; bReg=strFanCtl[iDevice].ubSmart;
		iRet = i2c_smbus_write_byte_data(i2cfd, bAddr, bReg);
		if ( iRet < 0) { 
			printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		printf("Enabled smart fan OK.\n");		

		break;
	case SEL_READFAN:
		ioctl(i2cfd, I2C_SLAVE, ubI2cAddr>>1); 
		bAddr = NCT7904_REG_BANK ; bReg=strFanIn[iDevice].bCPU_Bank;
		iRet = i2c_smbus_write_byte_data(i2cfd, bAddr, bReg);
		if ( iRet < 0 ) { 
			printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		bAddr = strFanIn[iDevice].bCPU_High + (uint8_t)(uwSpeedRpm-1)*2 ; 
		iRet = i2c_smbus_read_byte_data(i2cfd, bAddr);
		if ( iRet < 0 ) { 
			printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		dwData = iRet <<5 ;

		bAddr = strFanIn[iDevice].bCPU_Low + (uint8_t)(uwSpeedRpm-1)*2 ; 
		iRet = i2c_smbus_read_byte_data(i2cfd, bAddr);
		if ( iRet < 0 ) { 
			printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		dwData += iRet & 0x1F ;
	
		wFanRpm=(uint16_t)((float)1350000/(float)dwData);
		if( wFanRpm<500) wFanRpm=0;
		printf("FAN-%d Speed is %d rpm\n", uwSpeedRpm, wFanRpm);
		
		break;
	case SEL_FANSPEED:
		//disable smart
		ioctl(i2cfd, I2C_SLAVE, ubI2cAddr>>1); 
		bAddr = NCT7904_REG_BANK ; bReg=strFanCtl[iDevice].ubBank;
		iRet = i2c_smbus_write_byte_data(i2cfd, bAddr, bReg);
		if ( iRet < 0 ) { 
			printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		bAddr = strFanCtl[iDevice].ubIndex; bReg=strFanCtl[iDevice].ubManaul;
		iRet = i2c_smbus_write_byte_data(i2cfd, bAddr, bReg);
		if ( iRet < 0) { 
			printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		//read duty
		bAddr = strFanCtl[iDevice].ubPwmIndex;
		iRet = i2c_smbus_read_byte_data(i2cfd, bAddr);
		if ( iRet < 0) { 
			printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
			return -1;
		}
		ubDuty = (uint8_t)iRet;

		while(1) {
			bAddr = NCT7904_REG_BANK ; bReg=strFanIn[iDevice].bCPU_Bank;
			iRet = i2c_smbus_write_byte_data(i2cfd, bAddr, bReg);
			if ( iRet < 0 ) { 
				printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
				return -1;
			}
			
			bAddr = strFanIn[iDevice].bCPU_High; 
			iRet = i2c_smbus_read_byte_data(i2cfd, bAddr);
			if ( iRet < 0 ) { 
				printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
				return -1;
			}
			dwData = iRet <<5 ;
	
			bAddr = strFanIn[iDevice].bCPU_Low; 
			iRet = i2c_smbus_read_byte_data(i2cfd, bAddr);
			if ( iRet < 0 ) { 
				printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
				return -1;
			}
			dwData += iRet & 0x1F ;
			wFanRpm=(uint16_t)((float)1350000/(float)dwData);
			
			printf("NCT7904 %s FAN-1 speed = %d, Duty=%d\n", dev_name[iDevice],wFanRpm , ubDuty);
//printf("ubDuty=%d\n", ubDuty);
//printf("uwSpeedRpm=%d\n", uwSpeedRpm);
			if ( wFanRpm  > (uwSpeedRpm * 103 / 100) ) {
//printf("wFanRpm  > (uwSpeedRpm *1.1)\n");
				
				if ( ubDuty  >= 1 ) ubDuty -= 1;
				else 	break;
				bAddr = NCT7904_REG_BANK ; bReg=strFanCtl[iDevice].ubBank;
				iRet = i2c_smbus_write_byte_data(i2cfd, bAddr, bReg);
				if ( iRet < 0 ) { 
					printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
					return -1;
				}
				bAddr = strFanCtl[iDevice].ubPwmIndex; bReg=ubDuty;
				iRet = i2c_smbus_write_byte_data(i2cfd, bAddr, bReg);
				if ( iRet < 0) { 
					printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
					return -1;
				}
			}
			else if ( wFanRpm  < (uwSpeedRpm * 98 / 100) ) {
//printf("wFanRpm  < (uwSpeedRpm * 0.9)\n");
				if ( ubDuty  <= 254 ) ubDuty += 1;
				else 	break;
				bAddr = NCT7904_REG_BANK ; bReg=strFanCtl[iDevice].ubBank;
				iRet = i2c_smbus_write_byte_data(i2cfd, bAddr, bReg);
				if ( iRet < 0 ) { 
					printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
					return -1;
				}
				bAddr = strFanCtl[iDevice].ubPwmIndex; bReg=ubDuty;
				iRet = i2c_smbus_write_byte_data(i2cfd, bAddr, bReg);
				if ( iRet < 0) { 
					printf("\e[1;31m<Error> Write Sensor %s Device !!!\e[m\n", dev_name[iDevice]);
					return -1;
				}
			}
			else {
				break;
			}
			usleep(500000);
		}

		break;	
	default:
		__error_exit(argv[0]);
		break;
	}
}

	return iRet;
}


/********************************************/
int __check_hardware(int iDevice)
{
uint8_t xch;
uint8_t xData[4];
uint16_t wVendor, wChipID;
uint8_t bAddr, bReg, bData;
int ret;
	if ( SIO_INDEX == 0 && iDevice != DEV_NCT7904 ) {
		SIO_INDEX = 0x4e;
		SIO_DATA = SIO_INDEX +1;
		__sio_unlock();
		outb(0x02, SIO_INDEX);
		xch = inb(SIO_DATA);
		__sio_lock();
		if ( xch == 0xFF ) {
			SIO_INDEX = 0x2e;
			SIO_DATA = SIO_INDEX +1;
			__sio_unlock();
			outb(0x02, SIO_INDEX);
			xch = inb(SIO_DATA);
			__sio_lock();
			if ( xch == 0xFF ) return 0; //not found
		}
			
	}
	switch (iDevice) {
	case DEV_NCT6776:
		__sio_unlock();
		__sio_logic_device(0x0B); //NCT-6776 HWM 
		outb(0x30, SIO_INDEX);
		xch = inb(SIO_DATA);
		if ( !(xch & 0x01) ) return 0;
		outb(0x60, SIO_INDEX);
		xData[1] = inb(SIO_DATA);
		outb(0x61, SIO_INDEX);
		xData[0] = inb(SIO_DATA);
		__sio_lock();
	//printf("HWM Address=0x%04X\n", *(uint16_t*)xData);
		wHWM_INDEX=(*(uint16_t*)xData) + 0x05;
		wHWM_DATA=(*(uint16_t*)xData) + 0x06;
	//printf("HWM_INDEX=0x%04X\n", wHWM_INDEX);
		ioperm(wHWM_INDEX, 2, 1);
		outb(0x4E, wHWM_INDEX);
		outb(0x80, wHWM_DATA);	//bank 0, high
		outb(0x4F, wHWM_INDEX);
		xData[1] = inb(wHWM_DATA);
		outb(0x4E, wHWM_INDEX);
		outb(0x00, wHWM_DATA);	//bank 0, Low
		outb(0x4F, wHWM_INDEX);
		xData[0] = inb(wHWM_DATA);
		wVendor = *(uint16_t*)xData;
		outb(0x58, wHWM_INDEX);
		xData[0] = inb(wHWM_DATA);
		xData[1] = 0x00;
		wChipID = *(uint16_t*)xData;
	//printf("Vendor=0x%04X\n",wVendor);
	//printf("Chip ID=0x%04X\n",wChipID);
		ioperm(wHWM_INDEX, 2, 0);
		break;
	case DEV_FT81866:
	case DEV_FT81865:
		__sio_unlock();
		ioperm(wHWM_INDEX, 2, 1);
		outb(0x23, SIO_INDEX);
		xData[1] = inb(SIO_DATA);
		outb(0x24, SIO_INDEX);
		xData[0] = inb(SIO_DATA);
		wVendor = *(uint16_t*)xData;
		outb(0x20, SIO_INDEX);
		xData[1] = inb(SIO_DATA);
		outb(0x21, SIO_INDEX);
		xData[0] = inb(SIO_DATA);
		wChipID = *(uint16_t*)xData;	
	//printf("Vendor=0x%04X\n",wVendor);
	//printf("Chip ID=0x%04X\n",wChipID);
		__sio_lock();
		break;
	case DEV_NCT6116:
		__sio_unlock();
		__sio_logic_device(0x0B); //NCT-6776 HWM 
		outb(0x30, SIO_INDEX);
		xch = inb(SIO_DATA);
		if ( !(xch & 0x01) ) return 0;
		outb(0x60, SIO_INDEX);
		xData[1] = inb(SIO_DATA);
		outb(0x61, SIO_INDEX);
		xData[0] = inb(SIO_DATA);
		__sio_lock();
	//printf("HWM Address=0x%04X\n", *(uint16_t*)xData);
		wHWM_INDEX=(*(uint16_t*)xData) + 0x05;
		wHWM_DATA=(*(uint16_t*)xData) + 0x06;
	//printf("HWM_INDEX=0x%04X\n", wHWM_INDEX);
		ioperm(wHWM_INDEX, 2, 1);
		outb(0x4E, wHWM_INDEX);
		outb(0x80, wHWM_DATA);	//bank 0, high
		outb(0xFE, wHWM_INDEX);
		xData[1] = inb(wHWM_DATA);
		outb(0x4E, wHWM_INDEX);
		outb(0x00, wHWM_DATA);	//bank 0, Low
		outb(0xFE, wHWM_INDEX);
		xData[0] = inb(wHWM_DATA);
		wVendor = *(uint16_t*)xData;
		outb(0xFF, wHWM_INDEX);
		xData[0] = inb(wHWM_DATA);
		xData[1] = 0x00;
		wChipID = *(uint16_t*)xData;
	//printf("Vendor=0x%04X\n",wVendor);
	//printf("Chip ID=0x%04X\n",wChipID);
		ioperm(wHWM_INDEX, 2, 0);
		break;
	case DEV_NCT7904:
		bAddr = (0x2D << 1); //1st check 0x2D
		if ( ioctl(i2cfd, I2C_SLAVE, bAddr>>1) < 0)	return 0;
		bReg = 0xFF;
		ret = i2c_smbus_read_byte_data(i2cfd, bReg);
		if ( ret < 0 ) {
			bAddr = (0x2E << 1); //1st check 0x2E
			if(ioctl(i2cfd, I2C_SLAVE, bAddr>>1) < 0) return 0 ;
			bReg = 0x7B; 
			ret = i2c_smbus_read_byte_data(i2cfd, bReg);
			if ( ret < 0 || ret != 0xC5 ) 	return 0;
		}
		ubI2cAddr = bAddr ;
		return 1;
		break;
	default:
		return 0;
		break;
	}
	if ( wVendor != DEV_VENDOR[iDevice] || wChipID != DEV_CHIPID[iDevice] ) return 0; //not match
	return 1;


}
