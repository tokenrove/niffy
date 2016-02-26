
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "atom.h"
#include "macrology.h"
#include "nif_stubs.h"

/* We basically follow the ERTS tag and term structure here, but
 * loosely and with an eye on making implementation easy.
 */

#define TAG_PRIMARY_SIZE 2
#define TAG_PRIMARY ((1<<TAG_PRIMARY_SIZE)-1)
#define TAG_PRIMARY_LIST 0x1
#define TAG_PRIMARY_BOXED 0x2
#define TAG_PRIMARY_IMMED 0x3
#define TAG_IMMED1_SIZE 4
#define TAG_IMMED1 ((1<<TAG_IMMED1_SIZE)-1)
#define TAG_IMMED1_SMALL 0xf
#define TAG_IMMED2_SIZE 6
#define TAG_IMMED2 ((1<<TAG_IMMED2_SIZE)-1)
#define TAG_IMMED2_ATOM 0xB
#define TAG_IMMED2_NIL 0x3B
#define TAG_HEADER_SIZE 6
#define TAG_HEADER_FLONUM 0x18
#define TAG_HEADER_HEAP_BIN 0x24
#define TAG_HEADER_EXTERNAL_REF 0x38
#define TAG_HEADER ((1<<TAG_HEADER_SIZE)-1)

#define box(x) ((term)(x) | TAG_PRIMARY_BOXED)
#define unbox(x) (term *)((x) & ~TAG_PRIMARY)
#define box_list(x) ((term)(x) | TAG_PRIMARY_LIST)
#define make_small(x) (((x) << TAG_IMMED1_SIZE) | TAG_IMMED1_SMALL)

#define NIL TAG_IMMED2_NIL
#define MAX_ATOM_INDEX (~(~((unsigned) 0) << (sizeof(unsigned)*8 - TAG_IMMED2_SIZE)))

/* conservative */
#define MAX_LIST_LENGTH (1<<24)

const term nil = NIL;
const unsigned max_atom_index = MAX_ATOM_INDEX;

/* There's something I love about this name. */
#define THE_NON_VALUE 0

struct flonum {
    term header;
    double flonum;
};

#define CAR(p) ((p)[0])
#define CDR(p) ((p)[1])


static struct enif_environment_t global;

static bool track_freeable_pointer(struct enif_environment_t *env, void *p)
{
    struct alloc *cell = malloc(sizeof(*cell));
    if (cell == NULL)
        return false;

    if (!env) env = &global;
    *cell = (struct alloc){
        .p = p,
        .next = env->allocations
    };
    env->allocations = cell;
    return true;
}


__attribute__((alloc_size(2), malloc))
static void *alloc(ErlNifEnv *env, size_t size)
{
    void *p = malloc(size);
    if (p == NULL)
        return NULL;
    if (!track_freeable_pointer(env, p)) {
        free(p);
        return NULL;
    }
    return p;
}


term_type type_of_term(const term t)
{
    if (THE_NON_VALUE == t)
        return TERM_THE_NON_VALUE;

    switch (t & TAG_PRIMARY) {
    case TAG_PRIMARY_IMMED:
        if (TAG_IMMED1_SMALL == (t & TAG_IMMED1))
            return TERM_SMALL;
        switch (t & TAG_IMMED2) {
        case TAG_IMMED2_ATOM:
            return TERM_ATOM;
        case TAG_IMMED2_NIL:
            return TERM_NIL;
        default:
            return TERM_IMMEDIATE;
        }
        break;

    case TAG_PRIMARY_BOXED:
        return TERM_BOXED;

    case TAG_PRIMARY_LIST:
        return TERM_CONS;

    default:
        switch (t & TAG_HEADER) {
        case 0:
            return TERM_TUPLE;
        case TAG_HEADER_FLONUM:
            return TERM_FLOAT;
        case TAG_HEADER_HEAP_BIN:
            return TERM_BIN;
        case TAG_HEADER_EXTERNAL_REF:
            return TERM_EXTREF;
        default:
            return TERM_THING;
        }
        break;
    }

}


