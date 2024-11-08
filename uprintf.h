// Usage and other information can be found in README at https://github.com/spevnev/uprintf
// Bugs and suggestions can be submitted to https://github.com/spevnev/uprintf/issues

// MIT License
//
// Copyright (c) 2024 Serhii Pievniev
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// ====================== CHECKS ==========================

// clang-format off
#ifndef __linux__
#error [ERROR] uprintf only supports Linux
#endif

#ifdef __cplusplus
#error [ERROR] uprintf does NOT support C++
#endif
// clang-format on

// ====================== HEADER ==========================

#ifndef UPRINTF_H
#define UPRINTF_H

void _upf_uprintf(const char *file, int line, const char *fmt, const char *args, ...);

// If variadic arguments were to be stringified directly, the arguments which
// use macros would stringify to the macro name instead of being expanded, but
// by calling another macro the argument-macros will be expanded and stringified
// to their real values.
#define _upf_stringify_va_args(...) #__VA_ARGS__

// The noop following uprintf is required to guarantee that return PC of the
// function is within the same scope. This is especially problematic if uprintf
// is the last call in the function because then its return PC is that of the
// caller, which optimizes two returns to one.
#define uprintf(fmt, ...)                                                                        \
    do {                                                                                         \
        _upf_uprintf(__FILE__, __LINE__, fmt, _upf_stringify_va_args(__VA_ARGS__), __VA_ARGS__); \
        __asm__ volatile("nop");                                                                 \
    } while (0)

#ifdef UPRINTF_TEST
extern int _upf_test_status;
#endif

#endif  // UPRINTF_H

// ====================== SOURCE ==========================

#ifdef UPRINTF_IMPLEMENTATION

// ===================== OPTIONS ==========================

#ifndef UPRINTF_INDENTATION_WIDTH
#define UPRINTF_INDENTATION_WIDTH 4
#endif

#ifndef UPRINTF_MAX_DEPTH
#define UPRINTF_MAX_DEPTH 10
#endif

#ifndef UPRINTF_IGNORE_STDIO_FILE
#define UPRINTF_IGNORE_STDIO_FILE true
#endif

#ifndef UPRINTF_ARRAY_COMPRESSION_THRESHOLD
#define UPRINTF_ARRAY_COMPRESSION_THRESHOLD 4
#endif

#ifndef UPRINTF_MAX_STRING_LENGTH
#define UPRINTF_MAX_STRING_LENGTH 200
#endif

// ===================== INCLUDES =========================

#ifndef __USE_XOPEN_EXTENDED
#define __USE_XOPEN_EXTENDED
#endif

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// =================== DECLARATIONS =======================

// Feature test macros might not work since it is possible that the header has
// been included before this one and thus expanded without the macro, so the
// functions must be declared here.

ssize_t readlink(const char *path, char *buf, size_t bufsiz);
ssize_t getline(char **lineptr, size_t *n, FILE *stream);

// ===================== dwarf.h ==========================

// dwarf.h's location is inconsistent and the package containing it may not be
// install, so this is a partial implementation used instead of the header for
// consistency and convenience.

// However, all the defines must be prefixed in case the original program does
// include the real dwarf.h

#define _UPF_DW_UT_compile 0x01

#define _UPF_DW_TAG_array_type 0x01
#define _UPF_DW_TAG_enumeration_type 0x04
#define _UPF_DW_TAG_formal_parameter 0x05
#define _UPF_DW_TAG_lexical_block 0x0b
#define _UPF_DW_TAG_member 0x0d
#define _UPF_DW_TAG_pointer_type 0x0f
#define _UPF_DW_TAG_compile_unit 0x11
#define _UPF_DW_TAG_structure_type 0x13
#define _UPF_DW_TAG_subroutine_type 0x15
#define _UPF_DW_TAG_typedef 0x16
#define _UPF_DW_TAG_union_type 0x17
#define _UPF_DW_TAG_unspecified_parameters 0x18
#define _UPF_DW_TAG_inlined_subroutine 0x1d
#define _UPF_DW_TAG_subrange_type 0x21
#define _UPF_DW_TAG_base_type 0x24
#define _UPF_DW_TAG_const_type 0x26
#define _UPF_DW_TAG_enumerator 0x28
#define _UPF_DW_TAG_subprogram 0x2e
#define _UPF_DW_TAG_variable 0x34
#define _UPF_DW_TAG_volatile_type 0x35
#define _UPF_DW_TAG_restrict_type 0x37
#define _UPF_DW_TAG_atomic_type 0x47
#define _UPF_DW_TAG_call_site 0x48
#define _UPF_DW_TAG_call_site_parameter 0x49

#define _UPF_DW_FORM_addr 0x01
#define _UPF_DW_FORM_block2 0x03
#define _UPF_DW_FORM_block4 0x04
#define _UPF_DW_FORM_data2 0x05
#define _UPF_DW_FORM_data4 0x06
#define _UPF_DW_FORM_data8 0x07
#define _UPF_DW_FORM_string 0x08
#define _UPF_DW_FORM_block 0x09
#define _UPF_DW_FORM_block1 0x0a
#define _UPF_DW_FORM_data1 0x0b
#define _UPF_DW_FORM_flag 0x0c
#define _UPF_DW_FORM_sdata 0x0d
#define _UPF_DW_FORM_strp 0x0e
#define _UPF_DW_FORM_udata 0x0f
#define _UPF_DW_FORM_ref_addr 0x10
#define _UPF_DW_FORM_ref1 0x11
#define _UPF_DW_FORM_ref2 0x12
#define _UPF_DW_FORM_ref4 0x13
#define _UPF_DW_FORM_ref8 0x14
#define _UPF_DW_FORM_ref_udata 0x15
#define _UPF_DW_FORM_indirect 0x16
#define _UPF_DW_FORM_sec_offset 0x17
#define _UPF_DW_FORM_exprloc 0x18
#define _UPF_DW_FORM_flag_present 0x19
#define _UPF_DW_FORM_strx 0x1a
#define _UPF_DW_FORM_addrx 0x1b
#define _UPF_DW_FORM_ref_sup4 0x1c
#define _UPF_DW_FORM_strp_sup 0x1d
#define _UPF_DW_FORM_data16 0x1e
#define _UPF_DW_FORM_line_strp 0x1f
#define _UPF_DW_FORM_ref_sig8 0x20
#define _UPF_DW_FORM_implicit_const 0x21
#define _UPF_DW_FORM_loclistx 0x22
#define _UPF_DW_FORM_rnglistx 0x23
#define _UPF_DW_FORM_ref_sup8 0x24
#define _UPF_DW_FORM_strx1 0x25
#define _UPF_DW_FORM_strx2 0x26
#define _UPF_DW_FORM_strx3 0x27
#define _UPF_DW_FORM_strx4 0x28
#define _UPF_DW_FORM_addrx1 0x29
#define _UPF_DW_FORM_addrx2 0x2a
#define _UPF_DW_FORM_addrx3 0x2b
#define _UPF_DW_FORM_addrx4 0x2c

#define _UPF_DW_AT_name 0x03
#define _UPF_DW_AT_byte_size 0x0b
#define _UPF_DW_AT_bit_offset 0x0c
#define _UPF_DW_AT_bit_size 0x0d
#define _UPF_DW_AT_low_pc 0x11
#define _UPF_DW_AT_high_pc 0x12
#define _UPF_DW_AT_language 0x13
#define _UPF_DW_AT_const_value 0x1c
#define _UPF_DW_AT_upper_bound 0x2f
#define _UPF_DW_AT_abstract_origin 0x31
#define _UPF_DW_AT_count 0x37
#define _UPF_DW_AT_data_member_location 0x38
#define _UPF_DW_AT_encoding 0x3e
#define _UPF_DW_AT_type 0x49
#define _UPF_DW_AT_ranges 0x55
#define _UPF_DW_AT_data_bit_offset 0x6b
#define _UPF_DW_AT_str_offsets_base 0x72
#define _UPF_DW_AT_addr_base 0x73
#define _UPF_DW_AT_rnglists_base 0x74

#define _UPF_DW_ATE_address 0x01
#define _UPF_DW_ATE_boolean 0x02
#define _UPF_DW_ATE_complex_float 0x03
#define _UPF_DW_ATE_float 0x04
#define _UPF_DW_ATE_signed 0x05
#define _UPF_DW_ATE_signed_char 0x06
#define _UPF_DW_ATE_unsigned 0x07
#define _UPF_DW_ATE_unsigned_char 0x08
#define _UPF_DW_ATE_imaginary_float 0x09
#define _UPF_DW_ATE_packed_decimal 0x0a
#define _UPF_DW_ATE_numeric_string 0x0b
#define _UPF_DW_ATE_edited 0x0c
#define _UPF_DW_ATE_signed_fixed 0x0d
#define _UPF_DW_ATE_unsigned_fixed 0x0e
#define _UPF_DW_ATE_decimal_float 0x0f
#define _UPF_DW_ATE_UTF 0x10
#define _UPF_DW_ATE_UCS 0x11
#define _UPF_DW_ATE_ASCII 0x12

#define _UPF_DW_RLE_end_of_list 0x00
#define _UPF_DW_RLE_base_addressx 0x01
#define _UPF_DW_RLE_startx_endx 0x02
#define _UPF_DW_RLE_startx_length 0x03
#define _UPF_DW_RLE_offset_pair 0x04
#define _UPF_DW_RLE_base_address 0x05
#define _UPF_DW_RLE_start_end 0x06
#define _UPF_DW_RLE_start_length 0x07

#define _UPF_DW_LANG_C 0x0002
#define _UPF_DW_LANG_C89 0x0001
#define _UPF_DW_LANG_C99 0x000c
#define _UPF_DW_LANG_C11 0x001d
#define _UPF_DW_LANG_C17 0x002c

// ===================== TESTING ==========================

#ifdef UPRINTF_TEST
int _upf_test_status = EXIT_SUCCESS;

#define _UPF_SET_TEST_STATUS(status) _upf_test_status = status
#else
#define _UPF_SET_TEST_STATUS(status) (void) status
#endif

// ====================== ERRORS ==========================

#define _UPF_INVALID -1UL

#define _UPF_LOG(type, ...)                       \
    do {                                          \
        fprintf(stderr, "(uprintf) [%s] ", type); \
        fprintf(stderr, __VA_ARGS__);             \
        fprintf(stderr, "\n");                    \
    } while (0)

#define _UPF_ERROR(...)                            \
    do {                                           \
        _UPF_LOG("ERROR", __VA_ARGS__);            \
        _UPF_SET_TEST_STATUS(EXIT_FAILURE);        \
        longjmp(_upf_state.jmp_buf, EXIT_FAILURE); \
    } while (0)

#define _UPF_WARN(...)                      \
    do {                                    \
        _UPF_LOG("WARNING", __VA_ARGS__);   \
        _UPF_SET_TEST_STATUS(EXIT_FAILURE); \
    } while (0)

