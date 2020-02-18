/* standard include file */
//#include <stdio.h>
//#include <stdlib.h>
//#include <unistd.h>
//#include <sys/io.h>
//#include <time.h>
//#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <linux/ioctl.h>
#include <math.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <dirent.h>
//#include <syslog.h>
//#include "../include/lmbdrv_ioctl.h"
//#include "../include/config.h"

#define delay(x) usleep(x)
/**** Power Supply I2C Addr ***/
#define NCT7904_REG_BANK	0xFF
#define NCT7904_REG_FOV		0x10
int i2cfd;
unsigned char buf[1024];
//void _show_psu_info(uint8_t bAddr);


int adapter_number=-1;


int detect_i2c_busses(void)
{
    char sysfs[1024], n[1024], s[120], s_best[120];
    DIR *dir;
    struct dirent *de, *de_best=NULL;
    FILE *f;
    int bus_count=0, guess_bus=0;

    sprintf(sysfs,"/sys/class/i2c-adapter");
    if (!(dir = opendir(sysfs))) return guess_bus;
    while ((de = readdir(dir)) !=NULL) {
        if (!strcmp(de->d_name, ".")) continue;
        if (!strcmp(de->d_name, ".."))continue;
        sprintf(n, "%s/%s/name", sysfs, de->d_name);
        f = fopen(n, "r");
        if (f == NULL) continue;
        fgets(s, 120, f);
        fclose(f);
        if ( strstr(s, "801")){
            de_best = de;
            strcpy(s_best, s);
            sscanf(de_best->d_name, "i2c-%d", &guess_bus);
        }
        bus_count++;
    }
    return guess_bus;
}

int open_i2c_adapter(void)
{
	int fd;
	char str[20];

	/* try to probe how many i2c bus on system
         * Show warning message if multi adapter presented and no "-a" argument input
         */
	if (adapter_number < 0)
	    adapter_number = detect_i2c_busses();

	/* 1. check smbus adapter available or not */
	sprintf(str,"/dev/i2c-%d", adapter_number);
	fd = open(str, O_RDWR);
	if (fd < 0 && (errno == ENOENT || errno == ENOTDIR)) {
		sprintf(str,"/dev/i2c/%d", adapter_number);
		fd = open(str, O_RDWR);
	}
	if(fd < 0){
		fprintf( stderr, "\033[1;31mFailed to open '/dev/i2c-%d' or "
			"'/dev/i2c/%d'\033[0m\n", adapter_number, adapter_number);
		fprintf( stderr, "Please make sure i2c adapter, i2c-dev driver is loaded\n" );
		fprintf( stderr, "and proper device node is created(/dev/i2c-0 c 89 0, /dev/i2c-1 c 89 1 ...)\n" );
		return -1;
	}

	return fd;
}

inline __s32 i2c_smbus_access(int file, char read_write, __u8 command,
                                     int size, union i2c_smbus_data *data)
{
	int i, ret;
	struct i2c_smbus_ioctl_data args;

	args.read_write = read_write;
	args.command = command;
	args.size = size;
	args.data = data;

        //To implement failure_limitation counter in order to retry when smbus is busy
	for(i=0 ; i<5; i++){
	    if( (ret=ioctl(file,I2C_SMBUS,&args)) ){
	        if(i != 4 && ret==-1){
                    // In order to reduce the second collision of each thread 
                    // which uses SMBbus, we use a random delay time, and the
                    // random seed is base on PID. In short, the delay time is
                    // between 1 and 100ms.
	            srand(getpid());
	            ret = rand()%30000;
                    usleep(ret + 1000);
                }
		else {
                //    syslog(LOG_INFO, "[ERR] i2c_smbus_access return %d: %s",
                //           errno, strerror(errno));
		}
	    }else break;
	}
	return ret;
}

static inline __s32 i2c_smbus_write_quick(int file, __u8 value)
{
	return i2c_smbus_access(file,value,0,I2C_SMBUS_QUICK,NULL);
}
static inline __s32 i2c_smbus_write_word_data(int file, __u8 command,
                                              __u16 value)
{
	union i2c_smbus_data data;
	data.word = value;
	return i2c_smbus_access(file,I2C_SMBUS_WRITE,command,
	                        I2C_SMBUS_WORD_DATA, &data);
}