static void pretty_print_tuple(FILE *out, const term *p)
{
    unsigned count = (*p) >> TAG_HEADER_SIZE;
    fputc('{', out);
    for (unsigned i = 0; i < count; ++i) {
        if (i != 0)
            fputs(",", out);
        pretty_print_term(out, &p[1+i]);
    }
    fputc('}', out);
}


static bool is_printable_list(term t)
{
    if (NIL == t) return false;
    while (NIL != t) {
        if (TAG_PRIMARY_LIST != (t & TAG_PRIMARY))
            return false;
        term *p = unbox(t);
        int c;
        if (!enif_get_int(NULL, CAR(p), &c) ||
            !isgraph(c))
            return false;
        t = CDR(p);
    }
    return true;
}


static void print_list_as_string(FILE *out, term t)
{
    fputc('"', out);
    while (NIL != t) {
        assert(TAG_PRIMARY_LIST == (t & TAG_PRIMARY));
        term *p = unbox(t);
        int c;
        assert(enif_get_int(NULL, CAR(p), &c));
        if ('"' == c) fputc('\\', out);
        fputc(c, out);
        t = CDR(p);
    }
    fputc('"', out);
}


static void pretty_print_list(FILE *out, const term *p)
{
    term t = *p;
    if (is_printable_list(t)) {
        print_list_as_string(out, t);
        return;
    }

    bool print_sep_p = false;
    fputc('[', out);
    while (NIL != t) {
        if (TAG_PRIMARY_LIST != (t & TAG_PRIMARY)) {
            fputs("|", out);
            pretty_print_term(out, &t);
            break;
        }
        if (print_sep_p)
            fputs(",", out);
        else
            print_sep_p = true;
        p = unbox(t);
        pretty_print_term(out, &CAR(p));
        t = CDR(p);
    }
    fputc(']', out);
}


void pretty_print_argument_list(FILE *out, const term *p)
{
    term t = *p;
    bool print_sep_p = false;
    fputc('(', out);
    while (NIL != t) {
        if (print_sep_p)
            fputs(",", out);
        else
            print_sep_p = true;
        assert(TAG_PRIMARY_LIST == (t & TAG_PRIMARY));
        p = unbox(t);
        pretty_print_term(out, &CAR(p));
        t = CDR(p);
    }
    fputc(')', out);
}


static bool is_printable_binary(uint8_t *s, size_t len)
{
    if (0 == len) return false;
    for (size_t i = 0; i < len; ++i)
        if (!isgraph(s[i]))
            return false;
    return true;
}


static void print_binary_as_string(FILE *out, uint8_t *s, size_t len)
{
    fputc('"', out);
    for (size_t i = 0; i < len; ++i) {
        if ('"' == s[i]) fputc('\\', out);
        fputc(s[i], out);
    }
    fputc('"', out);
}


void pretty_print_binary(FILE *out, const term *p)
{
    unsigned size = p[0] >> TAG_HEADER_SIZE;
    uint8_t *q = (uint8_t *)(p+1);

    fputs("<<", out);
    if (is_printable_binary(q, size))
        print_binary_as_string(out, q, size);
    else {
        if (size > 0)
            fprintf(out, "%u", q[0]);
        for (unsigned i = 1; i < size; ++i)
            fprintf(out, ",%u", q[i]);
    }
    fputs(">>", out);
}


