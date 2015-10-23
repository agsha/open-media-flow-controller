#ifndef __NKN__ELF__H
#define __NKN__ELF__H

/*
 *
 * This function uses ELF technology to replace system call by our function.
 *
 * Input:
 *    psysapi: the address of any system API call.
 *    pnknapi: new API function address which matches related system API definition.
 * Return:
 *    If replacement is successful, returns original system API address.
 *    If replacement is failed, returns NULL.
 *
 * Usage Example:
 *    Here is one example on how to replace system API malloc().
 *    After nkn_replace_func() is called successfully, 
 *    any code in anywhere calling malloc() will automatically convert to nkn_malloc().
 *
 *
 *    typedef void * (*FUNC_MALLOC)(size_t size);
 *    FUNC_MALLOC psys_malloc;
 *
 *    uint64_t glob_numof_malloc_called;
 *    void * mymalloc(size_t size)
 *    {
 *        glob_numof_malloc_called ++;
 *	  return (*psys_malloc)(size);
 *    }
 *
 *    psys_malloc = (FUNC_MALLOC)nkn_replace_func("malloc", (void *)nkn_malloc);
 *
 */

void * nkn_replace_func(const char * api_name, void * pnknapi);

/*
 * The function to get address of a symbol
 */
int nkn_get_symbol_addr(char * symbolname, void ** ptr, int * size);
char * st_lookup_by_addr(uint64_t curaddr, uint64_t * funcaddr);

#endif // __NKN__ELF__H
