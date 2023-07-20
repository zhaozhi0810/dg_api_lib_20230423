/*
*********************************************************************************************************
*
*	Ä£¿éÃû³Æ : XYZmodemÐ­Òé
*	ÎÄ¼þÃû³Æ : xyzmodem.c
*	°æ    ±¾ : V1.0
*	Ëµ    Ã÷ : xyzmodemÐ­Òé
*
*	ÐÞ¸Ä¼ÇÂ¼ 
*		°æ±¾ºÅ    ÈÕÆÚ         ×÷Õß        ËµÃ÷
*		V1.0    2022-08-08  Eric2013      Ê×·¢
*
*	Copyright (C), 2022-2030, °²¸»À³µç×Ó www.armbbs.cn
*
*********************************************************************************************************
*/
#include <stdio.h>
#include <stdlib.h>
#include "stdint.h"
//#include <windows.h>
#include "uart_to_mcu.h"
#include <unistd.h>
#include <string.h>



#define SOH                     (0x01)  /* start of 128-byte data packet */
#define STX                     (0x02)  /* start of 1024-byte data packet */
#define EOT                     (0x04)  /* end of transmission */
#define ACK                     (0x06)  /* acknowledge */
#define NAK                     (0x15)  /* negative acknowledge */
#define CA                      (0x18)  /* two of these in succession aborts transfer */
#define CRC16                   (0x43)  /* 'C' == 0x43, request 16-bit CRC */

#define ABORT1                  (0x41)  /* 'A' == 0x41, abort by user */
#define ABORT2                  (0x61)  /* 'a' == 0x61, abort by user */

#define PACKET_SEQNO_INDEX      (1)
#define PACKET_SEQNO_COMP_INDEX (2)

#define PACKET_HEADER           (3)
#define PACKET_TRAILER          (2)
#define PACKET_OVERHEAD         (PACKET_HEADER + PACKET_TRAILER)
#define PACKET_SIZE             (128)
#define PACKET_1K_SIZE          (1024)

#define FILE_NAME_LENGTH        (64)   //ÎÄ¼þÃû³¤¶È×î¶à64×Ö½Ú£¬°üÀ¨½áÊø·û
#define FILE_SIZE_LENGTH        (16)
#define FILE_MD5_LENGTH         (32)   //MD5³¤¶È32×Ö½Ú£¬²»°üÀ¨½áÊø·û£¬2023-04-12Ôö¼Ó

#define NAK_TIMEOUT             (0x100000)
#define MAX_ERRORS              (5)

extern char md5_readBuf[64];   //´æ·ÅÎÄ¼þµÄmd5

extern int g_debug_mode;   //调试模式，用于输出一些打印信息，0表示不输出打印信息
/*
*********************************************************************************************************
*	º¯ Êý Ãû: Int2Str
*	¹¦ÄÜËµÃ÷: ½«ÕûÊý×ª»»³É×Ö·û
*	ÐÎ    ²Î: str ×Ö·û  intnum ÕûÊý
*	·µ »Ø Öµ: ÎÞ
*********************************************************************************************************
*/
// static int Int2Str(uint8_t* str,uint32_t arrlen, int32_t intnum)
// {
// 	return snprintf(str,arrlen-1,"%d",intnum);

// 	// for (i = 0; i < 10; i++)
// 	// {
// 	// 	str[j++] = (intnum / Div) + 48;

// 	// 	intnum = intnum % Div;
// 	// 	Div /= 10;
// 	// 	if ((str[j-1] == '0') & (Status == 0))
// 	// 	{
// 	// 		j = 0;
// 	// 	}
// 	// 	else
// 	// 	{
// 	// 		Status++;
// 	// 	}
// 	// }
// }

