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
#include <ulfius.h>
#include <sqlite3.h>
#include <jansson.h>

// Default values if not provided by Makefile
#ifndef PORT
#define PORT 8080
#endif

#ifndef DATABASE_FILE
#define DATABASE_FILE "workflow_templates.db"
#endif

// Buffer size constants to replace magic numbers
#define MAX_SQL_BUFFER_SIZE 4096
#define MEDIUM_SQL_BUFFER_SIZE 3072
#define SMALL_SQL_BUFFER_SIZE 2048
#define XSMALL_SQL_BUFFER_SIZE 1024
#define MAX_CATEGORIES 50
#define PARAM_NAME_BUFFER_SIZE 32
#define SEARCH_PATTERN_BUFFER_SIZE 256
#define CATEGORY_BUFFER_SIZE 512

// Ulfius framework uses signature methods in order to identify endpoints,
// mark parameters as unused if necessary
#define UNUSED(x) (void)(x)

// Global database connection
sqlite3 *db;

// Forward declarations
int callback_options(const struct _u_request *request, struct _u_response *response, void *user_data);
int callback_get_health(const struct _u_request *request, struct _u_response *response, void *user_data);
int callback_get_categories(const struct _u_request *request, struct _u_response *response, void *user_data);
int callback_get_collections(const struct _u_request *request, struct _u_response *response, void *user_data);
int callback_get_collection_by_id(const struct _u_request *request, struct _u_response *response, void *user_data);
int callback_search_templates(const struct _u_request *request, struct _u_response *response, void *user_data);
int callback_get_workflow_by_id(const struct _u_request *request, struct _u_response *response, void *user_data);
int callback_create_workflow(const struct _u_request *request, struct _u_response *response, void *user_data);
int callback_get_all_workflows(const struct _u_request *request, struct _u_response *response, void *user_data);
int callback_create_collection(const struct _u_request *request, struct _u_response *response, void *user_data);
int callback_add_workflow_to_collection(const struct _u_request *request, struct _u_response *response, void *user_data);

// Initialize database and create tables with proper schema
int init_database() {
    int rc = sqlite3_open(DATABASE_FILE, &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    printf("Database initialized successfully\n");
    return 0;
}

// Utility function to parse integer parameter with default
int get_int_param(const struct _u_request *request, const char *param_name, int default_value) {
    const char *param_str = u_map_get(request->map_url, param_name);
    if (param_str && strlen(param_str) > 0) {
        int value = atoi(param_str);
        return value > 0 ? value : default_value;
    }
    return default_value;
}

// Get or create user if user does not exist
int get_or_create_user(sqlite3 *db, json_t *user_json) {
    if (!db) {
        fprintf(stderr, "get_or_create_user ERROR: Invalid database.\n");
        return 0;
    }

    if (!json_is_object(user_json)) {
        fprintf(stderr, "get_or_create_user ERROR: Invalid input.\n");
        return 0;
    }

    /*
    const char *username = json_string_value(json_object_get(user_json, "username"));
    if (!username || strlen(username) == 0) {
        fprintf(stderr, "get_or_create_user ERROR: 'username' is a required field in the user object.\n");
        return 0; // Username is mandatory for lookup or creation
    }
    */

   const char *username = "Default API User";

    int user_id = 1;

    // Check if user already exists
    const char *user_check_sql = "SELECT id FROM users WHERE username = ?;";
    sqlite3_stmt *user_stmt;
    if (sqlite3_prepare_v2(db, user_check_sql, -1, &user_stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "get_or_create_user ERROR: Failed to prepare user check statement: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(user_stmt, 1, username, -1, SQLITE_STATIC);
    int step_result = sqlite3_step(user_stmt);

    if (step_result == SQLITE_ROW) {
        user_id = sqlite3_column_int(user_stmt, 0);
        sqlite3_finalize(user_stmt); // Clean up immediately
    } else if (step_result == SQLITE_DONE) {
        // User does not exist, so create them
        sqlite3_finalize(user_stmt); // Clean up check statement

        const char *user_name = json_string_value(json_object_get(user_json, "name"));
        const char *user_bio = json_string_value(json_object_get(user_json, "bio"));
        json_t *verified_json = json_object_get(user_json, "verified");
        json_t *links_json = json_object_get(user_json, "links");
        const char *avatar = json_string_value(json_object_get(user_json, "avatar"));

        const char *create_user_sql = "INSERT INTO users (name, username, bio, verified, links, avatar) VALUES (?, ?, ?, ?, ?, ?);";
        sqlite3_stmt *create_stmt;
        if (sqlite3_prepare_v2(db, create_user_sql, -1, &create_stmt, 0) == SQLITE_OK) {
            sqlite3_bind_text(create_stmt, 1, user_name ? user_name : username, -1, SQLITE_STATIC);
            sqlite3_bind_text(create_stmt, 2, username, -1, SQLITE_STATIC);
            sqlite3_bind_text(create_stmt, 3, user_bio ? user_bio : "", -1, SQLITE_STATIC);
            sqlite3_bind_int(create_stmt, 4, json_is_true(verified_json) ? 1 : 0);
            
            char *links_str = links_json ? json_dumps(links_json, JSON_COMPACT) : NULL;
            sqlite3_bind_text(create_stmt, 5, links_str ? links_str : "[]", -1, SQLITE_TRANSIENT);
            
            sqlite3_bind_text(create_stmt, 6, avatar ? avatar : "", -1, SQLITE_STATIC);
            
            if (sqlite3_step(create_stmt) == SQLITE_DONE) {
                user_id = sqlite3_last_insert_rowid(db);
            } else {
                 fprintf(stderr, "get_or_create_user ERROR: Failed to insert new user: %s\n", sqlite3_errmsg(db));
            }
            
            sqlite3_finalize(create_stmt);
            if (links_str) free(links_str);
        } else {
             fprintf(stderr, "get_or_create_user ERROR: Failed to prepare create user statement: %s\n", sqlite3_errmsg(db));
        }
    } else {
        // An actual error occurred during the SELECT step
        fprintf(stderr, "get_or_create_user ERROR: Failed to step on user check statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(user_stmt);
    }
    
    return user_id;
}

// Get or create category if user does not exist
int get_or_create_category(json_t *category_json) {
    if (!json_is_object(category_json)) {
        return 0;
    }

    const char *name = json_string_value(json_object_get(category_json, "name"));
    if (!name || strlen(name) == 0) {
        fprintf(stderr, "get_or_create_category: Category name is missing or empty.\n");
        return 0; // Name is a required field
    }

    // Check if a category with this name already exists.
    sqlite3_stmt *stmt;
    const char *sql_select = "SELECT id FROM categories WHERE name = ?;";
    if (sqlite3_prepare_v2(db, sql_select, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "get_or_create_category: Failed to prepare select category statement: %s\n", sqlite3_errmsg(db));
        return 0;
    }
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);

    int category_id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        category_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (category_id > 0) {
        return category_id; // Category already exists, return its ID.
    }

    // Category does not exist, so we need to create it.
    // First, resolve the parent category if one is specified.
    int parent_id = 0;
    json_t *parent_json = json_object_get(category_json, "parent");
    if (json_is_object(parent_json)) {
        // Recursively call this function to get or create the parent.
        parent_id = get_or_create_category(parent_json);
    }

    const char *icon = json_string_value(json_object_get(category_json, "icon"));

    // Prepare to insert the new category. We let the database handle the ID.
    const char *sql_insert = "INSERT INTO categories (name, icon, parent_id) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "get_or_create_category: Failed to prepare insert category statement: %s\n", sqlite3_errmsg(db));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, icon ? icon : "🏷️", -1, SQLITE_STATIC); // Provide a default icon
    if (parent_id > 0) {
        sqlite3_bind_int(stmt, 3, parent_id);
    } else {
        sqlite3_bind_null(stmt, 3);
    }

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        category_id = sqlite3_last_insert_rowid(db);
    } else {
        // This could fail due to a race condition (another request inserted it).
        // The UNIQUE constraint on 'name' would be violated. We can try selecting again.
        fprintf(stderr, "get_or_create_category: Insert failed for '%s', retrying select. Error: %s\n", name, sqlite3_errmsg(db));
        
        const char *sql_reselect = "SELECT id FROM categories WHERE name = ?;";
        sqlite3_stmt *reselect_stmt;
        if (sqlite3_prepare_v2(db, sql_reselect, -1, &reselect_stmt, 0) == SQLITE_OK) {
            sqlite3_bind_text(reselect_stmt, 1, name, -1, SQLITE_STATIC);
            if (sqlite3_step(reselect_stmt) == SQLITE_ROW) {
                category_id = sqlite3_column_int(reselect_stmt, 0);
            }
            sqlite3_finalize(reselect_stmt);
        }
    }
    sqlite3_finalize(stmt);

    return category_id;
}

// Get categories for a template
json_t* get_template_categories(int template_id) {
    const char *sql = "SELECT c.id, c.name FROM categories c "
                      "JOIN template_categories tc ON c.id = tc.category_id "
                      "WHERE tc.template_id = ?;";
    sqlite3_stmt *stmt;
    json_t *categories_array = json_array();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, template_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json_t *category_obj = json_object();
            json_object_set_new(category_obj, "id", json_integer(sqlite3_column_int(stmt, 0)));
            json_object_set_new(category_obj, "name", json_string((const char*)sqlite3_column_text(stmt, 1)));
            json_array_append_new(categories_array, category_obj);
        }
        sqlite3_finalize(stmt);
    }
    return categories_array;
}

