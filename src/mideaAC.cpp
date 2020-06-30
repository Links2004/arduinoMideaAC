#include "mideaAC.h"

void acSerial::onStatusEvent(acSerialEvent cbEvent) {
    _cbEvent = cbEvent;
}

void acSerial::begin(Stream * s, const char * name) {
    if(!s) return;
    _s    = s;
    _name = name;
    _s->setTimeout(1000);
    _needBytes = 0;
    _cbEvent   = nullptr;

    loop();

    uint8_t reset[] = { 0x00, 0x07, 0x00 };
    write(0xFF, &reset[0], sizeof(reset));
    write(0xFF, &reset[0], sizeof(reset));
    write(0xFF, &reset[0], sizeof(reset));
}

void acSerial::send_getSN() {
    uint8_t data[] = { 0x01, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x5e };
    write(0xFF, &data[0], sizeof(data));
}

void acSerial::send_status(bool wifi, bool app) {
    uint8_t data[] = { 0xAC, 0xB2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x01, 0x01, 0x02, 0x0A, 0x32, 0x0B, 0x0A, 0x00, (wifi ? 0x00 : 0x02), 0x00, (app ? 0x01 : 0x00), (app ? 0x01 : 0x00), 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    write_raw(&data[0], sizeof(data));
}

void acSerial::request_status() {
    uint8_t data[] = { 0x00, 0x03, 0x41, 0x81, 0x00, 0xFF, 0x03, 0xFF, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xCD };
    write(0xAC, &data[0], sizeof(data));
}

void acSerial::print_conf(ac_conf_t * conf) {
#ifdef AC_DEBUG_PORT
    AC_DEBUG_PORT.printf("on: %d\n", conf->on);
    AC_DEBUG_PORT.printf("soll: %d\n", conf->soll);
    AC_DEBUG_PORT.printf("fan: %d\n", conf->fan);
    AC_DEBUG_PORT.printf("mode: %d\n", conf->mode);
    AC_DEBUG_PORT.printf("lamelle: %d\n", conf->lamelle);
    AC_DEBUG_PORT.printf("turbo: %d\n", conf->turbo);
    AC_DEBUG_PORT.printf("eco: %d\n", conf->eco);
#endif
}

void acSerial::print_status(ac_status_t * status) {
#ifdef AC_DEBUG_PORT
    print_conf(&(status->conf));
    AC_DEBUG_PORT.print("ist: ");
    AC_DEBUG_PORT.println(status->ist);
    AC_DEBUG_PORT.print("aussen: ");
    AC_DEBUG_PORT.println(status->aussen);
#endif
}

void acSerial::send_conf_h(bool on, uint8_t soll, uint8_t fan, ac_mode_t mode, bool lamelle, bool turbo, bool eco) {
#ifdef AC_DEBUG_PORT
    AC_DEBUG_PORT.printf("on: %d\n", on);
    if(on) {
        AC_DEBUG_PORT.printf("soll: %d\n", soll);
        AC_DEBUG_PORT.printf("fan: %d\n", fan);
        AC_DEBUG_PORT.printf("mode: %d\n", mode);
        AC_DEBUG_PORT.printf("lamelle: %d\n", lamelle);
        AC_DEBUG_PORT.printf("turbo: %d\n", turbo);
        AC_DEBUG_PORT.printf("eco: %d\n", eco);
    }
#endif
    ac_conf_t conf;

    switch(fan) {
        case 1:
            conf.fan = acFAN1;
            break;
        case 2:
            conf.fan = acFAN2;
            break;
        case 3:
            conf.fan = acFAN3;
            break;
        default:
        case 0:
            conf.fan = acFANA;
            break;
    }

    conf.soll    = soll;
    conf.mode    = mode;
    conf.lamelle = (lamelle ? acLamelleHV : acLamelleOff);
    conf.on      = on;
    conf.turbo   = turbo;
    conf.eco     = eco;
    send_conf(&conf);
}

void acSerial::send_conf(ac_conf_t * conf) {
    //                   0     1     2     3     4     5     6     7     8     9    10    11    12    13    14    15    16    17    18    19    20    21    22    23    24    25    26
    uint8_t data[] = { 0x03, 0x02, 0x40, 0x42, 0x00, 0x00, 0x7F, 0x7F, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0x00, 0x04, 0xC0 };
    data[3]        = 0x42 | (conf->on & 0b1);
    data[4]        = (conf->soll & 0x0F) | ((conf->mode & 0x7) << 5);
    data[5]        = (conf->fan);
    data[9]        = 0x30 | (conf->lamelle & 0x0F);

    if(conf->turbo) {
        data[10] = 0x20;
        data[12] = 0x02;
    }

    if(conf->eco) {
        data[11] = 80;
    }

    write(0xAC, &data[0], sizeof(data));
}

void acSerial::handleMessage() {
    ac_message_t * msg = (ac_message_t *)&_buf[0];
    if(msg->header != 0xAA) {
        return;
    }

    if(msg->len == 0x22 && msg->targed == 0xAC) {
        if(msg->data[5] == 0x03 && msg->data[6] == 0x03 && msg->data[7] == 0xC0) {
            // request_status answer
            uint8_t * payload = &(msg->data[8]);
            ac_status_t status;
            status.conf.on      = payload[0] & 0x01;
            status.conf.soll    = (payload[1] & 0x0F) + 16;
            status.conf.mode    = (ac_mode_t)((payload[1] >> 5) & 0x07);
            status.conf.fan     = (ac_fan_speed_t)payload[2];
            status.conf.lamelle = (ac_lamelle_t)(payload[6] & 0x0F);
            status.conf.turbo   = (ac_lamelle_t)(payload[9] == 0x02);
            status.conf.eco     = (ac_lamelle_t)(payload[8] == 0x10);
            status.ist          = ((double)payload[10] * 0.5) - 25;
            status.aussen       = ((double)payload[11] * 0.5) - 25;

            if(_cbEvent) {
                _cbEvent(&status);
            } else {
                print_status(&status);
            }
        }
        return;
    }
}

bool acSerial::write(uint8_t target, uint8_t * data, uint8_t len) {
    if(!_s) return false;
    if(len <= 0 || len > 254) return false;

    uint8_t raw_len   = 6;
    uint8_t raw[0xFF] = { 0x00 };

    raw[0] = target;

    raw_len += len;
    memcpy(&raw[6], data, len);
    return write_raw(raw, raw_len);
}

bool acSerial::write_raw(uint8_t * data, uint8_t len) {
    if(!_s) return false;
    if(!data) return false;
    if(len <= 0 || len > 254) return false;
    // Handle old data
    loop();

    // send data
    _s->write(0xAA);
    _s->write(len + 2);
    _s->write(&data[0], len);
    bool ok = _s->write(calcCheckSum(&data[0], len));
    _s->flush();
    delay(100);
    if(data[0] == 0xAC) {
        unsigned long start = millis();
        while(_s->available() < 3) {
            if(millis() - start >= 1000) {
#ifdef AC_DEBUG_PORT
                AC_DEBUG_PORT.println("no Answer to write\n");
#endif
                return false;
            }
        }
    }
    // Handle new data
    loop();
    return ok;
}

uint8_t acSerial::calcCheckSum(uint8_t * data, uint8_t len) {
    uint8_t checkSum = len + 2;
    for(uint8_t i = 0; i < len; i++) {
        checkSum += data[i];
    }
    checkSum = (uint8_t)(checkSum ^ 0xFF) + 1;
    return checkSum;
}

void acSerial::loop() {
    if(!_s) return;
    if(_needBytes == 0 && _s->available() >= 2) {
        // AC_DEBUG_PORT.print(_name);
        // AC_DEBUG_PORT.print(": ");

        uint8_t header = _s->read();
        if(header == 0xAA) {
            _needBytes = _s->read() - 1;
            _buf       = (uint8_t *)malloc(_needBytes + 2);
            _buf[0]    = header;
            _buf[1]    = _needBytes + 1;
            if(!_buf) {
#ifdef AC_DEBUG_PORT
                AC_DEBUG_PORT.println("mallocFail");
#endif
                _needBytes = 0;
                return;
            }
            //   AC_DEBUG_PORT.println("found start");
        } else {
#ifdef AC_DEBUG_RAW
            AC_DEBUG_PORT.print("wrong Header ");
            AC_DEBUG_PORT.println(header, 16);
#endif
        }
    } else if(_needBytes > 0 && _needBytes >= _s->available()) {
        if(_buf) {
#ifdef AC_DEBUG_RAW
            AC_DEBUG_PORT.print(_name);
            AC_DEBUG_PORT.print(": ");
#endif
            size_t read = _s->readBytes(&_buf[2], _needBytes);
#ifdef AC_DEBUG_RAW
            if(read != _needBytes) {
                AC_DEBUG_PORT.print("short read ");
                AC_DEBUG_PORT.print(read);
                AC_DEBUG_PORT.print(" != ");
                AC_DEBUG_PORT.print(_needBytes);
                AC_DEBUG_PORT.print(" ");
            }
#endif

            uint8_t checkSum = calcCheckSum(&_buf[2], read - 1);
#ifdef AC_DEBUG_RAW
            if(_buf[read + 1] != checkSum) {
                AC_DEBUG_PORT.print("check Sum wrong ");
                AC_DEBUG_PORT.print(_buf[read + 1], 16);
                AC_DEBUG_PORT.print(" != ");
                AC_DEBUG_PORT.print(checkSum, 16);
                AC_DEBUG_PORT.print(" ");
            }

            for(uint8_t i = 0; i < (read + 2); i++) {
                if(_buf[i] < 0x10) {
                    AC_DEBUG_PORT.write('0');
                }
                AC_DEBUG_PORT.print(_buf[i], 16);
                AC_DEBUG_PORT.print(" ");
            }
            AC_DEBUG_PORT.println();
#endif
            if(_buf[read + 1] == checkSum) {
                handleMessage();
            }
            free(_buf);
            _buf = NULL;
        } else {
#ifdef AC_DEBUG_RAW
            AC_DEBUG_PORT.println("missing buf");
#endif
        }
        _needBytes = 0;
#ifdef AC_DEBUG_RAW
        AC_DEBUG_PORT.println();
#endif
    }
}