/*
*********************************************************************************************************
*	º¯ Êý Ãû: Ymodem_PrepareIntialPacket
*	¹¦ÄÜËµÃ÷: ×¼±¸µÚÒ»°üÒª·¢ËÍµÄÊý¾Ý     
*	ÐÎ    ²Î: data Êý¾Ý
*             fileName ÎÄ¼þÃû
*             length   ÎÄ¼þ´óÐ¡
*	·µ »Ø Öµ: 0
*********************************************************************************************************
*/
void Ymodem_PrepareIntialPacket(uint8_t *data, const uint8_t* fileName, uint32_t *length)
{
	uint16_t i, j;
	uint8_t file_ptr[16];

	data[0] = SOH; /* soh表示该帧128字节*/
	data[1] = 0x00;
	data[2] = 0xff;

	//1.填充文件名，这里FILE_NAME_LENGTH限制为64字节
	for (i = 0; (fileName[i] != '\0') && (i < FILE_NAME_LENGTH); i++)
	{
		data[i + PACKET_HEADER] = fileName[i];
	}
	data[i + PACKET_HEADER] = 0x00;   //填入字符串结尾符

	//2.填充文件大小，先把int转为字符串
	snprintf(file_ptr,sizeof file_ptr-1,"%d ",*length);   //增加一个空格
	for (j =0, i = i + PACKET_HEADER + 1; file_ptr[j] != '\0' ; )
	{
		data[i++] = file_ptr[j++];
	}

	data[i] = 0x00;

	//3.继续填入md5值，这里是32字节的字符串
	for (j =0, i = i + 1; (md5_readBuf[j] != '\0') && (j < FILE_MD5_LENGTH) ;i++,j++ )
	{
		data[i] = md5_readBuf[j];
	}

	//4.其他空间继续填充0
	for (j = i; j < PACKET_SIZE + PACKET_HEADER; j++)
	{
		data[j] = 0;
	}

}

/*
*********************************************************************************************************
*	º¯ Êý Ãû: Ymodem_PreparePacket
*	¹¦ÄÜËµÃ÷: ×¼±¸·¢ËÍÊý¾Ý°ü    
*	ÐÎ    ²Î: SourceBuf Òª·¢ËÍµÄÔ­Êý¾Ý
*             data      ×îÖÕÒª·¢ËÍµÄÊý¾Ý°ü£¬ÒÑ¾­°üº¬µÄÍ·ÎÄ¼þºÍÔ­Êý¾Ý
*             pktNo     Êý¾Ý°üÐòºÅ
*             sizeBlk   Òª·¢ËÍÊý¾ÝÊý
*	·µ »Ø Öµ: ÎÞ
*********************************************************************************************************
*/
static int sendsize = 0;
void Ymodem_PreparePacket(uint8_t *SourceBuf, uint8_t *data, uint8_t pktNo, uint32_t sizeBlk)
{
	uint16_t i, size, packetSize;
	uint8_t* file_ptr;

	/* ÉèÖÃºÃÒª·¢ËÍÊý¾Ý°üµÄÇ°Èý¸ö×Ö·ûdata[0]£¬data[1]£¬data[2] */
	/* ¸ù¾ÝsizeBlkµÄ´óÐ¡ÉèÖÃÊý¾ÝÇøÊý¾Ý¸öÊýÊÇÈ¡1024×Ö½Ú»¹ÊÇÈ¡128×Ö½Ú*/
	packetSize = sizeBlk >= PACKET_1K_SIZE ? PACKET_1K_SIZE : PACKET_SIZE;

	/* Êý¾Ý´óÐ¡½øÒ»²½È·¶¨ */
	size = sizeBlk < packetSize ? sizeBlk :packetSize;
	
	/* Ê××Ö½Ú£ºÈ·¶¨ÊÇ1024×Ö½Ú»¹ÊÇÓÃ128×Ö½Ú */
	if (packetSize == PACKET_1K_SIZE)
	{
		data[0] = STX;
	}
	else
	{
		data[0] = SOH;
	}
	
	/* µÚ2¸ö×Ö½Ú£ºÊý¾ÝÐòºÅ */
	data[1] = pktNo;
	/* µÚ3¸ö×Ö½Ú£ºÊý¾ÝÐòºÅÈ¡·´ */
	data[2] = (~pktNo);
	file_ptr = SourceBuf;

	/* Ìî³äÒª·¢ËÍµÄÔ­Ê¼Êý¾Ý */
	for (i = PACKET_HEADER; i < size + PACKET_HEADER;i++)
	{
		data[i] = *file_ptr++;
	}
	/* ²»×ãµÄ²¹ EOF (0x1A) »ò 0x00 */
	if ( size  <= packetSize)
	{
		for (i = size + PACKET_HEADER; i < packetSize + PACKET_HEADER; i++)
		{
			data[i] = 0x1A; /* EOF (0x1A) or 0x00 */
		}
	}
    sendsize += size;

    if(g_debug_mode)  //调试模式下才打印
    	printf("SendSize = %d\r\n", sendsize);
}

