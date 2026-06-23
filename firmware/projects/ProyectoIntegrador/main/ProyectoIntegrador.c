/*! @mainpage ASISTENTE INTELIGENTE PARA APLICACIONES MANUALES DE PULVERIZACIONES
 *
 * @section desc_general Descripción General
 * Este sistema basado en microcontrolador actúa como un asistente en tiempo real para
 * optimizar la aplicación manual de agroquímicos. Su objetivo es garantizar que el
 * operario aplique la dosis correcta de forma uniforme, proporcionando alertas visuales
 * y registrando los datos de la jornada.
 *
 * @section func_sec Funcionamiento del Sistema
 * El dispositivo procesa continuamente dos variables de entrada:
 * - **Caudal (L/min):** Calculado a través de los pulsos de un sensor de flujo en la boquilla.
 * - **Velocidad de avance (m/s):** Estimada mediante un sensor de movimiento (IMU/GPS).
 *
 * Con estas variables y el ancho de trabajo (configurado por el usuario), el algoritmo
 * calcula de forma continua la dosis real aplicada en hectáreas mediante la fórmula:
 * * \f[
 * Dosis = \frac{Caudal \times 600}{Ancho \times Velocidad}
 * \f]
 *
 * @section control_sec Control y Alertas (Feedback HMI)
 * La dosis real se compara en tiempo real con una dosis objetivo y un margen de tolerancia
 * configurados por el usuario, activando indicadores LED según el estado:
 * - **LED Rojo:** Sobreaplicación (Dosis > Límite Superior).
 * - **LED Amarillo:** Subaplicación (Dosis < Límite Inferior).
 * - **LED Verde:** Aplicación Correcta (Dentro del rango de tolerancia).
 *
 * @section com_sec Comunicación y Registro
 * Simultáneamente, el sistema transmite por Bluetooth los parámetros medidos y calculados
 * (caudal, velocidad, dosis real y estado) hacia una aplicación móvil o PC para su
 * visualización en tiempo real y registro histórico (Data Logging).
 *
 * @section hardConn Hardware Connection
 *
 * |        Periférico            |    ESP32     |
 * |:----------------------------:|:------------:|
 * | Sensor de caudal YF-S201     | GPIO_16      |
 * | Pulsador Inicio/Pausa        | SWITCH_1     |
 * | MPU6050 (SDA)                | SDA (I2C)    |
 * | MPU6050 (SCL)                | SCL (I2C)    |
 * | LED Verde                    | LED_1        |
 * | LED Amarillo                 | LED_2        |
 * | LED Rojo                     | LED_3        |
 * | Bluetooth BLE                | ESP32        |
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 27/05/2026 | Document creation		                         |
 *
 * @author Zahira Garces Antar (zahira.garces@ingenieria.uner.edu.ar)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_mcu.h"
#include "led.h"
#include "switch.h"
#include "timer_mcu.h"
#include "uart_mcu.h"
#include "i2c_mcu.h"
#include "ble_mcu.h"
/*==================[macros and definitions]=================================*/
/** @brief Período del temporizador para la tarea de cálculo (20 ms / 50Hz) para muestreo del MPU6050 */
#define CONFIG_PERIOD_CALCULO_US 20000 // 20 ms
/** @brief Período del temporizador para la tarea de comunicación (500 ms / 2Hz) */
#define CONFIG_PERIOD_COMUNIC_US 500000 // 500 ms
/** @brief Dirección esclava I2C del sensor MPU6050 */
#define MPU_ADDR 0x68
/** @brief Registro de gestión de energía del MPU6050 */
#define REG_PWR 0x6B
/** @brief Registro inicial de los datos de salida del acelerómetro */
#define REG_ACCEL 0x3B
/** @brief Pin GPIO asignado para la lectura del sensor de caudal YF-S201 */
#define GPIO_CAUDAL GPIO_16

