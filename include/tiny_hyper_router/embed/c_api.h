#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* thr_client_handle;

thr_client_handle thr_create_client_from_json(const char* config_json);
void thr_destroy_client(thr_client_handle handle);

const char* thr_send_message_json(thr_client_handle handle, const char* request_json);
const char* thr_get_session_json(thr_client_handle handle, const char* session_id);

void thr_free_string(const char* value);

#ifdef __cplusplus
}
#endif