/*
*********************************************************************************************************
*	º¯ Êý Ãû: UpdateCRC16
*	¹¦ÄÜËµÃ÷: ÉÏ´Î¼ÆËãµÄCRC½á¹û crcIn ÔÙ¼ÓÉÏÒ»¸ö×Ö½ÚÊý¾Ý¼ÆËãCRC
*	ÐÎ    ²Î: crcIn ÉÏÒ»´ÎCRC¼ÆËã½á¹û
*             byte  ÐÂÌí¼Ó×Ö½Ú
*	·µ »Ø Öµ: ÎÞ
*********************************************************************************************************
*/
uint16_t UpdateCRC16(uint16_t crcIn, uint8_t byte)
{
  uint32_t crc = crcIn;
  uint32_t in = byte | 0x100;

  do
  {
	crc <<= 1;
	in <<= 1;
	if(in & 0x100)
		++crc;
	if(crc & 0x10000)
		crc ^= 0x1021;
  }while(!(in & 0x10000));

  return crc & 0xffffu;
}

/*
*********************************************************************************************************
*	º¯ Êý Ãû: Cal_CRC16
*	¹¦ÄÜËµÃ÷: ¼ÆËãÒ»´®Êý¾ÝµÄCRC
*	ÐÎ    ²Î: data  Êý¾Ý
*             size  Êý¾Ý³¤¶È
*	·µ »Ø Öµ: CRC¼ÆËã½á¹û
*********************************************************************************************************
*/
uint16_t Cal_CRC16(const uint8_t* data, uint32_t size)
{
	uint32_t crc = 0;
	const uint8_t* dataEnd = data+size;

	while(data < dataEnd)
		crc = UpdateCRC16(crc, *data++);

	crc = UpdateCRC16(crc, 0);
	crc = UpdateCRC16(crc, 0);

	return crc&0xffffu;
}

/*
*********************************************************************************************************
*	º¯ Êý Ãû: CalChecksum
*	¹¦ÄÜËµÃ÷: ¼ÆËãÒ»´®Êý¾Ý×ÜºÍ
*	ÐÎ    ²Î: data  Êý¾Ý
*             size  Êý¾Ý³¤¶È
*	·µ »Ø Öµ: ¼ÆËã½á¹ûµÄºó8Î»
*********************************************************************************************************
*/
uint8_t CalChecksum(const uint8_t* data, uint32_t size)
{
  uint32_t sum = 0;
  const uint8_t* dataEnd = data+size;

  while(data < dataEnd )
    sum += *data++;

  return (sum & 0xffu);
}

