#ifndef __HTTP_CLIENT_H__
#define __HTTP_CLIENT_H__

struct http_header {
    const char *key;
    const char *value;
};

/**
 * http_get() - Sends GET request
 *
 * @param url: Request URL
 * @param headers: Array of headers
 * @param response: Pointer to a response buffer, will be allocated during request
 *
 * @returns: HTTP response status code or -1 if error occured
 */
int http_get(const char *url, struct http_header *headers, char **response);

/**
 * http_get() - Sends POST request
 *
 * @param url: Request URL
 * @param headers: Array of headers
 * @param body: Request body
 * @param response: Pointer to a response buffer, will be allocated during request
 *
 * @returns: HTTP response status code or -1 if error occured
 */
int http_post(const char *url, struct http_header *headers, const char *body, char **response);

// Simple wrapper to set a proper Content-Type
int http_post_json(const char *url, const char *body, char **response);

#endif