/*==================[internal data definition]===============================*/
/** @brief Bandera global para pausar o activar la medición y el procesamiento del sistema */
bool midiendo = true;
/** @brief Almacena la velocidad de avance estimada del operario en metros por segundo (m/s) */
float velocidad_actual = 0.0f;
/** @brief Almacena el caudal actual calculado en litros por minuto (L/min) */
float caudal_actual = 0.0f;
/** @brief Almacena la dosis real calculada en litros por hectárea (L/ha) */
float dosis_real = 0.0f;
/** @brief Contador acumulativo de los pasos detectados mediante el MPU6050 */
int pasos_totales = 0;
/** @brief Variable volátil que acumula los pulsos del caudalímetro en la ISR */
volatile uint32_t pulsos_caudal = 0;
/** @brief Variable utilizada para proteger el acceso concurrente al contador de pulsos. */
portMUX_TYPE caudalMux = portMUX_INITIALIZER_UNLOCKED;
/** @brief Manejador de la tarea encargada de realizar los cálculos. */
TaskHandle_t realizarCalculos_task_handle = NULL;
/** @brief Manejador de la tarea encargada de informar al usuario. */
TaskHandle_t informarUsuario_task_handle = NULL;

/*==================[internal functions declaration]=========================*/
/**
 * @brief Realiza los cálculos principales del sistema.
 *
 * Obtiene los datos del acelerómetro y del sensor
 * de caudal para calcular la velocidad de avance,
 * el caudal y la dosis aplicada.
 *
 * @param pvParameter Parámetro no utilizado.
 */
static void realizarCalculos(void *pvParameter);
/**
 * @brief Actualiza la información mostrada al usuario.
 *
 * Envía los datos al monitor serie y al Bluetooth,
 * además de activar las alertas visuales mediante LEDs.
 *
 * @param pvParameter Parámetro no utilizado.
 */
static void informarUsuario(void *pvParameter);
/**
 * @brief Despierta la tarea de cálculos.
 *
 * Es ejecutada por el Timer A.
 *
 * @param args Parámetro no utilizado.
 */
void atenderTimerCalculos(void *args);
/**
 * @brief Despierta la tarea de comunicación.
 *
 * Es ejecutada por el Timer B.
 *
 * @param args Parámetro no utilizado.
 */
void atenderTimerComunic(void *args);
/**
 * @brief Inicia o pausa el sistema.
 *
 * Cambia el estado de funcionamiento cada vez
 * que se presiona el pulsador.
 *
 * @param args Parámetro no utilizado.
 */
static void botonInicioPausa(void *args);
/**
 * @brief Cuenta los pulsos del sensor de caudal.
 *
 * Incrementa el contador cuando el sistema
 * está realizando mediciones.
 *
 * @param args Parámetro no utilizado.
 */
static void interrupcionCaudal(void *args);

/*==================[internal functions definition]=========================*/

static void interrupcionCaudal(void *args)
{  
	/* Verifica que el sistema se encuentre realizando mediciones antes de contabilizar los pulsos. */
	if (midiendo == true)
	{
		/* Incrementa el contador de pulsos detectados por el sensor de caudal. */
		pulsos_caudal++;
	}
}

void atenderTimerCalculos(void *args)
{
	vTaskNotifyGiveFromISR(realizarCalculos_task_handle, pdFALSE);
}

void atenderTimerComunic(void *args)
{
	vTaskNotifyGiveFromISR(informarUsuario_task_handle, pdFALSE);
}

static void botonInicioPausa(void *args)
{  
	/* Alterna el estado de funcionamiento del sistema. */
	midiendo = !midiendo;
}