/*
*********************************************************************************************************
*	º¯ Êý Ãû: Ymodem_Transmit
*	¹¦ÄÜËµÃ÷: ·¢ËÍÎÄ¼þ
*	ÐÎ    ²Î: buf  ÎÄ¼þÊý¾Ý
*             sendFileName  ÎÄ¼þÃû
*             sizeFile    ÎÄ¼þ´óÐ¡
*	·µ »Ø Öµ: 0  ÎÄ¼þ·¢ËÍ³É¹¦
*********************************************************************************************************
*/
uint8_t Ymodem_Transmit (uint8_t *buf, const uint8_t* sendFileName, uint32_t sizeFile)
{
	uint8_t packet_data[PACKET_1K_SIZE + PACKET_OVERHEAD];
	uint8_t filename[FILE_NAME_LENGTH];
	uint8_t *buf_ptr, tempCheckSum;
	uint16_t tempCRC;
	uint16_t blkNumber;
	uint8_t receivedC[2], CRC16_F = 0, i;
	uint32_t errors, ackReceived, size = 0, pktSize;

	errors = 0;
	ackReceived = 0;
	for (i = 0; i < (FILE_NAME_LENGTH - 1); i++)
	{
		filename[i] = sendFileName[i];
	}

	CRC16_F = 1;


	Ymodem_PrepareIntialPacket(&packet_data[0], filename, &sizeFile);
	do 
	{
		UART_SendPacket(packet_data, PACKET_SIZE + PACKET_HEADER);
		if (CRC16_F)
		{
			tempCRC = Cal_CRC16(&packet_data[3], PACKET_SIZE);
			UART_SendByte(tempCRC >> 8);
			UART_SendByte(tempCRC & 0xFF);
		}
		else
		{
			tempCheckSum = CalChecksum (&packet_data[3], PACKET_SIZE);
			UART_SendByte(tempCheckSum);
		}
  
		/* µÈ´ý Ack ºÍ×Ö·û 'C' */
		if (UART_ReceivePacket(&receivedC[0], 2, 10000) == 0)  
		{
			if ((receivedC[0] == ACK)&&(receivedC[1] == CRC16))
			{ 
				ackReceived = 1;
			}
			else if ((receivedC[0] == CA)&&(receivedC[1] == CA))  //ÖÐÖ¹ÐÅºÅ
			{ 
				errors =  0x0A;
				break;
			}
		}
		else
		{
			errors++;
		}
	}while (!ackReceived && (errors < 0x0A));
  
	if (errors >=  0x0A)
	{
		printf("errors = %d return\n",errors);
		return errors;
	}
	
	buf_ptr = buf;
	size = sizeFile;
	blkNumber = 0x01;

	/* Resend packet if NAK  for a count of 10 else end of communication */
	while (size)
	{
		/* ×¼±¸ÏÂÒ»°üÊý¾Ý */
		Ymodem_PreparePacket(buf_ptr, &packet_data[0], blkNumber, size);
		ackReceived = 0;
		receivedC[0]= 0;
		errors = 0;
		do
		{
			/* ·¢ËÍÏÂÒ»°üÊý¾Ý */
			if (size >= PACKET_1K_SIZE)
			{
				pktSize = PACKET_1K_SIZE;
			}
			else
			{
				pktSize = PACKET_SIZE;
			}
			
			UART_SendPacket(packet_data, pktSize + PACKET_HEADER);
			
			/* ¸ù¾ÝCRC16_F·¢ËÍCRCÐ£Ñé»òÕßÇóºÍµÄ½á¹û */
			if (CRC16_F)
			{
				tempCRC = Cal_CRC16(&packet_data[3], pktSize);
				UART_SendByte(tempCRC >> 8);
				UART_SendByte(tempCRC & 0xFF);
			}
			else
			{
				tempCheckSum = CalChecksum (&packet_data[3], pktSize);
				UART_SendByte(tempCheckSum);
			}

			/* µÈµ½AckÐÅºÅ */
			if ((UART_ReceiveByte(&receivedC[0], 100000) == 0)  && (receivedC[0] == ACK))
			{
				ackReceived = 1; 
				/* ÐÞ¸Äbuf_ptrÎ»ÖÃÒÔ¼°size´óÐ¡£¬×¼±¸·¢ËÍÏÂÒ»°üÊý¾Ý */
				if (size > pktSize)
				{
					buf_ptr += pktSize;  
					size -= pktSize;
					if (blkNumber == ((2*1024*1024)/128))
					{
						return 0xFF; /* ´íÎó */
					}
					else
					{
						blkNumber++;
					}
				}
				else
				{
					buf_ptr += pktSize;
					size = 0;
				}
			}
			else
			{
				errors++;
			}
			
		}while(!ackReceived && (errors < 0x0A));
		
		/* ³¬¹ý10´ÎÃ»ÓÐÊÕµ½Ó¦´ð¾ÍÍË³ö */
		if (errors >=  0x0A)
		{
			return errors;
		} 
	}
	
	ackReceived = 0;
	receivedC[0] = 0x00;
	errors = 0;
	do 
	{
		UART_SendByte(EOT);
		
		/* ·¢ËÍEOTÐÅºÅ */
		/* µÈ´ýAckÓ¦´ð */
		if ((UART_ReceiveByte(&receivedC[0], 10000) == 0) )
		{
            if(receivedC[0] == ACK)
            {
               ackReceived = 1;       
            }	 	
		}
		else
		{
			errors++;
		}
		
	}while (!ackReceived && (errors < 0x0A));
    

	if (errors >=  0x0A)
	{
		return errors;
	}

#if 1
	ackReceived = 0;
	receivedC[0] = 0x00;
	errors = 0;

	packet_data[0] = SOH;
	packet_data[1] = 0;
	packet_data [2] = 0xFF;

	for (i = PACKET_HEADER; i < (PACKET_SIZE + PACKET_HEADER); i++)
	{
		packet_data [i] = 0x00;
	}
  
	do 
	{
		/* ·¢ËÍÊý¾Ý°ü */
		UART_SendPacket(packet_data, PACKET_SIZE + PACKET_HEADER);

		/* ¸ù¾ÝCRC16_F·¢ËÍCRCÐ£Ñé»òÕßÇóºÍµÄ½á¹û */
		tempCRC = Cal_CRC16(&packet_data[3], PACKET_SIZE);
		UART_SendByte(tempCRC >> 8);
		UART_SendByte(tempCRC & 0xFF);

		/* µÈ´ý Ack */
		if (UART_ReceiveByte(&receivedC[0], 10000) == 0)  
		{
			if (receivedC[0] == ACK)
			{ 
				/* Êý¾Ý°ü·¢ËÍ³É¹¦ */
				ackReceived = 1;
			}
		}
		else
		{
			errors++;
		}
	}while (!ackReceived && (errors < 0x0A));

	if (errors >=  0x0A)
	{
		return errors;
	}  
#endif

	return 0; 
}



