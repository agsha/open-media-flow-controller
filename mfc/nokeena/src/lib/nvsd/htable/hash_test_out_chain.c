/* unit test case -- obtained from glib test directory */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "nkn_hash.h"
#include <assert.h>

struct ar {
   int value;
   NKN_CHAIN_ENTRY a;
} array[10000];


static uint64_t
my_hash (const void *key)
{
  return (uint64_t) *((const int*) key);
}
static void
fill_hash_table_and_array (NCHashTable *hash_table)
{
  int i;
  uint64_t keyhash;

  for (i = 0; i < 10000; i++)
    {
      array[i].value = i;
      keyhash = my_hash(&array[i].value);
      nchash_insert(hash_table, keyhash, &array[i].value, &array[i]);
      //g_hash_table_insert (hash_table, &array[i], &array[i]);
    }
}

static void
init_result_array (struct ar  result_array[10000])
{
  int i;

  for (i = 0; i < 10000; i++)
    result_array[i].value = -1;
}

static void
verify_result_array (struct ar  array[10000])
{
  int i;

  for (i = 0; i < 10000; i++)
    assert (array[i].value == i);
}

static void
handle_pair (void *key, void *value, struct ar result_array[10000])
{
  int n;

  assert (key == &((struct ar *) value)->value);

  n = (((struct ar *) value)->value);

  assert (n >= 0 && n < 10000);
  assert (result_array[n].value == -1);

  result_array[n].value = n;
}

static int
my_hash_callback_remove (void *key,
                   void *value,
                   void *user_data)
{
  int *d = &(((struct ar *) value)->value);

  if ((*d) % 2)
    return TRUE;

  return FALSE;
}

static int
my_hash_callback_remove_rem (void *key,
                   void *value,
                   void *user_data)
{
  int *d = &(((struct ar *) value)->value);

    return TRUE;

}


static void
my_hash_callback_remove_test (void *key,
                        void *value,
                        void *user_data)
{
  int *d = &(((struct ar *) value)->value);

  if ((*d) % 2)
    assert (0) ;
}

static void
my_hash_callback (void *key,
              void *value,
              void *user_data)
{
  handle_pair (key, value, user_data);
}



static int
my_hash_equal (const void * a,
             const void * b)
{
  return *((const int*) a) == *((const int*) b);
}



/*
 * This is a simplified version of the pathalias hashing function.
 * Thanks to Steve Belovin and Peter Honeyman
 *
 * hash a string into a long int.  31 bit crc (from andrew appel).
 * the crc table is computed at run time by crcinit() -- we could
 * precompute, but it takes 1 clock tick on a 750.
 *
 * This fast table calculation works only if POLY is a prime polynomial
 * in the field of integers modulo 2.  Since the coefficients of a
 * 32-bit polynomial won't fit in a 32-bit word, the high-order bit is
 * implicit.  IT MUST ALSO BE THE CASE that the coefficients of orders
 * 31 down to 25 are zero.  Happily, we have candidates, from
 * E. J.  Watson, "Primitive Polynomials (Mod 2)", Math. Comp. 16 (1962):
 *    x^32 + x^7 + x^5 + x^3 + x^2 + x^1 + x^0
 *    x^31 + x^3 + x^0
 *
 * We reverse the bits to get:
 *    111101010000000000000000000000001 but drop the last 1
 *         f   5   0   0   0   0   0   0
 *    010010000000000000000000000000001 ditto, for 31-bit crc
 *       4   8   0   0   0   0   0   0
 */

#define POLY 0x48000000L      /* 31-bit polynomial (avoids sign problems) */

static unsigned int CrcTable[128];

/*
 - crcinit - initialize tables for hash function
 */
static void crcinit(void)
{
      int i, j;
      unsigned int sum;

      for (i = 0; i < 128; ++i) {
            sum = 0L;
            for (j = 7 - 1; j >= 0; --j)
                  if (i & (1 << j))
                        sum ^= POLY >> j;
            CrcTable[i] = sum;
      }
}

/*
 - hash - Honeyman's nice hashing function
 */
