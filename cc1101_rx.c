#include <stdio.h>
#include <math.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include "cc1101.h"

#define RSSI_SAVE_INTERVAL 120//--seconds

//------global variables -----
struct timeval t_fstart,t_start,t_end;
long cost_time=0;
struct tm *tm_local;

char* str_file="/home/cc1101.dat";
FILE *fp; //save file 

int nTotal=0;
int nDATA_rec=0; // all data number received include nDATA_err, excetp CRC error.
int nDATA_err=0;
int nCRC_err=0;
int nFAIL=0;

void ExitProcess()
{
	float CRC_err_rate;
	printf("-------Exit Process------\n");
	printf("nTotal=%d\n",nTotal);
	printf("nDATA_rec=%d\n",nDATA_rec);
	printf("nDATA_err=%d\n",nDATA_err);
	printf("nCRC_err=%d\n",nCRC_err);
	printf("nFAIL=%d\n",nFAIL);
	CRC_err_rate=(float)nCRC_err/nTotal*100.0;
	printf("CRC error rate: %5.2f %%\n",CRC_err_rate);

	fclose(fp);
	SPI_Close();
	exit(0);
}

void SaveData(char* str)
{
	if(fp)
	{
		fputs(str,fp);
	}

}


//======================= MAIN =============================
int main(void)
{
	int len,i,j;
	int ret,ccret;
	int dbmRSSI;
	uint8_t Txtmp,data[32];
	unsigned char TxBuf[DATA_LENGTH];
	unsigned char RxBuf[DATA_LENGTH];

 	memset(data,0,sizeof(data));
	//memset(TxBuf,0,sizeof(TxBuf));
	memset(RxBuf,0,sizeof(RxBuf));

	//TxBuf[0]=0xac;
	//TxBuf[1]=0xab;

        //-------- Exit Signal Process ---------
        signal(SIGINT,ExitProcess);
	//-------- open file for RSSI record ----------------
	fp=fopen(str_file,"a"); // append data to a file, write only
	if (fp==(FILE *)NULL)
	{
		printf("%s: %s\n",str_file,strerror(errno));
		exit(-1);
	}
	fprintf(fp,"-----RSSI------\n");
	gettimeofday(&t_fstart,NULL); //timer for data save, every 10 minutes.


	SPI_Open();

        //Txtmp=0x3b;
        //SPI_Write(&Txtmp,1); //clear RxBuf in cc1101
	///Txtmp=0x3b|READ_SINGLE;
        //ret=SPI_Transfer(&Txtmp,RxBuf,1,1);
	//printf("RXBYTES: ret=%d, =x%02x\n",ret,RxBuf[0]);

	//Txtmp=0x3b|READ_BURST;
	//ret=SPI_Write_then_Read(&Txtmp,1,RxBuf,2); //RXBYTES
	//printf("RXBYTES: ret=%d, =x%02x %02x\n",ret,RxBuf[0],RxBuf[1]);

/*	//--------- register operation fucntion test  -----
        //halSpiWriteReg(0x25,0x7f);
	data[0]=data[1]=data[2]=data[3]=data[4]=0xff;
	halSpiWriteBurstReg(0x25,data,5);
	halSpiReadBurstReg(0x25,RxBuf,5);
	printf("halSpiReadBurstReg(0x25,RxBuf,5) : ");
	for(i=0;i<5;i++)
	    printf("%02x",RxBuf[i]);
	printf("\n");

	printf("halSpiReadReg(0x25)=x%02x\n",halSpiReadReg(0x25));

	printf("halSpiReadReg(0x31)=x%02x\n",halSpiReadReg(0x31));
	printf("halSpiReadStatus(0x31) Chip ID: x%02x\n",halSpiReadStatus(0x31));
*/

	//-----init CC1101-----
	//set SPI CLOCK = 5MHZ; MAX. SPI CLOCK=6.5MHz for burst access.
        usleep(200000);//Good for init?
        RESET_CC1100();
        usleep(200000);
	halRfWriteRfSettings(); //--default register set
        //----set symbol rate,deviatn,filter BW properly -------
	setFreqDeviatME(0,4); //deviation: 25.4KHz
        setKbitRateME(248,10); //rate: 50kbps
	setChanBWME(0,3); //filterBW: (3,3)58KHz (0,3)102KHz
	setChanSpcME(248,2);//channel spacing: (248,2)200KHz 
	setCarFreqMHz(433,0);//(Base freq,+channel_number)

	halSpiWriteBurstReg(CCxxx0_PATABLE,PaTabel,8);//---set Power

	printf("----------  CC1101 Configuration --------\n");
	//---- get Modulation Format  ---
	printf("Set modulation format to    %s\n",getModFmtStr());
	//----- get symbol Kbit rate ----
        printf("Set symbol rate to       %8.1f Kbit/s\n",getKbitRate());
        //------ get carrier frequency ---
        printf("Set carrier frequency to   %8.3f MHz\n",getCarFreqMHz());
	//------ get freq deviation -----
	printf("Set freq deviation to    %8.1f  KHz\n",getFreqDeviatKHz());
	//------ get  IF frequency ----
	printf("Set IF to                  %8.3f KHz\n",getIfFreqKHz());
	//------ get channel Bandwith ----
	printf("Set channel BW to          %8.3f KHz\n", getChanBWKHz());
	//------ get channel spacing  ----
	printf("Set channel spacing to     %8.3f KHz\n",getChanSpcKHz());

	printf("----------------------------------------\n");
	sleep(2);
        //----- transmit data -----
//        halRfSendPacket(TxBuf,DATA_LENGTH); //DATA_LENGTH);

	//----- receive data -----
	len=33; //max.29
	j=0;
  	while(1) //------ !!! Wait a little time just before setting up for next  TX_MODE !!!
	{
	   j++;
	   gettimeofday(&t_start,NULL);
  	   ccret=halRfReceivePacket(RxBuf,DATA_LENGTH);
	   gettimeofday(&t_end,NULL);
	   //printf("Receive Start at:%lds %ldus\n",t_start.tv_sec,t_start.tv_usec);
	   //printf("Receive End at:%lds %ldus\n",t_end.tv_sec,t_end.tv_usec);

	   //------ save RSSI every 10 minuts 
	   if((t_end.tv_sec-t_fstart.tv_sec)>RSSI_SAVE_INTERVAL)
	   {
		tm_local=localtime(&(t_end.tv_sec));
		fprintf(fp,"%d-%d-%d %02d:%02d:%02d  ",tm_local->tm_year,tm_local->tm_mon,tm_local->tm_mday,\
tm_local->tm_hour,tm_local->tm_min,tm_local->tm_sec);
		fprintf(fp,"RSSI:%ddBm\n",getRSSIdbm());
		//SaveData();
		gettimeofday(&t_fstart,NULL);
	   }

	   printf("Receive Cost Time:%d\n",t_end.tv_usec-t_start.tv_usec);
  	   if(ccret==1) //receive success
	   {
		nDATA_rec++;
		printf("------ nDATA_rec=%d ------\n",nDATA_rec);
		if(1)//j%10 == 0)
		{
 			printf("%dth received data:",j);
			for(i=0;i<len;i++)
				printf("%d, ",RxBuf[i]);
			printf("\n");
			printf("--------- getRSSI from RX_FIFO : %ddBm --------\n",getRSSIdbm());
			printf("--------- readRSSI from RSSI register: %ddBm --------\n",readRSSIdbm());

		}
		//---pick error data
		if(RxBuf[0]!=RxBuf[1])
		{
 			nDATA_err++;
			printf("------ nDATA_rec=%d ------\n",nDATA_err);
 			printf("%dth received data ERROR! :",j);
			for(i=0;i<len;i++)
				printf("%d, ",RxBuf[i]);
			printf("\n");
		}
	    }

  	   else if(ccret==2) // receive CRC error
	   {
 		nCRC_err++;
		printf("------ nCRC_err=%d ------\n",nCRC_err);
	   }
	   else if(ccret==0)//fail
	   {
		nFAIL++;
		printf("------ nFAIL=%d ------\n",nFAIL);
	   }

	   nTotal++;
        }//while
/*
	//---- check status ---- 
	for(i=0;i<10;i++)
	{
	 	usleep(100);
		printf("Status Byte:0x%02x\n",halSpiGetStatus());
	}
*/
	SPI_Close();
}
