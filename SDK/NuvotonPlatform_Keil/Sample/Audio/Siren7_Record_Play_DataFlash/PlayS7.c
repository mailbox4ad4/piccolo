/*---------------------------------------------------------------------------------------------------------*/
/*                                                                                                         */
/* Copyright(c) 2009 Nuvoton Technology Corp. All rights reserved.                                         */
/*                                                                                                         */
/* Siren7 (G.722) licensed from Polycom Technology                                                         */
/*---------------------------------------------------------------------------------------------------------*/
#include <stdio.h>
#include "ISD9xx.h"
#include "Driver\DrvPDMA.h"
#include "Driver\DrvDPWM.h"
#include "Driver\DrvFMC.h"


/*---------------------------------------------------------------------------------------------------------*/
/* Macro, type and constant definitions                                                                    */
/*---------------------------------------------------------------------------------------------------------*/
#define AUDIOBUFFERSIZE 320
#define DPWMSAMPLERATE  16000
#define S7BITRATE       16000
#define S7BANDWIDTH     7000
#define COMPBUFSIZE     20   //According to S7BITRATE


uint32_t	u32BufferAddr0, u32BufferAddr1;
uint32_t	AudioSampleCount,PDMA1CallBackCount,AudioDataAddr,BufferEmptyAddr,BufferReadyAddr;
BOOL	PCMPlaying,BufferEmpty,PDMA1Done;


#include "Lib\LibSiren7.h"
extern sSiren7_CODEC_CTL sEnDeCtl;
sSiren7_DEC_CTX sS7Dec_Ctx;


/*---------------------------------------------------------------------------------------------------------*/
/* Define Function Prototypes                                                                              */
/*---------------------------------------------------------------------------------------------------------*/
void PDMA1_Callback(void);
//void PDMA1forDPWM(void);




/*---------------------------------------------------------------------------------------------------------*/
/* InitialDPWM                                                                                             */
/*---------------------------------------------------------------------------------------------------------*/
void InitialDPWM(uint32_t u32SampleRate)
{
	DrvDPWM_Open();
	DrvDPWM_SetDPWMClk(E_DRVDPWM_DPWMCLK_HCLKX2);
	//DrvDPWM_SetDPWMClk(E_DRVDPWM_DPWMCLK_HCLK);
	DrvDPWM_SetSampleRate(u32SampleRate);
	DrvDPWM_Enable();
}





/*---------------------------------------------------------------------------------------------------------*/
/* Set PDMA0 to move ADC FIFO to MIC buffer with wrapped-around mode                                       */
/*---------------------------------------------------------------------------------------------------------*/
void PDMA1forDPWM(void)
{
	STR_PDMA_T sPDMA;  

	sPDMA.sSrcAddr.u32Addr 			= BufferReadyAddr; 
	sPDMA.sDestAddr.u32Addr 		= (uint32_t)&DPWM->FIFO;
	sPDMA.u8Mode 					= eDRVPDMA_MODE_MEM2APB;;
	sPDMA.u8TransWidth 				= eDRVPDMA_WIDTH_16BITS;
	sPDMA.sSrcAddr.eAddrDirection 	= eDRVPDMA_DIRECTION_INCREMENTED; 
	sPDMA.sDestAddr.eAddrDirection 	= eDRVPDMA_DIRECTION_FIXED;  
	//sPDMA.u8WrapBcr				 	= 0x5; 		//Interrupt condition set fro Half buffer & buffer end
    sPDMA.i32ByteCnt = AUDIOBUFFERSIZE * 2;	   	//Full MIC buffer length (byte)
    DrvPDMA_Open(eDRVPDMA_CHANNEL_1, &sPDMA);

	// PDMA Setting 
    //PDMA_GCR->PDSSR.ADC_RXSEL = eDRVPDMA_CHANNEL_2;
	DrvPDMA_SetCHForAPBDevice(
    	eDRVPDMA_CHANNEL_1, 
    	eDRVPDMA_DPWM,
    	eDRVPDMA_WRITE_APB    
	);

	// Enable DPWM DMA
	DrvDPWM_EnablePDMA();
	// Enable INT 
	DrvPDMA_EnableInt(eDRVPDMA_CHANNEL_1, eDRVPDMA_BLKD ); 
	// Install Callback function    
	DrvPDMA_InstallCallBack(eDRVPDMA_CHANNEL_1, eDRVPDMA_BLKD, (PFN_DRVPDMA_CALLBACK) PDMA1_Callback ); 	
	DrvPDMA_CHEnablelTransfer(eDRVPDMA_CHANNEL_1);

	PDMA1Done=FALSE;
}