void pretty_print_term(FILE *out, const term *p)
{
    term t = *p;
    switch (type_of_term(t)) {
    case TERM_SMALL:
        fprintf(out, "%ld", (long)t >> TAG_IMMED1_SIZE);
        break;
    case TERM_ATOM:
        pretty_print_atom(out, t >> TAG_IMMED2_SIZE);
        break;
    case TERM_NIL:
        fputs("[]", out);
        break;
    case TERM_IMMEDIATE:
        fprintf(out, "<unknown immediate>");
        break;
    case TERM_BOXED:
        pretty_print_term(out, unbox(t));
        break;
    case TERM_TUPLE:
        pretty_print_tuple(out, p);
        break;
    case TERM_CONS:
        pretty_print_list(out, p);
        break;
    case TERM_FLOAT:
        {
            struct flonum *fn = (struct flonum *)p;
            fprintf(out, "%g", fn->flonum);
        }
        break;
    case TERM_BIN:
        pretty_print_binary(out, p);
        break;
    case TERM_EXTREF:
        fprintf(out, "<exref>");
        break;
    case TERM_THE_NON_VALUE:
        fprintf(out, "<THE_NON_VALUE>");
        break;
    default:
        fprintf(out, "<some mystery header thing type %lx>", t&TAG_HEADER);
    }
}


bool nconc(term a, term b)
{
    int max_len = MAX_LIST_LENGTH;
    do {
        if (TAG_PRIMARY_LIST != (a & TAG_PRIMARY))
            return false;
        term *p = unbox(a);
        if (NIL == CDR(p)) {
            CDR(p) = b;
            return true;
        }
        a = CDR(p);
    } while (--max_len > 0);
    return 0;
}


term nreverse_list(term head)
{
    term new = NIL;
    int max_len = MAX_LIST_LENGTH;
    do {
        if (head == NIL)
            return new;
        if (TAG_PRIMARY_LIST != (head & TAG_PRIMARY))
            abort();
        term *p = unbox(head);
        term next = CDR(p);
        CDR(p) = new;
        new = head;
        head = next;
    } while (--max_len > 0);
    abort();
}


static bool inner_iolist_to_binary(struct str **acc, term t)
{
    while (NIL != t) {
        if (TAG_PRIMARY_LIST != (t & TAG_PRIMARY)) {
            fprintf(stderr, "eh, don't know what to do with an improper list here\n");
            abort();
        }
        term *p = unbox(t);
        switch (type_of_term(CAR(p))) {
        case TERM_BIN:
            fprintf(stderr, "append bin\n");
            break;
        case TERM_SMALL:
            str_appendch(acc, CAR(p)>>TAG_IMMED1_SIZE);
            break;
        case TERM_FLOAT:
            fprintf(stderr, "append float\n");
            break;
        case TERM_CONS:
            if (!inner_iolist_to_binary(acc, CAR(p)))
                return false;
            break;
        default:
            fprintf(stderr, "dunno what to do with this\n");
            return false;
        }
        t = CDR(p);
    }
    return true;
}


term iolist_to_binary(term t)
{
    struct str *acc = str_new(1);
    if (!inner_iolist_to_binary(&acc, t)) {
        str_free(&acc);
        return THE_NON_VALUE;
    }
    term out = enif_make_binary(NULL, &(ErlNifBinary){.size = acc->len, .data = (unsigned char *)acc->data});
    str_free(&acc);
    return out;
}


void *enif_priv_data(ErlNifEnv *env) { return env->priv_data; }


term enif_make_int(ErlNifEnv *UNUSED, int i)
{
    return make_small(i);
}


term enif_make_uint(ErlNifEnv *UNUSED, unsigned i)
{
    return make_small(i);
}


term enif_make_long(ErlNifEnv *UNUSED, long i)
{
    return make_small(i);
}


int enif_get_uint(ErlNifEnv *UNUSED, term t, unsigned *ip)
{
    if (TAG_IMMED1_SMALL != (t & TAG_IMMED1))
        return 0;
    *ip = t >> TAG_IMMED1_SIZE;
    return 1;
}


int enif_get_int(ErlNifEnv *UNUSED, term t, int *ip)
{
    if (TAG_IMMED1_SMALL != (t & TAG_IMMED1))
        return 0;
    *ip = t >> TAG_IMMED1_SIZE;
    return 1;
}


