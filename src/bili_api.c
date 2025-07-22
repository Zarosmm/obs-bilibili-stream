#include <obs-module.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/md5.h>
#include <json-c/json.h>
#include "bili_api.h"
#include "http_client.h"

// 请求头
static const char* default_headers[] = {
    "Accept: application/json, text/plain, */*",
    "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6",
    "Content-Type: application/x-www-form-urlencoded; charset=UTF-8",
    "Origin: https://link.bilibili.com",
    "Referer: https://link.bilibili.com/p/center/index",
    "Sec-Ch-Ua: \"Microsoft Edge\";v=\"129\", \"Not=A?Brand\";v=\"8\", \"Chromium\";v=\"129\"",
    "Sec-Ch-Ua-Mobile: ?0",
    "Sec-Ch-Ua-Platform: \"Windows\"",
    "Sec-Fetch-Dest: empty",
    "Sec-Fetch-Mode: cors",
    "Sec-Fetch-Site: same-site",
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/96.0.4664.110 Safari/537.36",
    NULL
};

// 参数键值对结构体
typedef struct {
    char* key;
    char* value;
} Param;

// appsign 函数：为参数添加签名
static void appsign(Param* params, size_t* param_count, const char* app_key, const char* app_sec) {
    // 添加 appkey
    params[*param_count].key = strdup("appkey");
    params[*param_count].value = strdup(app_key);
    (*param_count)++;

    // 按键排序
    for (size_t i = 0; i < *param_count - 1; i++) {
        for (size_t j = 0; j < *param_count - i - 1; j++) {
            if (strcmp(params[j].key, params[j + 1].key) > 0) {
                Param temp = params[j];
                params[j] = params[j + 1];
                params[j + 1] = temp;
            }
        }
    }

    // 序列化参数为 query 字符串
    char query[1024] = "";
    for (size_t i = 0; i < *param_count; i++) {
        if (i > 0) strncat(query, "&", sizeof(query) - strlen(query) - 1);
        strncat(query, params[i].key, sizeof(query) - strlen(query) - 1);
        strncat(query, "=", sizeof(query) - strlen(query) - 1);
        strncat(query, params[i].value, sizeof(query) - strlen(query) - 1);
    }

    // 计算 MD5 签名
    unsigned char md5_result[MD5_DIGEST_LENGTH];
    char md5_hex[33];
    char query_with_sec[2048];
    snprintf(query_with_sec, sizeof(query_with_sec), "%s%s", query, app_sec);
    MD5((unsigned char*)query_with_sec, strlen(query_with_sec), md5_result);
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        snprintf(&md5_hex[i * 2], 3, "%02x", md5_result[i]);
    }
    md5_hex[32] = '\0';

    // 添加签名到参数
    params[*param_count].key = strdup("sign");
    params[*param_count].value = strdup(md5_hex);
    (*param_count)++;

    // 释放临时内存（除了最后一个 sign）
    for (size_t i = 0; i < *param_count - 1; i++) {
        free(params[i].key);
        free(params[i].value);
    }
}

// 获取当前时间戳
static long get_current_timestamp() {
    HttpResponse* response = http_get_with_headers("https://api.bilibili.com/x/report/click/now", default_headers);
    if (!response || response->status != 200) {
        obs_log(LOG_ERROR, "获取时间戳失败，状态码: %ld", response ? response->status : 0);
        http_response_free(response);
        return 0;
    }

    // 解析 JSON 获取 data.now
    json_object* obj = json_tokener_parse(response->data);
    if (!obj) {
        obs_log(LOG_ERROR, "解析时间戳 JSON 失败");
        http_response_free(response);
        return 0;
    }

    json_object* data;
    if (!json_object_object_get_ex(obj, "data", &data)) {
        obs_log(LOG_ERROR, "无法获取 JSON 中的 'data' 字段");
        json_object_put(obj);
        http_response_free(response);
        return 0;
    }

    json_object* now;
    if (!json_object_object_get_ex(data, "now", &now)) {
        obs_log(LOG_ERROR, "无法获取 JSON 中的 'now' 字段");
        json_object_put(obj);
        http_response_free(response);
        return 0;
    }

    long ts = json_object_get_int64(now);
    json_object_put(obj);
    http_response_free(response);
    return ts;
}

// 初始化 Bilibili API
void bili_api_init(void) {
    http_client_init();
}

// 清理 Bilibili API
void bili_api_cleanup(void) {
    http_client_cleanup();
}

