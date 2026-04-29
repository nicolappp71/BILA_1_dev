#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

#define GPIO_INPUT_PIN 5

/**
 * @brief Inizializza GPIO e task per monitoraggio
 */
void gpio_init(void);

#ifdef __cplusplus
}
#endif

#endif // GPIO_MANAGER_H