char md5_readBuf[64] = {0};

#if 0
int get_file_md5sum(const char * filename)
{
	FILE * filep;
	char cmd[128] = {"md5sum "};
	
	int ret;

	strcat(cmd,filename);

	filep = popen(cmd,"r");
	if(!filep)
		return -1;
    ret = fread(md5_readBuf,32,1,filep);
 
//    printf("get_file_md5sum = %s\n",md5_readBuf);

    pclose(filep);

    return ret;
}

#endif

#if 1  

#define ROTATELEFT(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

/**
 * @desc: convert message and mes_bkp string into integer array and store them in w
 */
static void md5_process_part1(uint32_t *w, unsigned char *message, uint32_t *pos, uint32_t mes_len, const unsigned char *mes_bkp)
{
    uint32_t i; // used in for loop

    for(i = 0; i <= 15; i++)
    {
        int32_t count = 0;
        while(*pos < mes_len && count <= 24)
        {
            w[i] += (((uint32_t)message[*pos]) << count);
            (*pos)++;
            count += 8;
        }
        while(count <= 24)
        {
            w[i] += (((uint32_t)mes_bkp[*pos - mes_len]) << count);
            (*pos)++;
            count += 8;
        }
    }
}

/**
 * @desc: start encryption based on w
 */
static void md5_process_part2(uint32_t abcd[4], uint32_t *w, const uint32_t k[64], const uint32_t s[64])
{
    uint32_t i; // used in for loop

    uint32_t a = abcd[0];
    uint32_t b = abcd[1];
    uint32_t c = abcd[2];
    uint32_t d = abcd[3];
    uint32_t f = 0;
    uint32_t g = 0;

    for(i = 0; i < 64; i++)
    {
        if(i <= 15) //i >= 0 && 
        {
            f = (b & c) | ((~b) & d);
            g = i;
        }else if(i >= 16 && i <= 31)
        {
            f = (d & b) | ((~d) & c);
            g = (5 * i + 1) % 16;
        }else if(i >= 32 && i <= 47)
        {
            f = b ^ c ^ d;
            g = (3 * i + 5) % 16;
        }else if(i >= 48 && i <= 63)
        {
            f = c ^ (b | (~d));
            g = (7 * i) % 16;
        }
        uint32_t temp = d;
        d = c;
        c = b;
        b = ROTATELEFT((a + f + k[i] + w[g]), s[i]) + b;
        a = temp;
    }

    abcd[0] += a;
    abcd[1] += b;
    abcd[2] += c;
    abcd[3] += d;
}

