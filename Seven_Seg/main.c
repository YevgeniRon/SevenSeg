/* Example code for driving 7 segment 4 digits common anode display with a 74HC95 8-bit shift register and a Stellaris Launchpad
 *
 * The code cycles though 0-9 and A-F characters.
 *
 * Hardware:
 *
 * 74HC595 shift register attached to pins of Stellaris Launchpad (Tiva C):
 *Launchpad - 74HC595
 *	PB0   -	  SER	(DATA)
 *	PB1   -   SRCLK	(CLOCK)
 *	PB2   -   RCLK  (LATCH)
 *	PB7   -   SRCLK (RESET)
 *
 *	Pins to connect to common-anode LED display via a Stellaris Launchpad (Tiva C):
 *Launchpad - LED display
 *	PB3   -		DIG1 - pin 1 (LED Display)
 *	PB4   - 	DIG2 - pin 2 (LED Display)
 *	PB5   - 	DIG3 - pin 6 (LED Display)
 *	PB6   -		DIG4 - pin 8 (LED Display)
 *
 *
 *	Pins to connect to common-anode LED display via a 74HC595:
 *	74HC595 - LED display
 *	pin 15 - pin 14
 *	pin 1  - pin 16
 *	pin 2  - pin 13
 *	pin 3  - pin 3
 *	pin 4  - pin 5
 *	pin 5  - pin 11
 *	pin 6  - pin 15
 *
 *
 *  Created on: Jul 16, 2013
 *      By Overcheack Yevgeni Ron.
 */

#include <stdint.h> 				//Variable definitions for the C99 standard
#include <stdbool.h>				//Boolean definitions for the C99 standard

#include "inc/hw_memmap.h"			//Macros defining the memory map of the Tiva C Series device. This
//includes defines such as peripheral base address locations such as GPIO_PORTF_BASE.

#include "inc/hw_types.h"			//Defines common types and macros

#include "driverlib/gpio.h"			//Defines and macros for GPIO API of DriverLib. This includes API functions
//such as GPIOPinRead and GPIOPinWrite.

#include "driverlib/debug.h"

#include "driverlib/sysctl.h"		//Defines and macros for System Control API of DriverLib. This includes
//API functions such as SysCtlClockSet.

#include "examples/boards/ek-tm4c123gxl/drivers/buttons.h" //Defines prototypes for the evaluation board buttons driver

#include "driverlib/rom.h"			//Macros to facilitate calling functions in the ROM (reduces the code size of program in flash memory)

#include "driverlib/interrupt.h"	//Defines and macros for NVIC Controller (Interrupt) API of
//driverLib. This includes API functions such as IntEnable.

#include "inc/tm4c123gh6pm.h"		//Definitions for the interrupt and register assignments on the Tiva C
//Series device on the LaunchPad board



//-------Defines---------------//
#define DATA GPIO_PIN_0
#define SRCLK GPIO_PIN_1
#define RCLK GPIO_PIN_2
#define DIG1 GPIO_PIN_3
#define DIG2 GPIO_PIN_4
#define DIG3 GPIO_PIN_5
#define DIG4 GPIO_PIN_6
#define RESET GPIO_PIN_7
#define ALL_DIGITS DIG1|DIG2|DIG3|DIG4
#define ALL_PINS GPIO_PIN_0|GPIO_PIN_1| \
		GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7
//----------------------------//

#ifdef DEBUG
void__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

volatile int IntFlagLeft,IntFlagRight; 	//Flags used to indicate shift left/right

// Seven segment digits in bits - gfedcba
const uint8_t digit[] =
{
		0x7E, //0
		0x30, //1
		0x6D, //2
		0x79, //3
		0x33, //4
		0x5B, //5
		0x5F, //6
		0x70, //7
		0x7F, //8
		0x7B, //9
		0x77, //A
		0x1F, //B
		0x4E, //C
		0x3D, //D
		0x4F, //E
		0x47  //F
};

//Single clock pulse to shift register clock pin - Inputs and shifts serial data
void PulseClock(void)
{
	ROM_GPIOPinWrite(GPIO_PORTB_BASE,SRCLK,SRCLK);
	ROM_GPIOPinWrite(GPIO_PORTB_BASE,SRCLK,0);
}

