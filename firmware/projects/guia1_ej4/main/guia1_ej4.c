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
 * | 18/03/2026 | Document creation		                         |
 *
 * @author Zahira Garces Antar (zahira.garces@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
/*==================[macros and definitions]=================================*/

/*==================[internal data definition]===============================*/

/*==================[internal functions declaration]=========================*/
int8_t convertToBcdArray(uint32_t data, uint8_t digits, uint8_t *bcd_number)
{
    for (int8_t i = digits - 1; i >= 0; i--)
    {
        bcd_number[i] = data % 10;  // guardo el último dígito
        data /= 10;                 // elimino el último dígito
    }

    return 0;
}
/*==================[external functions definition]==========================*/
void app_main(void){

	uint8_t bcd[10];
    int32_t numero = 12345;

    convertToBcdArray(numero, 10, bcd);

}
/*==================[end of file]============================================*/