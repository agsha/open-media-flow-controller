/*
 * cb_router.c -- Content Broker (CB) route management.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdarg.h>
#include <netdb.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ptrie/persistent_trie.h"
#include "proto_http/proto_http.h"
#include "cb/xml/OCRP_XMLops.h"
#include "cb/OCRP_cgi_params.h"
#include "cb/OCRP_post_cgi_client.h"
#include "cb/content_broker.h"
#include "cb_log.h"
#include "cb_malloc.h"

#define UNUSED_ARGUMENT(x) (void)x
#define TSTR(_p, _l, _r) char *(_r); \
        ( 1 ? ( (_r) = alloca((_l)+1), \
	    memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )


/*
 * Static data
 */
static ptrie_context_t *cb_ptrie_ctx;
static pthread_t ptrie_update_thread_id;
static pthread_t get_asset_map_server_thread_id;

static char MAP2UPPER[128] = {
    0,				// 000 000 00000000 NUL (Null char.)
    1,				// 001 001 00000001 SOH (Start of Header)
    2,				// 002 002 00000010 STX (Start of Text)
    3,				// 003 003 00000011 ETX (End of Text)
    4,				// 004 004 00000100 EOT (End of Transmission)
    5,				// 005 005 00000101 ENQ (Enquiry)
    6,				// 006 006 00000110 ACK (Acknowledgment)
    7,				// 007 007 00000111 BEL (Bell)
    8,				// 010 008 00001000 BS (Backspace)
    9,				// 011 009 00001001 HT (Horizontal Tab)
    10,				// 012 00A 00001010 LF (Line Feed)
    11,				// 013 00B 00001011 VT (Vertical Tab)
    12,				// 014 00C 00001100 FF (Form Feed)
    13,				// 015 00D 00001101 CR (Carriage Return)
    14,				// 016 00E 00001110 SO (Shift Out)
    15,				// 017 00F 00001111 SI (Shift In)
    16,				// 020 010 00010000 DLE (Data Link Escape)
    17,				// 021 011 00010001 DC1 (XON) (Device Control 1)
    18,				// 022 012 00010010 DC2 (Device Control 2)
    19,				// 023 013 00010011 DC3 (XOFF)(Device Control 3)
    20,				// 024 014 00010100 DC4 (Device Control 4)
    21,				// 025 015 00010101 NAK (Negativ Acknowledgemnt)
    22,				// 026 016 00010110 SYN (Synchronous Idle)
    23,				// 027 017 00010111 ETB (End of Trans. Block)
    24,				// 030 018 00011000 CAN (Cancel)
    25,				// 031 019 00011001 EM (End of Medium)
    26,				// 032 01A 00011010 SUB (Substitute)
    27,				// 033 01B 00011011 ESC (Escape)
    28,				// 034 01C 00011100 FS (File Separator)
    29,				// 035 01D 00011101 GS (Group Separator)
    30,				// 036 01E 00011110 RS (Reqst to Send)(Rec.Sep.)
    31,				// 037 01F 00011111 US (Unit Separator)
    32,				// 040 020 00100000 SP (Space)
    33,				// 041 021 00100001 ! (exclamation mark)
    34,				// 042 022 00100010 " (double quote)
    35,				// 043 023 00100011 # (number sign)
    36,				// 044 024 00100100 $ (dollar sign)
    37,				// 045 025 00100101 % (percent)
    38,				// 046 026 00100110 & (ampersand)
    39,				// 047 027 00100111 ' (single quote)
    40,				// 050 028 00101000 ( (left/open parenthesis)
    41,				// 051 029 00101001 ) (right/closing parenth.)
    42,				// 052 02A 00101010 * (asterisk)
    43,				// 053 02B 00101011 + (plus)
    44,				// 054 02C 00101100 , (comma)
    45,				// 055 02D 00101101 - (minus or dash)
    46,				// 056 02E 00101110 .  (dot)
    47,				// 057 02F 00101111 / (forward slash)
    48,				// 060 030 00110000 0
    49,				// 061 031 00110001 1
    50,				// 062 032 00110010 2
    51,				// 063 033 00110011 3
    52,				// 064 034 00110100 4
    53,				// 065 035 00110101 5
    54,				// 066 036 00110110 6
    55,				// 067 037 00110111 7
    56,				// 070 038 00111000 8
    57,				// 071 039 00111001 9
    58,				// 072 03A 00111010 : (colon)
    59,				// 073 03B 00111011 ; (semi-colon)
    60,				// 074 03C 00111100 < (less than)
    61,				// 075 03D 00111101 = (equal sign)
    62,				// 076 03E 00111110 > (greater than)
    63,				// 077 03F 00111111 ? (question mark)
    64,				// 100 040 01000000 @ (AT symbol)
    65,				// 101 041 01000001 A
    66,				// 102 042 01000010 B
    67,				// 103 043 01000011 C
    68,				// 104 044 01000100 D
    69,				// 105 045 01000101 E
    70,				// 106 046 01000110 F
    71,				// 107 047 01000111 G
    72,				// 110 048 01001000 H
    73,				// 111 049 01001001 I
    74,				// 112 04A 01001010 J
    75,				// 113 04B 01001011 K
    76,				// 114 04C 01001100 L
    77,				// 115 04D 01001101 M
    78,				// 116 04E 01001110 N
    79,				// 117 04F 01001111 O
    80,				// 120 050 01010000 P
    81,				// 121 051 01010001 Q
    82,				// 122 052 01010010 R
    83,				// 123 053 01010011 S
    84,				// 124 054 01010100 T
    85,				// 125 055 01010101 U
    86,				// 126 056 01010110 V
    87,				// 127 057 01010111 W
    88,				// 130 058 01011000 X
    89,				// 131 059 01011001 Y
    90,				// 132 05A 01011010 Z
    91,				// 133 05B 01011011 [ (left/opening bracket)
    92,				// 134 05C 01011100 \ (back slash)
    93,				// 135 05D 01011101 ] (right/closing bracket)
    94,				// 136 05E 01011110 ^ (caret/circumflex)
    95,				// 137 05F 01011111 _ (underscore)
    96,				// 140 060 01100000 `
    65,				// 97, // 141 061 01100001 a
    66,				// 98, // 142 062 01100010 b
    67,				// 99, // 143 063 01100011 c
    68,				// 100, // 144 064 01100100 d
    69,				// 101, // 145 065 01100101 e
    70,				// 102, // 146 066 01100110 f
    71,				// 103, // 147 067 01100111 g
    72,				// 104, // 150 068 01101000 h
    73,				// 105, // 151 069 01101001 i
    74,				// 106, // 152 06A 01101010 j
    75,				// 107, // 153 06B 01101011 k
    76,				// 108, // 154 06C 01101100 l
    77,				// 109, // 155 06D 01101101 m
    78,				// 110, // 156 06E 01101110 n
    79,				// 111, // 157 06F 01101111 o
    80,				// 112, // 160 070 01110000 p
    81,				// 113, // 161 071 01110001 q
    82,				// 114, // 162 072 01110010 r
    83,				// 115, // 163 073 01110011 s
    84,				// 116, // 164 074 01110100 t
    85,				// 117, // 165 075 01110101 u
    86,				// 118, // 166 076 01110110 v
    87,				// 119, // 167 077 01110111 w
    88,				// 120, // 170 078 01111000 x
    89,				// 121, // 171 079 01111001 y
    90,				// 122, // 172 07A 01111010 z
    123,			// 173 07B 01111011 { (left/opening brace)
    124,			// 174 07C 01111100 | (vertical bar)
    125,			// 175 07D 01111101 } (right/closing brace)
    126,			// 176 07E 01111110 ~ (tilde)
    127				// 177 07F 01111111 DEL (delete)
};

int http_resp_body_hdr_strlen; // Set at init
const char *http_resp_body_hdr =
	"<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>"
	"<!DOCTYPE OCRP SYSTEM \"OCRP.dtd\">"
	"\r\n<OCRP>\r\n";

int http_resp_body_trailer_strlen; // Set at init
const char *http_resp_body_trailer = "</OCRP>\r\n";

