/*! @mainpage Convertir a BCD y mostrar en display
 *
 * @section
 *
 * Este módulo implementa la función de visualización de un dato de 32 bits sobre un conjunto de dígitos de un display de 7 segmentos controlado
 * mediante decodificadores BCD a 7 segmentos.
 *
 *
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	D1  	 	| 	GPIO_20   	|
 * | 	D2  	 	| 	GPIO_21   	|
 * | 	D3  	 	| 	GPIO_22   	|
 * | 	D4  	 	| 	GPIO_23   	|
 * | 	SEL_1  	 	| 	GPIO_19   	|
 * | 	SEL_2  	 	| 	GPIO_18   	|
 * | 	SEL_3  	 	| 	GPIO_9   	|
 * | 	+5V  	 	| 	+5V     	|
 * | 	GND      	| 	GND     	|
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 08/04/2026 | Document creation		                         |
 *
 * @author Zahira Garces Antar (zahira.garces@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_mcu.h"
/*==================[macros and definitions]=================================*/

/*==================[internal data definition]===============================*/
/**
 * @brief Estructura de configuración de un pin GPIO.
 *
 * Almacena el número de pin y su dirección (entrada o salida).
 */
typedef struct
{
	gpio_t pin; // numero de pin
	io_t dir;	// si es IN/OUT
} gpioConf_t;

/*==================[internal functions declaration]=========================*/
/**
 * @brief Convierte un dato entero en un arreglo de dígitos BCD.
 *
 * @details Descompone el valor @p data en sus dígitos decimales individuales,
 *          almacenándolos en el arreglo @p bcd_number desde el dígito más
 *          significativo (posición 0) hasta el menos significativo
 *          (posición @p digits - 1).
 *
 * @param[in]  data        Valor de 32 bits a convertir.
 * @param[in]  digits      Cantidad de dígitos BCD a extraer.
 * @param[out] bcd_number  Puntero al arreglo donde se almacenan los dígitos BCD.
 *
 * @return  codigo de error 
 */
int8_t convertToBcdArray(uint32_t data, uint8_t digits, uint8_t *bcd_number)
{
	for (int8_t i = digits - 1; i >= 0; i--)
	{
		bcd_number[i] = data % 10; // guardo el último dígito
		data /= 10;				   // elimino el último dígito
	}

	return 0;
}
/**
 * @brief Escribe un valor BCD en el bus de GPIOs conectado al decodificador.
 *
 * @details Recorre los 4 bits del valor @p bcd y activa o desactiva el GPIO
 *          correspondiente en el vector @p vec según el estado de cada bit,
 *          utilizando las funciones GPIOOn() y GPIOOff().
 *
 * @param[in] bcd  Valor BCD de 4 bits a escribir en el bus.
 * @param[in] vec  Vector de estructuras @c gpioConf_t con los 4 pines del
 *                 bus de datos (bit 0 → vec[0], bit 3 → vec[3]).
 * 
 * @return  void
 */
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
/**
 * @brief Muestra un dato de 32 bits en un display de 7 segmentos multiplexado.
 *
 * @details Convierte @p data en un arreglo BCD mediante convertToBcdArray(),
 *          luego itera sobre cada dígito: escribe el valor BCD en el bus GPIO
 *          con BCD_a_GPio(), habilita el dígito correspondiente del display
 *          activando su pin de control (GPIOOn) y lo desactiva al finalizar
 *          (GPIOOff) para continuar con el siguiente dígito.
 *
 * @param[in] data     Valor de 32 bits a visualizar.
 * @param[in] digits   Cantidad de dígitos a mostrar.
 * @param[in] vec      Vector de estructuras @c gpioConf_t con los pines del
 *                     bus de datos BCD (4 pines: GPIO_20 a GPIO_23).
 * @param[in] puertos  Vector de estructuras @c gpioConf_t con los pines de
 *                     habilitación de cada dígito del display:
 *                     - Digito 1 → GPIO_19
 *                     - Dígito 2 → GPIO_18
 *                     - Dígito 3 → GPIO_9
 * 
 * @return  void
 */
void final_display(uint32_t data, uint8_t digits, gpioConf_t *vec, gpioConf_t *puertos)
{
	uint8_t bcd_number[digits];

	convertToBcdArray(data, digits, bcd_number);

	for (int i = 0; i < digits; i++)
	{
		BCD_a_GPio(bcd_number[i], vec);
		GPIOOn(puertos[i].pin);
		GPIOOff(puertos[i].pin);
	}
}
/*==================[external functions definition]==========================*/
void app_main(void)
{
	uint32_t numero = 2403;

	gpioConf_t vec_GPio[4] = {{GPIO_20, GPIO_OUTPUT},
							  {GPIO_21, GPIO_OUTPUT},
							  {GPIO_22, GPIO_OUTPUT},
							  {GPIO_23, GPIO_OUTPUT}};

	gpioConf_t vec_puertos[3] = {{GPIO_19, GPIO_OUTPUT},
								 {GPIO_18, GPIO_OUTPUT},
								 {GPIO_9, GPIO_OUTPUT}};

	for (int i = 0; i < 4; i++)
	{
		GPIOInit(vec_GPio[i].pin, vec_GPio[i].dir);
	}

	for (int i = 0; i < 3; i++)
	{
		GPIOInit(vec_puertos[i].pin, vec_puertos[i].dir);
	}

	final_display(numero, 4, vec_GPio, vec_puertos);
}
/*==================[end of file]============================================*/