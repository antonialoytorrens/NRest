/*
* MIT LICENSE
* Copyright (c) 2025 Antoni Aloy Torrens
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is furnished
* to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
* FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
* COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
* IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
* WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/**
* "n8n" is a registered trademark. This project is not affiliated with, 
* endorsed by, or connected to n8n or n8n.io in any way. This is an 
* independent implementation for educational and interoperability purposes only.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <curl/curl.h>
#include <jansson.h>
#include <pthread.h>
#include <unistd.h>
#include <unity.h>

// Configuration constants
#define LOCAL_BASE_URL "http://localhost:8080"
#define UPSTREAM_BASE_URL "https://api.n8n.io"
#define MAX_RESPONSE_SIZE 1048576  // 1MB
#define HTTP_TIMEOUT_SECONDS 10L
#define SERVER_WAIT_TIMEOUT 10
#define UPSTREAM_WAIT_TIMEOUT 5
#define MAX_PATH_LENGTH 512
#define MAX_URL_LENGTH 512
#define DIFF_BUFFER_SIZE 1024
#define DEFAULT_PAGE_SIZE 20
#define SINGLE_RESULT_LIMIT 1

// HTTP status codes
#define HTTP_OK 200

// Endpoint paths
#define ENDPOINT_HEALTH "/health"
#define ENDPOINT_CATEGORIES "/templates/categories"
#define ENDPOINT_COLLECTIONS "/templates/collections"
#define ENDPOINT_SEARCH "/templates/search"
#define ENDPOINT_WORKFLOWS "/templates/workflows"

// JSON field names
#define FIELD_CATEGORIES "categories"
#define FIELD_COLLECTIONS "collections"
#define FIELD_WORKFLOWS "workflows"
#define FIELD_TOTAL_WORKFLOWS "totalWorkflows"
#define FIELD_ID "id"
#define FIELD_NAME "name"
#define FIELD_TOTAL_VIEWS "totalViews"
#define FIELD_DESCRIPTION "description"
#define FIELD_CREATED_AT "createdAt"
#define FIELD_NODES "nodes"
#define FIELD_USER "user"
#define FIELD_USERNAME "username"
#define FIELD_VERIFIED "verified"

// Global configuration
typedef struct {
    bool test_upstream;
    bool verbose_mode;
    CURL *curl;
} test_config_t;

static test_config_t g_config = {0};

// Response buffer structure
typedef struct {
    char *data;
    size_t size;
} response_buffer_t;

// Test endpoint structure
typedef struct {
    const char *path;
    const char *params;
    const char *description;
} test_endpoint_t;

// Initialize response buffer
static void init_response_buffer(response_buffer_t *buf) {
    buf->data = NULL;
    buf->size = 0;
}

// Free response buffer
static void free_response_buffer(response_buffer_t *buf) {
    if (buf->data) {
        free(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
}

// Write callback for CURL
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    response_buffer_t *buf = (response_buffer_t *)userp;
    
    char *ptr = realloc(buf->data, buf->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Not enough memory for response\n");
        return 0;
    }
    
    buf->data = ptr;
    memcpy(&(buf->data[buf->size]), contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = 0;
    
    return realsize;
}

// Build URL from base, path and optional params
static void build_url(char *dest, size_t dest_size, const char *base, const char *path, const char *params) {
    if (params && strlen(params) > 0) {
        snprintf(dest, dest_size, "%s%s?%s", base, path, params);
    } else {
        snprintf(dest, dest_size, "%s%s", base, path);
    }
}

// Perform HTTP GET request
static json_t* http_get(const char *url) {
    response_buffer_t response;
    init_response_buffer(&response);
    
    CURLcode res;
    json_error_t error;
    json_t *json = NULL;
    
    curl_easy_setopt(g_config.curl, CURLOPT_URL, url);
    curl_easy_setopt(g_config.curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(g_config.curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(g_config.curl, CURLOPT_TIMEOUT, HTTP_TIMEOUT_SECONDS);
    curl_easy_setopt(g_config.curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    res = curl_easy_perform(g_config.curl);
    
    if (res != CURLE_OK) {
        if (g_config.verbose_mode) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
    } else {
        long http_code = 0;
        curl_easy_getinfo(g_config.curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        if (http_code == HTTP_OK) {
            json = json_loads(response.data, 0, &error);
            if (!json && g_config.verbose_mode) {
                fprintf(stderr, "JSON parse error: %s at line %d\n", error.text, error.line);
            }
        } else if (g_config.verbose_mode) {
            fprintf(stderr, "HTTP %ld for %s\n", http_code, url);
        }
    }
    
    free_response_buffer(&response);
    return json;
}

// Get JSON type name as string
static const char* json_typeof_name(json_t *json) {
    switch(json_typeof(json)) {
        case JSON_OBJECT: return "object";
        case JSON_ARRAY: return "array";
        case JSON_STRING: return "string";
        case JSON_INTEGER: return "integer";
        case JSON_REAL: return "real";
        case JSON_TRUE:
        case JSON_FALSE: return "boolean";
        case JSON_NULL: return "null";
        default: return "unknown";
    }
}

// Compare JSON schema structure recursively
static bool compare_json_schema(json_t *local, json_t *upstream, const char *path, char *diff_buffer, size_t buffer_size) {
    char new_path[MAX_PATH_LENGTH];
    
    // Check if types match
    if (json_typeof(local) != json_typeof(upstream)) {
        if (diff_buffer) {
            snprintf(diff_buffer, buffer_size, 
                    "Type mismatch at %s: local=%s, upstream=%s",
                    path, json_typeof_name(local), json_typeof_name(upstream));
        }
        return false;
    }
    
    // For objects, compare keys
    if (json_is_object(local)) {
        const char *key;
        json_t *value;
        
        // Check local keys exist in upstream
        json_object_foreach(local, key, value) {
            json_t *upstream_value = json_object_get(upstream, key);
            if (!upstream_value) {
                if (diff_buffer) {
                    snprintf(diff_buffer, buffer_size, 
                            "Key '%s' exists in local but not in upstream at %s", key, path);
                }
                return false;
            }
            
            snprintf(new_path, sizeof(new_path), "%s.%s", path, key);
            if (!compare_json_schema(value, upstream_value, new_path, diff_buffer, buffer_size)) {
                return false;
            }
        }
        
        // Optionally log upstream-only keys
        if (g_config.verbose_mode) {
            json_object_foreach(upstream, key, value) {
                if (!json_object_get(local, key)) {
                    printf("Note: Key '%s' exists in upstream but not in local at %s\n", key, path);
                }
            }
        }
    }
    // For arrays, check first element schema (if exists)
    else if (json_is_array(local) && json_array_size(local) > 0 && json_array_size(upstream) > 0) {
        snprintf(new_path, sizeof(new_path), "%s[0]", path);
        return compare_json_schema(json_array_get(local, 0), 
                                 json_array_get(upstream, 0), 
                                 new_path, diff_buffer, buffer_size);
    }
    
    return true;
}

// Validate required field exists and has correct type
static void assert_field_type(json_t *obj, const char *field_name, json_type expected_type, const char *context) {
    json_t *field = json_object_get(obj, field_name);
    char message[256];
    
    snprintf(message, sizeof(message), "%s: field '%s' is missing", context, field_name);
    TEST_ASSERT_NOT_NULL_MESSAGE(field, message);
    
    snprintf(message, sizeof(message), "%s: field '%s' has wrong type", context, field_name);
    TEST_ASSERT_TRUE_MESSAGE(json_typeof(field) == expected_type, message);
}

// Generic endpoint schema test
static void test_endpoint_schema_impl(const test_endpoint_t *endpoint) {
    char local_url[MAX_URL_LENGTH];
    char upstream_url[MAX_URL_LENGTH];
    char diff_buffer[DIFF_BUFFER_SIZE] = {0};
    
    // Build URLs
    build_url(local_url, sizeof(local_url), LOCAL_BASE_URL, endpoint->path, endpoint->params);
    build_url(upstream_url, sizeof(upstream_url), UPSTREAM_BASE_URL, endpoint->path, endpoint->params);
    
    // Get local response
    json_t *local_json = http_get(local_url);
    TEST_ASSERT_NOT_NULL_MESSAGE(local_json, "Failed to get valid JSON from local server");
    
    // Compare with upstream if enabled
    if (g_config.test_upstream) {
        json_t *upstream_json = http_get(upstream_url);
        if (upstream_json) {
            bool schemas_match = compare_json_schema(local_json, upstream_json, "root", 
                                                    diff_buffer, sizeof(diff_buffer));
            
            if (!schemas_match && g_config.verbose_mode) {
                printf("Schema difference in %s:\n%s\n", endpoint->path, diff_buffer);
                printf("Local JSON:\n%s\n", json_dumps(local_json, JSON_INDENT(2)));
                printf("Upstream JSON:\n%s\n", json_dumps(upstream_json, JSON_INDENT(2)));
            }
            
            TEST_ASSERT_TRUE_MESSAGE(schemas_match, diff_buffer);
            json_decref(upstream_json);
        } else {
            TEST_IGNORE_MESSAGE("Could not fetch upstream data - skipping comparison");
        }
    }
    
    // Perform endpoint-specific validation
    if (strstr(endpoint->path, ENDPOINT_CATEGORIES)) {
        assert_field_type(local_json, FIELD_CATEGORIES, JSON_ARRAY, endpoint->path);
    } else if (strstr(endpoint->path, ENDPOINT_COLLECTIONS) && !strstr(endpoint->path, "/collections/")) {
        assert_field_type(local_json, FIELD_COLLECTIONS, JSON_ARRAY, endpoint->path);
    } else if (strstr(endpoint->path, ENDPOINT_SEARCH)) {
        assert_field_type(local_json, FIELD_TOTAL_WORKFLOWS, JSON_INTEGER, endpoint->path);
        assert_field_type(local_json, FIELD_WORKFLOWS, JSON_ARRAY, endpoint->path);
    }
    
    json_decref(local_json);
}

// Fetch first item ID from an endpoint
static int get_first_item_id(const char *endpoint_path, const char *array_field) {
    char url[MAX_URL_LENGTH];
    snprintf(url, sizeof(url), "%s%s?limit=%d", LOCAL_BASE_URL, endpoint_path, SINGLE_RESULT_LIMIT);
    
    json_t *result = http_get(url);
    int item_id = 0;
    
    if (result) {
        json_t *items = json_object_get(result, array_field);
        if (json_array_size(items) > 0) {
            json_t *first_item = json_array_get(items, 0);
            item_id = json_integer_value(json_object_get(first_item, FIELD_ID));
        }
        json_decref(result);
    }
    
    return item_id;
}

// Test functions using the test_endpoint_t structure
void test_health_endpoint(void) {
    test_endpoint_t endpoint = {ENDPOINT_HEALTH, NULL, "Health check"};
    test_endpoint_schema_impl(&endpoint);
}

void test_categories_endpoint(void) {
    test_endpoint_t endpoint = {ENDPOINT_CATEGORIES, NULL, "Categories listing"};
    test_endpoint_schema_impl(&endpoint);
}

void test_collections_endpoint(void) {
    test_endpoint_t endpoint = {ENDPOINT_COLLECTIONS, NULL, "Collections listing"};
    test_endpoint_schema_impl(&endpoint);
}

void test_collections_with_search(void) {
    test_endpoint_t endpoint = {ENDPOINT_COLLECTIONS, "search=test", "Collections with search"};
    test_endpoint_schema_impl(&endpoint);
}

void test_search_endpoint_basic(void) {
    char params[128];
    snprintf(params, sizeof(params), "search=&page=1&limit=%d", DEFAULT_PAGE_SIZE);
    test_endpoint_t endpoint = {ENDPOINT_SEARCH, params, "Basic search"};
    test_endpoint_schema_impl(&endpoint);
}

void test_search_endpoint_with_category(void) {
    char params[128];
    snprintf(params, sizeof(params), "search=&page=1&limit=%d&category=AI", DEFAULT_PAGE_SIZE);
    test_endpoint_t endpoint = {ENDPOINT_SEARCH, params, "Search with category"};
    test_endpoint_schema_impl(&endpoint);
}

// TODO
/*
void test_workflow_detail_endpoint(void) {
    int workflow_id = get_first_item_id(ENDPOINT_SEARCH, FIELD_WORKFLOWS);
    
    if (workflow_id > 0) {
        char endpoint_path[256];
        snprintf(endpoint_path, sizeof(endpoint_path), "%s/%d", ENDPOINT_WORKFLOWS, workflow_id);
        test_endpoint_t endpoint = {endpoint_path, NULL, "Workflow detail"};
        test_endpoint_schema_impl(&endpoint);
    } else {
        TEST_IGNORE_MESSAGE("No workflows found to test detail endpoint");
    }
}
*/