/*
 *******************************************************************************
 * Static ptrie callback functions
 *******************************************************************************
 */
static int 
ptrie_log(ptrie_log_level_t level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    return cb_log_var(SS_PTRIE, (cb_log_level)level, fmt, ap);
}

static void 
ptrie_copy_app_data(const app_data_t *src, app_data_t *dest)
{
    OCRP_app_data_t *psrc = (OCRP_app_data_t *) src;
    OCRP_app_data_t *pdest = (OCRP_app_data_t *) dest;

    switch(psrc->u.d.type) {
    case OCRP_AD_TYPE_RECORD:
    	pdest->u.d.type = OCRP_AD_TYPE_RECORD;
	pdest->u.d.u.rec =  psrc->u.d.u.rec;
	break;
    case OCRP_AD_TYPE_NAME:
    	pdest->u.d.type = OCRP_AD_TYPE_NAME;
    	pdest->u.d.u.name.nm_strlen = psrc->u.d.u.name.nm_strlen;
    	strcpy(pdest->u.d.u.name.nm, psrc->u.d.u.name.nm);
	break;
    case OCRP_AD_TYPE_NUM:
    	pdest->u.d.type = OCRP_AD_TYPE_NUM;
	pdest->u.d.u.num.d = psrc->u.d.u.num.d;
	break;
    default:
    	*pdest = *psrc;
	break;
    } // End switch

    return;
}

static void 
ptrie_destruct_app_data(app_data_t *d)
{
    UNUSED_ARGUMENT(d);
    // Free deep data
    return;
}

/*
 *******************************************************************************
 * OCRP support functions
 *******************************************************************************
 */
static int
make_H_record(uint64_t seqno, uint64_t version, char *buf, int sizeof_buf)
{
    int rv;

    rv = snprintf(buf, sizeof_buf, "<H>%ld:%ld</H>\r\n", seqno, version);
    if (rv < sizeof_buf) {
    	return 0;
    } else {
    	return 1;
    }
}

/*
 * OCRP ptrie mappings:
 *  1) URL absolute path to OCRP_app_data_t.u.d.u.rec
 *     URL absolute path always starts with '/'
 *  2) FQDN (upper case) to FQDN index (OCRP_app_data_t.u.d.u.num)
 *     Always preceded with "{FQDN2Index}/"
 *  3) FQDN index to FQDN (upper case) (OCRP_app_data.t.u.d.u.name)
 *     Always preceded with "{Index2FQDN}/"
 *  4) Last allocated FQDN index, which uses the fixed key "{FQDNLastIndex}",
 *     to FQDN index (OCRP_app_data_t.u.d.u.num)
 *
 *  Mappings 2), 3) and 4) are used to eliminate storing the FQDN for each
 *  URL absolute path mapping in the OCRP_app_data_t.u.d.u.rec
 */

/*
 * Fundamental OCRP ptrie definitions
 */
#define FQDN2IX_KEY_PREFIX "{FQDN2Index}/"
#define FQDN2IX_KEY_PREFIX_STRLEN 13

#define IX2FQDN_KEY_PREFIX "{Index2FQDN}/"
#define IX2FQDN_KEY_PREFIX_STRLEN 13

#define FQDN_LAST_INDEX_KEY "{FQDNLastIndex}"

static
void dump_OCRP_AD(char *buf, int sizeof_buf, OCRP_app_data_t *ocrp_ad)
{
    int cnt = 0;
    int rv;
    int n;

    switch(ocrp_ad->u.d.type) {
    case OCRP_AD_TYPE_RECORD:
        rv = snprintf(&buf[cnt], sizeof_buf-cnt, 
		      "OCRP_app_data Type: [Rec] flags=[0x%lx] "
		      "seqno=[%ld] vers=[%ld] num_loc=[%d] ",
		      ocrp_ad->u.d.u.rec.flags, ocrp_ad->u.d.u.rec.seqno,
		      ocrp_ad->u.d.u.rec.vers, ocrp_ad->u.d.u.rec.num_loc);
	if (rv >= (sizeof_buf-cnt)) {
	    buf[sizeof_buf-1] = '\0';
	    break;
	}
	cnt += rv;

	for (n = 0; n < ocrp_ad->u.d.u.rec.num_loc; n++) {
	    rv = snprintf(&buf[cnt], sizeof_buf-cnt, 
	    		  "loc[%d]: fqdn_ix=%d port=%hu weight=%hu ",
			  n, ocrp_ad->u.d.u.rec.loc[n].fqdn_ix,
			  ocrp_ad->u.d.u.rec.loc[n].port,
			  ocrp_ad->u.d.u.rec.loc[n].weight);
	    if (rv >= (sizeof_buf-cnt)) {
	    	buf[sizeof_buf-1] = '\0';
		break;
	    }
	    cnt += rv;
	}
	break;

    case OCRP_AD_TYPE_NAME:
        rv = snprintf(&buf[cnt], sizeof_buf-cnt, 
		       "OCRP_app_data Type: [Name] nm_strlen=[%d] nm=[%s]",
		       ocrp_ad->u.d.u.name.nm_strlen, ocrp_ad->u.d.u.name.nm);
	if (rv >= (sizeof_buf-cnt)) {
	    buf[sizeof_buf-1] = '\0';
	}
	cnt += rv;
	break;

    case OCRP_AD_TYPE_NUM:
        rv = snprintf(&buf[cnt], sizeof_buf-cnt, 
		       "OCRP_app_data Type: [Num] d=[%ld]", 
		       ocrp_ad->u.d.u.num.d);
	if (rv >= (sizeof_buf-cnt)) {
	    buf[sizeof_buf-1] = '\0';
	}
	cnt += rv;
	break;

    default:
        rv = snprintf(&buf[cnt], sizeof_buf-cnt, 
		      "OCRP_app_data Type: [Unknown]");
	if (rv >= (sizeof_buf-cnt)) {
	    buf[sizeof_buf-1] = '\0';
	}
	cnt += rv;
    	break;
    }
}

/*
 * add_FQDN_to_index() - Add entry to "{FQDN2Index}" ptrie.
 *  Add entry to ptrie "{FQDN2Index}/FQDN" where FQDN is mapped to upper case
 *
 *  fqdn - Max strlen of L_ELEMENT_FQDN_MAXLEN, where all alphas are upper case
 *
 *  Return:
 *    == 0, Success
 *     < 0, Commit xaction and retry
 *     > 0, Error
 */
static int
add_FQDN_to_index(ptrie_context_t *pctx, const char *fqdn, int64_t ix)
{
    char tbuf[2048];
    char ptrie_key[FQDN2IX_KEY_PREFIX_STRLEN + L_ELEMENT_FQDN_MAXLEN + 1];
    OCRP_app_data_t apd;
    int rv;

    if (!fqdn || !*fqdn) {
    	return 1;
    }

    memcpy(ptrie_key, FQDN2IX_KEY_PREFIX, FQDN2IX_KEY_PREFIX_STRLEN);
    strcpy(&ptrie_key[FQDN2IX_KEY_PREFIX_STRLEN], fqdn);

    memset(&apd, 0, sizeof(apd));
    apd.u.d.type = OCRP_AD_TYPE_NUM;
    apd.u.d.u.num.d = ix;

    rv = ptrie_add(pctx, ptrie_key, &apd.u.apd);
    if (!rv) {
	dump_OCRP_AD(tbuf, sizeof(tbuf), &apd);
	CB_LOG(CB_MSG, "Add key=%s ctx=%p app_data=%s", ptrie_key, pctx, tbuf);
    	return 0;
    } else if (rv < 0) { // Commit xaction and retry
    	return -1;
    } else {
	CB_LOG(CB_ERROR, "ptrie_add() failed, rv=%d key=%s", rv, ptrie_key);
	return 2;
    }
}

/*
 * FQDN_to_index() - Map FQDN to index using ptrie
 *  ptrie lookup "{FQDN2Index}/FQDN" where FQDN is mapped to upper case
 *
 *  fqdn - Max strlen of L_ELEMENT_FQDN_MAXLEN, where all alphas are upper case
 *
 *  Return:
 *    >  0, Success
 *    == 0, Error, where err denotes specific error
 */
