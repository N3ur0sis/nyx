/**
 * @file nyx_json.c
 * @brief Zero-dependency JSON builder, serializer, and recursive-descent parser
 */

#include "nyx_json.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ====================================================================
 *  Internal node structure
 * ==================================================================== */

struct nyx_json {
    nyx_json_type_t type;
    char *key; /* non-NULL only when child of an object */
    union {
        long i;
        double d;
        int b;
        char *s; /* heap-allocated for STRING */
    } v;
    struct nyx_json *child; /* first child (OBJECT / ARRAY) */
    struct nyx_json *next;  /* sibling in parent's child list */
};

/* ====================================================================
 *  Helpers
 * ==================================================================== */

static nyx_json_t *node_new(nyx_json_type_t type)
{
    nyx_json_t *n = calloc(1, sizeof(*n));
    if (n)
        n->type = type;
    return n;
}

static char *str_dup(const char *s)
{
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char *d = malloc(len + 1);
    if (d)
        memcpy(d, s, len + 1);
    return d;
}

/* ====================================================================
 *  Builder API
 * ==================================================================== */

nyx_json_t *nyx_json_object(void)
{
    return node_new(NYX_JSON_OBJECT);
}
nyx_json_t *nyx_json_array(void)
{
    return node_new(NYX_JSON_ARRAY);
}
nyx_json_t *nyx_json_null(void)
{
    return node_new(NYX_JSON_NULL);
}

nyx_json_t *nyx_json_bool(int val)
{
    nyx_json_t *n = node_new(NYX_JSON_BOOL);
    if (n)
        n->v.b = val ? 1 : 0;
    return n;
}

nyx_json_t *nyx_json_int(long val)
{
    nyx_json_t *n = node_new(NYX_JSON_INT);
    if (n)
        n->v.i = val;
    return n;
}

nyx_json_t *nyx_json_real(double val)
{
    nyx_json_t *n = node_new(NYX_JSON_DOUBLE);
    if (n)
        n->v.d = val;
    return n;
}

nyx_json_t *nyx_json_string(const char *val)
{
    nyx_json_t *n = node_new(NYX_JSON_STRING);
    if (n)
        n->v.s = str_dup(val ? val : "");
    return n;
}

void nyx_json_set(nyx_json_t *obj, const char *key, nyx_json_t *val)
{
    if (!obj || obj->type != NYX_JSON_OBJECT || !key || !val)
        return;

    /* Overwrite if key already exists */
    for (nyx_json_t *c = obj->child; c; c = c->next) {
        if (c->key && strcmp(c->key, key) == 0) {
            /* Replace this node's value in-place */
            nyx_json_type_t old_type = c->type;
            if (old_type == NYX_JSON_STRING)
                free(c->v.s);
            if (old_type == NYX_JSON_OBJECT || old_type == NYX_JSON_ARRAY) {
                nyx_json_t *ch = c->child;
                while (ch) {
                    nyx_json_t *tmp = ch->next;
                    nyx_json_free(ch);
                    ch = tmp;
                }
            }
            c->type = val->type;
            c->v = val->v;
            c->child = val->child;
            /* Prevent val's destructor from freeing transferred data */
            val->type = NYX_JSON_NULL;
            val->child = NULL;
            val->v.s = NULL;
            nyx_json_free(val);
            return;
        }
    }

    /* Append new child */
    val->key = str_dup(key);
    if (!obj->child) {
        obj->child = val;
    } else {
        nyx_json_t *tail = obj->child;
        while (tail->next)
            tail = tail->next;
        tail->next = val;
    }
}

void nyx_json_append(nyx_json_t *arr, nyx_json_t *val)
{
    if (!arr || arr->type != NYX_JSON_ARRAY || !val)
        return;
    if (!arr->child) {
        arr->child = val;
    } else {
        nyx_json_t *tail = arr->child;
        while (tail->next)
            tail = tail->next;
        tail->next = val;
    }
}

