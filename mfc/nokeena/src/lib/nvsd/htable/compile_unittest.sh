rm -rf nkn_chainhash.o nkn_hash.o libnknhash.a
gcc  -c -I ../../../include/ -DHASH_UNITTEST nkn_hash.c -o nkn_hash.o
gcc  -c -I ../../../include/ -DHASH_UNITTEST nkn_chainhash.c -o nkn_chainhash.o
ar rcs libnknhash.a nkn_hash.o nkn_chainhash.o
gcc  -I ../../../include/ -DHASH_UNITTEST -o hash_test hash_test_out_chain.c libnknhash.a

