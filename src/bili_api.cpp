#include <obs-module.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <plugin-support.h>
#include "bili_api.h"
#include "http_client.h"
#include "json11/json11.hpp"

// MD5 implementation (public domain, based on standard MD5 algorithm)
typedef struct {
    unsigned int state[4];
    unsigned long long count[2]; // Changed to 64-bit to handle large inputs
    unsigned char buffer[64];
} MD5_CTX;

static void md5_init(MD5_CTX *context);
static void md5_update(MD5_CTX *context, const unsigned char *input, size_t inputLen); // Use size_t
static void md5_final(unsigned char digest[16], MD5_CTX *context);
static void md5_transform(unsigned int state[4], const unsigned char block[64]);

#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

#define FF(a, b, c, d, x, s, ac) { \
    (a) += F((b), (c), (d)) + (x) + (unsigned int)(ac); \
    (a) = ROTATE_LEFT((a), (s)); \
    (a) += (b); \
}

#define GG(a, b, c, d, x, s, ac) { \
    (a) += G((b), (c), (d)) + (x) + (unsigned int)(ac); \
    (a) = ROTATE_LEFT((a), (s)); \
    (a) += (b); \
}

#define HH(a, b, c, d, x, s, ac) { \
    (a) += H((b), (c), (d)) + (x) + (unsigned int)(ac); \
    (a) = ROTATE_LEFT((a), (s)); \
    (a) += (b); \
}

#define II(a, b, c, d, x, s, ac) { \
    (a) += I((b), (c), (d)) + (x) + (unsigned int)(ac); \
    (a) = ROTATE_LEFT((a), (s)); \
    (a) += (b); \
}

