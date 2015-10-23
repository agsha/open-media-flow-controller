#include  "mfp_kms_client.h"
#include <stdlib.h>

int32_t main() {

	kms_client_t* kms_cl = createNativeKMSClient(1, 10, 
			(uint8_t*)"./",
			(uint8_t*)"http://test.com/key/");
	if (kms_cl == NULL) {
		printf("Verimatrix KMS client init failed\n");
		return -1;
	}
	int32_t i = 10, j = 0;
	while (i > 0) {
		kms_key_resp_t* k_resp = kms_cl->get_next_key(kms_cl);
		if (k_resp != NULL) {
			printf("Key URI: %s\n", k_resp->key_uri);
			for (j = 0; j < 16; j++)
				printf("%02X", k_resp->key[j]);
			printf("\n");
			free(k_resp->key_uri);
			free(k_resp->key);
			free(k_resp);
		}
		i--;
	}
	kms_cl->delete_client(kms_cl);
	return 0;
}