// Get categories for a collection
json_t* get_collection_categories(int collection_id) {
    const char *sql = "SELECT c.id, c.name FROM categories c "
                      "JOIN collection_categories cc ON c.id = cc.category_id "
                      "WHERE cc.collection_id = ?;";
    sqlite3_stmt *stmt;
    json_t *categories_array = json_array();

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, collection_id);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            json_t *category_obj = json_object();
            json_object_set_new(category_obj, "id", json_integer(sqlite3_column_int(stmt, 0)));
            json_object_set_new(category_obj, "name", json_string((const char*)sqlite3_column_text(stmt, 1)));
            json_array_append_new(categories_array, category_obj);
        }
        sqlite3_finalize(stmt);
    }
    return categories_array;
}

// GET /health
int callback_get_health(const struct _u_request *request, struct _u_response *response, void *user_data) {
    UNUSED(request);
    UNUSED(user_data);

    json_t *health_object = json_object();
    json_object_set_new(health_object, "status", json_string("OK"));
    
    ulfius_set_json_body_response(response, 200, health_object);
    json_decref(health_object);
    
    return U_CALLBACK_CONTINUE;
}

// GET /templates/categories
int callback_get_categories(const struct _u_request *request, struct _u_response *response, void *user_data) {
    UNUSED(request);
    UNUSED(user_data);

    const char *sql = 
        "SELECT c.id, c.name, c.icon, p.id AS parent_id, p.name AS parent_name, p.icon AS parent_icon "
        "FROM categories c "
        "LEFT JOIN categories p ON c.parent_id = p.id "
        "ORDER BY c.name;";
    sqlite3_stmt *stmt;
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        ulfius_set_string_body_response(response, 500, "Database error");
        return U_CALLBACK_CONTINUE;
    }
    
    json_t *categories_array = json_array();
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        json_t *category = json_object();
        json_object_set_new(category, "id", json_integer(sqlite3_column_int(stmt, 0)));
        json_object_set_new(category, "name", json_string((const char*)sqlite3_column_text(stmt, 1)));
        json_object_set_new(category, "icon", json_string((const char*)sqlite3_column_text(stmt, 2)));

        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            json_t *parent_obj = json_object();
            json_object_set_new(parent_obj, "id", json_integer(sqlite3_column_int(stmt, 3)));
            json_object_set_new(parent_obj, "name", json_string((const char*)sqlite3_column_text(stmt, 4)));
            json_object_set_new(parent_obj, "icon", json_string((const char*)sqlite3_column_text(stmt, 5)));
            json_object_set_new(category, "parent", parent_obj);
        } else {
            json_object_set_new(category, "parent", json_null());
        }
        
        json_array_append_new(categories_array, category);
    }
    
    sqlite3_finalize(stmt);

    json_t *response_json = json_object();
    json_object_set_new(response_json, "categories", categories_array);
    
    ulfius_set_json_body_response(response, 200, response_json);
    json_decref(response_json);
    
    return U_CALLBACK_CONTINUE;
}