// TODO
/*
void test_collection_detail_endpoint(void) {
    int collection_id = get_first_item_id(ENDPOINT_COLLECTIONS, FIELD_COLLECTIONS);
    
    if (collection_id > 0) {
        char endpoint_path[256];
        snprintf(endpoint_path, sizeof(endpoint_path), "%s/%d", ENDPOINT_COLLECTIONS, collection_id);
        test_endpoint_t endpoint = {endpoint_path, NULL, "Collection detail"};
        test_endpoint_schema_impl(&endpoint);
    } else {
        TEST_IGNORE_MESSAGE("No collections found to test detail endpoint");
    }
}
*/

void test_workflow_field_types(void) {
    char url[MAX_URL_LENGTH];
    snprintf(url, sizeof(url), "%s%s?limit=%d", LOCAL_BASE_URL, ENDPOINT_SEARCH, SINGLE_RESULT_LIMIT);
    json_t *result = http_get(url);
    
    TEST_ASSERT_NOT_NULL(result);
    json_t *workflows = json_object_get(result, FIELD_WORKFLOWS);
    TEST_ASSERT_NOT_NULL(workflows);
    
    if (json_array_size(workflows) > 0) {
        json_t *workflow = json_array_get(workflows, 0);
        
        // Check required fields and types
        assert_field_type(workflow, FIELD_ID, JSON_INTEGER, "workflow");
        assert_field_type(workflow, FIELD_NAME, JSON_STRING, "workflow");
        assert_field_type(workflow, FIELD_TOTAL_VIEWS, JSON_INTEGER, "workflow");
        assert_field_type(workflow, FIELD_DESCRIPTION, JSON_STRING, "workflow");
        assert_field_type(workflow, FIELD_CREATED_AT, JSON_STRING, "workflow");
        assert_field_type(workflow, FIELD_NODES, JSON_ARRAY, "workflow");
        
        // Check user object
        json_t *user = json_object_get(workflow, FIELD_USER);
        TEST_ASSERT_NOT_NULL(user);
        TEST_ASSERT_TRUE(json_is_object(user));
        assert_field_type(user, FIELD_USERNAME, JSON_STRING, "user");
        assert_field_type(user, FIELD_VERIFIED, JSON_INTEGER, "user"); // JSON_FALSE covers both true/false
    }
    
    json_decref(result);
}

