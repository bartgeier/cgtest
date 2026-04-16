/* see license at the end of file */

/*** Start of inlined file: arq_int.h ***/
#ifndef ARQ_STDINT_H
#define ARQ_STDINT_H

#include <stddef.h>  /* for size_t, ptrdiff_t */

#if defined(__cplusplus) || defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    /* C++ C99 */
    #include <stdint.h>
    #if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__)
        #define ARQ64
    #else
        #define ARQ32
    #endif
#else
    /* C89 */
    typedef signed char        int8_t;
    typedef unsigned char      uint8_t;

    typedef short              int16_t;
    typedef unsigned short     uint16_t;

    typedef int                int32_t;
    typedef unsigned int       uint32_t;

    typedef int8_t   int_least8_t;
    typedef int16_t  int_least16_t;
    typedef int32_t  int_least32_t;

    typedef uint8_t  uint_least8_t;
    typedef uint16_t uint_least16_t;
    typedef uint32_t uint_least32_t;

    #define INT8_MIN   (-128)
    #define INT8_MAX   127
    #define UINT8_MAX  255

    #define INT16_MIN  (-32768)
    #define INT16_MAX  32767
    #define UINT16_MAX 65535

    #define INT32_MIN  (-2147483647 - 1)
    #define INT32_MAX  2147483647
    #define UINT32_MAX 4294967295U

    /* ----------------- 32-bit vs 64-bit detection ----------------- */
    #if defined(_WIN64) || defined(__x86_64__) || defined(__ppc64__) || defined(__aarch64__)
        #define ARQ64
        #ifndef UINT64_T_DEFINED
            typedef size_t     uint64_t;
            typedef ptrdiff_t  int64_t;
            #define UINT64_T_DEFINED
        #endif
        #define UINT64_MAX ((size_t)-1)
        #define INT64_MAX  ((ptrdiff_t)(UINT64_MAX >> 1))
        #define INT64_MIN  (-INT64_MAX - 1)
    #else
        #define ARQ32
    #endif

    /* ----------------- Pointer-sized integer ----------------- */
    #ifndef UINTPTR_T_DEFINED
        #if defined(_MSC_VER)
            typedef unsigned __int64 uintptr_t;  /* MSVC 64-bit safe */
        #else
            typedef size_t uintptr_t;            /* GCC/Clang: pointer size */
        #endif
        #define UINTPTR_T_DEFINED
    #endif

#endif

#endif /* ARQ_STDINT_H */

/*** End of inlined file: arq_int.h ***/



/*** Start of inlined file: arq_main.h ***/
#ifndef ARQ_H
#define ARQ_H

typedef struct Arq_Queue_tag Arq_Queue;

typedef void (*function_pointer_t)(Arq_Queue *queue);
typedef struct {
        char chr;                /* -v        */
        const char *name;        /* --version */
        function_pointer_t fn;
        const char *arguments;   /* "uint8_t, bool = false" */
} Arq_Option;

#ifdef __cplusplus
extern "C" {
#endif

uint32_t arq_verify(
        char *arena_buffer, uint32_t const buffer_size,
        Arq_Option const *options, uint32_t const num_of_options
);

uint32_t arq_fn(
        int argc, char **argv,
        char *arena_buffer, uint32_t const buffer_size,
        Arq_Option const *options, uint32_t const num_of_options
);

void arq_unused(Arq_Queue *queue);
uint32_t arq_uint(Arq_Queue *queue);
uint32_t arq_array_size(Arq_Queue *queue);
int32_t arq_int(Arq_Queue *queue);
double arq_float(Arq_Queue *queue);
char const *arq_cstr_t(Arq_Queue *queue);

#ifdef __cplusplus
}
#endif
#endif

/*** End of inlined file: arq_main.h ***/

#ifdef ARQ_IMPLEMENTATION

/*** Start of inlined file: arq_token.h ***/
#ifndef ARQ_TOKEN_H
#define ARQ_TOKEN_H

typedef struct {
        uint32_t id;
        uint32_t size;
        char const *at;
} Arq_Token;

#define ARQ_LIST_OF_IDS                          \
                                                 \
        /* helper to control tokenizer */        \
        X(ARQ_NO_TOKEN)                          \
                                                 \
        /* literals */                           \
        X(ARQ_P_DEC)                             \
        X(ARQ_N_DEC)                             \
        X(ARQ_DEC_FLOAT)                         \
        X(ARQ_HEX)                               \
        X(ARQ_HEX_FLOAT)                         \
        X(ARQ_CMD_RAW_STR)                       \
                                                 \
        /* option operators */                   \
        X(ARQ_OP_EQ)                             \
        X(ARQ_OP_COMMA)                          \
        X(ARQ_OP_ARRAY)                          \
        X(ARQ_OP_L_PARENTHESIS)                  \
        X(ARQ_OP_R_PARENTHESIS)                  \
        X(ARQ_OP_TERMINATOR)                     \
        X(ARQ_OP_UNKNOWN)                        \
                                                 \
        X(ARQ_IDENTFIER)                         \
        /* option keywords */                    \
        X(ARQ_NULL)                              \
        /* option types */                       \
        X(ARQ_TYPE_CSTR)                         \
        X(ARQ_TYPE_UINT)                         \
        X(ARQ_TYPE_ARRAY_SIZE)                   \
        X(ARQ_TYPE_INT)                          \
        X(ARQ_TYPE_FLOAT)                        \
                                                 \
        /* command line tokens */                \
        X(ARQ_CMD_SHORT_OPTION)                  \
        X(ARQ_CMD_LONG_OPTION)                   \
        X(ARQ_CMD_DASHDASH)                      \

#define X(name)name,
typedef enum {
        ARQ_LIST_OF_IDS
        end
} Arq_SymbolID;
#undef X

extern char const *symbol_names[];

#endif

/*** End of inlined file: arq_token.h ***/



/*** Start of inlined file: arq_symbols.c ***/
#define X(name) #name,
char const *symbol_names[] = {
        ARQ_LIST_OF_IDS
};
#undef X

/*** End of inlined file: arq_symbols.c ***/


/*** Start of inlined file: arq_msg.h ***/
#ifndef ARQ_MSG_H
#define ARQ_MSG_H

typedef struct {
        uint32_t SIZE; /* sizeof(error_buffer), */
        uint32_t size;
        char *at;
} Arq_msg;

#ifdef __cplusplus
extern "C" {
#endif

void arq_msg_clear(Arq_msg *m);
void arq_msg_format(Arq_msg *m);

void arq_msg_append_lf(Arq_msg *m);
void arq_msg_append_chr(Arq_msg *m, char const chr);
void arq_msg_append_nchr(Arq_msg *m, char const chr, uint32_t const num_of_chr);
void arq_msg_append_cstr(Arq_msg *m, char const *cstr);
void arq_msg_append_str(Arq_msg *m, char const *str, uint32_t const size);

void arq_msg_set_cstr(Arq_msg *m, char const *cstr);

void arq_msg_insert_line_str(Arq_msg *m, uint32_t line_number, char const *str, uint32_t const size);
void arq_msg_insert_line_cstr(Arq_msg *m, uint32_t line_number, char const *cstr);

#ifdef __cplusplus
}
#endif
#endif

/*** End of inlined file: arq_msg.h ***/


/*** Start of inlined file: arq_msg.c ***/
#include <assert.h>
#include <string.h>

void arq_msg_clear(Arq_msg *m) {
        m->size = 0;
}

void arq_msg_format(Arq_msg *m) {
        uint32_t i;
        uint32_t number_of_lf = 0;
        for (i = 0; i < m->size; i++) {
                uint32_t const last = m->size - 1;
                if (m->at[i] == '\n' && i < last) {
                        number_of_lf++;
                }
        }
        {
                uint32_t j;
                uint32_t const INDENT_SIZE = 4;
                uint32_t const shift_right = number_of_lf * INDENT_SIZE;
                assert(m->size + shift_right < m->SIZE);
                for (i = 0; i < m->size; i++) {
                        m->at[m->size - 1 - i + shift_right] = m->at[m->size - 1 - i];
                }
                m->size += shift_right;
                m->at[m->size] = '\0';
                j = 0;
                for (i = shift_right; i < m->size; i++) {
                        m->at[j++] = m->at[i];
                        if (m->at[i] == '\n' && i + INDENT_SIZE < m->size) {
                                memset(&m->at[j], ' ', INDENT_SIZE);
                                j += INDENT_SIZE;
                        }
                }
        }
}

void arq_msg_append_chr(Arq_msg *m, char const chr) {
        assert(m->size + 1 < m->SIZE);
        m->at[m->size++] = chr;
        m->at[m->size] = 0; /* thats wy m->size has to be smaller than m->SIZE */
}

void arq_msg_append_nchr(Arq_msg *m, char const chr, uint32_t const num_of_chr) {
        uint32_t i;
        for (i = 0; i < num_of_chr; i++) {
                arq_msg_append_chr(m, chr);
        }
}

void arq_msg_append_lf(Arq_msg *m) {
        arq_msg_append_chr(m, '\n');
}

void arq_msg_append_cstr(Arq_msg *m, char const *cstr) {
        uint32_t const STRLEN = strlen(cstr);
        uint32_t i;
        for (i = 0; i < STRLEN; i++) {
                arq_msg_append_chr(m, cstr[i]);
        }
}

void arq_msg_append_str(Arq_msg *m, char const *str, uint32_t const size) {
        uint32_t i;
        for (i = 0; i < size; i++) {
                arq_msg_append_chr(m, str[i]);
        }
}

void arq_msg_insert_line_str(Arq_msg *m, uint32_t line_number, char const *str, uint32_t const size) {
        uint32_t A, B, C;
        uint32_t line_counter = 0;
        assert(m->size + size <= m->SIZE);
        for (A = 0; A < m->size; A++) {
                /* find start idx A */
                if (m->at[A] == '\n') line_counter++;
                if (line_counter == line_number) break;
        }
        for (B = m->size - 1; B > A; B--) {
                /* shift right, create space for insertion of str */
                m->at[B + size] = m->at[B];
        }
        m->size = m->size + size;
        A++;
        for(C = 0; C < size; C++) {
                /* copy str into arq->msg */
                m->at[A++] = str[C];
        }
}

void arq_msg_set_cstr(Arq_msg *m, char const *cstr) {
        arq_msg_clear(m);
        arq_msg_append_cstr(m, cstr);
}

void arq_msg_insert_line_cstr(Arq_msg *m, uint32_t line_number, char const *cstr) {
        arq_msg_insert_line_str(m, line_number, cstr, strlen(cstr));
}

/*** End of inlined file: arq_msg.c ***/


/*** Start of inlined file: arq_bool.h ***/
#ifndef ARQ_BOOL_H
#define ARQ_BOOL_H

#if defined(__cplusplus) || defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    /* C++, >= C99 */
    #include <stdbool.h>
#else
    /* C89 */
    typedef int bool;
    #define true 1
    #define false 0
#endif

#endif

/*** End of inlined file: arq_bool.h ***/


/*** Start of inlined file: arq_conversion.h ***/
#ifndef ARQ_TOK_H
#define ARQ_TOK_H

typedef struct {
        bool error;
        uint32_t u;
} uint_o;

typedef struct {
        bool error;
        int32_t i;
} int_o;

typedef struct {
        bool error;
        double f;
} float_o;

typedef union {
        uint_o ou;
        int_o oi;
        float_o of;
} union_o;

#ifdef __cplusplus
extern "C" {
#endif

bool token_long_option_eq(Arq_Token const *token, char const *cstr);

uint_o arq_tok_pDec_to_uint(Arq_Token const *token, Arq_msg *error_msg, char const *cstr);
int_o arq_tok_sDec_to_int(Arq_Token const *token, Arq_msg *error_msg, char const *cstr);
uint_o arq_tok_hex_to_uint(Arq_Token const *token, Arq_msg *error_msg, char const *cstr);

float_o arq_tok_decFloat_to_float(Arq_Token const *token);
float_o arq_tok_hexFloat_to_float(Arq_Token const *token);

#ifdef __cplusplus
}
#endif
#endif

/*** End of inlined file: arq_conversion.h ***/


/*** Start of inlined file: arq_inttypes.h ***/
#ifndef ARQ_INTTYPES_H
#define ARQ_INTTYPES_H