// GET /templates/collections
int callback_get_collections(const struct _u_request *request, struct _u_response *response, void *user_data) {
    UNUSED(user_data);

    const char *search_query = u_map_get(request->map_url, "search");
    
    // Parse category parameters (category[] array)
    int category_ids[MAX_CATEGORIES];
    int category_count = 0;
    char param_name[PARAM_NAME_BUFFER_SIZE];
    for (int i = 0; i < MAX_CATEGORIES; i++) {
        snprintf(param_name, sizeof(param_name), "category[%d]", i);
        const char *value = u_map_get(request->map_url, param_name);
        if (value) {
            category_ids[category_count++] = atoi(value);
        }
    }
    
    // Build dynamic SQL query
    char main_sql[SMALL_SQL_BUFFER_SIZE];
    strcpy(main_sql, "SELECT DISTINCT c.id, c.rank, c.name, c.description, c.total_views, c.created_at FROM collections c");
    
    int where_added = 0;
    
    // Add category join if needed
    if (category_count > 0) {
        strcat(main_sql, " JOIN collection_categories cc ON c.id = cc.collection_id");
        strcat(main_sql, " WHERE cc.category_id IN (");
        for (int i = 0; i < category_count; i++) {
            if (i > 0) strcat(main_sql, ",");
            strcat(main_sql, "?");
        }
        strcat(main_sql, ")");
        where_added = 1;
    }
    
    // Add search condition
    if (search_query && strlen(search_query) > 0) {
        if (where_added) {
            strcat(main_sql, " AND ");
        } else {
            strcat(main_sql, " WHERE ");
        }
        strcat(main_sql, "c.name LIKE ?");
    }
    
    strcat(main_sql, " ORDER BY c.rank, c.name;");
    
    sqlite3_stmt *main_stmt;
    if (sqlite3_prepare_v2(db, main_sql, -1, &main_stmt, 0) != SQLITE_OK) {
        ulfius_set_string_body_response(response, 500, "Database error on prepare");
        return U_CALLBACK_CONTINUE;
    }
    
    int param_idx = 1;
    
    // Bind category parameters
    for (int i = 0; i < category_count; i++) {
        sqlite3_bind_int(main_stmt, param_idx++, category_ids[i]);
    }
    
    // Bind search parameter
    if (search_query && strlen(search_query) > 0) {
        char search_pattern[SEARCH_PATTERN_BUFFER_SIZE];
        snprintf(search_pattern, sizeof(search_pattern), "%%%s%%", search_query);
        sqlite3_bind_text(main_stmt, param_idx++, search_pattern, -1, SQLITE_STATIC);
    }

    json_t *collections_array = json_array();
    while (sqlite3_step(main_stmt) == SQLITE_ROW) {
        int collection_id = sqlite3_column_int(main_stmt, 0);
        json_t *collection_obj = json_object();
        json_object_set_new(collection_obj, "id", json_integer(collection_id));
        json_object_set_new(collection_obj, "rank", json_integer(sqlite3_column_int(main_stmt, 1)));
        json_object_set_new(collection_obj, "name", json_string((const char *)sqlite3_column_text(main_stmt, 2)));
        
        // Handle nullable totalViews
        if (sqlite3_column_type(main_stmt, 4) != SQLITE_NULL) {
            json_object_set_new(collection_obj, "totalViews", json_integer(sqlite3_column_int(main_stmt, 4)));
        } else {
            json_object_set_new(collection_obj, "totalViews", json_null());
        }
        
        json_object_set_new(collection_obj, "createdAt", json_string((const char *)sqlite3_column_text(main_stmt, 5)));
        
        // Get workflows for this collection
        json_t *workflows_array = json_array();
        const char *workflow_sql = "SELECT template_id FROM collection_workflows WHERE collection_id = ? ORDER BY template_id;";
        sqlite3_stmt *workflow_stmt;
        
        if (sqlite3_prepare_v2(db, workflow_sql, -1, &workflow_stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(workflow_stmt, 1, collection_id);
            while (sqlite3_step(workflow_stmt) == SQLITE_ROW) {
                json_t *workflow_ref = json_object();
                json_object_set_new(workflow_ref, "id", json_integer(sqlite3_column_int(workflow_stmt, 0)));
                json_array_append_new(workflows_array, workflow_ref);
            }
            sqlite3_finalize(workflow_stmt);
        }
        
        json_object_set_new(collection_obj, "workflows", workflows_array);
        
        // Add empty nodes array to match the expected structure
        json_object_set_new(collection_obj, "nodes", json_array());
        
        json_array_append_new(collections_array, collection_obj);
    }
    sqlite3_finalize(main_stmt);
    
    // Wrap in a root object with "collections" key to match the expected structure
    json_t *response_json = json_object();
    json_object_set_new(response_json, "collections", collections_array);
    
    ulfius_set_json_body_response(response, 200, response_json);
    json_decref(response_json);
    return U_CALLBACK_CONTINUE;
}

// GET /templates/collections/<id>
int callback_get_collection_by_id(const struct _u_request *request, struct _u_response *response, void *user_data) {
    UNUSED(user_data);

    const char *id_str = u_map_get(request->map_url, "id");
    if (id_str == NULL) {
        ulfius_set_string_body_response(response, 400, "Missing collection ID");
        return U_CALLBACK_CONTINUE;
    }
    
    int collection_id = atoi(id_str);
    
    // Get collection basic info with description
    const char *sql = "SELECT id, name, description, total_views, created_at, rank FROM collections WHERE id = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        ulfius_set_string_body_response(response, 500, "Database error");
        return U_CALLBACK_CONTINUE;
    }
    
    sqlite3_bind_int(stmt, 1, collection_id);
    
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        json_t *root_obj = json_object();
        json_t *collection_obj = json_object();
        json_error_t error;
        
        json_object_set_new(collection_obj, "id", json_integer(sqlite3_column_int(stmt, 0)));
        json_object_set_new(collection_obj, "name", json_string((const char*)sqlite3_column_text(stmt, 1)));
        
        // Add description
        if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
            json_object_set_new(collection_obj, "description", json_string((const char*)sqlite3_column_text(stmt, 2)));
        } else {
            json_object_set_new(collection_obj, "description", json_string(""));
        }
        
        if (sqlite3_column_type(stmt, 3) != SQLITE_NULL) {
            json_object_set_new(collection_obj, "totalViews", json_integer(sqlite3_column_int(stmt, 3)));
        } else {
            json_object_set_new(collection_obj, "totalViews", json_integer(0));
        }
        
        json_object_set_new(collection_obj, "createdAt", json_string((const char*)sqlite3_column_text(stmt, 4)));
        
        sqlite3_finalize(stmt);
        
        // Get workflows with full details
        json_t *workflows_array = json_array();
        const char *workflow_sql = 
            "SELECT t.id, t.name, t.total_views, t.recent_views, t.created_at, t.description, "
            "t.workflow_data, t.last_updated_by, "
            "u.id, u.name, u.username, u.bio, u.verified, u.links, u.avatar, "
            "t.nodes_data, t.workflow_info, t.image_data "
            "FROM templates t "
            "JOIN collection_workflows cw ON t.id = cw.template_id "
            "JOIN users u ON t.user_id = u.id "
            "WHERE cw.collection_id = ? "
            "ORDER BY t.id;";
        
        sqlite3_stmt *workflow_stmt;
        if (sqlite3_prepare_v2(db, workflow_sql, -1, &workflow_stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(workflow_stmt, 1, collection_id);
            
            while (sqlite3_step(workflow_stmt) == SQLITE_ROW) {
                json_t *workflow_obj = json_object();
                
                int template_id = sqlite3_column_int(workflow_stmt, 0);
                json_object_set_new(workflow_obj, "id", json_integer(template_id));
                json_object_set_new(workflow_obj, "name", json_string((const char*)sqlite3_column_text(workflow_stmt, 1)));
                
                int views = sqlite3_column_int(workflow_stmt, 2);
                json_object_set_new(workflow_obj, "views", json_integer(views));
                json_object_set_new(workflow_obj, "recentViews", json_integer(sqlite3_column_int(workflow_stmt, 3)));
                json_object_set_new(workflow_obj, "totalViews", json_integer(views));
                json_object_set_new(workflow_obj, "createdAt", json_string((const char*)sqlite3_column_text(workflow_stmt, 4)));
                json_object_set_new(workflow_obj, "description", json_string((const char*)sqlite3_column_text(workflow_stmt, 5)));
                
                // Add nested workflow object
                const char *workflow_data_str = (const char*)sqlite3_column_text(workflow_stmt, 6);
                if (workflow_data_str) {
                    json_t *nested_workflow = json_loads(workflow_data_str, 0, &error);
                    if (nested_workflow) {
                        json_object_set_new(workflow_obj, "workflow", nested_workflow);
                    } else {
                        json_object_set_new(workflow_obj, "workflow", json_object());
                    }
                } else {
                    json_object_set_new(workflow_obj, "workflow", json_object());
                }
                
                // Add lastUpdatedBy
                if (sqlite3_column_type(workflow_stmt, 7) != SQLITE_NULL) {
                    json_object_set_new(workflow_obj, "lastUpdatedBy", json_integer(sqlite3_column_int(workflow_stmt, 7)));
                } else {
                    json_object_set_new(workflow_obj, "lastUpdatedBy", json_integer(sqlite3_column_int(workflow_stmt, 8)));
                }
                
                // Add workflowInfo
                const char *workflow_info_str = (const char*)sqlite3_column_text(workflow_stmt, 16);
                if (workflow_info_str) {
                    json_t *workflow_info = json_loads(workflow_info_str, 0, &error);
                    if (workflow_info) {
                        json_object_set_new(workflow_obj, "workflowInfo", workflow_info);
                    } else {
                        json_object_set_new(workflow_obj, "workflowInfo", json_object());
                    }
                } else {
                    json_object_set_new(workflow_obj, "workflowInfo", json_object());
                }
                
                // Build user object
                json_t *user_obj = json_object();
                json_object_set_new(user_obj, "name", json_string((const char*)sqlite3_column_text(workflow_stmt, 9)));
                json_object_set_new(user_obj, "username", json_string((const char*)sqlite3_column_text(workflow_stmt, 10)));
                
                if (sqlite3_column_type(workflow_stmt, 11) != SQLITE_NULL) {
                    json_object_set_new(user_obj, "bio", json_string((const char*)sqlite3_column_text(workflow_stmt, 11)));
                } else {
                    json_object_set_new(user_obj, "bio", json_null());
                }
                
                json_object_set_new(user_obj, "verified", json_boolean(sqlite3_column_int(workflow_stmt, 12)));
                
                const char *links_str = (const char*)sqlite3_column_text(workflow_stmt, 13);
                if (links_str) {
                    json_t *links_json = json_loads(links_str, 0, &error);
                    json_object_set_new(user_obj, "links", links_json ? links_json : json_array());
                } else {
                    json_object_set_new(user_obj, "links", json_array());
                }
                
                json_object_set_new(user_obj, "avatar", json_string((const char*)sqlite3_column_text(workflow_stmt, 14)));
                json_object_set_new(workflow_obj, "user", user_obj);
                
                // Add nodes
                const char *nodes_str = (const char*)sqlite3_column_text(workflow_stmt, 15);
                if (nodes_str) {
                    json_t *nodes_json = json_loads(nodes_str, 0, &error);
                    json_object_set_new(workflow_obj, "nodes", nodes_json ? nodes_json : json_array());
                } else {
                    json_object_set_new(workflow_obj, "nodes", json_array());
                }
                
                // Get categories for this workflow
                json_object_set_new(workflow_obj, "categories", get_template_categories(template_id));
                
                // Add image array
                const char *image_str = (const char*)sqlite3_column_text(workflow_stmt, 17);
                if (image_str) {
                    json_t *image_json = json_loads(image_str, 0, &error);
                    json_object_set_new(workflow_obj, "image", image_json ? image_json : json_array());
                } else {
                    json_object_set_new(workflow_obj, "image", json_array());
                }
                
                json_array_append_new(workflows_array, workflow_obj);
            }
            sqlite3_finalize(workflow_stmt);
        }
        
        json_object_set_new(collection_obj, "workflows", workflows_array);
        json_object_set_new(collection_obj, "nodes", json_array());
        
        // Get categories for the collection
        json_object_set_new(collection_obj, "categories", get_collection_categories(collection_id));
        
        // Add empty image array for collection
        json_object_set_new(collection_obj, "image", json_array());
        
        // Wrap in root object
        json_object_set_new(root_obj, "collection", collection_obj);
        
        ulfius_set_json_body_response(response, 200, root_obj);
        json_decref(root_obj);
    } else {
        ulfius_set_string_body_response(response, 404, "Collection not found");
        sqlite3_finalize(stmt);
    }
    
    return U_CALLBACK_CONTINUE;
}

