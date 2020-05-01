// ADCTestMain.c
// Runs on LM4F120/TM4C123
// This program periodically samples ADC channel 1 and stores the
// result to a global variable that can be accessed with the JTAG
// debugger and viewed with the variable watch feature.
// Daniel Valvano
// October 20, 2013

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2013

 Copyright 2013 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */

// input signal connected to PE2/AIN1

#include "UART.c"
#include "ADCSWTrigger.h"
#include "tm4c123gh6pm.h"
#include "PLL.h"
#include <stdio.h>
#include <string.h>

#define GPIO_PORTF_DATA_R       (*((volatile unsigned long *)0x400253FC))
#define GPIO_PORTF_DIR_R        (*((volatile unsigned long *)0x40025400))
#define GPIO_PORTF_AFSEL_R      (*((volatile unsigned long *)0x40025420))
#define GPIO_PORTF_PUR_R        (*((volatile unsigned long *)0x40025510))
#define GPIO_PORTF_DEN_R        (*((volatile unsigned long *)0x4002551C))
#define GPIO_PORTF_LOCK_R       (*((volatile unsigned long *)0x40025520))
#define GPIO_PORTF_CR_R         (*((volatile unsigned long *)0x40025524))
#define GPIO_PORTF_AMSEL_R      (*((volatile unsigned long *)0x40025528))
#define GPIO_PORTF_PCTL_R       (*((volatile unsigned long *)0x4002552C))
#define SYSCTL_RCGC2_R          (*((volatile unsigned long *)0x400FE108))
#define NVIC_SYS_PRI3_R         (*((volatile unsigned long *)0xE000ED20))  // Sys. Handlers 12 to 15 Priority
#define NVIC_ST_CTRL_R          (*((volatile unsigned long *)0xE000E010))
#define NVIC_ST_RELOAD_R        (*((volatile unsigned long *)0xE000E014))
#define NVIC_ST_CURRENT_R       (*((volatile unsigned long *)0xE000E018))

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
void PortF_Init(void);
void Delay(void);

volatile unsigned long Counts = 0;

typedef struct{
	volatile unsigned long ADCvalue;
	volatile unsigned long ADCvalue1;
	int Flag;
} Mailbox;

Mailbox mail;

// **************SysTick_Init*********************
// Initialize SysTick periodic interrupts
// Input: interrupt period
//        Units of period are 62.5ns (assuming 16 MHz clock)
//        Maximum is 2^24-1
//        Minimum is determined by length of ISR
// Output: none

void SysTick_Init(unsigned long period){
  NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_RELOAD_R = period-1;// reload value
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0x00FFFFFF)|0x40000000; // priority 2
                              // enable SysTick with core clock and interrupts
  NVIC_ST_CTRL_R = 0x07;
  EnableInterrupts();
}
// Interrupt service routine
// Executed every 12.5ns*(period) 12.5 for 80 Mhz clock 1/80000000 = 12.5
void SysTick_Handler(void){
  ADC0_PSSI_R = 0x0008;            // 1) initiate SS3
	ADC1_PSSI_R = 0x0008;
  while((ADC0_RIS_R&0x08)==0){};   // 2) wait for conversion done
	while((ADC1_RIS_R&0x08)==0){};
  mail.ADCvalue = ADC0_SSFIFO3_R&0xFFF;   // 3) read result
	mail.ADCvalue1 = ADC1_SSFIFO3_R&0xFFF;   // 3) read result
  ADC0_ISC_R = 0x0008;
	ADC1_ISC_R = 0x0008;
		
	mail.Flag = 1;
}

//---------------------OutCRLF---------------------
// Output a CR,LF to UART to go to a new line
// Input: none
// Output: none
void OutCRLF(void){
  UART_OutChar(CR);
  UART_OutChar(LF);
}

//volatile unsigned long ADCvalue;
// The digital number ADCvalue is a representation of the voltage on PE4 
// voltage  ADCvalue
// 0.00V     0
// 0.75V    1024
// 1.50V    2048
// 2.25V    3072
// 3.00V    4095
// .1 - 2