static inline __s32 i2c_smbus_write_byte_data(int file, __u8 command,
                                              __u8 value)
{
	union i2c_smbus_data data;
	data.byte = value;
	return i2c_smbus_access(file,I2C_SMBUS_WRITE,command,
	                        I2C_SMBUS_BYTE_DATA, &data);
}

inline __s32 i2c_smbus_write_byte(int file, __u8 value)
{
	return i2c_smbus_access(file,I2C_SMBUS_WRITE,value,
	                        I2C_SMBUS_BYTE,NULL);
}

static inline __s32 i2c_smbus_read_byte_data(int file, __u8 command)
{
    union i2c_smbus_data data;
    if (i2c_smbus_access(file,
                         I2C_SMBUS_READ,
                         command,
                         I2C_SMBUS_BYTE_DATA,
                         &data))
        return -1;
    else
        return 0x0FF & data.byte;
}
inline __s32 i2c_smbus_read_byte(int file)
{
    union i2c_smbus_data data;
    if (i2c_smbus_access(file,
                         I2C_SMBUS_READ,
                         0,
                         I2C_SMBUS_BYTE,
                         &data))
        return -1;
    else
        return 0x0FF & data.byte;
}

static inline __s32 i2c_smbus_read_word_data(int file, __u8 command)
{
    union i2c_smbus_data data;
    if (i2c_smbus_access(file,
                         I2C_SMBUS_READ,
                         command,
                         I2C_SMBUS_WORD_DATA,
                         &data))
        return -1;
    else
        return 0x0FFFF & data.word;
}


static inline __s32 i2c_smbus_read_block_data(int file, __u8 command,
                                              __u8 *values)
{
    union i2c_smbus_data data;
    int i;
    if (i2c_smbus_access(file,
                         I2C_SMBUS_READ,
                         command,
                         I2C_SMBUS_BLOCK_DATA,
                         &data))
        return -1;
    else {
        for (i = 1; i <= data.block[0]; i++)
            values[i-1] = data.block[i];
        return data.block[0];
    }
}



static inline __s32 i2c_smbus_read_block_data_2(int file, __u8 command,int len ,
                                              __u8 *values)
{
    union i2c_smbus_data data;
    int i;
    if (i2c_smbus_access(file,
                         I2C_SMBUS_READ,
                         command,
                         len,
                         &data))
        return -1;
    else {
        for (i = 1; i <= data.block[0]; i++)
            values[i-1] = data.block[i];
        return data.block[0];
    }
}


#if 0


#define SMBUS_BLOCK_MAX	32+1

