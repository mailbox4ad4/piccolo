/*---------------------------------------------------------------------------------------------------------*/
/*                                                                                                         */
/* Copyright(c) 2009 Nuvoton Technology Corp. All rights reserved.                                         */
/*                                                                                                         */
/*---------------------------------------------------------------------------------------------------------*/
#include <stdio.h>
#include "ISD9xx.h"
#include "Driver\DrvUART.h"
#include "Driver\DrvSYS.h"
#include "Driver\DrvGPIO.h"
//#include "Driver\DrvADC.h"

#include "Lib\libSPIFlash.h"


/*---------------------------------------------------------------------------------------------------------*/
/* Define Function Prototypes                                                                              */
/*---------------------------------------------------------------------------------------------------------*/
void InitialUART(void);
void Record2SPIFlash(uint32_t RecordStartAddr,uint32_t TotalPCMCount);
void PlaySPIFlash(uint32_t PlayStartAddr,uint32_t TotalPCMCount);


/*---------------------------------------------------------------------------------------------------------*/
/* Define global variables                                                                                 */
/*---------------------------------------------------------------------------------------------------------*/

#define RECORD_START_ADDR		0x20000		//0x20000 = 128KB, need 4K aligned for first address of a sector
#define MAX_RECORD_COUNT		10 * 8 * 1024	//maximum sampling points in each recording, should be multiple of 1024



//====================
// Functions & main

void UartInit(void)
{
	/* Reset IP */
	SYS->IPRSTC2.UART0_RST = 1;
	SYS->IPRSTC2.UART0_RST = 0;
	/* Enable UART clock */
    SYSCLK->APBCLK.UART0_EN = 1;
    /* Data format */
    UART0->LCR.WLS = 3;
    /* Configure the baud rate */
    M32(&UART0->BAUD) = 0x3F0001A8; /* Internal 48MHz, 115200 bps */

    /* Multi-Function Pin: Enable UART0:Tx Rx */
	SYS->GPA_ALT.GPA8 = 1;
	SYS->GPA_ALT.GPA9 = 1;
}

/*---------------------------------------------------------------------------------------------------------*/
/* SysTimerDelay                                                                                           */
/*---------------------------------------------------------------------------------------------------------*/
void SysTimerDelay(uint32_t us)
{
    SysTick->LOAD = us * 49; /* Assume the internal 49MHz RC used */
    SysTick->VAL  =  (0x00);
    SysTick->CTRL = (1 << SYSTICK_CLKSOURCE) | (1<<SYSTICK_ENABLE);

    /* Waiting for down-count to zero */
    while((SysTick->CTRL & (1 << 16)) == 0);
}


//====================================
// SPI-flash Base and initialization
#ifndef SPI0_BASE
#define SPI0_BASE (0x40000000 + 0x30000)
#endif // SPI0_BASE

static void spi_setcs(const struct tagSFLASH_CTX *ctx, int csOn)
{
	// use the SPI controller's CS control
}

const SFLASH_CTX g_SPIFLASH =
{
	SPI0_BASE,
	1, // channel 0, auto
	spi_setcs
};

#define SPIFLASH_CTX(arg) (&g_SPIFLASH)

void SpiFlashInit(void)
{
	SYS->GPA_ALT.GPA0              = 1; // MOSI0
	SYS->GPA_ALT.GPA2              = 1; // SSB0
	SYS->GPA_ALT.GPA1              = 1; // SCLK
	SYS->GPA_ALT.GPA3              = 1; // MISO0

	SYSCLK->APBCLK.SPI0_EN        = 1;
	SYS->IPRSTC2.SPI0_RST         = 1;
	SYS->IPRSTC2.SPI0_RST         = 0;

	// spi flash
	sflash_set(&g_SPIFLASH);
}


void LdoOn(void)
{
	SYSCLK->APBCLK.ANA_EN=1;
	ANA->LDOPD.PD=0;
	ANA->LDOSET=3;

	SysTimerDelay(500000);
}

void LcdSignalPrevention(void)
{
	DrvGPIO_Open(GPA,14, IO_OUTPUT);
	DrvGPIO_Open(GPA,15, IO_OUTPUT);
	DrvGPIO_SetBit(GPA,15);		//Set CS high if jumper J2 & J32 installed 
	DrvGPIO_SetBit(GPA,14);		//Set backlight off
}
/*---------------------------------------------------------------------------------------------------------*/
/*  Main Function									                                           			   */
/*---------------------------------------------------------------------------------------------------------*/
int32_t main (void)
{
   
int iRet;

	UNLOCKREG();
	SYSCLK->PWRCON.OSC49M_EN = 1;
	SYSCLK->CLKSEL0.HCLK_S = 0; /* Select HCLK source as 48MHz */ 
	SYSCLK->CLKDIV.HCLK_N  = 0;	/* Select no division          */
	SYSCLK->CLKSEL0.OSCFSel = 0;	/* 1= 32MHz, 0=48MHz */


	DrvADC_AnaOpen();

	/* Set UART Configuration */
    //UartInit();

	//LcdSignalPrevention();
//--------------------------------------------//
//	Initial SPI-flash interface

	LdoOn();
	
	SpiFlashInit();
	iRet= sflash_getid(&g_SPIFLASH);
	//printf("\n Device ID %x \n", iRet);

	//outpw(0x40030004,0);		//Change SPI divider to 0 for 24MHz, need good PCB layout
	iRet= sflash_canwrite(&g_SPIFLASH);


	//printf("\n=== 8K sampling PCM Recording to SPIFlash  ===\n");
	Record2SPIFlash(RECORD_START_ADDR,MAX_RECORD_COUNT);


	//printf("\n=== Play PCM from SPIFlash ===\n");
	PlaySPIFlash(RECORD_START_ADDR,MAX_RECORD_COUNT);

	//printf("\n=== Test Done ===\n");


	while(1);

	/* Lock protected registers */
	
}



