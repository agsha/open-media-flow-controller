#include  "mfp_kms_client.h"
#include  "mfp_kms_key_store.h"
#include <stdlib.h>

int32_t main() {

	kms_client_t* kms_cl = createNativeKMSClient(1, 10, 
			(uint8_t*)"./",
			(uint8_t*)"http://test.com/key/");
	if (kms_cl == NULL) {
		printf("Native KMS client init failed\n");
		return -1;
	}
	kms_client_key_store_t* key_store = createKMS_ClientKeyStore(kms_cl, 3,
			"sess_name", 1);
	if (key_store == NULL) {
		printf("KMS Key Store init failed\n");
		return -1;
	}
	int32_t i = 1, j = 0;
	while (i < 12) {
		kms_key_resp_t* k_resp = key_store->get_kms_key(key_store, i);
		if (k_resp != NULL) {
			printf("i: %d Key URI: %s\n", i, k_resp->key_uri);
			for (j = 0; j < 16; j++)
				printf("%02X", k_resp->key[j]);
			key_store->del_kms_key(key_store, i);
			printf("\n");
		}
		i = i + 2;
	}

	i = 2;
	while (i < 12) {
		kms_key_resp_t* k_resp = key_store->get_kms_key(key_store, i);
		if (k_resp != NULL) {
			printf("i: %d Key URI: %s\n", i, k_resp->key_uri);
			for (j = 0; j < 16; j++)
				printf("%02X", k_resp->key[j]);
			printf("\n");
			key_store->del_kms_key(key_store, i);
		}
		i = i + 2;
	}

	key_store->delete_ctx(key_store);
	return 0;
}