#if defined(__cplusplus) || defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
    /* C++, >= C99 */
    #include <inttypes.h>
#else
    /* C89 */
    #define PRId8  "d"
    #define PRId16 "d"
    #define PRId32 "d"
    #define PRIu8  "u"
    #define PRIu16 "u"
    #define PRIu32 "u"
    #define PRId64 "ld"
    #define PRIu64 "lu"
#endif

#endif /* ARQ_INTTYPES_H */

/*** End of inlined file: arq_inttypes.h ***/


/*** Start of inlined file: arq_conversion.c ***/
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

bool token_long_option_eq(Arq_Token const *token, char const *cstr) {
        uint32_t i;
        if (strlen(cstr) != token->size - 2) {
                return false;

        }
        for (i = 2; i < token->size; i++) {
                if (cstr[i - 2] != token->at[i]) {
                        return false;
                }
        }
        return true;
}

uint_o arq_tok_pDec_to_uint(Arq_Token const *token, Arq_msg *error_msg, char const *cstr) {
        uint_o result = {0};
        uint32_t i;
        assert(token->id == ARQ_P_DEC);
        if (token->at[0] == '+') {
                i = 1;
        } else {
                i = 0;
        }
        for (; i < token->size; i++) {
                uint32_t digit = token->at[i] - '0';
                if (result.u > (UINT32_MAX - digit) / 10) {
                        if (error_msg != NULL) {
                                Arq_Token tok = *token;
                                char buffer[12];
                                sprintf(buffer, "%" PRIu32, UINT32_MAX);
                                arq_msg_clear(error_msg);
                                arq_msg_append_cstr(error_msg, cstr);
                                /*arq_msg_append_cstr(error_msg, "Token '");*/
                                arq_msg_append_str(error_msg, tok.at, tok.size);
                                arq_msg_append_cstr(error_msg, "' positive number > UINT32_MAX ");
                                arq_msg_append_cstr(error_msg, buffer);
                                arq_msg_append_lf(error_msg);
                        }
                        result.error = true;
                        return result;
                }
                result.u = result.u * 10 + digit;
        }
        return result;
}

int_o arq_tok_sDec_to_int(Arq_Token const *token, Arq_msg *error_msg, char const *cstr) {
        int_o result = {0};
        int32_t SIGN;
        uint32_t i;
        assert(token->id == ARQ_P_DEC || token->id == ARQ_N_DEC);

        if (token->at[0] == '-') {
                SIGN = -1;
                i = 1;
        } else if (token->at[0] == '+') {
                SIGN = 1;
                i = 1;
        } else {
                SIGN = 1;
                i = 0;
        }
        assert(SIGN != 0);

        for (; i < token->size; i++) {
                char const ch = token->at[i];
                int32_t const digit = ch - '0';

                if (SIGN > 0) {
                        if (result.i > (INT32_MAX - digit) / 10) {
                                result.error = true;
                                if (error_msg != NULL) {
                                        Arq_Token tok = *token;
                                        char buffer[12];
                                        sprintf(buffer, "%" PRId32, INT32_MAX);
                                        arq_msg_clear(error_msg);
                                        arq_msg_append_cstr(error_msg, cstr);
                                        arq_msg_append_str(error_msg, tok.at, tok.size);
                                        arq_msg_append_cstr(error_msg, "' positive number > INT32_MAX ");
                                        arq_msg_append_cstr(error_msg, buffer);
                                        arq_msg_append_lf(error_msg);
                                }
                                return result;
                        }
                        result.i = result.i * 10 + digit;
                } else {
                        if (result.i < (INT32_MIN + digit) / 10) {
                                result.error = true;
                                if (error_msg != NULL) {
                                        Arq_Token tok = *token;
                                        char buffer[12];
                                        sprintf(buffer, "%" PRId32, INT32_MIN);
                                        arq_msg_clear(error_msg);
                                        arq_msg_append_cstr(error_msg, cstr);
                                        arq_msg_append_str(error_msg, tok.at, tok.size);
                                        arq_msg_append_cstr(error_msg, "' negative number < INT32_MIN ");
                                        arq_msg_append_cstr(error_msg, buffer);
                                        arq_msg_append_lf(error_msg);
                                }
                                return result;
                        }
                        result.i = result.i * 10 - digit;
                }
        }
        return result;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}
static int decval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    return -1;
}

uint_o arq_tok_hex_to_uint(Arq_Token const *token, Arq_msg *error_msg, char const *cstr) {
        uint_o result = {0};
        uint32_t i;
        assert(token->id == ARQ_HEX);
        for (i = 2; i < token->size; i++) {
                char const ch = token->at[i];
                int const digit = hexval(ch);
                assert(digit >= 0);
                if (result.u > (UINT32_MAX - digit) / 10) {
                        result.error = true;
                        if (error_msg != NULL) {
                                Arq_Token tok = *token;
                                arq_msg_clear(error_msg);
                                arq_msg_append_cstr(error_msg, cstr);
                                arq_msg_append_str(error_msg, tok.at, tok.size);
                                arq_msg_append_cstr(error_msg, "' more than 8 hex digits");
                                arq_msg_append_lf(error_msg);
                        }
                        return result;
                }
                result.u = result.u * 16 + digit;
        }
        return result;
}

static double arq_pow(double base, int exp) {
        double result = 1.0;
        int negative = 0;

        if (exp < 0) {
                negative = 1;
                exp = -exp;
        }

        while (exp > 0) {
                if (exp % 2 == 1) {
                        result *= base;
                }
                base *= base;
                exp /= 2;
        }

        if (negative) {
                return 1.0 / result;
        }
        return result;
}

float_o arq_tok_decFloat_to_float(Arq_Token const *token) {
        float_o result;
        double value = 0.0;
        int mantissa_sign = 1;
        int exp10 = 0;
        int exp_sign = 1;

        uint32_t i = 0;

        assert(token->id == ARQ_DEC_FLOAT);

        if (i < token->size) {
                switch (token->at[i]) {
                case '+':
                        mantissa_sign = 1;
                        i++;
                        break;
                case '-':
                        mantissa_sign = -1;
                        i++;
                        break;
                }
        }

        /* integer part */
        while (i < token->size) {
                int v = decval(token->at[i]);
                if (v < 0) {
                        break;
                }
                value = value * 10.0 + (double)v;
                i++;
        }

        /* fractional part */
        if (i < token->size && '.' == token->at[i]) {
                double place = 1.0 / 10.0;
                i++;
                while (i < token->size) {
                        int v = decval(token->at[i]);
                        if (v < 0) {
                                break;
                        }
                        value += (double)v * place;
                        place /= 10.0;
                        i++;
                }
        }
        value = mantissa_sign * value;

        /* exponent part (binary exponent) */
        if (i < token->size + 1 && ('e' == token->at[i] || 'E' == token->at[i])) {
                i++;
                switch (token->at[i]) {
                case '+':
                        exp_sign = 1;
                        i++;
                        break;
                case '-':
                        exp_sign = -1;
                        i++;
                        break;
                }
                while (i < token->size) {
                        if (exp10 < 2000) {
                                exp10 = exp10 * 10 + (token->at[i] - '0');
                        }
                        i++;
                }
                if (exp_sign > 0 && exp10 > 1200) {
                        result.f = HUGE_VAL; /* INFINITY */
                        result.error = false;
                        return result;
                }
                if (exp_sign < 0 && exp10 > 1200) {
                        result.f = 0.0;
                        result.error = false;
                        return result;
                }
        }

        /* scale by value * 10^exp10 */
        {
                result.f = value * arq_pow(10.0, (double)(exp_sign * exp10));
                result.error = false;
                return result;
        }
}

float_o arq_tok_hexFloat_to_float(Arq_Token const *token) {
        float_o result;
        double value = 0.0;
        int exp10 = 0;
        int exp_sign = 1;
        int frac_sign;

        uint32_t i = 0;

        if (token->at[i] == '-') {
                frac_sign = -1;
                i++;
        } else if (token->at[i] == '+') {
                frac_sign = 1;
                i++;
        } else {
                frac_sign = 1;
        }

        assert(token->id == ARQ_HEX_FLOAT);
        assert(token->at[i] == '0' && (token->at[i + 1] == 'x' || token->at[i + 1] == 'X'));
        i = i + 2;

        /* integer part */
        while (i < token->size) {
               int v = hexval(token->at[i]);
                if (v < 0) {
                        break;
                }
                value = value * 16.0 + (double)v;
                i++;
        }

        /* fractional part */
        if (i < token->size && '.' == token->at[i]) {
                double place = 1.0 / 16.0;
                i++;
                while (i < token->size) {
                        int v = hexval(token->at[i]);
                        if (v < 0) {
                                break;
                        }
                        value += (double)v * place;
                        place /= 16.0;
                        i++;
                }
        }

        /* exponent part (binary exponent) */
        if (i < token->size + 1 && ('p' == token->at[i] || 'P' == token->at[i])) {
                i++;
                switch (token->at[i]) {
                case '+':
                        exp_sign = 1;
                        i++;
                        break;
                case '-':
                        exp_sign = -1;
                        i++;
                        break;
                }
                while (i < token->size) {
                        if (exp10 < 2000) {
                                exp10 = exp10 * 10 + (token->at[i] - '0');
                        }
                        i++;
                }
                if (exp_sign > 0 && exp10 > 1200) {
                        result.f = HUGE_VAL; /* INFINITY */
                        result.error = false;
                        return result;
                }
                if (exp_sign < 0 && exp10 > 1200) {
                        result.f = 0.0;
                        result.error = false;
                        return result;
                }
        }

        /* scale by 2^final_exp */
        {
                int final_exp = exp_sign * exp10;
                result.f = frac_sign * ldexp(value, final_exp);
                result.error = false;
                return result;
        }
}

/*** End of inlined file: arq_conversion.c ***/


/*** Start of inlined file: arq_lexer.h ***/
#ifndef ARQ_LEXER_H
#define ARQ_LEXER_H

typedef struct {
        uint32_t cursor_idx;
        uint32_t SIZE;
        char const *at;
        Arq_Token token;
} Arq_Lexer;

typedef struct {
        Arq_Lexer lexer;
        uint32_t idx;
} Arq_LexerOpt;

typedef struct {
        Arq_Lexer lexer;
        uint32_t state;
        int argc;
        char **argv;
        int argIdx;
} Arq_LexerCmd;

#ifdef __cplusplus
extern "C" {
#endif

Arq_Lexer arq_lexer_create(void);
Arq_LexerOpt arq_lexerOpt_create(void);
Arq_LexerCmd arq_lexerCmd_create(int argc, char **argv);

void arq_lexer_next_opt_token(Arq_LexerOpt *l);

void arq_lexerCmd_reset(Arq_LexerCmd *cmd);
void arq_lexer_next_cmd_token(Arq_LexerCmd *l);

#ifdef __cplusplus
}
#endif
#endif

/*** End of inlined file: arq_lexer.h ***/


/*** Start of inlined file: arq_lexer.c ***/
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>

typedef struct {
    uint32_t id;
    char const *at;
} KeyWord;

static KeyWord const key_words[] = {
        {  ARQ_NULL,          "NULL" },
        {  ARQ_TYPE_CSTR,     "cstr_t" },
        {  ARQ_TYPE_UINT,     "uint" },
        {  ARQ_TYPE_INT,      "int" },
        {  ARQ_TYPE_FLOAT,    "float" },
};

static bool str_eq_keyword(char const *str, uint32_t const str_size, KeyWord const *cstr) {
        uint32_t i;
        if (str_size != strlen(cstr->at)) {
                return false;
        }
        for (i = 0; i < str_size; i++) {
                if (str[i] != cstr->at[i]) {
                        return false;
                }
        }
        return true;
}

static bool is_identifier(char const chr) {
        return isalnum(chr) || chr == '_';
}

static bool identifier_start(Arq_Lexer *l) {
        uint32_t const idx = l->cursor_idx;
        if (isalpha(l->at[idx]) || l->at[idx] == '_') {
                l->cursor_idx += 1;
                return true;
        }
        return false;
}

static bool array_start(Arq_Lexer *l) {
        uint32_t const idx = l->cursor_idx;
        if ((idx + 1 < l->SIZE)
        && (l->at[idx] == '[')
        && (l->at[idx + 1] == ']')) {
                l->cursor_idx += 2;
                return true;
        }
        return false;
}