int main(void){
	unsigned long volatile delay;
	char string[20];
	char string1[40];
	double ADCv = 0.0;
	int photo = 0;
	double tempf = 0.0;
	double tempc = 0.0;
	int i = 0;
	//int temp = 0;
	//int p;
  PLL_Init();                           // 80 MHz
  ADC0_InitSWTriggerSeq3_Ch1();         // ADC initialization PE2/AIN1 and PE3/AIN2
	ADC1_InitSWTriggerSeq3_Ch1();
	UART_Init();
	SysTick_Init(1000000); //40 Mhz = 2000000

  while(1){
		if(mail.Flag == 1)
		{
			if(i < 5){
			//Uart stuff convert ADCvalue to tempurature
			// temp from -40 to 125
			// sample from 0 to 4095
			ADCv = mail.ADCvalue;
			photo = mail.ADCvalue1;
			tempc = (((((ADCv/4095) * .83) + .1) * 1000) - 500)/10;
			tempf = ((tempc * 9) /5) + 32;
			// could just put these in a buffer and send the average of buffer every second
			
			// For project {
//			snprintf(string1, 20, "%0.2f", tempc);
//			strcat(string1, " \xb0 C"); // degrees celcius
//			UART_OutString(string1);
//			OutCRLF();
//			snprintf(string1, 20, "%0.2f", tempf);
//			strcat(string1, " \xb0 F");// degrees fahrenheit
//			UART_OutString(string1);
//			OutCRLF();
			
			if (photo <= 754){
				snprintf(string, 20, "Off");
				UART_OutString(string);
				OutCRLF();
			}
			else{
				if(photo <= 1201){
					if(photo <= 1116){
						if(tempf <= 79.26){
							snprintf(string, 20, "Off");
							UART_OutString(string);
							OutCRLF();
						}
						else{
							if(photo <= 1102){
								snprintf(string, 20, "On2");
								UART_OutString(string);
								OutCRLF();
							}
							else{
								snprintf(string, 20, "Off");
								UART_OutString(string);
								OutCRLF();
							}
						}
					}
					else{
						if(photo <= 1175){
							if(tempf <= 78.75){
								snprintf(string, 20, "On3");
								UART_OutString(string);
								OutCRLF();
							}
							else{
								if(tempf <= 79.05){
									if(tempf <= 78.83){
										if(photo <= 1137){
											snprintf(string, 20, "On4");
											UART_OutString(string);
											OutCRLF();
										}
										else{
											snprintf(string, 20, "Off");
											UART_OutString(string);
											OutCRLF();
										}
									}
									else{
										if(photo <= 1122){
											if(photo <= 1120){
												snprintf(string, 20, "On5");
												UART_OutString(string);
												OutCRLF();
											}
											else{
												snprintf(string, 20, "Off");
												UART_OutString(string);
												OutCRLF();
											}
										}
										else{
											snprintf(string, 20, "Off");
											UART_OutString(string);
											OutCRLF();
										}
									}
								}
								else{
									if(photo <= 1166){
										if(tempf <= 79.08){
											if(photo <= 1124){
												snprintf(string, 20, "On6");
												UART_OutString(string);
												OutCRLF();
											}
											else{
												snprintf(string, 20, "Off");
												UART_OutString(string);
												OutCRLF();
											}
										}
										else{
											if(tempf <= 79.3){
												snprintf(string, 20, "On7");
												UART_OutString(string);
												OutCRLF();
											}
											else{
												snprintf(string, 20, "Off");
												UART_OutString(string);
												OutCRLF();
											}
										}
									}
									else{
										snprintf(string, 20, "On8");
										UART_OutString(string);
										OutCRLF();
									}
								}
							}
						}
						else{
							if(tempf <= 78.97){
								if(tempf <= 78.75){
									snprintf(string, 20, "On9");
									UART_OutString(string);
									OutCRLF();
								}
								else{
									if(photo <= 1179){
										if(tempf <= 78.9){
											snprintf(string, 20, "Off");
											UART_OutString(string);
											OutCRLF();
										}
										else{
											if(tempf <= 78.94){
												if(photo <= 1177){
													snprintf(string, 20, "Off");
													UART_OutString(string);
													OutCRLF();
												}
												else{
													snprintf(string, 20, "On10");
													UART_OutString(string);
													OutCRLF();
												}
											}
											else{
												snprintf(string, 20, "On11");
												UART_OutString(string);
												OutCRLF();
											}
										}
									}
									else{
										if(tempf <= 78.83){
											snprintf(string, 20, "On12");
											UART_OutString(string);
											OutCRLF();
										}
										else{
											if(tempf <= 78.94){
												if(tempf <= 78.86){
													snprintf(string, 20, "Off");
													UART_OutString(string);
													OutCRLF();
												}
												else{
													if(photo <= 1188){
														snprintf(string, 20, "On13");
														UART_OutString(string);
														OutCRLF();
													}
													else{
														snprintf(string, 20, "Off");
														UART_OutString(string);
														OutCRLF();
													}
												}
											}
											else{
												if(photo <= 1194){
													snprintf(string, 20, "On14");
													UART_OutString(string);
													OutCRLF();
												}
												else{
													if(photo <= 1196){
														snprintf(string, 20, "Off");
														UART_OutString(string);
														OutCRLF();
													}
													else{
														snprintf(string, 20, "On14");
														UART_OutString(string);
														OutCRLF();
													}
												}
											}
										}
									}
								}
							}
							else{
								snprintf(string, 20, "On");
								UART_OutString(string);
								OutCRLF();
							}
						}
					}
				}
				else{
					snprintf(string, 20, "On");
					UART_OutString(string);
					OutCRLF();
				}
			}
				
//			snprintf(string1, 20, "%d", photo);
//			snprintf(string, 20, "%0.2f", tempf);
//			strcat(string1, ",");
//			strcat(string1, string);
//			UART_OutString(string1);
//			OutCRLF();
			
			i++;
		}
			
			
			// For phone {
//			p = tempc;
//			snprintf(string1, 20, "*R%0.2f", tempc);
//			UART_OutString(string1);
//			OutCRLF();
//			
//			snprintf(string1, 10, "*T%0.1f", tempc);
//			UART_OutString(string1);
//			OutCRLF();
//			
//			snprintf(string1, 10, "*D%d", p);
//			UART_OutString(string1);
//			OutCRLF();
//			
//			snprintf(string1, 10, "*F%0.2f", tempf);
//			UART_OutString(string1);
//			OutCRLF();
//			
//			snprintf(string1, 10, "*P%0.1f", tempf);
//			UART_OutString(string1);
//			OutCRLF();
//			
//			p = tempf;
//			snprintf(string1, 10, "*O%d", p);
//			UART_OutString(string1);
//			OutCRLF();
			// }
			
			//temp = tempc;
			mail.Flag = 0;
		}
  }
}

