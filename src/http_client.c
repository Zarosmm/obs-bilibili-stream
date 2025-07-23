#include <obs-module.h>
#include <curl/curl.h>
#include <plugin-support.h>
#include "http_client.h"

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    HttpResponse* response = (HttpResponse*)userp;
    char* ptr = realloc(response->data, response->data_size + realsize + 1);
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

// 新增：用于收集 cookies 的回调函数
static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userp) {
    size_t realsize = size * nitems;
    HttpResponse* response = (HttpResponse*)userp;
    const char* set_cookie = "Set-Cookie: ";
    if (strncmp(buffer, set_cookie, strlen(set_cookie)) == 0) {
        char* cookie = buffer + strlen(set_cookie);
        size_t cookie_len = strlen(cookie);
        if (cookie_len > 0 && cookie[cookie_len - 1] == '\n') {
            cookie_len--;
        }
        char* new_cookie = malloc(cookie_len + 1);
        strncpy(new_cookie, cookie, cookie_len);
        new_cookie[cookie_len] = '\0';
        if (response->cookies) {
            char* old_cookies = response->cookies;
            size_t old_len = strlen(old_cookies);
            response->cookies = realloc(old_cookies, old_len + cookie_len + 2);
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

    HttpResponse* response = calloc(1, sizeof(HttpResponse));
    response->data = malloc(1);
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