void nyx_json_free(nyx_json_t *node)
{
    if (!node)
        return;
    free(node->key);
    if (node->type == NYX_JSON_STRING)
        free(node->v.s);
    nyx_json_t *c = node->child;
    while (c) {
        nyx_json_t *tmp = c->next;
        nyx_json_free(c);
        c = tmp;
    }
    free(node);
}

/* ====================================================================
 *  Accessor API
 * ==================================================================== */

nyx_json_type_t nyx_json_type(const nyx_json_t *node)
{
    return node ? node->type : NYX_JSON_NULL;
}

const char *nyx_json_get_string(const nyx_json_t *node)
{
    return (node && node->type == NYX_JSON_STRING) ? node->v.s : NULL;
}

long nyx_json_get_int(const nyx_json_t *node)
{
    return (node && node->type == NYX_JSON_INT) ? node->v.i : 0;
}

double nyx_json_get_real(const nyx_json_t *node)
{
    if (!node)
        return 0.0;
    if (node->type == NYX_JSON_DOUBLE)
        return node->v.d;
    if (node->type == NYX_JSON_INT)
        return (double)node->v.i;
    return 0.0;
}

int nyx_json_get_bool(const nyx_json_t *node)
{
    return (node && node->type == NYX_JSON_BOOL) ? node->v.b : 0;
}

nyx_json_t *nyx_json_get(const nyx_json_t *obj, const char *key)
{
    if (!obj || obj->type != NYX_JSON_OBJECT || !key)
        return NULL;
    for (nyx_json_t *c = obj->child; c; c = c->next) {
        if (c->key && strcmp(c->key, key) == 0)
            return c;
    }
    return NULL;
}

size_t nyx_json_length(const nyx_json_t *arr_or_obj)
{
    if (!arr_or_obj)
        return 0;
    if (arr_or_obj->type != NYX_JSON_ARRAY && arr_or_obj->type != NYX_JSON_OBJECT)
        return 0;
    size_t n = 0;
    for (nyx_json_t *c = arr_or_obj->child; c; c = c->next)
        n++;
    return n;
}

nyx_json_t *nyx_json_at(const nyx_json_t *arr, size_t index)
{
    if (!arr || arr->type != NYX_JSON_ARRAY)
        return NULL;
    size_t i = 0;
    for (nyx_json_t *c = arr->child; c; c = c->next, i++) {
        if (i == index)
            return c;
    }
    return NULL;
}

/* ====================================================================
 *  Serializer
 * ==================================================================== */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} sbuf_t;

static void sbuf_init(sbuf_t *sb)
{
    sb->cap = 256;
    sb->len = 0;
    sb->buf = malloc(sb->cap);
    if (sb->buf)
        sb->buf[0] = '\0';
}

static void sbuf_grow(sbuf_t *sb, size_t need)
{
    if (!sb->buf)
        return;
    size_t required = sb->len + need + 1;
    if (required <= sb->cap)
        return;
    size_t new_cap = sb->cap * 2;
    while (new_cap < required)
        new_cap *= 2;
    char *tmp = realloc(sb->buf, new_cap);
    if (!tmp) {
        free(sb->buf);
        sb->buf = NULL;
        return;
    }
    sb->buf = tmp;
    sb->cap = new_cap;
}

static void sbuf_append(sbuf_t *sb, const char *s, size_t slen)
{
    sbuf_grow(sb, slen);
    if (!sb->buf)
        return;
    memcpy(sb->buf + sb->len, s, slen);
    sb->len += slen;
    sb->buf[sb->len] = '\0';
}

static void sbuf_puts(sbuf_t *sb, const char *s)
{
    sbuf_append(sb, s, strlen(s));
}

static void sbuf_putc(sbuf_t *sb, char c)
{
    sbuf_append(sb, &c, 1);
}

static void sbuf_indent(sbuf_t *sb, int depth, int indent)
{
    if (indent <= 0)
        return;
    int spaces = depth * indent;
    for (int i = 0; i < spaces; i++)
        sbuf_putc(sb, ' ');
}

