/* unit test case -- obtained from glib test directory */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "nkn_hash.h"
#include <assert.h>



int array[10000];
static uint64_t
my_hash (const void *key)
{
  return (uint64_t) *((const int*) key);
}
static void
fill_hash_table_and_array (NHashTable *hash_table)
{
  int i;
  uint64_t keyhash;

  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      keyhash = my_hash(&array[i]);
      nhash_insert(hash_table, keyhash, &array[i], &array[i]);
      //g_hash_table_insert (hash_table, &array[i], &array[i]);
    }
}

static void
init_result_array (int result_array[10000])
{
  int i;

  for (i = 0; i < 10000; i++)
    result_array[i] = -1;
}

static void
verify_result_array (int array[10000])
{
  int i;

  for (i = 0; i < 10000; i++)
    assert (array[i] == i);
}

static void
handle_pair (void *key, void *value, int result_array[10000])
{
  int n;

  assert (key == value);

  n = *((int *) value);

  assert (n >= 0 && n < 10000);
  assert (result_array[n] == -1);

  result_array[n] = n;
}

static int
my_hash_callback_remove (void *key,
                   void *value,
                   void *user_data)
{
  int *d = value;

  if ((*d) % 2)
    return TRUE;

  return FALSE;
}

static void
my_hash_callback_remove_test (void *key,
                        void *value,
                        void *user_data)
{
  int *d = value;

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


static void not_even_foreach (void *       key,
                         void *       value,
                         void *   user_data)
{
  const char *_key = (const char *) key;
  const char *_value = (const char *) value;
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


static int remove_even_foreach (void *       key,
                         void *       value,
                         void *   user_data)
{
  const char *_key = (const char *) key;
  const char *_value = (const char *) value;
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
     char      key[20] = "", val[20]="", *v, *orig_key, *orig_val;
     void *key_val[20];
     void *value[20];
     NHashTable	*h;
     int found;
     uint64_t  hash;

     crcinit ();
     h = nhash_table_new (simple_hash ? one_hash : honeyman_hash,
                          second_hash_cmp, 0);
     assert (h != NULL);
     for (i=0; i<20; i++)
          {
          sprintf (key, "%d", i);
        assert (atoi (key) == i);

        sprintf (val, "%d value", i);
        assert (atoi (val) == i);
	  hash = simple_hash? one_hash(key):
			honeyman_hash(key);
		key_val[i] = strdup(key);
		value[i] = strdup(val);
           nhash_insert(h, hash, key_val[i], value[i]);
          }

     assert (nhash_size (h) == 20);

     for (i=0; i<20; i++)
          {
          sprintf (key, "%d", i);
        assert (atoi(key) == i);
	  hash = simple_hash? one_hash(key):
			honeyman_hash(key);

          v = (char *) nhash_lookup (h, hash, key);

        assert (v != NULL);
        assert (*v != 0);
        assert (atoi (v) == i);
          }

     sprintf (key, "%d", 3);
     hash = simple_hash? one_hash(key):
                        honeyman_hash(key);
     nhash_remove(h, hash, key);
     assert (nhash_size (h) == 19);
     nhash_foreach_remove (h, remove_even_foreach, NULL);
     assert (nhash_size (h) == 9);
     nhash_foreach (h, not_even_foreach, NULL);
     for (i=0; i<20; i++)
          {
          sprintf (key, "%d", i);
        assert (atoi(key) == i);

        sprintf (val, "%d value", i);
        assert (atoi (val) == i);

        orig_key = orig_val = NULL;
	  hash = simple_hash? one_hash(key):
				honeyman_hash(key);
          found = nhash_lookup_extented (h, hash, key,
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
        assert (strcmp (val, orig_val) == 0);
          }

	for (i =0; i < 20 ; ++i) {
		free(key_val[i]);
		free(value[i]);
	}

    nhash_destroy(h);
}

static int find_first     (void * key, 
                        void * value, 
                        void * user_data)
{
  int *v = value; 
  int *test = user_data;
  return (*v == *test);
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
  NHashTable *hash_table;
  int i;
  int value = 120;
  int *pvalue;
  NHTableIter iter;
  void *ikey, *ivalue;
  int result_array[10000];
  uint64_t hash;

  hash_table = nhash_table_new (my_hash, my_hash_equal, 0);
  fill_hash_table_and_array (hash_table);
  pvalue = nhash_find (hash_table, find_first, &value);
  if (!pvalue || *pvalue != value)
    assert (0);

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
   if (nhash_size (hash_table) != 10000)
	assert (0) ;
  init_result_array (result_array);
  nhash_iter_init (&iter, hash_table);
  for (i = 0; i < 10000; i++)
    {
      assert (nhash_iter_next (&iter, &ikey, &ivalue));

      handle_pair (ikey, ivalue, result_array);

      if (i % 2)
      nhash_iter_remove(&iter);
    }
  assert (! nhash_iter_next(&iter, &ikey, &ivalue));
  assert (nhash_size (hash_table) == 5000);
  verify_result_array (result_array);

  fill_hash_table_and_array (hash_table);

  init_result_array (result_array);
  nhash_foreach (hash_table, my_hash_callback, result_array);
  verify_result_array (result_array);

  for (i = 0; i < 10000; i++) {
    hash = my_hash(&array[i]);
    nhash_remove (hash_table, hash, &array[i]);
  }

  fill_hash_table_and_array (hash_table);

  if (nhash_foreach_remove (hash_table, my_hash_callback_remove, NULL) != 5000 ||
      nhash_size (hash_table) != 5000)
    assert (0);

  nhash_foreach (hash_table, my_hash_callback_remove_test, NULL);


  nhash_destroy (hash_table);

  second_hash_test (TRUE);
  second_hash_test (FALSE);
  //direct_hash_test ();
  printf("glib hash test: test passed --- hit no assert\n");
  return 0;

}


