#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <sys/queue.h>
#include <assert.h>

#include "nkn_stat.h"
#include "uri_mgr.h"
#include "nkn_memalloc.h"


/*
 * Uri manager includes a hash table.
 * hash function is defined in uri_hash_value().
 * like checksum, it calculate the checksum value of uri.
 */


/*
 * This structure is defined to point to each uri mgr slot
 * This structure is privately hold by uri_mgr.c file.
 * No module should reference to any internal fiedls.
 */
typedef struct uri_mgr {
        /* public data */
        char * uri;
        int32_t uri_size;       /* length of the uri string */

        /* private data */
        uint32_t magic;
        LIST_ENTRY(uri_mgr) entries;
        int32_t rfc;            /* currently referenced so many times */
} uri_mgr_t;


NKNCNT_DEF(tot_uri_in_urimgr, int32_t, "", "Total uri entries storied in URI manager")

static pthread_mutex_t um_mutex = PTHREAD_MUTEX_INITIALIZER;

#define URI_HT_SIZE	32767   // 2^15-1
#define MAGIC_SIG	0x12345678

/*
 * Build up a hash table with a link list
 */
LIST_HEAD(urimgr_ht_queue, uri_mgr);
struct urimgr_ht_queue uri_ht[URI_HT_SIZE];

void urimgr_display(void);

void urimgr_init(void)
{
	int i;
	for(i=0;i<URI_HT_SIZE;i++) {
		LIST_INIT( &uri_ht[i] );
	}
}

/*
 * Calculate the hash value
 */
static uint16_t uri_hash_value(char * uri)
{
	uint16_t hvalue;
	uint16_t * pvalue;
	unsigned int len;

	/* 
	 * when calculating hash value,
	 * if uri length is odd, we count in the ending '\0'
	 * if uri length is even, we does not include the endinf '\0'
	 */

	hvalue=0;
	pvalue=(uint16_t *)uri;
	for(len=0; len<(strlen(uri)+1)/2; len++) {
		hvalue += *pvalue;
		pvalue++;
	}
	hvalue = hvalue % URI_HT_SIZE;

	return hvalue;
}

/*
 * Make a copy and do the conversion of uri
 * the conversion includes:
 * 1. convert %xx into ascii
 * 2. the flag can be set if it needs to cut off "?"
 */
static char * urimgr_dup_uri(char * uri, int cutoff)
{
	int len;
	char *pbuf, *ps, *pd;

	if(!uri) return NULL;

	len = strlen(uri);
	pbuf = (char *)nkn_malloc_type(sizeof(char)*len, mod_uri_mgr_t);
	if(!pbuf) return NULL;

	pd = pbuf;
	ps = uri;
	while(len) {
		switch(*ps) {

		case '%':
		{
			char ch;

			if(len < 3) goto done;

			/* 
			 * This is not a good code here,
			 * but don't know anyway to improve it.
			 */

			// convert format %xx into ascii
			ch = *(ps+1); 
			if(ch>='0' && ch<='9') *pd = ch - '0';
			else if(ch>='a' && ch<='f') *pd = ch - 'a';
			else if(ch>='A' && ch<='F') *pd = ch - 'A';
			else goto done;
			
			*pd = *pd << 4;

			ch = *(ps+2);
			if(ch>='0' && ch<='9') *pd += ch - '0';
			else if(ch>='a' && ch<='f') *pd += ch - 'a';
			else if(ch>='A' && ch<='F') *pd += ch - 'A';
			else goto done;

			pd++; ps+=3; len-=3;
		}
		break;

		case '?':
			if(cutoff) goto done;
			// Otherwise, fall through default case 
		default:
			*pd = *ps;
			pd++; ps++; len--;
		break;
		}
	}
done:
	*pd=0;
	return pbuf;
}

/*
 * Any module can call to add one uri into urimgr.
 * If the same uri is found, it will increase the counter by one.
 * The return id should NOT be casted into a pointer.
 *
 * urimgr_add_uri makes a copy of uri, so caller can release this buffer
 * after called.
 *
 * return 0: failed to add this uri into uri manager.
 */

uint64_t urimgr_add_uri(char * uri)
{
	uint16_t hvalue;
	int uri_size;
	uri_mgr_t * pum;

	if(!uri) return 0;
	uri_size=strlen(uri);
	if(!uri_size) return 0;
	hvalue=uri_hash_value(uri);

	pthread_mutex_lock(&um_mutex);

	// Follow through the collision link list
	LIST_FOREACH(pum, &uri_ht[hvalue], entries)
	{
		if(pum->uri_size != uri_size) continue;
		if(strcmp(pum->uri, uri)==0) {
			// It has already had one entry.
			pum->rfc++;
			pthread_mutex_unlock(&um_mutex);
			return (uint64_t)pum;
		}
	}

	// not found the same uri, we need to insert one
	pum = nkn_malloc_type(sizeof(uri_mgr_t), mod_uri_mgr_t);
	if(!pum) {
		pthread_mutex_unlock(&um_mutex);
		return 0;
	}
	pum->magic=MAGIC_SIG;
	pum->uri=urimgr_dup_uri(uri, 0);
	pum->uri_size=uri_size;
	pum->rfc=1;
	LIST_INSERT_HEAD(&uri_ht[hvalue], pum, entries);

	glob_tot_uri_in_urimgr++;

	pthread_mutex_unlock(&um_mutex);

	return (uint64_t)pum;
}

