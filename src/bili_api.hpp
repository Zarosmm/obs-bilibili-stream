#ifndef BILI_API_H
#define BILI_API_H

#include <stdbool.h>

typedef struct {
	char* room_id;
	char* csrf_token;
	char* cookies;
	char* title;
	bool login_status;
	bool streaming;
	char* rtmp_addr;
	char* rtmp_code;
	int part_id;
	int area_id;
} BiliConfig;

#ifdef __cplusplus
extern "C" {
#endif

	void bili_api_init(void);
	void bili_api_cleanup(void);
	bool bili_get_qrcode(const char* cookies, char** qrcode_data, char** qrcode_key);
	bool bili_qr_login(char** qrcode_key, char** cookies);
	bool bili_check_login_status(const char* cookies);
	bool bili_get_room_id_and_csrf(const char* cookies, char** room_id, char** csrf_token);
	bool bili_start_live(BiliConfig* config, char** rtmp_addr, char** rtmp_code);
	bool bili_stop_live(BiliConfig* config);
	bool bili_update_room_info(BiliConfig* config);

#ifdef __cplusplus
}
#endif

#endif // BILI_API_H