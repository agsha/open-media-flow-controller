/*
 * nkn_nodemap_clustermap_stubs.c
 *
 *	nkn_nodemap_clustermap.c is linked into libnkn_common.a
 *	which is shared by multiple binaries.   Stubs are added
 *	to break the dependency on libketama.so which is only required by
 *	nvsd.
 */
#include "stdlib.h"
#include <assert.h>
#include <string.h>
#include <strings.h>
#include <alloca.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <pthread.h>
#include "nkn_defs.h"
#include "ketama.h"

int ketama_roll(ketama_continuum *contptr, char *filename)
{
    UNUSED_ARGUMENT(contptr);
    UNUSED_ARGUMENT(filename);
    assert(!"ketama_roll() call to stub");
    return 0;
}

void ketama_smoke(ketama_continuum contptr)
{
    UNUSED_ARGUMENT(contptr);
    assert(!"ketama_smoke() call to stub");
    return;
}

mcs *ketama_get_server(char *key, ketama_continuum contptr)
{
    UNUSED_ARGUMENT(key);
    UNUSED_ARGUMENT(contptr);
    assert(!"ketama_get_server() call to stub");
    return 0;
}

void ketama_print_continuum(ketama_continuum contptr)
{
    UNUSED_ARGUMENT(contptr);
    assert(!"ketama_print_continuum() call to stub");
    return;
}

int ketama_compare(mcs *pm1, mcs *pm2)
{
    UNUSED_ARGUMENT(pm1);
    UNUSED_ARGUMENT(pm2);
    assert(!"ketama_compare() call to stub");
    return 0;
}

unsigned int ketama_hashi(char *inString)
{
    UNUSED_ARGUMENT(inString);
    assert(!"ketama_hashi() call to stub");
    return 0;
}

void ketama_md5_digest(char *inString, unsigned char md5pword[16])
{
    UNUSED_ARGUMENT(inString);
    UNUSED_ARGUMENT(md5pword);
    assert(!"ketama_md5_digest() call to stub");
    return;
}

char* ketama_error(void)
{
    assert(!"ketama_error() call to stub");
    return 0;
}

/*
 * End of nkn_nodemap_clustermap_stubs.c
 */
