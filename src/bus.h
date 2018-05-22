#ifndef LPX_BUS_H
#define LPX_BUS_H

#include <poll.h>
#include <stdint.h>

enum BusState {
    NOT_INITIALIZED,
    UNKNOWN,
    WAIT_TRAIN,
    TRAIN,
    CLOSED
};

int8_t bus_init();

struct pollfd *bus_fds(uint8_t *len);

enum BusState bus_state();

int64_t bus_trainId();

int8_t bus_last_train_wheel_time_offsets();

int8_t bus_handle_events();

void bus_close();

const uint16_t MARKER;

struct TMsbState {                // XX - hex смещение в словах (uint16_t)
    uint16_t marker;                 // 0x00 0xA55A маркер начала структуры данных. (R/O)
    struct {
        uint8_t software;                 // версия прошивки
        uint8_t hardware;                 // версия платы
    };
    uint16_t empty;                  // 0x02 резерв
    uint16_t longWaitTime;           // 0x03 время подавления дребезга сигнала извещения в тугриках 10 мс.
    //      Длительность равна N+1 * 10мс после последнего дребезга. (R/W WF)
    uint16_t dpWaitTime;             // 0x04 время подавления дребезга сигнала датчика поезда в тугриках 10 мс.
    //      Длительность равна N+1 * 10мс после последнего дребезга. (R/W WF)
    uint16_t waitPoezdTime;          // 0x05 время таймаута конца поезда в режиме окончания по отсутствию колес в секундах.
    //      Значение 0 – мгновенная реакция на событие. (R/W WF)
    uint16_t reconnect;              // 0x06 счетчик попыток пересоединения с PC (сбрасывается чтением)
    uint16_t empty1;                 // 0x07 резерв
    uint32_t unixTime;               // 0x08 счетчик секунд. (R/O)
    uint16_t pedalCnrt;              // 0x0a слово управления работой системы педалей. (R/W WF)
    //    D1, D0 – биты управления сигналами педалей
    //      01 - использование сигналов педалей одной правой стороны,
    //      10 - использование сигналов педалей одной левой стороны, в противном случае
    //           используются сигналы педалей обеих сторон
    //    D3, D2 – биты управления режимом начала - завершения поезда.
    //      00 – режим работы по сигналу извещения т.е. поезд начинается и
    //           оканчивается  сигналом извещения.
    //      01 – режим работы по датчику поезда т.е. поезд начинается и оканчивается
    //           сигналом датчика поезда (соответственно учитывается направление).
    //      10 - режим работы по датчику поезда или по извещению, т.е. начало по появлению любого
    //           из этих сигналов, окончание зависит от ситуации, если во время поезда есть сигнал
    //           датчика поезда окончание поезда произойдет по его проподанию, если этого сигнала
    //           так и не появилось, то окончание произойдет по снятию сигнала извещения.
    //      11 – режим работы по датчику поезда и по извещению, т.е. начало по появлению
    //           обоих  этих сигналов, окончание когда снимается один из сигналов
    //    D4 – бит завершения поезда по таймауту последнего колеса, данный бит влияет только
    //         на окончание поезда, начало определяется строго предыдущими битами. Необходимо
    //         учесть, что после закрытия (завершения поезда по таймауту) все равно происходит
    //         ожидание снятия сигнала определенного для начала поезда, т.е. датчика поезда,
    //         извещения, либо их комбинаций.
    //    D5 – бит разрешения противохода, т.е. разрешается формировать поезд при противоходе.
    //         При этом если проходит такой поезд происходит переключение педалей стартовых и
    //         стоповых между собой.
    //    D6 – бит запрета формирования таблицы времен по центральным педалям сигналов центральных
    //         педалей правой стороны (дополнительные счетчики центральных педалей продолжают
    //         считать при любом состоянии этого бита)
    //    D7 – бит запрета формирования таблицы времен по центральным педалям сигналов центральных
    //         педалей левой стороны (дополнительные счетчики центральных педалей продолжают
    //         считать при любом состоянии этого бита)
    //    D8 – бит насильственного завершения поезда не зависимо от режима работы.
    //         Этот бит сбрасывается автоматически т.е. можно считать что он не читается.
    uint16_t wheelLen;               // 0x0b время формирования внутреннего колеса N+1 [мс]. (R/W). Если busCsr == 2, то
    //      формируется wagonCount колес с промежутком wheelInterval, если busCsr == 1, то
    //      формируется wagonCount вагонов со следующей диаграммой:
    //        Ткол = wheelLen; Ттел (время между колесами в тележке)  = Ткол * 2;
    //        Тваг (время между тележками в вагоне)  = Ткол * 10;
    //        Тваг-ваг  (время между вагонами)  = Ткол * 3.
    uint16_t wheelInterval;          // 0x0c время между колесами при формировании колес а не вагонов
    uint16_t wagonCount;             // 0x0d количество формируемых вагонов по 4 оси в каждом (или колес при busCsr == 2). (R/W)
    uint16_t pedalCsr;               // 0x0e слово управления формированием внутренних сигналов и переключением внутренние или
    //      внешние. (R/W)
    //    D0 – бит переключения сигнала извещения на внутренний 1 – внутренний.
    //    D1 – бит переключения сигнала наличия поезда на внутренний 1 – внутренний.
    //    D2 – бит переключения сигнала педалей на внутренний 1 – внутренний.
    //    D3 - n/a
    //    D4 – бит внутреннего сигнала извещения.
    //    D5 – бит внутреннего сигнала наличия поезда.
    //    D6 – бит разрешения формирования сигналов педалей правой стороны при формировании
    //         внутреннего поезда.
    //    D7 – бит разрешения формирования сигналов педалей левой стороны при формировании
    //         внутреннего поезда.
    //    D8 – бит 'противоход' датчика поезда при формировании внутреннего поезда.
    uint16_t busCsr;                 // 0x0f прочие сигналы управления. (R/W)
    //    D0 – бит разрешения формирования внутреннего поезда, автоматически сбрасывается
    //         после формирования заданного количество вагонов.
    //    D1 – бит разрешения формирования внутренних колес, автоматически сбрасывается после
    //          формирования заданного количество колес.
    uint16_t busCsr2;                // 0x10 прочие сигналы. (R/W)
    //    D0 – статусный бит, показывает, что произошло изменение (запись поезда) памяти
    //         поездов после последнего чтения этой памяти. Сбрасывается автоматически после
    //         чтения содержимого памяти поездов.
    //    D7 – бит очистки памяти поездов, т.е. указатель поездов сбрасывается в начало.
    //         Сам бит автоматически сбрасывается после выполнения операции. Содержимое
    //         памяти инициализируется нулями. Т.к. это тоже изменение памяти данных поездов,
    //         то взводиться статусный бит изменения.
    uint16_t busCnt0;                // 0x11 для дальнейшего расширения. (R/W )
    struct {
        uint16_t rightStart;         // 0x12 основной счетчик стартовых педалей правой стороны. Данный счетчик сбрасывается
        //      началом следующего поезда. Счет производится только при открытом поезде.(R/O)
        uint16_t rightStop;          // 0x13 основной счетчик стоповых педалей правой стороны. Данный счетчик сбрасывается
        //      началом следующего поезда. Счет производится только при открытом поезде.(R/O)
        uint16_t rightCentral;       // 0x14 основной счетчик центральных педалей левой стороны.  Данный счетчик сбрасывается
        //      началом следующего поезда. Счет производится только при открытом поезде. (R/O)
        uint16_t leftStart;          // 0x15 основной счетчик стартовых педалей левой стороны.  Данный счетчик сбрасывается
        //      началом следующего поезда. Счет производится только при открытом поезде. (R/O)
        uint16_t leftStop;           // 0x16 основной счетчик стоповых педалей левой стороны. Данный счетчик сбрасывается
        //      началом следующего поезда. Счет производится только при открытом поезде. (R/O)
        uint16_t leftCentral;        // 0x17 основной счетчик центральных педалей левой стороны. Данный счетчик сбрасывается
        //      началом следующего поезда. Счет производится только при открытом поезде. (R/O)
    };
    uint16_t reserv0;                // 0x18 резерв
    uint16_t reserv1;                // 0x19 резерв
    uint32_t openTrain;              // 0x1a адрес текущего открытого поезда (-1 если нет открытого поезда)
    uint32_t lastTrain;              // 0x1c адрес начала последнего запомненного поезда в памяти (R/O)
    uint32_t prevTrain;              // 0x1e адрес начала предыдущего запомненного поезда в памяти (R/O)
    uint16_t status;                 // 0x20 состояние. (R/O)
    //    D3 – D0 – состояние основного автомата формирования поезда.
    //      0x0 – автомат в состоянии сброса, в работе быть не должно.
    //      0x1 – автомат в ожидании прихода сигнала "синхронизации" поезда
    //            (извещение, датчик поезда, или их комбинация.).
    //      0x2 – автомат в ожидании прихода сигнала первого колеса с педалей. Если до прихода
    //            сигнала с педалей происходит снятие сигнала "синхронизации" поезда, ни какая
    //            информация о поезде не записывается и автомат переходит опять в состояние 0х1.
    //            В случае получения сигнала с педалей автомат переходит в состояние либо
    //            0х3 (окончание по сигналу "синхронизации" поезда), либо
    //            0х4 (окончание по таймауту после последнего колеса).
    //      0x3 – автомат в режиме окончания поезда по сигналу "синхронизации" поезда и ждет
    //            его снятия.
    //      0x4 – автомат в режиме окончания поезда по таймауту последнего колеса и ждет
    //            наступления этого таймаута.
    //      0x5 – автомат в состоянии ожидания снятия сигнала "синхронизации" поезда для
    //            перехода в состояние ожидания его появления снова (0х1).
    //    D4 – входной сигнал извещения.
    //    D5 – выходной сигнал поезда на датчики.
    //    D6 – входной сигнал наличия поезда.
    //    D7 - выходной сигнал наличия поезда (после подавления дребезга).
    //    D8 – входной сигнал 'противоход' датчика поезда
    //    D9 - выходной сигнал 'противоход' датчика поезда (после подавления дребезга).
    //    D10 - выходной сигнал извещения (после подавления дребезга).
    uint16_t emptyState;               // 0x21 для дальнейшего расширения (R/O).
    uint32_t trFirstSignalTime;        // 0x22 время прихода первого из сигналов нотификации по последнему поезду в секундах. (R/O).
    //      Первым может быть любой из ниже перечисленных сигналов. По приходу первого сигнала
    //      следующие шесть регистров времен обнуляются, а в соответствующий регистр времени
    //      (в микросекундах) записывается 1 и разрешается счетчик микросекунд. Дальше приход остальных
    //      сигналов (если они есть) фиксируется в своих регистрах в микросекундах от первого сигнала.
    uint32_t trNotifInTime;            // 0x24 время прихода входного сигнала извещения в микросекундах от trFirstSignalTime. (R/O)
    uint32_t trNotifOutTime;           // 0x26 время формирования выходного сигнала извещения после антидребезга в микросекундах от
    //      trFirstSignalTime. (R/O)
    uint32_t trDPInTime;               // 0x28 время прихода входного сигнала датчика поезда в микросекундах от trFirstSignalTime.(R/O)
    uint32_t trDPOutTime;              // 0x2a время формирования выходного сигнала датчика поезда после антидребезга в микросекундах от
    //      trFirstSignalTime. (R/O)
    uint32_t trTrainToSenTime;         // 0x2c время выдачи сигнала поезд на датчики в микросекундах от trFirstSignalTime (R/O)
    uint32_t trFirstWheelTime;         // 0x2e время формирования сигнала первого колеса  в микросекундах от trFirstSignalTime (R/O)
    struct {
        uint16_t rightStartAlt;      // 0x30 дополнительный счетчик стартовых педалей правой стороны. Данный счетчик
        //      сбрасывается началом следующего поезда. Счет производится не зависимо от
        //      открытия поезда и всегда только по своей стороне. (R/O)
        uint16_t rightStopAlt;       // 0x31 дополнительный счетчик стоповых педалей правой стороны. Данный счетчик
        //      сбрасывается началом следующего поезда. Счет производится не зависимо от
        //      открытия поезда и всегда только по своей стороне. (R/O)
        uint16_t rightCentralAlt;    // 0x32 дополнительный счетчик центральных педалей правой стороны. Данный счетчик
        //      сбрасывается началом следующего поезда. Счет производится не зависимо от
        //      открытия поезда и всегда только по своей стороне. (R/O)
        uint16_t leftStartAlt;       // 0x33 дополнительный счетчик стартовых педалей левой стороны. Данный счетчик
        //      сбрасывается началом следующего поезда. Счет производится не зависимо от
        //      открытия поезда и всегда только по своей стороне. (R/O)
        uint16_t leftStopAlt;        // 0x34 дополнительный счетчик стоповых педалей левой стороны. Данный счетчик
        //      сбрасывается началом следующего поезда. Счет производится не зависимо от
        //      открытия поезда и всегда только по своей стороне. (R/O)
        uint16_t leftCentralAlt;     // 0x35 дополнительный счетчик центральных педалей левой стороны. Данный счетчик
        //      сбрасывается началом следующего поезда. Счет производится не зависимо от
        //      открытия поезда и всегда только по своей стороне. (R/O)
    };
    uint16_t reserv2;                // 0x36 резерв
    uint16_t reserv3;                // 0x37 резерв
    uint16_t reserv4;                // 0x38 резерв
    uint16_t reserv5;                // 0x39 резерв
    // всего 116 байт
};