void _show_psu_info(uint8_t bAddr)
{
int value, ret; 
char arybString[SMBUS_BLOCK_MAX];	
uint16_t wY, wN;
float fTemp_1, fTemp_2;
uint8_t arData[2];
uint16_t uwFanRpm;
float fAmperes, fVolts, fWatts;
uint8_t bReg;
uint16_t *wptr;
int devfd;

//check PSU status is normal or filler type 
uint8_t bEPromAddr;
		
		if(ioctl(i2cfd, I2C_SLAVE, bAddr>>1) < 0){
	                printf("\e[0;31m[ERR] cannot set i2c address to 0x%02x. errno %d: %s\e[m\n",
         	              bAddr, errno, strerror(errno));
			return ;
		}
		//checking PSU is exist or filler
		//get status 
		bReg = PSU_ADDR_STATUS;
		ret = i2c_smbus_read_word_data(i2cfd, bReg);
		if ( ret < 0 ) {
			//PMBus 0xBx Address no device reply and ACK
			//read 0xAx EEPROM for checking PSU filler 
			bEPromAddr = bAddr & 0xEF ; //clear bit4 for 0xB0/B2/B4 to 0xA0/A2/A4
			if(ioctl(i2cfd, I2C_SLAVE, bEPromAddr>>1) < 0){
	              		printf("\e[0;31m[ERR] cannot set i2c address to 0x%02x. errno %d: %s\e[m\n",
         		              bAddr, errno, strerror(errno));
				return ;
			}
			bReg = 0x01; //any address between 0x00 ~ 0xff for check EEPROM exist or not?
			ret = i2c_smbus_read_byte_data(i2cfd, bReg);
			if ( ret < 0 ) {
				//0xAx no reply, PSU slot no plugged
				//printf("slot %d is not exist\n", xxi+1);
			}
			else {
				//that is a PSU-Filler
				printf("found a filler in slot %d\n", (bEPromAddr-0xA0)/2 +1);
				//how to ? when the filler was found
				return;
			}
		}


#if USES_I2C_DEV
	if(ioctl(i2cfd, I2C_SLAVE, bAddr>>1) < 0){
                syslog(LOG_INFO, "[ERR] cannot set i2c address to 0x%02x. errno %d: %s",
                       bAddr, errno, strerror(errno));
                printf("\e[0;31m[ERR] cannot set i2c address to 0x%02x. errno %d: %s\e[m\n",
                       bAddr, errno, strerror(errno));
		return ;
	}
#endif	

	bReg = PSU_ADDR_MFRID;
#if USES_I2C_DEV
	ret = i2c_smbus_read_block_data(i2cfd, bReg, buf);
	if ( ret < 0 ) {
		printf("\e[0;31m[ERR] i2c_smbus_read_block_data failed\e[m\n");
		//syslog(LOG_INFO, "[ERR] i2c_smbus_read_block_data failed");
	}
	else {
		buf[ret]=0; printf("  MFRID: %s\n",buf);
	}
#else
	value = (int)bAddr | (int)bReg<<8 ;
  	ret = ioctl(i2cfd, IOCTL_SMB_READ_BLOCK, &value);
	if ( ret != 0 )  printf("\033[1;31mcommand IOCTL_SMB_READ_BLOCK return error %d\033[0m\n", ret);
	else 	{
		//get block data
		memset(arybString, 0, SMBUS_BLOCK_MAX);
		ret = read(i2cfd, &arybString, value);
		if ( ret > 0 )  printf("MFRID = %s\n", arybString);
		else  		printf("\033[1;31m read return error %d\033[0m\n", ret);
	}
#endif

	bReg = PSU_ADDR_MFRMODEL;
#if USES_I2C_DEV
	ret = i2c_smbus_read_block_data(i2cfd, bReg, buf);
	if ( ret < 0 ) {
		printf("\e[0;31m[ERR] i2c_smbus_read_block_data failed\e[m\n");
		//syslog(LOG_INFO, "[ERR] i2c_smbus_read_block_data failed");
	}
	else {
		buf[ret]=0; printf("  MFRMODEL: %s\n",buf);
	}

	int cmp_ret;
	char dps850[] = "DPS-850AB-4";
	char dps1300_6[] = "DPS-1300AB-6";
	char dps1300_7[] = "DPS-1300AB-7";

	if ( strncmp(dps850 , buf , 11 ) == 0  ){
		psu_model = 0;
	}else if( strncmp(dps1300_6 , buf , 12 ) == 0  ){
		psu_model = 1;
	}else if( strncmp(dps1300_7 , buf , 12 ) == 0  ){
		psu_model = 2;
	}else{
		psu_model = -1;
	}
#else
	value = (int)bAddr | (int)bReg<<8 ;
  	ret = ioctl(i2cfd, IOCTL_SMB_READ_BLOCK, &value);
	if ( ret != 0 ) printf("\033[1;31mcommand IOCTL_SMB_READ_BLOCK return error %d\033[0m\n", ret);
	else 	{
		//get block data
		memset(arybString, 0, SMBUS_BLOCK_MAX);
		read(i2cfd, &arybString, value);
		printf("MFRMODEL = %s\n", arybString);
	}
#endif
	bReg = PSU_ADDR_MFRSERIAL;
#if USES_I2C_DEV
	ret = i2c_smbus_read_block_data(i2cfd, bReg, buf);
	if ( ret < 0 ) {
		printf("\e[0;31m[ERR] i2c_smbus_read_block_data failed\e[m\n");
		//syslog(LOG_INFO, "[ERR] i2c_smbus_read_block_data failed");
	}
	else {
		buf[ret]=0; printf("  MFRSERIAL: %s\n",buf);
	}
#else
	value = (int)bAddr | (int)bReg<<8 ;
  	ret = ioctl(i2cfd, IOCTL_SMB_READ_BLOCK, &value);
	if ( ret != 0 ) printf("\033[1;31mcommand IOCTL_SMB_READ_BLOCK return error %d\033[0m\n", ret);
	else 	{
		//get block data
		memset(arybString, 0, SMBUS_BLOCK_MAX);
		read(i2cfd, &arybString, value);
		printf("MFRSERIAL = %s\n", arybString);
	}
#endif

#if 0
	bReg = PSU_ADDR_MFRREVISION; 
#if USES_I2C_DEV
	ret = i2c_smbus_read_block_data(i2cfd, bReg, buf);
	if ( ret < 0 ) {
		printf("\e[0;31m[ERR] i2c_smbus_read_block_data failed\e[m\n");
		//syslog(LOG_INFO, "[ERR] i2c_smbus_read_block_data failed");
	}
	else {
		printf("  MFR_REVISION = %c%d.%d.%d\n",buf[0],buf[1],buf[5],buf[2]);
	}
#else
	value = (int)bAddr | (int)bReg<<8 ;
  	ret = ioctl(i2cfd, IOCTL_SMB_READ_BLOCK, &value);
	if ( ret != 0 ) printf("\033[1;31mcommand IOCTL_SMB_READ_BLOCK return error %d\033[0m\n", ret);
	else 	{
		//get block data
		memset(arybString, 0, SMBUS_BLOCK_MAX);
		read(i2cfd, &arybString, value);
		printf("MFR_REVISION = %s\n", arybString);
	}
#endif
#endif



#if USES_I2C_DEV


switch(psu_model){
	case 0: // DPS-850AB-4
		bReg = 0xD9;
		ret = i2c_smbus_read_block_data(i2cfd, bReg, buf);
		if ( ret < 0 ) 	printf("\e[0;31m[ERR] i2c_smbus_read_block_data failed\e[m\n");
		else 		printf("  MFR_FW_REVISION = %d.%d.%d\n",buf[2],buf[1],buf[0]);
		break;
	case 1: // DPS-1300AB-6
		bReg = 0xD5;
		ret = i2c_smbus_read_block_data_2(i2cfd, bReg, 8 ,buf);
		if ( ret < 0 ) 	printf("\e[0;31m[ERR] i2c_smbus_read_block_data failed\e[m\n");
		else 		printf("  MFR_FW_REVISION = %c%c%c%c%c%c%c%c\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7]);
		break;

	case 2: // DPS-1300AB-7
		bReg = 0xE3;
		ret = i2c_smbus_read_block_data_2(i2cfd, bReg, 8 ,buf);
		if ( ret < 0 )	printf("\e[0;31m[ERR] i2c_smbus_read_block_data failed\e[m\n");
		else {		printf("  MFR_FW_REVISION PRIMARY = %c%c%c%c\n",buf[1],buf[2],buf[3],buf[4]);
		}

		bReg = 0xE4;
		ret = i2c_smbus_read_block_data_2(i2cfd, bReg, 8 ,buf);
		if ( ret < 0 ) {
			printf("\e[0;31m[ERR] i2c_smbus_read_block_data failed\e[m\n");
			//syslog(LOG_INFO, "[ERR] i2c_smbus_read_block_data failed");
		}
		else {
			//printf("  MFR_FW_REVISION SECONDARY = %0X%0X%0X%0X%0X%0X%0X%0X\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7]);
			printf("  MFR_FW_REVISION SECONDARY = %c%c%c%c\n",buf[1],buf[2],buf[3],buf[4]);
		}

		break;

	default:
		//other Delta Power supply
		bReg = 0xD9;
		ret = i2c_smbus_read_block_data(i2cfd, bReg, buf);
		if ( ret < 0 ) {
			printf("\e[0;31m[ERR] i2c_smbus_read_block_data failed\e[m\n");
			//syslog(LOG_INFO, "[ERR] i2c_smbus_read_block_data failed");
		}
		else {
			//printf("  MFR_FW_REVISION PRIMARY = %0X%0X%0X%0X%0X%0X%0X%0X\n",buf[0],buf[1],buf[2],buf[3],buf[4],buf[5],buf[6],buf[7]);
			printf("  MFR_FW_REVISION = %d.%d.%d\n",buf[2],buf[1],buf[0]);
		}

		break;
		//printf("\e[0;31m[ERR] unknow psu module \e[m\n");
		//close(i2cfd);
		//exit(1);
}

#else
	bReg = PSU_ADDR_FWREVISION;
	value = (int)bAddr | (int)bReg<<8 ;
  	ret = ioctl(i2cfd, IOCTL_SMB_READ_BLOCK, &value);
	if ( ret != 0 ) printf("\033[1;31mcommand IOCTL_SMB_READ_BLOCK return error %d\033[0m\n", ret);
	else 	{
		//get block data
		memset(arybString, 0, SMBUS_BLOCK_MAX);
		read(i2cfd, &arybString, value);
		printf("MFR_FW_REVISION = %s\n", arybString);
	}
#endif

	/***********  PSU Sensor *************/
	bReg = PSU_ADDR_TEMP_1;
#if USES_I2C_DEV
	ret = i2c_smbus_read_word_data(i2cfd, bReg);
	if ( ret < 0 ) {
		printf("\e[0;31m[ERR] i2c_smbus_read_word_data failed\e[m\n");
		//syslog(LOG_INFO, "[ERR] i2c_smbus_read_word_data failed");
	}
	else {
		value = (uint16_t)(ret & 0xFFFF);
		arData[0] = (uint8_t)(value & 0xff);
		arData[1] = (uint8_t)((value >> 8 ) & 0xff);
		wY = (uint16_t)((arData[1] & 0x07 ) << 8 ) |  arData[0];
		if ( wY & 0x0400 ) wY = ((wY ^ 0xFFFF) + 1 ) & 0x7FF ;	//11 bits
		wN = ((uint16_t)(arData[1] & 0xF8 )) >> 3;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			fTemp_1 =(float)( (float)wY / (float)(pow((double)2, (double)wN)));
		}
		else {
			fTemp_1 =(float)( (float)wY * (float)(pow((double)2, (double)wN)));
		}		
		printf("Temp-1 = %.2f °C\n",  fTemp_1);
	}
#else
	value = (int)bAddr | (int)bReg<<8 ;
  	ret = ioctl(i2cfd, IOCTL_SMB_READ_WORD, &value);
	if ( ret != 0 ) printf("\033[1;31mcommand IOCTL_SMB_READ_WORD return error %d\033[0m\n", ret);
	else 	{
		arData[1] = (uint8_t)(value & 0xff);
		arData[0] = (uint8_t)((value >> 8 ) & 0xff);
		wY = (uint16_t)((arData[1] & 0x07 ) << 8 ) |  arData[0];
		if ( wY & 0x0400 ) wY = ((wY ^ 0xFFFF) + 1 ) & 0x7FF ;	//11 bits
		wN = ((uint16_t)(arData[1] & 0xF8 )) >> 3;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			fTemp_1 =(float)( (float)wY / (float)(pow((double)2, (double)wN)));
		}
		else {
			fTemp_1 =(float)( (float)wY * (float)(pow((double)2, (double)wN)));
		}		
		printf("Temp-1 = %.2f °C\n",  fTemp_1);
	}