static bool hex_start(Arq_Lexer *l) {
        uint32_t idx = l->cursor_idx;
        if (l->at[idx] == '+' || l->at[idx] == '-') {
                if (idx + 1 == l->SIZE) {
                        return false;
                }
                idx++;
        }
        if (l->at[idx] != '0') {
                return false;
        }
        if (idx + 1 == l->SIZE) {
                return false;
        }
        idx++;
        if (l->at[idx] != 'x' && l->at[idx + 1] != 'X') {
                return false;
        }
        if (idx + 1 == l->SIZE) {
                return false;
        }
        idx++;
        if (!isxdigit(l->at[idx])) {
                return false;
        }
        l->cursor_idx = idx + 1;
        return true;
}

static bool has_hex_exponent(char const s) {
    return (s == 'p') || (s == 'P');
}

static bool p_dec_start(Arq_Lexer *l) {
        uint32_t const idx = l->cursor_idx;
        if (isdigit(l->at[idx])) {
                l->cursor_idx += 1;
                return true;
        } else if (idx + 1 < l->SIZE
        && l->at[idx] == '+'
        && isdigit(l->at[idx + 1])) {
                l->cursor_idx += 2;
                return true;
        }
        return false;
}

static bool n_dec_start(Arq_Lexer *l) {
        uint32_t const idx = l->cursor_idx;
        if (idx + 1 < l->SIZE
        && l->at[idx] == '-'
        && isdigit(l->at[idx + 1])) {
                l->cursor_idx += 2;
                return true;
        }
        return false;
}

static bool has_dec_exponent(Arq_Lexer *l) {
        if (l->cursor_idx + 1 < l->SIZE) {
                uint32_t const idx = l->cursor_idx;
                char const chr = l->at[l->cursor_idx];
                bool isExp = (chr == 'e') || (chr == 'E');
                l->cursor_idx++;
                isExp &= p_dec_start(l) || n_dec_start(l);
                if (isExp) {
                        return true;
                }
                l->cursor_idx = idx;
        }
        return false;
}

static void dec_float(Arq_Lexer *l, Arq_Token *t) {
        if (l->cursor_idx < l->SIZE && ('.' == l->at[l->cursor_idx])) {
                /* fractional part */
                t->id = ARQ_DEC_FLOAT;
                l->cursor_idx++;
                t->size++;
                while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t->size++;
                }
        }
        if (has_dec_exponent(l)) {
                t->id = ARQ_DEC_FLOAT;
                t->size = &l->at[l->cursor_idx] - t->at;
                while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t->size++;
                }
                return;
        }
        return;
}