// GET /templates/search
int callback_search_templates(const struct _u_request *request, struct _u_response *response, void *user_data) {
    UNUSED(user_data);

    const char *search_query_str = u_map_get(request->map_url, "search");
    const char *category_str = u_map_get(request->map_url, "category");
    const int default_page_size = 20;
    const int max_page_size = 100;
    int page = get_int_param(request, "page", 1);
    int limit = get_int_param(request, "limit", default_page_size);
    if (limit > max_page_size) limit = max_page_size;
    int offset = (page - 1) * limit;

    int total_workflows = 0;
    sqlite3_stmt *count_stmt;
    sqlite3_stmt *main_stmt;

    // Base queries with category joins when needed
    char count_sql_base[] = "SELECT COUNT(DISTINCT t.id) FROM templates t";
    char main_sql_base[] = "SELECT DISTINCT t.id, t.name, t.total_views, t.purchase_url, "
                           "u.id, u.name, u.username, u.bio, u.verified, u.links, u.avatar, "
                           "t.description, t.created_at, t.nodes_data, t.price "
                           "FROM templates t JOIN users u ON t.user_id = u.id";
    
    // Build WHERE clause and JOIN clause dynamically
    char join_clause[CATEGORY_BUFFER_SIZE] = "";
    char where_clause[XSMALL_SQL_BUFFER_SIZE] = "";
    int where_conditions = 0;
    
    // Parse and prepare category conditions
    char *categories[MAX_CATEGORIES]; // Max 50 categories, just to be sure
    int category_count = 0;
    char category_buffer[CATEGORY_BUFFER_SIZE];
    
    if (category_str && strlen(category_str) > 0) {
        // Create a copy of category_str for tokenization
        strncpy(category_buffer, category_str, sizeof(category_buffer) - 1);
        category_buffer[sizeof(category_buffer) - 1] = '\0';
        
        // Split by comma
        char *token = strtok(category_buffer, ",");
        while (token != NULL && category_count < MAX_CATEGORIES) {
            // Trim whitespace
            while (*token == ' ') token++;
            char *end = token + strlen(token) - 1;
            while (end > token && *end == ' ') end--;
            *(end + 1) = '\0';
            
            categories[category_count++] = token;
            token = strtok(NULL, ",");
        }
        
        if (category_count > 0) {
            // Add joins for category filtering
            strcat(join_clause, " JOIN template_categories tc ON t.id = tc.template_id");
            strcat(join_clause, " JOIN categories c ON tc.category_id = c.id");
            
            if (where_conditions == 0) {
                strcat(where_clause, " WHERE ");
            } else {
                strcat(where_clause, " AND ");
            }
            
            strcat(where_clause, "(");
            for (int i = 0; i < category_count; i++) {
                if (i > 0) {
                    strcat(where_clause, " OR ");
                }
                strcat(where_clause, "c.name = ?");
            }
            strcat(where_clause, ")");
            where_conditions++;
        }
    }
    
    // Add search condition
    if (search_query_str && strlen(search_query_str) > 0) {
        if (where_conditions == 0) {
            strcat(where_clause, " WHERE ");
        } else {
            strcat(where_clause, " AND ");
        }
        strcat(where_clause, "(t.name LIKE ? OR t.description LIKE ?)");
        where_conditions++;
    }

    char full_count_sql[MEDIUM_SQL_BUFFER_SIZE];
    snprintf(full_count_sql, sizeof(full_count_sql), "%s%s%s;", count_sql_base, join_clause, where_clause);
    
    char full_main_sql[MAX_SQL_BUFFER_SIZE];
    snprintf(full_main_sql, sizeof(full_main_sql), "%s%s%s ORDER BY t.id DESC LIMIT ? OFFSET ?;", main_sql_base, join_clause, where_clause);
    
    // Get total count
    if (sqlite3_prepare_v2(db, full_count_sql, -1, &count_stmt, 0) == SQLITE_OK) {
        int param_index = 1;
        
        // Bind category parameters first (if any)
        for (int i = 0; i < category_count; i++) {
            sqlite3_bind_text(count_stmt, param_index++, categories[i], -1, SQLITE_STATIC);
        }
        
        // Bind search parameters
        if (search_query_str && strlen(search_query_str) > 0) {
            char search_pattern[SEARCH_PATTERN_BUFFER_SIZE];
            snprintf(search_pattern, sizeof(search_pattern), "%%%s%%", search_query_str);
            sqlite3_bind_text(count_stmt, param_index++, search_pattern, -1, SQLITE_STATIC);
            sqlite3_bind_text(count_stmt, param_index++, search_pattern, -1, SQLITE_STATIC);
        }
        
        if (sqlite3_step(count_stmt) == SQLITE_ROW) {
            total_workflows = sqlite3_column_int(count_stmt, 0);
        }
        sqlite3_finalize(count_stmt);
    } else {
        ulfius_set_string_body_response(response, 500, "Database error on count query");
        return U_CALLBACK_CONTINUE;
    }

    // Get paginated results
    if (sqlite3_prepare_v2(db, full_main_sql, -1, &main_stmt, 0) != SQLITE_OK) {
        ulfius_set_string_body_response(response, 500, "Database error on main query");
        return U_CALLBACK_CONTINUE;
    }
    
    int param_index = 1;
    
    // Bind category parameters first (if any)
    for (int i = 0; i < category_count; i++) {
        sqlite3_bind_text(main_stmt, param_index++, categories[i], -1, SQLITE_STATIC);
    }
    
    // Bind search parameters
    if (search_query_str && strlen(search_query_str) > 0) {
        char search_pattern[SEARCH_PATTERN_BUFFER_SIZE];
        snprintf(search_pattern, sizeof(search_pattern), "%%%s%%", search_query_str);
        sqlite3_bind_text(main_stmt, param_index++, search_pattern, -1, SQLITE_STATIC);
        sqlite3_bind_text(main_stmt, param_index++, search_pattern, -1, SQLITE_STATIC);
    }
    
    // Bind pagination parameters
    sqlite3_bind_int(main_stmt, param_index++, limit);
    sqlite3_bind_int(main_stmt, param_index++, offset);

    json_t *response_json = json_object();
    json_object_set_new(response_json, "totalWorkflows", json_integer(total_workflows));
    json_t *workflows_array = json_array();

    while (sqlite3_step(main_stmt) == SQLITE_ROW) {
        json_t *workflow_obj = json_object();
        json_error_t error;

        // Workflow fields
        json_object_set_new(workflow_obj, "id", json_integer(sqlite3_column_int(main_stmt, 0)));
        json_object_set_new(workflow_obj, "name", json_string((const char*)sqlite3_column_text(main_stmt, 1)));
        json_object_set_new(workflow_obj, "totalViews", json_integer(sqlite3_column_int(main_stmt, 2)));
        
        // Handle nullable purchaseUrl
        if (sqlite3_column_type(main_stmt, 3) != SQLITE_NULL) {
            json_object_set_new(workflow_obj, "purchaseUrl", json_string((const char*)sqlite3_column_text(main_stmt, 3)));
        } else {
            json_object_set_new(workflow_obj, "purchaseUrl", json_null());
        }

        // User object
        json_t *user_obj = json_object();
        json_object_set_new(user_obj, "id", json_integer(sqlite3_column_int(main_stmt, 4)));
        json_object_set_new(user_obj, "name", json_string((const char*)sqlite3_column_text(main_stmt, 5)));
        json_object_set_new(user_obj, "username", json_string((const char*)sqlite3_column_text(main_stmt, 6)));
        json_object_set_new(user_obj, "bio", json_string((const char*)sqlite3_column_text(main_stmt, 7)));
        json_object_set_new(user_obj, "verified", json_boolean(sqlite3_column_int(main_stmt, 8)));
        
        const char *links_str = (const char*)sqlite3_column_text(main_stmt, 9);
        json_t *links_json = json_loads(links_str ? links_str : "[]", 0, &error);
        json_object_set_new(user_obj, "links", links_json ? links_json : json_array());

        json_object_set_new(user_obj, "avatar", json_string((const char*)sqlite3_column_text(main_stmt, 10)));
        json_object_set_new(workflow_obj, "user", user_obj);

        // Other workflow fields
        json_object_set_new(workflow_obj, "description", json_string((const char*)sqlite3_column_text(main_stmt, 11)));
        json_object_set_new(workflow_obj, "createdAt", json_string((const char*)sqlite3_column_text(main_stmt, 12)));

        // Nodes
        const char *nodes_data_str = (const char*)sqlite3_column_text(main_stmt, 13);
        json_t *nodes_json = json_loads(nodes_data_str ? nodes_data_str : "[]", 0, &error);
        json_object_set_new(workflow_obj, "nodes", nodes_json ? nodes_json : json_array());
        
        // Handle nullable price
        if (sqlite3_column_type(main_stmt, 14) != SQLITE_NULL) {
            json_object_set_new(workflow_obj, "price", json_real(sqlite3_column_double(main_stmt, 14)));
        } else {
             // Official API uses 0 for null price in lists
            json_object_set_new(workflow_obj, "price", json_integer(0));
        }

        json_array_append_new(workflows_array, workflow_obj);
    }

    sqlite3_finalize(main_stmt);
    
    json_object_set_new(response_json, "workflows", workflows_array);
    ulfius_set_json_body_response(response, 200, response_json);
    json_decref(response_json);

    return U_CALLBACK_CONTINUE;
}