static void realizarCalculos(void *pvParameter)
{
	/* Variables utilizadas para almacenar los datos del acelerómetro
	y realizar los cálculos de tiempo. */
	uint8_t datos[6];
	uint32_t tiempo_actual = 0;
	uint32_t tiempo_anterior = 0;
	float dt = 0.0f;

	/* Parámetros físicos fijos utilizados para estimar la longitud
	del paso y el ancho de trabajo de la pulverización. */
	float altura_usuario = 1.60f;
	float L = altura_usuario * 0.415f; // Longitud estimada del paso (~0.664 metros)
	float ancho_trabajo = 2.0f;

	/*Variables utilizadas para el cálculo del caudal*/
	uint32_t tiempo_ultimo_calculo_caudal = 0;
	uint32_t tiempo_ahora = 0;

	/* La tarea permanece ejecutándose continuamente. */
	while (1)
	{
		/* Espera la notificación enviada por el Timer A para comenzar un nuevo ciclo de cálculos. */
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		/* Los cálculos solamente se realizan si el sistema se encuentra en funcionamiento. */
		if (midiendo == true)
		{
			/* ==================== VELOCIDAD ==================== */

			/* Se leen los datos del acelerómetro MPU6050 para estimar la velocidad de desplazamiento. */
			if (I2C_readBytes(MPU_ADDR, REG_ACCEL, 6, datos, 1000) > 0)
			{
				int16_t x = (datos[0] << 8) | datos[1];
				int16_t y = (datos[2] << 8) | datos[3];
				int16_t z = (datos[4] << 8) | datos[5];

				float ax = x / 16384.0f;
				float ay = y / 16384.0f;
				float az = z / 16384.0f;

				/* Se calcula la magnitud de la aceleración para detectar los pasos del operario. */
				float magnitud = sqrtf((ax * ax) + (ay * ay) + (az * az));
				tiempo_actual = pdTICKS_TO_MS(xTaskGetTickCount());

				/* Si la aceleración supera el umbral y pasó el tiempo suficiente, se detecta un paso. */
				if (magnitud > 1.25f)
				{
					if ((tiempo_actual - tiempo_anterior) > 400)
					{
						pasos_totales++;
						dt = (float)(tiempo_actual - tiempo_anterior) / 1000.0f;
						/* La velocidad se calcula utilizando la longitud del paso y el tiempo transcurrido entre dos pasos. */
						if (tiempo_anterior != 0 && dt > 0.1f)
						{
							float vel_calculada = (float)L / dt;
							if (vel_calculada < 3.0f)
							{
								velocidad_actual = (velocidad_actual * 0.4f) + (vel_calculada * 0.6f);
							}
						}
						tiempo_anterior = tiempo_actual;
					}
				}
			}
			/* Si no se detecta movimiento durante un tiempo determinado, la velocidad se reduce
			   progresivamente hasta llegar a cero. */
			if ((pdTICKS_TO_MS(xTaskGetTickCount()) - tiempo_anterior) > 1500)
			{
				velocidad_actual = velocidad_actual * 0.5f;
				if (velocidad_actual < 0.05f)
					velocidad_actual = 0.0f;
			}

			/* ==================== CAUDAL ==================== */

			/* Se calcula el tiempo transcurrido desde la última actualización del caudal. */
			tiempo_ahora = pdTICKS_TO_MS(xTaskGetTickCount());
			uint32_t delta_tiempo_caudal = tiempo_ahora - tiempo_ultimo_calculo_caudal;

			/* Cada 200 ms se calcula el caudal a partir de los pulsos recibidos por el sensor. */
			if (delta_tiempo_caudal >= 200)
			{
				/* Se protege la variable compartida para evitar conflictos con la interrupción. */
				portENTER_CRITICAL(&caudalMux);
				uint32_t copia_pulsos = pulsos_caudal;
				pulsos_caudal = 0;
				portEXIT_CRITICAL(&caudalMux);

				float frecuencia_hz = ((float)copia_pulsos / (float)delta_tiempo_caudal) * 1000.0f;

				/* Se calcula la frecuencia de pulsos y posteriormente el caudal en L/min. */
				if (frecuencia_hz > 0.5f)
				{
					caudal_actual = frecuencia_hz / 7.5f;
				}
				else
				{
					caudal_actual = 0.0f;
				}

				tiempo_ultimo_calculo_caudal = tiempo_ahora;
			}

			/* ==================== DOSIS ==================== */

			/* Se calcula la dosis real aplicada en L/ha a partir del caudal, la velocidad y el ancho de trabajo. */
			if (velocidad_actual > 0.1f && caudal_actual > 0.05f)
			{
				dosis_real = (float)(caudal_actual * 600.0f) / (float)(velocidad_actual * ancho_trabajo);
			}
			else
			{
				/* Si no existe movimiento o caudal, la dosis se considera nula. */
				dosis_real = 0.0f;
			}
		}
	}
}

