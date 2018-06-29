#ifndef LPX_STREAM_H
#define LPX_STREAM_H

/**
 * Ошибка генерации потока архива стрима
 */
#define STRM_IO -2

/**
 * Структура записи в индексе потока
 */
typedef struct FrameMeta {
    int64_t start_time; // ситемное (астрономическое) время запроса фрейма в микросекундах
    int64_t end_time; // систмное (астрономическое) время получения фрейма в микросекундах
} FrameMeta;

/**
 * Поток байт фреймов видео-потока.
 * BNF формата потока:
 * поток ::= <eof> | <фрейм>
 * eof ::= <0 байт>
 * фрейм :: = <имя файла><размер файла><n байт файла>
 * имя файла ::= строка в кодировке ascii с завершающим нулём
 * размер файла ::= 64-битное число (little endian)
 * n байт файла ::= массив байт длинной n
 */
typedef struct VideoStreamBytesStream VideoStreamBytesStream;

/**
 * Поиск индекса фрейма ближайшего к заданному time
 * index - индекс фреймов
 * index - длина индекса фреймов
 * time - время в микросекундах относительно момента начала стриминга (времени запроса первого фрейма)
 */
ssize_t stream_find_frame(FrameMeta **index, size_t index_len, uint64_t time_offset);

/**
 * Инициализирует структура архива потока, содержащего заданные файлы. В случае ошибки возвращает NULL.
 */
VideoStreamBytesStream *stream_open(char **files, size_t files_size);

/**
 * Записывает до `max` байт архива в буффер. Возвращает количество реально записанных байт, EOF в случае
 * когда стрим был целиком прочитан и STRM_IO в случае ошибок генерации архива стрима
 */
ssize_t stream_read(VideoStreamBytesStream *stream, uint8_t *buf, size_t max);

/**
 * Закрывет архив и освобождает все ресурсы
 */
void stream_close(VideoStreamBytesStream *stream);

#endif //LPX_STREAM_H