static void serialize_string(sbuf_t *sb, const char *s)
{
    sbuf_putc(sb, '"');
    if (!s) {
        sbuf_putc(sb, '"');
        return;
    }
    for (const char *p = s; *p; p++) {
        unsigned char ch = (unsigned char)*p;
        switch (ch) {
        case '"':
            sbuf_puts(sb, "\\\"");
            break;
        case '\\':
            sbuf_puts(sb, "\\\\");
            break;
        case '\b':
            sbuf_puts(sb, "\\b");
            break;
        case '\f':
            sbuf_puts(sb, "\\f");
            break;
        case '\n':
            sbuf_puts(sb, "\\n");
            break;
        case '\r':
            sbuf_puts(sb, "\\r");
            break;
        case '\t':
            sbuf_puts(sb, "\\t");
            break;
        default:
            if (ch < 0x20) {
                char esc[8];
                snprintf(esc, sizeof(esc), "\\u%04x", ch);
                sbuf_puts(sb, esc);
            } else {
                sbuf_putc(sb, (char)ch);
            }
        }
    }
    sbuf_putc(sb, '"');
}

static void serialize_node(sbuf_t *sb, const nyx_json_t *node, int depth, int indent)
{
    if (!node) {
        sbuf_puts(sb, "null");
        return;
    }

    switch (node->type) {
    case NYX_JSON_NULL:
        sbuf_puts(sb, "null");
        break;

    case NYX_JSON_BOOL:
        sbuf_puts(sb, node->v.b ? "true" : "false");
        break;

    case NYX_JSON_INT: {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%ld", node->v.i);
        sbuf_puts(sb, tmp);
        break;
    }

    case NYX_JSON_DOUBLE: {
        char tmp[64];
        if (isinf(node->v.d) || isnan(node->v.d)) {
            snprintf(tmp, sizeof(tmp), "null");
        } else {
            snprintf(tmp, sizeof(tmp), "%.6g", node->v.d);
            /* Ensure there's a decimal point for real numbers */
            if (!strchr(tmp, '.') && !strchr(tmp, 'e') && !strchr(tmp, 'E'))
                strcat(tmp, ".0");
        }
        sbuf_puts(sb, tmp);
        break;
    }

    case NYX_JSON_STRING:
        serialize_string(sb, node->v.s);
        break;

    case NYX_JSON_ARRAY: {
        sbuf_putc(sb, '[');
        nyx_json_t *c = node->child;
        if (c && indent > 0)
            sbuf_putc(sb, '\n');
        int first = 1;
        for (; c; c = c->next) {
            if (!first) {
                sbuf_putc(sb, ',');
                if (indent > 0)
                    sbuf_putc(sb, '\n');
            }
            first = 0;
            sbuf_indent(sb, depth + 1, indent);
            serialize_node(sb, c, depth + 1, indent);
        }
        if (node->child && indent > 0) {
            sbuf_putc(sb, '\n');
            sbuf_indent(sb, depth, indent);
        }
        sbuf_putc(sb, ']');
        break;
    }

    case NYX_JSON_OBJECT: {
        sbuf_putc(sb, '{');
        nyx_json_t *c = node->child;
        if (c && indent > 0)
            sbuf_putc(sb, '\n');
        int first = 1;
        for (; c; c = c->next) {
            if (!first) {
                sbuf_putc(sb, ',');
                if (indent > 0)
                    sbuf_putc(sb, '\n');
            }
            first = 0;
            sbuf_indent(sb, depth + 1, indent);
            serialize_string(sb, c->key);
            sbuf_putc(sb, ':');
            if (indent > 0)
                sbuf_putc(sb, ' ');
            serialize_node(sb, c, depth + 1, indent);
        }
        if (node->child && indent > 0) {
            sbuf_putc(sb, '\n');
            sbuf_indent(sb, depth, indent);
        }
        sbuf_putc(sb, '}');
        break;
    }
    }
}