static void informarUsuario(void *pvParameter)
{
	/* Valores de referencia utilizados para evaluar si la dosis aplicada es correcta. */
	float dosis_objetivo = 150.0f;
	float tolerancia = 0.10f;

	/* Buffers utilizados para construir el mensaje que será enviado mediante Bluetooth. */
	char msg[128];
	char msg_chunk[32];

	/* La tarea permanece ejecutándose continuamente. */
	while (1)
	{
		/* Espera la notificación enviada por el Timer B para comenzar un nuevo ciclo de actualización. */
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		/* Se apagan todos los LEDs para actualizar correctamente el estado del sistema. */
		LedsOffAll();

		/* La información sólo se actualiza cuando el sistema se encuentra en funcionamiento. */
		if (midiendo == true)
		{
			/* ==================== MONITOR SERIE ==================== */

			/* Se envían los valores calculados al monitor serie para visualizar el estado actual. */
			UartSendString(UART_PC, "Pasos: ");
			UartSendString(UART_PC, (char *)UartItoa(pasos_totales, 10));
			UartSendString(UART_PC, " | Vel: ");
			UartSendString(UART_PC, (char *)UartItoa((uint16_t)(velocidad_actual * 100), 10));
			UartSendString(UART_PC, " cm/s | Caudal: ");
			UartSendString(UART_PC, (char *)UartItoa((uint16_t)(caudal_actual * 10), 10));
			UartSendString(UART_PC, " dL/min | Dosis: ");
			UartSendString(UART_PC, (char *)UartItoa((uint16_t)dosis_real, 10));
			UartSendString(UART_PC, " L/ha -> ");

			/* ==================== BLUETOOTH BLE ==================== */

			/* Se limpia el buffer antes de construir un nuevo mensaje. */
			strcpy(msg, "");

			/* Se agregan los valores de velocidad,caudal y dosis al mensaje que será enviado por Bluetooth. */
			sprintf(msg_chunk, "Vel: %.2f m/s | ", velocidad_actual);
			strcat(msg, msg_chunk);

			sprintf(msg_chunk, "Caudal: %.2f L/min | ", caudal_actual);
			strcat(msg, msg_chunk);

			sprintf(msg_chunk, "Dosis: %.2f L/ha -> ", dosis_real);
			strcat(msg, msg_chunk);

			/* ==================== EVALUACIÓN DE LA DOSIS ==================== */

			/* Se compara la dosis real con la dosis objetivo para determinar el estado actual de la aplicación. */
			if (dosis_real == 0.0f)
			{ /* El sistema está detenido o no se está realizando ninguna aplicación. */
				UartSendString(UART_PC, "SISTEMA DETENIDO / SIN APLICACION\r\n");
				strcat(msg, "SISTEMA DETENIDO / SIN APLICACION\r\n");
			}
			else if (dosis_real < (dosis_objetivo * (1.0f - tolerancia)))
			{ /* La dosis aplicada es inferior a la dosis objetivo. Se indica una situación de subaplicación. */
				UartSendString(UART_PC, "ALERTA: SUBAPLICACION\r\n");
				strcat(msg, "ALERTA: SUBAPLICACION\r\n");
				/* Se enciende el LED amarillo. */
				LedOn(LED_2);
			}
			else if (dosis_real > (dosis_objetivo * (1.0f + tolerancia)))
			{ /* La dosis aplicada es superior a la dosis objetivo. Se indica una situación de sobreaplicación. */
				UartSendString(UART_PC, "ALERTA: SOBREAPLICACION\r\n");
				strcat(msg, "ALERTA: SOBREAPLICACION\r\n");
				/* Se enciende el LED rojo. */
				LedOn(LED_3);
			}
			else
			{ /* La dosis se encuentra dentro del rango */
				UartSendString(UART_PC, "ESTADO: DOSIS OPTIMA\r\n");
				strcat(msg, "ESTADO: DOSIS OPTIMA\r\n");
				/* Se enciende el LED verde. */
				LedOn(LED_1);
			}
			/* Se envía el mensaje completo por BLE. */
			BleSendString(msg);
		}
		else
		{
			/* Si el sistema está pausado, se informa el estado tanto por monitor serie como por Bluetooth. */
			UartSendString(UART_PC, "SISTEMA EN PAUSA\r\n");
			BleSendString("SISTEMA EN PAUSA\r\n");
		}
	}
}