/*
 * When provided the uri_id, you can get the address of uri string.
 * After get the pointer, any module should not modify the
 * content returned by this function.
 */

char * urimgr_get_uri_addr(uint64_t uri_id)
{
	uri_mgr_t * pum;

	assert(uri_id != 0);
	pum=(uri_mgr_t *)uri_id;
	assert(pum->magic == MAGIC_SIG);

	return pum->uri;
}

/*
 * For performance concern, any module does not need to calculate the
 * size of uri if needed. Instead of it, any module can apply this
 * function to withdraw the size of uri.
 */

int32_t urimgr_get_uri_size(uint64_t uri_id)
{
	uri_mgr_t * pum;

	assert(uri_id != 0);
	pum=(uri_mgr_t *)uri_id;
	assert(pum->magic == MAGIC_SIG);

	return pum->uri_size;
}

/*
 * Lookup up the uri_id based on uri
 *
 * return 0: failed to lookup this uri.
 */
uint64_t urimgr_lookup_uri(char * uri)
{
	uint16_t hvalue;
	int uri_size;
	uri_mgr_t * pum;

	if(!uri) return 0;
	uri_size=strlen(uri);
	if(!uri_size) return 0;
	hvalue=uri_hash_value(uri);

	pthread_mutex_lock(&um_mutex);

	// Follow through the collision link list
	LIST_FOREACH(pum, &uri_ht[hvalue], entries)
	{
		if(pum->uri_size != uri_size) continue;
		if(strcmp(pum->uri, uri)==0) {
			pthread_mutex_unlock(&um_mutex);
			return (uint64_t)pum;
		}
	}

	pthread_mutex_unlock(&um_mutex);
	return 0;
}


/*
 * This function should be called to decrease the counter if
 * urimgr_add_uri() is called.
 */

void urimgr_del_uri(uint64_t uri_id)
{
	uri_mgr_t * pum;

	assert(uri_id != 0);
	pum=(uri_mgr_t *)uri_id;
	assert(pum->magic == MAGIC_SIG);

	pthread_mutex_lock(&um_mutex);

	pum->rfc--;
	if(pum->rfc==0) {
		LIST_REMOVE(pum, entries);
		free(pum->uri);
		free(pum);
		glob_tot_uri_in_urimgr--;
	}

	pthread_mutex_unlock(&um_mutex);
}


/*
 * This is debug function, it will only be called for debug purpose.
 */
void urimgr_display(void)
{
	int i;
	int tot=0;
	uri_mgr_t * pum;

	for(i=0;i<URI_HT_SIZE;i++) {
		LIST_FOREACH(pum, &uri_ht[i], entries)
		{
			printf("hashid=%d size=%d rfc=%d uri=%s\n",
				i, pum->uri_size, pum->rfc, pum->uri );
			tot++;
		}
	}
	printf("tot=%d\n\n", tot);
}


#ifdef URIMGR_DEBUG
/*
 * The following code is used for unit testing purpose.
 * Compilation command is:
 *	gcc -DURIMGR_DEBUG -o urimgr -g uri_mgr.c
 * Command to run is:
 *	./urimgr
 */

int main(int argc, void * argv[])
{
	uint64_t uri_id[20];
	int i;

	uri_id[0]=urimgr_add_uri("/");
	uri_id[1]=urimgr_add_uri("/1234567890123456789012345678901234567890");
	uri_id[2]=urimgr_add_uri("/12345678/12345678/12345678");
	uri_id[3]=urimgr_add_uri("/12345678/12345678/12345678");
	uri_id[4]=urimgr_add_uri("/12345678/12345678/12345678");
	uri_id[5]=urimgr_add_uri("/12345678/12345678/12345678");
	uri_id[6]=urimgr_add_uri("/");
	uri_id[7]=urimgr_add_uri("");
	uri_id[8]=urimgr_add_uri("/90239032ajkhhdaj/dshgjsdhjksweuiwe/weuiweuweyuyuweuywe/weuywu");
	uri_id[9]=urimgr_add_uri("/");
	uri_id[10]=urimgr_add_uri("/12345678/12345678/12345678");
	uri_id[11]=urimgr_add_uri("/12345678/12345678/%3e%4f?abcd=11");
	uri_id[12]=urimgr_add_uri("/12345678/12345678/%3e%3G?abcd=11");
	uri_id[13]=urimgr_add_uri("/12345678/12345678/%3e%3a?abcd=11");
	uri_id[14]=urimgr_add_uri("/%20%20%3e%3f%3a");

	urimgr_display();

	urimgr_del_uri(uri_id[0]);
	urimgr_del_uri(uri_id[1]);
	urimgr_del_uri(uri_id[9]);

	urimgr_display();

	printf("Segmentation fault is expected after this line\n");
	urimgr_del_uri(89898);

	return 1;
}

#endif // URIMGR_DEBUG