// 获取登录二维码
bool bili_get_qrcode(char** qrcode_data) {
    HttpResponse* response = http_get_with_headers("https://api.bilibili.com/x/web-interface/qrcode", default_headers);
    if (!response || response->status != 200) {
        obs_log(LOG_ERROR, "获取二维码失败，状态码: %ld", response ? response->status : 0);
        http_response_free(response);
        return false;
    }

    // 解析 JSON 获取二维码数据
    json_object* obj = json_tokener_parse(response->data);
    if (!obj) {
        obs_log(LOG_ERROR, "解析二维码 JSON 失败");
        http_response_free(response);
        return false;
    }

    json_object* data;
    if (!json_object_object_get_ex(obj, "data", &data)) {
        obs_log(LOG_ERROR, "无法获取 JSON 中的 'data' 字段");
        json_object_put(obj);
        http_response_free(response);
        return false;
    }

    *qrcode_data = strdup(json_object_get_string(data));
    json_object_put(obj);
    http_response_free(response);
    obs_log(LOG_INFO, "获取二维码成功");
    return true;
}

// 检查登录状态
bool bili_check_login_status(char** status_data) {
    HttpResponse* response = http_get_with_headers("https://api.bilibili.com/x/web-interface/nav", default_headers);
    if (!response || response->status != 200) {
        obs_log(LOG_ERROR, "检查登录状态失败，状态码: %ld", response ? response->status : 0);
        http_response_free(response);
        return false;
    }

    // 解析 JSON 获取登录状态数据
    json_object* obj = json_tokener_parse(response->data);
    if (!obj) {
        obs_log(LOG_ERROR, "解析登录状态 JSON 失败");
        http_response_free(response);
        return false;
    }

    *status_data = strdup(json_object_get_string(obj));
    json_object_put(obj);
    http_response_free(response);
    obs_log(LOG_INFO, "检查登录状态成功");
    return true;
}