static void skip_space(Arq_Lexer *l) {
    while (l->cursor_idx < l->SIZE && (l->at[l->cursor_idx] == 0 || isspace(l->at[l->cursor_idx]))) {
            l->cursor_idx++;
    }
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
/* cmd_ */
static bool is_long_identifier(char chr) {
        return isalnum(chr) || chr == '-' || chr == '_';
}

static bool is_short_identifier(char chr) {
        return isalpha(chr) || chr == '?';
}

static bool start_short_identifier(Arq_Lexer *l) {
        if (l->at[l->cursor_idx] == '-'
        && is_short_identifier(l->at[l->cursor_idx + 1])) {
                l->cursor_idx += 2;
                return true;
        }
        return false;
}

static bool start_long_identifier(Arq_Lexer *l) {
        if (l->at[l->cursor_idx] == '-'
        && l->at[l->cursor_idx + 1] == '-'
        && is_long_identifier(l->at[l->cursor_idx + 2])) {
                l->cursor_idx += 3;
                return true;
        }
        return false;
}

static bool start_dash_dash(Arq_Lexer *l) {
        if (l->at[l->cursor_idx] == '-'
        && l->at[l->cursor_idx + 1] == '-'
        && l->SIZE == 2) {
                l->cursor_idx += 2;
                return true;
        }
        return false;
}

static Arq_Token next_token(Arq_Lexer *l, bool has_identifier) {
        Arq_Token t = {0};
        skip_space(l);
        t.at = &l->at[l->cursor_idx];
        t.size = 0;
        if (l->cursor_idx == l->SIZE ) {
                /* space tail */
                t.id = ARQ_NO_TOKEN;
                t.size = 0;
                return t;
        }

        if (l->at[l->cursor_idx] == '=') {
                t.id = ARQ_OP_EQ;
                l->cursor_idx++;
                t.size = 1;
                return t;
        }

        if (l->at[l->cursor_idx] == ',') {
                t.id = ARQ_OP_COMMA;
                l->cursor_idx++;
                t.size = 1;
                return t;
        }

        if (l->at[l->cursor_idx] == '(') {
                t.id = ARQ_OP_L_PARENTHESIS;
                l->cursor_idx++;
                t.size = 1;
                return t;
        }

        if (l->at[l->cursor_idx] == ')') {
                t.id = ARQ_OP_R_PARENTHESIS;
                l->cursor_idx++;
                t.size = 1;
                return t;
        }

        if (l->at[l->cursor_idx] == 0) {
                t.id = ARQ_OP_TERMINATOR;
                l->cursor_idx++;
                t.size = 1;
                return t;
        }

        if (has_identifier) {
                if (identifier_start(l)) {
                        uint32_t i;
                        t.id = ARQ_IDENTFIER;
                        t.size = &l->at[l->cursor_idx] - t.at;
                        while (l->cursor_idx < l->SIZE && is_identifier(l->at[l->cursor_idx])) {
                                l->cursor_idx++;
                                t.size++;
                        }
                        for (i = 0; i < sizeof(key_words)/sizeof(KeyWord); i++) {
                                if (str_eq_keyword(t.at,t.size, &key_words[i])) {
                                        t.id = key_words[i].id;
                                }
                        }
                        return t;
                }
        }
        if (array_start(l)) {
                t.id = ARQ_OP_ARRAY;
                t.size = &l->at[l->cursor_idx] - t.at;
                return t;
        }

        if (hex_start(l)) {
                t.id = ARQ_HEX;
                t.size = &l->at[l->cursor_idx] - t.at;
                while (l->cursor_idx < l->SIZE && isxdigit(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t.size++;
                }
                if (l->cursor_idx < l->SIZE && ('.' == l->at[l->cursor_idx])) {
                        t.id = ARQ_NO_TOKEN;
                        l->cursor_idx++;
                        t.size++;
                        while (l->cursor_idx < l->SIZE && isxdigit(l->at[l->cursor_idx])) {
                                l->cursor_idx++;
                                t.size++;
                        }
                        if (l->cursor_idx < l->SIZE && has_hex_exponent(l->at[l->cursor_idx])) {
                                l->cursor_idx++;
                                t.size++;
                                if (p_dec_start(l) || n_dec_start(l)) {
                                        t.id = ARQ_HEX_FLOAT;
                                        t.size = &l->at[l->cursor_idx] - t.at;
                                        while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                                                l->cursor_idx++;
                                                t.size++;
                                        }
                                        return t;
                                }
                        }
                } else if (l->cursor_idx < l->SIZE && has_hex_exponent(l->at[l->cursor_idx])) {
                        t.id = ARQ_NO_TOKEN;
                        l->cursor_idx++;
                        t.size++;
                        if (p_dec_start(l) || n_dec_start(l)) {
                                t.id = ARQ_HEX_FLOAT;
                                t.size = &l->at[l->cursor_idx] - t.at;
                                while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                                        l->cursor_idx++;
                                        t.size++;
                                }
                                return t;
                        }
                } else {
                        if (t.at[0] == '0') {
                                return t;
                        }
                        t.size = 0;
                        t.id = ARQ_NO_TOKEN;
                }

        }

        if (l->at[l->cursor_idx] ==  '.') {
                dec_float(l, &t);
                switch (t.id) {
                case ARQ_DEC_FLOAT: return t;
                default: break;
                };
        }

        if (p_dec_start(l)) {
                t.id = ARQ_P_DEC;
                t.size = &l->at[l->cursor_idx] - t.at;
                while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t.size++;
                }
                dec_float(l, &t);
                switch (t.id) {
                case ARQ_DEC_FLOAT:
                case ARQ_P_DEC: return t;
                default: break;
                };
        }

        if (n_dec_start(l)) {
                t.id = ARQ_N_DEC;
                t.size = &l->at[l->cursor_idx] - t.at;
                while (l->cursor_idx < l->SIZE && isdigit(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t.size++;
                }
                dec_float(l, &t);
                switch (t.id) {
                case ARQ_DEC_FLOAT:
                case ARQ_N_DEC: return t;
                default: break;
                };
        }

        if (start_short_identifier(l)) {
                t.id = ARQ_CMD_SHORT_OPTION;
                t.size = &l->at[l->cursor_idx] - t.at;
                return t;
        }

        if (start_long_identifier(l)) {
                t.id = ARQ_CMD_LONG_OPTION;
                t.size = &l->at[l->cursor_idx] - t.at;
                while (l->cursor_idx < l->SIZE && is_long_identifier(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t.size++;
                }
                if (l->cursor_idx == l->SIZE) {
                        return t;
                }
                if (l->cursor_idx + 1 < l->SIZE && l->at[l->cursor_idx] == '=') {
                        return t;
                }
        }

        if (start_dash_dash(l)) {
                t.id = ARQ_CMD_DASHDASH;
                t.size = &l->at[l->cursor_idx] - t.at;
                return t;
        }

        if (l->cursor_idx < l->SIZE) {
                t.id = ARQ_OP_UNKNOWN;
                while (l->cursor_idx < l->SIZE && !isspace(l->at[l->cursor_idx])) {
                        l->cursor_idx++;
                        t.size++;
                }
        }
        return t;
}

void arq_lexer_next_opt_token(Arq_LexerOpt *opt) {
        bool has_identifier = true;
        opt->lexer.token = next_token(&opt->lexer, has_identifier);
}

Arq_Lexer arq_lexer_create(void) {
        Arq_Lexer lexer;
        lexer.cursor_idx = 0;
        lexer.SIZE = 0;
        lexer.at = NULL;
        lexer.token.at = NULL;
        lexer.token.id = 0;
        lexer.token.size = 0;
        return lexer;
}

Arq_LexerOpt arq_lexerOpt_create(void) {
        Arq_LexerOpt opt;
        opt.lexer = arq_lexer_create();
        opt.idx = 0;
        return opt;
}

/******************************************************************************/
/******************************************************************************/
/******************************************************************************/

static Arq_Token next_cmd_token(Arq_Lexer *lexer) {
        bool has_identifier = false;
        Arq_Token token = next_token(lexer, has_identifier);
#if 0
        if (token.id == ARQ_CMD_SHORT_OPTION) {
                return token;
        }
        if (lexer->cursor_idx < lexer->SIZE) {
                token.id = ARQ_CMD_RAW_STR;
                token.size = lexer->SIZE;
                lexer->cursor_idx = lexer->SIZE;
        }
#endif
        return token;
}

Arq_LexerCmd arq_lexerCmd_create(int argc, char **argv) {
        Arq_LexerCmd cmd;
        cmd.lexer = arq_lexer_create();
        cmd.argc = argc - 1;
        cmd.argv = argv + 1;
        cmd.argIdx = 0;
        cmd.state = 0;
        return cmd;
}

void arq_lexerCmd_reset(Arq_LexerCmd *cmd) {
        cmd->lexer = arq_lexer_create();
        cmd->argIdx = 0;
        return;
}

#if 0
void arq_lexer_next_cmd_token(Arq_LexerCmd *cmd) {
        if (cmd->argIdx >= cmd->argc) {
                cmd->lexer.token.id = ARQ_NO_TOKEN;
                cmd->lexer.SIZE = 0;
                cmd->lexer.cursor_idx = 0;
                cmd->lexer.at = NULL;
                cmd->lexer.token.at = NULL;
                cmd->lexer.token.size = 0;
                cmd->bundeling = false;
                return;
        }

        if (!cmd->bundeling) {
                cmd->lexer.SIZE = strlen(cmd->argv[cmd->argIdx]);
                cmd->lexer.at = cmd->argv[cmd->argIdx];
                cmd->lexer.cursor_idx = 0;
                cmd->lexer.token = next_cmd_token(&cmd->lexer);
        } else if (is_short_identifier(cmd->lexer.at[cmd->lexer.cursor_idx])) {
                /* bundled options, Option clustering */
                cmd->lexer.token.at = &cmd->lexer.at[cmd->lexer.cursor_idx];
                cmd->lexer.token.id = ARQ_CMD_SHORT_OPTION;
                cmd->lexer.token.size = 1;
                cmd->lexer.cursor_idx++;
        } else {
                cmd->lexer.token = next_cmd_token(&cmd->lexer);
        }

        if (cmd->lexer.cursor_idx < cmd->lexer.SIZE) {
                cmd->bundeling = true;
                return;
        }
        if (cmd->argIdx < cmd->argc) {
                cmd->argIdx++;
        }
        cmd->bundeling = false;
        return;
}
#else
void arq_lexer_next_cmd_token(Arq_LexerCmd *cmd) {
        if (cmd->argIdx >= cmd->argc) {
                cmd->lexer.token.id = ARQ_NO_TOKEN;
                cmd->lexer.SIZE = 0;
                cmd->lexer.cursor_idx = 0;
                cmd->lexer.at = NULL;
                cmd->lexer.token.at = NULL;
                cmd->lexer.token.size = 0;
                cmd->state = 0; /* init=0, token=1, bundeling=2 */
                return;
        }

        switch (cmd->state) {
        case 0: /* Init */ {
                cmd->lexer.SIZE = strlen(cmd->argv[cmd->argIdx]);
                cmd->lexer.at = cmd->argv[cmd->argIdx];
                cmd->lexer.cursor_idx = 0;
                cmd->lexer.token = next_cmd_token(&cmd->lexer);

                if (cmd->lexer.cursor_idx == cmd->lexer.SIZE) {
                        cmd->argIdx++;
                        return;
                }

                if (cmd->lexer.cursor_idx < cmd->lexer.SIZE &&
                cmd->lexer.token.id == ARQ_CMD_SHORT_OPTION &&
                is_short_identifier(cmd->lexer.at[cmd->lexer.cursor_idx])) {
                        cmd->state = 2; /* bundeling */
                        return;
                }

                if (cmd->lexer.cursor_idx + 1 < cmd->lexer.SIZE &&
                cmd->lexer.at[cmd->lexer.cursor_idx] == '=') {
                        cmd->lexer.cursor_idx++;
                        cmd->state = 1; /* token */
                        return;
                }
                cmd->state = 1; /* token */
                return;
        }
        case 1: /* token */ {
                cmd->lexer.token = next_cmd_token(&cmd->lexer);
                if (cmd->lexer.cursor_idx == cmd->lexer.SIZE) {
                        cmd->state = 0; /* init */
                        cmd->argIdx++;
                        return;
                }

                if (cmd->lexer.cursor_idx + 1 < cmd->lexer.SIZE &&
                cmd->lexer.at[cmd->lexer.cursor_idx] == '=') {
                        cmd->lexer.cursor_idx++;
                        return;
                }
                } return;
        case 2: /* bundeling */ {
                cmd->lexer.token.at = &cmd->lexer.at[cmd->lexer.cursor_idx];
                cmd->lexer.token.id = ARQ_CMD_SHORT_OPTION;
                cmd->lexer.token.size = 1;
                cmd->lexer.cursor_idx++;

                if (cmd->lexer.cursor_idx == cmd->lexer.SIZE) {
                        cmd->state = 0; /* init */
                        cmd->argIdx++;
                        return;
                }

                if (cmd->lexer.cursor_idx < cmd->lexer.SIZE &&
                is_short_identifier(cmd->lexer.at[cmd->lexer.cursor_idx])) {
                        return;
                }

                if (cmd->lexer.cursor_idx + 1 < cmd->lexer.SIZE &&
                cmd->lexer.at[cmd->lexer.cursor_idx] == '=') {
                        cmd->lexer.cursor_idx++;
                        cmd->state = 1; /* token */
                        return;
                }

                cmd->state = 1; /* token */
                } return;
        default:
                assert(false);
                return;
        }
}
#endif

/*** End of inlined file: arq_lexer.c ***/


/*** Start of inlined file: arq_immediate.h ***/
#ifndef ARQ_IMMEDIATE_H
#define ARQ_IMMEDIATE_H

#define CMD_LINE_FAILURE "CMD line failure:\nToken '"
#define OPTION_FAILURE "Option failure:\nToken '"

typedef  bool (*arq_fn_imm_literal_error)(Arq_LexerOpt*,  Arq_msg*);

/*///////////////////////////////////////////////////////////////////////////*/

bool arq_imm(Arq_SymbolID const id, Arq_LexerOpt *opt);
bool arq_imm_noToken(Arq_Token *token);
bool arq_imm_not_identifier(Arq_LexerOpt *opt);

bool arq_imm_literal_uint_error(Arq_LexerOpt *opt,  Arq_msg *error_msg);
bool arq_imm_literal_int_error(Arq_LexerOpt *opt,  Arq_msg *error_msg);
bool arq_imm_literal_float_error(Arq_LexerOpt *opt,  Arq_msg *error_msg);
bool arq_imm_literal_NULL_error(Arq_LexerOpt *opt,  Arq_msg *error_msg);

typedef union_o (*arq_imm_default)(Arq_LexerOpt *opt);
union_o arq_imm_default_uint(Arq_LexerOpt *opt);
union_o arq_imm_default_int(Arq_LexerOpt *opt);
union_o arq_imm_default_float(Arq_LexerOpt *opt);
char const *arq_imm_default_cstr_t(Arq_LexerOpt *opt);

bool arq_imm_is_a_NULL(Arq_LexerOpt *opt);

/*///////////////////////////////////////////////////////////////////////////*/

bool arq_imm_cmd_is_dashdash(Arq_LexerCmd *cmd);

void arq_imm_cmd_next(Arq_LexerCmd *cmd);
bool arq_imm_cmd_has_token_left(Arq_LexerCmd *cmd);
bool arq_imm_end_of_line(Arq_LexerCmd *cmd);

Arq_LexerOpt arq_imm_get_long(
        Arq_Option const *options,
        uint32_t const num_of_options,
        Arq_LexerCmd *cmd,
        Arq_msg *error_msg
);
Arq_LexerOpt arq_imm_get_short(
        Arq_Option const *options,
        uint32_t const num_of_options,
        Arq_LexerCmd *cmd,
        Arq_msg *error_msg
);

void arq_imm_cmd_not_a_option(Arq_LexerCmd const *cmd, Arq_msg *error_msg);
bool arq_imm_cmd_is_long_option(Arq_LexerCmd *cmd);
bool arq_imm_cmd_is_short_option(Arq_LexerCmd *cmd);

typedef bool (*arq_imm_is)(Arq_LexerCmd *cmd);
bool arq_imm_is_uint(Arq_LexerCmd *cmd);
bool arq_imm_is_int(Arq_LexerCmd *cmd);
bool arq_imm_is_float(Arq_LexerCmd *cmd);

typedef bool (*arq_imm_optional_argument)(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg);
bool arq_imm_optional_argument_uint(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg);
bool arq_imm_optional_argument_int(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg);
bool arq_imm_optional_argument_float(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg);
bool arq_imm_optional_argument_cstr_t(Arq_LexerCmd *cmd, char const **cstr);
bool arq_imm_pick_cstr_t(Arq_LexerCmd *cmd, char const **cstr);

typedef union_o (*arq_imm_argument)(Arq_LexerCmd *cmd, Arq_msg *error_msg);
union_o arq_imm_argument_uint(Arq_LexerCmd *cmd, Arq_msg *error_msg);
union_o arq_imm_argument_int(Arq_LexerCmd *cmd, Arq_msg *error_msg);
union_o arq_imm_argument_float(Arq_LexerCmd *cmd, Arq_msg *error_msg);
char const *arq_imm_argument_csrt_t(Arq_LexerCmd *cmd, Arq_msg *error_msg);

#endif

/*** End of inlined file: arq_immediate.h ***/


/*** Start of inlined file: arq_immediate.c ***/
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

/*///////////////////////////////////////////////////////////////////////////*/

bool arq_imm(Arq_SymbolID const id, Arq_LexerOpt *opt) {
        const bool b = (opt->lexer.token.id == id);
        if (b) {
                arq_lexer_next_opt_token(opt);
        }
        return b;
}

bool arq_imm_noToken(Arq_Token *token) {
        const bool b = (token->id == ARQ_NO_TOKEN);
        return b;
}

bool arq_imm_not_identifier(Arq_LexerOpt *opt) {
        const bool b = (opt->lexer.token.id == ARQ_IDENTFIER);
        if (b) {
                arq_lexer_next_opt_token(opt);
        }
        return !b;
}

bool arq_imm_literal_uint_error(Arq_LexerOpt *opt,  Arq_msg *error_msg) {
        uint_o num;
        switch (opt->lexer.token.id) {
        case ARQ_P_DEC:
                num = arq_tok_pDec_to_uint(&opt->lexer.token, error_msg, OPTION_FAILURE);
                break;
        case ARQ_HEX:
                num = arq_tok_hex_to_uint(&opt->lexer.token, error_msg, OPTION_FAILURE);
                break;
        default:
                arq_msg_clear(error_msg);
                arq_msg_append_cstr(error_msg, OPTION_FAILURE);
                arq_msg_append_str(error_msg, opt->lexer.token.at, opt->lexer.token.size);
                arq_msg_append_cstr(error_msg, "' is not a uint literal\n");
                num.u = 0;
                num.error = true;
                break;
        }
        if (!num.error) {
                /* success */
                arq_lexer_next_opt_token(opt);
        }
        return num.error; /* return true if successful */
}

bool arq_imm_literal_int_error(Arq_LexerOpt *opt,  Arq_msg *error_msg) {
        int_o num;
        switch (opt->lexer.token.id) {
        case ARQ_P_DEC: case ARQ_N_DEC:
                num = arq_tok_sDec_to_int(&opt->lexer.token, error_msg, OPTION_FAILURE);
                break;
        case ARQ_HEX: {
                uint_o const n = arq_tok_hex_to_uint(&opt->lexer.token, NULL, "");
                num.i = (int32_t)n.u;
                num.error = n.error;
                } break;
        default:
                arq_msg_clear(error_msg);
                arq_msg_append_cstr(error_msg, OPTION_FAILURE);
                arq_msg_append_str(error_msg, opt->lexer.token.at, opt->lexer.token.size);
                arq_msg_append_cstr(error_msg, "' is not a int literal\n");
                num.i = 0;
                num.error = true;
                break;
        }
        if (!num.error) {
                /* success */
                arq_lexer_next_opt_token(opt);
        }
        return num.error; /* return true if successful */
}

bool arq_imm_literal_float_error(Arq_LexerOpt *opt,  Arq_msg *error_msg) {
        float_o num;
        switch (opt->lexer.token.id) {
        case ARQ_DEC_FLOAT:
                num = arq_tok_decFloat_to_float(&opt->lexer.token);
                break;
        case ARQ_HEX_FLOAT:
                num = arq_tok_hexFloat_to_float(&opt->lexer.token);
                break;
        default:
                arq_msg_clear(error_msg);
                arq_msg_append_cstr(error_msg, OPTION_FAILURE);
                arq_msg_append_str(error_msg, opt->lexer.token.at, opt->lexer.token.size);
                arq_msg_append_cstr(error_msg, "' is not a float literal\n");
                num.f = 0.0;
                num.error = true;
                break;
        }
        if (!num.error) {
                /* success */
                arq_lexer_next_opt_token(opt);
        }
        return num.error; /* return true if successful */
}

bool arq_imm_literal_NULL_error(Arq_LexerOpt *opt,  Arq_msg *error_msg) {
        bool const b = opt->lexer.token.id == ARQ_NULL;
        if (b) {
                /* success */
                arq_lexer_next_opt_token(opt);
                return false;
        } else {
                arq_msg_clear(error_msg);
                arq_msg_append_cstr(error_msg, OPTION_FAILURE);
                arq_msg_append_str(error_msg, opt->lexer.token.at, opt->lexer.token.size);
                arq_msg_append_cstr(error_msg, "' must be NULL\n");
                return true;
        }
}

union_o arq_imm_default_uint(Arq_LexerOpt *opt) {
        union_o num = {0};
        switch (opt->lexer.token.id) {
        case ARQ_P_DEC:
                num.ou = arq_tok_pDec_to_uint(&opt->lexer.token, NULL, "");
                break;
        case ARQ_HEX:
                num.ou = arq_tok_hex_to_uint(&opt->lexer.token, NULL, "");
                break;
        default:
                assert(false);
                break;
        }
        assert(num.ou.error == false);
        arq_lexer_next_opt_token(opt);
        return num;
}

union_o arq_imm_default_int(Arq_LexerOpt *opt) {
        union_o num = {0};
        switch (opt->lexer.token.id) {
        case ARQ_P_DEC: case ARQ_N_DEC:
                num.oi = arq_tok_sDec_to_int(&opt->lexer.token, NULL, "");
                break;
        case ARQ_HEX: {
                uint_o const x = arq_tok_hex_to_uint(&opt->lexer.token, NULL, "");
                num.oi.i = (int32_t)x.u;
                num.oi.error = x.error;
                } break;
        default:
                assert(false);
                break;
        }
        assert(num.oi.error == false);
        arq_lexer_next_opt_token(opt);
        return num;
}

union_o arq_imm_default_float(Arq_LexerOpt *opt) {
        union_o num = {0};
        switch (opt->lexer.token.id) {
        case ARQ_DEC_FLOAT:
                num.of = arq_tok_decFloat_to_float(&opt->lexer.token);
                break;
        case ARQ_HEX_FLOAT:
                num.of = arq_tok_hexFloat_to_float(&opt->lexer.token);
                break;
        default:
                assert(false);
                break;
        }
        assert(num.of.error == false);
        arq_lexer_next_opt_token(opt);
        return num;
}

char const *arq_imm_default_cstr_t(Arq_LexerOpt *opt) {
        arq_lexer_next_opt_token(opt);
        return NULL;
}

union_o arq_imm_default_value(Arq_LexerOpt *opt) {
        union_o a = {0};
        (void) opt;
        return a;
}

/*///////////////////////////////////////////////////////////////////////////*/

bool arq_imm_cmd_is_dashdash(Arq_LexerCmd *cmd) {
        Arq_Token const *token = &cmd->lexer.token;
        const bool b = (token->id == ARQ_CMD_DASHDASH);
        if (b) {
                arq_imm_cmd_next(cmd);
        }
        return b;
}

/* jumps over a bundel of short options */
/* -shello */
/* s is a short option */
/* if s take no argument then 'h' and maybe 'ello' are all short options */
/* if s takes an argument then 'hello' is the argument */
/*     'next_bundle_idx' over jumps 'hello' because in the vector they all bundled short options */
/*     'hello' should be a cstring in the vector but it isn't. */
/*     That's why we have to increment idx for every char (short option) in the bundle 'hello' */
static void next_bundle_idx(Arq_LexerCmd *cmd) {
        char const *const begin = cmd->lexer.token.at;
        char const *const end = begin + strlen(begin);
        char const *x = begin;
        assert(*end == 0);
        while ((cmd->lexer.token.id != ARQ_NO_TOKEN) && (x >= begin) && (x < end)) {
                arq_lexer_next_cmd_token(cmd);
                x = cmd->lexer.token.at;
        }
}

void arq_imm_cmd_next(Arq_LexerCmd *cmd) {
        if (cmd->lexer.token.id != ARQ_NO_TOKEN) {
                arq_lexer_next_cmd_token(cmd);
        }
}

bool arq_imm_cmd_has_token_left(Arq_LexerCmd *cmd) {
        return cmd->lexer.token.id != ARQ_NO_TOKEN;
}

bool arq_imm_cmd_is_long_option(Arq_LexerCmd *cmd) {
        return (cmd->lexer.token.id == ARQ_CMD_LONG_OPTION);
}

bool arq_imm_cmd_is_short_option(Arq_LexerCmd *cmd) {
        return (cmd->lexer.token.id == ARQ_CMD_SHORT_OPTION);
}

bool arq_imm_is_uint(Arq_LexerCmd *cmd) {
        return (cmd->lexer.token.id == ARQ_P_DEC) || (cmd->lexer.token.id == ARQ_HEX);
}

bool arq_imm_is_int(Arq_LexerCmd *cmd) {
        return (cmd->lexer.token.id == ARQ_P_DEC) || (cmd->lexer.token.id == ARQ_N_DEC) || (cmd->lexer.token.id == ARQ_HEX);
}

bool arq_imm_is_float(Arq_LexerCmd *cmd) {
        return (cmd->lexer.token.id == ARQ_DEC_FLOAT) || (cmd->lexer.token.id == ARQ_HEX_FLOAT);
}

Arq_LexerOpt arq_imm_get_long(
        Arq_Option const *options,
        uint32_t const num_of_options,
        Arq_LexerCmd *cmd,
        Arq_msg *error_msg
) {
        Arq_LexerOpt opt = arq_lexerOpt_create();
        Arq_Token const *token = &cmd->lexer.token;
        for (opt.idx = 0; opt.idx < num_of_options; opt.idx++) {
                if (token_long_option_eq(token, options[opt.idx].name)) {
                        opt.lexer.at = options[opt.idx].arguments;
                        opt.lexer.SIZE = strlen(options[opt.idx].arguments);
                        opt.lexer.cursor_idx = 0;
                        arq_imm_cmd_next(cmd);
                        return opt;
                }
        }
        arq_msg_append_cstr(error_msg, CMD_LINE_FAILURE);
        /* arq_msg_append_cstr(error_msg, "'--"); */
        arq_msg_append_str(error_msg, token->at, token->size);
        arq_msg_append_cstr(error_msg, "' unknown long option ");
        arq_msg_append_lf(error_msg);
        return opt;
}
Arq_LexerOpt arq_imm_get_short(
        Arq_Option const *options,
        uint32_t const num_of_options,
        Arq_LexerCmd *cmd,
        Arq_msg *error_msg
) {
        Arq_Token const *token = &cmd->lexer.token;
        uint32_t const IDX = (token->at[0] == '-') ? 1 : 0; /* : 0 because of bundled short options */
        Arq_LexerOpt opt = arq_lexerOpt_create();
        for (opt.idx = 0; opt.idx < num_of_options; opt.idx++) {
                if (token->at[IDX] == options[opt.idx].chr) {
                        opt.lexer.at = options[opt.idx].arguments;
                        opt.lexer.SIZE = strlen(options[opt.idx].arguments);
                        opt.lexer.cursor_idx = 0;
                        arq_imm_cmd_next(cmd);
                        return opt;
                }
        }
        arq_msg_append_cstr(error_msg, CMD_LINE_FAILURE);
        /* arq_msg_append_cstr(error_msg, "'-"); */
        arq_msg_append_str(error_msg, token->at, token->size);
        arq_msg_append_cstr(error_msg, "' unknown short option ");
        arq_msg_append_lf(error_msg);
        return opt;
}

void arq_imm_cmd_not_a_option(Arq_LexerCmd const *cmd, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        arq_msg_append_cstr(error_msg, CMD_LINE_FAILURE);
        arq_msg_append_str(error_msg, token->at, token->size);
        arq_msg_append_cstr(error_msg, "' is not an option");
        arq_msg_append_lf(error_msg);
}
bool arq_imm_end_of_line(Arq_LexerCmd *cmd) {
        return (cmd->lexer.token.id == ARQ_NO_TOKEN);
}

bool arq_imm_optional_argument_uint(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        switch (token->id) {
        case ARQ_P_DEC:
                num->ou = arq_tok_pDec_to_uint(token, error_msg, CMD_LINE_FAILURE);
                break;
        case ARQ_HEX:
                num->ou = arq_tok_hex_to_uint(token, error_msg, CMD_LINE_FAILURE);
                break;
        default:
                return false;
        }
        if (num->ou.error) {
                return true; /* overflow */
        }
        arq_imm_cmd_next(cmd);
        return false;
}

bool arq_imm_optional_argument_int(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        switch (token->id) {
        case ARQ_P_DEC:
        case ARQ_N_DEC:
                num->oi = arq_tok_sDec_to_int(token, error_msg, CMD_LINE_FAILURE);
                break;
        case ARQ_HEX: {
                uint_o n = arq_tok_hex_to_uint(token, error_msg, CMD_LINE_FAILURE);
                num->oi.i = (int32_t)n.u;
                num->oi.error = n.error;
                } break;
        default:
                return false;
        }
        if (num->oi.error) {
                return true; /* overflow */
        }
        arq_imm_cmd_next(cmd);
        return false;
}

bool arq_imm_optional_argument_float(Arq_LexerCmd *cmd, union_o *num, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        (void)error_msg;
        switch (token->id) {
        case ARQ_DEC_FLOAT:
                num->of = arq_tok_decFloat_to_float(token);
                break;
        case ARQ_HEX_FLOAT:
                num->of = arq_tok_hexFloat_to_float(token);
                break;
        default:
                return false;
        }
        if (num->of.error) {
                return true;
        }
        arq_imm_cmd_next(cmd);
        return false;
}

bool arq_imm_optional_argument_cstr_t(Arq_LexerCmd *cmd, char const **cstr) {
        Arq_Token const *token = &cmd->lexer.token;
        if (token->id != ARQ_CMD_LONG_OPTION
        && token->id != ARQ_CMD_SHORT_OPTION) {
                *cstr = token->at;
                if (*cstr != NULL) {
                        next_bundle_idx(cmd);
                        return true;
                }
        }
        return false;
}

bool arq_imm_pick_cstr_t(Arq_LexerCmd *cmd, char const **cstr) {
        Arq_Token const *token = &cmd->lexer.token;
        if (token->id != ARQ_NO_TOKEN) {
                *cstr = arq_imm_argument_csrt_t(cmd, NULL);
                return true;
        }
        *cstr = NULL;
        return false;
}

union_o arq_imm_argument_uint(Arq_LexerCmd *cmd, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        union_o result = {0};
        char const *cstr = CMD_LINE_FAILURE;
        switch (token->id) {
        case ARQ_HEX:
                result.ou = arq_tok_hex_to_uint(token, error_msg, cstr);
                break;
        case ARQ_P_DEC:
                result.ou = arq_tok_pDec_to_uint(token, error_msg, cstr);
                break;
        default:
                if (error_msg != NULL) {
                        Arq_Token const tok = *token;
                        arq_msg_append_cstr(error_msg, cstr);
                        arq_msg_append_str(error_msg, tok.at, tok.size);
                        arq_msg_append_cstr(error_msg, "' is not a positiv number");
                        arq_msg_append_lf(error_msg);
                }
                result.ou.error = true;
                return result;
        }
        arq_imm_cmd_next(cmd);
        return result;
}

union_o arq_imm_argument_int(Arq_LexerCmd *cmd, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        union_o result = {0};
        char const *cstr = CMD_LINE_FAILURE;
        switch (token->id) {
        case ARQ_HEX: {
                uint_o const r = arq_tok_hex_to_uint(token, error_msg, cstr);
                result.oi.i = (int32_t) r.u;
                result.oi.error = r.error;
                } break;
        case ARQ_P_DEC:
        case ARQ_N_DEC:
                result.oi = arq_tok_sDec_to_int(token, error_msg, cstr);
                break;
        default:
                if (error_msg != NULL) {
                        Arq_Token const tok = *token;
                        arq_msg_append_cstr(error_msg, cstr);
                        arq_msg_append_str(error_msg, tok.at, tok.size);
                        arq_msg_append_cstr(error_msg, "' is not a signed number");
                        arq_msg_append_lf(error_msg);
                }
                result.oi.error = true;
                return result;
        }
        arq_imm_cmd_next(cmd);
        return result;
}

union_o arq_imm_argument_float(Arq_LexerCmd *cmd, Arq_msg *error_msg) {
        Arq_Token const *token = &cmd->lexer.token;
        union_o result = {0};
        char const *cstr = CMD_LINE_FAILURE;
        switch (token->id) {
        case ARQ_HEX_FLOAT:
                result.of = arq_tok_hexFloat_to_float(token);
                break;
        case ARQ_DEC_FLOAT:
                result.of = arq_tok_decFloat_to_float(token);
                break;
        default:
                if (error_msg != NULL) {
                        Arq_Token const tok = *token;
                        arq_msg_append_cstr(error_msg, cstr);
                        arq_msg_append_str(error_msg, tok.at, tok.size);
                        arq_msg_append_cstr(error_msg, "' is not a float number");
                        arq_msg_append_lf(error_msg);
                }
                result.of.error = true;
                return result;
        }
        arq_imm_cmd_next(cmd);
        return result;
}

char const *arq_imm_argument_csrt_t(Arq_LexerCmd *cmd, Arq_msg *error_msg) {
        Arq_Token *token = &cmd->lexer.token;
        char const *result;
        if (token->id == ARQ_NO_TOKEN) {
                Arq_Token const tok = *token;
                arq_msg_append_cstr(error_msg, CMD_LINE_FAILURE);
                arq_msg_append_str(error_msg, tok.at, tok.size);
                arq_msg_append_cstr(error_msg, "' is not a c string => expected an argument");
                arq_msg_append_lf(error_msg);
                result = NULL;
                return result;
        }

        /* Even it looks like a short or long option but it is not it expects an argument */
        result = token->at;
        next_bundle_idx(cmd);
        return result;
}

/*** End of inlined file: arq_immediate.c ***/


/*** Start of inlined file: arq_arena.h ***/
#ifndef ARQ_ARENA_H
#define ARQ_ARENA_H

#define ARQ_ARENA_SIZE_OF_PADDING sizeof(size_t)

typedef struct {
        uint32_t SIZE;
        uint32_t size;
        char at[1];
} Arq_Arena;

#ifdef __cplusplus
extern "C" {
#endif

Arq_Arena *arq_arena_init(void *buffer, uint32_t const size);
void *arq_arena_malloc(Arq_Arena *m, uint32_t const num_of_bytes);
void *arq_arena_malloc_rest(Arq_Arena *m, uint32_t const size_of_header, uint32_t const size_of_element, uint32_t *num_of_elements);

#ifdef __cplusplus
}
#endif
#endif

/*** End of inlined file: arq_arena.h ***/


/*** Start of inlined file: arq_arena.c ***/
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

Arq_Arena *arq_arena_init(void *buffer, uint32_t const _size) {
        uint32_t const offset = (size_t)buffer % ARQ_ARENA_SIZE_OF_PADDING;
        uint32_t const padding = offset > 0 ? ARQ_ARENA_SIZE_OF_PADDING - offset : 0;
        uint32_t const size = _size - padding;
        uint32_t const header_size = offsetof(Arq_Arena, at);
        Arq_Arena *m = (Arq_Arena *)((char*)buffer + padding);
        assert(_size > padding);
        assert(size > header_size);
        assert((size_t)m % ARQ_ARENA_SIZE_OF_PADDING == 0 && "buffer does not align");
        m->SIZE = size - header_size;
        m->size = 0;
        m->at[0] = 0;
        return m;
}

void *arq_arena_malloc(Arq_Arena *m, uint32_t const num_of_bytes) {
        uint32_t const padded_size = ARQ_ARENA_SIZE_OF_PADDING * ((num_of_bytes + ARQ_ARENA_SIZE_OF_PADDING - 1) / ARQ_ARENA_SIZE_OF_PADDING);

        if (num_of_bytes == 0) return NULL;
        assert(m->size + num_of_bytes <= m->SIZE && "arq_arena_malloc need more memory");

        if (m->size + padded_size <= m->SIZE) {
                uint32_t const begin = m->size;
                void *buffer = &m->at[begin];
                m->size += padded_size;
                assert((size_t)buffer % ARQ_ARENA_SIZE_OF_PADDING == 0 && "buffer does not align");
                return buffer;
        } else {
                uint32_t const begin = m->size;
                void *buffer = &m->at[begin];
                m->size += num_of_bytes;
                assert((size_t)buffer % ARQ_ARENA_SIZE_OF_PADDING == 0 && "buffer does not align");
                return buffer;
        }
}

void *arq_arena_malloc_rest(Arq_Arena *m, uint32_t const size_of_header, uint32_t const size_of_element, uint32_t *num_of_elements) {
        uint32_t const size = (m->SIZE - m->size);
        assert(size_of_element > 0);
        assert(size >= size_of_element && "size >= size_of_element arq_arena need more memory");
        *num_of_elements = (size - size_of_header) / size_of_element;
        return arq_arena_malloc(m, size);
}

/*** End of inlined file: arq_arena.c ***/


/*** Start of inlined file: arq_log.h ***/
#ifndef ARQ_LOG_H
#define ARQ_LOG_H

#ifdef ARQ_LOG_MEMORY
        #include <stdio.h>
        #define log_memory(args) do {   \
                        printf args;    \
                        printf("\n");   \
                } while (0)
                /* do { printf(fmt "\n", ##__VA_ARGS__); } while (0) */