static const unsigned int T[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

static const unsigned char PADDING[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void md5_init(MD5_CTX *context) {
    context->count[0] = context->count[1] = 0;
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
}

static void md5_update(MD5_CTX *context, const unsigned char *input, size_t inputLen) {
    size_t i, index, partLen;
    index = (size_t)((context->count[0] >> 3) & 0x3F);
    if ((context->count[0] += ((unsigned long long)inputLen << 3)) < ((unsigned long long)inputLen << 3))
        context->count[1]++;
    context->count[1] += ((unsigned long long)inputLen >> 29);
    partLen = 64 - index;
    if (inputLen >= partLen) {
        memcpy(&context->buffer[index], input, partLen);
        md5_transform(context->state, context->buffer);
        for (i = partLen; i + 63 < inputLen; i += 64)
            md5_transform(context->state, &input[i]);
        index = 0;
    } else {
        i = 0;
    }
    memcpy(&context->buffer[index], &input[i], inputLen - i);
}

static void md5_final(unsigned char digest[16], MD5_CTX *context) {
    unsigned char bits[8];
    size_t index, padLen;
    bits[0] = (unsigned char)(context->count[0] & 0xFF);
    bits[1] = (unsigned char)((context->count[0] >> 8) & 0xFF);
    bits[2] = (unsigned char)((context->count[0] >> 16) & 0xFF);
    bits[3] = (unsigned char)((context->count[0] >> 24) & 0xFF);
    bits[4] = (unsigned char)(context->count[1] & 0xFF);
    bits[5] = (unsigned char)((context->count[1] >> 8) & 0xFF);
    bits[6] = (unsigned char)((context->count[1] >> 16) & 0xFF);
    bits[7] = (unsigned char)((context->count[1] >> 24) & 0xFF);
    index = (size_t)((context->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    md5_update(context, PADDING, padLen);
    md5_update(context, bits, 8);
    for (index = 0; index < 4; index++) {
        digest[index * 4] = (unsigned char)(context->state[index] & 0xFF);
        digest[index * 4 + 1] = (unsigned char)((context->state[index] >> 8) & 0xFF);
        digest[index * 4 + 2] = (unsigned char)((context->state[index] >> 16) & 0xFF);
        digest[index * 4 + 3] = (unsigned char)((context->state[index] >> 24) & 0xFF);
    }
}

static void md5_transform(unsigned int state[4], const unsigned char block[64]) {
    unsigned int a = state[0], b = state[1], c = state[2], d = state[3], x[16];
    for (int i = 0, j = 0; i < 16; ++i, j += 4)
        x[i] = ((unsigned int)block[j]) | (((unsigned int)block[j + 1]) << 8) |
               (((unsigned int)block[j + 2]) << 16) | (((unsigned int)block[j + 3]) << 24);
    FF(a, b, c, d, x[0], 7, 0xd76aa478); FF(d, a, b, c, x[1], 12, 0xe8c7b756);
    FF(c, d, a, b, x[2], 17, 0x242070db); FF(b, c, d, a, x[3], 22, 0xc1bdceee);
    FF(a, b, c, d, x[4], 7, 0xf57c0faf); FF(d, a, b, c, x[5], 12, 0x4787c62a);
    FF(c, d, a, b, x[6], 17, 0xa8304613); FF(b, c, d, a, x[7], 22, 0xfd469501);
    FF(a, b, c, d, x[8], 7, 0x698098d8); FF(d, a, b, c, x[9], 12, 0x8b44f7af);
    FF(c, d, a, b, x[10], 17, 0xffff5bb1); FF(b, c, d, a, x[11], 22, 0x895cd7be);
    FF(a, b, c, d, x[12], 7, 0x6b901122); FF(d, a, b, c, x[13], 12, 0xfd987193);
    FF(c, d, a, b, x[14], 17, 0xa679438e); FF(b, c, d, a, x[15], 22, 0x49b40821);
    GG(a, b, c, d, x[1], 5, 0xf61e2562); GG(d, a, b, c, x[6], 9, 0xc040b340);
    GG(c, d, a, b, x[11], 14, 0x265e5a51); GG(b, c, d, a, x[0], 20, 0xe9b6c7aa);
    GG(a, b, c, d, x[5], 5, 0xd62f105d); GG(d, a, b, c, x[10], 9, 0x02441453);
    GG(c, d, a, b, x[15], 14, 0xd8a1e681); GG(b, c, d, a, x[4], 20, 0xe7d3fbc8);
    GG(a, b, c, d, x[9], 5, 0x21e1cde6); GG(d, a, b, c, x[14], 9, 0xc33707d6);
    GG(c, d, a, b, x[3], 14, 0xf4d50d87); GG(b, c, d, a, x[8], 20, 0x455a14ed);
    GG(a, b, c, d, x[13], 5, 0xa9e3e905); GG(d, a, b, c, x[2], 9, 0xfcefa3f8);
    GG(c, d, a, b, x[7], 14, 0x676f02d9); GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);
    HH(a, b, c, d, x[5], 4, 0xfffa3942); HH(d, a, b, c, x[8], 11, 0x8771f681);
    HH(c, d, a, b, x[11], 16, 0x6d9d6122); HH(b, c, d, a, x[14], 23, 0xfde5380c);
    HH(a, b, c, d, x[1], 4, 0xa4beea44); HH(d, a, b, c, x[4], 11, 0x4bdecfa9);
    HH(c, d, a, b, x[7], 16, 0xf6bb4b60); HH(b, c, d, a, x[10], 23, 0xbebfbc70);
    HH(a, b, c, d, x[13], 4, 0x289b7ec6); HH(d, a, b, c, x[0], 11, 0xeaa127fa);
    HH(c, d, a, b, x[3], 16, 0xd4ef3085); HH(b, c, d, a, x[6], 23, 0x04881d05);
    HH(a, b, c, d, x[9], 4, 0xd9d4d039); HH(d, a, b, c, x[12], 11, 0xe6db99e5);
    HH(c, d, a, b, x[15], 16, 0x1fa27cf8); HH(b, c, d, a, x[2], 23, 0xc4ac5665);
    II(a, b, c, d, x[0], 6, 0xf4292244); II(d, a, b, c, x[7], 10, 0x432aff97);
    II(c, d, a, b, x[14], 15, 0xab9423a7); II(b, c, d, a, x[5], 21, 0xfc93a039);
    II(a, b, c, d, x[12], 6, 0x655b59c3); II(d, a, b, c, x[3], 10, 0x8f0ccc92);
    II(c, d, a, b, x[10], 15, 0xffeff47d); II(b, c, d, a, x[1], 21, 0x85845dd1);
    II(a, b, c, d, x[8], 6, 0x6fa87e4f); II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
    II(c, d, a, b, x[6], 15, 0xa3014314); II(b, c, d, a, x[13], 21, 0x4e0811a1);
    II(a, b, c, d, x[4], 6, 0xf7537e82); II(d, a, b, c, x[11], 10, 0xbd3af235);
    II(c, d, a, b, x[2], 15, 0x2ad7d2bb); II(b, c, d, a, x[9], 21, 0xeb86d391);
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

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
    unsigned char md5_result[16];
    char md5_hex[33];
    char query_with_sec[2048];
    snprintf(query_with_sec, sizeof(query_with_sec), "%s%s", query, app_sec);
    MD5_CTX ctx;
    md5_init(&ctx);
    md5_update(&ctx, (unsigned char*)query_with_sec, strlen(query_with_sec));
    md5_final(md5_result, &ctx);
    for (int i = 0; i < 16; i++) {
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

    // 使用 json11 解析 JSON
    std::string err;
    json11::Json json = json11::Json::parse(response->data, err);
    if (!err.empty()) {
        obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
        http_response_free(response);
        return false;
    }

    // 提取 data.now 和 data.qrcode_key
    std::string now = json["data"]["now"].string_value();
    if (now.empty()) {
        obs_log(LOG_ERROR, "无法提取 data.url 或 data.qrcode_key");
        http_response_free(response);
        return false;
    }
    long ts = atol(now);
    free(now);
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
bool bili_get_qrcode(char** qrcode_data, char** qrcode_key) {
    HttpResponse* response = http_get_with_headers("https://passport.bilibili.com/x/passport-login/web/qrcode/generate", default_headers);
    if (!response || response->status != 200) {
        obs_log(LOG_ERROR, "获取二维码失败，状态码: %ld", response ? response->status : 0);
        http_response_free(response);
        return false;
    }

    // 使用 json11 解析 JSON
    std::string err;
    json11::Json json = json11::Json::parse(response->data, err);
    if (!err.empty()) {
        obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
        http_response_free(response);
        return false;
    }

    // 提取 data.url 和 data.qrcode_key
    std::string url = json["data"]["url"].string_value();
    std::string key = json["data"]["qrcode_key"].string_value();
    if (url.empty() || key.empty()) {
        obs_log(LOG_ERROR, "无法提取 data.url 或 data.qrcode_key");
        http_response_free(response);
        return false;
    }

    // 分配内存并复制字符串
    *qrcode_data = strdup(url.c_str());
    *qrcode_key = strdup(key.c_str());
    if (!*qrcode_data || !*qrcode_key) {
        obs_log(LOG_ERROR, "内存分配失败");
        free(*qrcode_data);
        free(*qrcode_key);
        http_response_free(response);
        return false;
    }

    obs_log(LOG_INFO, "获取二维码成功，URL: %s, Key: %s", *qrcode_data, *qrcode_key);
    http_response_free(response);
    return true;
}

// 检查二维码情况
bool bili_qr_login(char** qrcode_key) {
    char qr_login_url[2048];
    snprintf(qr_login_url, sizeof(qr_login_url), "https://passport.bilibili.com/x/passport-login/web/qrcode/poll?qrcode_key?qrcode_key=%s", *qrcode_key);
    HttpResponse* response = http_get_with_headers(qr_login_url, default_headers);
    if (!response || response->status != 200) {
        obs_log(LOG_ERROR, "获取二维码失败，状态码: %ld", response ? response->status : 0);
        http_response_free(response);
        return false;
    }

    // 使用 json11 解析 JSON
    std::string err;
    json11::Json json = json11::Json::parse(response->data, err);
    if (!err.empty()) {
        obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
        http_response_free(response);
        return false;
    }

    // 检查 code 是否为 0
    if (json["code"].int_value() != 0) {
        obs_log(LOG_ERROR, "API 返回错误，code: %d, message: %s",
                json["code"].int_value(), json["message"].string_value().c_str());
        http_response_free(response);
        return false;
    } else{
        http_response_free(response);
        obs_log(LOG_INFO, "登录成功");
        return true;
    }
}

// 检查登录状态
bool bili_check_login_status(char** status_data) {
    HttpResponse* response = http_get_with_headers("https://api.bilibili.com/x/web-interface/nav", default_headers);
    if (!response || response->status != 200) {
        obs_log(LOG_ERROR, "检查登录状态失败，状态码: %ld", response ? response->status : 0);
        http_response_free(response);
        return false;
    }

    // 返回整个 JSON 响应
    *status_data = strdup(response->data);
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

    // 使用 json11 解析 JSON
    std::string err;
    json11::Json json = json11::Json::parse(version_response->data, err);
    if (!err.empty()) {
        obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
        http_response_free(version_response);
        return false;
    }

    // 提取 data.build 和 data.curr_version
    std::string build_str = json["data"]["build"].string_value();
    std::string curr_version_str = json["data"]["curr_version"].string_value();
    if (build_str.empty() || curr_version_str.empty()) {
        obs_log(LOG_ERROR, "无法提取 data.build 或 data.curr_version");
        http_response_free(version_response);
        return false;
    }

    long build = build_str ? atol(build_str) : 1234; // 默认值
    if (!curr_version_str) curr_version_str = strdup("1.0.0"); // 默认值
    free(build_str);
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
        free(curr_version_str);
        return false;
    }
    // 使用 json11 解析 JSON
    std::string err;
    json11::Json json = json11::Json::parse(title_response->data, err);
    if (!err.empty()) {
        obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
        http_response_free(title_response);
        return false;
    }
    // 检查 code
    std::string code_str = json["code"].string_value();
    if (code_str && atoi(code_str) != 0) {
        obs_log(LOG_ERROR, "设置直播标题失败，错误码: %s", code_str);
        free(code_str);
        http_response_free(title_response);
        free(curr_version_str);
        return false;
    }
    free(code_str);
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
    char build_str_buf[32];
    snprintf(build_str_buf, sizeof(build_str_buf), "%ld", build);
    start_params[param_count].key = strdup("build");
    start_params[param_count].value = strdup(build_str_buf);
    param_count++;
    start_params[param_count].key = strdup("version");
    start_params[param_count].value = strdup(curr_version_str);
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
    free(curr_version_str);

    HttpResponse* response = http_post_with_headers("https://api.live.bilibili.com/room/v1/Room/startLive", start_data, default_headers);
    // 使用 json11 解析 JSON
    std::string err;
    json11::Json json = json11::Json::parse(response->data, err);
    if (!err.empty()) {
        obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
        http_response_free(response);
        return false;
    }
    if (!response || response->status != 200) {
        obs_log(LOG_ERROR, "获取推流码失败，状态码: %ld", response ? response->status : 0);
        if (response) {
            std::string code_str = json["code"].string_value();
            if (code_str && atoi(code_str) == 60024) {
                obs_log(LOG_ERROR, "获取推流码失败: 需要人脸认证, 二维码: %s", qr ? qr : "无二维码数据");
            } else if (code_str) {
                obs_log(LOG_ERROR, "获取推流码失败，错误码: %s", code_str);
            }
            free(code_str);
        }
        http_response_free(response);
        return false;
    }

    // 解析 data.rtmp.addr 和 data.rtmp.code
    *rtmp_addr = json["rtmp"]["addr"].string_value();
    *rtmp_code = json["rtmp"]["code"].string_value();
    if (!*rtmp_addr || !*rtmp_code) {
        obs_log(LOG_ERROR, "无法解析 JSON 中的 'data.rtmp.addr' 或 'data.rtmp.code' 字段");
        free(*rtmp_addr);
        free(*rtmp_code);
        http_response_free(response);
        return false;
    }

    obs_log(LOG_INFO, "直播已开启！RTMP 地址: %s, 推流码: %s", *rtmp_addr, *rtmp_code);
    http_response_free(response);
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
        http_response_free(response);
        return false;
    }

    // 使用 json11 解析 JSON
    std::string err;
    json11::Json json = json11::Json::parse(response->data, err);
    if (!err.empty()) {
        obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
        http_response_free(response);
        return false;
    }

    // 检查 code
    char* code_str = json["code"].string_value();
    if (code_str && atoi(code_str) != 0) {
        obs_log(LOG_ERROR, "停止直播失败，错误码: %s", code_str);
        free(code_str);
        http_response_free(response);
        return false;
    }
    free(code_str);

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
    // 使用 json11 解析 JSON
    std::string err;
    json11::Json json = json11::Json::parse(response->data, err);
    if (!err.empty()) {
        obs_log(LOG_ERROR, "JSON 解析失败: %s", err.c_str());
        http_response_free(response);
        return false;
    }
    // 检查 code
    char* code_str = json["code"].string_value();
    if (code_str && atoi(code_str) != 0) {
        obs_log(LOG_ERROR, "更新直播间信息失败，错误码: %s", code_str);
        free(code_str);
        http_response_free(response);
        return false;
    }
    free(code_str);

    http_response_free(response);
    obs_log(LOG_INFO, "直播间信息更新成功");
    return true;
}
