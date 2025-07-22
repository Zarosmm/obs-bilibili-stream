// http_client.h
#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

    // HTTP 响应结构体
    typedef struct {
        char* data;    // 响应数据
        size_t size;   // 数据大小
        long status;   // HTTP 状态码
    } HttpResponse;

    // 初始化 HTTP 客户端
    void http_client_init(void);

    // 清理 HTTP 客户端
    void http_client_cleanup(void);

    // 发送 GET 请求
    HttpResponse* http_get(const char* url);

    // 发送 GET 请求（带自定义头）
    HttpResponse* http_get_with_headers(const char* url, const char** headers);

    // 发送 POST 请求
    HttpResponse* http_post(const char* url, const char* post_data);

    // 发送 POST 请求（带自定义头）
    HttpResponse* http_post_with_headers(const char* url, const char* post_data, const char** headers);

    // 释放 HttpResponse 内存
    void http_response_free(HttpResponse* response);

#ifdef __cplusplus
}
#endif

#endif // HTTP_CLIENT_H