void PortF_Init(void){ volatile unsigned long delay;
  SYSCTL_RCGC2_R |= 0x00000020;     // 1) F clock
  delay = SYSCTL_RCGC2_R;           // delay   
  GPIO_PORTF_LOCK_R = 0x4C4F434B;   // 2) unlock PortF PF0  
  GPIO_PORTF_CR_R = 0x1F;           // allow changes to PF4-0       
  //GPIO_PORTF_AMSEL_R = 0x00;        // 3) disable analog function
  GPIO_PORTF_PCTL_R = 0x00000000;   // 4) GPIO clear bit PCTL  
  GPIO_PORTF_DIR_R = 0x0E;          // 5) PF4,PF0 input, PF3,PF2,PF1 output   
  //GPIO_PORTF_AFSEL_R = 0x00;        // 6) no alternate function
  GPIO_PORTF_PUR_R = 0x11;          // enable pullup resistors on PF4,PF0       
  GPIO_PORTF_DEN_R = 0x1F;          // 7) enable digital pins PF4-PF0 
	
}
// Color    LED(s) PortF
// dark     ---    0
// red      R--    0x02
// blue     --B    0x04
// green    -G-    0x08
// yellow   RG-    0x0A
// sky blue -GB    0x0C
// white    RGB    0x0E
// pink     R-B    0x06

void Delay(void){unsigned long volatile time;
  time = 727240*200/999;  // 0.1sec
	time = time * 10;
  while(time){
		time--;
  }
}
