/*
 * uf_utils.h -- URL Filter utilities
 */
#ifndef _UF_UTILS_H_
#define _UF_UTILS_H_

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CompressURLSlashes() -- Remove redundant slashes in URL abs path
 *   Returns:
 *	=0 - Not modified
 *	>0 - Modified (actually, number of removed '/'(s))
 *
 *   Examples:
 *      http://xyz.com/a///////b.html ==> http://xyz.com/a/b.html
 *      /a///////b.html ==> /a/b.html
 *      //a///////b.html ==> /a/b.html
 */
static inline int 
CompressURLSlashes(char *buf, int *buf_bytes /* less NULL char */ )
{
    char *pbuf = buf;
    char *pcur;
    int bytes = *buf_bytes;
    int modified = 0;

    while (1) {
    	pcur = strstr(pbuf, "//");
	if (!pcur) {
	    break; /* done */
	} else if ((pcur[-1] == ':') && 
	   (( ((pcur-5) >= buf) &&
	      ((pcur[-2] == 'p') || (pcur[-2] == 'P')) &&
	      ((pcur[-3] == 't') || (pcur[-3] == 'T')) &&
	      ((pcur[-4] == 't') || (pcur[-4] == 'T')) &&
	      ((pcur[-5] == 'h') || (pcur[-5] == 'H')) ) 
	   ||
	    ( ((pcur-6) >= buf) &&
	      ((pcur[-2] == 's') || (pcur[-2] == 'S')) &&
	      ((pcur[-3] == 'p') || (pcur[-3] == 'P')) &&
	      ((pcur[-4] == 't') || (pcur[-4] == 'T')) &&
	      ((pcur[-5] == 't') || (pcur[-5] == 'T')) &&
	      ((pcur[-6] == 'h') || (pcur[-6] == 'H')) ) ) ) { 
	    // Legal case "http://" or "https://", ignore it
	    pbuf += 2;
	} else {
	    /* Remove slash */
	    modified++;
	    memmove(pcur+1, pcur+2, bytes-(pcur-buf+1));
	    pbuf = pcur;
	    bytes--;
	}
    }
    *buf_bytes = bytes;
    return modified;
}

#ifdef __cplusplus
}
#endif

#endif /* _UF_UTILS_H_ */

/*
 * End of uf_utils.h
 */