static const uint32_t k_table[]={
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,
    0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,0x698098d8,
    0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,
    0xa679438e,0x49b40821,0xf61e2562,0xc040b340,0x265e5a51,
    0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,
    0xfcefa3f8,0x676f02d9,0x8d2a4c8a,0xfffa3942,0x8771f681,
    0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,
    0xbebfbc70,0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,
    0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,0xf4292244,
    0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,
    0xffeff47d,0x85845dd1,0x6fa87e4f,0xfe2ce6e0,0xa3014314,
    0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};

static const uint32_t s_table[]={
    7,12,17,22,7,12,17,22,7,12,17,22,7,
    12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,
    15,21,6,10,15,21,6,10,15,21,6,10,15,21
};

int32_t cal_md5(unsigned char *result, unsigned char *data, int length){
    if (result == NULL)
    {
        return 1;
    }

    uint32_t w[16];

    uint32_t i; // used in for loop

    uint32_t mes_len = length;
    uint32_t looptimes = (mes_len + 8) / 64 + 1;
    uint32_t abcd[] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476};

    uint32_t pos = 0; // position pointer for message
    uint32_t bkp_len = 64 * looptimes - mes_len; // 经过计算发现不超过72

//    unsigned char *bkp_mes = (unsigned char *)calloc(1, bkp_len);
    unsigned char bkp_mes[80];
    for(int i = 0; i < 80; i++) //初始化
    {
        bkp_mes[i] = 0;
    }

    bkp_mes[0] = (unsigned char)(0x80);
    uint64_t mes_bit_len = ((uint64_t)mes_len) * 8;
    for(i = 0; i < 8; i++)
    {
        bkp_mes[bkp_len-i-1] = (unsigned char)((mes_bit_len & (0x00000000000000FF << (8 * (7 - i)))) >> (8 * (7 - i)));
    }

    for(i = 0; i < looptimes; i++)
    {
        for(int j = 0; j < 16; j++) //初始化
        {
            w[j] = 0x00000000;
        }

        md5_process_part1(w, data, &pos, mes_len, bkp_mes); // compute w

        md5_process_part2(abcd, w, k_table, s_table); // calculate md5 and store the result in abcd
    }

    for(int i = 0; i < 16; i++)
    {
        //result[i] = ((unsigned char*)abcd)[i];
		sprintf((char*)result+i*2,"%02x",((unsigned char*)abcd)[i]);   //2023-05-09 返回字符串
    }

    return 0;
}

#endif


//本次计算文件的md5，是烧录文件，需要做偏移
//文件缓存，和文件大小
//结果保存在全局变量中
//返回0是成功，1是失败
int get_file_md5sum2(unsigned char * filebuf,int size1)
{
	int ret;

    ret = cal_md5(md5_readBuf, filebuf, size1);
    //md5(buf, size1, md5_readBuf);
    if(!ret && g_debug_mode)  //调试模式下才打印) 
    	printf("cal_md5 = %s\n",md5_readBuf);

    return ret;
}




uint8_t checksum(uint8_t *buf, uint8_t len)
{
	uint8_t sum;
	uint8_t i;

	for(i=0,sum=0; i<len; i++)
		sum += buf[i];

	return sum;
}

#if 0
void send_update_cmd_tomcu(uint8_t phase);
#else
int  send_update_cmd_tomcu(uint8_t*data,uint8_t phase);
#endif

//·µ»ØÖµÎª0 ±íÊ¾ÒªÉý¼¶£¬ÆäËûÖµ²»Éý¼¶
static int ready_to_update(void)
{
	char data[40] = {0};
	int offset=0;
	uint8_t csum = 0,c,rsum = 0;
	int ret;
	uint8_t i = 0;

	//1.发送请求md5单片机命令，单片机返回32个字节的md5值（实际不止，还有帧头帧尾）
	ret = send_update_cmd_tomcu(data,0);
	if(ret == 0)
	{
		rsum = data[34];
		data[34] = 0;
		if(g_debug_mode)  //调试模式下才打印
			printf("Receive mcu checksum: %s\n",data+2);

		csum = checksum(data, 34);  //帧尾包含校验和，计算这一帧的校验和
		if(csum == rsum)  //校验和正常
		{
			if(memcmp(data+2,md5_readBuf,32)==0) //md5 对比，发现与bin文件相同，则不进行升级
			{
				//if(g_debug_mode)  //调试模式下才打印
				{
					printf("md5sum memcmp ret = 0,is the same\n");
					printf("not need update!!!\n");
				}				
				return 1;
			}
			else  //md5不同，则继续
			{
				if(g_debug_mode)  //调试模式下才打印
					printf("md5sum different , readyto update\n");
				return send_update_cmd_tomcu(NULL,1); //发送准备升级命令，正常返回0，其他返回-1
				//return 0;
			}	
		}
		else //收到单片机的数据，但是校验和不正常，应该数据有问题，结束升级
		{
			printf("checksum error csum = %d,rsum = %d\n",csum,rsum);
			//uart_exit();
			return -1;
		}
	}
	if(g_debug_mode)  //调试模式下才打印
		printf("ready to update!\n");
	return 0;
}