static uint32_t
FQDN_to_index(ptrie_context_t *pctx, const char *fqdn, int *err)
{
    char ptrie_key[FQDN2IX_KEY_PREFIX_STRLEN + L_ELEMENT_FQDN_MAXLEN + 1];
    OCRP_app_data_t *apd;
    int ret = 0;
    uint32_t ix = 0;

    while (1) {

    if (!fqdn || !*fqdn) {
    	ret =  1;
    }

    memcpy(ptrie_key, FQDN2IX_KEY_PREFIX, FQDN2IX_KEY_PREFIX_STRLEN);
    strcpy(&ptrie_key[FQDN2IX_KEY_PREFIX_STRLEN], fqdn);

    apd = (OCRP_app_data_t *)ptrie_tr_exact_match(pctx, ptrie_key);
    if (apd) {
    	if (apd->u.d.type == OCRP_AD_TYPE_NUM) {
	    ix = apd->u.d.u.num.d;
	} else {
	    CB_LOG(CB_ERROR, "Invalid OCRP app_data type (%d) expected (%d)",
	    	   apd->u.d.type, OCRP_AD_TYPE_NUM);
	    ret = 2;
	}
    } else {
	CB_LOG(CB_MSG, "ptrie_tr_exact_match() failed, key=%s", ptrie_key);
    	ret = 3;
    }
    break;

    } // End while


    if (err) {
    	*err = ret;
    }
    return ix;
}

/*
 * add_index_to_FQDN() - Add entry to "{Index2FQDN}" ptrie.
 *  Add entry to "{Index2FQDN}/num" ptrie where num is a hex value
 *
 *  fqdn - Max strlen of L_ELEMENT_FQDN_MAXLEN, where all alphas are upper case
 *
 *  Return:
 *    == 0, Success
 *     < 0, Commit xaction and retry
 *     > 0, Error
 */
static int
add_index_to_FQDN(ptrie_context_t *pctx, int64_t ix, const char *fqdn)
{
    char tbuf[2048];
    char key[IX2FQDN_KEY_PREFIX_STRLEN+64];
    int rv;
    OCRP_app_data_t apd;

    rv = snprintf(key, sizeof(key), "%s%lx", IX2FQDN_KEY_PREFIX, ix);
    if (rv >= (int)sizeof(key)) {
    	key[sizeof(key)-1] = '\0';
    }

    memset(&apd, 0, sizeof(apd));
    apd.u.d.type = OCRP_AD_TYPE_NAME;
    apd.u.d.u.name.nm_strlen = strlen(fqdn);
    strncpy(apd.u.d.u.name.nm, fqdn, sizeof(apd.u.d.u.name.nm)-1);

    rv = ptrie_add(pctx, key, &apd.u.apd);
    if (!rv) {
	dump_OCRP_AD(tbuf, sizeof(tbuf), &apd);
	CB_LOG(CB_MSG, "Add key=%s ctx=%p app_data=%s", key, pctx, tbuf);
    	return 0;
    } else if (rv < 0) { // Commit xaction and retry
    	return -1;
    } else {
	CB_LOG(CB_ERROR, "ptrie_add() failed, rv=%d key=%s", rv, key);
	return 2;
    }
}

#define MK_IX2FQDN_KEY(_buf, _buflen, _ix) { \
    int _rv; \
    _rv = snprintf((_buf), (_buflen), "%s%lx", IX2FQDN_KEY_PREFIX, (_ix)); \
    if (_rv >= (int)(_buflen)) { \
    	(_buf)[(_buflen)-1] = '\0'; \
    } \
}

#ifdef _UNUSED_FUNC
/*
 * index_to_FQDN() - Map index to FQDN using ptrie
 *  ptrie lookup "{Index2FQDN}/num" where num is a hex value
 *
 *  Note: Update side caller
 *
 *  Return:
 *    == 0, Success
 *    != 0, Error
 */
static int
index_to_FQDN(ptrie_context_t *pctx, int64_t ix, 
	      const char **fqdn, int *fqdn_strlen)
{
    char key[IX2FQDN_KEY_PREFIX_STRLEN+64];
    int ret = 0;
    OCRP_app_data_t *apd;

    MK_IX2FQDN_KEY(key, sizeof(key), ix);

    apd = (OCRP_app_data_t *)ptrie_tr_exact_match(pctx, key);
    if (apd) {
    	if (apd->u.d.type == OCRP_AD_TYPE_NAME) {
	    *fqdn = apd->u.d.u.name.nm;
	    *fqdn_strlen = apd->u.d.u.name.nm_strlen;
	} else {
	    CB_LOG(CB_ERROR, "Invalid OCRP app_data type (%d) expected (%d)",
	    	   apd->u.d.type, OCRP_AD_TYPE_NAME);
	    ret = 1;
	}
    } else {
	CB_LOG(CB_ERROR, "ptrie_tr_exact_match() failed, key=%s", key);
    	ret = 2;
    }
    return ret;
}
#endif /* _UNUSED_FUNC */

/*
 * reader_index_to_FQDN() - Map index to FQDN using ptrie
 *  ptrie lookup "{Index2FQDN}/num" where num is a hex value
 *
 *  Note: Reader side caller
 *
 *  Return:
 *    == 0, Success
 *    != 0, Error
 */
static int
reader_index_to_FQDN(ptrie_context_t *pctx, int64_t ix, 
	      	     char *fqdn, int fqdn_buflen)
{
    char key[IX2FQDN_KEY_PREFIX_STRLEN+64];
    int rv;
    int ret = 0;
    OCRP_app_data_t apd;

    MK_IX2FQDN_KEY(key, sizeof(key), ix);

    rv = ptrie_exact_match(pctx, key, &apd.u.apd);
    if (!rv) {
    	if (apd.u.d.type == OCRP_AD_TYPE_NAME) {
	    strncpy(fqdn, apd.u.d.u.name.nm, fqdn_buflen);
	} else {
	    CB_LOG(CB_ERROR, "Invalid OCRP app_data type (%d) expected (%d)",
	    	   apd.u.d.type, OCRP_AD_TYPE_NAME);
	    ret = 1;
	}
    } else {
	CB_LOG(CB_ERROR, "ptrie_exact_match() failed, rv=%d key=%s", rv, key);
    	ret = 2;
    }
    return ret;
}

/*
 * add_FQDN_mapping - Add FQDN mapping to {FQDN2Index} and {Index2FQDN} ptrie
 *
 *  fqdn - Max strlen of L_ELEMENT_FQDN_MAXLEN, where all alphas are upper case
 *
 *  Return:
 *    ==0, Success
 *    < 0, Commit xaction and retry
 *    > 0, Error
 */