/*---------------------------------------------------------------------------------------------------------*/
/* DPWM Callback                                                                                         */
/*---------------------------------------------------------------------------------------------------------*/
void PDMA1_Callback()
{
	if ((PDMA1CallBackCount&0x1)==0)
		{
			BufferReadyAddr=u32BufferAddr1;
			BufferEmptyAddr=u32BufferAddr0;
		}
	else
		{
			BufferReadyAddr=u32BufferAddr0;
			BufferEmptyAddr=u32BufferAddr1;
		}


	PDMA1CallBackCount++;
	if (BufferEmpty==FALSE)
		PDMA1forDPWM();
	else
	{	
		printf("Late to copy audio data to buffer for PDMA\n");
		PDMA1Done=TRUE;
	}

	BufferEmpty=TRUE;
}


/*---------------------------------------------------------------------------------------------------------*/
/* Copy Data from flash to SRAM                                                                            */
/*---------------------------------------------------------------------------------------------------------*/
void CopyFlash2RAM(uint32_t *Addr1,uint32_t *Addr2,uint32_t WordCount)
{
uint32_t	u32LoopCount;
	for(u32LoopCount=0;u32LoopCount<WordCount;u32LoopCount++ )
	{
		*Addr2++ = *Addr1++;
	}
}  

void CopySoundData(uint32_t TotalPCMCount)
{
signed short s16Out_words[COMPBUFSIZE];

	CopyFlash2RAM((uint32_t *)AudioDataAddr,(unsigned long *)s16Out_words, (COMPBUFSIZE/2));

	LibS7Decode(&sEnDeCtl, &sS7Dec_Ctx, s16Out_words, (signed short *)BufferEmptyAddr);

	AudioSampleCount=AudioSampleCount+AUDIOBUFFERSIZE;
	AudioDataAddr=AudioDataAddr+(COMPBUFSIZE*2);
	if (AudioSampleCount >TotalPCMCount)
		PCMPlaying=FALSE;
}


/*---------------------------------------------------------------------------------------------------------*/
/* Play sound initialization                                                                               */
/*---------------------------------------------------------------------------------------------------------*/
void PlaySound(uint32_t DataAddr, uint32_t TotalPCMCount)
{
	InitialDPWM(16000);
	AudioDataAddr= DataAddr;
	PCMPlaying=TRUE;
	AudioSampleCount=0;
	PDMA1CallBackCount=0;
	
	BufferEmptyAddr= u32BufferAddr0;
	CopySoundData(TotalPCMCount);
	BufferReadyAddr= u32BufferAddr0;
	PDMA1forDPWM();

	BufferEmptyAddr= u32BufferAddr1;
	BufferEmpty=TRUE;
}



/*---------------------------------------------------------------------------------------------------------*/
/*  Main Function									                                           			   */
/*---------------------------------------------------------------------------------------------------------*/
void PlayDataFlash(uint32_t PlayStartAddr, uint32_t TotalPCMCount)
{
//	uint8_t u8Option;
//	int32_t	i32Err;
//	uint32_t	u32temp;


__align(4) int16_t AudioBuffer[2][AUDIOBUFFERSIZE];
   	u32BufferAddr0=	(uint32_t) &AudioBuffer[0][0];
	u32BufferAddr1=	(uint32_t) &AudioBuffer[1][0];

    LibS7Init(&sEnDeCtl,S7BITRATE,S7BANDWIDTH);
    LibS7DeBufReset(sEnDeCtl.frame_size,&sS7Dec_Ctx);

    printf("+-------------------------------------------------------------------------+\n");
    printf("|       Playing 8K Sampling PCM	Start									      |\n");
    printf("+-------------------------------------------------------------------------+\n");

   
    DrvPDMA_Init();			//PDMA initialization
	UNLOCKREG();
	 
	PlaySound(PlayStartAddr, TotalPCMCount);


	while (PCMPlaying == TRUE)
	{
		if (BufferEmpty==TRUE)
		{
			CopySoundData(TotalPCMCount);
			BufferEmpty=FALSE;
		}
	
					
		if ((PDMA1Done==TRUE) && (BufferEmpty==FALSE))
			PDMA1forDPWM();

	}

	while(BufferEmpty==FALSE);			 //Waiting last audio buffer played

	//DrvPDMA_Close();
	DrvPDMA_DisableInt(eDRVPDMA_CHANNEL_1, eDRVPDMA_BLKD );		//Disable INT
	DrvDPWM_Close();
	UNLOCKREG();

	printf("Play Done\n");
	
	/* Lock protected registers */
	
}



