/**
 * @file nyx_json.h
 * @brief Zero-dependency JSON builder and parser for the NYX framework
 * @author Neur0sis (2025)
 *
 * Provides a minimal, self-contained JSON library for building structured
 * output and parsing tool results. Uses a tagged-union tree model with
 * linked-list children for objects and arrays.
 *
 * Builder usage:
 *   nyx_json_t *obj = nyx_json_object();
 *   nyx_json_set(obj, "name", nyx_json_string("pingsweep"));
 *   nyx_json_set(obj, "count", nyx_json_int(42));
 *   char *str = nyx_json_serialize(obj, 2);
 *   free(str);
 *   nyx_json_free(obj);
 *
 * Parser usage:
 *   nyx_json_t *root = nyx_json_parse_file("results.json");
 *   const char *name = nyx_json_get_string(nyx_json_get(root, "name"));
 *   nyx_json_free(root);
 */

#ifndef NYX_JSON_H
#define NYX_JSON_H

#include <stddef.h>

typedef enum {
    NYX_JSON_NULL,
    NYX_JSON_BOOL,
    NYX_JSON_INT,
    NYX_JSON_DOUBLE,
    NYX_JSON_STRING,
    NYX_JSON_ARRAY,
    NYX_JSON_OBJECT
} nyx_json_type_t;

typedef struct nyx_json nyx_json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Builder ---- */

nyx_json_t *nyx_json_object(void);
nyx_json_t *nyx_json_array(void);
nyx_json_t *nyx_json_string(const char *val);
nyx_json_t *nyx_json_int(long val);
nyx_json_t *nyx_json_real(double val);
nyx_json_t *nyx_json_bool(int val);
nyx_json_t *nyx_json_null(void);

void nyx_json_set(nyx_json_t *obj, const char *key, nyx_json_t *val);
void nyx_json_append(nyx_json_t *arr, nyx_json_t *val);

void nyx_json_free(nyx_json_t *node);

/* ---- Serializer ---- */

char *nyx_json_serialize(const nyx_json_t *node, int indent);
int   nyx_json_write_file(const nyx_json_t *node, const char *path, int indent);

/* ---- Parser ---- */

nyx_json_t *nyx_json_parse(const char *str);
nyx_json_t *nyx_json_parse_file(const char *path);

/* ---- Accessors ---- */

nyx_json_type_t  nyx_json_type(const nyx_json_t *node);
const char      *nyx_json_get_string(const nyx_json_t *node);
long             nyx_json_get_int(const nyx_json_t *node);
double           nyx_json_get_real(const nyx_json_t *node);
int              nyx_json_get_bool(const nyx_json_t *node);

nyx_json_t      *nyx_json_get(const nyx_json_t *obj, const char *key);
size_t           nyx_json_length(const nyx_json_t *arr_or_obj);
nyx_json_t      *nyx_json_at(const nyx_json_t *arr, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* NYX_JSON_H */