static uint64_t honeyman_hash(const void * key)
{
      const char *name = (const char *) key;
      int size;
      unsigned int sum = 0;

      assert (name != NULL);
      assert (*name != 0);

      size = strlen(name);

      while (size--) {
            sum = (sum >> 7) ^ CrcTable[(sum ^ (*name++)) & 0x7f];
      }

      return(sum);
}


static int second_hash_cmp (const void * a,const void * b)
{
  return (strcmp (a, b) == 0);
}



static uint64_t one_hash(const void * key)
{
  return 1;
}
struct ar1 {
    char value[200];
    NKN_CHAIN_ENTRY __entry;
};


static void not_even_foreach (void *       key,
                         void *       value,
                         void *   user_data)
{
  const char *_key = (const char *) key;
  const char *_value = ((struct ar1 *) value)->value;
  int i;
  char val [20];

  assert (_key != NULL);
  assert (*_key != 0);
  assert (_value != NULL);
  assert (*_value != 0);

  i = atoi (_key);

  sprintf (val, "%d value", i);
  assert (strcmp (_value, val) == 0);

  assert ((i % 2) != 0);
  assert (i != 3);
}

int return_success(void *key, void *value, void * userdata)
{
    return 1;
}


int remove_even_foreach (void *       key,
                         void *       value,
                         void *   user_data);
int remove_even_foreach (void *       key,
                         void *       value,
                         void *   user_data)
{
  const char *_key = (const char *) key;
  const char *_value = ((struct ar1 *) value)->value;
  int i;
  char val [20];

  assert (_key != NULL);
  assert (*_key != 0);
  assert (_value != NULL);
  assert (*_value != 0);

  i = atoi (_key);

  sprintf (val, "%d value", i);
  assert (strcmp (_value, val) == 0);

  return ((i % 2) == 0) ? TRUE : FALSE;
}



static void second_hash_test (int simple_hash)
{
     int       i;
     char      key[20] = "", *orig_key;
     void *key_val[20];
     struct ar1 value[20];
     struct ar1 val, *orig_val, *v, *l;
     NCHashTable	*h;
     int found;
     uint64_t  hash;

     crcinit ();
     h = nchash_table_new (simple_hash ? one_hash : honeyman_hash,
                          second_hash_cmp, 0, NKN_CHAINFIELD_OFFSET(struct ar1, __entry));
     assert (h != NULL);
     for (i=0; i<20; i++)
          {
          sprintf (key, "%d", i);
        assert (atoi (key) == i);

        sprintf (value[i].value, "%d value", i);
        assert (atoi (value[i].value) == i);
	  hash = simple_hash? one_hash(key):
			honeyman_hash(key);
		key_val[i] = strdup(key);
           nchash_insert(h, hash, key_val[i], &value[i]);
	}
     assert (nchash_size (h) == 20);

     for (i=0; i<20; i++)
          {
          sprintf (key, "%d", i);
        assert (atoi(key) == i);
	  hash = simple_hash? one_hash(key):
			honeyman_hash(key);

          v =  nchash_lookup (h, hash, key);

        assert (v != NULL);
        assert (*v->value != 0);
        assert (atoi (v->value) == i);
          }

     sprintf (key, "%d", 3);
     hash = simple_hash? one_hash(key):
                        honeyman_hash(key);
     nchash_remove(h, hash, key);
     assert (nchash_size (h) == 19);
     void *key1;
     while (l = nchash_find_match (h, remove_even_foreach, NULL, &key1)) {
     hash = simple_hash? one_hash(key1):
                        honeyman_hash(key1);
	nchash_remove(h, hash, key1);
     }
     assert (nchash_size (h) == 9);
     nchash_foreach(h, not_even_foreach, NULL);

     for (i=0; i<20; i++)
          {
          sprintf (key, "%d", i);
        assert (atoi(key) == i);

        sprintf (val.value, "%d value", i);
        assert (atoi (val.value) == i);

        orig_key = NULL;
             orig_val=  NULL;
	  hash = simple_hash? one_hash(key):
				honeyman_hash(key);
          found = nchash_lookup_extended (h, hash, key,
                                    (void *)&orig_key,
                                    (void *)&orig_val);
        if ((i % 2) == 0 || i == 3)
            {
              assert (!found);
            continue;
            }

        assert (found);

        assert (orig_key != NULL);
        assert (strcmp (key, orig_key) == 0);

        assert (orig_val != NULL);
        assert (strcmp (val.value, orig_val->value) == 0);
          }

     while (l = nchash_find_match (h, return_success, NULL, &key1)) {
	  hash = simple_hash? one_hash(key1):
				honeyman_hash(key1);
	
	nchash_remove(h, hash, key1);
     }

	for (i =0; i < 20 ; ++i) {
		free(key_val[i]);
	}

    nchash_destroy(h);
}

