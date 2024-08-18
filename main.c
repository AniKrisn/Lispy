#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char *readline(char *prompt) {
    fputs(prompt, stdout);
    if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        return NULL;  // Handle EOF or error
    }
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len-1] == '\n') {
        buffer[len-1] = '\0';  // Remove newline in-place
    }
    return strdup(buffer);  // Create a new copy and return it
}


//??
void add_history(char *unused) {}


#else 
#include <editline/readline.h>
#include <editline/history.h>
#endif

// possible lval types
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR };

// def "lisp value" -- 
typedef struct lval {
    int type;
    long num;
    // err and symbol types have string data
    char *err;
    char *sym;
    // count and point to a list of "lval*"
    int count;
    struct lval **cell;
} lval;


// init all types with constructor functions
// construct a pointer to a new num lval
lval *lval_num (long x) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}
// construct a pointer to a new err lval
lval *lval_err(char *m) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);  
    return v; 
}
// construct a pointer to a new sym lval
lval *lval_sym(char *s) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}
// a pointer to a new empty sexpr lval
lval *lval_sexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
} 


// destructor for lval, frees the memory used by the lval after used
void lval_del(lval *v) {
    switch (v->type) {
        // for nums the type is long so nothing special
        case LVAL_NUM: break;
        // err and sym are strings so freeing is straightforward
        case LVAL_ERR: free(v->err); break;
        case LVAL_SYM: free(v->sym); break;
        // sexprs are lists so we need to free each element and then the mem used to store the pointers

        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
        break;
    }
    // free the mem used to store the lval struct
    free(v);
}


lval *lval_add(lval *v, lval *x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}


lval *lval_read_num(mpc_ast_t *t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval *lval_read(mpc_ast_t *t) {
    // if the tag is a number or sym return a pointer to a num lval
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    lval *x = NULL;
    // ">" is the root of the expression in the AST. If root or sexpr, create empty list
    if (strcmp(t->tag, ">") == 0) {x = lval_sexpr();}
    if (strstr(t->tag, "sexpr"))  {x = lval_sexpr();}

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}



/* use operator string to see which operation to perform */
/*
lval eval_op(lval x, char* op, lval y) {

    // if either operands are errs return err
    if (x.type == LVAL_ERR) { return x; }
    if (y.type == LVAL_ERR) { return y; }
    
    if (strcmp(op, "+") == 0) {return lval_num(x.num + y.num);}
    if (strcmp(op, "-") == 0) {return lval_num(x.num - y.num);}
    if (strcmp(op, "*") == 0) {return lval_num(x.num * y.num);}
    if (strcmp(op, "/") == 0) {
        // if second operand is 0 return corresponding err
        return y.num == 0
        ? lval_err("div 0")
        : lval_num(x.num / y.num);
        
    }
    
    return lval_err("bad number");
}
*/





// forward declaration, allows us to use lval_print before it is defined: sometimes lval_expr_print needs it
void lval_print(lval *v);

// for sexprs
void lval_expr_print(lval *v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);
        if (i != v->count - 1) {
            putchar(' ');
        }
    }
    putchar(close);
}

// print an lval
void lval_print(lval *v) {
    switch (v->type) {
        case LVAL_NUM: printf("%li", v->num); break;
        case LVAL_ERR: printf("Error: %s", v->err); break;
        case LVAL_SYM: printf("%s", v->sym); break;
        case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
    }
}

// print an lval followed by a newline
void lval_println(lval *v) { lval_print(v); putchar('\n'); }


int main (int argc, char **argv) {

    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr  = mpc_new("sexpr");
    mpc_parser_t *Expr   = mpc_new("expr");
    mpc_parser_t *Lispy  = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
    "                                                       \
        number   : /-?[0-9]+/ ;                             \
        symbol   : '+' | '-' | '*' | '/' ;                  \
        sexpr    : '(' <expr>* ')' ;                        \
        expr     : <number> | <symbol> | <expr> ;  \
        lispy    : /^/ <expr>* /$/ ;             \
    ",
    Number, Symbol, Sexpr, Expr, Lispy);


    puts("Lispy Version 0.1");
    puts("Press Ctrl+C to escape\n");  
    
    while (1) {

        char *input = readline("lispy> ");

        // readline history is stored separately, primarily in the heap. Calling free() doesn't affect this; there are separate functions to read/write the readline history
        add_history(input);

        // pass user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {

            // lval result = eval(r.output);
            lval *x = lval_read(r.output);
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        // if successful, eval & print, else error

        free(input);
    }

    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);
    // undefine and delete the parsers

    return 0;
}