#else
        #define log_memory(args) do {} while (0)
#endif

#ifdef ARQ_LOG_TOKENS

        #include <string.h>
        #include <stdio.h>
        void log_print_token_member(Arq_Token *t, uint32_t toknr);
        void log_options_tokens(Arq_Option const *options, uint32_t const num_of_options);
        void log_cmd_tokens(int argc, char **argv);
#else
        #define log_print_token_member(token, nr) ((void)0)
        #define log_options_tokens(opt, num_of_options) ((void)0)
        #define log_cmd_tokens(argc, argv) ((void)0)
#endif

#ifdef ARQ_LOG_TOKENIZER
        /* used for interpreter logging */

        #include <stdio.h>
        #define log_int_banner(fmt) \
                do { printf("---------" fmt "------------\n"); } while (0)
        #define log_int_ln() \
                do { printf("\n"); } while (0)
        #define log_int_comment(fmt) \
                do { printf("    " fmt "\n"); } while (0)
        #define log_int_token(fmt) \
                do { printf("%s\n",symbol_names[fmt]); } while (0)
        #define log_int_token_indent(fmt) \
                do { printf("    %s\n",symbol_names[fmt]); } while (0)
        #define log_inta(args) do { \
                printf("    ");     \
                printf args ;       \
                printf("\n");       \
        } while (0)
        void log_int_uint(uint_o const *x);
        void log_int_int(int_o const *x);
        void log_int_float(float_o const *x);