static int
add_FQDN_mapping(ptrie_context_t *pctx, const char *fqdn)
{
#define ACT_EXIT		0
#define ACT_INDEX		1
#define ACT_FQDN_TO_INDEX	2
#define ACT_INDEX_TO_FQDN	3

    char tbuf[2048];
    OCRP_app_data_t *apd;
    OCRP_app_data_t new_apd;
    int64_t ix;
    int rv;
    int ret = 0;
    int err;
    int action;

    /*
     * Actions:
     *  1) Allocate FQDN index
     *  2) Create FQDN to index ptrie mapping
     *  3) Create index to FQDN ptrie mapping
     *
     *  Since any of the above actions can request commit and retry,
     *  we must be prepared to handle multiple calls for a given
     *  FQDN mapping.
     */

    ix = FQDN_to_index(pctx, fqdn, &err);
    if (ix) {
    	action = ACT_INDEX_TO_FQDN;
    } else {
    	action = ACT_INDEX;
    }

    while (action != ACT_EXIT) {

    switch (action) {
    case ACT_INDEX:
    	apd = (OCRP_app_data_t *)
	      ptrie_tr_exact_match(pctx, FQDN_LAST_INDEX_KEY);
    	if (apd) {
	    if (apd->u.d.type == OCRP_AD_TYPE_NUM) {
	    	new_apd = *apd; 
	    	ix = ++new_apd.u.d.u.num.d;
		if (ix > UINT_MAX) {
		    CB_LOG(CB_SEVERE, 
		    	   "FQDN free index max exceeded (%ld > %ud)",
			   ix, UINT_MAX)
		    ret = 1;
		}
	    	rv = ptrie_update_appdata(pctx, FQDN_LAST_INDEX_KEY,
					  &apd->u.apd, &new_apd.u.apd);
		if (!rv) {
		    dump_OCRP_AD(tbuf, sizeof(tbuf), &new_apd);
		    CB_LOG(CB_MSG, "Update key=%s ctx=%p app_data=%s", 
			   FQDN_LAST_INDEX_KEY, pctx, tbuf);

		    action = ACT_FQDN_TO_INDEX;
		} else if (rv < 0) {
		    if (rv == -1) {
		    	ret = -1; // Commit xaction and retry
		    	action = ACT_EXIT;
		    } else {
		    	CB_LOG(CB_ERROR, "ptrie_update_appdata() failed, "
		    	       "rv=%d ctx=%p", rv, pctx);
		    	ret = 2;
		    	action = ACT_EXIT;
		    }
	    	}
	    } else {
	    	CB_LOG(CB_ERROR, "Invalid OCRP app_data type (%d) "
		       "expected (%d)", apd->u.d.type, OCRP_AD_TYPE_NUM);
	    	ret = 3;
		action = ACT_EXIT;
	    }
    	} else {
	    memset(&new_apd, 0, sizeof(new_apd));
	    new_apd.u.d.type = OCRP_AD_TYPE_NUM;
	    new_apd.u.d.u.num.d = 1; // Start at non-zero value

	    rv = ptrie_add(pctx, FQDN_LAST_INDEX_KEY, &new_apd.u.apd);
	    if (!rv) {
		dump_OCRP_AD(tbuf, sizeof(tbuf), &new_apd);
		CB_LOG(CB_MSG, "Add key=%s ctx=%p app_data=%s", 
		       FQDN_LAST_INDEX_KEY, pctx, tbuf);

	    	ix = new_apd.u.d.u.num.d;
		action = ACT_FQDN_TO_INDEX;
	    } else if (rv == -1) { // Commit xaction and retry
		ret = -1;
	    	action = ACT_EXIT;
	    } else {
	    	CB_LOG(CB_ERROR, "ptrie_add() failed, rv=%d ctx=%p", pctx, rv);
		ret = 4;
	    	action = ACT_EXIT;
	    }
    	}
    	break;

    case ACT_FQDN_TO_INDEX:
	rv = add_FQDN_to_index(pctx, fqdn, ix);
	if (!rv) {
	    action = ACT_INDEX_TO_FQDN;
	} else if (rv < 0) { // Commit xaction and retry
	    ret = -1;
	    action = ACT_EXIT;
	} else {
	    CB_LOG(CB_ERROR, 
	    	   "add_FQDN_to_ix() failed, rv=%d ctx=%p fqdn=%s ix=%ld", 
		   rv, pctx, fqdn, ix);
	    ret = 10;
	    action = ACT_EXIT;
	}
    	break;

    case ACT_INDEX_TO_FQDN:
	rv = add_index_to_FQDN(pctx, ix, fqdn);
	if (!rv) {
	    action = ACT_EXIT;
	} else if (rv < 0) { // Commit xaction and retry
	    ret = -1;
	    action = ACT_EXIT;
	} else {
	    CB_LOG(CB_ERROR, 
	    	   "add_FQDN_to_ix() failed, rv=%d ctx=%p fqdn=%s ix=%ld", 
		   rv, pctx, fqdn, ix);
	    ret = 20;
	    action = ACT_EXIT;
	}
    	break;

    default:
	CB_LOG(CB_ERROR, "Invalid action=%d", action);
	ret = 30;
	action = ACT_EXIT;
	break;
    	
    } // End switch

    } // End while

    return ret;

#undef ACT_INDEX
#undef ACT_FQDN_TO_INDEX
#undef ACT_INDEX_TO_FQDN
}

static int 
commit_begin_xaction(ptrie_context_t *pctx, OCRP_fh_user_data_t *fhd)
{
    int rv;

    rv = ptrie_end_xaction(pctx, 1 /* commit */, (fh_user_data_t *) fhd);
    if (rv) {
	CB_LOG(CB_ERROR, "ptrie_end_xaction() failed, "
	       "rv=%d ctx=%p", rv, pctx);
	return 1;
    }
    rv = ptrie_begin_xaction(pctx);
    if (rv) {
	CB_LOG(CB_ERROR, "ptrie_begin_xaction() failed, "
	       "rv=%d ctx=%p", rv, pctx);
	return 2;
    }
    return 0;
}

static int
qsort_cmp_location_t(const void *p1, const void *p2)
{
    location_t *ploc1 = (location_t *) p1;
    location_t *ploc2 = (location_t *) p2;

    if (ploc1->weight > ploc2->weight) {
    	return 1;
    } else if (ploc1->weight < ploc2->weight) {
    	return -1;
    } else {
    	return 0;
    }
}

static int 
mk_OCRP_appdata(ptrie_context_t *pctx, OCRP_fh_user_data_t *fhd,
		const OCRP_record_t *ocrp, OCRP_app_data_t *ad)
{
    int n;
    int pin;
    int rv;
    int err;

    uint64_t seqno;
    uint64_t vers;
    const char *FQDN_start[L_ELEMENT_MAX_TUPLES];
    const char *FQDN_end[L_ELEMENT_MAX_TUPLES];
    long port[L_ELEMENT_MAX_TUPLES];
    long weight[L_ELEMENT_MAX_TUPLES];
    int lentries;
    char fqdn[L_ELEMENT_FQDN_MAXLEN+1];
    const char *p;
    int k;

    pin = 0;
    for (n = 0; n < ocrp->u.Entry.num_A; n++) {
   	if (ocrp->u.Entry.A[n]) {
	    if (!strcasecmp(OCRP_ATTR_PIN, (const char *)ocrp->u.Entry.A[n])) {
	    	pin++;
	    }
	}
    }

    rv = OCRP_parse_H(ocrp->u.Entry.H, &seqno, &vers);
    if (rv) {
    	CB_LOG(CB_ERROR, "OCRP_parse_H(data=%s) failed, rv=%d", 
	       (char *)ocrp->u.Entry.H, rv);
	return 1;
    }

    rv = OCRP_parse_L(ocrp->u.Entry.L, &lentries, FQDN_start, FQDN_end, 
		      &port, &weight);
    if (rv) {
    	CB_LOG(CB_ERROR, "OCRP_parse_L(data=%s) failed, rv=%d", 
	       (char *)ocrp->u.Entry.L, rv);
	return 2;
    }

    memset(ad, 0, sizeof(*ad));
    ad->u.d.type = OCRP_AD_TYPE_RECORD;
    ad->u.d.u.rec.flags |= pin ? OCRP_REC_FLAGS_PIN : 0;
    ad->u.d.u.rec.seqno = seqno;
    ad->u.d.u.rec.vers = vers;
    ad->u.d.u.rec.num_loc = lentries;

    for (n = 0; n < lentries; n++) {
    	for (k = 0, p = FQDN_start[n]; p <= FQDN_end[n]; p++, k++) {
	    fqdn[k] = MAP2UPPER[(int)(*p)];
	}
	fqdn[k] = '\0';
	ad->u.d.u.rec.loc[n].port = port[n];
	ad->u.d.u.rec.loc[n].weight = weight[n];

	while (1) {
	    ad->u.d.u.rec.loc[n].fqdn_ix = FQDN_to_index(pctx, fqdn, &err);
	    if (ad->u.d.u.rec.loc[n].fqdn_ix) {
	    	break;
	    } else {
	    	while (1) {
		    rv = add_FQDN_mapping(pctx, fqdn);
		    if (!rv) {
		    	break;
		    } else if (rv < 0) { // Commit xaction and retry
		    	rv = commit_begin_xaction(pctx, fhd);
			if (rv) {
			    CB_LOG(CB_ERROR, "commit_begin_xaction() failed, "
			    	   "rv=%d ctx=%p fhd=%p", rv, pctx, fhd);
			    return 3;
			}
		    } else {
    			CB_LOG(CB_ERROR, "add_FQDN_mapping() failed, "
			       "rv=%d, ctx=%p key=%s", rv, pctx, fqdn);
			return 4;
		    }
		}
	    }
	}
    }

    if (lentries) {
    	qsort(&ad->u.d.u.rec.loc[0], lentries, sizeof(location_t),
	      qsort_cmp_location_t);
    }
    return 0;
}