static int find_first     (void * key, 
                        void * value, 
                        void * user_data)
{
  int v = ((struct ar *)value)->value; 
  int test = ((struct ar *)user_data)->value;
  return (v == test);
}

#if 0
static void direct_hash_test (void)
{
     int       i, rc;
     GHashTable     *h;

     h = g_hash_table_new (NULL, NULL);
     assert (h != NULL);
     for (i=1; i<=20; i++)
          {
          g_hash_table_insert (h, GINT_TO_POINTER (i),
                         GINT_TO_POINTER (i + 42));
          }

     assert (g_hash_table_size (h) == 20);

     for (i=1; i<=20; i++)
          {
          rc = GPOINTER_TO_INT (
            g_hash_table_lookup (h, GINT_TO_POINTER (i)));

        assert (rc != 0);
        assert ((rc - 42) == i);
          }

    g_hash_table_destroy (h);
}
#endif


int
main (int   argc,
      char *argv[])
{
  NCHashTable *hash_table;
  int i;
  struct ar value;
  value.value = 120;
  struct ar *pvalue;
  void *ikey, *ivalue;
  struct ar result_array[10000];
  struct ar *l;
  uint64_t hash;

  hash_table = nchash_table_new (my_hash, my_hash_equal, 2000, NKN_CHAINFIELD_OFFSET(struct ar, a));
  fill_hash_table_and_array (hash_table);
  void *key;
  pvalue = nchash_find_match (hash_table, find_first, &value, &key);
  if (!pvalue || pvalue->value != value.value)
    assert (0);
  key = NULL;
#if 0
  keys = g_hash_table_get_keys (hash_table);
  if (!keys)
    assert (0) ;

  values = g_hash_table_get_values (hash_table);
  if (!values)
    assert (0) ;

  keys_len = g_list_length (keys);
  values_len = g_list_length (values);
  if (values_len != keys_len &&  keys_len != g_hash_table_size (hash_table))
    assert (0) ;

  g_list_free (keys);
  g_list_free (values);
#endif
   if (nchash_size (hash_table) != 10000)
	assert (0) ;

//  fill_hash_table_and_array (hash_table);

  init_result_array (result_array);
  nchash_foreach (hash_table, my_hash_callback, result_array);
  verify_result_array (result_array);

  for (i = 0; i < 10000; i++) {
    hash = my_hash(&array[i].value);
    nchash_remove (hash_table, hash, &array[i].value);
  }
if (nchash_size (hash_table) != 0)   assert(0); 

  fill_hash_table_and_array (hash_table);

  while(l = nchash_find_match(hash_table, my_hash_callback_remove, NULL, &key)) {
	hash = my_hash(key);
	nchash_remove(hash_table, hash, key);
	key = NULL;
  }
  if (nchash_size (hash_table) != 5000)
    assert (0);

  nchash_foreach (hash_table, my_hash_callback_remove_test, NULL);

  while(l = nchash_find_match(hash_table, my_hash_callback_remove_rem, NULL, &key)) {
	hash = my_hash(key);
	nchash_remove(hash_table, hash, key);
	key = NULL;
  }

  nchash_destroy (hash_table);

  second_hash_test (TRUE);
  second_hash_test (FALSE);
  //direct_hash_test ();
  printf("glib hash test: test passed --- hit no assert\n");
  return 0;

}