// 启动直播
bool bili_start_live(BiliConfig* config, int area_id, char** rtmp_addr, char** rtmp_code) {
    const char* app_key = "aae92bc66f3edfab";
    const char* app_sec = "af125a0d5279fd576c1b4418a3e8276d";

    // 获取直播版本信息
    Param version_params[10];
    size_t param_count = 0;
    version_params[param_count].key = strdup("system_version");
    version_params[param_count].value = strdup("2");
    param_count++;
    long ts = get_current_timestamp();
    if (ts == 0) return false;
    char ts_str[32];
    snprintf(ts_str, sizeof(ts_str), "%ld", ts);
    version_params[param_count].key = strdup("ts");
    version_params[param_count].value = strdup(ts_str);
    param_count++;
    appsign(version_params, &param_count, app_key, app_sec);

    char version_query[1024] = "";
    for (size_t i = 0; i < param_count; i++) {
        if (i > 0) strncat(version_query, "&", sizeof(version_query) - strlen(version_query) - 1);
        strncat(version_query, version_params[i].key, sizeof(version_query) - strlen(version_query) - 1);
        strncat(version_query, "=", sizeof(version_query) - strlen(version_query) - 1);
        strncat(version_query, version_params[i].value, sizeof(version_query) - strlen(version_query) - 1);
        free(version_params[i].key);
        free(version_params[i].value);
    }

    char version_url[2048];
    snprintf(version_url, sizeof(version_url),
             "https://api.live.bilibili.com/xlive/app-blink/v1/liveVersionInfo/getHomePageLiveVersion?%s",
             version_query);

    HttpResponse* version_response = http_get_with_headers(version_url, default_headers);
    if (!version_response || version_response->status != 200) {
        obs_log(LOG_ERROR, "获取直播版本信息失败，状态码: %ld", version_response ? version_response->status : 0);
        http_response_free(version_response);
        return false;
    }

    // 解析 JSON 获取 build 和 curr_version
    json_object* version_obj = json_tokener_parse(version_response->data);
    if (!version_obj) {
        obs_log(LOG_ERROR, "解析直播版本 JSON 失败");
        http_response_free(version_response);
        return false;
    }

    json_object* version_data;
    if (!json_object_object_get_ex(version_obj, "data", &version_data)) {
        obs_log(LOG_ERROR, "无法获取 JSON 中的 'data' 字段");
        json_object_put(version_obj);
        http_response_free(version_response);
        return false;
    }

    json_object* build_obj;
    long build = 1234; // 默认值
    if (json_object_object_get_ex(version_data, "build", &build_obj)) {
        build = json_object_get_int64(build_obj);
    }

    json_object* curr_version_obj;
    const char* curr_version = "1.0.0"; // 默认值
    if (json_object_object_get_ex(version_data, "curr_version", &curr_version_obj)) {
        curr_version = json_object_get_string(curr_version_obj);
    }
    json_object_put(version_obj);
    http_response_free(version_response);

    // 设置直播标题
    char title_data[512];
    snprintf(title_data, sizeof(title_data),
             "room_id=%s&platform=pc_link&title=%s&csrf_token=%s&csrf=%s",
             config->room_id, config->title, config->csrf_token, config->csrf_token);

    HttpResponse* title_response = http_post_with_headers("https://api.live.bilibili.com/room/v1/Room/update", title_data, default_headers);
    if (!title_response || title_response->status != 200) {
        obs_log(LOG_ERROR, "设置直播标题失败，状态码: %ld", title_response ? title_response->status : 0);
        http_response_free(title_response);
        return false;
    }

    // 检查标题响应 JSON
    json_object* title_obj = json_tokener_parse(title_response->data);
    if (!title_obj) {
        obs_log(LOG_ERROR, "解析标题响应 JSON 失败");
        http_response_free(title_response);
        return false;
    }

    json_object* title_code;
    if (json_object_object_get_ex(title_obj, "code", &title_code) && json_object_get_int(title_code) != 0) {
        obs_log(LOG_ERROR, "设置直播标题失败，错误码: %d", json_object_get_int(title_code));
        json_object_put(title_obj);
        http_response_free(title_response);
        return false;
    }
    json_object_put(title_obj);
    http_response_free(title_response);
    obs_log(LOG_INFO, "直播标题设置成功");

    // 启动直播
    Param start_params[10];
    param_count = 0;
    start_params[param_count].key = strdup("room_id");
    start_params[param_count].value = strdup(config->room_id);
    param_count++;
    start_params[param_count].key = strdup("platform");
    start_params[param_count].value = strdup("pc_link");
    param_count++;
    char area_str[16];
    snprintf(area_str, sizeof(area_str), "%d", area_id);
    start_params[param_count].key = strdup("area_v2");
    start_params[param_count].value = strdup(area_str);
    param_count++;
    start_params[param_count].key = strdup("backup_stream");
    start_params[param_count].value = strdup("0");
    param_count++;
    start_params[param_count].key = strdup("csrf_token");
    start_params[param_count].value = strdup(config->csrf_token);
    param_count++;
    start_params[param_count].key = strdup("csrf");
    start_params[param_count].value = strdup(config->csrf_token);
    param_count++;
    char build_str[32];
    snprintf(build_str, sizeof(build_str), "%ld", build);
    start_params[param_count].key = strdup("build");
    start_params[param_count].value = strdup(build_str);
    param_count++;
    start_params[param_count].key = strdup("version");
    start_params[param_count].value = strdup(curr_version);
    param_count++;
    snprintf(ts_str, sizeof(ts_str), "%ld", ts);
    start_params[param_count].key = strdup("ts");
    start_params[param_count].value = strdup(ts_str);
    param_count++;
    appsign(start_params, &param_count, app_key, app_sec);

    char start_data[1024] = "";
    for (size_t i = 0; i < param_count; i++) {
        if (i > 0) strncat(start_data, "&", sizeof(start_data) - strlen(start_data) - 1);
        strncat(start_data, start_params[i].key, sizeof(start_data) - strlen(start_data) - 1);
        strncat(start_data, "=", sizeof(start_data) - strlen(start_data) - 1);
        strncat(start_data, start_params[i].value, sizeof(start_data) - strlen(start_data) - 1);
        free(start_params[i].key);
        free(start_params[i].value);
    }

    HttpResponse* response = http_post_with_headers("https://api.live.bilibili.com/room/v1/Room/startLive", start_data, default_headers);
    if (!response || response->status != 200) {
        obs_log(LOG_ERROR, "获取推流码失败，状态码: %ld", response ? response->status : 0);
        if (response) {
            json_object* obj = json_tokener_parse(response->data);
            if (obj) {
                json_object* code;
                if (json_object_object_get_ex(obj, "code", &code) && json_object_get_int(code) == 60024) {
                    obs_log(LOG_ERROR, "获取推流码失败: 需要人脸认证");
                    json_object* data;
                    if (json_object_object_get_ex(obj, "data", &data)) {
                        json_object* qr;
                        if (json_object_object_get_ex(data, "qr", &qr)) {
                            obs_log(LOG_INFO, "人脸认证二维码: %s", json_object_get_string(qr));
                            // TODO: 显示二维码
                        }
                    }
                } else {
                    obs_log(LOG_ERROR, "获取推流码失败，错误码: %d", json_object_get_int(code));
                }
                json_object_put(obj);
            }
        }
        http_response_free(response);
        return false;
    }

    // 解析 JSON 获取 rtmp_addr 和 rtmp_code
    json_object* obj = json_tokener_parse(response->data);
    if (!obj) {
        obs_log(LOG_ERROR, "解析推流响应 JSON 失败");
        http_response_free(response);
        return false;
    }

    json_object* data;
    if (!json_object_object_get_ex(obj, "data", &data)) {
        obs_log(LOG_ERROR, "无法获取 JSON 中的 'data' 字段");
        json_object_put(obj);
        http_response_free(response);
        return false;
    }

    json_object* rtmp;
    if (!json_object_object_get_ex(data, "rtmp", &rtmp)) {
        obs_log(LOG_ERROR, "无法获取 JSON 中的 'rtmp' 字段");
        json_object_put(obj);
        http_response_free(response);
        return false;
    }

    json_object* addr;
    json_object* code;
    if (!json_object_object_get_ex(rtmp, "addr", &addr) || !json_object_object_get_ex(rtmp, "code", &code)) {
        obs_log(LOG_ERROR, "无法获取 JSON 中的 'addr' 或 'code' 字段");
        json_object_put(obj);
        http_response_free(response);
        return false;
    }

    *rtmp_addr = strdup(json_object_get_string(addr));
    *rtmp_code = strdup(json_object_get_string(code));
    json_object_put(obj);
    http_response_free(response);
    obs_log(LOG_INFO, "直播已开启！RTMP 地址: %s, 推流码: %s", *rtmp_addr, *rtmp_code);
    return true;
}