#define ApplicationAddress    0x8006000


//成功返回buf的地址，否则返回NULL
char* file_read_check(const char *filename,int *filesize)
{
	size_t len;
    int ret;// fd;
    FILE *fin;// *fout;
    int size = 0;
    int bw = 0;       
    int readcount = 0;

    char md5_value[64] = {0};
    int file_offset = 0;

    //1.判断文件名的长度，太长了缓存不够，其实意义不大，这里主要考虑就是绝对路径的时候，可能会比较长
    len = strlen(filename);   
    if(len < 5 || len > 63)
    {
    	printf("ERROR: filename length = %ld <5-63>\n",len);
    	return NULL;
    }

    //2.只读方式打开
    fin = fopen(filename, "rb");
    if (fin != NULL)
    {
        /* 文件打开成功*/
        if(g_debug_mode)  //调试模式下才打印
        	printf("open %s success \r\n",filename);
    }
    else
    {
        printf("open %s error\r\n",filename);
        return NULL;
    }

    //3.计算iap的长度，用区分不同单片机的bin文件，所以偏移不一定相同
    file_offset = ApplicationAddress & 0x7f00;  //iap的偏移全部去掉
    fseek(fin, 0, SEEK_END);
    size = ftell(fin);   //得到文件长度

    //4.从bin文件读取md5值
    fseek(fin, file_offset-512, SEEK_SET);   //读出md5,2023-06-12 增加一个偏移
    bw = fread(md5_value, 1, 32, fin);
    if(bw != 32)
    {
    	fclose(fin);
    	printf("ERROR: read bin md5 failed ! ret = %d\n",bw);
    	return NULL;
    }
    if(g_debug_mode)  //调试模式下才打印
    	printf("read bin md5 success! md5_value = %s\n",md5_value);

    //5.计算去掉iap+1k的文件长度，这iap+1k部分不要了
    fseek(fin, file_offset, SEEK_SET);   //读取的位置也是不从0开始
    size -= (file_offset);  //去掉偏移的字节
    if(g_debug_mode)  //调试模式下才打印
    	printf("file size = %d\r\n", size);

    //6.分配缓存，用于把文件的内容全部读出来
    char* buf = malloc(size);
    if(buf == NULL)
    {
    	printf("error: malloc %d\n",size);
    	fclose(fin);
    	return NULL;
    }

    //7.把文件读出来
    do
    {
        bw = fread(&buf[readcount], 1, size, fin);
        readcount += bw;
    } while (readcount < size);
    if(g_debug_mode)  //调试模式下才打印
    	printf("file readcount = %d\r\n", readcount);
    fclose(fin);

    //8.比较md5，对缓存中的内容计算md5，再与刚刚从bin文件中读取的md5进行比较
    ret = get_file_md5sum2(buf,size);
	if(ret == 0)
	{
		if(g_debug_mode)  //调试模式下才打印
			printf("get_file_md5sum = %s,strlen = %lu\n",md5_readBuf,strlen(md5_readBuf));
		//比较文件的md5
		if(strcmp(md5_readBuf,md5_value))  //md5异常，为无效bin文件，不能进行升级。
		{
			printf("md5 compare failed ! please check bin file!!!!\n");
			free(buf);
			return NULL;
		}
	}
	else  //9. md5 获取失败，不进行升级
	{
		printf("error : get_file_md5sum ret = %d\n",ret);
		free(buf);
		return NULL;
	}

	//10.对bin文件内容进行判断，单片机的bin文件0-3这4个字节一定是0x20000000开头
    if (((*(uint32_t*)buf) & 0xfFFE0000 ) != 0x20000000)
	{
		printf("image addr 0 != 0x20000000\n");
		printf("ERROR: bad image(bin)!!!!! update cancle!!!,please check bin file!!!");
		free(buf);
		return NULL;
	}
	else if(((*(uint32_t*)(buf+4)) & 0xfFFffc00 ) != ApplicationAddress)//单片机的bin文件4-7这4个字节一定是与镜像的偏移有关
	{
		printf("image  addr %#x != ApplicationAddress %#x\n",((*(uint32_t*)(buf+4)) & 0xfFFffc00 ),ApplicationAddress);
		printf("ERROR: bad image(bin)!!!!! update cancle!!!,please check again!!!");
		free(buf);
		return NULL;
	}
	*filesize = size;   //返回文件大小
	return buf;  //返回文件的内容的首地址
}