#endif


	bReg = PSU_ADDR_TEMP_2;
#if USES_I2C_DEV
	ret = i2c_smbus_read_word_data(i2cfd, bReg);
	if ( ret < 0 ) {
		printf("\e[0;31m[ERR] i2c_smbus_read_word_data failed\e[m\n");
		//syslog(LOG_INFO, "[ERR] i2c_smbus_read_word_data failed");
	}
	else {
		value = (uint16_t)(ret & 0xFFFF);
		arData[0] = (uint8_t)(value & 0xff);
		arData[1] = (uint8_t)((value >> 8 ) & 0xff);
		wY = (uint16_t)((arData[1] & 0x07 ) << 8 ) |  arData[0];
		if ( wY & 0x0400 ) wY = ((wY ^ 0xFFFF) + 1 ) & 0x7FF ;	//11 bits
		wN = ((uint16_t)(arData[1] & 0xF8 )) >> 3;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			fTemp_1 =(float)( (float)wY / (float)(pow((double)2, (double)wN)));
		}
		else {
			fTemp_1 =(float)( (float)wY * (float)(pow((double)2, (double)wN)));
		}		
		printf("Temp-2 = %.2f °C\n",  fTemp_1);
	}
#else
	value = (int)bAddr | (int)bReg<<8 ;
  	ret = ioctl(i2cfd, IOCTL_SMB_READ_WORD, &value);
	if ( ret != 0 ) printf("\033[1;31mcommand IOCTL_SMB_READ_WORD return error %d\033[0m\n", ret);
	else 	{
		arData[1] = (uint8_t)(value & 0xff);
		arData[0] = (uint8_t)((value >> 8 ) & 0xff);
		wY = (uint16_t)((arData[1] & 0x07 ) << 8 ) |  arData[0];
		if ( wY & 0x0400 ) wY = ((wY ^ 0xFFFF) + 1 ) & 0x7FF ;	//11 bits
		wN = ((uint16_t)(arData[1] & 0xF8 )) >> 3;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			fTemp_2 =(float)( (float)wY / (float)(pow((double)2, (double)wN)));
		}
		else {
			fTemp_2 =(float)( (float)wY * (float)(pow((double)2, (double)wN)));
		}		
		printf("Temp-2 = %.2f °C\n",  fTemp_2);
	}