// 停止直播
bool bili_stop_live(BiliConfig* config) {
    char stop_data[512];
    snprintf(stop_data, sizeof(stop_data),
             "room_id=%s&platform=pc_link&csrf_token=%s&csrf=%s",
             config->room_id, config->csrf_token, config->csrf_token);

    HttpResponse* response = http_post_with_headers("https://api.live.bilibili.com/room/v1/Room/stopLive", stop_data, default_headers);
    if (!response || response->status != 200) {
        obs_log(LOG_ERROR, "停止直播失败，状态码: %ld", response ? response->status : 0);
        if (response) {
            json_object* obj = json_tokener_parse(response->data);
            if (obj) {
                json_object* code;
                if (json_object_object_get_ex(obj, "code", &code)) {
                    obs_log(LOG_ERROR, "停止直播失败，错误码: %d", json_object_get_int(code));
                }
                json_object_put(obj);
            }
        }
        http_response_free(response);
        return false;
    }

    json_object* obj = json_tokener_parse(response->data);
    if (!obj) {
        obs_log(LOG_ERROR, "解析停止直播响应 JSON 失败");
        http_response_free(response);
        return false;
    }

    json_object* code;
    if (json_object_object_get_ex(obj, "code", &code) && json_object_get_int(code) != 0) {
        obs_log(LOG_ERROR, "停止直播失败，错误码: %d", json_object_get_int(code));
        json_object_put(obj);
        http_response_free(response);
        return false;
    }

    json_object_put(obj);
    http_response_free(response);
    obs_log(LOG_INFO, "直播已停止！");
    return true;
}

// 更新直播间信息（例如分区）
bool bili_update_room_info(BiliConfig* config, int area_id) {
    char id_data[512];
    snprintf(id_data, sizeof(id_data),
             "room_id=%s&area_id=%d&activity_id=0&platform=pc_link&csrf_token=%s&csrf=%s",
             config->room_id, area_id, config->csrf_token, config->csrf_token);

    HttpResponse* response = http_post_with_headers("https://api.live.bilibili.com/room/v1/Room/update", id_data, default_headers);
    if (!response || response->status != 200) {
        obs_log(LOG_ERROR, "更新直播间信息失败，状态码: %ld", response ? response->status : 0);
        http_response_free(response);
        return false;
    }

    json_object* obj = json_tokener_parse(response->data);
    if (!obj) {
        obs_log(LOG_ERROR, "解析直播间信息响应 JSON 失败");
        http_response_free(response);
        return false;
    }

    json_object* code;
    if (json_object_object_get_ex(obj, "code", &code) && json_object_get_int(code) != 0) {
        obs_log(LOG_ERROR, "更新直播间信息失败，错误码: %d", json_object_get_int(code));
        json_object_put(obj);
        http_response_free(response);
        return false;
    }

    json_object_put(obj);
    http_response_free(response);
    obs_log(LOG_INFO, "直播间信息更新成功");
    return true;
}