char *nyx_json_serialize(const nyx_json_t *node, int indent)
{
    sbuf_t sb;
    sbuf_init(&sb);
    serialize_node(&sb, node, 0, indent);
    if (indent > 0)
        sbuf_putc(&sb, '\n');
    return sb.buf;
}

int nyx_json_write_file(const nyx_json_t *node, const char *path, int indent)
{
    char *str = nyx_json_serialize(node, indent);
    if (!str)
        return -1;

    FILE *fp = fopen(path, "w");
    if (!fp) {
        free(str);
        return -1;
    }

    size_t len = strlen(str);
    size_t written = fwrite(str, 1, len, fp);
    fclose(fp);
    free(str);

    return (written == len) ? 0 : -1;
}

/* ====================================================================
 *  Recursive-descent parser
 * ==================================================================== */

typedef struct {
    const char *src;
    size_t pos;
} parser_t;

static void skip_ws(parser_t *p)
{
    while (p->src[p->pos] && isspace((unsigned char)p->src[p->pos]))
        p->pos++;
}

static char peek(parser_t *p)
{
    skip_ws(p);
    return p->src[p->pos];
}

static char advance(parser_t *p)
{
    return p->src[p->pos++];
}

static int match(parser_t *p, char c)
{
    if (peek(p) == c) {
        advance(p);
        return 1;
    }
    return 0;
}

static int match_literal(parser_t *p, const char *lit)
{
    skip_ws(p);
    size_t len = strlen(lit);
    if (strncmp(p->src + p->pos, lit, len) == 0) {
        p->pos += len;
        return 1;
    }
    return 0;
}

static nyx_json_t *parse_value(parser_t *p);

static char *parse_string_raw(parser_t *p)
{
    if (!match(p, '"'))
        return NULL;

    sbuf_t sb;
    sbuf_init(&sb);

    while (p->src[p->pos] && p->src[p->pos] != '"') {
        if (p->src[p->pos] == '\\') {
            p->pos++;
            switch (p->src[p->pos]) {
            case '"':
                sbuf_putc(&sb, '"');
                p->pos++;
                break;
            case '\\':
                sbuf_putc(&sb, '\\');
                p->pos++;
                break;
            case '/':
                sbuf_putc(&sb, '/');
                p->pos++;
                break;
            case 'b':
                sbuf_putc(&sb, '\b');
                p->pos++;
                break;
            case 'f':
                sbuf_putc(&sb, '\f');
                p->pos++;
                break;
            case 'n':
                sbuf_putc(&sb, '\n');
                p->pos++;
                break;
            case 'r':
                sbuf_putc(&sb, '\r');
                p->pos++;
                break;
            case 't':
                sbuf_putc(&sb, '\t');
                p->pos++;
                break;
            case 'u': {
                p->pos++;
                char hex[5] = {0};
                for (int i = 0; i < 4 && p->src[p->pos]; i++)
                    hex[i] = p->src[p->pos++];
                unsigned long cp = strtoul(hex, NULL, 16);
                /* Basic UTF-8 encoding of BMP codepoint */
                if (cp < 0x80) {
                    sbuf_putc(&sb, (char)cp);
                } else if (cp < 0x800) {
                    sbuf_putc(&sb, (char)(0xC0 | (cp >> 6)));
                    sbuf_putc(&sb, (char)(0x80 | (cp & 0x3F)));
                } else {
                    sbuf_putc(&sb, (char)(0xE0 | (cp >> 12)));
                    sbuf_putc(&sb, (char)(0x80 | ((cp >> 6) & 0x3F)));
                    sbuf_putc(&sb, (char)(0x80 | (cp & 0x3F)));
                }
                break;
            }
            default:
                sbuf_putc(&sb, p->src[p->pos++]);
            }
        } else {
            sbuf_putc(&sb, p->src[p->pos++]);
        }
    }

    if (p->src[p->pos] == '"')
        p->pos++; /* consume closing quote */
    return sb.buf;
}

