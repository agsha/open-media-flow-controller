#include  "mfp_kms_client.h"
#include <stdlib.h>

int32_t main() {

	/*
	kms_client_t* kms_cl = createVerimatrixKMSClient("74.62.179.10",
			12684, 22, 1000, 4, 0, NULL, NULL);
	*/
	kms_client_t* kms_cl = createVerimatrixKMSClient("74.62.179.10",
			12684, 22, 1000, KMS_VOD, (uint8_t*)"./",
			(uint8_t*)"http://10.157.42.200/mfp/jegadish/");
	if (kms_cl == NULL) {
		printf("Verimatrix KMS client init failed\n");
		return -1;
	}
	int32_t i = 10;
	while (i > 0) {
		kms_key_resp_t* k_resp = kms_cl->get_next_key(kms_cl);
		if (k_resp != NULL) {
			printf("Key URI: %s\n", k_resp->key_uri);
			free(k_resp->key_uri);
			free(k_resp->key);
			free(k_resp);
		}
		i--;
	}
	kms_cl->delete_client(kms_cl);

	return 0;
}

