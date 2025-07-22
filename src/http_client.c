#include <obs-module.h>
#include <curl/curl.h>
#include <plugin-support.h>
#include "http_client.h"

// 回调函数，用于收集 HTTP 响应数据
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    HttpResponse* response = (HttpResponse*)userp;

    char* ptr = realloc(response->data, response->size + realsize + 1);
    if (!ptr) {
        obs_log(LOG_ERROR, "无法分配内存用于 HTTP 响应");
        return 0;
    }

    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;

    return realsize;
}

// 初始化 HTTP 客户端
void http_client_init(void) {
    curl_global_init(CURL_GLOBAL_ALL);
}

// 清理 HTTP 客户端
void http_client_cleanup(void) {
    curl_global_cleanup();
}

// 发送 GET 请求
HttpResponse* http_get(const char* url) {
    return http_get_with_headers(url, NULL);
}

// 发送 GET 请求（带自定义头）
HttpResponse* http_get_with_headers(const char* url, const char** headers) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        obs_log(LOG_ERROR, "无法初始化 CURL");
        return NULL;
    }

    HttpResponse* response = calloc(1, sizeof(HttpResponse));
    if (!response) {
        obs_log(LOG_ERROR, "无法分配 HTTP 响应内存");
        curl_easy_cleanup(curl);
        return NULL;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    struct curl_slist* header_list = NULL;
    if (headers) {
        for (int i = 0; headers[i]; i++) {
            header_list = curl_slist_append(header_list, headers[i]);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        obs_log(LOG_ERROR, "GET 请求失败: %s", curl_easy_strerror(res));
        free(response);
        curl_easy_cleanup(curl);
        if (header_list) curl_slist_free_all(header_list);
        return NULL;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status);
    if (header_list) curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    return response;
}

// 发送 POST 请求
HttpResponse* http_post(const char* url, const char* post_data) {
    return http_post_with_headers(url, post_data, NULL);
}

// 发送 POST 请求（带自定义头）
HttpResponse* http_post_with_headers(const char* url, const char* post_data, const char** headers) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        obs_log(LOG_ERROR, "无法初始化 CURL");
        return NULL;
    }

    HttpResponse* response = calloc(1, sizeof(HttpResponse));
    if (!response) {
        obs_log(LOG_ERROR, "无法分配 HTTP 响应内存");
        curl_easy_cleanup(curl);
        return NULL;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    struct curl_slist* header_list = NULL;
    if (headers) {
        for (int i = 0; headers[i]; i++) {
            header_list = curl_slist_append(header_list, headers[i]);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        obs_log(LOG_ERROR, "POST 请求失败: %s", curl_easy_strerror(res));
        free(response);
        curl_easy_cleanup(curl);
        if (header_list) curl_slist_free_all(header_list);
        return NULL;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status);
    if (header_list) curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
    return response;
}

// 释放 HttpResponse 内存
void http_response_free(HttpResponse* response) {
    if (response) {
        free(response->data);
        free(response);
    }
}