/*==================[external functions definition]==========================*/
/**
 * @brief Función principal del programa.
 *
 * Inicializa todos los periféricos utilizados
 * en el sistema y configura su funcionamiento.
 *
 * Se encarga de:
 * - Inicializar LEDs y pulsadores.
 * - Configurar la comunicación I2C.
 * - Inicializar el acelerómetro MPU6050.
 * - Configurar el sensor de caudal.
 * - Inicializar la UART.
 * - Inicializar Bluetooth BLE.
 * - Crear las tareas del sistema.
 * - Configurar los temporizadores.
 */
void app_main(void)
{
	/* Inicialización de periféricos */
	LedsInit();
	SwitchesInit();
	I2C_initialize(400000);

	// Despertamos el acelerómetro real
	I2C_writeByte(MPU_ADDR, REG_PWR, 0x00);

	/* Configuración del sensor de caudal. */
	GPIOInit(GPIO_CAUDAL, GPIO_INPUT);
	// Le asignamos una interrupción por flanco de subida (cuando pasa de 0 a 1)
	GPIOActivInt(GPIO_CAUDAL, interrupcionCaudal, true, NULL);

	/*Configuración de la UART (Monitor Serie) */
	serial_config_t uart_pc = {
		.port = UART_PC,
		.baud_rate = 115200,
		.func_p = NULL,
		.param_p = NULL};
	UartInit(&uart_pc);

	/* Inicialización del Bluetooth */
	ble_config_t config_bt = {
		.device_name = "Pulverizador_ESP32", // Nombre con el que va a aparecer en el celular
		.func_p = NULL};
	BleInit(&config_bt); // Inicializa el stack BLE

	/* Configuración del SWITCH_1 (Pausa/Play del sistema) */
	SwitchActivInt(SWITCH_1, botonInicioPausa, NULL);

	/* Creación de las tareas principales*/
	xTaskCreate(&realizarCalculos, "CalcTask", 4096, NULL, 5, &realizarCalculos_task_handle);
	xTaskCreate(&informarUsuario, "ComTask", 3072, NULL, 4, &informarUsuario_task_handle);

	/* Configuración de Timers */
	timer_config_t timer_calc = {
		.timer = TIMER_A,
		.period = CONFIG_PERIOD_CALCULO_US,
		.func_p = atenderTimerCalculos,
		.param_p = NULL};
	TimerInit(&timer_calc);

	timer_config_t timer_comu = {
		.timer = TIMER_B,
		.period = CONFIG_PERIOD_COMUNIC_US,
		.func_p = atenderTimerComunic,
		.param_p = NULL};
	TimerInit(&timer_comu);

	// Arrancamos los temporizadores
	TimerStart(TIMER_A);
	TimerStart(TIMER_B);
}
/*==================[end of file]============================================*/