int enif_get_long(ErlNifEnv *UNUSED, term t, long *ip)
{
    if (TAG_IMMED1_SMALL != (t & TAG_IMMED1))
        return 0;
    *ip = t >> TAG_IMMED1_SIZE;
    return 1;
}


term enif_make_double(ErlNifEnv *env, double d)
{
    struct flonum *p;

    p = alloc(env, sizeof(*p));
    p->header = TAG_HEADER_FLONUM;
    p->flonum = d;
    return box(p);
}


int enif_get_double(ErlNifEnv *UNUSED, term t, double *dp)
{
    if (TAG_PRIMARY_BOXED != (t & TAG_PRIMARY))
        return 0;
    struct flonum *p = (struct flonum *)unbox(t);
    if (TAG_HEADER_FLONUM != (p->header & TAG_HEADER))
        return 0;
    *dp = p->flonum;
    return 1;
}


static int cmp_bin(term *a, term *b)
{
    size_t alen = a[0]>>TAG_HEADER_SIZE,
        blen = b[0]>>TAG_HEADER_SIZE;
    if (alen != blen)
        return 0;
    return 0 == memcmp(a+1, b+1, alen);
}


static int cmp_cons(term *a, term *b)
{
    return enif_is_identical(CAR(a), CAR(b)) &&
        enif_is_identical(CDR(a), CDR(b));
}


static int cmp_tuple(term *a, term *b)
{
    unsigned aritya = a[0]>>TAG_HEADER_SIZE,
        arityb = b[0]>>TAG_HEADER_SIZE;
    if (aritya != arityb)
        return 0;
    for (unsigned i = 0; i < aritya; ++i)
        if (!enif_is_identical(a[1+i], b[1+i]))
            return 0;
    return 1;
}


int enif_is_identical(term a, term b)
{
    if (a == b) return 1;
    if (TAG_PRIMARY_IMMED == (a & b & TAG_PRIMARY))
        return 0;
    term_type at = type_of_term(a), bt = type_of_term(b);
    if (at != bt)
        return 0;
    switch (at) {
    case TERM_BIN:
        return cmp_bin(unbox(a), unbox(b));
    case TERM_CONS:
        return cmp_cons(unbox(a), unbox(b));
    case TERM_TUPLE:
        return cmp_tuple(unbox(a), unbox(b));
    default:
        return 0;
    }
}


int enif_compare(term a, term b)
{
    return -1;
}


int enif_is_atom(ErlNifEnv *UNUSED, term t)
{
    return type_of_term(t) == TERM_ATOM;
}


int enif_is_binary(ErlNifEnv *UNUSED, term t)
{
    return type_of_term(t) == TERM_BIN;
}


int enif_is_tuple(ErlNifEnv *UNUSED, term t)
{
    return type_of_term(t) == TERM_TUPLE;
}


int enif_is_list(ErlNifEnv *UNUSED, term t)
{
    term_type type = type_of_term(t);
    return (type == TERM_CONS) ||
        (type == TERM_NIL);
}


int enif_is_empty_list(ErlNifEnv *UNUSED, term t)
{
    return t == NIL;
}


int enif_is_number(ErlNifEnv *UNUSED, term t)
{
    return type_of_term(t) == TERM_SMALL;
}


int enif_is_ref(ErlNifEnv *UNUSED, term t)
{
    return type_of_term(t) == TERM_EXTREF;
}


/* Not sure what to do here. */
int enif_is_exception(ErlNifEnv *UNUSED, term UNUSED) { return 0; }

int enif_is_map(ErlNifEnv *UNUSED, term UNUSED) { return 0; }
int enif_is_fun(ErlNifEnv *UNUSED, term UNUSED) { return 0; }
int enif_is_pid(ErlNifEnv *UNUSED, term UNUSED) { return 0; }
int enif_is_port(ErlNifEnv *UNUSED, term UNUSED) { return 0; }

static term copy_tuple(ErlNifEnv *env, term *p)
{
    unsigned arity = p[0]>>TAG_HEADER_SIZE;
    return enif_make_tuple_from_array(env, p+1, arity);
}


