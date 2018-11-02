#ifndef LPX_BUS2_H
#define LPX_BUS2_H

/**
 * Интерфейс модулей сенсора поезда - устройства, которое отслеживает прохождение прохождение поезда мимо комплекса
 * и вызывает переданный коллбэк в момент обнаружения подъезда и отъезда поезда
 */
#include <stdint.h>

// Статусы прерываний
#define TS_INT_TRAIN_LEAVE 0
#define TS_INT_TRAIN_IN    1

// Коды ошибок
#define TS_THREAD   2 // ошибки управления потоком
#define TS_IFACE    3 // ошибки передачи данных по интерфейсу

typedef struct TrainSensor TrainSensor;

/**
 * user_data - указатель на данные, переданные в bus_init
 * status - статус сенсора, 0 - ок, не 0 - не ок
 * train_status - статус поезда, 0 - поезда нет, 1 - поезд проходит
 */
typedef void (*train_sensor_callback)(void *user_data, int8_t status, int8_t train_status);

int32_t train_sensor_init(TrainSensor **train_sensor, void *user_data, train_sensor_callback tscb);

void train_sensor_close(TrainSensor *train_sensor);

#endif //LPX_BUS_H