#define _UPF_ASSERT(condition)                                                                        \
    do {                                                                                              \
        if (!(condition)) _UPF_ERROR("Assert (%s) failed at %s:%d.", #condition, __FILE__, __LINE__); \
    } while (0)

#define _UPF_OUT_OF_MEMORY() _UPF_ERROR("Process ran out of memory.")
#define _UPF_UNREACHABLE() _UPF_ERROR("Unreachable.");

// ====================== VECTOR ==========================

#define _UPF_INITIAL_VECTOR_CAPACITY 4

#define _UPF_VECTOR_TYPEDEF(name, type) \
    typedef struct {                    \
        _upf_arena *arena;              \
        uint32_t capacity;              \
        uint32_t length;                \
        type *data;                     \
    } name

#define _UPF_VECTOR_NEW(a) \
    {                      \
        .arena = (a),      \
        .capacity = 0,     \
        .length = 0,       \
        .data = NULL,      \
    }

#define _UPF_VECTOR_INIT(vec, a) \
    do {                         \
        (vec)->arena = (a);      \
        (vec)->capacity = 0;     \
        (vec)->length = 0;       \
        (vec)->data = NULL;      \
    } while (0)

#define _UPF_VECTOR_PUSH(vec, element)                                     \
    do {                                                                   \
        if ((vec)->capacity == 0) {                                        \
            (vec)->capacity = _UPF_INITIAL_VECTOR_CAPACITY;                \
            uint32_t size = (vec)->capacity * sizeof(*(vec)->data);        \
            (vec)->data = _upf_arena_alloc((vec)->arena, size);            \
        } else if ((vec)->capacity == (vec)->length) {                     \
            uint32_t old_size = (vec)->capacity * sizeof(*(vec)->data);    \
            (vec)->capacity *= 2;                                          \
            void *new_data = _upf_arena_alloc((vec)->arena, old_size * 2); \
            memcpy(new_data, (vec)->data, old_size);                       \
            (vec)->data = new_data;                                        \
        }                                                                  \
        (vec)->data[(vec)->length++] = (element);                          \
    } while (0)

#define _UPF_VECTOR_COPY(dst, src)                              \
    do {                                                        \
        (dst)->arena = (src)->arena;                            \
        (dst)->capacity = (src)->length;                        \
        (dst)->length = (src)->length;                          \
        uint32_t size = (dst)->capacity * sizeof(*(dst)->data); \
        (dst)->data = _upf_arena_alloc((dst)->arena, size);     \
        memcpy((dst)->data, (src)->data, size);                 \
    } while (0)

#define _UPF_VECTOR_TOP(vec) (vec)->data[(vec)->length - 1]

#define _UPF_VECTOR_POP(vec) (vec)->length--

// ====================== TYPES ===========================

typedef struct _upf_arena_region {
    uint8_t *data;
    size_t capacity;
    size_t length;
    struct _upf_arena_region *next;
} _upf_arena_region;

typedef struct {
    _upf_arena_region *tail;
    _upf_arena_region *head;
} _upf_arena;

_UPF_VECTOR_TYPEDEF(_upf_size_t_vec, size_t);
_UPF_VECTOR_TYPEDEF(_upf_cstr_vec, const char *);

typedef struct {
    uint64_t name;
    uint64_t form;
    int64_t implicit_const;
} _upf_attr;

_UPF_VECTOR_TYPEDEF(_upf_attr_vec, _upf_attr);

typedef struct {
    uint64_t code;
    uint64_t tag;
    bool has_children;
    _upf_attr_vec attrs;
} _upf_abbrev;

_UPF_VECTOR_TYPEDEF(_upf_abbrev_vec, _upf_abbrev);

enum _upf_type_kind {
    _UPF_TK_STRUCT,
    _UPF_TK_UNION,
    _UPF_TK_ENUM,
    _UPF_TK_ARRAY,
    _UPF_TK_POINTER,
    _UPF_TK_FUNCTION,
    _UPF_TK_U1,
    _UPF_TK_U2,
    _UPF_TK_U4,
    _UPF_TK_U8,
    _UPF_TK_S1,
    _UPF_TK_S2,
    _UPF_TK_S4,
    _UPF_TK_S8,
    _UPF_TK_F4,
    _UPF_TK_F8,
    _UPF_TK_BOOL,
    _UPF_TK_SCHAR,
    _UPF_TK_UCHAR,
    _UPF_TK_VOID,
    _UPF_TK_UNKNOWN
};

typedef struct {
    const char *name;
    size_t type;
    size_t offset;
    int bit_size;
} _upf_member;

_UPF_VECTOR_TYPEDEF(_upf_member_vec, _upf_member);

typedef struct {
    const char *name;
    int64_t value;
} _upf_enum;

_UPF_VECTOR_TYPEDEF(_upf_enum_vec, _upf_enum);

#define _UPF_MOD_CONST 1 << 0
#define _UPF_MOD_VOLATILE 1 << 1
#define _UPF_MOD_RESTRICT 1 << 2
#define _UPF_MOD_ATOMIC 1 << 3

typedef struct {
    const char *name;
    enum _upf_type_kind kind;
    int modifiers;
    size_t size;
    union {
        struct {
            _upf_member_vec members;
        } cstruct;
        struct {
            size_t underlying_type;
            _upf_enum_vec enums;
        } cenum;
        struct {
            size_t element_type;
            _upf_size_t_vec lengths;
        } array;
        struct {
            size_t type;
        } pointer;
        struct {
            size_t return_type;
            _upf_size_t_vec arg_types;
        } function;
    } as;
} _upf_type;

typedef struct {
    uint64_t start;
    uint64_t end;
} _upf_range;

_UPF_VECTOR_TYPEDEF(_upf_range_vec, _upf_range);

typedef struct {
    const uint8_t *die;
    const char *name;
} _upf_named_type;

_UPF_VECTOR_TYPEDEF(_upf_named_type_vec, _upf_named_type);

typedef struct {
    const char *name;
    const uint8_t *return_type;
    _upf_named_type_vec args;
    bool is_variadic;
    uint64_t low_pc;
} _upf_function;

_UPF_VECTOR_TYPEDEF(_upf_function_vec, _upf_function);

typedef struct {
    const void *data;
    const _upf_type *type;
    bool is_visited;
    int id;
} _upf_indexed_struct;

_UPF_VECTOR_TYPEDEF(_upf_indexed_struct_vec, _upf_indexed_struct);

struct _upf_scope;
_UPF_VECTOR_TYPEDEF(_upf_scope_vec, struct _upf_scope);

typedef struct _upf_scope {
    _upf_range_vec ranges;
    _upf_named_type_vec vars;
    _upf_scope_vec scopes;
} _upf_scope;

typedef struct {
    int depth;
    _upf_scope *scope;
} _upf_scope_stack_entry;

_UPF_VECTOR_TYPEDEF(_upf_scope_stack, _upf_scope_stack_entry);

enum _upf_token_kind {
    _UPF_TOK_NONE,
    _UPF_TOK_NUMBER,
    _UPF_TOK_STRING,
    _UPF_TOK_ID,
    _UPF_TOK_TYPE_SPECIFIER,
    _UPF_TOK_TYPE_QUALIFIER,
    _UPF_TOK_OPEN_PAREN,
    _UPF_TOK_CLOSE_PAREN,
    _UPF_TOK_OPEN_BRACKET,
    _UPF_TOK_CLOSE_BRACKET,
    _UPF_TOK_COMMA,
    _UPF_TOK_DOT,
    _UPF_TOK_ARROW,
    _UPF_TOK_INCREMENT,
    _UPF_TOK_DECREMENT,
    _UPF_TOK_PLUS,
    _UPF_TOK_MINUS,
    _UPF_TOK_STAR,
    _UPF_TOK_EXCLAMATION,
    _UPF_TOK_TILDE,
    _UPF_TOK_AMPERSAND,
    _UPF_TOK_QUESTION,
    _UPF_TOK_COLON,
    _UPF_TOK_ASSIGNMENT,
    _UPF_TOK_COMPARISON,
    _UPF_TOK_MATH
};

typedef struct {
    enum _upf_token_kind kind;
    const char *string;
} _upf_token;

_UPF_VECTOR_TYPEDEF(_upf_token_vec, _upf_token);

typedef struct {
    _upf_token_vec tokens;
    size_t idx;
} _upf_tokenizer;

enum _upf_base_type {
    _UPF_BT_TYPENAME,
    _UPF_BT_VARIABLE,
    _UPF_BT_FUNCTION,
};

typedef struct {
    int dereference;
    int suffix_calls;
    const char *base;
    enum _upf_base_type base_type;
    _upf_cstr_vec members;
} _upf_parser_state;

typedef struct {
    uint8_t *file;
    off_t file_size;

    bool is64bit;
    uint8_t offset_size;
    uint8_t address_size;

    const uint8_t *die;
    size_t die_size;
    const uint8_t *abbrev;
    const char *str;
    const char *line_str;
    const uint8_t *str_offsets;
    const uint8_t *addr;
    const uint8_t *rnglists;
} _upf_dwarf;

typedef struct {
    const uint8_t *die;
    _upf_type type;
} _upf_type_map_entry;

_UPF_VECTOR_TYPEDEF(_upf_type_map_vec, _upf_type_map_entry);

typedef struct {
    const uint8_t *base;

    _upf_abbrev_vec abbrevs;
    _upf_named_type_vec types;
    _upf_function_vec functions;

    uint64_t addr_base;
    uint64_t str_offsets_base;
    uint64_t rnglists_base;

    _upf_scope scope;
} _upf_cu;

_UPF_VECTOR_TYPEDEF(_upf_cu_vec, _upf_cu);

// =================== GLOBAL STATE =======================

struct _upf_state {
    bool is_init;
    _upf_arena arena;
    _upf_dwarf dwarf;

    int circular_id;
    _upf_range_vec addresses;
    _upf_type_map_vec type_map;
    _upf_cu_vec cus;

    jmp_buf jmp_buf;
    const char *file;
    int line;

    char *buffer;
    size_t size;
    char *ptr;
    size_t free;

    _upf_range_vec uprintf_ranges;
    bool init_pc;
    uint8_t *pc_base;
};

static struct _upf_state _upf_state = {0};

// ====================== ARENA ===========================

#define _UPF_INITIAL_ARENA_SIZE 65535

static _upf_arena_region *_upf_arena_alloc_region(size_t capacity) {
    _upf_arena_region *region = (_upf_arena_region *) malloc(sizeof(*region));
    if (region == NULL) _UPF_OUT_OF_MEMORY();
    region->capacity = capacity;
    region->data = (uint8_t *) malloc(region->capacity * sizeof(*region->data));
    if (region->data == NULL) _UPF_OUT_OF_MEMORY();
    region->length = 0;
    region->next = NULL;
    return region;
}

static void _upf_arena_init(_upf_arena *a) {
    _UPF_ASSERT(a != NULL);

    _upf_arena_region *region = _upf_arena_alloc_region(_UPF_INITIAL_ARENA_SIZE);
    a->head = region;
    a->tail = region;
}

static void *_upf_arena_alloc(_upf_arena *a, size_t size) {
    _UPF_ASSERT(a != NULL && a->head != NULL && a->tail != NULL);

    size_t alignment = (a->head->length % sizeof(void *));
    if (alignment > 0) alignment = sizeof(void *) - alignment;

    if (alignment + size > a->head->capacity - a->head->length) {
        _upf_arena_region *region = _upf_arena_alloc_region(a->head->capacity * 2);
        a->head->next = region;
        a->head = region;
        alignment = 0;
    }

    void *memory = a->head->data + a->head->length + alignment;
    a->head->length += alignment + size;
    return memory;
}

static void _upf_arena_free(_upf_arena *a) {
    if (a == NULL || a->head == NULL || a->tail == NULL) return;

    _upf_arena_region *region = a->tail;
    while (region != NULL) {
        _upf_arena_region *next = region->next;

        free(region->data);
        free(region);

        region = next;
    }

    a->head = NULL;
    a->tail = NULL;
}

// Copies [begin, end) to arena-allocated string
static char *_upf_arena_string(_upf_arena *a, const char *begin, const char *end) {
    _UPF_ASSERT(a != NULL && begin != NULL && end != NULL);

    size_t len = end - begin;

    char *string = (char *) _upf_arena_alloc(a, len + 2);
    memcpy(string, begin, len);
    string[len] = '\0';

    return string;
}

static char *_upf_arena_concat2(_upf_arena *a, ...) {
    _UPF_ASSERT(a != NULL);

    va_list va_args;

    size_t size = 0;
    va_start(va_args, a);
    while (true) {
        const char *str = va_arg(va_args, const char *);
        if (str == NULL) break;
        size += strlen(str);
    }
    va_end(va_args);

    char *result = _upf_arena_alloc(a, size + 1);
    result[size] = '\0';

    char *p = result;
    va_start(va_args, a);
    while (true) {
        const char *str = va_arg(va_args, const char *);
        if (str == NULL) break;

        size_t len = strlen(str);
        memcpy(p, str, len);
        p += len;
    }
    va_end(va_args);

    return result;
}

#define _upf_arena_concat(a, ...) _upf_arena_concat2(a, __VA_ARGS__, NULL)

// ====================== HELPERS =========================

// Converts unsigned LEB128 to uint64_t and returns the size of LEB in bytes
static size_t _upf_uLEB_to_uint64(const uint8_t *leb, uint64_t *result) {
    _UPF_ASSERT(leb != NULL && result != NULL);

    static const uint8_t BITS_MASK = 0x7f;      // 01111111
    static const uint8_t CONTINUE_MASK = 0x80;  // 10000000

    size_t i = 0;
    int shift = 0;

    *result = 0;
    while (true) {
        uint8_t b = leb[i++];
        *result |= (((uint64_t) (b & BITS_MASK)) << shift);
        if ((b & CONTINUE_MASK) == 0) break;
        shift += 7;
    }

    return i;
}

// Converts signed LEB128 to int64_t and returns the size of LEB in bytes
static size_t _upf_LEB_to_int64(const uint8_t *leb, int64_t *result) {
    _UPF_ASSERT(leb != NULL && result != NULL);

    static const uint8_t BITS_MASK = 0x7f;      // 01111111
    static const uint8_t CONTINUE_MASK = 0x80;  // 10000000
    static const uint8_t SIGN_MASK = 0x40;      // 01000000

    size_t i = 0;
    uint8_t b = 0;
    size_t shift = 0;

    *result = 0;
    while (true) {
        b = leb[i++];
        *result |= (((uint64_t) (b & BITS_MASK)) << shift);
        shift += 7;
        if ((b & CONTINUE_MASK) == 0) break;
    }
    if ((shift < sizeof(*result) * 8) && (b & SIGN_MASK)) *result |= -(1 << shift);

    return i;
}

static const _upf_abbrev *_upf_get_abbrev(const _upf_cu *cu, size_t code) {
    _UPF_ASSERT(cu != NULL && code > 0 && code - 1 < cu->abbrevs.length);
    return &cu->abbrevs.data[code - 1];
}

static const _upf_type *_upf_get_type(size_t type_idx) {
    _UPF_ASSERT(type_idx != _UPF_INVALID && type_idx < _upf_state.type_map.length);
    return &_upf_state.type_map.data[type_idx].type;
}

static bool _upf_is_in_range(uint64_t pc, _upf_range_vec ranges) {
    for (size_t i = 0; i < ranges.length; i++) {
        if (ranges.data[i].start <= pc && pc < ranges.data[i].end) return true;
    }
    return false;
}

static bool _upf_is_primitive(const _upf_type *type) {
    _UPF_ASSERT(type != NULL);

    switch (type->kind) {
        case _UPF_TK_STRUCT:
        case _UPF_TK_UNION:
        case _UPF_TK_ARRAY:
            return false;
        case _UPF_TK_FUNCTION:
        case _UPF_TK_ENUM:
        case _UPF_TK_POINTER:
        case _UPF_TK_U1:
        case _UPF_TK_U2:
        case _UPF_TK_U4:
        case _UPF_TK_U8:
        case _UPF_TK_S1:
        case _UPF_TK_S2:
        case _UPF_TK_S4:
        case _UPF_TK_S8:
        case _UPF_TK_F4:
        case _UPF_TK_F8:
        case _UPF_TK_BOOL:
        case _UPF_TK_SCHAR:
        case _UPF_TK_UCHAR:
        case _UPF_TK_VOID:
        case _UPF_TK_UNKNOWN:
            return true;
    }
    _UPF_UNREACHABLE();
}

// ====================== DWARF ===========================

static uint64_t _upf_offset_cast(const uint8_t *die) {
    _UPF_ASSERT(die != NULL);

    uint64_t offset = 0;
    memcpy(&offset, die, _upf_state.dwarf.offset_size);
    return offset;
}

static uint64_t _upf_address_cast(const uint8_t *die) {
    _UPF_ASSERT(die != NULL);

    uint64_t address = 0;
    memcpy(&address, die, _upf_state.dwarf.address_size);
    return address;
}

static size_t _upf_get_attr_size(const uint8_t *die, uint64_t form) {
    _UPF_ASSERT(die != NULL);

    switch (form) {
        case _UPF_DW_FORM_addr:
            return _upf_state.dwarf.address_size;
        case _UPF_DW_FORM_strx1:
        case _UPF_DW_FORM_addrx1:
        case _UPF_DW_FORM_flag:
        case _UPF_DW_FORM_ref1:
        case _UPF_DW_FORM_data1:
            return 1;
        case _UPF_DW_FORM_strx2:
        case _UPF_DW_FORM_addrx2:
        case _UPF_DW_FORM_ref2:
        case _UPF_DW_FORM_data2:
            return 2;
        case _UPF_DW_FORM_strx3:
        case _UPF_DW_FORM_addrx3:
            return 3;
        case _UPF_DW_FORM_ref_sup4:
        case _UPF_DW_FORM_strx4:
        case _UPF_DW_FORM_addrx4:
        case _UPF_DW_FORM_ref4:
        case _UPF_DW_FORM_data4:
            return 4;
        case _UPF_DW_FORM_ref_sig8:
        case _UPF_DW_FORM_ref_sup8:
        case _UPF_DW_FORM_ref8:
        case _UPF_DW_FORM_data8:
            return 8;
        case _UPF_DW_FORM_data16:
            return 16;
        case _UPF_DW_FORM_block1: {
            uint8_t length;
            memcpy(&length, die, sizeof(length));
            return sizeof(length) + length;
        }
        case _UPF_DW_FORM_block2: {
            uint16_t length;
            memcpy(&length, die, sizeof(length));
            return sizeof(length) + length;
        }
        case _UPF_DW_FORM_block4: {
            uint32_t length;
            memcpy(&length, die, sizeof(length));
            return sizeof(length) + length;
        }
        case _UPF_DW_FORM_sdata: {
            int64_t result;
            return _upf_LEB_to_int64(die, &result);
        } break;
        case _UPF_DW_FORM_loclistx:
        case _UPF_DW_FORM_rnglistx:
        case _UPF_DW_FORM_addrx:
        case _UPF_DW_FORM_strx:
        case _UPF_DW_FORM_ref_udata:
        case _UPF_DW_FORM_udata: {
            uint64_t result;
            return _upf_uLEB_to_uint64(die, &result);
        } break;
        case _UPF_DW_FORM_string:
            return strlen((const char *) die) + 1;
        case _UPF_DW_FORM_exprloc:
        case _UPF_DW_FORM_block: {
            uint64_t length;
            size_t leb_size = _upf_uLEB_to_uint64(die, &length);
            return leb_size + length;
        } break;
        case _UPF_DW_FORM_line_strp:
        case _UPF_DW_FORM_strp_sup:
        case _UPF_DW_FORM_sec_offset:
        case _UPF_DW_FORM_ref_addr:
        case _UPF_DW_FORM_strp:
            return _upf_state.dwarf.offset_size;
        case _UPF_DW_FORM_indirect: {
            uint64_t form;
            size_t offset = _upf_uLEB_to_uint64(die, &form);
            return _upf_get_attr_size(die + offset, form);
        } break;
        case _UPF_DW_FORM_flag_present:
        case _UPF_DW_FORM_implicit_const:
            return 0;
    }
    _UPF_UNREACHABLE();
}

static uint64_t _upf_get_x_offset(const uint8_t *die, uint64_t form) {
    _UPF_ASSERT(die != NULL);

    uint64_t offset = 0;
    switch (form) {
        case _UPF_DW_FORM_strx1:
        case _UPF_DW_FORM_addrx1:
            memcpy(&offset, die, 1);
            return offset;
        case _UPF_DW_FORM_strx2:
        case _UPF_DW_FORM_addrx2:
            memcpy(&offset, die, 2);
            return offset;
        case _UPF_DW_FORM_strx3:
        case _UPF_DW_FORM_addrx3:
            memcpy(&offset, die, 3);
            return offset;
        case _UPF_DW_FORM_strx4:
        case _UPF_DW_FORM_addrx4:
            memcpy(&offset, die, 4);
            return offset;
        case _UPF_DW_FORM_addrx:
        case _UPF_DW_FORM_strx:
            _upf_uLEB_to_uint64(die, &offset);
            return offset;
    }
    _UPF_UNREACHABLE();
}

static const char *_upf_get_str(const _upf_cu *cu, const uint8_t *die, uint64_t form) {
    _UPF_ASSERT(cu != NULL && die != NULL);

    switch (form) {
        case _UPF_DW_FORM_strp:
            return _upf_state.dwarf.str + _upf_offset_cast(die);
        case _UPF_DW_FORM_line_strp:
            _UPF_ASSERT(_upf_state.dwarf.line_str != NULL);
            return _upf_state.dwarf.line_str + _upf_offset_cast(die);
        case _UPF_DW_FORM_string:
            return (const char *) die;
        case _UPF_DW_FORM_strx:
        case _UPF_DW_FORM_strx1:
        case _UPF_DW_FORM_strx2:
        case _UPF_DW_FORM_strx3:
        case _UPF_DW_FORM_strx4: {
            _UPF_ASSERT(_upf_state.dwarf.str_offsets != NULL && cu->str_offsets_base != _UPF_INVALID);
            uint64_t offset = _upf_get_x_offset(die, form) * _upf_state.dwarf.offset_size;
            return _upf_state.dwarf.str + _upf_offset_cast(_upf_state.dwarf.str_offsets + cu->str_offsets_base + offset);
        }
    }
    _UPF_UNREACHABLE();
}

static uint64_t _upf_get_ref(const uint8_t *die, uint64_t form) {
    _UPF_ASSERT(die != NULL);

    uint64_t ref = 0;
    switch (form) {
        case _UPF_DW_FORM_ref1:
        case _UPF_DW_FORM_ref2:
        case _UPF_DW_FORM_ref4:
        case _UPF_DW_FORM_ref8:
            memcpy(&ref, die, _upf_get_attr_size(die, form));
            return ref;
        case _UPF_DW_FORM_ref_udata:
            _upf_uLEB_to_uint64(die, &ref);
            return ref;
    }
    _UPF_ERROR("Only references within single compilation unit are supported.");
}

static bool _upf_is_data(uint64_t form) {
    switch (form) {
        case _UPF_DW_FORM_data1:
        case _UPF_DW_FORM_data2:
        case _UPF_DW_FORM_data4:
        case _UPF_DW_FORM_data8:
        case _UPF_DW_FORM_data16:
        case _UPF_DW_FORM_implicit_const:
        case _UPF_DW_FORM_sdata:
        case _UPF_DW_FORM_udata:
            return true;
        default:
            return false;
    }
}

static int64_t _upf_get_data(const uint8_t *die, _upf_attr attr) {
    _UPF_ASSERT(die != NULL);

    switch (attr.form) {
        case _UPF_DW_FORM_data1:
        case _UPF_DW_FORM_data2:
        case _UPF_DW_FORM_data4:
        case _UPF_DW_FORM_data8: {
            int64_t data = 0;
            memcpy(&data, die, _upf_get_attr_size(die, attr.form));
            return data;
        } break;
        case _UPF_DW_FORM_data16:
            _UPF_ERROR("16 byte data blocks aren't supported.");
        case _UPF_DW_FORM_implicit_const:
            return attr.implicit_const;
        case _UPF_DW_FORM_sdata: {
            int64_t data = 0;
            _upf_LEB_to_int64(die, &data);
            return data;
        } break;
        case _UPF_DW_FORM_udata: {
            uint64_t data = 0;
            _upf_uLEB_to_uint64(die, &data);
            return data;
        } break;
    }
    _UPF_UNREACHABLE();
}

static bool _upf_is_addr(uint64_t form) {
    switch (form) {
        case _UPF_DW_FORM_addr:
        case _UPF_DW_FORM_addrx:
        case _UPF_DW_FORM_addrx1:
        case _UPF_DW_FORM_addrx2:
        case _UPF_DW_FORM_addrx3:
        case _UPF_DW_FORM_addrx4:
            return true;
        default:
            return false;
    }
}

static uint64_t _upf_get_addr(const _upf_cu *cu, const uint8_t *die, uint64_t form) {
    _UPF_ASSERT(cu != NULL && die != NULL);

    switch (form) {
        case _UPF_DW_FORM_addr:
            return _upf_address_cast(die);
        case _UPF_DW_FORM_addrx1:
        case _UPF_DW_FORM_addrx2:
        case _UPF_DW_FORM_addrx3:
        case _UPF_DW_FORM_addrx4:
        case _UPF_DW_FORM_addrx: {
            _UPF_ASSERT(_upf_state.dwarf.addr != NULL);
            uint64_t offset = cu->addr_base + _upf_get_x_offset(die, form) * _upf_state.dwarf.address_size;
            return _upf_address_cast(_upf_state.dwarf.addr + offset);
        }
    }
    _UPF_UNREACHABLE();
}

static const uint8_t *_upf_skip_die(const uint8_t *die, const _upf_abbrev *abbrev) {
    _UPF_ASSERT(die != NULL && abbrev != NULL);

    for (size_t i = 0; i < abbrev->attrs.length; i++) {
        die += _upf_get_attr_size(die, abbrev->attrs.data[i].form);
    }

    return die;
}

static enum _upf_type_kind _upf_get_type_kind(int64_t encoding, int64_t size) {
    switch (encoding) {
        case _UPF_DW_ATE_boolean:
            if (size == 1) return _UPF_TK_BOOL;
            _UPF_WARN("Expected boolean to be 1 byte long. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_address:
            _UPF_WARN("Segmented addresses aren't supported. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_signed:
            if (size == 1) return _UPF_TK_S1;
            if (size == 2) return _UPF_TK_S2;
            if (size == 4) return _UPF_TK_S4;
            if (size == 8) return _UPF_TK_S8;
            if (size == 16) {
                _UPF_WARN("16 byte integers aren't supported. Ignoring this type.");
                return _UPF_TK_UNKNOWN;
            }
            _UPF_WARN("Expected signed integer to be 1, 2, 4, 8 or 16 bytes long. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_signed_char:
            if (size == 1) return _UPF_TK_SCHAR;
            _UPF_WARN("Expected char to be 1 byte long. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_unsigned:
            if (size == 1) return _UPF_TK_U1;
            if (size == 2) return _UPF_TK_U2;
            if (size == 4) return _UPF_TK_U4;
            if (size == 8) return _UPF_TK_U8;
            if (size == 16) {
                _UPF_WARN("16 byte unsigned integers aren't supported. Ignoring this type.");
                return _UPF_TK_UNKNOWN;
            }
            _UPF_WARN("Expected unsigned integer to be 1, 2, 4, 8 or 16 bytes long. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_unsigned_char:
            if (size == 1) return _UPF_TK_UCHAR;
            _UPF_WARN("Expected char to be 1 byte long. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_ASCII:
        case _UPF_DW_ATE_UCS:
        case _UPF_DW_ATE_UTF:
            _UPF_WARN("C shouldn't have character encodings besides _UPF_DW_ATE_(un)signed_char. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_signed_fixed:
        case _UPF_DW_ATE_unsigned_fixed:
            _UPF_WARN("C shouldn't have fixed-point decimals. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_float:
            if (size == 4) return _UPF_TK_F4;
            if (size == 8) return _UPF_TK_F8;
            if (size == 16) {
                _UPF_WARN("16 byte floats aren't supported. Ignoring this type.");
                return _UPF_TK_UNKNOWN;
            }
            _UPF_WARN("Expected floats to be 4, 8 or 16 bytes long. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_complex_float:
            _UPF_WARN("Complex floats aren't supported. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_imaginary_float:
            _UPF_WARN("Imaginary floats aren't supported. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_decimal_float:
            _UPF_WARN("Decimal floats aren't supported. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_packed_decimal:
            _UPF_WARN("C shouldn't have packed decimals. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_numeric_string:
            _UPF_WARN("C shouldn't have numeric strings. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        case _UPF_DW_ATE_edited:
            _UPF_WARN("C shouldn't have edited strings. Ignoring this type.");
            return _UPF_TK_UNKNOWN;
        default:
            _UPF_WARN("Skipping unknown DWARF type encoding (0x%02lx)", encoding);
            return _UPF_TK_UNKNOWN;
    }
}

static int _upf_get_type_modifier(uint64_t tag) {
    // clang-format off
    switch (tag) {
        case _UPF_DW_TAG_const_type:    return _UPF_MOD_CONST;
        case _UPF_DW_TAG_volatile_type: return _UPF_MOD_VOLATILE;
        case _UPF_DW_TAG_restrict_type: return _UPF_MOD_RESTRICT;
        case _UPF_DW_TAG_atomic_type:   return _UPF_MOD_ATOMIC;
    }
    // clang-format on
    _UPF_UNREACHABLE();
}

static _upf_type _upf_get_subarray(const _upf_type *array, int count) {
    _UPF_ASSERT(array != NULL);

    _upf_type subarray = *array;
    if (array->name != NULL) {
        subarray.name = _upf_arena_string(&_upf_state.arena, array->name, array->name + strlen(array->name) - 2 * count);
    }
    subarray.as.array.lengths.length -= count;
    subarray.as.array.lengths.data += count;

    return subarray;
}

static size_t _upf_add_type(const uint8_t *type_die, _upf_type type) {
    if (type_die != NULL) {
        for (size_t i = 0; i < _upf_state.type_map.length; i++) {
            if (_upf_state.type_map.data[i].die == type_die) return i;
        }
    }

    _upf_type_map_entry entry = {
        .die = type_die,
        .type = type,
    };
    _UPF_VECTOR_PUSH(&_upf_state.type_map, entry);

    return _upf_state.type_map.length - 1;
}

static size_t _upf_parse_type(const _upf_cu *cu, const uint8_t *die) {
    _UPF_ASSERT(cu != NULL && die != NULL);

    for (size_t i = 0; i < _upf_state.type_map.length; i++) {
        if (_upf_state.type_map.data[i].die == die) return i;
    }

    const uint8_t *base = die;

    uint64_t code;
    die += _upf_uLEB_to_uint64(die, &code);
    const _upf_abbrev *abbrev = _upf_get_abbrev(cu, code);

    const char *name = NULL;
    uint64_t subtype_offset = _UPF_INVALID;
    size_t size = _UPF_INVALID;
    int64_t encoding = 0;
    for (size_t i = 0; i < abbrev->attrs.length; i++) {
        _upf_attr attr = abbrev->attrs.data[i];

        if (attr.name == _UPF_DW_AT_name) {
            name = _upf_get_str(cu, die, attr.form);
        } else if (attr.name == _UPF_DW_AT_type) {
            subtype_offset = _upf_get_ref(die, attr.form);
        } else if (attr.name == _UPF_DW_AT_byte_size) {
            if (_upf_is_data(attr.form)) {
                size = _upf_get_data(die, attr);
            } else {
                _UPF_WARN("Non-constant type sizes aren't supported. Marking size as unknown.");
            }
        } else if (attr.name == _UPF_DW_AT_encoding) {
            encoding = _upf_get_data(die, attr);
        }
        die += _upf_get_attr_size(die, attr.form);
    }

    switch (abbrev->tag) {
        case _UPF_DW_TAG_array_type: {
            _UPF_ASSERT(subtype_offset != _UPF_INVALID);

            _upf_type type = {
                .name = name,
                .kind = _UPF_TK_ARRAY,
                .modifiers = 0,
                .size = size,
                .as.array = {
                    .element_type = _upf_parse_type(cu, cu->base + subtype_offset),
                    .lengths = _UPF_VECTOR_NEW(&_upf_state.arena),
                },
            };

            const _upf_type *element_type = _upf_get_type(type.as.array.element_type);

            bool generate_name = element_type->name != NULL && type.name == NULL;
            if (generate_name) type.name = element_type->name;

            bool is_static = true;
            size_t array_size = element_type->size;
            if (!abbrev->has_children) return _upf_add_type(base, type);
            while (true) {
                die += _upf_uLEB_to_uint64(die, &code);
                if (code == 0) break;

                abbrev = _upf_get_abbrev(cu, code);
                if (abbrev->tag == _UPF_DW_TAG_enumeration_type) {
                    _UPF_WARN("Enumerator array subranges aren't unsupported. Ignoring this type.");
                    goto unknown_type;
                }
                _UPF_ASSERT(abbrev->tag == _UPF_DW_TAG_subrange_type);

                size_t length = _UPF_INVALID;
                for (size_t i = 0; i < abbrev->attrs.length; i++) {
                    _upf_attr attr = abbrev->attrs.data[i];

                    if (attr.name == _UPF_DW_AT_count || attr.name == _UPF_DW_AT_upper_bound) {
                        if (_upf_is_data(attr.form)) {
                            length = _upf_get_data(die, attr);
                            if (attr.name == _UPF_DW_AT_upper_bound) length++;
                        } else {
                            _UPF_WARN("Non-constant array lengths aren't supported. Marking it unknown.");
                        }
                    }

                    die += _upf_get_attr_size(die, attr.form);
                }

                if (length == _UPF_INVALID) {
                    is_static = false;
                    array_size = _UPF_INVALID;
                    type.as.array.lengths.length = 0;
                }

                if (is_static) {
                    array_size *= length;
                    _UPF_VECTOR_PUSH(&type.as.array.lengths, length);
                }

                if (generate_name) type.name = _upf_arena_concat(&_upf_state.arena, type.name, "[]");
            }

            if (element_type->size != _UPF_INVALID && type.size == _UPF_INVALID) {
                type.size = array_size;
            }

            return _upf_add_type(base, type);
        }
        case _UPF_DW_TAG_enumeration_type: {
            _UPF_ASSERT(subtype_offset != _UPF_INVALID);

            _upf_type type = {
                .name = name ? name : "enum",
                .kind = _UPF_TK_ENUM,
                .modifiers = 0,
                .size = size,
                .as.cenum = {
                    .underlying_type = _upf_parse_type(cu, cu->base + subtype_offset),
                    .enums = _UPF_VECTOR_NEW(&_upf_state.arena),
                },
            };

            if (type.size == _UPF_INVALID) type.size = _upf_get_type(type.as.cenum.underlying_type)->size;

            if (!abbrev->has_children) return _upf_add_type(base, type);
            while (true) {
                die += _upf_uLEB_to_uint64(die, &code);
                if (code == 0) break;

                abbrev = _upf_get_abbrev(cu, code);
                _UPF_ASSERT(abbrev->tag == _UPF_DW_TAG_enumerator);

                bool found_value = false;
                _upf_enum cenum = {
                    .name = NULL,
                    .value = 0,
                };
                for (size_t i = 0; i < abbrev->attrs.length; i++) {
                    _upf_attr attr = abbrev->attrs.data[i];

                    if (attr.name == _UPF_DW_AT_name) {
                        cenum.name = _upf_get_str(cu, die, attr.form);
                    } else if (attr.name == _UPF_DW_AT_const_value) {
                        if (_upf_is_data(attr.form)) {
                            cenum.value = _upf_get_data(die, attr);
                            found_value = true;
                        } else {
                            _UPF_WARN("Non-constant enum values aren't supported. Ignoring this type.");
                            goto unknown_type;
                        }
                    }

                    die += _upf_get_attr_size(die, attr.form);
                }
                _UPF_ASSERT(cenum.name != NULL && found_value);

                _UPF_VECTOR_PUSH(&type.as.cenum.enums, cenum);
            }

            return _upf_add_type(base, type);
        }
        case _UPF_DW_TAG_pointer_type: {
            _UPF_ASSERT(size == _UPF_INVALID || size == sizeof(void *));

            _upf_type type = {
                .name = name,
                .kind = _UPF_TK_POINTER,
                .modifiers = 0,
                .size = sizeof(void*),
                .as.pointer = {
                    .type = _UPF_INVALID,
                },
            };

            // Pointers have to be added before their data gets filled in so that
            // structs that pointer to themselves, such as linked lists, don't get
            // stuck in an infinite loop.
            size_t type_idx = _upf_add_type(base, type);

            if (subtype_offset != _UPF_INVALID) {
                // `void*`s have invalid offset (since they don't point to any type), thus
                // pointer with invalid type represents a `void*`

                size_t subtype_idx = _upf_parse_type(cu, cu->base + subtype_offset);
                // Pointers inside vector may become invalid if _upf_parse_type fills up the vector triggering realloc
                _upf_type *type = &_upf_state.type_map.data[type_idx].type;
                type->as.pointer.type = subtype_idx;
                type->name = _upf_get_type(subtype_idx)->name;
            }

            return type_idx;
        }
        case _UPF_DW_TAG_structure_type:
        case _UPF_DW_TAG_union_type: {
            bool is_struct = abbrev->tag == _UPF_DW_TAG_structure_type;
            _upf_type type = {
                .name = name ? name : (is_struct ? "struct" : "union"),
                .kind = is_struct ? _UPF_TK_STRUCT : _UPF_TK_UNION,
                .modifiers = 0,
                .size = size,
                .as.cstruct = {
                    .members = _UPF_VECTOR_NEW(&_upf_state.arena),
                },
            };

            if (!abbrev->has_children) return _upf_add_type(base, type);
            while (true) {
                die += _upf_uLEB_to_uint64(die, &code);
                if (code == 0) break;

                abbrev = _upf_get_abbrev(cu, code);
                if (abbrev->tag != _UPF_DW_TAG_member) {
                    die = _upf_skip_die(die, abbrev);
                    continue;
                }

                _upf_member member = {
                    .name = NULL,
                    .type = _UPF_INVALID,
                    .offset = is_struct ? _UPF_INVALID : 0,
                    .bit_size = 0,
                };
                bool skip_member = false;
                for (size_t i = 0; i < abbrev->attrs.length; i++) {
                    _upf_attr attr = abbrev->attrs.data[i];

                    if (attr.name == _UPF_DW_AT_name) {
                        member.name = _upf_get_str(cu, die, attr.form);
                    } else if (attr.name == _UPF_DW_AT_type) {
                        const uint8_t *type_die = cu->base + _upf_get_ref(die, attr.form);
                        member.type = _upf_parse_type(cu, type_die);
                    } else if (attr.name == _UPF_DW_AT_data_member_location) {
                        if (_upf_is_data(attr.form)) {
                            member.offset = _upf_get_data(die, attr);
                        } else {
                            _UPF_WARN("Non-constant member offsets aren't supported. Skipping this field.");
                            skip_member = true;
                        }
                    } else if (attr.name == _UPF_DW_AT_bit_offset) {
                        _UPF_WARN("Bit offset uses old format. Skipping this field.");
                        skip_member = true;
                    } else if (attr.name == _UPF_DW_AT_data_bit_offset) {
                        member.offset = _upf_get_data(die, attr);
                    } else if (attr.name == _UPF_DW_AT_bit_size) {
                        if (_upf_is_data(attr.form)) {
                            member.bit_size = _upf_get_data(die, attr);
                        } else {
                            _UPF_WARN("Non-constant bit field sizes aren't supported. Skipping this field.");
                            skip_member = true;
                        }
                    }

                    die += _upf_get_attr_size(die, attr.form);
                }
                if (skip_member) continue;

                _UPF_ASSERT(member.name != NULL && member.type != _UPF_INVALID && member.offset != _UPF_INVALID);
                _UPF_VECTOR_PUSH(&type.as.cstruct.members, member);
            }

            return _upf_add_type(base, type);
        }
        case _UPF_DW_TAG_subroutine_type: {
            _upf_type type = {
                .name = name,
                .kind = _UPF_TK_FUNCTION,
                .modifiers = 0,
                .size = size,
                .as.function = {
                    .return_type = _UPF_INVALID,
                    .arg_types = _UPF_VECTOR_NEW(&_upf_state.arena),
                },
            };

            if (subtype_offset != _UPF_INVALID) {
                type.as.function.return_type = _upf_parse_type(cu, cu->base + subtype_offset);
            }

            if (!abbrev->has_children) return _upf_add_type(base, type);
            while (true) {
                die += _upf_uLEB_to_uint64(die, &code);
                if (code == 0) break;

                abbrev = _upf_get_abbrev(cu, code);
                if (abbrev->tag != _UPF_DW_TAG_formal_parameter) {
                    die = _upf_skip_die(die, abbrev);
                    continue;
                }

                for (size_t i = 0; i < abbrev->attrs.length; i++) {
                    _upf_attr attr = abbrev->attrs.data[i];

                    if (attr.name == _UPF_DW_AT_type) {
                        const uint8_t *type_die = cu->base + _upf_get_ref(die, attr.form);
                        size_t arg_type = _upf_parse_type(cu, type_die);
                        _UPF_VECTOR_PUSH(&type.as.function.arg_types, arg_type);
                    }

                    die += _upf_get_attr_size(die, attr.form);
                }
            }

            return _upf_add_type(base, type);
        }
        case _UPF_DW_TAG_typedef: {
            _UPF_ASSERT(name != NULL);

            if (subtype_offset == _UPF_INVALID) {
                // void type is represented by absence of type attribute, e.g. typedef void NAME

                _upf_type type = {
                    .name = name,
                    .kind = _UPF_TK_VOID,
                    .modifiers = 0,
                    .size = _UPF_INVALID,
                };
                return _upf_add_type(base, type);
            }

            size_t type_idx = _upf_parse_type(cu, cu->base + subtype_offset);
            _upf_type type = *_upf_get_type(type_idx);
            type.name = name;

            if (type.kind == _UPF_TK_SCHAR && strcmp(name, "int8_t") == 0) type.kind = _UPF_TK_S1;
            else if (type.kind == _UPF_TK_UCHAR && strcmp(name, "uint8_t") == 0) type.kind = _UPF_TK_U1;

            return _upf_add_type(base, type);
        }
        case _UPF_DW_TAG_base_type: {
            _UPF_ASSERT(name != NULL && size != _UPF_INVALID && encoding != 0);

            _upf_type type = {
                .name = name,
                .kind = _upf_get_type_kind(encoding, size),
                .modifiers = 0,
                .size = size,
            };

            return _upf_add_type(base, type);
        }
        case _UPF_DW_TAG_const_type:
        case _UPF_DW_TAG_volatile_type:
        case _UPF_DW_TAG_restrict_type:
        case _UPF_DW_TAG_atomic_type: {
            if (subtype_offset == _UPF_INVALID) {
                _upf_type type = {
                    .name = "void",
                    .kind = _UPF_TK_VOID,
                    .modifiers = _upf_get_type_modifier(abbrev->tag),
                    .size = sizeof(void *),
                };

                return _upf_add_type(base, type);
            } else {
                size_t type_idx = _upf_parse_type(cu, cu->base + subtype_offset);
                _upf_type type = *_upf_get_type(type_idx);
                type.modifiers |= _upf_get_type_modifier(abbrev->tag);

                return _upf_add_type(base, type);
            }
        }
        default:
            _UPF_WARN("Found unsupported type (0x%lx). Ignoring it.", abbrev->tag);
            break;
    }

    _upf_type type;
unknown_type:
    type = (_upf_type){
        .name = name,
        .kind = _UPF_TK_UNKNOWN,
        .modifiers = 0,
        .size = size,
    };
    return _upf_add_type(base, type);
}

static _upf_range_vec _upf_get_ranges(const _upf_cu *cu, const uint8_t *die, uint64_t form) {
    _UPF_ASSERT(cu != NULL && die != NULL && _upf_state.dwarf.rnglists != NULL);

    const uint8_t *rnglist = NULL;
    if (form == _UPF_DW_FORM_sec_offset) {
        rnglist = _upf_state.dwarf.rnglists + _upf_offset_cast(die);
    } else {
        _UPF_ASSERT(cu->rnglists_base != _UPF_INVALID);
        uint64_t index;
        _upf_uLEB_to_uint64(die, &index);
        uint64_t rnglist_offset = _upf_offset_cast(_upf_state.dwarf.rnglists + cu->rnglists_base + index * _upf_state.dwarf.offset_size);
        rnglist = _upf_state.dwarf.rnglists + cu->rnglists_base + rnglist_offset;
    }
    _UPF_ASSERT(rnglist != NULL);

    uint64_t base = 0;
    if (cu->scope.ranges.length == 1) {
        base = cu->scope.ranges.data[0].start;
    }

    _upf_range_vec ranges = _UPF_VECTOR_NEW(&_upf_state.arena);
    while (*rnglist != _UPF_DW_RLE_end_of_list) {
        switch (*rnglist++) {
            case _UPF_DW_RLE_base_addressx:
                base = _upf_get_addr(cu, rnglist, _UPF_DW_FORM_addrx);
                rnglist += _upf_get_attr_size(rnglist, _UPF_DW_FORM_addrx);
                break;
            case _UPF_DW_RLE_startx_endx: {
                uint64_t start = _upf_get_addr(cu, rnglist, _UPF_DW_FORM_addrx);
                rnglist += _upf_get_attr_size(rnglist, _UPF_DW_FORM_addrx);
                uint64_t end = _upf_get_addr(cu, rnglist, _UPF_DW_FORM_addrx);
                rnglist += _upf_get_attr_size(rnglist, _UPF_DW_FORM_addrx);

                _upf_range range = {
                    .start = start,
                    .end = end,
                };

                _UPF_VECTOR_PUSH(&ranges, range);
            } break;
            case _UPF_DW_RLE_startx_length: {
                uint64_t address = _upf_get_addr(cu, rnglist, _UPF_DW_FORM_addrx);
                rnglist += _upf_get_attr_size(rnglist, _UPF_DW_FORM_addrx);
                uint64_t length = _upf_get_addr(cu, rnglist, _UPF_DW_FORM_addrx);
                rnglist += _upf_get_attr_size(rnglist, _UPF_DW_FORM_addrx);

                _upf_range range = {
                    .start = address,
                    .end = address + length,
                };

                _UPF_VECTOR_PUSH(&ranges, range);
            } break;
            case _UPF_DW_RLE_offset_pair: {
                uint64_t start, end;
                rnglist += _upf_uLEB_to_uint64(rnglist, &start);
                rnglist += _upf_uLEB_to_uint64(rnglist, &end);

                _upf_range range = {
                    .start = base + start,
                    .end = base + end,
                };

                _UPF_VECTOR_PUSH(&ranges, range);
            } break;
            case _UPF_DW_RLE_base_address:
                base = _upf_address_cast(rnglist);
                rnglist += _upf_state.dwarf.address_size;
                break;
            case _UPF_DW_RLE_start_end: {
                uint64_t start = _upf_address_cast(rnglist);
                rnglist += _upf_state.dwarf.address_size;
                uint64_t end = _upf_address_cast(rnglist);
                rnglist += _upf_state.dwarf.address_size;

                _upf_range range = {
                    .start = start,
                    .end = end,
                };

                _UPF_VECTOR_PUSH(&ranges, range);
            } break;
            case _UPF_DW_RLE_start_length: {
                uint64_t address = _upf_address_cast(rnglist);
                rnglist += _upf_state.dwarf.address_size;
                uint64_t length;
                rnglist += _upf_uLEB_to_uint64(rnglist, &length);

                _upf_range range = {
                    .start = address,
                    .end = address + length,
                };

                _UPF_VECTOR_PUSH(&ranges, range);
            } break;
            default:
                _UPF_WARN("Found unsupported range list entry kind (0x%x). Skipping it.", *(rnglist - 1));
                return ranges;
        }
    }

    return ranges;
}

static _upf_named_type _upf_get_var(const _upf_cu *cu, const uint8_t *die) {
    _UPF_ASSERT(cu != NULL && die != NULL);

    uint64_t code;
    die += _upf_uLEB_to_uint64(die, &code);
    const _upf_abbrev *abbrev = _upf_get_abbrev(cu, code);

    _upf_named_type var = {
        .die = NULL,
        .name = NULL,
    };
    for (size_t i = 0; i < abbrev->attrs.length; i++) {
        _upf_attr attr = abbrev->attrs.data[i];

        if (attr.name == _UPF_DW_AT_name) {
            var.name = _upf_get_str(cu, die, attr.form);
        } else if (attr.name == _UPF_DW_AT_type) {
            var.die = cu->base + _upf_get_ref(die, attr.form);
        } else if (attr.name == _UPF_DW_AT_abstract_origin) {
            return _upf_get_var(cu, cu->base + _upf_get_ref(die, attr.form));
        }

        die += _upf_get_attr_size(die, attr.form);
    }

    return var;
}

static _upf_abbrev_vec _upf_parse_abbrevs(const uint8_t *abbrev_table) {
    _UPF_ASSERT(abbrev_table != NULL);

    _upf_abbrev_vec abbrevs = _UPF_VECTOR_NEW(&_upf_state.arena);
    while (true) {
        _upf_abbrev abbrev = {
            .code = _UPF_INVALID,
            .tag = _UPF_INVALID,
            .has_children = false,
            .attrs = _UPF_VECTOR_NEW(&_upf_state.arena),
        };
        abbrev_table += _upf_uLEB_to_uint64(abbrev_table, &abbrev.code);
        if (abbrev.code == 0) break;
        abbrev_table += _upf_uLEB_to_uint64(abbrev_table, &abbrev.tag);

        abbrev.has_children = *abbrev_table;
        abbrev_table += sizeof(abbrev.has_children);

        while (true) {
            _upf_attr attr = {0};
            abbrev_table += _upf_uLEB_to_uint64(abbrev_table, &attr.name);
            abbrev_table += _upf_uLEB_to_uint64(abbrev_table, &attr.form);
            if (attr.form == _UPF_DW_FORM_implicit_const) abbrev_table += _upf_LEB_to_int64(abbrev_table, &attr.implicit_const);
            if (attr.name == 0 && attr.form == 0) break;
            _UPF_VECTOR_PUSH(&abbrev.attrs, attr);
        }

        _UPF_VECTOR_PUSH(&abbrevs, abbrev);
    }

    return abbrevs;
}

static bool _upf_is_language_c(int64_t language) {
    switch (language) {
        case _UPF_DW_LANG_C:
        case _UPF_DW_LANG_C89:
        case _UPF_DW_LANG_C99:
        case _UPF_DW_LANG_C11:
        case _UPF_DW_LANG_C17:
            return true;
        default:
            return false;
    }
}

static _upf_range_vec _upf_get_cu_ranges(const _upf_cu *cu, const uint8_t *low_pc_die, _upf_attr low_pc_attr, const uint8_t *high_pc_die,
                                         _upf_attr high_pc_attr, const uint8_t *ranges_die, _upf_attr ranges_attr) {
    _UPF_ASSERT(cu != NULL);

    if (ranges_die != NULL) return _upf_get_ranges(cu, ranges_die, ranges_attr.form);

    _upf_range_vec ranges = _UPF_VECTOR_NEW(&_upf_state.arena);
    _upf_range range = {
        .start = _UPF_INVALID,
        .end = _UPF_INVALID,
    };

    if (low_pc_die == NULL) {
        _UPF_VECTOR_PUSH(&ranges, range);
        return ranges;
    }

    range.start = _upf_get_addr(cu, low_pc_die, low_pc_attr.form);
    if (high_pc_die != NULL) {
        if (_upf_is_addr(high_pc_attr.form)) {
            range.end = _upf_get_addr(cu, high_pc_die, high_pc_attr.form);
        } else {
            range.end = range.start + _upf_get_data(high_pc_die, high_pc_attr);
        }
    }

    _UPF_VECTOR_PUSH(&ranges, range);
    return ranges;
}

static bool _upf_parse_cu_scope(const _upf_cu *cu, _upf_scope_stack *scope_stack, int depth, const uint8_t *die,
                                const _upf_abbrev *abbrev) {
    _UPF_ASSERT(cu != NULL && scope_stack != NULL && die != NULL && abbrev != NULL);

    if (!abbrev->has_children) return false;
    _UPF_ASSERT(scope_stack->length > 0);

    _upf_scope new_scope = {
        .ranges = _UPF_VECTOR_NEW(&_upf_state.arena),
        .vars = _UPF_VECTOR_NEW(&_upf_state.arena),
        .scopes = _UPF_VECTOR_NEW(&_upf_state.arena),
    };

    uint64_t low_pc = _UPF_INVALID;
    const uint8_t *high_pc_die = NULL;
    _upf_attr high_pc_attr = {0};
    for (size_t i = 0; i < abbrev->attrs.length; i++) {
        _upf_attr attr = abbrev->attrs.data[i];

        if (attr.name == _UPF_DW_AT_low_pc) {
            low_pc = _upf_get_addr(cu, die, attr.form);
        } else if (attr.name == _UPF_DW_AT_high_pc) {
            high_pc_die = die;
            high_pc_attr = attr;
        } else if (attr.name == _UPF_DW_AT_ranges) {
            new_scope.ranges = _upf_get_ranges(cu, die, attr.form);
        }

        die += _upf_get_attr_size(die, attr.form);
    }

    if (low_pc != _UPF_INVALID) {
        if (high_pc_die == NULL) _UPF_ERROR("Expected CU to have both low and high PC.");

        _upf_range range = {
            .start = low_pc,
            .end = _UPF_INVALID,
        };

        if (_upf_is_addr(high_pc_attr.form)) {
            range.end = _upf_get_addr(cu, high_pc_die, high_pc_attr.form);
        } else {
            range.end = low_pc + _upf_get_data(high_pc_die, high_pc_attr);
        }

        _UPF_VECTOR_PUSH(&new_scope.ranges, range);
    }

    _upf_scope_stack_entry stack_entry = {
        .depth = depth,
        .scope = NULL,
    };

    if (new_scope.ranges.length > 0) {
        _upf_scope *scope = NULL;
        for (size_t i = 0; i < scope_stack->length && scope == NULL; i++) {
            scope = scope_stack->data[scope_stack->length - 1 - i].scope;
        }
        _UPF_ASSERT(scope != NULL);

        _UPF_VECTOR_PUSH(&scope->scopes, new_scope);
        stack_entry.scope = &_UPF_VECTOR_TOP(&scope->scopes);
    } else {
        stack_entry.scope = NULL;
    }

    _UPF_VECTOR_PUSH(scope_stack, stack_entry);
    return new_scope.ranges.length > 0;
}

static void _upf_parse_cu_type(_upf_cu *cu, const uint8_t *die) {
    _UPF_ASSERT(cu != NULL && die != NULL);

    const uint8_t *die_base = die;

    uint64_t code;
    die += _upf_uLEB_to_uint64(die, &code);
    const _upf_abbrev *abbrev = _upf_get_abbrev(cu, code);

    const char *name = NULL;
    for (size_t i = 0; i < abbrev->attrs.length; i++) {
        _upf_attr attr = abbrev->attrs.data[i];

        if (attr.name == _UPF_DW_AT_name) name = _upf_get_str(cu, die, attr.form);

        die += _upf_get_attr_size(die, attr.form);
    }
    if (abbrev->tag == _UPF_DW_TAG_pointer_type) {
        if (name) return;
        name = "void";
    }
    if (name == NULL) return;

    _upf_named_type type = {
        .die = die_base,
        .name = name,
    };
    _UPF_VECTOR_PUSH(&cu->types, type);
}

static bool _upf_parse_subprogram_args(_upf_cu *cu, const uint8_t *die, _upf_named_type_vec *args) {
    _UPF_ASSERT(cu != NULL && die != NULL && args != NULL);

    bool is_variadic = false;
    while (true) {
        uint64_t code;
        die += _upf_uLEB_to_uint64(die, &code);
        if (code == 0) break;

        const _upf_abbrev *abbrev = _upf_get_abbrev(cu, code);
        if (abbrev->tag == _UPF_DW_TAG_unspecified_parameters) is_variadic = true;
        if (abbrev->tag != _UPF_DW_TAG_formal_parameter) break;

        _upf_named_type arg = {
            .die = NULL,
            .name = NULL,
        };
        for (size_t i = 0; i < abbrev->attrs.length; i++) {
            _upf_attr attr = abbrev->attrs.data[i];

            if (attr.name == _UPF_DW_AT_name) arg.name = _upf_get_str(cu, die, attr.form);
            else if (attr.name == _UPF_DW_AT_type) arg.die = cu->base + _upf_get_ref(die, attr.form);

            die += _upf_get_attr_size(die, attr.form);
        }
        _UPF_ASSERT(die != NULL);

        _UPF_VECTOR_PUSH(args, arg);
    }

    return is_variadic;
}

static const char *_upf_parse_cu_function(_upf_cu *cu, const uint8_t *die, const _upf_abbrev *abbrev) {
    _UPF_ASSERT(cu != NULL && die != NULL && abbrev != NULL);

    _upf_function function = {
        .name = NULL,
        .return_type = NULL,
        .args = _UPF_VECTOR_NEW(&_upf_state.arena),
        .is_variadic = false,
        .low_pc = _UPF_INVALID,
    };
    for (size_t i = 0; i < abbrev->attrs.length; i++) {
        _upf_attr attr = abbrev->attrs.data[i];

        if (attr.name == _UPF_DW_AT_name) function.name = _upf_get_str(cu, die, attr.form);
        else if (attr.name == _UPF_DW_AT_type) function.return_type = cu->base + _upf_get_ref(die, attr.form);
        else if (attr.name == _UPF_DW_AT_low_pc) function.low_pc = _upf_get_addr(cu, die, attr.form);
        else if (attr.name == _UPF_DW_AT_ranges) {
            _upf_range_vec ranges = _upf_get_ranges(cu, die, attr.form);
            _UPF_ASSERT(ranges.length > 0);
            function.low_pc = ranges.data[0].start;
        }

        die += _upf_get_attr_size(die, attr.form);
    }
    if (abbrev->has_children && function.low_pc != _UPF_INVALID) {
        // Args only need to be parsed in case the function has low_pc, because
        // they are only used for printing the signature of a function pointer.
        function.is_variadic = _upf_parse_subprogram_args(cu, die, &function.args);
    }
    if (function.name != NULL) _UPF_VECTOR_PUSH(&cu->functions, function);

    return function.name;
}

static void _upf_parse_cu(const uint8_t *cu_base, const uint8_t *die, const uint8_t *die_end, const uint8_t *abbrev_table) {
    _UPF_ASSERT(cu_base != NULL && die != NULL && die_end != NULL && abbrev_table != NULL);

    _upf_cu cu = {
        .base = cu_base,
        .abbrevs = _upf_parse_abbrevs(abbrev_table),
        .types = _UPF_VECTOR_NEW(&_upf_state.arena),
        .functions = _UPF_VECTOR_NEW(&_upf_state.arena),
        .addr_base = 0,
        .str_offsets_base = _UPF_INVALID,
        .rnglists_base = _UPF_INVALID,
        .scope = {
            .ranges = {0},
            .vars = _UPF_VECTOR_NEW(&_upf_state.arena),
            .scopes = _UPF_VECTOR_NEW(&_upf_state.arena),
        },
    };

    uint64_t code;
    die += _upf_uLEB_to_uint64(die, &code);
    const _upf_abbrev *abbrev = _upf_get_abbrev(&cu, code);
    _UPF_ASSERT(abbrev->tag == _UPF_DW_TAG_compile_unit);

    // Save to parse after the initializing addr_base, str_offsets, and rnglists_base
    const uint8_t *low_pc_die = NULL;
    _upf_attr low_pc_attr = {0};
    const uint8_t *high_pc_die = NULL;
    _upf_attr high_pc_attr = {0};
    const uint8_t *ranges_die = NULL;
    _upf_attr ranges_attr = {0};
    for (size_t i = 0; i < abbrev->attrs.length; i++) {
        _upf_attr attr = abbrev->attrs.data[i];

        if (attr.name == _UPF_DW_AT_low_pc) {
            low_pc_attr = attr;
            low_pc_die = die;
        } else if (attr.name == _UPF_DW_AT_high_pc) {
            high_pc_attr = attr;
            high_pc_die = die;
        } else if (attr.name == _UPF_DW_AT_ranges) {
            ranges_attr = attr;
            ranges_die = die;
        } else if (attr.name == _UPF_DW_AT_addr_base) {
            cu.addr_base = _upf_offset_cast(die);
        } else if (attr.name == _UPF_DW_AT_str_offsets_base) {
            cu.str_offsets_base = _upf_offset_cast(die);
        } else if (attr.name == _UPF_DW_AT_rnglists_base) {
            cu.rnglists_base = _upf_offset_cast(die);
        } else if (attr.name == _UPF_DW_AT_language) {
            int64_t language = _upf_get_data(die, attr);
            if (!_upf_is_language_c(language)) return;
        }

        die += _upf_get_attr_size(die, attr.form);
    }

    cu.scope.ranges = _upf_get_cu_ranges(&cu, low_pc_die, low_pc_attr, high_pc_die, high_pc_attr, ranges_die, ranges_attr);

    int depth = 0;
    _upf_scope_stack scope_stack = _UPF_VECTOR_NEW(&_upf_state.arena);

    _upf_scope_stack_entry stack_entry = {
        .depth = depth,
        .scope = &cu.scope,
    };
    _UPF_VECTOR_PUSH(&scope_stack, stack_entry);

    while (die < die_end) {
        const uint8_t *die_base = die;

        die += _upf_uLEB_to_uint64(die, &code);
        if (code == 0) {
            _UPF_ASSERT(scope_stack.length > 0);
            if (depth == _UPF_VECTOR_TOP(&scope_stack).depth) _UPF_VECTOR_POP(&scope_stack);
            depth--;
            continue;
        }

        abbrev = _upf_get_abbrev(&cu, code);
        if (abbrev->has_children) depth++;

        bool is_uprintf = false;
        switch (abbrev->tag) {
            case _UPF_DW_TAG_subprogram: {
                const char *function_name = _upf_parse_cu_function(&cu, die, abbrev);
                if (function_name != NULL && strcmp(function_name, "_upf_uprintf") == 0) is_uprintf = true;
                __attribute__((fallthrough));
            }
            case _UPF_DW_TAG_lexical_block:
            case _UPF_DW_TAG_inlined_subroutine: {
                bool scope = _upf_parse_cu_scope(&cu, &scope_stack, depth, die, abbrev);
                if (is_uprintf && scope) {
                    _UPF_ASSERT(_upf_state.uprintf_ranges.length == 0);
                    _UPF_VECTOR_COPY(&_upf_state.uprintf_ranges, &_UPF_VECTOR_TOP(&scope_stack).scope->ranges);
                }
                is_uprintf = false;
            } break;
            case _UPF_DW_TAG_array_type:
            case _UPF_DW_TAG_enumeration_type:
            case _UPF_DW_TAG_pointer_type:
            case _UPF_DW_TAG_structure_type:
            case _UPF_DW_TAG_typedef:
            case _UPF_DW_TAG_union_type:
            case _UPF_DW_TAG_base_type:
                _upf_parse_cu_type(&cu, die_base);
                break;
            case _UPF_DW_TAG_variable:
            case _UPF_DW_TAG_formal_parameter: {
                _upf_scope *scope = _UPF_VECTOR_TOP(&scope_stack).scope;
                if (scope == NULL) break;

                _upf_named_type var = _upf_get_var(&cu, die_base);
                if (var.name == NULL) break;
                if (var.die == NULL) {
                    _UPF_ERROR(
                        "Unable to find variable DIE. "
                        "Ensure that the executable contains debugging information of at least 2nd level (-g2 or -g3).");
                }

                _UPF_VECTOR_PUSH(&scope->vars, var);
            } break;
        }

        die = _upf_skip_die(die, abbrev);
    }

    if (cu.scope.scopes.length > 0 && cu.scope.ranges.length == 1) {
        _upf_range *range = &cu.scope.ranges.data[0];

        if (range->start == _UPF_INVALID) {
            if (cu.scope.scopes.data[0].ranges.length == 0) {
                range->start = 0;
            } else {
                range->start = cu.scope.scopes.data[0].ranges.data[0].start;
            }
        }

        if (range->end == _UPF_INVALID) {
            if (_UPF_VECTOR_TOP(&cu.scope.scopes).ranges.length == 0) {
                range->end = UINT64_MAX;
            } else {
                range->end = _UPF_VECTOR_TOP(&_UPF_VECTOR_TOP(&cu.scope.scopes).ranges).end;
            }
        }
    }

    _UPF_VECTOR_PUSH(&_upf_state.cus, cu);
}

static void _upf_parse_dwarf(void) {
    const uint8_t *die = _upf_state.dwarf.die;
    const uint8_t *die_end = die + _upf_state.dwarf.die_size;
    while (die < die_end) {
        const uint8_t *cu_base = die;

        uint64_t length = 0;
        memcpy(&length, die, sizeof(uint32_t));
        die += sizeof(uint32_t);

        _upf_state.dwarf.is64bit = false;
        if (length == 0xffffffffU) {
            memcpy(&length, die, sizeof(uint64_t));
            die += sizeof(uint64_t);
            _upf_state.dwarf.is64bit = true;
        }

        _upf_state.dwarf.offset_size = _upf_state.dwarf.is64bit ? 8 : 4;

        const uint8_t *next = die + length;

        uint16_t version = 0;
        memcpy(&version, die, sizeof(version));
        die += sizeof(version);
        if (version != 5) _UPF_ERROR("uprintf only supports DWARF version 5.");

        uint8_t type = *die;
        die += sizeof(type);
        _UPF_ASSERT(type == _UPF_DW_UT_compile);

        uint8_t address_size = *die;
        _UPF_ASSERT(_upf_state.dwarf.address_size == 0 || _upf_state.dwarf.address_size == address_size);
        _upf_state.dwarf.address_size = address_size;
        die += sizeof(address_size);

        uint64_t abbrev_offset = _upf_offset_cast(die);
        die += _upf_state.dwarf.offset_size;

        _upf_parse_cu(cu_base, die, next, _upf_state.dwarf.abbrev + abbrev_offset);

        die = next;
    }
}

// ======================= ELF ============================

static void _upf_parse_elf(void) {
    struct stat file_info;
    if (stat("/proc/self/exe", &file_info) == -1) _UPF_ERROR("Unable to stat \"/proc/self/exe\": %s.", strerror(errno));
    size_t size = file_info.st_size;
    _upf_state.dwarf.file_size = size;

    int fd = open("/proc/self/exe", O_RDONLY);
    _UPF_ASSERT(fd != -1);

    uint8_t *file = (uint8_t *) mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    _UPF_ASSERT(file != MAP_FAILED);
    _upf_state.dwarf.file = file;

    close(fd);

    const Elf64_Ehdr *header = (Elf64_Ehdr *) file;

    if (memcmp(header->e_ident, ELFMAG, SELFMAG) != 0 || header->e_ident[EI_CLASS] != ELFCLASS64 || header->e_ident[EI_VERSION] != 1
        || header->e_machine != EM_X86_64 || header->e_version != 1 || header->e_shentsize != sizeof(Elf64_Shdr)) {
        _UPF_ERROR("Unsupported or invalid ELF file.");
    }

    const Elf64_Shdr *string_section = (Elf64_Shdr *) (file + header->e_shoff + header->e_shstrndx * header->e_shentsize);
    const char *string_table = (char *) (file + string_section->sh_offset);

    const Elf64_Shdr *section = (Elf64_Shdr *) (file + header->e_shoff);
    for (size_t i = 0; i < header->e_shnum; i++) {
        const char *name = string_table + section->sh_name;

        if (strcmp(name, ".debug_info") == 0) {
            _upf_state.dwarf.die = file + section->sh_offset;
            _upf_state.dwarf.die_size = section->sh_size;
        } else if (strcmp(name, ".debug_abbrev") == 0) {
            _upf_state.dwarf.abbrev = file + section->sh_offset;
        } else if (strcmp(name, ".debug_str") == 0) {
            _upf_state.dwarf.str = (const char *) (file + section->sh_offset);
        } else if (strcmp(name, ".debug_line_str") == 0) {
            _upf_state.dwarf.line_str = (const char *) (file + section->sh_offset);
        } else if (strcmp(name, ".debug_str_offsets") == 0) {
            _upf_state.dwarf.str_offsets = file + section->sh_offset;
        } else if (strcmp(name, ".debug_rnglists") == 0) {
            _upf_state.dwarf.rnglists = file + section->sh_offset;
        } else if (strcmp(name, ".debug_addr") == 0) {
            _upf_state.dwarf.addr = file + section->sh_offset;
        }

        section++;
    }

    if (_upf_state.dwarf.die == NULL || _upf_state.dwarf.abbrev == NULL || _upf_state.dwarf.str == NULL) {
        _UPF_ERROR("Unable to find debugging information. Ensure that the executable contains it by using -g2 or -g3.");
    }
}

// ==================== TOKENIZING ========================

static _upf_cstr_vec _upf_get_args(char *string) {
    _UPF_ASSERT(string != NULL);

    _upf_cstr_vec args = _UPF_VECTOR_NEW(&_upf_state.arena);

    bool in_quotes = false;
    int paren = 0;
    const char *prev = string;
    for (char *ch = string; *ch != '\0'; ch++) {
        if (*ch == '"') {
            if (in_quotes) {
                if (*(ch - 1) == '\\') in_quotes = true;
                else in_quotes = !in_quotes;
            } else {
                if (*(ch - 1) == '\'') in_quotes = false;
                else in_quotes = !in_quotes;
            }
        }
        if (in_quotes) continue;

        if (*ch == '(') {
            paren++;
        } else if (*ch == ')') {
            paren--;
        } else if (*ch == ',' && paren == 0) {
            *ch = '\0';
            _UPF_VECTOR_PUSH(&args, prev);
            prev = ch + 1;
        }
    }
    _UPF_VECTOR_PUSH(&args, prev);

    return args;
}

static void _upf_tokenize(_upf_tokenizer *t, const char *string) {
    _UPF_ASSERT(t != NULL && string != NULL);

    // Signs should be ordered so that longer ones go first in order to avoid multi character sign
    // being tokenized as multiple single character ones, e.g. >= being '>' '='.
    static const _upf_token signs[]
        = {{_UPF_TOK_ASSIGNMENT, "<<="}, {_UPF_TOK_ASSIGNMENT, ">>="},

           {_UPF_TOK_ARROW, "->"},       {_UPF_TOK_INCREMENT, "++"},   {_UPF_TOK_DECREMENT, "--"},   {_UPF_TOK_COMPARISON, "<="},
           {_UPF_TOK_COMPARISON, ">="},  {_UPF_TOK_COMPARISON, "=="},  {_UPF_TOK_COMPARISON, "!="},  {_UPF_TOK_COMPARISON, "&&"},
           {_UPF_TOK_COMPARISON, "||"},  {_UPF_TOK_MATH, "<<"},        {_UPF_TOK_MATH, ">>"},        {_UPF_TOK_ASSIGNMENT, "*="},
           {_UPF_TOK_ASSIGNMENT, "/="},  {_UPF_TOK_ASSIGNMENT, "%="},  {_UPF_TOK_ASSIGNMENT, "+="},  {_UPF_TOK_ASSIGNMENT, "-="},
           {_UPF_TOK_ASSIGNMENT, "&="},  {_UPF_TOK_ASSIGNMENT, "^="},  {_UPF_TOK_ASSIGNMENT, "|="},

           {_UPF_TOK_COMMA, ","},        {_UPF_TOK_AMPERSAND, "&"},    {_UPF_TOK_STAR, "*"},         {_UPF_TOK_OPEN_PAREN, "("},
           {_UPF_TOK_CLOSE_PAREN, ")"},  {_UPF_TOK_DOT, "."},          {_UPF_TOK_OPEN_BRACKET, "["}, {_UPF_TOK_CLOSE_BRACKET, "]"},
           {_UPF_TOK_QUESTION, "?"},     {_UPF_TOK_COLON, ":"},        {_UPF_TOK_COMPARISON, "<"},   {_UPF_TOK_COMPARISON, ">"},
           {_UPF_TOK_EXCLAMATION, "!"},  {_UPF_TOK_PLUS, "+"},         {_UPF_TOK_MINUS, "-"},        {_UPF_TOK_TILDE, "~"},
           {_UPF_TOK_MATH, "/"},         {_UPF_TOK_MATH, "%"},         {_UPF_TOK_MATH, "^"},         {_UPF_TOK_MATH, "|"},
           {_UPF_TOK_ASSIGNMENT, "="}};
    static const char *type_specifiers[] = {"struct", "union", "enum"};
    static const char *type_qualifiers[] = {"const", "volatile", "restrict", "_Atomic"};

    const char *ch = string;
    while (*ch != '\0') {
        if (*ch == ' ') {
            ch++;
            continue;
        }

        if ('0' <= *ch && *ch <= '9') {
            // Handle floats written as ".123"
            if (t->tokens.length > 0 && _UPF_VECTOR_TOP(&t->tokens).kind == _UPF_TOK_DOT) {
                _UPF_VECTOR_POP(&t->tokens);
                while (*ch != '.') ch--;
            }

            errno = 0;
            char *end = NULL;
            strtof(ch, &end);
            _UPF_ASSERT(errno == 0 && end != NULL);

            _upf_token token = {
                .kind = _UPF_TOK_NUMBER,
                .string = _upf_arena_string(&_upf_state.arena, ch, end),
            };
            _UPF_VECTOR_PUSH(&t->tokens, token);

            ch = end;
        } else if (('a' <= *ch && *ch <= 'z') || ('A' <= *ch && *ch <= 'Z') || *ch == '_') {
            const char *end = ch;
            while (('a' <= *end && *end <= 'z') || ('A' <= *end && *end <= 'Z') || ('0' <= *end && *end <= '9') || *end == '_') end++;

            const char *string = _upf_arena_string(&_upf_state.arena, ch, end);

            enum _upf_token_kind kind = _UPF_TOK_ID;

            for (size_t i = 0; i < sizeof(type_qualifiers) / sizeof(*type_qualifiers); i++) {
                if (strcmp(string, type_qualifiers[i]) == 0) {
                    kind = _UPF_TOK_TYPE_QUALIFIER;
                    break;
                }
            }

            for (size_t i = 0; i < sizeof(type_specifiers) / sizeof(*type_specifiers); i++) {
                if (strcmp(string, type_specifiers[i]) == 0) {
                    kind = _UPF_TOK_TYPE_SPECIFIER;
                    break;
                }
            }

            _upf_token token = {
                .kind = kind,
                .string = string,
            };
            _UPF_VECTOR_PUSH(&t->tokens, token);

            ch = end;
        } else if (*ch == '"') {
            ch++;

            int escape = 0;
            const char *end = ch;
            while ((*end != '"' || escape % 2 == 1) && *end != '\0') {
                if (*end == '\\') escape++;
                else escape = 0;

                end++;
            }
            _UPF_ASSERT(*end != '\0');

            _upf_token token = {
                .kind = _UPF_TOK_STRING,
                .string = _upf_arena_string(&_upf_state.arena, ch, end),
            };
            _UPF_VECTOR_PUSH(&t->tokens, token);

            ch = end + 1;
        } else {
            bool found = false;
            for (size_t i = 0; i < sizeof(signs) / sizeof(*signs) && !found; i++) {
                size_t len = strlen(signs[i].string);
                if (strncmp(ch, signs[i].string, len) == 0) {
                    _UPF_VECTOR_PUSH(&t->tokens, signs[i]);
                    ch += len;
                    found = true;
                }
            }
            if (!found) {
                _UPF_ERROR("Unknown character '%c' when parsing arguments \"%s\" at %s:%d.", *ch, string, _upf_state.file, _upf_state.line);
            }
        }
    }
}

static _upf_token _upf_next(_upf_tokenizer *t) {
    const _upf_token none = {
        .kind = _UPF_TOK_NONE,
        .string = "none",
    };

    _UPF_ASSERT(t != NULL);
    return t->idx < t->tokens.length ? t->tokens.data[t->idx++] : none;
}

static void _upf_back(_upf_tokenizer *t) {
    _UPF_ASSERT(t != NULL && t->idx > 0);
    t->idx--;
}

static _upf_token _upf_consume2(_upf_tokenizer *t, ...) {
    const _upf_token none = {
        .kind = _UPF_TOK_NONE,
        .string = "none",
    };

    _UPF_ASSERT(t != NULL);
    if (t->idx >= t->tokens.length) return none;

    _upf_token token = t->tokens.data[t->idx];

    va_list va_args;
    va_start(va_args, t);
    while (true) {
        enum _upf_token_kind kind = va_arg(va_args, enum _upf_token_kind);
        if (kind == _UPF_TOK_NONE) break;

        if (token.kind == kind) {
            t->idx++;
            va_end(va_args);
            return token;
        }
    }
    va_end(va_args);

    return none;
}

#define _upf_consume(t, ...) _upf_consume2(t, __VA_ARGS__, _UPF_TOK_NONE)

// ===================== PARSING ==========================

// Recursive descent parser with backtracking for parsing function arguments.
// Parsing functions contain commented out rules from Yacc used as a reference.
// Complete C11 Yacc grammar that was used as a reference: https://www.quut.com/c/ANSI-C-grammar-y.html

static bool _upf_parse_unary_expr(_upf_tokenizer *t, _upf_parser_state *p);
static bool _upf_parse_expr(_upf_tokenizer *t, _upf_parser_state *p);

static const char *_upf_modifier_typename_to_stdint(int type, bool is_signed, int longness) {
    if (type == _UPF_DW_ATE_signed_char) {
        return is_signed ? "char" : "unsigned char";
    } else if (type == _UPF_DW_ATE_float) {
        if (longness == 1 && sizeof(long double) > sizeof(double)) {
            _UPF_WARN("Long doubles aren't supported. Ignoring this type.");
            return NULL;
        }
        return "double";
    } else if (type == _UPF_DW_ATE_signed) {
        int offset;
        char *name = _upf_arena_alloc(&_upf_state.arena, 9);
        if (is_signed) {
            offset = 3;
            memcpy(name, "int", offset);
        } else {
            offset = 4;
            memcpy(name, "uint", offset);
        }

        int size;
        switch (longness) {
            case -1:
                size = sizeof(short int);
                break;
            case 0:
                size = sizeof(int);
                break;
            case 1:
                size = sizeof(long int);
                break;
            case 2:
                size = sizeof(long long int);
                break;
            default:
                _UPF_UNREACHABLE();
        }

        if (snprintf(name + offset, 5, "%d_t", size * 8) < 3) _UPF_ERROR("Error in snprintf: %s.", strerror(errno));
        return name;
    }
    _UPF_UNREACHABLE();
}

static bool _upf_parse_typename(_upf_tokenizer *t, const char **typename, int *dereference) {
    _UPF_ASSERT(t != NULL);
    // pointer
    // 	: '*' type_qualifier[] pointer
    // 	| '*' type_qualifier[]
    // 	| '*' pointer
    // 	| '*'
    // typename
    // 	: specifier_qualifier[] pointer
    // 	| specifier_qualifier[]

    bool is_type = false;
    const char *ids[4];
    int ids_length = 0;
    while (true) {
        _upf_token token = _upf_consume(t, _UPF_TOK_TYPE_SPECIFIER, _UPF_TOK_TYPE_QUALIFIER, _UPF_TOK_ID);
        if (token.kind == _UPF_TOK_NONE) break;

        is_type = true;
        if (token.kind == _UPF_TOK_ID) {
            _UPF_ASSERT(ids_length < 4);
            ids[ids_length++] = token.string;
        }
    }
    if (!is_type) return false;

    while (_upf_consume(t, _UPF_TOK_STAR).kind != _UPF_TOK_NONE) {
        if (dereference) *dereference -= 1;
        while (_upf_consume(t, _UPF_TOK_TYPE_QUALIFIER).kind != _UPF_TOK_NONE) continue;
    }

    if (typename == NULL) return true;

    bool is_base_type = true;
    int type = _UPF_DW_ATE_signed;
    bool is_signed = true;
    int longness = 0;
    for (int i = 0; i < ids_length; i++) {
        if (strcmp(ids[i], "long") == 0) longness++;
        else if (strcmp(ids[i], "short") == 0) longness--;
        else if (strcmp(ids[i], "unsigned") == 0) is_signed = false;
        else if (strcmp(ids[i], "char") == 0) type = _UPF_DW_ATE_signed_char;
        else if (strcmp(ids[i], "double") == 0) type = _UPF_DW_ATE_float;
        else is_base_type = false;
    }
    if (is_base_type) {
        *typename = _upf_modifier_typename_to_stdint(type, is_signed, longness);
    } else {
        _UPF_ASSERT(ids_length == 1);
        *typename = ids[0];
    }

    return true;
}

static bool _upf_parse_cast_expr(_upf_tokenizer *t, _upf_parser_state *p) {
    _UPF_ASSERT(t != NULL);
    // cast_expr
    // 	: unary_expr
    // 	| '(' typename ')' cast_expr

    const char *typename;
    int dereference;
    size_t save = t->idx;
    if (_upf_consume(t, _UPF_TOK_OPEN_PAREN).kind != _UPF_TOK_NONE &&   //
        _upf_parse_typename(t, &typename, &dereference) &&              //
        _upf_consume(t, _UPF_TOK_CLOSE_PAREN).kind != _UPF_TOK_NONE &&  //
        _upf_parse_cast_expr(t, NULL)) {
        if (p) {
            p->dereference = dereference;
            p->base = typename;
            p->base_type = _UPF_BT_TYPENAME;
        }
        return true;
    }

    t->idx = save;
    return _upf_parse_unary_expr(t, p);
}

static bool _upf_parse_math_expr(_upf_tokenizer *t, _upf_parser_state *p) {
    _UPF_ASSERT(t != NULL);
    // multiplicative_expr
    // 	: cast_expr
    // 	| cast_expr MULTIPLICATIVE_OP multiplicative_expr
    // additive_expr
    // 	: multiplicative_expr
    // 	| multiplicative_expr ADDITIVE_OP additive_expr
    // shift_expr
    // 	: additive_expr
    // 	| additive_expr SHIFT_OP shift_expr
    // relational_expr
    // 	: shift_expr
    // 	| shift_expr RELATION_OP relational_expr
    // equality_expr
    // 	: relational_expr
    // 	| relational_expr '==' equality_expr
    // 	| relational_expr '!=' equality_expr
    // and_expr
    // 	: equality_expr
    // 	| equality_expr '&' and_expr
    // xor_expr
    // 	: and_expr
    // 	| and_expr '^' xor_expr
    // inclusive_or_expr
    // 	: xor_expr
    // 	| xor_expr '|' inclusive_or_expr
    // logical_and_expr
    // 	: inclusive_or_expr
    // 	| inclusive_or_expr '&&' logical_and_expr
    // logical_or_expr
    // 	: logical_and_expr
    // 	| logical_and_expr '||' logical_or_expr

    bool result = false;
    size_t save = t->idx;
    do {
        if (!_upf_parse_cast_expr(t, p)) {
            t->idx = save;
            return result;
        }

        result = true;
        save = t->idx;
    } while (_upf_consume(t, _UPF_TOK_MATH, _UPF_TOK_COMPARISON, _UPF_TOK_STAR, _UPF_TOK_AMPERSAND, _UPF_TOK_PLUS, _UPF_TOK_MINUS).kind
             != _UPF_TOK_NONE);
    return true;
}

static bool _upf_parse_assignment_expr(_upf_tokenizer *t, _upf_parser_state *p) {
    _UPF_ASSERT(t != NULL);
    // conditional_expr
    // 	: logical_or_expr
    // 	| logical_or_expr '?' expr ':' conditional_expr
    // assignment_expr
    // 	: conditional_expr
    // 	| unary_expr ASSIGNMENT_OP assignment_expr

    size_t save = t->idx;
    while (true) {
        if (!_upf_parse_math_expr(t, p)) break;
        if (_upf_consume(t, _UPF_TOK_QUESTION).kind == _UPF_TOK_NONE) return true;
        if (!_upf_parse_expr(t, NULL) || _upf_consume(t, _UPF_TOK_COLON).kind == _UPF_TOK_NONE) break;
    }
    t->idx = save;

    if (!_upf_parse_unary_expr(t, p) || _upf_consume(t, _UPF_TOK_ASSIGNMENT).kind == _UPF_TOK_NONE || !_upf_parse_assignment_expr(t, p)) {
        t->idx = save;
        return false;
    }
    return true;
}

static bool _upf_parse_postfix_expr(_upf_tokenizer *t, _upf_parser_state *p) {
    _UPF_ASSERT(t != NULL);
    // postfix_expr
    // 	: IDENTIFIER postfix_expr'
    // 	| STRING postfix_expr'
    //  | NUMBER postfix_expr'
    // 	| '(' expr ')' postfix_expr'

    bool is_base = false;
    int dereference_save = p ? p->dereference : 0;
    size_t save = t->idx;
    _upf_token token = _upf_next(t);
    switch (token.kind) {
        case _UPF_TOK_ID:
            if (p) {
                p->base = token.string;
                p->base_type = _UPF_BT_VARIABLE;
                is_base = true;
            }
            break;
        case _UPF_TOK_STRING:
        case _UPF_TOK_NUMBER:
            break;
        case _UPF_TOK_OPEN_PAREN:
            if (!_upf_parse_expr(t, p) || _upf_consume(t, _UPF_TOK_CLOSE_PAREN).kind == _UPF_TOK_NONE) {
                t->idx = save;
                return false;
            }
            break;
        default:
            _upf_back(t);
            return false;
    }

    // postfix_expr'
    // 	: '[' expr ']' postfix_expr'
    // 	| '(' ')' postfix_expr'
    // 	| '(' assignment_expr[] ')' postfix_expr'
    // 	| DOT IDENTIFIER postfix_expr'
    // 	| POSTFIX_OP postfix_expr'
    // 	| POSTFIX_OP postfix_expr'
    while (true) {
        token = _upf_next(t);
        if (token.kind == _UPF_TOK_NONE) return true;

        if (p && token.kind != _UPF_TOK_OPEN_PAREN) p->suffix_calls = 0;
        switch (token.kind) {
            case _UPF_TOK_OPEN_BRACKET:
                if (p) p->dereference++;
                if (!_upf_parse_expr(t, NULL) || _upf_consume(t, _UPF_TOK_CLOSE_BRACKET).kind == _UPF_TOK_NONE) {
                    t->idx = save;
                    return false;
                }
                break;
            case _UPF_TOK_OPEN_PAREN:
                while (_upf_parse_assignment_expr(t, NULL) && _upf_consume(t, _UPF_TOK_COMMA).kind != _UPF_TOK_NONE) continue;
                if (_upf_consume(t, _UPF_TOK_CLOSE_PAREN).kind == _UPF_TOK_NONE) {
                    t->idx = save;
                    return false;
                }

                if (p) {
                    if (is_base) p->base_type = _UPF_BT_FUNCTION;
                    p->dereference = dereference_save;
                    p->suffix_calls++;
                }
                break;
            case _UPF_TOK_DOT:
            case _UPF_TOK_ARROW:
                is_base = false;

                token = _upf_next(t);
                if (token.kind != _UPF_TOK_ID) {
                    t->idx = save;
                    return false;
                }

                if (p) {
                    p->dereference = dereference_save;
                    _UPF_VECTOR_PUSH(&p->members, token.string);
                }
                break;
            case _UPF_TOK_INCREMENT:
            case _UPF_TOK_DECREMENT:
                break;
            default:
                _upf_back(t);
                return true;
        }
    }
}

static bool _upf_parse_unary_expr(_upf_tokenizer *t, _upf_parser_state *p) {
    _UPF_ASSERT(t != NULL);
    // unary_expr
    // 	: postfix_expr
    // 	| PREFIX_OP unary_expr
    // 	| UNARY_OP cast_expr
    // 	| sizeof unary_expr
    // 	| sizeof '(' typename ')'
    // 	| alignof '(' typename ')'

    size_t save = t->idx;
    if (_upf_consume(t, _UPF_TOK_INCREMENT, _UPF_TOK_DECREMENT).kind != _UPF_TOK_NONE) {
        if (!_upf_parse_unary_expr(t, p)) {
            t->idx = save;
            return false;
        }
        return true;
    }

    if (_upf_consume(t, _UPF_TOK_PLUS, _UPF_TOK_MINUS, _UPF_TOK_TILDE, _UPF_TOK_EXCLAMATION).kind != _UPF_TOK_NONE) {
        if (!_upf_parse_cast_expr(t, p)) {
            t->idx = save;
            return false;
        }
        return true;
    }

    if (_upf_consume(t, _UPF_TOK_AMPERSAND).kind != _UPF_TOK_NONE) {
        if (!_upf_parse_cast_expr(t, p)) {
            t->idx = save;
            return false;
        }

        if (p) p->dereference--;
        return true;
    }

    if (_upf_consume(t, _UPF_TOK_STAR).kind != _UPF_TOK_NONE) {
        if (!_upf_parse_cast_expr(t, p)) {
            t->idx = save;
            return false;
        }

        if (p) p->dereference++;
        return true;
    }

    _upf_token token = _upf_consume(t, _UPF_TOK_ID);
    if (token.kind == _UPF_TOK_ID) {
        if (strcmp(token.string, "sizeof") == 0) {
            size_t save2 = t->idx;
            if (_upf_consume(t, _UPF_TOK_OPEN_PAREN).kind != _UPF_TOK_NONE && _upf_parse_typename(t, NULL, NULL)
                && _upf_consume(t, _UPF_TOK_CLOSE_PAREN).kind != _UPF_TOK_NONE) {
                return true;
            }
            t->idx = save2;

            if (!_upf_parse_unary_expr(t, p)) {
                t->idx = save;
                return false;
            }
            return true;
        }

        if (strcmp(token.string, "_Alignof") == 0 || strcmp(token.string, "alignof") == 0) {
            if (_upf_consume(t, _UPF_TOK_OPEN_PAREN).kind == _UPF_TOK_NONE || !_upf_parse_typename(t, NULL, NULL)
                || _upf_consume(t, _UPF_TOK_CLOSE_PAREN).kind == _UPF_TOK_NONE) {
                t->idx = save;
                return false;
            }
            return true;
        }

        _upf_back(t);
    }

    return _upf_parse_postfix_expr(t, p);
}

static bool _upf_parse_expr(_upf_tokenizer *t, _upf_parser_state *p) {
    _UPF_ASSERT(t != NULL);
    // expr
    // 	: assignment_expr
    // 	| assignment_expr ',' expr

    size_t save = t->idx;
    _upf_parser_state p_copy;
    do {
        if (p) p_copy = *p;
        if (!_upf_parse_assignment_expr(t, p ? &p_copy : NULL)) {
            t->idx = save;
            return false;
        }
    } while (_upf_consume(t, _UPF_TOK_COMMA).kind != _UPF_TOK_NONE);
    if (p) *p = p_copy;

    return true;
}

// ================== TYPE INFERENCE ======================

static size_t _upf_get_return_type(size_t type_idx, int count) {
    while (count-- > 0) {
        const _upf_type *type = _upf_get_type(type_idx);
        while (type->kind == _UPF_TK_POINTER) type = _upf_get_type(type->as.pointer.type);
        if (type->kind != _UPF_TK_FUNCTION) {
            _UPF_ERROR("Unable to get return type of \"%s\" because it is not a function pointer.", type->name);
        }
        type_idx = type->as.function.return_type;
    }

    return type_idx;
}

static const uint8_t *_upf_find_var_type(uint64_t pc, const char *var, const _upf_scope *scope) {
    _UPF_ASSERT(var != NULL && scope != NULL);

    if (!_upf_is_in_range(pc, scope->ranges)) return NULL;

    for (size_t i = 0; i < scope->scopes.length; i++) {
        const uint8_t *result = _upf_find_var_type(pc, var, &scope->scopes.data[i]);
        if (result != NULL) return result;
    }

    for (size_t i = 0; i < scope->vars.length; i++) {
        if (strcmp(scope->vars.data[i].name, var) == 0) {
            return scope->vars.data[i].die;
        }
    }

    return NULL;
}

static size_t _upf_get_member_type(const _upf_cstr_vec *member_names, size_t idx, size_t type_idx) {
    _UPF_ASSERT(member_names != NULL && type_idx != _UPF_INVALID);

    if (idx == member_names->length) return type_idx;

    const _upf_type *type = _upf_get_type(type_idx);
    if (type->kind == _UPF_TK_POINTER) {
        return _upf_get_member_type(member_names, idx, type->as.pointer.type);
    }

    if (type->kind == _UPF_TK_FUNCTION && idx < member_names->length) {
        size_t return_type_idx = type_idx;
        while (true) {
            const _upf_type *type = _upf_get_type(return_type_idx);
            if (type->kind == _UPF_TK_POINTER) type = _upf_get_type(type->as.pointer.type);
            if (type->kind != _UPF_TK_FUNCTION) break;
            return_type_idx = type->as.function.return_type;
        }
        return _upf_get_member_type(member_names, idx, return_type_idx);
    }

    if (type->kind == _UPF_TK_STRUCT || type->kind == _UPF_TK_UNION) {
        _upf_member_vec members = type->as.cstruct.members;
        for (size_t i = 0; i < members.length; i++) {
            if (strcmp(members.data[i].name, member_names->data[idx]) == 0) {
                return _upf_get_member_type(member_names, idx + 1, members.data[i].type);
            }
        }
    }

    _UPF_ERROR("Unable to find member \"%s\" in \"%s\".", member_names->data[idx], type->name);
}

static size_t _upf_find_typename(_upf_parser_state *p, uint64_t pc) {
    for (size_t i = 0; i < _upf_state.cus.length; i++) {
        const _upf_cu *cu = &_upf_state.cus.data[i];
        if (!_upf_is_in_range(pc, cu->scope.ranges)) continue;

        for (size_t j = 0; j < cu->types.length; j++) {
            if (strcmp(cu->types.data[j].name, p->base) == 0) {
                return _upf_parse_type(cu, cu->types.data[j].die);
            }
        }
    }

    return _UPF_INVALID;
}

static size_t _upf_find_variable(_upf_parser_state *p, uint64_t pc) {
    for (size_t i = 0; i < _upf_state.cus.length; i++) {
        const _upf_cu *cu = &_upf_state.cus.data[i];

        const uint8_t *type_die = _upf_find_var_type(pc, p->base, &cu->scope);
        if (type_die == NULL) continue;

        return _upf_parse_type(cu, type_die);
    }

    return _UPF_INVALID;
}

static size_t _upf_find_function(_upf_parser_state *p, uint64_t pc) {
    for (size_t i = 0; i < _upf_state.cus.length; i++) {
        const _upf_cu *cu = &_upf_state.cus.data[i];
        if (!_upf_is_in_range(pc, cu->scope.ranges)) continue;

        for (size_t j = 0; j < cu->functions.length; j++) {
            if (strcmp(cu->functions.data[j].name, p->base) == 0) {
                if (cu->functions.data[j].return_type == NULL) {
                    _upf_type type = {
                        .name = "void",
                        .kind = _UPF_TK_VOID,
                        .modifiers = 0,
                        .size = _UPF_INVALID,
                    };
                    return _upf_add_type(NULL, type);
                } else {
                    return _upf_parse_type(cu, cu->functions.data[j].return_type);
                }
            }
        }
    }

    return _UPF_INVALID;
}

static size_t _upf_get_base_type(_upf_parser_state *p, uint64_t pc, const char *arg) {
    size_t type_idx = _UPF_INVALID;
    switch (p->base_type) {
        case _UPF_BT_TYPENAME:
            type_idx = _upf_find_typename(p, pc);

            if (type_idx == _UPF_INVALID) {
                _UPF_ERROR(
                    "Unable to find type \"%s\" in \"%s\" at %s:%d. "
                    "Ensure that the executable contains debugging information of at least 2nd level (-g2 or -g3).",
                    p->base, arg, _upf_state.file, _upf_state.line);
            }
            break;
        case _UPF_BT_VARIABLE:
            type_idx = _upf_find_variable(p, pc);
            if (type_idx != _UPF_INVALID) break;

            if (_upf_find_function(p, pc) != _UPF_INVALID) {
                _upf_type type = {
                    .name = NULL,
                    .kind = _UPF_TK_FUNCTION,
                    .modifiers = 0,
                    .size = sizeof(void *),
                };
                type_idx = _upf_add_type(NULL, type);
            }

            if (type_idx == _UPF_INVALID) {
                _UPF_ERROR(
                    "Unable to find type of \"%s\" in \"%s\" at %s:%d. "
                    "Ensure that the executable contains debugging information of at least 2nd level (-g2 or -g3).",
                    p->base, arg, _upf_state.file, _upf_state.line);
            }
            break;
        case _UPF_BT_FUNCTION:
            type_idx = _upf_find_variable(p, pc);

            if (type_idx != _UPF_INVALID) {
                const _upf_type *type = _upf_get_type(type_idx);
                if (type->kind != _UPF_TK_POINTER) goto not_function_error;

                type = _upf_get_type(type->as.pointer.type);
                if (type->kind != _UPF_TK_FUNCTION) goto not_function_error;

                type_idx = type->as.function.return_type;
            } else {
                type_idx = _upf_find_function(p, pc);
            }

            if (type_idx == _UPF_INVALID) {
            not_function_error:
                _UPF_ERROR(
                    "Unable to find type of function \"%s\" in \"%s\" at %s:%d. "
                    "Ensure that the executable contains debugging information of at least 2nd level (-g2 or -g3).",
                    p->base, arg, _upf_state.file, _upf_state.line);
            }

            if (p->members.length == 0 && p->suffix_calls > 0) p->suffix_calls--;
            break;
    }

    _UPF_ASSERT(type_idx != _UPF_INVALID);
    return type_idx;
}

static size_t _upf_dereference_type(size_t type_idx, int dereference, const char *arg) {
    // Arguments are pointers to data that should be printed, so they get dereferenced
    // in order not to be interpreted as actual pointers.
    dereference++;

    while (dereference < 0) {
        _upf_type type = {
            .name = NULL,
            .kind = _UPF_TK_POINTER,
            .modifiers = 0,
            .size = sizeof(void*),
            .as.pointer = {
                .type = type_idx,
            },
        };

        type_idx = _upf_add_type(NULL, type);
        dereference++;
    }

    while (dereference > 0) {
        const _upf_type *type = _upf_get_type(type_idx);

        if (type->kind == _UPF_TK_POINTER) {
            type_idx = type->as.pointer.type;
            dereference--;
        } else if (type->kind == _UPF_TK_ARRAY) {
            int dimensions = type->as.array.lengths.length;
            if (dereference > dimensions) {
                goto not_pointer_error;
            } else if (dereference == dimensions) {
                type_idx = type->as.array.element_type;
            } else {
                // This may invalidate type pointer
                type_idx = _upf_add_type(NULL, _upf_get_subarray(type, dereference));
            }

            dereference = 0;
        } else {
        not_pointer_error:
            _UPF_ERROR("Arguments must be pointers to data that should be printed. You must take pointer (&) of \"%s\" at %s:%d.", arg,
                       _upf_state.file, _upf_state.line);
        }

        if (type_idx == _UPF_INVALID) {
            _UPF_ERROR(
                "Unable to print void* because it can point to arbitrary data of any length. "
                "To print the pointer itself, you must take pointer (&) of \"%s\" at %s:%d.",
                arg, _upf_state.file, _upf_state.line);
        }
    }

    return type_idx;
}

static const _upf_type *_upf_get_arg_type(const char *arg, uint64_t pc) {
    _UPF_ASSERT(arg != NULL);

    _upf_tokenizer t = {
        .tokens = _UPF_VECTOR_NEW(&_upf_state.arena),
        .idx = 0,
    };
    _upf_tokenize(&t, arg);

    _upf_parser_state p = {
        .dereference = 0,
        .suffix_calls = 0,
        .base = NULL,
        .base_type = 0,
        .members = _UPF_VECTOR_NEW(&_upf_state.arena),
    };
    if (!_upf_parse_expr(&t, &p) || t.idx != t.tokens.length) {
        _UPF_ERROR("Unable to parse argument \"%s\" at %s:%d.", arg, _upf_state.file, _upf_state.line);
    }

    size_t base_type = _upf_get_base_type(&p, pc, arg);
    size_t member_type = _upf_get_member_type(&p.members, 0, base_type);
    if (p.suffix_calls > 0) member_type = _upf_get_return_type(member_type, p.suffix_calls);
    size_t type = _upf_dereference_type(member_type, p.dereference, arg);

    _UPF_ASSERT(type != _UPF_INVALID);
    return _upf_get_type(type);
}

// ===================== GETTING PC =======================

// Function returns the address to which this executable is mapped to.
// It is retrieved by reading `/proc/self/maps` (see `man proc_pid_maps`).
// It is used to convert between DWARF addresses and runtime/real ones: DWARF
// addresses are relative to the beginning of the file, thus real = base + dwarf.
static void *_upf_get_this_file_address(void) {
    static const ssize_t PATH_BUFFER_SIZE = 1024;

    char this_path[PATH_BUFFER_SIZE];
    ssize_t read = readlink("/proc/self/exe", this_path, PATH_BUFFER_SIZE);
    if (read == -1) _UPF_ERROR("Unable to readlink \"/proc/self/exe\": %s.", strerror(errno));
    if (read == PATH_BUFFER_SIZE) _UPF_ERROR("Unable to readlink \"/proc/self/exe\": path is too long.");
    this_path[read] = '\0';

    FILE *file = fopen("/proc/self/maps", "r");
    if (file == NULL) _UPF_ERROR("Unable to open \"/proc/self/maps\": %s.", strerror(errno));

    uint64_t address = _UPF_INVALID;
    size_t length = 0;
    char *line = NULL;
    while ((read = getline(&line, &length, file)) != -1) {
        if (read == 0) continue;
        if (line[read - 1] == '\n') line[read - 1] = '\0';

        int path_offset;
        if (sscanf(line, "%lx-%*x %*s %*x %*x:%*x %*u %n", &address, &path_offset) != 1) {
            _UPF_ERROR("Unable to parse \"/proc/self/maps\": invalid format.");
        }

        if (strcmp(this_path, line + path_offset) == 0) break;
    }
    if (line) free(line);
    fclose(file);

    _UPF_ASSERT(address != _UPF_INVALID);
    return (void *) address;
}

__attribute__((noinline)) static uint64_t _upf_get_pc(void) {
    return (uint64_t) __builtin_extract_return_addr(__builtin_return_address(0));
}

// ================== /proc/pid/maps ======================

static _upf_range_vec _upf_get_address_ranges(void) {
    FILE *file = fopen("/proc/self/maps", "r");
    if (file == NULL) _UPF_ERROR("Unable to open \"/proc/self/maps\": %s.", strerror(errno));

    _upf_range_vec ranges = _UPF_VECTOR_NEW(&_upf_state.arena);
    _upf_range range = {
        .start = _UPF_INVALID,
        .end = _UPF_INVALID,
    };
    size_t length = 0;
    char *line = NULL;
    ssize_t read;
    while ((read = getline(&line, &length, file)) != -1) {
        if (read == 0) continue;
        if (line[read - 1] == '\n') line[read - 1] = '\0';

        if (sscanf(line, "%lx-%lx %*s %*x %*x:%*x %*u", &range.start, &range.end) != 2) {
            _UPF_ERROR("Unable to parse \"/proc/self/maps\": invalid format.");
        }

        _UPF_VECTOR_PUSH(&ranges, range);
    }
    if (line) free(line);
    fclose(file);

    return ranges;
}

static const void *_upf_get_memory_region_end(const void *ptr) {
    for (size_t i = 0; i < _upf_state.addresses.length; i++) {
        _upf_range range = _upf_state.addresses.data[i];
        if ((void *) range.start <= ptr && ptr <= (void *) range.end) {
            return (void *) range.end;
        }
    }
    return NULL;
}

// ===================== PRINTING =========================

// All the printing is done to the global buffer stored in the _upf_state, which
// is why calls don't accept or return string pointers.

#define _UPF_INITIAL_BUFFER_SIZE 512

#define _upf_bprintf(...)                                                                         \
    while (true) {                                                                                \
        int bytes = snprintf(_upf_state.ptr, _upf_state.free, __VA_ARGS__);                       \
        if (bytes < 0) _UPF_ERROR("Unexpected error occurred in snprintf: %s.", strerror(errno)); \
        if ((size_t) bytes >= _upf_state.free) {                                                  \
            size_t used = _upf_state.size - _upf_state.free;                                      \
            _upf_state.size *= 2;                                                                 \
            _upf_state.buffer = (char *) realloc(_upf_state.buffer, _upf_state.size);             \
            if (_upf_state.buffer == NULL) _UPF_OUT_OF_MEMORY();                                  \
            _upf_state.ptr = _upf_state.buffer + used;                                            \
            _upf_state.free = _upf_state.size - used;                                             \
            continue;                                                                             \
        }                                                                                         \
        _upf_state.free -= bytes;                                                                 \
        _upf_state.ptr += bytes;                                                                  \
        break;                                                                                    \
    }

static const char *_upf_escape_char(char ch) {
    // clang-format off
    switch (ch) {
        case '\a': return "\\a";
        case '\b': return "\\b";
        case '\f': return "\\f";
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        case '\v': return "\\v";
        case '\\': return "\\\\";
    }
    // clang-format on

    static char str[2];
    str[0] = ch;
    str[1] = '\0';
    return str;
}

static bool _upf_is_printable(char c) { return ' ' <= c && c <= '~'; }

static void _upf_print_modifiers(int modifiers) {
    if (modifiers & _UPF_MOD_CONST) _upf_bprintf("const ");
    if (modifiers & _UPF_MOD_VOLATILE) _upf_bprintf("volatile ");
    if (modifiers & _UPF_MOD_RESTRICT) _upf_bprintf("restrict ");
    if (modifiers & _UPF_MOD_ATOMIC) _upf_bprintf("atomic ");
}

static void _upf_print_typename(const _upf_type *type, bool print_trailing_whitespace) {
    _UPF_ASSERT(type != NULL);
    switch (type->kind) {
        case _UPF_TK_POINTER: {
            if (type->as.pointer.type == _UPF_INVALID) {
                _upf_bprintf("void *");
                _upf_print_modifiers(type->modifiers);
                break;
            }

            const _upf_type *pointer_type = _upf_get_type(type->as.pointer.type);
            if (pointer_type->kind == _UPF_TK_FUNCTION) {
                _upf_print_typename(pointer_type, print_trailing_whitespace);
                break;
            }

            _upf_print_typename(pointer_type, true);
            _upf_bprintf("*");
            _upf_print_modifiers(type->modifiers);
        } break;
        case _UPF_TK_FUNCTION:
            if (type->as.function.return_type == _UPF_INVALID) {
                _upf_bprintf("void");
            } else {
                _upf_print_typename(_upf_get_type(type->as.function.return_type), false);
            }

            _upf_bprintf("(");
            for (size_t i = 0; i < type->as.function.arg_types.length; i++) {
                if (i > 0) _upf_bprintf(", ");
                _upf_print_typename(_upf_get_type(type->as.function.arg_types.data[i]), false);
            }
            _upf_bprintf(")");
            if (print_trailing_whitespace) _upf_bprintf(" ");
            break;
        default:
            _upf_print_modifiers(type->modifiers);
            if (type->name) {
                _upf_bprintf("%s", type->name);
            } else {
                _upf_bprintf("<unnamed>");
            }
            if (print_trailing_whitespace) _upf_bprintf(" ");
            break;
    }
}

static void _upf_print_bit_field(const uint8_t *data, int total_bit_offset, int bit_size) {
    int byte_offset = total_bit_offset / 8;
    int bit_offset = total_bit_offset % 8;

    uint8_t value;
    memcpy(&value, data + byte_offset, sizeof(value));
    value = (value >> bit_offset) & ((1 << bit_size) - 1);
    _upf_bprintf("%hhu <%d bit%s>", value, bit_size, bit_size > 1 ? "s" : "");
}

__attribute__((no_sanitize_address)) static void _upf_print_char_ptr(const char *str) {
    const char *end = _upf_get_memory_region_end(str);
    if (end == NULL) {
        _upf_bprintf("%p (<out-of-bounds>)", (void *) str);
        return;
    }

    bool is_limited = UPRINTF_MAX_STRING_LENGTH > 0;
    bool is_truncated = false;
    const char *max_str = str + UPRINTF_MAX_STRING_LENGTH;
    _upf_bprintf("%p (\"", (void *) str);
    while (*str != '\0' && str < end) {
        if (is_limited && str >= max_str) {
            is_truncated = true;
            break;
        }
        _upf_bprintf("%s", _upf_escape_char(*str));
        str++;  // Increment inside of macro (_upf_bprintf) may be triggered twice
    }
    _upf_bprintf("\"");
    if (is_truncated) _upf_bprintf("...");
    _upf_bprintf(")");
}

static void _upf_collect_circular_structs(_upf_indexed_struct_vec *seen, _upf_indexed_struct_vec *circular, const uint8_t *data,
                                          const _upf_type *type, int depth) {
    _UPF_ASSERT(seen != NULL && circular != NULL && type != NULL);

    if (UPRINTF_MAX_DEPTH >= 0 && depth >= UPRINTF_MAX_DEPTH) return;
    if (data == NULL || _upf_get_memory_region_end(data) == NULL) return;

    if (type->kind == _UPF_TK_POINTER) {
        void *ptr;
        memcpy(&ptr, data, sizeof(ptr));
        if (ptr == NULL || type->as.pointer.type == _UPF_INVALID) return;

        const _upf_type *pointed_type = _upf_get_type(type->as.pointer.type);

        _upf_collect_circular_structs(seen, circular, ptr, pointed_type, depth);
        return;
    }

    if (type->kind != _UPF_TK_STRUCT && type->kind != _UPF_TK_UNION) return;

    for (size_t i = 0; i < circular->length; i++) {
        if (data == circular->data[i].data && type == circular->data[i].type) {
            return;
        }
    }

    for (size_t i = 0; i < seen->length; i++) {
        if (data == seen->data[i].data && type == seen->data[i].type) {
            _UPF_VECTOR_PUSH(circular, seen->data[i]);
            return;
        }
    }

    _upf_indexed_struct indexed_struct = {
        .data = data,
        .type = type,
        .is_visited = false,
        .id = -1,
    };
    _UPF_VECTOR_PUSH(seen, indexed_struct);

    _upf_member_vec members = type->as.cstruct.members;
    for (size_t i = 0; i < members.length; i++) {
        const _upf_member *member = &members.data[i];
        if (member->bit_size != 0) continue;

        _upf_collect_circular_structs(seen, circular, data + member->offset, _upf_get_type(member->type), depth + 1);
    }
}

static void _upf_print_type(_upf_indexed_struct_vec *circular, const uint8_t *data, const _upf_type *type, int depth) {
    _UPF_ASSERT(type != NULL);

    if (UPRINTF_MAX_DEPTH >= 0 && depth >= UPRINTF_MAX_DEPTH) {
        switch (type->kind) {
            case _UPF_TK_UNION:
            case _UPF_TK_STRUCT:
                _upf_bprintf("{...}");
                return;
            default:
                break;
        }
    }

    if (type->kind == _UPF_TK_UNKNOWN) {
        _upf_bprintf("<unknown>");
        return;
    }

    if (data == NULL) {
        _upf_bprintf("NULL");
        return;
    }

    if (_upf_get_memory_region_end(data) == NULL) {
        _upf_bprintf("<out-of-bounds>");
        return;
    }

    switch (type->kind) {
        case _UPF_TK_UNION:
            _upf_bprintf("<union> ");
            __attribute__((fallthrough));  // Handle union as struct
        case _UPF_TK_STRUCT: {
#if UPRINTF_IGNORE_STDIO_FILE
            if (strcmp(type->name, "FILE") == 0) {
                _upf_bprintf("<ignored>");
                return;
            }
#endif

            _upf_member_vec members = type->as.cstruct.members;

            if (members.length == 0) {
                _upf_bprintf("{}");
                return;
            }

            for (size_t i = 0; i < circular->length; i++) {
                if (data == circular->data[i].data && type == circular->data[i].type) {
                    if (circular->data[i].is_visited) {
                        _upf_bprintf("<points to #%d>", circular->data[i].id);
                        return;
                    }

                    circular->data[i].is_visited = true;
                    circular->data[i].id = _upf_state.circular_id++;
                    _upf_bprintf("<#%d> ", circular->data[i].id);
                }
            }

            _upf_bprintf("{\n");
            for (size_t i = 0; i < members.length; i++) {
                const _upf_member *member = &members.data[i];
                const _upf_type *member_type = _upf_get_type(member->type);

                _upf_bprintf("%*s", UPRINTF_INDENTATION_WIDTH * (depth + 1), "");
                _upf_print_typename(member_type, true);
                _upf_bprintf("%s = ", member->name);
                if (member->bit_size == 0) {
                    _upf_print_type(circular, data + member->offset, member_type, depth + 1);
                } else {
                    _upf_print_bit_field(data, member->offset, member->bit_size);
                }
                _upf_bprintf("\n");
            }
            _upf_bprintf("%*s}", UPRINTF_INDENTATION_WIDTH * depth, "");
        } break;
        case _UPF_TK_ENUM: {
            _upf_enum_vec enums = type->as.cenum.enums;
            const _upf_type *underlying_type = _upf_get_type(type->as.cenum.underlying_type);

            int64_t enum_value;
            if (underlying_type->kind == _UPF_TK_U4) {
                uint32_t temp;
                memcpy(&temp, data, sizeof(temp));
                enum_value = temp;
            } else if (underlying_type->kind == _UPF_TK_S4) {
                int32_t temp;
                memcpy(&temp, data, sizeof(temp));
                enum_value = temp;
            } else {
                _UPF_WARN("Expected enum to use int32_t or uint32_t. Ignoring this type.");
                _upf_bprintf("<enum>");
                break;
            }

            const char *name = NULL;
            for (size_t i = 0; i < enums.length; i++) {
                if (enum_value == enums.data[i].value) {
                    name = enums.data[i].name;
                    break;
                }
            }

            _upf_bprintf("%s (", name ? name : "<unknown>");
            _upf_print_type(circular, data, underlying_type, depth);
            _upf_bprintf(")");
        } break;
        case _UPF_TK_ARRAY: {
            const _upf_type *element_type = _upf_get_type(type->as.array.element_type);
            size_t element_size = element_type->size;

            if (element_size == _UPF_INVALID) {
                _upf_bprintf("<unknown>");
                return;
            }

            if (type->as.array.lengths.length == 0) {
                _upf_bprintf("<non-static array>");
                return;
            }

            _upf_type subarray;
            if (type->as.array.lengths.length > 1) {
                subarray = _upf_get_subarray(type, 1);
                element_type = &subarray;

                for (size_t i = 0; i < subarray.as.array.lengths.length; i++) {
                    element_size *= subarray.as.array.lengths.data[i];
                }
            }

            bool is_primitive = _upf_is_primitive(element_type);
            _upf_bprintf(is_primitive ? "[" : "[\n");
            for (size_t i = 0; i < type->as.array.lengths.data[0]; i++) {
                if (i > 0) _upf_bprintf(is_primitive ? ", " : ",\n");
                if (!is_primitive) _upf_bprintf("%*s", UPRINTF_INDENTATION_WIDTH * (depth + 1), "");

                const uint8_t *current = data + element_size * i;
                _upf_print_type(circular, current, element_type, depth + 1);

#if UPRINTF_ARRAY_COMPRESSION_THRESHOLD > 0
                size_t j = i;
                while (j < type->as.array.lengths.data[0] && memcmp(current, data + element_size * j, element_size) == 0) j++;

                int count = j - i;
                if (j - i >= UPRINTF_ARRAY_COMPRESSION_THRESHOLD) {
                    _upf_bprintf(" <repeats %d times>", count);
                    i = j - 1;
                }
#endif
            }

            if (is_primitive) {
                _upf_bprintf("]");
            } else {
                _upf_bprintf("\n%*s]", UPRINTF_INDENTATION_WIDTH * depth, "");
            }
        } break;
        case _UPF_TK_POINTER: {
            void *ptr;
            memcpy(&ptr, data, sizeof(ptr));
            if (ptr == NULL) {
                _upf_bprintf("NULL");
                return;
            }

            if (type->as.pointer.type == _UPF_INVALID) {
                _upf_bprintf("%p", ptr);
                return;
            }

            const _upf_type *pointed_type = _upf_get_type(type->as.pointer.type);
            if (pointed_type->kind == _UPF_TK_POINTER || pointed_type->kind == _UPF_TK_VOID) {
                _upf_bprintf("%p", ptr);
                return;
            }

            if (pointed_type->kind == _UPF_TK_FUNCTION) {
                _upf_print_type(circular, ptr, pointed_type, depth);
                break;
            }

            if (pointed_type->kind == _UPF_TK_SCHAR || pointed_type->kind == _UPF_TK_UCHAR) {
                _upf_print_char_ptr(ptr);
                return;
            }

            _upf_bprintf("%p (", ptr);
            _upf_print_type(circular, ptr, pointed_type, depth);
            _upf_bprintf(")");
        } break;
        case _UPF_TK_FUNCTION: {
            _upf_function *function = NULL;
            _upf_cu *cu = NULL;
            uint64_t function_pc = data - _upf_state.pc_base;
            for (uint32_t i = 0; i < _upf_state.cus.length; i++) {
                cu = &_upf_state.cus.data[i];
                for (uint32_t j = 0; j < cu->functions.length; j++) {
                    if (function_pc == cu->functions.data[j].low_pc) {
                        function = &cu->functions.data[j];
                        break;
                    }
                }
                if (function) break;
            }

            _upf_bprintf("%p", (void *) data);
            if (function != NULL) {
                _UPF_ASSERT(cu != NULL);

                _upf_bprintf(" <");

                size_t return_type_idx;
                if (function->return_type == NULL) {
                    _upf_type type = {
                        .name = "void",
                        .kind = _UPF_TK_VOID,
                        .modifiers = 0,
                        .size = _UPF_INVALID,
                    };
                    return_type_idx = _upf_add_type(NULL, type);
                } else {
                    return_type_idx = _upf_parse_type(cu, function->return_type);
                }
                _upf_print_typename(_upf_get_type(return_type_idx), true);
                _upf_bprintf("%s(", function->name);
                for (uint32_t i = 0; i < function->args.length; i++) {
                    if (i > 0) _upf_bprintf(", ");
                    size_t arg_type_idx = _upf_parse_type(cu, function->args.data[i].die);
                    bool has_name = function->args.data[i].name != NULL;
                    _upf_print_typename(_upf_get_type(arg_type_idx), has_name);
                    if (has_name) _upf_bprintf("%s", function->args.data[i].name);
                }
                if (function->is_variadic) {
                    if (function->args.length > 0) _upf_bprintf(", ");
                    _upf_bprintf("...");
                }
                _upf_bprintf(")>");
            }
        } break;
        case _UPF_TK_U1:
            _upf_bprintf("%hhu", *data);
            break;
        case _UPF_TK_U2: {
            uint16_t temp;
            memcpy(&temp, data, sizeof(temp));
            _upf_bprintf("%hu", temp);
        } break;
        case _UPF_TK_U4: {
            uint32_t temp;
            memcpy(&temp, data, sizeof(temp));
            _upf_bprintf("%u", temp);
        } break;
        case _UPF_TK_U8: {
            uint64_t temp;
            memcpy(&temp, data, sizeof(temp));
            _upf_bprintf("%lu", temp);
        } break;
        case _UPF_TK_S1: {
            int8_t temp;
            memcpy(&temp, data, sizeof(temp));
            _upf_bprintf("%hhd", temp);
        } break;
        case _UPF_TK_S2: {
            int16_t temp;
            memcpy(&temp, data, sizeof(temp));
            _upf_bprintf("%hd", temp);
        } break;
        case _UPF_TK_S4: {
            int32_t temp;
            memcpy(&temp, data, sizeof(temp));
            _upf_bprintf("%d", temp);
        } break;
        case _UPF_TK_S8: {
            int64_t temp;
            memcpy(&temp, data, sizeof(temp));
            _upf_bprintf("%ld", temp);
        } break;
        case _UPF_TK_F4: {
            float temp;
            memcpy(&temp, data, sizeof(temp));
            _upf_bprintf("%f", temp);
        } break;
        case _UPF_TK_F8: {
            double temp;
            memcpy(&temp, data, sizeof(temp));
            _upf_bprintf("%lf", temp);
        } break;
        case _UPF_TK_BOOL:
            _upf_bprintf("%s", *data ? "true" : "false");
            break;
        case _UPF_TK_SCHAR: {
            char ch = *((char *) data);
            _upf_bprintf("%hhd", ch);
            if (_upf_is_printable(ch)) _upf_bprintf(" ('%s')", _upf_escape_char(ch));
        } break;
        case _UPF_TK_UCHAR: {
            char ch = *((char *) data);
            _upf_bprintf("%hhu", ch);
            if (_upf_is_printable(ch)) _upf_bprintf(" ('%s')", _upf_escape_char(ch));
        } break;
        case _UPF_TK_VOID:
            _UPF_WARN("void must be a pointer. Ignoring this type.");
            break;
        case _UPF_TK_UNKNOWN:
            _upf_bprintf("<unknown>");
            break;
    }
}

// =================== ENTRY POINTS =======================

__attribute__((constructor)) void _upf_init(void) {
    if (setjmp(_upf_state.jmp_buf) != 0) return;

    if (access("/proc/self/exe", R_OK) != 0) _UPF_ERROR("Expected \"/proc/self/exe\" to be a valid path.");
    if (access("/proc/self/maps", R_OK) != 0) _UPF_ERROR("Expected \"/proc/self/maps\" to be a valid path.");

    _upf_arena_init(&_upf_state.arena);
    _UPF_VECTOR_INIT(&_upf_state.cus, &_upf_state.arena);
    _UPF_VECTOR_INIT(&_upf_state.type_map, &_upf_state.arena);

    _upf_parse_elf();
    _upf_parse_dwarf();

    _upf_state.is_init = true;
}

__attribute__((destructor)) void _upf_fini(void) {
    // Must be unloaded at the end of the program because many variables point
    // into the _upf_state.dwarf.file to avoid unnecessarily copying date.
    if (_upf_state.dwarf.file != NULL) munmap(_upf_state.dwarf.file, _upf_state.dwarf.file_size);
    if (_upf_state.buffer != NULL) free(_upf_state.buffer);
    _upf_arena_free(&_upf_state.arena);
}

__attribute__((noinline)) void _upf_uprintf(const char *file, int line, const char *fmt, const char *args_string, ...) {
    _UPF_ASSERT(file != NULL && line > 0 && fmt != NULL && args_string != NULL);

    if (!_upf_state.is_init) return;
    if (setjmp(_upf_state.jmp_buf) != 0) return;

    if (_upf_state.buffer == NULL) {
        _upf_state.size = _UPF_INITIAL_BUFFER_SIZE;
        _upf_state.buffer = (char *) malloc(_upf_state.size * sizeof(*_upf_state.buffer));
        if (_upf_state.buffer == NULL) _UPF_OUT_OF_MEMORY();
    }
    _upf_state.ptr = _upf_state.buffer;
    _upf_state.free = _upf_state.size;
    _upf_state.addresses = _upf_get_address_ranges();
    _upf_state.circular_id = 0;
    _upf_state.file = file;
    _upf_state.line = line;

    if (!_upf_state.init_pc) {
        _upf_state.init_pc = true;

        _UPF_ASSERT(_upf_state.uprintf_ranges.length > 0);
        bool is_pc_absolute = _upf_is_in_range(_upf_get_pc(), _upf_state.uprintf_ranges);
        if (is_pc_absolute) {
            _upf_state.pc_base = NULL;
        } else {
            _upf_state.pc_base = _upf_get_this_file_address();
        }
    }

    uint8_t *pc_ptr = __builtin_extract_return_addr(__builtin_return_address(0));
    _UPF_ASSERT(pc_ptr != NULL);
    uint64_t pc = pc_ptr - _upf_state.pc_base;

    _upf_indexed_struct_vec seen = _UPF_VECTOR_NEW(&_upf_state.arena);
    _upf_indexed_struct_vec circular = _UPF_VECTOR_NEW(&_upf_state.arena);
    char *args_string_copy = _upf_arena_string(&_upf_state.arena, args_string, args_string + strlen(args_string));
    _upf_cstr_vec args = _upf_get_args(args_string_copy);
    size_t arg_idx = 0;

    va_list va_args;
    va_start(va_args, args_string);
    const char *ch = fmt;
    while (true) {
        const char *start = ch;
        while (*ch != '%' && *ch != '\0') ch++;
        _upf_bprintf("%.*s", (int) (ch - start), start);

        if (*ch == '\0') break;
        ch++;  // Skip percent sign

        if (*ch == '%') {
            _upf_bprintf("%%");
        } else if (('a' <= *ch && *ch <= 'z') || ('A' <= *ch && *ch <= 'Z')) {
            if (arg_idx >= args.length) _UPF_ERROR("There are more format specifiers than arguments provided at %s:%d.", file, line);

            const void *ptr = va_arg(va_args, void *);
            const _upf_type *type = _upf_get_arg_type(args.data[arg_idx++], pc);
            seen.length = 0;
            circular.length = 0;
            _upf_collect_circular_structs(&seen, &circular, ptr, type, 0);
            _upf_print_type(&circular, ptr, type, 0);
        } else if (*ch == '\n' || *ch == '\0') {
            _UPF_ERROR("Unfinished format specifier at the end of the line at %s:%d.", file, line);
        } else {
            _UPF_ERROR("Unknown format specifier \"%%%c\" at %s:%d.", *ch, file, line);
        }

        ch++;
    }
    va_end(va_args);

    if (arg_idx < args.length) _UPF_ERROR("There are more arguments provided than format specifiers at %s:%d.", file, line);

    printf("%s", _upf_state.buffer);
    fflush(stdout);
}

// ====================== UNDEF ===========================

#undef _UPF_DW_UT_compile
#undef _UPF_DW_TAG_array_type
#undef _UPF_DW_TAG_enumeration_type
#undef _UPF_DW_TAG_formal_parameter
#undef _UPF_DW_TAG_lexical_block
#undef _UPF_DW_TAG_member
#undef _UPF_DW_TAG_pointer_type
#undef _UPF_DW_TAG_compile_unit
#undef _UPF_DW_TAG_structure_type
#undef _UPF_DW_TAG_subroutine_type
#undef _UPF_DW_TAG_typedef
#undef _UPF_DW_TAG_union_type
#undef _UPF_DW_TAG_unspecified_parameters
#undef _UPF_DW_TAG_inlined_subroutine
#undef _UPF_DW_TAG_subrange_type
#undef _UPF_DW_TAG_base_type
#undef _UPF_DW_TAG_const_type
#undef _UPF_DW_TAG_enumerator
#undef _UPF_DW_TAG_subprogram
#undef _UPF_DW_TAG_variable
#undef _UPF_DW_TAG_volatile_type
#undef _UPF_DW_TAG_restrict_type
#undef _UPF_DW_TAG_atomic_type
#undef _UPF_DW_TAG_call_site
#undef _UPF_DW_TAG_call_site_parameter
#undef _UPF_DW_FORM_addr
#undef _UPF_DW_FORM_block2
#undef _UPF_DW_FORM_block4
#undef _UPF_DW_FORM_data2
#undef _UPF_DW_FORM_data4
#undef _UPF_DW_FORM_data8
#undef _UPF_DW_FORM_string
#undef _UPF_DW_FORM_block
#undef _UPF_DW_FORM_block1
#undef _UPF_DW_FORM_data1
#undef _UPF_DW_FORM_flag
#undef _UPF_DW_FORM_sdata
#undef _UPF_DW_FORM_strp
#undef _UPF_DW_FORM_udata
#undef _UPF_DW_FORM_ref_addr
#undef _UPF_DW_FORM_ref1
#undef _UPF_DW_FORM_ref2
#undef _UPF_DW_FORM_ref4
#undef _UPF_DW_FORM_ref8
#undef _UPF_DW_FORM_ref_udata
#undef _UPF_DW_FORM_indirect
#undef _UPF_DW_FORM_sec_offset
#undef _UPF_DW_FORM_exprloc
#undef _UPF_DW_FORM_flag_present
#undef _UPF_DW_FORM_strx
#undef _UPF_DW_FORM_addrx
#undef _UPF_DW_FORM_ref_sup4
#undef _UPF_DW_FORM_strp_sup
#undef _UPF_DW_FORM_data16
#undef _UPF_DW_FORM_line_strp
#undef _UPF_DW_FORM_ref_sig8
#undef _UPF_DW_FORM_implicit_const
#undef _UPF_DW_FORM_loclistx
#undef _UPF_DW_FORM_rnglistx
#undef _UPF_DW_FORM_ref_sup8
#undef _UPF_DW_FORM_strx1
#undef _UPF_DW_FORM_strx2
#undef _UPF_DW_FORM_strx3
#undef _UPF_DW_FORM_strx4
#undef _UPF_DW_FORM_addrx1
#undef _UPF_DW_FORM_addrx2
#undef _UPF_DW_FORM_addrx3
#undef _UPF_DW_FORM_addrx4
#undef _UPF_DW_AT_name
#undef _UPF_DW_AT_byte_size
#undef _UPF_DW_AT_bit_offset
#undef _UPF_DW_AT_bit_size
#undef _UPF_DW_AT_low_pc
#undef _UPF_DW_AT_high_pc
#undef _UPF_DW_AT_language
#undef _UPF_DW_AT_const_value
#undef _UPF_DW_AT_upper_bound
#undef _UPF_DW_AT_abstract_origin
#undef _UPF_DW_AT_count
#undef _UPF_DW_AT_data_member_location
#undef _UPF_DW_AT_encoding
#undef _UPF_DW_AT_type
#undef _UPF_DW_AT_ranges
#undef _UPF_DW_AT_data_bit_offset
#undef _UPF_DW_AT_str_offsets_base
#undef _UPF_DW_AT_addr_base
#undef _UPF_DW_AT_rnglists_base
#undef _UPF_DW_ATE_address
#undef _UPF_DW_ATE_boolean
#undef _UPF_DW_ATE_complex_float
#undef _UPF_DW_ATE_float
#undef _UPF_DW_ATE_signed
#undef _UPF_DW_ATE_signed_char
#undef _UPF_DW_ATE_unsigned
#undef _UPF_DW_ATE_unsigned_char
#undef _UPF_DW_ATE_imaginary_float
#undef _UPF_DW_ATE_packed_decimal
#undef _UPF_DW_ATE_numeric_string
#undef _UPF_DW_ATE_edited
#undef _UPF_DW_ATE_signed_fixed
#undef _UPF_DW_ATE_unsigned_fixed
#undef _UPF_DW_ATE_decimal_float
#undef _UPF_DW_ATE_UTF
#undef _UPF_DW_ATE_UCS
#undef _UPF_DW_ATE_ASCII
#undef _UPF_DW_RLE_end_of_list
#undef _UPF_DW_RLE_base_addressx
#undef _UPF_DW_RLE_startx_endx
#undef _UPF_DW_RLE_startx_length
#undef _UPF_DW_RLE_offset_pair
#undef _UPF_DW_RLE_base_address
#undef _UPF_DW_RLE_start_end
#undef _UPF_DW_RLE_start_length
#undef _UPF_DW_LANG_C
#undef _UPF_DW_LANG_C89
#undef _UPF_DW_LANG_C99
#undef _UPF_DW_LANG_C11
#undef _UPF_DW_LANG_C17
#undef _UPF_SET_TEST_STATUS
#undef _UPF_INVALID
#undef _UPF_LOG
#undef _UPF_ERROR
#undef _UPF_WARN
#undef _UPF_ASSERT
#undef _UPF_OUT_OF_MEMORY
#undef _UPF_UNREACHABLE
#undef _UPF_INITIAL_VECTOR_CAPACITY
#undef _UPF_VECTOR_TYPEDEF
#undef _UPF_VECTOR_NEW
#undef _UPF_VECTOR_INIT
#undef _UPF_VECTOR_PUSH
#undef _UPF_VECTOR_COPY
#undef _UPF_VECTOR_TOP
#undef _UPF_VECTOR_POP
#undef _UPF_MOD_CONST
#undef _UPF_MOD_VOLATILE
#undef _UPF_MOD_RESTRICT
#undef _UPF_MOD_ATOMIC
#undef _UPF_INITIAL_ARENA_SIZE
#undef _upf_arena_concat
#undef _upf_consume
#undef _UPF_INITIAL_BUFFER_SIZE
#undef _upf_bprintf

#endif  // UPRINTF_IMPLEMENTATION