#endif

	bReg = PSU_ADDR_FANSPEED_1;
#if USES_I2C_DEV
	ret = i2c_smbus_read_word_data(i2cfd, bReg);
	if ( ret < 0 ) {
		printf("\e[0;31m[ERR] i2c_smbus_read_word_data failed\e[m\n");
		//syslog(LOG_INFO, "[ERR] i2c_smbus_read_word_data failed");
	}
	else {
		value = (uint16_t)(ret & 0xFFFF);
		arData[0] = (uint8_t)(value & 0xff);
		arData[1] = (uint8_t)((value >> 8 ) & 0xff);
		wY = ((uint16_t)(arData[1] & 0x07 )) << 8 |  arData[0];
		if ( wY & 0x0400 ) wY = ((wY ^ 0xFFFF) + 1 ) & 0x7FF ;	//11 bits
		wN = ((uint16_t)(arData[1] & 0xF8 )) >> 3;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			uwFanRpm =(uint16_t)( wY / (uint16_t)(pow((double)2, (double)wN)));
		}
		else {
			uwFanRpm =(uint16_t)( wY * (uint16_t)(pow((double)2, (double)wN)));
		}	
		printf("Fan speed = %d rpm\n",  uwFanRpm);	
	}
#else
	value = (int)bAddr | (int)bReg<<8 ;
  	ret = ioctl(i2cfd, IOCTL_SMB_READ_WORD, &value);
	if ( ret != 0 ) printf("\033[1;31mcommand IOCTL_SMB_READ_WORD return error %d\033[0m\n", ret);
	else 	{
		arData[1] = (uint8_t)(value & 0xff);
		arData[0] = (uint8_t)((value >> 8 ) & 0xff);
		wY = ((uint16_t)(arData[1] & 0x07 )) << 8 |  arData[0];
		if ( wY & 0x0400 ) wY = ((wY ^ 0xFFFF) + 1 ) & 0x7FF ;	//11 bits
		wN = ((uint16_t)(arData[1] & 0xF8 )) >> 3;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			uwFanRpm =(uint16_t)( wY / (uint16_t)(pow((double)2, (double)wN)));
		}
		else {
			uwFanRpm =(uint16_t)( wY * (uint16_t)(pow((double)2, (double)wN)));
		}	
		printf("Fan speed = %d rpm\n",  uwFanRpm);	
	}
	
