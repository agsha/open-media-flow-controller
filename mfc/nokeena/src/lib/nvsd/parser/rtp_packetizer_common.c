#include <stdio.h>
#include <stdlib.h>

#include "rtp_packetizer_common.h"

char base64_t[] = {
	'A','B','C','D','E','F','G','H','I','J',
        'K','L','M','N','O','P','Q','R','S','T',
        'U','V','W','X','Y','Z',
        'a','b','c','d','e','f','g','h','i','j',
        'k','l','m','n','o','p','q','r','s','t',
        'u','v','w','x','y','z',
        '0','1','2','3','4','5','6','7','8','9',
	'+','/'	
};

void 
bin2base64(uint8_t* data,uint8_t num_bytes,char *out)
{
    int32_t bytes_left = num_bytes;
    uint8_t word,word1,word2,index;
    while(bytes_left > 0){
	if(bytes_left>=3){
	    word =  *data++;
	    word1 = *data++;
	    word2 = *data++;
	    *out++ = base64_t[word>>2];
	    index = ((word&0x03)<<4)|(word1>>4);
	    *out++ = base64_t[index];
	    index = ((word1&0x0F)<<2)|(word2>>6);
            *out++ = base64_t[index];
	    index = word2&0x3F;
            *out++ = base64_t[index];
	    bytes_left -=3;
	}
	else{
	    switch(bytes_left){
		case 1:
		    word = *data;
		    *out++ = base64_t[word>>2];
		    *out = base64_t[(word&0x03)<<4];
		    bytes_left = 0;
		    break;
		case 2:
		    word = *data++;
                    *out++ = base64_t[word>>2];
                    word&=0x03;
		    word1 = *data;
		    word = (word<<4)|(word1>>4);
		    *out++ = base64_t[word];
		    word = word1&0x0F;
                    *out++ = base64_t[word<<2];
                    bytes_left = 0;
		    break;
		default:
		    break;
	    }

	}

    }
}
