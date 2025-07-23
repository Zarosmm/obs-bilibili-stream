#include <obs-module.h>
#include <curl/curl.h>
#include <plugin-support.h>
#include "http_client.h"

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    HttpResponse* response = (HttpResponse*)userp;
    char* ptr = (char*)realloc(response->data, response->data_size + realsize + 1);
    if (!ptr) {
        obs_log(LOG_ERROR, "内存分配失败");
        return 0;
    }
    response->data = ptr;
    memcpy(&(response->data[response->data_size]), contents, realsize);
    response->data_size += realsize;
    response->data[response->data_size] = 0;
    return realsize;
}

static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userp) {
    size_t realsize = size * nitems;
    HttpResponse* response = (HttpResponse*)userp;
    const char* set_cookie = "Set-Cookie: ";
    if (strncmp(buffer, set_cookie, strlen(set_cookie)) == 0) {
        char* cookie = buffer + strlen(set_cookie);
        // 查找第一个分号的位置
        char* semicolon = strchr(cookie, ';');
        size_t cookie_len = semicolon ? (size_t)(semicolon - cookie) : strlen(cookie);
        if (cookie_len > 0 && cookie[cookie_len - 1] == '\r') {
            cookie_len--; // 移除可能的回车符
        }
        if (cookie_len == 0) {
            obs_log(LOG_WARNING, "Set-Cookie 头为空，忽略");
            return realsize;
        }
        // 分配内存存储单个 cookie
        char* new_cookie = (char*)malloc(cookie_len + 1);
        if (!new_cookie) {
            obs_log(LOG_ERROR, "内存分配失败，无法保存 cookie");
            return realsize;
        }
        strncpy(new_cookie, cookie, cookie_len);
        new_cookie[cookie_len] = '\0';
        // 验证 cookie 格式（简单检查是否包含 '='）
        if (!strchr(new_cookie, '=')) {
            obs_log(LOG_WARNING, "无效的 cookie 格式: %s，忽略", new_cookie);
            free(new_cookie);
            return realsize;
        }
        obs_log(LOG_INFO, "提取的 cookie: %s", new_cookie);
        // 追加到 response->cookies
        if (response->cookies) {
            char* old_cookies = response->cookies;
            size_t old_len = strlen(old_cookies);
            response->cookies = (char*)realloc(old_cookies, old_len + cookie_len + 3); // +3 for "; " and null terminator
            if (!response->cookies) {
                free(old_cookies);
                free(new_cookie);
                obs_log(LOG_ERROR, "Cookies 内存重新分配失败");
                return realsize;
            }
            strcat(response->cookies, "; ");
            strcat(response->cookies, new_cookie);
            free(new_cookie);
        } else {
            response->cookies = new_cookie;
        }
    }
    return realsize;
}

void http_client_init(void) {
    curl_global_init(CURL_GLOBAL_ALL);
}

void http_client_cleanup(void) {
    curl_global_cleanup();
}

HttpResponse* http_get_with_headers(const char* url, const char** headers) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        obs_log(LOG_ERROR, "无法初始化 CURL");
        return NULL;
    }

    HttpResponse* response = (HttpResponse*)calloc(1, sizeof(HttpResponse));
    response->data = (char*)malloc(1);
    response->data_size = 0;
    response->cookies = NULL;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, response);

    struct curl_slist* header_list = NULL;
    if (headers) {
        for (int i = 0; headers[i]; i++) {
            header_list = curl_slist_append(header_list, headers[i]);
        }
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        obs_log(LOG_ERROR, "CURL 请求失败: %s", curl_easy_strerror(res));
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
        free(response->data);
        free(response->cookies);
        free(response);
        return NULL;
    }

    	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status);
    	curl_slist_free_all(header_list);
    	curl_easy_cleanup(curl);
	obs_log(LOG_INFO, "response data : %s", response->data ? response->data : "无数据");
	obs_log(LOG_INFO, "response cookies : %s", response->cookies ? response->cookies : "无 cookies");
    return response;
}

HttpResponse* http_post_with_headers(const char* url, const char* data, const char** headers) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        obs_log(LOG_ERROR, "无法初始化 CURL");
        return NULL;
    }

    HttpResponse* response = calloc(1, sizeof(HttpResponse));
    response->data = malloc(1);
    response->data_size = 0;
    response->cookies = NULL;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, response);

    struct curl_slist* header_list = NULL;
    if (headers) {
        for (int i = 0; headers[i]; i++) {
            header_list = curl_slist_append(header_list, headers[i]);
        }
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
    	obs_log(LOG_ERROR, "POST 请求失败: %s", curl_easy_strerror(res));
        curl_slist_free_all(header_list);
        curl_easy_cleanup(curl);
        free(response->data);
        free(response->cookies);
        free(response);
        return NULL;
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response->status);
    curl_slist_free_all(header_list);
    curl_easy_cleanup(curl);
	obs_log(LOG_INFO, "response data : %s", response->data ? response->data : "无数据");
	obs_log(LOG_INFO, "response cookies : %s", response->cookies ? response->cookies : "无 cookies");
    return response;
}

// 释放 HttpResponse 内存
void http_response_free(HttpResponse* response) {
    if (response) {
        free(response->data);
        free(response->cookies);
        free(response);
    }
}