// GET /templates/workflows
int callback_get_all_workflows(const struct _u_request *request, struct _u_response *response, void *user_data) {
    UNUSED(request);
    UNUSED(user_data);

    const char *sql = "SELECT id, name, total_views FROM templates;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        ulfius_set_string_body_response(response, 500, "Database error");
        return U_CALLBACK_CONTINUE;
    }

    json_t *workflows_array = json_array();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        json_t *workflow_obj = json_object();
        json_object_set_new(workflow_obj, "id", json_integer(sqlite3_column_int(stmt, 0)));
        json_object_set_new(workflow_obj, "name", json_string((const char*)sqlite3_column_text(stmt, 1)));
        json_object_set_new(workflow_obj, "totalViews", json_integer(sqlite3_column_int(stmt, 2)));
        json_array_append_new(workflows_array, workflow_obj);
    }
    sqlite3_finalize(stmt);

    ulfius_set_json_body_response(response, 200, workflows_array);
    json_decref(workflows_array);

    return U_CALLBACK_CONTINUE;
}

// GET /templates/workflows/<id>
int callback_get_workflow_by_id(const struct _u_request *request, struct _u_response *response, void *user_data) {
    UNUSED(user_data);

    const char *id_str = u_map_get(request->map_url, "id");
    if (id_str == NULL) {
        ulfius_set_string_body_response(response, 400, "Missing workflow ID");
        return U_CALLBACK_CONTINUE;
    }
    
    int template_id = atoi(id_str);
    const char *sql = "SELECT t.id, t.name, t.total_views, t.price, t.purchase_url, t.recent_views, "
                     "t.created_at, t.description, t.workflow_data, t.workflow_info, t.nodes_data, t.image_data, "
                     "t.last_updated_by, "
                     "u.id, u.name, u.username, u.bio, u.verified, u.links, u.avatar "
                     "FROM templates t "
                     "JOIN users u ON t.user_id = u.id "
                     "WHERE t.id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        ulfius_set_string_body_response(response, 500, "Database error preparing statement");
        return U_CALLBACK_CONTINUE;
    }
    
    sqlite3_bind_int(stmt, 1, template_id);
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        json_t *root_obj = json_object();
        json_error_t error;
        
        // Build the nested "workflow" object exactly like the API
        json_t *workflow_obj = json_object();
        int views = sqlite3_column_int(stmt, 2);
        
        json_object_set_new(workflow_obj, "id", json_integer(sqlite3_column_int(stmt, 0)));
        json_object_set_new(workflow_obj, "name", json_string((const char*)sqlite3_column_text(stmt, 1)));
        json_object_set_new(workflow_obj, "views", json_integer(views));
        json_object_set_new(workflow_obj, "recentViews", json_integer(sqlite3_column_int(stmt, 5)));
        json_object_set_new(workflow_obj, "totalViews", json_integer(views));
        json_object_set_new(workflow_obj, "createdAt", json_string((const char*)sqlite3_column_text(stmt, 6)));
        json_object_set_new(workflow_obj, "description", json_string((const char*)sqlite3_column_text(stmt, 7)));
        
        // Handle price and purchaseUrl
        if (sqlite3_column_type(stmt, 3) == SQLITE_NULL) {
            json_object_set_new(workflow_obj, "price", json_null());
        } else {
            json_object_set_new(workflow_obj, "price", json_real(sqlite3_column_double(stmt, 3)));
        }
        
        if (sqlite3_column_type(stmt, 4) == SQLITE_NULL) {
            json_object_set_new(workflow_obj, "purchaseUrl", json_null());
        } else {
            json_object_set_new(workflow_obj, "purchaseUrl", json_string((const char*)sqlite3_column_text(stmt, 4)));
        }

        // Add the nested workflow data
        const char *workflow_data_str = (const char*)sqlite3_column_text(stmt, 8);
        if (workflow_data_str) {
            json_t *nested_workflow_json = json_loads(workflow_data_str, 0, &error);
            if (nested_workflow_json) {
                json_object_set_new(workflow_obj, "workflow", nested_workflow_json);
            } else {
                json_object_set_new(workflow_obj, "workflow", json_object());
            }
        } else {
            json_object_set_new(workflow_obj, "workflow", json_object());
        }

        json_object_set_new(root_obj, "workflow", workflow_obj);
        
        // Add lastUpdatedBy
        if (sqlite3_column_type(stmt, 12) != SQLITE_NULL) {
            json_object_set_new(root_obj, "lastUpdatedBy", json_integer(sqlite3_column_int(stmt, 12)));
        } else {
            json_object_set_new(root_obj, "lastUpdatedBy", json_integer(sqlite3_column_int(stmt, 13))); // Use user_id as fallback
        }
        
        // Build the "user" object
        json_t *user_obj = json_object();
        json_object_set_new(user_obj, "name", json_string((const char*)sqlite3_column_text(stmt, 14)));
        json_object_set_new(user_obj, "username", json_string((const char*)sqlite3_column_text(stmt, 15)));
        json_object_set_new(user_obj, "bio", json_string((const char*)sqlite3_column_text(stmt, 16)));
        json_object_set_new(user_obj, "verified", json_boolean(sqlite3_column_int(stmt, 17)));
        json_object_set_new(user_obj, "avatar", json_string((const char*)sqlite3_column_text(stmt, 19)));
        
        // Parse links JSON
        const char *links_str = (const char*)sqlite3_column_text(stmt, 18);
        if (links_str) {
            json_t *links_json = json_loads(links_str, 0, &error);
            if (links_json) {
                json_object_set_new(user_obj, "links", links_json);
            } else {
                json_object_set_new(user_obj, "links", json_array());
            }
        } else {
            json_object_set_new(user_obj, "links", json_array());
        }
        json_object_set_new(root_obj, "user", user_obj);
        
        // Get categories
        json_object_set_new(root_obj, "categories", get_template_categories(template_id));
        
        // Add workflowInfo from stored JSON
        const char *workflow_info_str = (const char*)sqlite3_column_text(stmt, 9);
        if (workflow_info_str) {
            json_t *workflow_info_json = json_loads(workflow_info_str, 0, &error);
            if (workflow_info_json) {
                json_object_set_new(root_obj, "workflowInfo", workflow_info_json);
            } else {
                json_object_set_new(root_obj, "workflowInfo", json_object());
            }
        } else {
            json_object_set_new(root_obj, "workflowInfo", json_object());
        }
        
        // Add nodes from stored JSON
        const char *nodes_data_str = (const char*)sqlite3_column_text(stmt, 10);
        if (nodes_data_str) {
            json_t *nodes_json = json_loads(nodes_data_str, 0, &error);
            if (nodes_json && json_is_array(nodes_json)) {
                json_object_set_new(root_obj, "nodes", nodes_json);
            } else {
                json_object_set_new(root_obj, "nodes", json_array());
            }
        } else {
            json_object_set_new(root_obj, "nodes", json_array());
        }
        
        // Add image array from stored JSON
        const char *image_data_str = (const char*)sqlite3_column_text(stmt, 11);
        if (image_data_str) {
            json_t *image_json = json_loads(image_data_str, 0, &error);
            if (image_json && json_is_array(image_json)) {
                json_object_set_new(root_obj, "image", image_json);
            } else {
                json_object_set_new(root_obj, "image", json_array());
            }
        } else {
            json_object_set_new(root_obj, "image", json_array());
        }
        
        ulfius_set_json_body_response(response, 200, root_obj);
        json_decref(root_obj);
    } else if (rc == SQLITE_DONE) {
        ulfius_set_string_body_response(response, 404, "Workflow not found");
    } else {
        fprintf(stderr, "Error executing step: %s\n", sqlite3_errmsg(db));
        ulfius_set_string_body_response(response, 500, "Database error executing step");
    }
    
    sqlite3_finalize(stmt);
    return U_CALLBACK_CONTINUE;
}