#else
        #define log_int_banner(fmt) do {} while (0)
        #define log_int_ln() do {} while (0)
        #define log_int_comment(fmt) do {} while (0)
        #define log_int_token(fmt) do {} while (0)
        #define log_int_token_indent(fmt) do {} while (0)
        #define log_inta(args) do {} while (0)
        #define log_int_uint(x) ((void)0);
        #define log_int_int(x) ((void)0);
        #define log_int_float(x) ((void)0);
#endif

#endif

/*** End of inlined file: arq_log.h ***/


/*** Start of inlined file: arq_log.c ***/
#ifdef ARQ_LOG_TOKENS

void log_print_token_member(Arq_Token *t, uint32_t toknr) {
        uint32_t i;
        printf("%3d %30s -> ", toknr, symbol_names[t->id]);
        printf("%2d ", t->size);
        for (i = 0; i < t->size; i++) {
                putchar(t->at[i]);
        }
        printf("\n");
}

void log_options_tokens(Arq_Option const *options, uint32_t const num_of_options) {
        uint32_t n;
        for (n = 0; n < num_of_options; n++) {
                uint32_t i = 0;
                Arq_LexerOpt opt = arq_lexerOpt_create();
                opt.lexer.at = options[n].arguments;
                opt.lexer.SIZE = strlen(options[n].arguments);
                opt.lexer.cursor_idx = 0;
                printf("Option[%d] -%c --%s %s\n", n, options[n].chr, options[n].name, options[n].arguments);
                do {
                        arq_lexer_next_opt_token(&opt);
                        log_print_token_member(&opt.lexer.token, i++);
                } while (opt.lexer.token.id != ARQ_NO_TOKEN);
                printf("\n");
        }
}

void log_cmd_tokens(int argc, char **argv)  {
        Arq_LexerCmd cmd = arq_lexerCmd_create(argc, argv);
        uint32_t i = 0;
        printf("Command line tokens:\n");
        do {
                arq_lexer_next_cmd_token(&cmd);
                log_print_token_member(&cmd.lexer.token, i++);

        } while(cmd.lexer.token.id != ARQ_NO_TOKEN);
        printf("\n");
}
#endif

#ifdef ARQ_LOG_TOKENIZER
void log_int_uint(uint_o const *x) {
        printf("    ");
        printf("%d\n", x->u);
}
void log_int_int(int_o const *x) {
        printf("    ");
        printf("%d\n", x->i);
}
void log_int_float(float_o const *x) {
        printf("    ");
        printf("%f\n", x->f);
}
#endif

/*** End of inlined file: arq_log.c ***/


/*** Start of inlined file: arq_queue.h ***/
#ifndef ARQ_QUEUE_H
#define ARQ_QUEUE_H

typedef struct {
        Arq_SymbolID type_id;
        union {
                uint8_t u8;
                uint16_t u16;
                uint32_t u32;
                /* uint64_t u64; */
                int8_t i8;
                int16_t i16;
                int32_t i32;
                /* int64_t i64; */
                double f;
                char const *cstr;
        } value;
} Arq_Argument;

struct Arq_Queue_tag{
        uint32_t shrink;
        uint32_t NUM_OF_ARGUMENTS;
        uint32_t read_idx;
        uint32_t write_idx;
        Arq_Argument at[1];
};

#ifdef __cplusplus
extern "C" {
#endif

Arq_Queue *arq_queue_malloc(Arq_Arena *arena);
void arq_queue_clear(Arq_Queue *queue);

typedef void (*arq_push)(Arq_Queue *queue, union_o const *x);
void arq_push_uint(Arq_Queue *queue, union_o const *x);
void arq_push_int(Arq_Queue *queue, union_o const *x);
void arq_push_float(Arq_Queue *queue, union_o const *x);
uint32_t *arq_push_array_size(Arq_Queue *queue, uint32_t n);
void arq_push_cstr_t(Arq_Queue *queue, char const *cstr);

#ifdef __cplusplus
}
#endif
#endif

/*** End of inlined file: arq_queue.h ***/


/*** Start of inlined file: arq_queue.c ***/
#include <stddef.h>
#include <string.h>
#include <assert.h>

Arq_Queue *arq_queue_malloc(Arq_Arena *arena) {
        uint32_t NUM_OF_ARGUMENTS;
        uint32_t const shrink_snapshot = arena->size;
        Arq_Queue *queue = (Arq_Queue *)arq_arena_malloc_rest(arena, offsetof(Arq_Queue, at), sizeof(Arq_Argument), &NUM_OF_ARGUMENTS);
        queue->shrink = shrink_snapshot;
        queue->NUM_OF_ARGUMENTS = NUM_OF_ARGUMENTS;
        queue->read_idx = 0;
        queue->write_idx = 0;
        {
                Arq_Argument a;
                a.type_id = ARQ_TYPE_UINT;
                a.value.u32 = 0;
                queue->at[0] = a;
        }
        return queue;
}

void arq_queue_clear(Arq_Queue *queue) {
        queue->read_idx = 0;
        queue->write_idx = 0;
}

static Arq_Argument pop(Arq_Queue *queue) {
        assert(queue->read_idx < queue->write_idx && "queue is empty");
        assert(queue->read_idx < queue->NUM_OF_ARGUMENTS);
        {
                Arq_Argument argument = queue->at[queue->read_idx];
                queue->read_idx++;
                return argument;
        }
}

static void push(Arq_Queue *queue, Arq_Argument const *argument) {
        assert(queue->write_idx < queue->NUM_OF_ARGUMENTS);
        queue->at[queue->write_idx] = *argument;
        queue->write_idx++;
}

void arq_unused(Arq_Queue *queue) {
        (void)pop(queue);
}

uint32_t arq_uint(Arq_Queue *queue) {
        Arq_Argument t = pop(queue);
        assert(t.type_id == ARQ_TYPE_UINT);
        return t.value.u32;
}

uint32_t arq_array_size(Arq_Queue *queue) {
        Arq_Argument t = pop(queue);
        assert(t.type_id == ARQ_TYPE_ARRAY_SIZE);
        return t.value.u32;
}

#if 0
uint64_t arq_uint64_t(Arq_Queue *queue) {
        Arq_Argument t = pop(queue);
        assert(t.type_id == ARQ_OPT_UINT64_T);
        return t.value.u64;
}
#endif

