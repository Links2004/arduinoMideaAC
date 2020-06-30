#ifndef _AC_control_H_
#define _AC_control_H_

#include <Arduino.h>

//#define AC_DEBUG_PORT DEBUG_ESP_PORT
//#define AC_DEBUG_RAW

#ifndef AC_DEBUG_PORT
#ifdef DEBUG_ESP_PORT
#define AC_DEBUG_PORT DEBUG_ESP_PORT
#else
#define AC_DEBUG_PORT Serial1
#endif
#endif

typedef enum {
    acFAN1 = 0x28,
    acFAN2 = 0x3C,
    acFAN3 = 0x50,
    acFANA = 0x66,
    // 65 ??
} ac_fan_speed_t;

typedef enum {
    acModeAuto = 0b001,    // 1
    acModeCool = 0b010,    // 2
    acModeDry  = 0b011,    // 3
    acModeHeat = 0b100,    // 4
    acModeFan  = 0b101,    // 5
} ac_mode_t;

typedef enum {
    acLamelleOff = 0b0000,
    acLamelleH   = 0b0011,
    acLamelleV   = 0b1100,
    acLamelleHV  = 0b1111,
} ac_lamelle_t;

typedef struct {
    bool on;
    bool turbo;
    bool eco;
    ac_mode_t mode;
    ac_lamelle_t lamelle;
    ac_fan_speed_t fan;
    uint8_t soll;
} ac_conf_t;

typedef struct {
    ac_conf_t conf;
    double ist;
    double aussen;
} ac_status_t;

class acSerial {
  public:
#ifdef __AVR__
    typedef void (*acSerialEvent)(ac_status_t * status);
#else
    typedef std::function<void(ac_status_t * status)> acSerialEvent;
#endif

    void onStatusEvent(acSerialEvent cbEvent);

    void begin(Stream * s, const char * name);
    void loop();
    bool write(uint8_t target, uint8_t * data, uint8_t len);
    void send_getSN();
    void send_status(bool wifi, bool app);
    void request_status();
    void send_conf(ac_conf_t * conf);
    void send_conf_h(bool on, uint8_t soll, uint8_t fan, ac_mode_t mode, bool lamelle = false, bool turbo = false, bool eco = false);

    void print_conf(ac_conf_t * conf);
    void print_status(ac_status_t * status);

  protected:
    Stream * _s        = NULL;
    const char * _name = NULL;
    uint8_t * _buf     = NULL;
    uint8_t _needBytes = 0;
    acSerialEvent _cbEvent;

    void handleMessage();
    uint8_t calcCheckSum(uint8_t * data, uint8_t len);
    bool write_raw(uint8_t * data, uint8_t len);
};

typedef struct {
    uint8_t header;
    uint8_t len;
    uint8_t targed;
    uint8_t data[0];
} __attribute__((packed)) ac_message_t;

#endif /* _AC_control_H_ */
