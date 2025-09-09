#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

typedef struct {
	long status;
	char* data;
	size_t data_size;
	char* cookies; // 新增字段，用于存储响应中的 cookies
} HttpResponse;

#ifdef __cplusplus
extern "C" {
#endif

	void http_client_init(void);
	void http_client_cleanup(void);
	HttpResponse* http_get_with_headers(const char* url, const char** headers);
	HttpResponse* http_post_with_headers(const char* url, const char* data, const char** headers);
	void http_response_free(HttpResponse* response);

#ifdef __cplusplus
}
#endif

#endif // HTTP_CLIENT_H