// Wait for server with timeout
bool wait_for_server(const char *base_url, int timeout_seconds) {
    char health_url[MAX_URL_LENGTH];
    snprintf(health_url, sizeof(health_url), "%s%s", base_url, ENDPOINT_HEALTH);
    
    time_t start_time = time(NULL);
    
    while (time(NULL) - start_time < timeout_seconds) {
        json_t *response = http_get(health_url);
        if (response) {
            json_decref(response);
            return true;
        }
        sleep(1);
    }
    
    return false;
}

// Setup and teardown
void setUp(void) {
    if (g_config.curl) {
        curl_easy_reset(g_config.curl);
    }
}

void tearDown(void) {
    // Nothing specific needed per test
}

// Parse command line arguments
static void parse_arguments(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--upstream") == 0) {
            g_config.test_upstream = true;
            printf("Upstream comparison enabled\n");
        } else if (strcmp(argv[i], "--verbose") == 0) {
            g_config.verbose_mode = true;
            printf("Verbose mode enabled\n");
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--upstream] [--verbose]\n", argv[0]);
            printf("Options:\n");
            printf("  --upstream  Compare against real n8n.io API\n");
            printf("  --verbose   Print detailed JSON comparison output\n");
            exit(0);
        }
    }
}

int main(int argc, char *argv[]) {
    parse_arguments(argc, argv);
    
    // Initialize CURL globally
    curl_global_init(CURL_GLOBAL_DEFAULT);
    g_config.curl = curl_easy_init();
    
    if (!g_config.curl) {
        fprintf(stderr, "Failed to initialize CURL\n");
        return 1;
    }
    
    // Wait for local server
    printf("Waiting for local server at %s...\n", LOCAL_BASE_URL);
    if (!wait_for_server(LOCAL_BASE_URL, SERVER_WAIT_TIMEOUT)) {
        fprintf(stderr, "Local server not responding. Please start nrest-api first.\n");
        curl_easy_cleanup(g_config.curl);
        curl_global_cleanup();
        return 1;
    }
    printf("Local server is ready\n");
    
    // Test upstream connectivity if needed
    if (g_config.test_upstream) {
        printf("Testing upstream connectivity to %s...\n", UPSTREAM_BASE_URL);
        if (!wait_for_server(UPSTREAM_BASE_URL, UPSTREAM_WAIT_TIMEOUT)) {
            printf("Warning: Upstream server not accessible. Tests will run without comparison.\n");
            g_config.test_upstream = false;
        } else {
            printf("Upstream server is accessible\n");
        }
    }
    
    // Run Unity tests
    UNITY_BEGIN();
    
    // Basic endpoint tests
    RUN_TEST(test_health_endpoint);
    RUN_TEST(test_categories_endpoint);
    RUN_TEST(test_collections_endpoint);
    RUN_TEST(test_collections_with_search);
    RUN_TEST(test_search_endpoint_basic);
    RUN_TEST(test_search_endpoint_with_category);
    
    // Detail endpoint tests (require data)
    RUN_TEST(test_workflow_detail_endpoint);
    RUN_TEST(test_collection_detail_endpoint);
    
    // Field validation tests
    RUN_TEST(test_workflow_field_types);
    
    int result = UNITY_END();
    
    // Cleanup
    curl_easy_cleanup(g_config.curl);
    curl_global_cleanup();
    
    return result;
}
