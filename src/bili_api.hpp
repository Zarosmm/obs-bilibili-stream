#ifndef BILI_API_H
#define BILI_API_H

#include <stdbool.h>

typedef struct {
	char* room_id;
	char* csrf_token;
	char* cookies;
	char* title;
} BiliConfig;

#ifdef __cplusplus
extern "C" {
#endif

	void bili_api_init(void);
	void bili_api_cleanup(void);
	bool bili_get_qrcode(const char* cookies, char** qrcode_data, char** qrcode_key);
	bool bili_qr_login(const char* cookies, char** qrcode_key);
	bool bili_check_login_status(const char* input_cookies, char** output_cookies);
	bool bili_start_live(BiliConfig* config, int area_id, char** rtmp_addr, char** rtmp_code);
	bool bili_stop_live(BiliConfig* config);
	bool bili_update_room_info(BiliConfig* config, int area_id);

#ifdef __cplusplus
}
#endif

#endif // BILI_API_H