#endif

	/***********  PSU Watts Informat *************/
	bReg = PSU_ADDR_READVIN;
#if USES_I2C_DEV
	ret = i2c_smbus_read_word_data(i2cfd, bReg);
	if ( ret < 0 ) {
		printf("\e[0;31m[ERR] i2c_smbus_read_word_data failed\e[m\n");
		//syslog(LOG_INFO, "[ERR] i2c_smbus_read_word_data failed");
	}
	else {
		value = (uint16_t)(ret & 0xFFFF);
		arData[0] = (uint8_t)(value & 0xff);
		arData[1] = (uint8_t)((value >> 8 ) & 0xff);
		wY = ((uint16_t)(arData[1] & 0x07 )) << 8 |  arData[0];
		if ( wY & 0x0400 ) wY = ((wY ^ 0xFFFF) + 1 ) & 0x7FF ;	//11 bits
		wN = (uint16_t)(arData[1] & 0xF8 ) >> 3;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			fVolts =(float)( wY / (float)(pow((double)2, (double)wN)));
		}
		else {
			fVolts =(float)( wY * (float)(pow((double)2, (double)wN)));
		}		
		printf("Input voltage = %.3f V\n",  fVolts);	
	}
#else
	value = (int)bAddr | (int)bReg<<8 ;
  	ret = ioctl(i2cfd, IOCTL_SMB_READ_WORD, &value);
	if ( ret != 0 ) printf("\033[1;31mcommand IOCTL_SMB_READ_WORD return error %d\033[0m\n", ret);
	else 	{
		arData[1] = (uint8_t)(value & 0xff);
		arData[0] = (uint8_t)((value >> 8 ) & 0xff);
		wY = ((uint16_t)(arData[1] & 0x07 )) << 8 |  arData[0];
		if ( wY & 0x0400 ) wY = ((wY ^ 0xFFFF) + 1 ) & 0x7FF ;	//11 bits
		wN = (uint16_t)(arData[1] & 0xF8 ) >> 3;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			fVolts =(float)( wY / (float)(pow((double)2, (double)wN)));
		}
		else {
			fVolts =(float)( wY * (float)(pow((double)2, (double)wN)));
		}		
		printf("Input voltage = %.3f V\n",  fVolts);	
	}