int32_t arq_int(Arq_Queue *queue) {
        Arq_Argument t = pop(queue);
        assert(t.type_id == ARQ_TYPE_INT);
        return t.value.i32;
}

double arq_float(Arq_Queue *queue) {
        Arq_Argument t = pop(queue);
        assert(t.type_id == ARQ_TYPE_FLOAT);
        return t.value.f;
}

char const *arq_cstr_t(Arq_Queue *queue) {
        Arq_Argument t = pop(queue);
        assert(t.type_id == ARQ_TYPE_CSTR);
        return t.value.cstr;
}

void arq_push_uint(Arq_Queue *queue, union_o const *x) {
        Arq_Argument a;
        a.type_id = ARQ_TYPE_UINT;
        a.value.u32 = x->ou.u;
        push(queue, &a);
        log_int_uint(&x->ou);
}

uint32_t *arq_push_array_size(Arq_Queue *queue, uint32_t n) {
        Arq_Argument a;
        a.type_id = ARQ_TYPE_ARRAY_SIZE;
        a.value.u32 = n;
        push(queue, &a);
        return &queue->at[queue->write_idx - 1].value.u32;
}

#if 0
void arq_push_uint64_t(Arq_Queue *queue, uint64_t n) {
        Arq_Argument a;
        a.type_id = ARQ_OPT_UINT64_T;
        a.value.u64 = n;
        push(queue, &a);
}
#endif

void arq_push_int(Arq_Queue *queue, union_o const *x) {
        Arq_Argument a;
        a.type_id = ARQ_TYPE_INT;
        a.value.i32 = x->oi.i;
        push(queue, &a);
        log_int_int(&x->oi);
}

void arq_push_float(Arq_Queue *queue, union_o const *x) {
        Arq_Argument a;
        a.type_id = ARQ_TYPE_FLOAT;
        a.value.f = x->of.f;
        push(queue, &a);
        log_int_float(&x->of);
}

void arq_push_cstr_t(Arq_Queue *queue, char const * cstr) {
        Arq_Argument a;
        a.type_id = ARQ_TYPE_CSTR;
        a.value.cstr = cstr;
        push(queue, &a);
}

/*** End of inlined file: arq_queue.c ***/


/*** Start of inlined file: arq_main.c ***/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>

static void error_msg_append_option(Arq_msg *error_msg, Arq_Option const *o) {
        assert(o != NULL);
        if (o->chr != 0) {
                arq_msg_append_cstr(error_msg, "-");
                arq_msg_append_chr(error_msg, o->chr);
                arq_msg_append_cstr(error_msg, " ");
        }
        if (strlen(o->name) != 0) {
                arq_msg_append_cstr(error_msg, "--");
                arq_msg_append_cstr(error_msg, o->name);
                arq_msg_append_cstr(error_msg, " ");
        }
        if (strlen(o->arguments ) != 0) {
                arq_msg_append_cstr(error_msg, o->arguments);
        }
        arq_msg_append_lf(error_msg);
}

static void error_msg_insert_cmd_line(Arq_msg *m, uint32_t line_nr, Arq_LexerCmd *cmd) {
        Arq_Token const token = cmd->lexer.token;
        uint32_t A_IDX, B_IDX, C_IDX, D_IDX, ARGV_LEN;
        uint32_t ln_count = 0;
        uint32_t i;
        log_int_token(cmd->lexer.token.id);
        A_IDX = 0;
        for (i = 0; i < m->size; i++) {
                if (m->at[i] == '\n') {
                        ln_count++;
                        if (line_nr == ln_count) {
                                A_IDX = i + 1;
                        }
                }
        }

        B_IDX = m->size;
        cmd->argIdx = 0;
        cmd->lexer = arq_lexer_create();
        arq_lexer_next_cmd_token(cmd);
        while(true) {
                /* render argv to calculate argv_len */
                if (cmd->lexer.token.id == ARQ_CMD_SHORT_OPTION) {
                        uint32_t const x = cmd->lexer.token.at[0] == '-' ? 1 : 0; /* because of short option bundeling  */
                        arq_msg_append_chr(m, cmd->lexer.token.at[0]);
                        arq_msg_append_nchr(m, cmd->lexer.token.at[1], x);
                        arq_msg_append_chr(m, '_');
                } else if (cmd->lexer.token.id == ARQ_CMD_LONG_OPTION) {
                        /* arq_msg_append_nchr(m, '-', 2); */
                        arq_msg_append_cstr(m, cmd->lexer.token.at);
                        arq_msg_append_chr(m, '_');
                } else if (cmd->lexer.token.id == ARQ_NO_TOKEN) {
                } else {
                        arq_msg_append_cstr(m, cmd->lexer.token.at);
                        arq_msg_append_chr(m, '_');
                }
                if (cmd->lexer.token.at == token.at) {
                        break;
                }
                arq_lexer_next_cmd_token(cmd);
        }
        arq_msg_append_lf(m);
        C_IDX = m->size;
        ARGV_LEN = C_IDX - B_IDX;
        {
                uint32_t const shift_right = B_IDX - A_IDX;
                for (i = 0; i < shift_right; i++) {
                        uint32_t const idx = B_IDX - 1 - i;
                        m->at[idx + ARGV_LEN] = m->at[idx];
                }
        }

        D_IDX = m->size;
        cmd->argIdx = 0;
        cmd->lexer = arq_lexer_create();
        arq_lexer_next_cmd_token(cmd);
        while(true) {
                /* render argv once more for moving argv */
                if (cmd->lexer.token.id == ARQ_CMD_SHORT_OPTION) {
                        uint32_t const x = cmd->lexer.token.at[0] == '-' ? 1 : 0; /* because of short option bundeling  */
                        arq_msg_append_chr(m, cmd->lexer.token.at[0]);
                        arq_msg_append_nchr(m, cmd->lexer.token.at[1], x);
                        arq_msg_append_chr(m, ' ');
                } else if (cmd->lexer.token.id == ARQ_CMD_LONG_OPTION) {
                        /* arq_msg_append_nchr(m, '-', 2); */
                        arq_msg_append_cstr(m, cmd->lexer.token.at);
                        arq_msg_append_chr(m, ' ');
                } else if (cmd->lexer.token.id == ARQ_NO_TOKEN) {
                } else {
                        arq_msg_append_cstr(m, cmd->lexer.token.at);
                        arq_msg_append_chr(m, ' ');
                }
                if (cmd->lexer.token.at == token.at) {
                        break;
                }
                arq_lexer_next_cmd_token(cmd);
        }
        arq_msg_append_lf(m);

        {
                /* moving rendered argv */
                for (i = 0; i < ARGV_LEN; i++) {
                        m->at[i + A_IDX] =  m->at[i + D_IDX];
                }
                /* delete argv */
                m->size = D_IDX;
        }
}

static void output_error_msg(Arq_msg *error_msg, char *arena_buffer) {
        uint32_t i;
        arq_msg_format(error_msg);
        for (i = 0; i < error_msg->size; i++) {
                arena_buffer[i] = error_msg->at[i];
        }
        arena_buffer[error_msg->size] = 0;
        assert(arena_buffer[error_msg->size] == 0);
}

static void call_back_function(Arq_Option const *options, uint32_t option_idx, Arq_Queue *queue) {
        Arq_Option const *option = &options[option_idx];
        option->fn(queue);
        assert(queue->read_idx == queue->write_idx && "Queue is not empty, there are still arguments in the queue!");
        arq_queue_clear(queue);
}

static uint32_t arq_option_parameter_idx(Arq_Option const *option) {
        uint32_t STRLEN;
        uint32_t result = 0;
        if (option->chr != 0) {
                result += 3;
        }
        STRLEN = strlen(option->name);
        if (STRLEN > 0) {
                result += STRLEN + 3;
        }
        return result;
}

uint32_t arq_verify(
        char *arena_buffer, uint32_t const buffer_size,
        Arq_Option const *options, uint32_t const num_of_options
) {
        Arq_msg error_msg;
        uint32_t i;
        Arq_Arena *arena;
        (void) buffer_size;

        arena = arq_arena_init(arena_buffer, buffer_size);

        {
                uint32_t SIZE_OF_ERROR_MSG;
                uint32_t const shrink = arena->size;
                char *chr = (char *)arq_arena_malloc_rest(arena, 0, sizeof(char), &SIZE_OF_ERROR_MSG);
                arena->size = shrink;
                error_msg.SIZE = SIZE_OF_ERROR_MSG;
                error_msg.size = 0;
                error_msg.at = chr;
        }

        for (i = 0; i < num_of_options; i++) {
                bool for_loop_continue = false;
                Arq_LexerOpt opt = arq_lexerOpt_create();
                opt.lexer.at = options[i].arguments;
                opt.lexer.SIZE = strlen(options[i].arguments);
                opt.lexer.cursor_idx = 0;
                arq_lexer_next_opt_token(&opt);
                arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                arq_msg_append_cstr(&error_msg, "' missing open parenthesis '('\n");
                if (arq_imm(ARQ_OP_L_PARENTHESIS, &opt)) {
                        if (arq_imm(ARQ_OP_R_PARENTHESIS, &opt)) {
                                if (arq_imm_noToken(&opt.lexer.token)) {
                                        continue;
                                }
                                arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                arq_msg_append_cstr(&error_msg, "' after ')' no tokens allowed!\n");
                                goto error;
                        }
                        do {
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
                                arq_fn_imm_literal_error LITERAL_ERROR = NULL;
                                if      (arq_imm(ARQ_TYPE_UINT,  &opt)) LITERAL_ERROR = arq_imm_literal_uint_error;
                                else if (arq_imm(ARQ_TYPE_INT,   &opt)) LITERAL_ERROR = arq_imm_literal_int_error;
                                else if (arq_imm(ARQ_TYPE_FLOAT, &opt)) LITERAL_ERROR = arq_imm_literal_float_error;
                                else if (arq_imm(ARQ_TYPE_CSTR,  &opt)) LITERAL_ERROR = arq_imm_literal_NULL_error;
                                else {
                                        arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                        arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                        arq_msg_append_cstr(&error_msg, "' is not a type\n");
                                        goto error;
                                }

                                if (arq_imm_not_identifier(&opt)) {
                                        arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                        arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                        arq_msg_append_cstr(&error_msg, "' is not a parameter name\n");
                                        goto error;
                                } else {
                                        arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                        arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                        arq_msg_append_cstr(&error_msg, "' but expected '=' or '[]' or ',' or ')'\n");
                                }
                                if (arq_imm(ARQ_OP_EQ, &opt)) {
                                        if (LITERAL_ERROR(&opt, &error_msg)) {
                                                goto error;
                                        }
                                        arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                        arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                        arq_msg_append_cstr(&error_msg, "' but expected ',' or ')'\n");
                                } else if (arq_imm(ARQ_OP_ARRAY, &opt)) {
                                        arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                        arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                        arq_msg_append_cstr(&error_msg, "' but expected ',' or ')'\n");
                                }
                                if (arq_imm(ARQ_OP_COMMA, &opt)) {
                                        continue;
                                }
                                if (arq_imm(ARQ_OP_R_PARENTHESIS, &opt)) {
                                        arq_msg_set_cstr(&error_msg, OPTION_FAILURE);
                                        arq_msg_append_str(&error_msg, opt.lexer.token.at, opt.lexer.token.size);
                                        arq_msg_append_cstr(&error_msg, "' after ')' no tokens allowed!\n");
                                        if (arq_imm_noToken(&opt.lexer.token)) {
                                                for_loop_continue = true;
                                                break;
                                        }
                                }
                                goto error;
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
                        } while (opt.lexer.token.id != ARQ_NO_TOKEN);
                        if (for_loop_continue) {
                                continue;
                        }
                }
error:
                {
                        uint32_t n;
                        uint_o ups;
                        ups.error = true;
                        ups.u = opt.lexer.cursor_idx - opt.lexer.token.size;
                        n = arq_option_parameter_idx(&options[i]) + ups.u;
                        error_msg_append_option(&error_msg, &options[i]);
                        arq_msg_append_nchr(&error_msg, ' ', n);
                        arq_msg_append_cstr(&error_msg, "^\n");
                        output_error_msg(&error_msg, arena_buffer);
                        return error_msg.size;
                }

        } /* for loop */
        /* assert(false); */
        return 0;
}