// GET /workflows/templates/:id
// Needed when importing a workflow from a template
int callback_get_workflow_for_import(const struct _u_request *request, struct _u_response *response, void *user_data) {
    UNUSED(user_data);

    const char *id_str = u_map_get(request->map_url, "id");
    if (id_str == NULL) {
        ulfius_set_string_body_response(response, 400, "Missing workflow ID");
        return U_CALLBACK_CONTINUE;
    }

    int template_id = atoi(id_str);
    const char *sql = "SELECT id, name, workflow_data FROM templates WHERE id = ?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        ulfius_set_string_body_response(response, 500, "Database error preparing statement");
        return U_CALLBACK_CONTINUE;
    }

    sqlite3_bind_int(stmt, 1, template_id);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        json_t *root_obj = json_object();
        json_error_t error;

        // Set top-level id and name
        json_object_set_new(root_obj, "id", json_integer(sqlite3_column_int(stmt, 0)));
        json_object_set_new(root_obj, "name", json_string((const char*)sqlite3_column_text(stmt, 1)));

        // Get the nested workflow data
        const char *workflow_data_str = (const char*)sqlite3_column_text(stmt, 2);
        if (workflow_data_str) {
            json_t *nested_workflow_json = json_loads(workflow_data_str, 0, &error);
            if (nested_workflow_json) {
                json_object_set_new(root_obj, "workflow", nested_workflow_json);
            } else {
                // If parsing fails, add an empty object to avoid breaking the client
                fprintf(stderr, "Failed to parse workflow_data for template %d: %s\n", template_id, error.text);
                json_object_set_new(root_obj, "workflow", json_object());
            }
        } else {
            // If data is NULL, add an empty object
            json_object_set_new(root_obj, "workflow", json_object());
        }

        ulfius_set_json_body_response(response, 200, root_obj);
        json_decref(root_obj);
    } else if (rc == SQLITE_DONE) {
        ulfius_set_string_body_response(response, 404, "Workflow not found");
    } else {
        fprintf(stderr, "Error executing step for import: %s\n", sqlite3_errmsg(db));
        ulfius_set_string_body_response(response, 500, "Database error executing step");
    }

    sqlite3_finalize(stmt);
    return U_CALLBACK_CONTINUE;
}

