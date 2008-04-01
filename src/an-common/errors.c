
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "errors.h"
#include "ioutils.h"

static pl* estack = NULL;

static err_t* error_copy(err_t* e) {
    err_t* copy = error_new();
    copy->print = e->print;
    copy->save = e->save;
    sl_append_contents(copy->errstack, e->errstack);
    sl_append_contents(copy->modstack, e->modstack);
    return copy;
}

err_t* errors_get_state() {
    if (!estack)
        estack = pl_new(4);
    if (!pl_size(estack)) {
        err_t* e = error_new();
        e->print = stderr;
        pl_append(estack, e);
    }
    return pl_get(estack, pl_size(estack)-1);
}

void errors_free() {
    int i;
    for (i=0; i<pl_size(estack); i++) {
        err_t* e = pl_get(estack, i);
        error_free(e);
    }
    pl_free(estack);
}

void errors_push_state() {
    err_t* now;
    err_t* snapshot;
    // make sure the stack and current state are initialized
    errors_get_state();
    now = pl_pop(estack);
    snapshot = error_copy(now);
    pl_push(estack, snapshot);
    pl_push(estack, now);
}

void errors_pop_state() {
    err_t* now = pl_pop(estack);
    error_free(now);
}

void errors_print_stack(FILE* f) {
    error_print_stack(errors_get_state(), f);
}

void report_error(const char* modfile, int modline, const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    error_reportv(errors_get_state(), modfile, modline, fmt, va);
    va_end(va);
}

void report_errno() {
    error_report(errors_get_state(), "system", -1, "%s", strerror(errno));
}

err_t* error_new() {
    err_t* e = calloc(1, sizeof(err_t));
    e->modstack = sl_new(4);
    e->errstack = sl_new(4);
    return e;
}

void error_free(err_t* e) {
    if (!e) return;
    sl_free2(e->modstack);
    sl_free2(e->errstack);
    free(e);
}

void error_nerrs(err_t* e) {
    return sl_size(e->errstack);
}

char* error_get_errstr(err_t* e, int i) {
    return sl_get(e->errstack, i);
}

void error_report(err_t* e, const char* module, int line, const char* fmt, ...) {
    va_list va;
    va_start(va, fmt);
    error_reportv(errors_get_state(), module, line, fmt, va);
    va_end(va);
}

void error_reportv(err_t* e, const char* module, int line, const char* fmt, va_list va) {
    if (e->print) {
        fprintf(e->print, "%s:%i: ", module, line);
        vfprintf(e->print, fmt, va);
        fprintf(e->print, "\n");
    }
    if (e->save) {
        sl_appendvf(e->errstack, fmt, va);
        if (line >= 0) {
            sl_appendf(e->modstack, "%s:%i", module, line);
        } else {
            sl_appendf(e->modstack, "%s", module);
        }
    }
}

void error_print_stack(err_t* e, FILE* f) {
    int i;
    for (i=0; i<sl_size(e->modstack); i++) {
        char* mod = sl_get(e->modstack, i);
        char* err = sl_get(e->errstack, i);
        fprintf(f, "%s: %s\n", mod, err);
    }
}

char* error_get_errs(err_t* e, const char* separator) {
    return sl_join_reverse(e->errstack, separator);
}
