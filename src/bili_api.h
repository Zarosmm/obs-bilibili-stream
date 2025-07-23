#ifndef BILI_API_H
#define BILI_API_H

#include <stdbool.h>

typedef struct {
    const char* room_id;
    const char* csrf_token;
    const char* cookies;
    const char* title;
} BiliConfig;

bool bili_get_qrcode(char** qrcode_data, char** qrcode_key);
bool bili_check_login_status(const char* qrcode_key, char** status_data);
bool bili_start_live(BiliConfig* config, int area_id, char** rtmp_addr, char** rtmp_code);
bool bili_stop_live(BiliConfig* config);
bool bili_update_room_info(BiliConfig* config, int area_id);
void bili_api_init(void);
void bili_api_cleanup(void);

#endif