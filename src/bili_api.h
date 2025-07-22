#ifndef BILI_API_H
#define BILI_API_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

    // Bilibili API 配置结构体
    typedef struct {
        const char* room_id;     // 直播间 ID
        const char* csrf_token;  // CSRF 令牌
        const char* cookies;     // Cookies 字符串
        const char* title;       // 直播标题
    } BiliConfig;

    // 初始化 Bilibili API
    void bili_api_init(void);

    // 清理 Bilibili API
    void bili_api_cleanup(void);

    // 获取登录二维码
    bool bili_get_qrcode(char** qrcode_data);

    // 检查登录状态
    bool bili_check_login_status(char** status_data);

    // 启动直播
    bool bili_start_live(BiliConfig* config, int area_id, char** rtmp_addr, char** rtmp_code);

    // 停止直播
    bool bili_stop_live(BiliConfig* config);

    // 更新直播间信息（例如分区）
    bool bili_update_room_info(BiliConfig* config, int area_id);

#ifdef __cplusplus
}
#endif

#endif // BILI_API_H