static term copy_list(ErlNifEnv *UNUSED, term *UNUSED)
{
    fprintf(stderr, "copy_list not implemented\n");
    abort();
    return THE_NON_VALUE;
}


static term copy_bin(ErlNifEnv *env, term *p)
{
    size_t len = p[0]>>TAG_HEADER_SIZE;
    return enif_make_binary(env, &(ErlNifBinary){.size = len, .data = (uint8_t *)(p+1)});
}


term enif_make_copy(ErlNifEnv *env, term t)
{
    switch (type_of_term(t)) {
    case TERM_SMALL:
    case TERM_IMMEDIATE:
    case TERM_NIL:
        return t;
    case TERM_TUPLE:
        return copy_tuple(env, unbox(t));
    case TERM_CONS:
        return copy_list(env,unbox(t));
    case TERM_BIN:
        return copy_bin(env, unbox(t));
    default:
        /* XXX unimplemented */
        fprintf(stderr, "copying a %d is unimplemented\n", type_of_term(t));
        abort();
        return THE_NON_VALUE;
    }
}


term enif_make_atom(ErlNifEnv *env, const char *name)
{
    return enif_make_atom_len(env, name, strlen(name));
}


term tagged_atom(atom sym)
{
    return TAG_IMMED2_ATOM | (sym << TAG_IMMED2_SIZE);
}


atom atom_untagged(term t)
{
    assert(TAG_IMMED2_ATOM == (t & TAG_IMMED2));
    return t >> TAG_IMMED2_SIZE;
}


#define MAX_ATOM_CHARACTERS 255

term enif_make_atom_len(ErlNifEnv *env, const char *name, size_t len)
{
    if (len > MAX_ATOM_CHARACTERS)
        return enif_make_badarg(env);
    struct str *s = str_new(len);
    s->len = len;
    memcpy(s->data, name, len);
    return tagged_atom(intern(s));
}


int enif_make_existing_atom(ErlNifEnv *env, const char *name, term *atom, ErlNifCharEncoding encoding)
{
    return enif_make_existing_atom_len(env, name, strlen(name), atom, encoding);
}


int enif_make_existing_atom_len(ErlNifEnv *env, const char *name, size_t len, term *atom, ErlNifCharEncoding encoding)
{
    assert(ERL_NIF_LATIN1 == encoding);
    /* XXX need to avoid interning here */
    *atom = enif_make_atom_len(env, name, len);
    return 1;
}


int enif_get_atom_length(ErlNifEnv *UNUSED, term t, unsigned *len, ErlNifCharEncoding UNUSED)
{
    if (TAG_IMMED2_ATOM != (t & TAG_IMMED2))
        return 0;
    atom sym = t >> TAG_IMMED2_SIZE;
    const struct str *s = symbol_name(sym);
    *len = s->len;
    return 1;
}


int enif_get_atom(ErlNifEnv *UNUSED, term t, char *buf, unsigned len,
                  ErlNifCharEncoding UNUSED)
{
    if (TAG_IMMED2_ATOM != (t & TAG_IMMED2))
        return 0;
    atom sym = t >> TAG_IMMED2_SIZE;
    const struct str *s = symbol_name(sym);
    if (s->len >= len)
        return 0;
    memcpy(buf, s->data, s->len);
    buf[s->len] = 0;
    return s->len;
}


term enif_make_tuple(ErlNifEnv *env, unsigned count, ...)
{
    term *t = alloc(env, (1+count) * sizeof(*t));
    t[0] = count << TAG_HEADER_SIZE;
    va_list ap;
    va_start(ap, count);
    for (unsigned i = 0; i < count; ++i)
        t[i+1] = va_arg(ap, term);
    va_end(ap);
    return box(t);
}