//Single clock pulse to storage register clock pin  - Outputs parallel-out
void PulseLatch(void)
{
	ROM_GPIOPinWrite(GPIO_PORTB_BASE,RCLK,RCLK);
	ROM_GPIOPinWrite(GPIO_PORTB_BASE,RCLK,0);
}

//Direct Overide Clear
void Reset_Display(void)
{
	ROM_GPIOPinWrite(GPIO_PORTB_BASE,RESET,0);
	ROM_GPIOPinWrite(GPIO_PORTB_BASE,RESET,RESET);
}

//Buttons Interrupt handler
void ButtonIntHandler(void)
{
	uint32_t ulInts;

	//Check which button was pressed
	ulInts = GPIOIntStatus(BUTTONS_GPIO_BASE, true) ;

	if(ulInts & LEFT_BUTTON)
	{
		GPIOIntClear(BUTTONS_GPIO_BASE, GPIO_INT_PIN_4);
		IntFlagLeft = 1;
	}

	if(ulInts & RIGHT_BUTTON)
	{
		GPIOIntClear(BUTTONS_GPIO_BASE, GPIO_INT_PIN_0);
		IntFlagRight = 1;
	}

}


int main(void)
{
	int i,j;
	uint8_t shift_dig = DIG1; // uint8_t is unsigned char - C99 Standard

	//Configure the system clock to run using a 16MHz crystal on the main oscillator, driving
	//the 400MHz PLL. The 400MHz PLL oscillates at only that frequency, but can be driven
	//by crystals or oscillators running between 5 and 25MHz. There is a default /2 divider in
	//the clock path and we are specifying another /5, which totals 10. That means the System
	//Clock will be 40MHz.
	ROM_SysCtlClockSet(SYSCTL_SYSDIV_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);

	//Enable clock for Port B.
	ROM_SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);

	//Configures GPIO Port B pins 0 to 7 as output
	ROM_GPIOPinTypeGPIOOutput(GPIO_PORTB_BASE, ALL_PINS);

	//API function that initializes the GPIO pins used by the board pushbuttons
	ButtonsInit();

	//Enabling the specific vector associated with GPIO Port F
	IntEnable(INT_GPIOF);

	//Enables the indicated GPIO interrupt sources (pushbuttons 1 and 2)
	GPIOIntEnable(BUTTONS_GPIO_BASE, GPIO_INT_PIN_0 | GPIO_INT_PIN_4);

	//Clear Interrupts
	GPIOIntClear(BUTTONS_GPIO_BASE, GPIO_INT_PIN_0 | GPIO_INT_PIN_4);

	//Enables a specific event within the pins to generate interrupt
	ROM_GPIOIntTypeSet(BUTTONS_GPIO_BASE, ALL_BUTTONS, GPIO_RISING_EDGE);

	//Master interrupt enable API for all interrupts
	IntMasterEnable();

	//Resets LED Display
	Reset_Display();

	//Enables DIG1 on LED display - power on default
	ROM_GPIOPinWrite(GPIO_PORTB_BASE,ALL_DIGITS, shift_dig);


	while(1)
	{
		//Loops through all the characters
		for (j = 0 ; j < 16 ; j++)
		{
			//Loops through each bit in a specific character and sends it to serial input
			for (i = 0 ; i < 7 ; i++)
			{
				//Common Anode display - write LOW to the specific led segment
				ROM_GPIOPinWrite(GPIO_PORTB_BASE,DATA, ~(DATA & (digit[j]>>i)) );
				PulseClock();
			}

			//Checks which button was pressed by using asserted flag from interrupt
			if (IntFlagLeft)
			{
				IntFlagLeft = 0;
				if (shift_dig == DIG1)
					shift_dig = DIG4;
				else
					//Enable only the next left digit in LED display
					shift_dig = shift_dig>>1;
			}
			else if (IntFlagRight)
			{
				IntFlagRight = 0;
				if (shift_dig == DIG4)
					shift_dig = DIG1;
				else
					//Enable only the next right digit in LED display
					shift_dig = shift_dig<<1;
			}

			//Write HIGH to the digit which was chosen before
			ROM_GPIOPinWrite(GPIO_PORTB_BASE,ALL_DIGITS, shift_dig);
			PulseLatch();

			//API function for delay - each loop takes 3 CPU cycles.
			//Each CPU cycle takes 1/(40E9) [1/Hz] = 25 [ns]
			//Delay of 25E-9 * 25E5 = 62.5 [ms]
			SysCtlDelay(2500000);
		}
	}
}