#endif
	
	//get current in
	bReg = PSU_ADDR_READIIN;
#if USES_I2C_DEV
	ret = i2c_smbus_read_word_data(i2cfd, bReg);
	if ( ret < 0 ) {
		printf("\e[0;31m[ERR] i2c_smbus_read_word_data failed\e[m\n");
		//syslog(LOG_INFO, "[ERR] i2c_smbus_read_word_data failed");
	}
	else {
		value = (uint16_t)(ret & 0xFFFF);
		arData[0] = (uint8_t)(value & 0xff);
		arData[1] = (uint8_t)((value >> 8 ) & 0xff);
		wY = (uint16_t)((arData[1] & 0x07 ) << 8) |  arData[0];
		if ( wY & 0x0400 ) wY = ((wY ^ 0xFFFF) + 1 ) & 0x7FF ;	//11 bits
		wN = ((uint16_t)(arData[1] & 0xF8 )) >> 3;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			fAmperes =(float)( wY / (float)(pow((double)2, (double)wN)));
		}
		else {
			fAmperes =(float)( wY * (float)(pow((double)2, (double)wN)));
		}		
		printf("Input current = %.3f A\n",  fAmperes);		
	}
#else
	value = (int)bAddr | (int)bReg<<8 ;
  	ret = ioctl(i2cfd, IOCTL_SMB_READ_WORD, &value);
	if ( ret != 0 ) printf("\033[1;31mcommand IOCTL_SMB_READ_WORD return error %d\033[0m\n", ret);
	else 	{
		arData[1] = (uint8_t)(value & 0xff);
		arData[0] = (uint8_t)((value >> 8 ) & 0xff);
		wY = (uint16_t)((arData[1] & 0x07 ) << 8) |  arData[0];
		if ( wY & 0x0400 ) wY = ((wY ^ 0xFFFF) + 1 ) & 0x7FF ;	//11 bits
		wN = ((uint16_t)(arData[1] & 0xF8 )) >> 3;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			fAmperes =(float)( wY / (float)(pow((double)2, (double)wN)));
		}
		else {
			fAmperes =(float)( wY * (float)(pow((double)2, (double)wN)));
		}		
		printf("Input current = %.3f A\n",  fAmperes);	
	}
#endif
	fWatts = fVolts * fAmperes ;
	printf("Input Watts = %.3f W\n",  fWatts);	


	/***********  PSU Watts Informat *************/
	bReg = PSU_ADDR_READVOUT;
#if USES_I2C_DEV
	ret = i2c_smbus_read_word_data(i2cfd, bReg);
	if ( ret < 0 ) {
		printf("\e[0;31m[ERR] i2c_smbus_read_word_data failed\e[m\n");
		//syslog(LOG_INFO, "[ERR] i2c_smbus_read_word_data failed");
	}
	else {
		value = (uint16_t)(ret & 0xFFFF);
		arData[0] = (uint8_t)(value & 0xff);
		arData[1] = (uint8_t)((value >> 8 ) & 0xff);
		wY = (uint16_t)(arData[1] << 8) | (uint16_t)arData[0];
		//get VOUT_MODE
		bReg = PSU_ADDR_VOUTMODE;
		value = (int)bAddr | (int)bReg<<8 ;
  		ret = i2c_smbus_read_byte_data(i2cfd, bReg);
		if ( ret < 0 ) printf("\033[1;31mi2c_smbus_read_byte_data return error %d\033[0m\n", ret);
		value =(uint16_t) (ret & 0xFFFF);
		wN = (uint16_t)(value & 0x1F ) ;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			fVolts =(float)( wY / (float)(pow((double)2, (double)wN)));
		}
		else {
			fVolts =(float)( wY * (float)(pow((double)2, (double)wN)));
		}					
		printf("Output voltage = %.3f V\n",  fVolts);	
	}