term enif_make_tuple_from_array(ErlNifEnv *env, const term arr[], unsigned count)
{
    term *t = alloc(env, (1+count) * sizeof(*t));
    t[0] = count << TAG_HEADER_SIZE;
    memcpy(t+1, arr, sizeof(*t) * count);
    return box(t);
}


int enif_get_tuple(ErlNifEnv *UNUSED, term tuple, int *arity, const term **array)
{
    if (TAG_PRIMARY_BOXED != (tuple & TAG_PRIMARY))
        return 0;
    term *p = unbox(tuple);
    if (0 != (*p & TAG_HEADER))
        return 0;
    if (arity)
        *arity = *p >> TAG_HEADER_SIZE;
    if (array)
        *array = p+1;
    return 1;
}


term tuple_of_list(ErlNifEnv *env, term head)
{
    unsigned count;
    if (1 != enif_get_list_length(env, head, &count))
        return THE_NON_VALUE;
    term *t = alloc(env, (1+count) * sizeof(*t));
    t[0] = count << TAG_HEADER_SIZE;
    for (unsigned i = 0; i < count; ++i) {
        term *cell = unbox(head);
        t[i+1] = CAR(cell);
        head = CDR(cell);
        term_type type = type_of_term(head);
        assert(TERM_NIL == type || TERM_CONS == type);
    }
    return box(t);
}


#define HEAP_BIN_TAG(s) (TAG_HEADER_HEAP_BIN | ((s)<< TAG_HEADER_SIZE))

term enif_make_binary(ErlNifEnv *env, ErlNifBinary *bin)
{
    /* Right now, all our binaries are heap binaries. */
    term *p = alloc(env, sizeof(*p) + bin->size);
    memcpy(p+1, bin->data, bin->size);
    p[0] = HEAP_BIN_TAG(bin->size);
    return box(p);
}


term enif_make_sub_binary(ErlNifEnv *env, term bin_term, size_t pos, size_t size)
{
    /* XXX shouldn't allocate */
    term *q = unbox(bin_term);
    term *p = alloc(env, sizeof(*p) + size);
    memcpy(p+1, ((uint8_t *)(q+1))+pos, size);
    p[0] = HEAP_BIN_TAG(size);
    return box(p);
}


unsigned char *enif_make_new_binary(ErlNifEnv *env, size_t size, term *termp)
{
    /* XXX */
    term *p = alloc(env, sizeof(*p) + size);
    p[0] = HEAP_BIN_TAG(size);
    if (termp) *termp = box(p);
    return p+1;
}


int enif_inspect_binary(ErlNifEnv *UNUSED, term t, ErlNifBinary *bin)
{
    if (TAG_PRIMARY_BOXED != (t & TAG_PRIMARY))
        return 0;
    term *p = unbox(t);
    if (TAG_HEADER_HEAP_BIN != (*p & TAG_HEADER))
        return 0;
    bin->size = *p >> TAG_HEADER_SIZE;
    bin->data = (unsigned char *)(p+1);
    return 1;
}


/* XXX wasteful */
int enif_inspect_iolist_as_binary(ErlNifEnv *env, term t, ErlNifBinary *bin)
{
    return enif_inspect_binary(env, iolist_to_binary(t), bin);
}


int enif_alloc_binary(size_t size, ErlNifBinary *bin)
{
    bin->data = malloc(size);
    if (NULL == bin->data)
        return 0;
    bin->size = size;
    return 1;
}


int enif_realloc_binary(ErlNifBinary *bin, size_t size)
{
    unsigned char *p = realloc(bin->data, size);
    if (NULL == p)
        return 0;
    bin->data = p;
    bin->size = size;
    return 1;
}


int enif_has_pending_exception(ErlNifEnv *env, term *reason)
{
    if (reason) *reason = env->exception;
    return !!env->exception;
}


void *enif_alloc(size_t size)
{
    return malloc(size);
}


void enif_free(void *p)
{
    free(p);
}


void *enif_alloc_resource(ErlNifResourceType *UNUSED, size_t size)
{
    return malloc(size);
}