struct TMsbTrainHdr {             // XX - смещение в двойных словах (uint32_t)
    uint32_t marker;                 // 00 0xA5A5 маркер начала данных (прописывается при открытии поезда)
    uint32_t syncMode;               // 01 режимы синхронизации и направление с датчика поезда (прописывается при открытии поезда)
    //    D1, D0 – биты управления сигналами педалей
    //      01 - использование сигналов педалей одной правой стороны,
    //      10 - использование сигналов педалей одной левой стороны, в противном случае
    //           используются сигналы педалей обеих сторон
    //    D3, D2 – биты управления режимом начала - завершения поезда.
    //      00 – режим работы по сигналу извещения т.е. поезд начинается и
    //           оканчивается  сигналом извещения.
    //      01 – режим работы по датчику поезда т.е. поезд начинается и оканчивается
    //           сигналом датчика поезда (соответственно учитывается направление).
    //      10 - режим работы по датчику поезда или по оповещению, т.е. начало по появлению
    //           любого из этих сигналов, окончание когда нет обоих сигналов.
    //      11 – режим работы по датчику поезда и по оповещению, т.е. начало по появлению
    //           обоих  этих сигналов, окончание когда снимается один из сигналов
    //    D4 – бит завершения поезда по таймауту последнего колеса, данный бит влияет только
    //         на окончание поезда, начало определяется строго предыдущими битами. Необходимо
    //         учесть, что после закрытия (завершения поезда по таймауту) все равно происходит
    //         ожидание снятия сигнала определенного для начала поезда, т.е. датчика поезда,
    //         извещения, либо их комбинаций.
    //    D15 - 'противоход' с датчика поезда
    //    D16 -  бит переключения сигнала извещения на внутренний 1 – внутренний.
    //    D17 –  бит переключения сигнала наличия поезда на внутренний 1 – внутренний.
    //    D18 –  бит переключения сигнала педалей на внутренний 1 – внутренний.
    //    D19 -  бит формирования внутреннего поезда.
    //    D20 -  бит формирования внутренних колес.
    uint32_t timeStart;              // 02 время прихода сигнала извещения (прописывается при открытии поезда)
    uint32_t timeFirstWheel;         // 03 время первого колеса (прописывается при открытии поезда)
    struct {
        uint32_t leftStart;          // 04 счетчик левых стартовых педалей
        uint32_t leftCentral;        // 05 счетчик левых центральных педалей
        uint32_t leftStop;           // 06 счетчик левых стоповых педалей
        uint32_t rightStart;         // 07 счетчик правых стартовых педалей
        uint32_t rightCentral;       // 08 счетчик правых центральных педалей
        uint32_t rightStop;          // 09 счетчик правых стоповых педалей
    };
    uint32_t timeStop;               // 10 время закрытия поезда
    uint32_t nextTrain;              // 11 адрес следующего поезда
    uint32_t prevTrain;              // 12 адрес предыдущего поезда
    uint32_t thisTrainData;          // 13 адрес начала данных по временам колес текущего состава
    uint32_t numberOfWheels;         // 14 количество блоков данных колес (MsbWheelTime) в текущем составе
    uint32_t trFirstSignalTime;      // 15 время первого сигнала (какой первый придет)
    uint32_t trNotifInTime;          // 16 время в микросекундах от первого сигнала до входного сигнала извещения
    uint32_t trNotifOutTime;         // 17 время в микросекундах от первого сигнала до выходного сигнала извещения (после антидребезга)
    uint32_t trDPInTime;             // 18 время в микросекундах от первого сигнала до входного сигнала датчика поезда
    uint32_t trDPOutTime;            // 19 время в микросекундах от первого сигнала до выходного сигнала датчика поезда
    uint32_t trTrainToSenTime;       // 20 время в микросекундах от первого сигнала до формирования сигнала “поезд” на датчики
    uint32_t trFirstWheelTime;       // 21 время в микросекундах от первого сигнала до сигнала первого колеса (любой из педалей)
    union {                         // 22 параметры режима калибровки (прописываются извне калибровочной программой)
        uint32_t ifCalibrate;
        struct {
            uint32_t numberOfMeasurements:7;   // количество измерений в серии
            uint32_t ifBinding:1;              // запрошен режим привязки
            uint32_t quantityOfSeries:5;       // количество серий
            uint32_t step:6;                   // шаг с которым выполняется калибровка
            uint32_t ifCancelling:1;           // отмена калибровки
            uint32_t side:1;                   // сторона по которой выполняется калибровка (0 - левая, 1 - правая)
        };
    };
    uint32_t empty;                  // 23 для дальнейшего расширения
    // всего 96 байт
};

struct TMsbWheelTime {
    uint32_t leftStart;          // 01 время левого старта
    uint32_t leftCentral;        // 02 время левого центра
    uint32_t leftStop;           // 03 время левого стопа
    uint32_t rightStart;         // 04 время правого старта
    uint32_t rightCentral;       // 05 время правого центра
    uint32_t rightStop;          // 06 время правого стопа
};

#endif //LPX_BUS_H