#else
	value = (int)bAddr | (int)bReg<<8 ;
  	ret = ioctl(i2cfd, IOCTL_SMB_READ_WORD, &value);
	if ( ret != 0 ) printf("\033[1;31mcommand IOCTL_SMB_READ_WORD return error %d\033[0m\n", ret);
	else 	{
		arData[1] = (uint8_t)(value & 0xff);
		arData[0] = (uint8_t)((value >> 8 ) & 0xff);
		wY = (uint16_t)(arData[1] << 8) | (uint16_t)arData[0];
		//get VOUT_MODE
		bReg = PSU_ADDR_VOUTMODE;
		value = (int)bAddr | (int)bReg<<8 ;
  		ret = ioctl(devfd, IOCTL_SMB_READ_BYTE, &value);
		if ( ret != 0 ) printf("\033[1;31mcommand IOCTL_SMB_READ_WORD return error %d\033[0m\n", ret);
		wN = (uint16_t)(value & 0x1F ) ;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			fVolts =(float)( wY / (float)(pow((double)2, (double)wN)));
		}
		else {
			fVolts =(float)( wY * (float)(pow((double)2, (double)wN)));
		}					
		printf("Output voltage = %.3f V\n",  fVolts);	
	}
#endif
	
	//get current in
	bReg = PSU_ADDR_READIOUT;
#if USES_I2C_DEV
	ret = i2c_smbus_read_word_data(i2cfd, bReg);
	if ( ret < 0 ) {
		printf("\e[0;31m[ERR] i2c_smbus_read_word_data failed\e[m\n");
		//syslog(LOG_INFO, "[ERR] i2c_smbus_read_word_data failed");
	}
	else {
		value = (uint16_t)(ret & 0xFFFF);
		arData[0] = (uint8_t)(value & 0xff);
		arData[1] = (uint8_t)((value >> 8 ) & 0xff);
		wY = (uint16_t)((arData[1] & 0x07 ) << 8) |  arData[0];
		if ( wY & 0x0400 ) wY = ((wY ^ 0xFFFF) + 1 ) & 0x7FF ;	//11 bits
		wN = ((uint16_t)(arData[1] & 0xF8 )) >> 3;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			fAmperes =(float)( wY / (float)(pow((double)2, (double)wN)));
		}
		else {
			fAmperes =(float)( wY * (float)(pow((double)2, (double)wN)));
		}		
		printf("Output current = %.3f A\n",  fAmperes);	
	}
#else
	value = (int)bAddr | (int)bReg<<8 ;
  	ret = ioctl(i2cfd, IOCTL_SMB_READ_WORD, &value);
	if ( ret != 0 ) printf("\033[1;31mcommand IOCTL_SMB_READ_WORD return error %d\033[0m\n", ret);
	else 	{
		arData[1] = (uint8_t)(value & 0xff);
		arData[0] = (uint8_t)((value >> 8 ) & 0xff);
		wY = (uint16_t)((arData[1] & 0x07 ) << 8) |  arData[0];
		if ( wY & 0x0400 ) wY = ((wY ^ 0xFFFF) + 1 ) & 0x7FF ;	//11 bits
		wN = ((uint16_t)(arData[1] & 0xF8 )) >> 3;
		if ( wN & 0x0010 ) {
			wN = ((wN ^ 0xFFFF) + 1 ) & 0x1F ;	//5 bits
			fAmperes =(float)( wY / (float)(pow((double)2, (double)wN)));
		}
		else {
			fAmperes =(float)( wY * (float)(pow((double)2, (double)wN)));
		}		
		printf("Output current = %.3f A\n",  fAmperes);	
	}
#endif
	fWatts = fVolts * fAmperes ;
	printf("Output Watts = %.3f W\n",  fWatts);	

}
#endif