// PUT /templates/workflows
int callback_create_workflow(const struct _u_request *request, struct _u_response *response, void *user_data) {
    UNUSED(user_data);

    json_t *json_body = ulfius_get_json_body_request(request, NULL);
    if (json_body == NULL) {
        ulfius_set_string_body_response(response, 400, "Invalid JSON");
        return U_CALLBACK_CONTINUE;
    }

    json_t *workflow_json = json_object_get(json_body, "workflow");
    json_t *user_json = json_object_get(workflow_json, "user");
    json_t *categories_json = json_object_get(workflow_json, "categories");
    json_t *workflow_info_json = json_object_get(workflow_json, "workflowInfo");
    json_t *nodes_json = json_object_get(workflow_json, "nodes");
    json_t *image_json = json_object_get(workflow_json, "image");

    if (!json_is_object(workflow_json) || !json_is_object(user_json)) {
        json_decref(json_body);
        ulfius_set_string_body_response(response, 400, "Missing 'workflow' or 'user' object in request body");
        return U_CALLBACK_CONTINUE;
    }

    // Extract workflow fields
    const char *name = json_string_value(json_object_get(workflow_json, "name"));
    const char *description = json_string_value(json_object_get(workflow_json, "description"));
    const char *created_at = json_string_value(json_object_get(workflow_json, "createdAt"));
    int total_views = json_integer_value(json_object_get(workflow_json, "totalViews"));
    int recent_views = json_integer_value(json_object_get(workflow_json, "recentViews"));
    json_t *nested_workflow = json_object_get(workflow_json, "workflow");
    
    // Optional fields
    json_t *price_json = json_object_get(workflow_json, "price");
    json_t *purchase_url_json = json_object_get(workflow_json, "purchaseUrl");

    if (!name || !description || !created_at || !nested_workflow) {
        json_decref(json_body);
        ulfius_set_string_body_response(response, 400, "Missing required fields in workflow object");
        return U_CALLBACK_CONTINUE;
    }

    // Handle user creation or lookup
    int user_id = get_or_create_user(db, user_json);
    if (user_id == 0) {
        json_decref(json_body);
        ulfius_set_string_body_response(response, 400, "Invalid or incomplete user object provided. 'username' is required.");
        return U_CALLBACK_CONTINUE;
    }

    // Convert JSON objects to strings for storage
    char* workflow_data_str = json_dumps(nested_workflow, JSON_COMPACT);
    char* workflow_info_str = workflow_info_json ? json_dumps(workflow_info_json, JSON_COMPACT) : NULL;
    char* nodes_data_str = nodes_json ? json_dumps(nodes_json, JSON_COMPACT) : NULL;
    char* image_data_str = image_json ? json_dumps(image_json, JSON_COMPACT) : NULL;
    
    // Get lastUpdatedBy value
    int last_updated_by = user_id;

    // Insert or replace the template
    const char *sql = "INSERT OR REPLACE INTO templates (id, name, description, created_at, total_views, recent_views, price, purchase_url, user_id, last_updated_by, workflow_data, workflow_info, nodes_data, image_data) "
                     "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        json_decref(json_body);
        if (workflow_data_str) free(workflow_data_str);
        if (workflow_info_str) free(workflow_info_str);
        if (nodes_data_str) free(nodes_data_str);
        if (image_data_str) free(image_data_str);
        ulfius_set_string_body_response(response, 500, "Database error on prepare");
        return U_CALLBACK_CONTINUE;
    }
    
    int template_id = json_integer_value(json_object_get(workflow_json, "id"));
    if (template_id <= 0) {
        template_id = 0; // Let SQLite auto-increment if ID is not provided
    }
    
    sqlite3_bind_int(stmt, 1, template_id);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, description, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, created_at, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, total_views);
    sqlite3_bind_int(stmt, 6, recent_views);
    
    if (price_json && !json_is_null(price_json)) {
        sqlite3_bind_double(stmt, 7, json_real_value(price_json));
    } else {
        sqlite3_bind_null(stmt, 7);
    }
    
    if (purchase_url_json && !json_is_null(purchase_url_json)) {
        sqlite3_bind_text(stmt, 8, json_string_value(purchase_url_json), -1, SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 8);
    }
    
    sqlite3_bind_int(stmt, 9, user_id);
    sqlite3_bind_int(stmt, 10, last_updated_by);
    sqlite3_bind_text(stmt, 11, workflow_data_str, -1, SQLITE_TRANSIENT);
    
    if (workflow_info_str) sqlite3_bind_text(stmt, 12, workflow_info_str, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 12);
    
    if (nodes_data_str) sqlite3_bind_text(stmt, 13, nodes_data_str, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 13);
    
    if (image_data_str) sqlite3_bind_text(stmt, 14, image_data_str, -1, SQLITE_TRANSIENT);
    else sqlite3_bind_null(stmt, 14);
    
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        if (template_id == 0) {
            template_id = sqlite3_last_insert_rowid(db);
        }
        
        // Add category if it does not already exist
        /*
        const char *delete_cat_sql = "DELETE FROM template_categories WHERE template_id = ?;";
        sqlite3_stmt *delete_stmt;
        if (sqlite3_prepare_v2(db, delete_cat_sql, -1, &delete_stmt, 0) == SQLITE_OK) {
            sqlite3_bind_int(delete_stmt, 1, template_id);
            sqlite3_step(delete_stmt);
            sqlite3_finalize(delete_stmt);
        }
        */

        if (json_is_array(categories_json)) {
            size_t index;
            json_t *category_json_obj;
            json_array_foreach(categories_json, index, category_json_obj) {
                int category_id = get_or_create_category(category_json_obj);
                if (category_id > 0) {
                    const char *link_sql = "INSERT OR IGNORE INTO template_categories (template_id, category_id) VALUES (?, ?);";
                    sqlite3_stmt *link_stmt;
                    if (sqlite3_prepare_v2(db, link_sql, -1, &link_stmt, 0) == SQLITE_OK) {
                        sqlite3_bind_int(link_stmt, 1, template_id);
                        sqlite3_bind_int(link_stmt, 2, category_id);
                        sqlite3_step(link_stmt);
                        sqlite3_finalize(link_stmt);
                    }
                }
            }
        }

        json_t *response_json = json_object();
        json_object_set_new(response_json, "id", json_integer(template_id));
        // json_object_set_new(response_json, "message", "Workflow created/updated successfully");
        ulfius_set_json_body_response(response, 201, response_json);
        json_decref(response_json);
    } else {
        const char *db_error_msg = sqlite3_errmsg(db);
        fprintf(stderr, "Failed to create workflow: %s\n", db_error_msg);
        ulfius_set_string_body_response(response, 500, db_error_msg);
    }

    sqlite3_finalize(stmt);
    json_decref(json_body);
    
    if (workflow_data_str) free(workflow_data_str);
    if (workflow_info_str) free(workflow_info_str);
    if (nodes_data_str) free(nodes_data_str);
    if (image_data_str) free(image_data_str);
    
    return U_CALLBACK_CONTINUE;
}

// PUT /templates/collections
int callback_create_collection(const struct _u_request *request, struct _u_response *response, void *user_data) {
    UNUSED(user_data);

    json_t *json_body = ulfius_get_json_body_request(request, NULL);
    if (json_body == NULL) {
        ulfius_set_string_body_response(response, 400, "Invalid JSON");
        return U_CALLBACK_CONTINUE;
    }

    const char *name = json_string_value(json_object_get(json_body, "name"));
    json_t *rank_json = json_object_get(json_body, "rank");
    json_t *total_views_json = json_object_get(json_body, "totalViews");
    json_t *workflows_json = json_object_get(json_body, "workflows");

    if (!name || strlen(name) == 0) {
        json_decref(json_body);
        ulfius_set_string_body_response(response, 400, "Missing required field: name");
        return U_CALLBACK_CONTINUE;
    }

    const char *created_at = json_string_value(json_object_get(json_body, "createdAt"));
    if (!created_at) {
        json_decref(json_body);
        ulfius_set_string_body_response(response, 400, "Missing required field: createdAt");
        return U_CALLBACK_CONTINUE;
    }

    // Insert collection
    const char *sql = "INSERT INTO collections (rank, name, total_views, created_at) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        json_decref(json_body);
        ulfius_set_string_body_response(response, 500, "Database error on prepare");
        return U_CALLBACK_CONTINUE;
    }
    
    // Bind parameters
    if (rank_json && json_is_integer(rank_json)) {
        sqlite3_bind_int(stmt, 1, json_integer_value(rank_json));
    } else {
        sqlite3_bind_int(stmt, 1, 0);
    }
    
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_STATIC);
    
    if (total_views_json && json_is_integer(total_views_json)) {
        sqlite3_bind_int(stmt, 3, json_integer_value(total_views_json));
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    
    sqlite3_bind_text(stmt, 4, created_at, -1, SQLITE_STATIC);
    
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        int collection_id = sqlite3_last_insert_rowid(db);
        sqlite3_finalize(stmt);
        
        // Add workflows if provided
        if (json_is_array(workflows_json)) {
            size_t index;
            json_t *workflow;
            json_array_foreach(workflows_json, index, workflow) {
                int workflow_id = json_integer_value(json_object_get(workflow, "id"));
                if (workflow_id > 0) {
                    const char *link_sql = "INSERT OR IGNORE INTO collection_workflows (collection_id, template_id) VALUES (?, ?);";
                    sqlite3_stmt *link_stmt;
                    if (sqlite3_prepare_v2(db, link_sql, -1, &link_stmt, 0) == SQLITE_OK) {
                        sqlite3_bind_int(link_stmt, 1, collection_id);
                        sqlite3_bind_int(link_stmt, 2, workflow_id);
                        sqlite3_step(link_stmt);
                        sqlite3_finalize(link_stmt);
                    }
                }
            }
        }
        
        // Return the created collection
        json_t *response_json = json_object();
        json_object_set_new(response_json, "id", json_integer(collection_id));
        json_object_set_new(response_json, "name", json_string(name));
        json_object_set_new(response_json, "rank", json_integer(rank_json ? json_integer_value(rank_json) : 0));
        json_object_set_new(response_json, "totalViews", total_views_json ? json_integer(json_integer_value(total_views_json)) : json_null());
        json_object_set_new(response_json, "createdAt", json_string(created_at));
        json_object_set_new(response_json, "workflows", workflows_json ? workflows_json : json_array());
        json_object_set_new(response_json, "nodes", json_array());
        json_object_set_new(response_json, "message", json_string("Collection created successfully"));
        
        ulfius_set_json_body_response(response, 201, response_json);
        json_decref(response_json);
    } else {
        sqlite3_finalize(stmt);
        ulfius_set_string_body_response(response, 500, "Failed to create collection");
    }
    
    json_decref(json_body);
    return U_CALLBACK_CONTINUE;
}