/* 
 * process_OCRP_Entry() - OCRP add/update entry action
 *
 *  Note: OCRP_record_t validated prior to the call
 *
 *  Return:
 *    == 0, Success
 *    != 0, Error
 */
static int
process_OCRP_Entry(ptrie_context_t *pctx, OCRP_fh_user_data_t *fhd, 
		   OCRP_record_t *ocrp)
{
    char tbuf[2048];
    uint64_t seqno;
    uint64_t vers;
    int rv;
    const char *key;
    OCRP_app_data_t *old_app_data;
    OCRP_app_data_t new_app_data;
    int ret = 0;

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    rv = OCRP_parse_H(ocrp->u.DeleteEntry.H, &seqno, &vers);
    if (rv) {
    	CB_LOG(CB_ERROR, "OCRP_parse_H(data=%s) failed, rv=%d", 
	       (char *)ocrp->u.Entry.H, rv);
	ret = 1;
	break;
    }

    rv = OCRP_parse_K(ocrp->u.Entry.K, &key);
    if (rv) {
    	CB_LOG(CB_ERROR, "OCRP_parse_K(data=%s) failed, rv=%d ctx=%p", 
	       (char *)ocrp->u.Entry.K, rv, pctx);
	ret = 2;
	break;
    }

    rv = mk_OCRP_appdata(pctx, fhd, ocrp, &new_app_data);
    if (rv) {
    	CB_LOG(CB_ERROR, "mk_OCRP_appdata() failed, rv=%d ctx=%p", rv, pctx);
	ret = 3;
	break;
    }

    old_app_data = (OCRP_app_data_t *)ptrie_tr_exact_match(pctx, key);
    if (!old_app_data) {
	while (1) {
	    rv = ptrie_add(pctx, key, &new_app_data.u.apd);
	    if (!rv) {
	    	fhd->u.d.OCRP_seqno = seqno;
	    	fhd->u.d.OCRP_version = vers;

	    	dump_OCRP_AD(tbuf, sizeof(tbuf), &new_app_data);
	    	CB_LOG(CB_MSG, "Add key=%s ctx=%p app_data=%s",
		       key, pctx, tbuf);
		break;
	    } else if (rv < 0) { // Commit xaction and retry
	    	rv = commit_begin_xaction(pctx, fhd);
		if (rv) {
		    CB_LOG(CB_ERROR, 
			   "commit_begin_xaction() failed, rv=%d ctx=%p fhd=%p",
			   pctx, fhd);
		    ret = 4;
		    break;
		}
		// Retry
	    } else {
		CB_LOG(CB_ERROR, "ptrie_add() failed, rv=%d key=%s", rv, key);
		ret = 5;
		break;
	    }
	}
    } else {
	if (old_app_data->u.d.type != OCRP_AD_TYPE_RECORD) {
	    CB_LOG(CB_ERROR, 
	    	   "Invalid old_app_data type (%d) expected (%d) ctx=%p key=%s",
		   old_app_data->u.d.type, OCRP_AD_TYPE_RECORD, pctx, key);
	    ret = 6;
	    break;
	}

	if (old_app_data->u.d.u.rec.vers != new_app_data.u.d.u.rec.vers) {
	    CB_LOG(CB_MSG, 
	    	   "Invalid new_app_data vers(%d) != old_app_data vers(%d), "
		   "ctx=%p key=%s, ignoring new data", 
		   new_app_data.u.d.u.rec.vers,
		   old_app_data->u.d.u.rec.vers, pctx);
	    // Ignore, exit with success
	    break;
	}

	if (old_app_data->u.d.u.rec.seqno >= new_app_data.u.d.u.rec.seqno) {
	    CB_LOG(CB_MSG, 
	    	   "Invalid new_app_data seqno(%d) <= old_app_data seqno(%d), "
		   "ctx=%p key=%s, ignoring new data", 
		   new_app_data.u.d.u.rec.seqno,
		   old_app_data->u.d.u.rec.seqno, pctx,key);
	    // Ignore, exit with success
	    break;
	}

	while (1) {
	    rv = ptrie_update_appdata(pctx, key, &old_app_data->u.apd, 
				      &new_app_data.u.apd);
	    if (!rv) {
	    	fhd->u.d.OCRP_seqno = seqno;
	    	fhd->u.d.OCRP_version = vers;

	    	dump_OCRP_AD(tbuf, sizeof(tbuf), &new_app_data);
	    	CB_LOG(CB_MSG, "Update key=%s ctx=%p app_data=%s",
		       key, pctx, tbuf);
	    	break;
	    } else if (rv < 0) { // Commit xaction and retry
	    	rv = commit_begin_xaction(pctx, fhd);
		if (rv) {
		    CB_LOG(CB_ERROR, 
			   "commit_begin_xaction() failed, rv=%d ctx=%p "
			   "fhd=%p key=%s", pctx, fhd, key);
		    ret = 7;
		    break;
		}
	    } else {
		CB_LOG(CB_ERROR, "ptrie_add() failed, rv=%d ctx=%p key=%s", 
		       rv, pctx, key);
		ret = 8;
		break;
	    }
	}
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    return ret;
}

/* 
 * process_OCRP_DeleteEntry() - OCRP delete entry action
 *
 *  Note: OCRP_record_t validated prior to the call
 *
 *  Return:
 *    == 0, Success
 *    != 0, Error
 */
static int
process_OCRP_DeleteEntry(ptrie_context_t *pctx, OCRP_fh_user_data_t *fhd,
			 OCRP_record_t *ocrp)
{
    uint64_t seqno;
    uint64_t vers;
    int rv;
    const char *key;
    int ret = 0;

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    rv = OCRP_parse_H(ocrp->u.DeleteEntry.H, &seqno, &vers);
    if (rv) {
    	CB_LOG(CB_ERROR, "OCRP_parse_H(data=%s) failed, rv=%d", 
	       (char *)ocrp->u.DeleteEntry.H, rv);
	ret = 1;
	break;
    }

    rv = OCRP_parse_K(ocrp->u.DeleteEntry.K, &key);
    if (rv) {
    	CB_LOG(CB_ERROR, "OCRP_parse_K(data=%s) failed, rv=%d", 
	       (char *)ocrp->u.DeleteEntry.K, rv);
	ret = 2;
	break;
    }

    while (1) {
	rv = ptrie_remove(pctx, key);
	if (!rv) {
	    fhd->u.d.OCRP_seqno = seqno;
	    fhd->u.d.OCRP_version = vers;
	    CB_LOG(CB_MSG, "Remove key=%s ctx=%p", key, pctx);
	    break;
	} else if (rv < 0) { // commit and retry
	    rv = commit_begin_xaction(pctx, fhd);
	    if (rv) {
		CB_LOG(CB_ERROR, 
		       "commit_begin_xaction() failed, rv=%d ctx=%p "
		       "fhd=%p key=%s", rv, pctx, fhd, key);
		ret = 3;
		break;
	    }
	} else {
	    CB_LOG(CB_ERROR, "ptrie_remove() failed, rv=%d ctx=%p key=%s", 
		   rv, pctx, key);
	    ret = 4;
	    break;
	}
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    return ret;
}

static int 
update_ptrie(ptrie_context_t *pctx, int load_op, XMLDocCTX_t *doc_ctx)
{
    char errbuf[1024];
    char OCRP_state_str[1024];
    OCRP_record_t ocrp;
    OCRP_fh_user_data_t fhd;
    int rv;
    int ret = 0;

    fhd = *((OCRP_fh_user_data_t *) ptrie_get_fh_data(pctx));

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    rv = ptrie_begin_xaction(pctx);
    if (rv) {
	CB_LOG(CB_ERROR, "ptrie_begin_xaction() failed");
	ret = 1;
	break;
    }

    if (load_op) {
    	rv = ptrie_reset(pctx);
	if (rv) {
	    CB_LOG(CB_ERROR, "ptrie_reset() failed, rv=%d", rv);
	    ptrie_end_xaction(pctx, 0 /*abort*/, (fh_user_data_t *) &fhd);
	    ret = 2;
	    break;
	}
    }

    while (1) {
	rv = OCRP_XML2Struct(doc_ctx, &ocrp, errbuf, sizeof(errbuf));
	if (rv == 0) {
	    switch(ocrp.type) {
	    case OCRP_Entry:
	        rv = process_OCRP_Entry(pctx, &fhd, &ocrp);
		if (rv) {
		    CB_LOG(CB_ERROR, "process_OCRP_Entry() failed, rv=%d", rv);
		    ret = 3;
		}
	        break;
	    case OCRP_DeleteEntry:
	        rv = process_OCRP_DeleteEntry(pctx, &fhd, &ocrp);
		if (rv) {
		    CB_LOG(CB_ERROR, 
		    	   "process_OCRP_DeleteEntry() failed, rv=%d", rv);
		    ret = 4;
		}
	        break;
	    default:
	        break;
	    }
	} else if (rv > 0) {
	    CB_LOG(CB_ERROR, "OCRP_XML2Struct() error, rv=%d errbuf=[%s]\n",
		   rv, errbuf);
	} else { // EOF
	    break;
	}

	if (ret) {
	    break;
	}
    }

    if (ret) {
    	ptrie_end_xaction(pctx, 0 /*abort*/, (fh_user_data_t *) &fhd);
	break;
    }

    rv = ptrie_end_xaction(pctx, 1 /*commit*/, (fh_user_data_t *) &fhd);
    if (!rv) {
    	rv = make_H_record(fhd.u.d.OCRP_seqno, fhd.u.d.OCRP_version,
			   OCRP_state_str, sizeof(OCRP_state_str));
    	if (!rv) {
	    rv = update_OCRP_state(OCRP_state_str);
	    if (rv) {
	    	CB_LOG(CB_ERROR, "update_OCRP_state() failed, rv=%d", rv);
		ret = 1000 + 5;
	    }
	} else {
	    CB_LOG(CB_ERROR, "make_H_record() failed, rv=%d", rv);
	    ret = 1000 + 6;
	}
    } else {
	CB_LOG(CB_ERROR, "ptrie_end_xaction() error, rv=%d", rv);
	ret = 7;
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    return ret;
}

static int
process_OCRP_post_file(ptrie_context_t *pctx, const char *filename, 
		       const OCRP_post_file_t *pfd)
{
    char errbuf[1024];
    XMLDocCTX_t *doc_ctx;
    int rv;
    int ret = 0;
    const char *DTD;
    int load_op = 0;

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

    doc_ctx = OCRP_newXMLDocCTX();
    if (!doc_ctx) {
	CB_LOG(CB_ERROR, "OCRP_newXMLDocCTX() failed");
	ret = 1;
	break;
    }

    switch(pfd->type) {
    case OCRP_FT_LOAD:
    	DTD = OCRP_DTD;
	load_op = 1;
	break;

    case OCRP_FT_UPDATE:
    	DTD = OCRPupdate_DTD;
	break;

    default:
    	DTD = 0;
	break;
    }

    if (DTD) {
    	rv = OCRP_validateXML(filename, DTD, doc_ctx, errbuf, sizeof(errbuf));
    	if (!rv) {
	    rv = update_ptrie(pctx, load_op, doc_ctx);
	    if (rv) {
	    	CB_LOG(CB_ERROR, "OCRP_validateXML() failed, rv=%d msg=[%s]", 
		       rv, errbuf);
		ret = 2;
	    }
	} else {
	    CB_LOG(CB_ERROR, "OCRP_validateXML() failed, rv=%d msg=[%s]",
		   rv, errbuf);
	    ret = 3;
    	}
    } else {
	CB_LOG(CB_ERROR, "Invalid file type=%d", pfd->type);
	ret = 4;
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    if (doc_ctx) {
    	OCRP_deleteXMLDocCTX(doc_ctx);
    }
    return ret;
}

static void *
ptrie_update_thread(void *arg)
{
    char filename[2048];
    ptrie_context_t *ctx = (ptrie_context_t *)arg;
    OCRP_post_flist_t *flist;
    int rv;
    int n;

    prctl(PR_SET_NAME, "cb-update", 0, 0, 0);

    while (1) {
    	sleep(1);

	flist = get_OCRP_post_filelist(&rv);
	if (rv) {
	    CB_LOG(CB_ERROR, "get_OCRP_post_filelist() failed, rv=%d", rv);
	    continue;
	}

	for (n = 0; n < flist->entries; n++) {
	    rv = OCRP_post_file2filename(&flist->files[n], 
				     	 filename, sizeof(filename));
	    if (!rv) {
	    	rv = process_OCRP_post_file(ctx, filename, &flist->files[n]);
		if (rv) {
		    CB_LOG(CB_ERROR, "process_OCRP_post_file() failed, rv=%d", 
		    	   rv);
		}
		unlink(filename);
	    }
	}
	free(flist);
    }

    return 0;
}

static
int write_chunk(int fd, const char *data, long chunksize)
{
    char buf[128];
    int len;
    ssize_t bytes;
    struct iovec iov[3];

    if (chunksize) {
    	len = snprintf(buf, sizeof(buf), "%lx\r\n", chunksize);
    	iov[0].iov_base = buf;
    	iov[0].iov_len = len;

    	iov[1].iov_base = (void *)data;
    	iov[1].iov_len = chunksize;

    	iov[2].iov_base = (void *)"\r\n";
    	iov[2].iov_len = 2;

	bytes = writev(fd, iov, 3);
	if (bytes == (len + chunksize + 2)) {
	    return 0;
	} else {
	    return 1;
	}
    } else {
    	bytes = write(fd, "0\r\n\r\n", 5);
	if (bytes == 5) {
	    return 0;
	} else {
	    return 1;
	}
    }
}

static
int write_data(int fd, const char *data, long datalen)
{
    ssize_t bytes;

    bytes = write(fd, data, datalen);
    if (datalen == bytes) {
	return 0;
    } else {
	return 1;
    }
}

static int
mk_http_response(token_data_t token, int http_resp_code, 
		 const char **buf, int *buflen)
{
    int rv;

    if (http_resp_code == 200) {
    	rv = HTTPAddKnownHeader(token, 1, H_TRANSFER_ENCODING, "chunked", 7);
	if (rv) {
	    return 1000;
	}
    	rv = HTTPAddKnownHeader(token, 1, H_CONTENT_TYPE, 
				"application/octet-stream", 24);
	if (rv) {
	    return 1001;
	}
	rv = HTTPResponse(token, 200, buf, buflen, 0);

    } else {
    	rv = HTTPAddKnownHeader(token, 1, H_TRANSFER_ENCODING, "chunked", 7);
	if (rv) {
	    return 2000;
	}

    	rv = HTTPAddKnownHeader(token, 1, H_CONTENT_TYPE, "text/plain", 10);
	if (rv) {
	    return 2001;
	}
	rv = HTTPResponse(token, 404, buf, buflen, 0);
    }
    return rv;
}

static int
mk_record_response(const char *key, cb_route_data_t *cb_rd,
		   char *buf, int sizeof_buf)
{

#define WRITE_BUF(_err_retcode, _fmt, ...) { \
    rv = snprintf(&buf[cnt], sizeof_buf-cnt, _fmt, ##__VA_ARGS__); \
    if (rv >= (sizeof_buf-cnt)) { \
	buf[sizeof_buf-1] = '\0'; \
	return (_err_retcode); \
    } \
    cnt += rv; \
}

    int rv;
    int cnt = 0;
    int n;

    WRITE_BUF(-1, "<Entry>\n");
    WRITE_BUF(-2, "<H>%ld:%ld</H>\n", cb_rd->seqno, cb_rd->vers);
    WRITE_BUF(-3, "<K>%s</K>\n", key);

    if (cb_rd->options & CB_RT_OPT_PIN) {
    	WRITE_BUF(-4, "<A>pin</A>\n");
    }

    WRITE_BUF(-5, "<L>");

    for (n = 0; n < cb_rd->num_locs; n++) {
	if (n != 0) {
	    WRITE_BUF(-6, ",%s:%hu:%hu", 
		      cb_rd->loc[n].fqdn,
		      cb_rd->loc[n].port, cb_rd->loc[n].weight);
	} else {
	    WRITE_BUF(-7, "%s:%hu:%hu",
		      cb_rd->loc[n].fqdn,
		      cb_rd->loc[n].port, cb_rd->loc[n].weight);
	}
    }

    WRITE_BUF(-8, "</L>\n");
    WRITE_BUF(-9, "</Entry>\n"); 

    return cnt;

#undef WRITE_BUF
}

static int
send_get_asset_data(const char *key, int fd)
{
    char body[4096];
    int bodylen;
    int rv;
    cb_route_data_t cb_rd;

    rv = cb_route(key, 1 /* all routes */, &cb_rd);
    if (!rv) {
	bodylen = mk_record_response(key, &cb_rd, body, sizeof(body));
	if (bodylen <= 0) {
	    CB_LOG(CB_ERROR, "mk_record_response() failed, rv=%d", bodylen);
	    return 1;
	}
    } else {
	CB_LOG(CB_MSG, "cb_route() failed, rv=%d key=%s", rv, key);
	return 2;

    }
    rv = write_chunk(fd, body, bodylen);
    if (rv) {
	CB_LOG(CB_MSG, "write_chunk() failed, rv=%d fd=%d", rv, fd);
	return 3;
    }
    return 0;
}

static int 
ptrie_list_keys_callback(const char *key, void *arg)
{
    int fd = *(int *)arg;
    int rv;

    if (*key == '/') {
    	rv = send_get_asset_data(key, fd);
	if (rv) {
	    CB_LOG(CB_ERROR, "send_get_asset_data() failed, rv=%d", rv);
	}
    }
    return 0;
}

static int 
send_get_asset_response(ptrie_context_t *ctx, token_data_t token, int fd)
{
    const char *val;
    int vallen;
    int hdrcnt;
    char uristr[1024];
    const char *http_resp;
    int http_resp_len;
    int rv;

    rv = HTTPGetKnownHeader(token, 0, H_X_NKN_URI, &val, &vallen, &hdrcnt);
    if (!rv) {
    	if (vallen < (int)sizeof(uristr)) {
	    memcpy(uristr, val, vallen);
	    uristr[vallen] = '\0';
	}
    } else {
	CB_LOG(CB_ERROR, "HTTPGetKnownHeader() failed, rv=%d", rv);
	return 1;
    }

    rv = mk_http_response(token, 200, &http_resp, &http_resp_len);
    if (rv) {
	CB_LOG(CB_ERROR, "mk_http_response() failed, rv=%d", rv);
	return 2;
    }

    rv = write_data(fd, http_resp, http_resp_len);
    if (rv) {
	CB_LOG(CB_ERROR, "write_data() failed, rv=%d fd=%d", rv, fd);
	return 3;
    }

    rv = write_chunk(fd, http_resp_body_hdr, http_resp_body_hdr_strlen);
    if (rv) {
	CB_LOG(CB_ERROR, "write_chunk() failed, rv=%d fd=%d", rv, fd);
	return 4;
    }

    if (vallen > 1) {
    	rv = send_get_asset_data(uristr, fd);
    	if (rv) {
	    CB_LOG(CB_MSG, "send_get_asset_data() failed, rv=%d fd=%d", rv, fd);
    	}
    } else {
    	rv = ptrie_list_keys(ctx, (void *)&fd, ptrie_list_keys_callback);
	if (rv) {
	    CB_LOG(CB_ERROR, "ptrie_list_keys() failed, rv=%d fd=%d", rv, fd);
	}
    }

    rv = write_chunk(fd, http_resp_body_trailer, 
		     http_resp_body_trailer_strlen);
    if (rv) {
	CB_LOG(CB_MSG, "write_chunk() failed, rv=%d fd=%d", rv, fd);
	return 5;
    }

    rv = write_chunk(fd, 0, 0); // write zero chunk
    if (rv) {
	CB_LOG(CB_MSG, "write_chunk() failed, rv=%d fd=%d", rv, fd);
	return 6;
    }

    return 0;
}

static void *
get_asset_map_server_thread(void *arg)
{
    ptrie_context_t *ctx = (ptrie_context_t *)arg;
    char recvbuf[16384];
    int listen_fd;
    int fd;
    int ret;
    int val;
    struct sockaddr_in lskaddr;
    struct sockaddr_in skaddr;
    socklen_t skaddr_len;

    proto_data_t pd;
    token_data_t token;
    HTTPProtoStatus pstatus;
    int read_retry;
    int cnt;
    char ip_str[NI_MAXHOST];
    const char *p;

    UNUSED_ARGUMENT(ctx);

    prctl(PR_SET_NAME, "cb-get-map", 0, 0, 0);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
    	CB_LOG(CB_SEVERE, "socket() failed, errno=%d", errno);
    	return 0;
    }

    val = 1;
    ret = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    if (ret) {
    	CB_LOG(CB_SEVERE, "setsockopt(SO_REUSEADDR)failed, fd=%d errno=%d", 
	       listen_fd, errno);
	return 0;
    }

    memset(&lskaddr, 0, sizeof(lskaddr));
    lskaddr.sin_family = AF_INET;
    lskaddr.sin_addr.s_addr = inet_addr(HTTP_GET_ASSET_SERVER_IP);
    lskaddr.sin_port = htons(HTTP_GET_ASSET_SERVER_PORT);

    ret = bind(listen_fd, (struct sockaddr *)&lskaddr, sizeof(lskaddr));
    if (ret) {
    	CB_LOG(CB_SEVERE, "bind() failed, fd=%d errno=%d", listen_fd, errno);
    	return 0;
    }

    ret = listen(listen_fd, 8);
    if (ret) {
    	CB_LOG(CB_SEVERE, "listen() failed, fd=%d errno=%d", listen_fd, errno);
    	return 0;
    }

    ////////////////////////////////////////////////////////////////////////////
    while (1) { // Begin while
    ////////////////////////////////////////////////////////////////////////////

	skaddr_len = 0;
    	fd = accept(listen_fd, &skaddr, &skaddr_len);
	if (fd < 0) {
	    CB_LOG(CB_SEVERE, "accept() failed, fd=%d errno=%d", fd, errno);
	    continue;
	}

	p = inet_ntop(AF_INET, &skaddr.sin_addr.s_addr, ip_str, sizeof(ip_str));
	if (p) {
	    pd.u.HTTP.clientIP = ip_str;
	    pd.u.HTTP.clientPort = 
		ntohs(((struct sockaddr_in *)&skaddr)->sin_port);
	} else {
	    pd.u.HTTP.clientIP = "";
	    pd.u.HTTP.clientPort = 0;
	}
	pd.u.HTTP.destIP = HTTP_GET_ASSET_SERVER_IP;
	pd.u.HTTP.destPort = HTTP_GET_ASSET_SERVER_PORT;
	pd.u.HTTP.data = recvbuf;

	cnt = 0;
	token = 0;
	read_retry = 3;

	////////////////////////////////////////////////////////////////////////
	while (1) { // Begin while
	////////////////////////////////////////////////////////////////////////

	ret = read(fd, recvbuf + cnt, sizeof(recvbuf) - cnt);
	if (ret > 0) {
	    cnt += ret;
	    pd.u.HTTP.datalen = cnt;

	    token = HTTPProtoData2TokenData(0, &pd, &pstatus);
	    if (pstatus != HP_SUCCESS) {
	    	if (pstatus == HP_MORE_DATA) {
		    if (read_retry--)  {
			sleep(1);
		    	continue;
		    } else {
	    		CB_LOG(CB_MSG, 
			       "Short read retry, unable to parse, fd=%d", fd);
		    }
		} else {
		    CB_LOG(CB_MSG, "HTTPProtoData2TokenData() failed, "
		    	   "rv=%d fd=%d", ret, fd);
		}
		break;
	    }
	    ret = send_get_asset_response(ctx, token, fd);
	    if (ret) {
	    	CB_LOG(CB_MSG, "send_get_asset_response() failed, fd=%d ret=%d",
		       fd, ret);
	    }
	} else if (!ret) {
	    CB_LOG(CB_MSG, "read(fd=%d)==0, client close", fd);
	} else {
	    CB_LOG(CB_MSG, "read(fd=%d) error, errno=%d", fd, errno);
	}
	break;

	////////////////////////////////////////////////////////////////////////
	} // End while
	////////////////////////////////////////////////////////////////////////

	if (token) {
	    deleteHTTPtoken_data(token);
	}
	shutdown(fd, SHUT_RDWR);
	close(fd);

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    return 0;
}

/* 
 * cb_init() --- Content Router (CB) subsystem initialization.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 */
int
cb_init()
{
    int rv;
    ptrie_context_t *pctx;
    pthread_attr_t attr;
    int stacksize = 256 * 1024;
    const OCRP_fh_user_data_t *OCRP_fh;
    char OCRP_state_str[1024];
    ptrie_config_t ptrie_cfg;

    assert(sizeof(OCRP_fh_user_data_t) <= sizeof(fh_user_data_t));
    assert(sizeof(OCRP_app_data_t) <= sizeof(app_data_t));

    http_resp_body_hdr_strlen = strlen(http_resp_body_hdr);
    http_resp_body_trailer_strlen = strlen(http_resp_body_trailer);

    // Setup ptrie
    memset(&ptrie_cfg, 0, sizeof(ptrie_cfg));
    ptrie_cfg.interface_version = PTRIE_INTF_VERSION;
    ptrie_cfg.log_level = &log_level;
    ptrie_cfg.proc_logfunc = ptrie_log;
    ptrie_cfg.proc_malloc = cb_malloc;
    ptrie_cfg.proc_calloc = cb_calloc;
    ptrie_cfg.proc_realloc = cb_realloc;
    ptrie_cfg.proc_free = cb_free;

    rv = ptrie_init(&ptrie_cfg);
    if (rv) {
    	CB_LOG(CB_SEVERE, "ptrie_init() failed, rv=%d", rv);
    	return 1;
    }

    pctx = new_ptrie_context(ptrie_copy_app_data, ptrie_destruct_app_data);
    if (!pctx) {
    	CB_LOG(CB_SEVERE, "new_ptrie_context() failed");
	return 2;
    }

    rv = ptrie_recover_from_ckpt(pctx, CB_TRIE_CKP_1, CB_TRIE_CKP_2);
    if (rv) {
    	CB_LOG(CB_SEVERE, "ptrie_recover_from_ckpt() failed, rv=%d", rv);
	return 3;
    }

    // Update OCRP state
    OCRP_fh = (OCRP_fh_user_data_t *) ptrie_get_fh_data(pctx);
    rv = make_H_record(OCRP_fh->u.d.OCRP_seqno, OCRP_fh->u.d.OCRP_version,
    		       OCRP_state_str, sizeof(OCRP_state_str));
    if (rv) {
    	CB_LOG(CB_SEVERE, "make_H_record() failed, rv=%d", rv);
	return 4;
    	
    }
    rv = update_OCRP_state(OCRP_state_str);
    if (rv) {
    	CB_LOG(CB_SEVERE, "update_OCRP_state() failed, rv=%d", rv);
	return 5;
    }

    // Create OCRP POST file consumer thread
    rv = pthread_attr_init(&attr);
    if (rv) {
    	CB_LOG(CB_SEVERE, "pthread_attr_init() failed");
	return 6;
    }

    rv = pthread_attr_setstacksize(&attr, stacksize);
    if (rv) {
    	CB_LOG(CB_SEVERE, "pthread_attr_setstacksize() failed");
	return 7;
    }

    rv = pthread_create(&ptrie_update_thread_id, &attr, ptrie_update_thread, 
    			(void *)pctx);
    if (rv) {
    	CB_LOG(CB_SEVERE, "pthread_create() failed");
	return 8;
    }

    // Create OCRP GET handler thread
    rv = pthread_create(&get_asset_map_server_thread_id, &attr, 
    			get_asset_map_server_thread, (void *)pctx);
    if (rv) {
    	CB_LOG(CB_SEVERE, "pthread_create() failed");
	return 9;
    }
    cb_ptrie_ctx = pctx;

    return 0;
}

/*
 * cb_route -- Given URL_abs_path return the associated route data.
 *
 * Return:
 *  ==0, Success
 *  !=0, Error
 *
 * 1) port is returned in native byte order
 */
int
cb_route(const char *URL_abs_path, int all_locations, cb_route_data_t *cb_rd)
{
    OCRP_app_data_t ad;
    int rv = 0;
    int rv2;
    int n;

    ////////////////////////////////////////////////////////////////////////////
    while (1) {
    ////////////////////////////////////////////////////////////////////////////

    if (!URL_abs_path || (*URL_abs_path != '/')) {
    	rv = 1; // Bad args
	break;
    }

    if (cb_ptrie_ctx) {
	/*
	 * Lock ptrie, need a consistent view across ptrie_prefix_match() 
	 * and reader_index_to_FQDN() calls.
	 */
        rv = ptrie_lock(cb_ptrie_ctx);
	if (rv) {
	    CB_LOG(CB_ERROR, "ptrie_lock() failed, rv=%d ctx=%p", 
		   rv, cb_ptrie_ctx);
	    rv = 2;
	    break;
	}

    	rv = ptrie_prefix_match(cb_ptrie_ctx, URL_abs_path, &ad.u.apd);
	if (rv) {
	    if (ad.u.d.type == OCRP_AD_TYPE_RECORD) {
	    	if (all_locations) {
		    cb_rd->num_locs = ad.u.d.u.rec.num_loc;
		} else {
		    cb_rd->num_locs = 1;
		}

		for (n = 0; n < cb_rd->num_locs; n++) {
		    rv = reader_index_to_FQDN(cb_ptrie_ctx, 
					      ad.u.d.u.rec.loc[n].fqdn_ix, 
					      cb_rd->loc[n].fqdn,
					      sizeof(cb_rd->loc[n].fqdn));
		    if (!rv) {
		        cb_rd->loc[n].port = ad.u.d.u.rec.loc[n].port;
		        cb_rd->loc[n].weight = ad.u.d.u.rec.loc[n].weight;
		    } else {
    			CB_LOG(CB_ERROR, "reader_index_to_FQDN() failed, "
			       "rv=%d n=%d ix=%hu",
			       rv, n, ad.u.d.u.rec.loc[n].fqdn_ix);
		    	rv = 3; // reader_index_to_FQDN lookup failed
			break;
		    }
		}
		cb_rd->options = 0;
		if (ad.u.d.u.rec.flags & OCRP_REC_FLAGS_PIN) {
		    cb_rd->options |= CB_RT_OPT_PIN;
		}
		cb_rd->seqno = ad.u.d.u.rec.seqno;
		cb_rd->vers = ad.u.d.u.rec.vers;
	    } else {
	    	rv = 4; // Bad record type, expected OCRP_AD_TYPE_RECORD
	    }
	} else {
	    rv = 5; // Prefix lookup failed
	}

	/* Unlock ptrie */
        rv2 = ptrie_unlock(cb_ptrie_ctx);
	if (rv2) {
	    CB_LOG(CB_ERROR, "ptrie_unlock() failed, rv=%d ctx=%p", 
		   rv2, cb_ptrie_ctx);
	    rv += 1000;
	}
    } else {
    	rv = 6; // No ctx
    }
    break;

    ////////////////////////////////////////////////////////////////////////////
    } // End while
    ////////////////////////////////////////////////////////////////////////////

    return rv;
}

/*
 * End of cb_router.c
 */