uint32_t arq_fn(
        int argc, char **argv,
        char *arena_buffer, uint32_t const buffer_size,
        Arq_Option const *options, uint32_t const num_of_options
) {
        Arq_LexerCmd cmd = arq_lexerCmd_create(argc, argv);
        Arq_LexerOpt opt;
        Arq_Arena *arena;
        Arq_msg error_msg;
        Arq_Queue *queue;

        log_memory( ("size of arq_int %ld bit\n", 8 * sizeof(((uint_o *)0)->u) ));

        log_options_tokens(options, num_of_options);
        log_cmd_tokens(argc, argv);

        log_memory(("------- memory usage in byte --------"));
        log_memory(("%11shead %2scapacity %6srest", "", "", ""));
        log_memory(("Buffer    %5d %10d %10s", 0, buffer_size, "-"));
        arena = arq_arena_init(arena_buffer, buffer_size);
        log_memory(("Arena     %5d %10d %10s", (int)offsetof(Arq_Arena, at), arena->SIZE, "-"));

        {
                uint32_t SIZE_OF_ERROR_MSG = 500;
                error_msg.at = (char *)arq_arena_malloc(arena, SIZE_OF_ERROR_MSG);
                error_msg.SIZE = SIZE_OF_ERROR_MSG;
                error_msg.size = 0;
                log_memory(("error_msg %5d %10d %10d", 0, error_msg.SIZE, (int)(arena->SIZE - arena->size)));
        }

        log_int_banner("interpreter");

        queue = arq_queue_malloc(arena);

        log_memory(("Arq_Queue %5d %10d %10d",
                (int)offsetof(Arq_Queue, at),
                (int)(queue->NUM_OF_ARGUMENTS * sizeof(queue->at[0])),
                (int)(arena->SIZE - arena->size))
                /*(int)(arena->SIZE - queue->NUM_OF_ARGUMENTS * sizeof(queue->at[0])))*/

        );
        log_memory(("\n%d arguments fit in the queue.\n", queue->NUM_OF_ARGUMENTS));

        arq_lexer_next_cmd_token(&cmd);
        while(arq_imm_cmd_has_token_left(&cmd)) {
                struct {
                        arq_imm_default DEFAULT_VALUE;
                        arq_imm_optional_argument OPTIONAL_ARGUMENT;
                        arq_push PUSH;
                        arq_imm_is IS_LITERAL_TYPE;
                        arq_imm_argument ARGUMENT;
                } imm;
                /* Arq_OptVector *opt = NULL; */
                log_int_ln();
                if (arq_imm_cmd_is_long_option(&cmd)) {
                        log_int_token(ARQ_CMD_LONG_OPTION);
                        opt = arq_imm_get_long(options, num_of_options, &cmd, &error_msg);
                        if (opt.lexer.at == NULL) {
                                error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                output_error_msg(&error_msg, arena_buffer);
                                return error_msg.size;
                        }
                } else if (arq_imm_cmd_is_short_option(&cmd)) {
                        log_int_token(ARQ_CMD_SHORT_OPTION);
                        opt = arq_imm_get_short(options, num_of_options, &cmd, &error_msg);
                        if (opt.lexer.at == NULL) {
                                error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                output_error_msg(&error_msg, arena_buffer);
                                return error_msg.size;
                        }
                } else if (arq_imm_end_of_line(&cmd)) {
                        log_int_token(ARQ_NO_TOKEN);
                        arena_buffer[0] = 0;
                        return 0;
                } else {
                        arq_imm_cmd_not_a_option(&cmd, &error_msg);
                        error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                        output_error_msg(&error_msg, arena_buffer);
                        return error_msg.size;
                }
                arq_lexer_next_opt_token(&opt);
                (void)arq_imm(ARQ_OP_L_PARENTHESIS, &opt);
                while (true) {
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
                        if (arq_imm(ARQ_TYPE_CSTR, &opt)) {
                                char const *cstr;
                                log_int_token_indent(ARQ_TYPE_CSTR);
                                (void)arq_imm_not_identifier(&opt);
                                if (arq_imm(ARQ_OP_EQ, &opt)) {
                                        cstr = arq_imm_default_cstr_t(&opt);
                                        if (arq_imm_cmd_is_dashdash(&cmd)) {
                                                cstr = arq_imm_argument_csrt_t(&cmd, &error_msg);
                                                if (cstr == NULL) {
                                                        error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                                        arq_msg_append_cstr(&error_msg, "'--' alone isn't enough if you want '--' as an argument then do -- --\n");
                                                        arq_msg_append_cstr(&error_msg, "'--' allows you to set an argument that looks like an option -- --hello\n");
                                                        arq_msg_append_cstr(&error_msg, "'--' undoes optional behavior in case of an cstr_t = NULL\n");
                                                        error_msg_append_option(&error_msg, &options[opt.idx]);
                                                        output_error_msg(&error_msg, arena_buffer);
                                                        return error_msg.size;
                                                }
                                        } else {
                                                /* For a short option with an optional cstr_t as an argument. */
                                                /* It is not always possible to include the argument immediately after the option. */
                                                /* This is the case whether the next character is a bundled option or a character from a cstr_t. */
                                                /* If the next character is a number, then it is an argument => here is it possible. */
                                                /* {'S', "cstring", fn_cstring, &ctx, "cstr_t = NULL"}, */
                                                /* failure: -abcShello    => the 'h' is interpreted as short option part of the bundle (no space) thats why failure */
                                                /* ok:      -abcS hello   => is string token fine */
                                                /* ok:      -abcS69       => 69 is a number fine can't be a short option */
                                                (void)arq_imm_optional_argument_cstr_t(&cmd, &cstr);
                                        }
                                        arq_push_cstr_t(queue, cstr);
                                } else if (arq_imm(ARQ_OP_ARRAY, &opt)) {
                                        struct {
                                                bool on;
                                                bool edge;
                                        } dashdash = {0};
                                        uint32_t *array_size = arq_push_array_size(queue, 0);
                                        log_inta(("u32 %u // init array_size", *array_size));
                                        while (1) {
                                                dashdash.on |= arq_imm_cmd_is_dashdash(&cmd);
                                                if (dashdash.on && !arq_imm_pick_cstr_t(&cmd, &cstr)) {
                                                        if (dashdash.edge) {
                                                                break;
                                                        }
                                                        arq_msg_append_cstr(&error_msg, CMD_LINE_FAILURE);
                                                        arq_msg_append_cstr(&error_msg, "'--' alone isn't enough if you want '--' as an argument then do -- --\n");
                                                        arq_msg_append_cstr(&error_msg, "'--' allows to set an argument that looks like an option -- --hello\n");
                                                        arq_msg_append_cstr(&error_msg, "'--' switch to positional arguments in case of an cstr_t array\n");
                                                        error_msg_append_option(&error_msg, &options[opt.idx]);
                                                        output_error_msg(&error_msg, arena_buffer);
                                                        return error_msg.size;
                                                }
                                                if (!dashdash.on && !arq_imm_optional_argument_cstr_t(&cmd, &cstr)) {
                                                        break;
                                                }
                                                dashdash.edge = dashdash.on;
                                                *array_size += 1;
                                                arq_push_cstr_t(queue, cstr);
                                                log_inta(("cstr %s", cstr));
                                        }
                                } else {
                                        /* A short option with a mandatory argument allows the argument to be included immediately after the option. */
                                        /* However, this short option must be the last option in a bundle of options. */
                                        /* {'S', "cstring", fn_cstring, &ctx, "cstr_t"}, */
                                        /* 'hello' is the argument */
                                        /* ok: -abcShello    => 'hello' is the argument */
                                        /* ok: -abcS hello   => 'hello' is the argument */
                                        /* ok: -abcS--hello  => '--hello' is the argument */
                                        /* ok: -abcS-hello   => '-hello' is the argument */
                                        cstr = arq_imm_argument_csrt_t(&cmd, &error_msg);
                                        if (cstr == NULL) {
                                                error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                                error_msg_append_option(&error_msg, &options[opt.idx]);
                                                output_error_msg(&error_msg, arena_buffer);
                                                return error_msg.size;
                                        }
                                        arq_push_cstr_t(queue, cstr);
                                }
                                if (arq_imm(ARQ_OP_COMMA, &opt)) continue;
                                goto terminator;
/******************************************************************************/
/******************************************************************************/
/******************************************************************************/
                        } else if (arq_imm(ARQ_TYPE_UINT, &opt)) {
                                log_int_token_indent(ARQ_TYPE_UINT);
                                imm.DEFAULT_VALUE = arq_imm_default_uint;
                                imm.OPTIONAL_ARGUMENT = arq_imm_optional_argument_uint;
                                imm.PUSH = arq_push_uint;
                                imm.IS_LITERAL_TYPE = arq_imm_is_uint;
                                imm.ARGUMENT = arq_imm_argument_uint;

                        } else if (arq_imm(ARQ_TYPE_INT, &opt)) {
                                log_int_token_indent(ARQ_TYPE_INT);
                                imm.DEFAULT_VALUE = arq_imm_default_int;
                                imm.OPTIONAL_ARGUMENT = arq_imm_optional_argument_int;
                                imm.PUSH = arq_push_int;
                                imm.IS_LITERAL_TYPE = arq_imm_is_int;
                                imm.ARGUMENT = arq_imm_argument_int;

                        } else if (arq_imm(ARQ_TYPE_FLOAT, &opt)) {
                                log_int_token_indent(ARQ_TYPE_FLOAT);
                                imm.DEFAULT_VALUE = arq_imm_default_float;
                                imm.OPTIONAL_ARGUMENT = arq_imm_optional_argument_float;
                                imm.PUSH = arq_push_float;
                                imm.IS_LITERAL_TYPE = arq_imm_is_float;
                                imm.ARGUMENT = arq_imm_argument_float;

                        } else {
                                goto terminator;
                        }

                        (void)arq_imm_not_identifier(&opt);
                        if (arq_imm(ARQ_OP_EQ, &opt)) {
                                union_o x = imm.DEFAULT_VALUE(&opt);
                                /*uint_o num = arq_imm_default_uint(&opt);*/
                                if (imm.OPTIONAL_ARGUMENT(&cmd, &x, &error_msg)) {
                                        /* overflow */
                                        error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                        error_msg_append_option(&error_msg, &options[opt.idx]);
                                        output_error_msg(&error_msg, arena_buffer);
                                        return error_msg.size;
                                }
                                imm.PUSH(queue, &x);
                        } else if (arq_imm(ARQ_OP_ARRAY, &opt)) {
                                uint32_t *array_size = arq_push_array_size(queue, 0);
                                log_inta(("u32 %u // init array_size", *array_size));
                                while (imm.IS_LITERAL_TYPE(&cmd)) {
                                        union_o x = {0};
                                        if (imm.OPTIONAL_ARGUMENT(&cmd, &x, &error_msg)) {
                                                /* overflow */
                                                error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                                error_msg_append_option(&error_msg, &options[opt.idx]);
                                                output_error_msg(&error_msg, arena_buffer);
                                                return error_msg.size;
                                        }
                                        *array_size += 1;
                                        imm.PUSH(queue, &x);
                                }
                        } else {
                                union_o x = {0};
                                arq_msg_clear(&error_msg);
                                x = imm.ARGUMENT(&cmd, &error_msg);
                                if (error_msg.size > 0) {
                                        /* wasn't an uint32_t number or overflow */
                                        error_msg_insert_cmd_line(&error_msg, 1, &cmd);
                                        error_msg_append_option(&error_msg, &options[opt.idx]);
                                        output_error_msg(&error_msg, arena_buffer);
                                        return error_msg.size;
                                }
                                imm.PUSH(queue, &x);
                        }
                        if (arq_imm(ARQ_OP_COMMA, &opt)) continue;
terminator:
                        if (arq_imm(ARQ_OP_R_PARENTHESIS, &opt)) {
                                log_int_comment("call_back_function");
                                call_back_function(options, opt.idx, queue);
                                break;
                        }
                        assert(false);
                } /* while */
        } /* while */
        arena_buffer[0] = 0;
        return 0;
}

/*** End of inlined file: arq_main.c ***/

#endif

/*** Start of inlined file: license.h ***/
#if 1
/*
The MIT License (MIT)

Copyright (c) 2026 Bernhard Bertrand

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#endif
/*** End of inlined file: license.h ***/