// PATCH /templates/collections
int callback_add_workflow_to_collection(const struct _u_request *request, struct _u_response *response, void *user_data) {
    UNUSED(user_data);

    json_t *json_body = ulfius_get_json_body_request(request, NULL);
    if (json_body == NULL) {
        ulfius_set_string_body_response(response, 400, "Invalid JSON");
        return U_CALLBACK_CONTINUE;
    }

    json_t *collection_id_json = json_object_get(json_body, "collectionId");
    json_t *template_id_json = json_object_get(json_body, "templateId");

    if (!json_is_integer(collection_id_json) || !json_is_integer(template_id_json)) {
        json_decref(json_body);
        ulfius_set_string_body_response(response, 400, "Missing required fields: collectionId and templateId must be integers");
        return U_CALLBACK_CONTINUE;
    }

    int collection_id = json_integer_value(collection_id_json);
    int template_id = json_integer_value(template_id_json);

    // Verify collection exists
    const char *check_collection_sql = "SELECT id FROM collections WHERE id = ?;";
    sqlite3_stmt *check_stmt;
    if (sqlite3_prepare_v2(db, check_collection_sql, -1, &check_stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(check_stmt, 1, collection_id);
        if (sqlite3_step(check_stmt) != SQLITE_ROW) {
            sqlite3_finalize(check_stmt);
            json_decref(json_body);
            ulfius_set_string_body_response(response, 404, "Collection not found");
            return U_CALLBACK_CONTINUE;
        }
        sqlite3_finalize(check_stmt);
    }

    // Verify template exists
    const char *check_template_sql = "SELECT id FROM templates WHERE id = ?;";
    if (sqlite3_prepare_v2(db, check_template_sql, -1, &check_stmt, 0) == SQLITE_OK) {
        sqlite3_bind_int(check_stmt, 1, template_id);
        if (sqlite3_step(check_stmt) != SQLITE_ROW) {
            sqlite3_finalize(check_stmt);
            json_decref(json_body);
            ulfius_set_string_body_response(response, 404, "Template not found");
            return U_CALLBACK_CONTINUE;
        }
        sqlite3_finalize(check_stmt);
    }

    // Insert the relationship
    const char *sql = "INSERT OR IGNORE INTO collection_workflows (collection_id, template_id) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) {
        json_decref(json_body);
        ulfius_set_string_body_response(response, 500, "Database error on prepare");
        return U_CALLBACK_CONTINUE;
    }
    
    sqlite3_bind_int(stmt, 1, collection_id);
    sqlite3_bind_int(stmt, 2, template_id);
    
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        int changes = sqlite3_changes(db);
        sqlite3_finalize(stmt);
        
        json_t *response_json = json_object();
        if (changes > 0) {
            json_object_set_new(response_json, "message", json_string("Workflow added to collection successfully"));
            json_object_set_new(response_json, "collectionId", json_integer(collection_id));
            json_object_set_new(response_json, "templateId", json_integer(template_id));
            ulfius_set_json_body_response(response, 200, response_json);
        } else {
            json_object_set_new(response_json, "message", json_string("Workflow already exists in collection"));
            json_object_set_new(response_json, "collectionId", json_integer(collection_id));
            json_object_set_new(response_json, "templateId", json_integer(template_id));
            ulfius_set_json_body_response(response, 200, response_json);
        }
        json_decref(response_json);
    } else {
        sqlite3_finalize(stmt);
        ulfius_set_string_body_response(response, 500, "Failed to add workflow to collection");
        fprintf(stderr, "callback_add_workflow_to_collection ERROR: Failed to add workflow to collection: %s\n", sqlite3_errmsg(db));
    }
    
    json_decref(json_body);
    return U_CALLBACK_CONTINUE;
}

// Callback to handle OPTIONS requests for CORS and Allow header
int callback_options(const struct _u_request *request, struct _u_response *response, void *user_data) {
    UNUSED(request);
    UNUSED(user_data);

    u_map_put(response->map_header, "Allow", "GET, HEAD");
    ulfius_set_empty_body_response(response, 200);
    return U_CALLBACK_CONTINUE;
}

int main(void) {
    struct _u_instance instance;

    if (init_database() != 0) {
        fprintf(stderr, "Failed to initialize database\n");
        return 1;
    }
    
    if (ulfius_init_instance(&instance, PORT, NULL, NULL) != U_OK) {
        fprintf(stderr, "Error initializing instance\n");
        sqlite3_close(db);
        return 1;
    }
    
    // Add most basic endpoints. See: https://ai-rockstars.com/create-your-own-n8n-templates-step-by-step-guide
    ulfius_add_endpoint_by_val(&instance, "GET", "/health", NULL, 0, &callback_get_health, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/templates", "/categories", 0, &callback_get_categories, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/templates", "/collections", 0, &callback_get_collections, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/templates/collections", "/:id", 0, &callback_get_collection_by_id, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/templates", "/search", 0, &callback_search_templates, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/templates/workflows", "/:id", 0, &callback_get_workflow_by_id, NULL);
    ulfius_add_endpoint_by_val(&instance, "GET", "/templates", "/workflows", 0, &callback_get_all_workflows, NULL);

    // When importing a template workflow it seems to swap the root url directories.
    ulfius_add_endpoint_by_val(&instance, "GET", "/workflows/templates", "/:id", 0, &callback_get_workflow_for_import, NULL);

    ulfius_add_endpoint_by_val(&instance, "OPTIONS", "/templates", "/categories", 0, &callback_options, NULL);
    ulfius_add_endpoint_by_val(&instance, "OPTIONS", "/templates", "/collections", 0, &callback_options, NULL);
    ulfius_add_endpoint_by_val(&instance, "OPTIONS", "/templates/collections", "/:id", 0, &callback_options, NULL);
    ulfius_add_endpoint_by_val(&instance, "OPTIONS", "/templates", "/search", 0, &callback_options, NULL);
    ulfius_add_endpoint_by_val(&instance, "OPTIONS", "/templates/workflows", "/:id", 0, &callback_options, NULL);
    ulfius_add_endpoint_by_val(&instance, "OPTIONS", "/templates", "/workflows", 0, &callback_options, NULL);

    // Custom endpoint to insert a template
    ulfius_add_endpoint_by_val(&instance, "PUT", "/templates", "/workflows", 0, &callback_create_workflow, NULL);
    // Custom endpoint to insert a collection of workflows
    ulfius_add_endpoint_by_val(&instance, "PUT", "/templates", "/collections", 0, &callback_create_collection, NULL);
    // Custom endpoint to insert a workflow into a collection
    ulfius_add_endpoint_by_val(&instance, "PATCH", "/templates", "/collections", 0, &callback_add_workflow_to_collection, NULL);
    
    if (ulfius_start_framework(&instance) == U_OK) {
        printf("n8n Templates API server started on port %d\n", PORT);
        printf("Using database file %s\n", DATABASE_FILE);
        printf("Available endpoints:\n");
        printf("  GET    /health                         - API health status\n");
        printf("  GET    /templates/categories           - Get all categories\n");
        printf("  GET    /templates/collections          - Get collections with optional filters\n");
        printf("  GET    /templates/collections/:id      - Get specific collection by ID\n");
        printf("  GET    /templates/search               - Search workflows with pagination\n");
        printf("  GET    /templates/workflows            - Get all workflows\n");
        printf("  GET    /templates/workflows/:id        - Get specific workflow by ID\n");
        printf("  PUT    /templates/workflows            - Create new workflow\n");
        printf("  PUT    /templates/collections          - Create new collection of workflows\n");
        printf("  PATCH  /templates/collections          - Insert new template workflow into a collection\n");
        printf("Press Ctrl+C to quit...\n");

        // Wait forever until signal (SIGINT/SIGTERM)
        pause();
    } else {
        fprintf(stderr, "Error starting framework\n");
    }
    
    printf("Shutting down...\n");
    ulfius_stop_framework(&instance);
    ulfius_clean_instance(&instance);
    sqlite3_close(db);
    
    return 0;
}