void enif_release_resource(void *UNUSED)
{
    /* XXX We don't free the resource here, because you're supposed to have tracked it with enif_make_resource; however, it would behoove us to still have  */
}


term enif_make_resource(ErlNifEnv *env, void *obj)
{
    if (!track_freeable_pointer(env, obj))
        abort();
    term *p = alloc(env, sizeof(*p) + sizeof(obj));
    *p = TAG_HEADER_EXTERNAL_REF;
    void **q = (void **)(p+1);
    *q = obj;
    return box(p);
}


int enif_get_resource(ErlNifEnv *UNUSED, term t, ErlNifResourceType *UNUSED,
                      void **objp)
{
    if (TAG_PRIMARY_BOXED != (t & TAG_PRIMARY))
        return 0;
    term *p = unbox(t);
    if (*p != TAG_HEADER_EXTERNAL_REF)
        return 0;
    void **q = (void **)(p+1);
    *objp = *q;
    return 1;
}


term enif_make_badarg(ErlNifEnv *env)
{
    env->exception = tagged_atom(intern(str_dup_cstr("badarg")));
    return env->exception;
}


term enif_make_list(ErlNifEnv *env, unsigned count, ...)
{
    if (0 == count)
        return NIL;

    term *p = alloc(env, 2*count*sizeof(*p));
    term head = box_list(p);
    term *q = &head;

    va_list ap;
    va_start(ap, count);
    for (unsigned i = 0; i < count; ++i, p += 2) {
        *q = box_list(p);
        CAR(p) = va_arg(ap, term);
        q = &CDR(p);
    }
    va_end(ap);
    *q = NIL;
    return head;
}


term enif_make_list_from_array(ErlNifEnv *env, const term arr[], unsigned count)
{
    if (0 == count)
        return NIL;

    term *p = alloc(env, 2*count*sizeof(*p));
    term head = box_list(p);
    term *q = &head;

    for (unsigned i = 0; i < count; ++i, p += 2) {
        *q = box_list(p);
        CAR(p) = arr[i];
        q = &CDR(p);
    }
    *q = NIL;
    return head;
}


int enif_get_list_length(ErlNifEnv *UNUSED, term t, unsigned *len)
{
    int max_len = MAX_LIST_LENGTH;
    *len = 0;
    do {
        if (NIL == t) return 1;
        if (TAG_PRIMARY_LIST != (t & TAG_PRIMARY))
            return 0;
        ++(*len);
        term *cell = unbox(t);
        t = CDR(cell);
    } while (--max_len > 0);
    /* list was too long. */
    return 0;
}


int enif_get_list_cell(ErlNifEnv *UNUSED, term t, term *car, term *cdr)
{
    if (TAG_PRIMARY_LIST != (t & TAG_PRIMARY))
        return 0;
    term *cell = unbox(t);
    if (car) *car = CAR(cell);
    if (cdr) *cdr = CDR(cell);
    return 1;
}


term enif_make_list_cell(ErlNifEnv *env, term car, term cdr)
{
    term *cell = alloc(env, 2*sizeof(*cell));
    term p = box_list(cell);
    CAR(cell) = car;
    CDR(cell) = cdr;
    return p;
}


ERL_NIF_TERM enif_make_string(ErlNifEnv* env, const char* string,
                              ErlNifCharEncoding encoding)
{
    return enif_make_string_len(env, string, strlen(string), encoding);
}


ERL_NIF_TERM enif_make_string_len(ErlNifEnv *env, const char *string,
                                  size_t len, ErlNifCharEncoding encoding)
{
    assert(encoding == ERL_NIF_LATIN1);
    term *p = alloc(env, 2*len*sizeof(*p));
    term head = box_list(p);
    term *q = &head;

    for (unsigned i = 0; i < len; ++i, p += 2) {
        *q = box_list(p);
        CAR(p) = make_small(string[i]);
        q = &CDR(p);
    }
    *q = NIL;
    return head;
}