/*
*********************************************************************************************************
*	º¯ Êý Ãû: xymodem_send
*	¹¦ÄÜËµÃ÷: ·¢ËÍÎÄ¼þ
*	ÐÎ    ²Î: filename  ÎÄ¼þÃû
*	·µ »Ø Öµ: 0  ÎÄ¼þ·¢ËÍ³É¹¦
*********************************************************************************************************
*/
int xymodem_send(const char *filename)
{
	int ret;
    int skip_payload = 0;
    int timeout = 0;
	char data[2] = {0};
    int size = 0;
    int recv_0x43 = 0;//adcount = 0, remain = 0;
	char *buf;

	//1.读取bin文件，并且判断md5是否正确，返回bin文件内容的首地址（正常返回时，bin文件已经被读出来了，存在在buf指定的控件）
	//md5正确时，对应的md5存放在全局变量md5_readBuf中。
	buf = file_read_check(filename,&size);
	if(buf == NULL)
	{
		printf("error : bin_file_read_check\n");
		return -1;
	}

	//2.读取串口缓存中的所有数据，以免被缓存的数据干扰，等待新的数据过来
	do
	{
		ret = UART_ReceiveByte (data, 500);  //这是超时读取，500ms的超时
		if(!ret && data[0] == 0x43)
			recv_0x43 = 1;
	}while(ret == 0);  //如果有数据就一直读，直到返回-1，表示没有读到数据了。

	//3.收到的不是0x43，表示单片机此时没有进入到下载模式，需要发送下载命令，让单片机进入到下载模式
	if(!recv_0x43)  //没有收到数据，或者收到的不是0x43，表示
	{
		if(g_debug_mode)  //调试模式下才打印
			printf("enter ready_to_update\n");
		//4.
		if(ready_to_update())   //不等于0就是退出
		{
			free(buf);
			printf("error return : ready_to_update()\n");
			return -1;
		}	

		//5.单片机进入到下载模式时，会不断给上位机发送0x43的数据。等待这个数据，用于单片机进入下载模式
		do
		{
			if(g_debug_mode)  //调试模式下才打印
				printf("wait for mcu ready ...  ... timeout = %d \n",timeout++);
			if(timeout >= 600)   //10·等待超时退出
			{
				printf("wait for mcu ready timeout,abort now \n");
				free(buf);
				return -1;
			}
			ret = UART_ReceiveByte (data,  1000);
			if(ret == 0)
			{
				if(g_debug_mode)  //调试模式下才打印
					printf("3.data[0] = %#x\n",data[0]);
				if(data[0] == 0x43)
				{
					if(g_debug_mode)  //调试模式下才打印
						printf("recive 0x43 ----2\n");
					break;
				}						
			}
		}while(1);


	}
	else if(g_debug_mode)  //调试模式下才打印  //6.表示读到了0x43，表示单片机此时已经进入到下载模式，在不断给上位机发送0x43的数据。
		printf("recive 0x43 ----1\n");

	if(g_debug_mode)  //调试模式下才打印
		printf("go to update now!!!\n");	

	//7.一切准备就绪，开始发送流程
    Ymodem_Transmit(buf, filename, size);

    //8.释放空间
    free(buf);
    return 0;
}


