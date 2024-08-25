#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "ansi.h"

// https://en.wikipedia.org/wiki/Lexical_analysis
// https://www.reddit.com/r/ProgrammingLanguages/comments/1475h9o/comment/jntyywr/?utm_source=share&utm_medium=web3x&utm_name=web3xcss&utm_term=1&utm_content=share_button

#define ERR_AMBIGUOUS        1 
#define ERR_SOURCE_NOT_GIVEN 2 
#define ERR_SOURCE_NOT_FOUND 3 
#define ERR_SOURCE_READ_FAIL 4

#define ERRS_FATAL "fatal error"
#define ERRS_GEN "error"
#define ERRS_SYN "syntax error"

enum error_type {
    ERROR_FATAL,
    ERROR_GENERAL,
    ERROR_SYNTAX
};

static const char* errlog_out_progpath = NULL;

void compile_exit(int exitcode) {
    fprintf(stderr, "compilation terminated.\n");
    exit(exitcode);
}

void errlogf(enum error_type err, const char* errfmt, ...) {
    va_list args;
    va_start(args, errfmt);
    
    if (!errlog_out_progpath) {
        fprintf(stderr, "program path not set. This is a bug.\n");
        exit(1);
    }

    char* strbuf = NULL;
    size_t needed = vsnprintf(NULL, 0, errfmt, args) + 1;
    strbuf = calloc(needed, sizeof *strbuf);
    
    va_start(args, errfmt);

    vsnprintf(strbuf, needed, errfmt, args);
    const char* errs = NULL;
    switch (err) {
        case ERROR_FATAL:
            errs = ERRS_FATAL;
            break;
        case ERROR_GENERAL: 
            errs = ERRS_GEN;
            break;
        case ERROR_SYNTAX:
            errs = ERRS_SYN;
            break;
        default:
            __builtin_unreachable();
    }
    fprintf(stderr, "%s: " BRED "%s: " reset "%s", errlog_out_progpath, 
            errs, strbuf);
    if (errno) {
        fprintf(stderr, ": %s", strerror(errno));
    } fputs("\n", stderr);
    free(strbuf);
    
    va_end(args);
}

void perrorf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    size_t needed = vsnprintf(NULL, 0, fmt, args) + 1;
    char* strbuf = calloc(needed, 1);
    va_start(args, fmt);

    vsnprintf(strbuf, needed, fmt, args);
    perror(strbuf);

    free(strbuf);
    va_end(args);
}

typedef struct token {
    const char* start;
    const char* end;
} token;

typedef enum token_like {
    TL_SYMBOL,
    TL_EQ,
    TL_EQ_EQ,
    TL_COLON,
    TL_CCOLON,
    TL_PLUS,

    TL_QUOT,

    TL_PAREN_OPEN,
    TL_PAREN_END,
    TL_BRACE_OPEN,
    TL_BRACE_END,
    TL_COMMA,
    TL_SEMIC,
    TL_DOT
} token_like;

enum token_like next_token_like(const char* s) {
    token_like tokenlike;
    switch (*s) {
        case '=':
        tokenlike = TL_EQ;
        break;
        
        case '+':
        tokenlike = TL_PLUS;
        break;

        case '"':
        tokenlike = TL_QUOT;
        break;

        case '{':
        tokenlike = TL_BRACE_OPEN;
        break;

        case '}':
        tokenlike = TL_BRACE_END;
        break;

        case '(':
        tokenlike = TL_PAREN_OPEN;
        break;

        case ')':
        tokenlike = TL_PAREN_END;
        break;

        case ',':
        tokenlike = TL_COMMA;
        break;

        case ';':
        tokenlike = TL_SEMIC;
        break;

        case '.':
        tokenlike = TL_DOT;
        break;

        case ':':
        tokenlike = TL_COLON;
        break;

        default:
        tokenlike = TL_SYMBOL;
        break;
    }

    return tokenlike;

}

struct token next_token(const char* start) {
    while (*start == ' ' || *start == '\n')
        ++start; // we just skip these 
    const char* s = start;

    token_like tokenlike = next_token_like(s++);
    // string parse 
    // operator parse 
    // general symbol parse 

    if (tokenlike == TL_QUOT) {
        while (*s++ != '"') {
            if (!*s) {
                errlogf(ERROR_SYNTAX, "unterminated quote");
                exit(1);
            }
        }
        goto next_token__done;
    }

    if (tokenlike == TL_EQ) {
        if (*s == '=') {
            ++s;
        }
        goto next_token__done;
    }

    if (tokenlike == TL_COLON) {
        if (*s == ':') {
            ++s;
            goto next_token__done;
        }
        errlogf(ERROR_SYNTAX, "expected ::, found :");
        exit(1);
    }

    if (tokenlike == TL_SYMBOL) {
        while (*s && *s != ' ' && *s != '\n') {
            tokenlike = next_token_like(s);
            if (tokenlike != TL_SYMBOL) break;
            ++s;
        }
    }

next_token__done:
    return (token) { .start = start, .end = s };
}

int main(int argc, const char** argv) {
    errlog_out_progpath = argv[0];

    // TODO: parse with getopt
    if (argc < 2) {
        errlogf(ERROR_FATAL, "no input files");
        compile_exit(ERR_SOURCE_NOT_GIVEN);
    }

    const char* src = argv[1];

    FILE* src_fp = fopen(src, "r");
    if (!src_fp) {
        errlogf(ERROR_FATAL, "%s", src);
        //perrorf("%s: " BRED "fatal error: " reset "%s", argv[0], src);
        compile_exit(ERR_SOURCE_READ_FAIL);
    }

    char* filebuf;
    fseek(src_fp, 0L, SEEK_END);
    size_t len = ftell(src_fp);
    filebuf = calloc(len + 1, sizeof (char));
    filebuf[len] = '\0'; 
    fseek(src_fp, 0L, SEEK_SET);

    if (fread(filebuf, sizeof (char), len, src_fp) != len) {
        errlogf(ERROR_GENERAL, "not all bytes of source file read.");
        compile_exit(1);
    }

    fclose(src_fp);

    if (!filebuf) {
        errlogf(ERROR_FATAL, "source file empty");
        compile_exit(1);
    }

    const char* srcp = filebuf;
    struct token token = next_token(srcp);
    while (token.end - token.start > 0 && *srcp) {
        token = next_token(srcp);
        srcp = token.end;

        fwrite("'", 1, 1, stdout);
        fwrite(token.start, 1, token.end - token.start, stdout);
        fwrite("' ", 1, 2, stdout);
    } fputs("\n", stdout);

    free(filebuf);
}