int enif_get_string(ErlNifEnv *UNUSED, term t, char *buf, unsigned size,
                    ErlNifCharEncoding UNUSED)
{
    unsigned count = 0;
    while (count < size && NIL != t) {
        if (type_of_term(t) != TERM_CONS)
            return 0;
        term *p = unbox(t);
        if (type_of_term(CAR(p)) != TERM_SMALL)
            return 0;
        /* XXX urk, could be a big character */
        buf[count++] = CAR(p) >> TAG_IMMED1_SIZE;
        t = CDR(p);
    }
    if (size > 0)
        buf[count++] = 0;
    return (NIL == t) ? count : -count;
}


/*
 * FUNCTIONS WHOSE IMPLEMENTATION IS PATHOLOGICAL
 */


ErlNifPid *enif_self(ErlNifEnv *UNUSED, ErlNifPid *pid)
{
    assert(pid);
    pid->pid = 1;
    return pid;
}


int enif_is_on_dirty_scheduler(ErlNifEnv *UNUSED)
{
    return 1;                   /* why not? */
}


ErlNifResourceType *
enif_open_resource_type(ErlNifEnv *UNUSED,
                        const char *UNUSED,
                        const char *UNUSED,
                        ErlNifResourceDtor *UNUSED,
                        ErlNifResourceFlags flags, ErlNifResourceFlags *tried)
{
    if (tried) *tried = flags;
    return NULL;
}


term enif_make_ref(ErlNifEnv *UNUSED)
{
    static int64_t counter = 0;
    return make_small(counter++);
}


ErlNifEnv *enif_alloc_env(void)
{
    /* XXX unimplemented */
    struct enif_environment_t *env = calloc(1, sizeof(*env));
    return env;
}


void enif_free_env(ErlNifEnv *env)
{
    bool global_p = false;
    if (!env) {
        env = &global;
        global_p = true;
    }
    for (struct alloc *ap = env->allocations; ap; ap = ap->next) {
        free(ap->p);
        ap->p = NULL;
    }
    if (!global_p)
        free(env);
}




/*
 * REALLY UNIMPLEMENTED FUNCTIONS
 */

int enif_thread_create(char *UNUSED, ErlNifTid *UNUSED,
                       void *(_)(void *) __attribute__((unused)),
                       void *UNUSED, ErlNifThreadOpts *UNUSED)
{
    abort();
}


int enif_thread_join(ErlNifTid UNUSED, void **UNUSED)
{
    abort();
}


int enif_send(ErlNifEnv *UNUSED, const ErlNifPid *UNUSED,
              ErlNifEnv *UNUSED, term UNUSED)
{
    /* XXX unimplemented */
    assert(0);
    return 0;
}


int enif_map_iterator_create(ErlNifEnv *UNUSED,
                             term UNUSED,
                             ErlNifMapIterator *UNUSED,
                             ErlNifMapIteratorEntry UNUSED)
{
    abort();
    return 0;
}

void enif_map_iterator_destroy(ErlNifEnv *UNUSED, ErlNifMapIterator *UNUSED)
{
    abort();
}

int enif_map_iterator_is_tail(ErlNifEnv *UNUSED, ErlNifMapIterator *UNUSED)
{
    abort();
}

int enif_map_iterator_is_head(ErlNifEnv *UNUSED, ErlNifMapIterator *UNUSED)
{
    abort();
}

int enif_map_iterator_next(ErlNifEnv *UNUSED, ErlNifMapIterator *UNUSED)
{
    abort();
}

int enif_map_iterator_prev(ErlNifEnv *UNUSED, ErlNifMapIterator *UNUSED)
{
    abort();
}

int enif_map_iterator_get_pair(ErlNifEnv *UNUSED,
                               ErlNifMapIterator *UNUSED,
                               term *UNUSED,
                               term *UNUSED)
{
    abort();
}


int enif_consume_timeslice(ErlNifEnv *env, int percent)
{
    /* XXX should log */
    return 0;
}