static nyx_json_t *parse_string(parser_t *p)
{
    char *s = parse_string_raw(p);
    if (!s)
        return NULL;
    nyx_json_t *n = node_new(NYX_JSON_STRING);
    if (!n) {
        free(s);
        return NULL;
    }
    n->v.s = s;
    return n;
}

static nyx_json_t *parse_number(parser_t *p)
{
    skip_ws(p);
    const char *start = p->src + p->pos;
    char *end = NULL;

    /* Try integer first, then fall back to double */
    errno = 0;
    long lval = strtol(start, &end, 10);

    if (end > start && (*end == '.' || *end == 'e' || *end == 'E')) {
        /* It's a floating point number */
        errno = 0;
        double dval = strtod(start, &end);
        if (end == start)
            return NULL;
        p->pos += (size_t)(end - start);
        nyx_json_t *n = node_new(NYX_JSON_DOUBLE);
        if (n)
            n->v.d = dval;
        return n;
    }

    if (end == start || errno == ERANGE)
        return NULL;
    p->pos += (size_t)(end - start);
    nyx_json_t *n = node_new(NYX_JSON_INT);
    if (n)
        n->v.i = lval;
    return n;
}

static nyx_json_t *parse_array(parser_t *p)
{
    if (!match(p, '['))
        return NULL;
    nyx_json_t *arr = nyx_json_array();
    if (!arr)
        return NULL;

    if (peek(p) == ']') {
        advance(p);
        return arr;
    }

    for (;;) {
        nyx_json_t *val = parse_value(p);
        if (!val) {
            nyx_json_free(arr);
            return NULL;
        }
        nyx_json_append(arr, val);
        if (!match(p, ','))
            break;
    }

    if (!match(p, ']')) {
        nyx_json_free(arr);
        return NULL;
    }
    return arr;
}

static nyx_json_t *parse_object(parser_t *p)
{
    if (!match(p, '{'))
        return NULL;
    nyx_json_t *obj = nyx_json_object();
    if (!obj)
        return NULL;

    if (peek(p) == '}') {
        advance(p);
        return obj;
    }

    for (;;) {
        char *key = parse_string_raw(p);
        if (!key) {
            nyx_json_free(obj);
            return NULL;
        }

        if (!match(p, ':')) {
            free(key);
            nyx_json_free(obj);
            return NULL;
        }

        nyx_json_t *val = parse_value(p);
        if (!val) {
            free(key);
            nyx_json_free(obj);
            return NULL;
        }

        val->key = key;
        if (!obj->child) {
            obj->child = val;
        } else {
            nyx_json_t *tail = obj->child;
            while (tail->next)
                tail = tail->next;
            tail->next = val;
        }

        if (!match(p, ','))
            break;
    }

    if (!match(p, '}')) {
        nyx_json_free(obj);
        return NULL;
    }
    return obj;
}

static nyx_json_t *parse_value(parser_t *p)
{
    char c = peek(p);

    if (c == '"')
        return parse_string(p);
    if (c == '{')
        return parse_object(p);
    if (c == '[')
        return parse_array(p);
    if (c == '-' || (c >= '0' && c <= '9'))
        return parse_number(p);
    if (match_literal(p, "true"))
        return nyx_json_bool(1);
    if (match_literal(p, "false"))
        return nyx_json_bool(0);
    if (match_literal(p, "null"))
        return nyx_json_null();

    return NULL;
}

nyx_json_t *nyx_json_parse(const char *str)
{
    if (!str)
        return NULL;
    parser_t p = {.src = str, .pos = 0};
    return parse_value(&p);
}

nyx_json_t *nyx_json_parse_file(const char *path)
{
    if (!path)
        return NULL;
    FILE *fp = fopen(path, "r");
    if (!fp)
        return NULL;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if (sz <= 0) {
        fclose(fp);
        return NULL;
    }
    fseek(fp, 0, SEEK_SET);

    char *buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }

    size_t nread = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[nread] = '\0';

    nyx_json_t *result = nyx_json_parse(buf);
    free(buf);
    return result;
}
