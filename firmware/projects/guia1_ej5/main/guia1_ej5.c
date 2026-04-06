/*! @mainpage Template
 *
 * @section genDesc General Description
 *
 * This section describes how the program works.
 *
 * <a href="https://drive.google.com/...">Operation Example</a>
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	PIN_X	 	| 	GPIO_X		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 25/03/2026 | Document creation		                         |
 *
 * @author Zahira Garces Antar (zahira.garces@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <gpio_mcu.h>
/*==================[macros and definitions]=================================*/

/*==================[internal data definition]===============================*/
typedef struct
{
	gpio_t pin; // numero de pin
	io_t dir;	// si es IN/OUT
} gpioConf_t;

/*==================[internal functions declaration]=========================*/
void BCD_a_GPio(uint8_t bcd, gpioConf_t *vec)
{
	for (int i = 0; i < 4; i++)
	{
		if (bcd & (1 << i))
		{
			GPIOOn(vec[i].pin);
		}
		else
		{
			GPIOOff(vec[i].pin);
		}
	}
}
/*==================[external functions definition]==========================*/
void app_main(void)
{
	gpioConf_t vec_GPio[4] = {{GPIO_20, GPIO_OUTPUT},
							  {GPIO_21, GPIO_OUTPUT},
							  {GPIO_22, GPIO_OUTPUT},
							  {GPIO_23, GPIO_OUTPUT}};

	for (int i = 0; i < 4; i++)
	{
		GPIOInit(vec_GPio[i].pin, vec_GPio[i].dir);
	}

	BCD_a_GPio(7, vec_GPio);

}
/*==================[end of file]============================================*/