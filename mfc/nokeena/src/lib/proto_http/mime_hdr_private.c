/* File : mime_hdr_private.c Author : 
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */

/** 
 * @file mime_hdr_private.c
 * @brief Private functions for mime header parser
 * @author 
 * @version 1.00
 * @date 2009-09-08
 */

#if 1
#define STATIC static
#else
#define STATIC
#endif

STATIC const char *day_names_short[7] = {
    "Sun",
    "Mon",
    "Tue",
    "Wed",
    "Thur",
    "Fri",
    "Sat"
};

STATIC const char *day_names_long[7] = {
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday"
};

STATIC const char *month_names[12] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

STATIC char map_upper[128] = {
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
    30,				// 036 01E 00011110 RS (Reqst to Send)(Rec. Sep.)
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

STATIC char map_lower[128] = {
      0,      // 000    000   00000000      NUL    (Null char.)
      1,      // 001    001   00000001      SOH    (Start of Header)
      2,      // 002    002   00000010      STX    (Start of Text)
      3,      // 003    003   00000011      ETX    (End of Text)
      4,      // 004    004   00000100      EOT    (End of Transmission)
      5,      // 005    005   00000101      ENQ    (Enquiry)
      6,      // 006    006   00000110      ACK    (Acknowledgment)
      7,      // 007    007   00000111      BEL    (Bell)
      8,      // 010    008   00001000       BS    (Backspace)
      9,      // 011    009   00001001       HT    (Horizontal Tab)
     10,      // 012    00A   00001010       LF    (Line Feed)
     11,      // 013    00B   00001011       VT    (Vertical Tab)
     12,      // 014    00C   00001100       FF    (Form Feed)
     13,      // 015    00D   00001101       CR    (Carriage Return)
     14,      // 016    00E   00001110       SO    (Shift Out)
     15,      // 017    00F   00001111       SI    (Shift In)
     16,      // 020    010   00010000      DLE    (Data Link Escape)
     17,      // 021    011   00010001      DC1 (XON) (Device Control 1)
     18,      // 022    012   00010010      DC2       (Device Control 2)
     19,      // 023    013   00010011      DC3 (XOFF)(Device Control 3)
     20,      // 024    014   00010100      DC4       (Device Control 4)
     21,      // 025    015   00010101      NAK (Negativ Acknowledgemnt)
     22,      // 026    016   00010110      SYN    (Synchronous Idle)
     23,      // 027    017   00010111      ETB    (End of Trans. Block)
     24,      // 030    018   00011000      CAN    (Cancel)
     25,      // 031    019   00011001       EM    (End of Medium)
     26,      // 032    01A   00011010      SUB    (Substitute)
     27,      // 033    01B   00011011      ESC    (Escape)
     28,      // 034    01C   00011100       FS    (File Separator)
     29,      // 035    01D   00011101       GS    (Group Separator)
     30,      // 036    01E   00011110       RS (Reqst to Send)(Rec. Sep.)
     31,      // 037    01F   00011111       US    (Unit Separator)
     32,      // 040    020   00100000       SP    (Space)
     33,      // 041    021   00100001        !    (exclamation mark)
     34,      // 042    022   00100010        "    (double quote)
     35,      // 043    023   00100011        #    (number sign)
     36,      // 044    024   00100100        $    (dollar sign)
     37,      // 045    025   00100101        %    (percent)
     38,      // 046    026   00100110        &    (ampersand)
     39,      // 047    027   00100111        '    (single quote)
     40,      // 050    028   00101000        (  (left/open parenthesis)
     41,      // 051    029   00101001        )  (right/closing parenth.)
     42,      // 052    02A   00101010        *    (asterisk)
     43,      // 053    02B   00101011        +    (plus)
     44,      // 054    02C   00101100        ,    (comma)
     45,      // 055    02D   00101101        -    (minus or dash)
     46,      // 056    02E   00101110        .    (dot)
     47,      // 057    02F   00101111        /    (forward slash)
     48,      // 060    030   00110000        0
     49,      // 061    031   00110001        1
     50,      // 062    032   00110010        2
     51,      // 063    033   00110011        3
     52,      // 064    034   00110100        4
     53,      // 065    035   00110101        5
     54,      // 066    036   00110110        6
     55,      // 067    037   00110111        7
     56,      // 070    038   00111000        8
     57,      // 071    039   00111001        9
     58,      // 072    03A   00111010        :    (colon)
     59,      // 073    03B   00111011        ;    (semi-colon)
     60,      // 074    03C   00111100        <    (less than)
     61,      // 075    03D   00111101        =    (equal sign)
     62,      // 076    03E   00111110        >    (greater than)
     63,      // 077    03F   00111111        ?    (question mark)
     64,      // 100    040   01000000        @    (AT symbol)
     97,      // 101    041   01000001        A
     98,      // 102    042   01000010        B
     99,      // 103    043   01000011        C
     100,     // 104    044   01000100        D
     101,     // 105    045   01000101        E
     102,     // 106    046   01000110        F
     103,     // 107    047   01000111        G
     104,     // 110    048   01001000        H
     105,     // 111    049   01001001        I
     106,     // 112    04A   01001010        J
     107,     // 113    04B   01001011        K
     108,     // 114    04C   01001100        L
     109,     // 115    04D   01001101        M
     110,     // 116    04E   01001110        N
     111,     // 117    04F   01001111        O
     112,     // 120    050   01010000        P
     113,     // 121    051   01010001        Q
     114,     // 122    052   01010010        R
     115,     // 123    053   01010011        S
     116,     // 124    054   01010100        T
     117,     // 125    055   01010101        U
     118,     // 126    056   01010110        V
     119,     // 127    057   01010111        W
     120,     // 130    058   01011000        X
     121,     // 131    059   01011001        Y
     122,     // 132    05A   01011010        Z
     91,      // 133    05B   01011011        [    (left/opening bracket)
     92,      // 134    05C   01011100        \    (back slash)
     93,      // 135    05D   01011101        ]    (right/closing bracket)
     94,      // 136    05E   01011110        ^    (caret/circumflex)
     95,      // 137    05F   01011111        _    (underscore)
     96,      // 140    060   01100000        `
     97,      // 141    061   01100001        a
     98,      // 142    062   01100010        b
     99,      // 143    063   01100011        c
    100,      // 144    064   01100100        d
    101,      // 145    065   01100101        e
    102,      // 146    066   01100110        f
    103,      // 147    067   01100111        g
    104,      // 150    068   01101000        h
    105,      // 151    069   01101001        i
    106,      // 152    06A   01101010        j
    107,      // 153    06B   01101011        k
    108,      // 154    06C   01101100        l
    109,      // 155    06D   01101101        m
    110,      // 156    06E   01101110        n
    111,      // 157    06F   01101111        o
    112,      // 160    070   01110000        p
    113,      // 161    071   01110001        q
    114,      // 162    072   01110010        r
    115,      // 163    073   01110011        s
    116,      // 164    074   01110100        t
    117,      // 165    075   01110101        u
    118,      // 166    076   01110110        v
    119,      // 167    077   01110111        w
    120,      // 170    078   01111000        x
    121,      // 171    079   01111001        y
    122,      // 172    07A   01111010        z
    123,      // 173    07B   01111011        {    (left/opening brace)
    124,      // 174    07C   01111100        |    (vertical bar)
    125,      // 175    07D   01111101        }    (right/closing brace)
    126,      // 176    07E   01111110        ~    (tilde)
    127       // 177    07F   01111111      DEL    (delete)
};

/*
 *******************************************************************************
 * Hash Table definitions
 *******************************************************************************
 */

STATIC int name_hash(const char *name, int len);
STATIC int add_hash(hash_table_def_t * ht, const char *name, int len,
		    int data);
STATIC int del_hash(hash_table_def_t * ht, const char *name, int len);
STATIC int nocase_compare(const char *p1, const char *p2, int size);
STATIC int case_compare(const char *p1, const char *p2, int size);
STATIC int lookup_hash(const hash_table_def_t * ht, const char *name,
		       int len, int *data);
STATIC int dealloc_hash_entries(const hash_table_def_t * ht);

STATIC hash_entry_t hash_cache_control_vals[128];

STATIC hash_table_def_t ht_cache_control_vals = {
    hash_cache_control_vals,
    sizeof(hash_cache_control_vals) / sizeof(hash_entry_t),
    0,
    name_hash,
    add_hash,
    nocase_compare,
    lookup_hash,
    dealloc_hash_entries,
    del_hash,
    0
};

STATIC hash_entry_t hash_known_headers_http[128];
STATIC hash_entry_t hash_known_headers_rtsp[128];

STATIC hash_table_def_t ht_known_headers[MIME_PROT_MAX] = {
    {
     /*
      * HTTP 
      */
     hash_known_headers_http,
     sizeof(hash_known_headers_http) / sizeof(hash_entry_t),
     0,
     name_hash,
     add_hash,
     nocase_compare,
     lookup_hash,
     dealloc_hash_entries,
     del_hash,
     0}
    ,
    {
     /*
      * RTSP 
      */
     hash_known_headers_rtsp,
     sizeof(hash_known_headers_rtsp) / sizeof(hash_entry_t),
     0,
     name_hash,
     add_hash,
     nocase_compare,
     lookup_hash,
     dealloc_hash_entries,
     del_hash,
     0}
};

hash_table_def_t ht_base_nocase_hash = {
    0,
    0,
    0,
    name_hash,
    add_hash,
    nocase_compare,
    lookup_hash,
    dealloc_hash_entries,
    del_hash,
    0
};

hash_table_def_t ht_base_case_hash = {
    0,
    0,
    0,
    name_hash,
    add_hash,
    case_compare,
    lookup_hash,
    dealloc_hash_entries,
    del_hash,
    0
};


STATIC name_value_descriptor_t cache_control_data[CT_MAX_DEFS] = {
    {"public", CT_PUBLIC, 0, DT_STRING},
    {"private", CT_PRIVATE, -1 /* optional */ , DT_STRING},
    {"no-cache", CT_NO_CACHE, -1 /* optional */ , DT_STRING},
    {"no-store", CT_NO_STORE, 0, DT_STRING},
    {"no-transform", CT_NO_TRANSFORM, 0, DT_STRING},
    {"must-revalidate", CT_MUST_REVALIDATE, 0, DT_STRING},
    {"proxy-revalidate", CT_PROXY_REVALIDATE, 0, DT_STRING},
    {"max-age", CT_MAX_AGE, 1, DT_INT},
    {"max-stale", CT_MAX_STALE, -1 /* optional */ , DT_INT},
    {"min-fresh", CT_MIN_FRESH, 1, DT_INT},
    {"only-if-cached", CT_ONLY_IF_CACHED, 0, DT_STRING},
    {"s-maxage", CT_S_MAXAGE, -1 /* optional */ , DT_INT}
};

STATIC int have_local_tm = 0;
STATIC struct tm local_tm;


void suppress_unused_func_http_header(void);
STATIC char *CHAR_PTR(const mime_header_t * hd, dataddr_t off, int flag);


typedef struct name_value {
    char *name;
    char *value;
} name_value_t;

/*
 *******************************************************************************
 * name_hash() -- djb2 based hash function
 *
 * Return: computed hash value 
 *******************************************************************************
 */
STATIC int name_hash(const char *name, int len)
{
    // djb2 hash function
    int hash_val = 5381;
    int n;
    for (n = 0; n < len; n++) {
	hash_val = ((hash_val << 5) + hash_val) + map_upper[(int) name[n]];
    }
    return hash_val;
}

/*
 *******************************************************************************
 * del_hash() -- Delete entry from hash table
 *
 * Return: 0 -> Success; != 0 -> Failed
 *******************************************************************************
 */
 STATIC int
 del_hash(hash_table_def_t * ht, const char *name, int len) {
     int hash_val = (*ht->hash_func) (name, len) & (ht->size - 1);
     hash_entry_t *he;
     hash_entry_t *he_first = NULL;
     hash_entry_t *he_prev = NULL;
     int retval = 1;

     he = &ht->ht[hash_val];
     he_first = he;

     (ht->ht_lock)?pthread_mutex_lock(&ht->ht_lock[hash_val]):0;

     if (he->name) {
         do {
             if ((he->name_len == len) && 
                 !(*ht->cmp_func) (he->name, name, len)) {
                 // do del op here
                 if (he == he_first) {
                     free((void *)he->name);
                     memset(he, 0, sizeof(hash_entry_t));
                 }
                 else {
                     he_prev->next = he->next;
                     free((void *)he->name);
                     free(he);
                 }
                 retval = 0;
                 goto del_exit;
             }
             he_prev = he;
             he = he->next;
         } while(he);
     }

del_exit:
     (ht->ht_lock)?pthread_mutex_unlock(&ht->ht_lock[hash_val]):0;

     return retval;
}

/*
 *******************************************************************************
 * add_hash() -- Add entry to hash table
 *
 * Return: 0 -> Success; != 0 -> Failed
 *******************************************************************************
 */
STATIC int
add_hash(hash_table_def_t * ht, const char *name, int len, int data)
{
    int hash_val = (*ht->hash_func) (name, len) & (ht->size - 1);
    hash_entry_t *he;
    hash_entry_t *he_last;

    (ht->ht_lock)?pthread_mutex_lock(&ht->ht_lock[hash_val]):0;

    if (!ht->ht[hash_val].name) {
	ht->ht[hash_val].next = 0;
	ht->ht[hash_val].name = name;
	ht->ht[hash_val].name_len = len;
	ht->ht[hash_val].data = data;
    } else {
	he = ht->ht[hash_val].next;
	he_last = &ht->ht[hash_val];

	while (he) {
	    he_last = he;
	    he = he->next;
	}
	he_last->next =
	    nkn_calloc_type(1, sizeof(hash_entry_t),
			    mod_http_hash_entry_t);
	he_last->next->next = 0;
	he_last->next->name = name;
	he_last->next->name_len = len;
	he_last->next->data = data;
    }

    if (len > ht->max_strlen) {
	ht->max_strlen = len;
    }

    (ht->ht_lock)?pthread_mutex_unlock(&ht->ht_lock[hash_val]):0;

    return 0;
}

/*
 *******************************************************************************
 * lookup_hash() -- Lookup in given hash table
 *
 * Return: 0 -> Success, *data=<entry data>; != 0 -> Failed
 *******************************************************************************
 */
STATIC int
lookup_hash(const hash_table_def_t * ht, const char *name, int len,
	    int *data)
{
    int hash_val;
    hash_entry_t *he;
    int retval = 1;

    if (len > ht->max_strlen) {
	return 1;		// Not found
    }

    hash_val = ht->hash_func(name, len) & (ht->size - 1);
    he = &ht->ht[hash_val];

    (ht->ht_lock)?pthread_mutex_lock(&ht->ht_lock[hash_val]):0;

    if (he->name) {
        do {
            if ((he->name_len == len) &&
                    !(*ht->cmp_func) (he->name, name, len)) {
                *data = he->data;
                retval = 0;
                goto lookup_exit;
            }
            he = he->next;
        } while (he);
    }

lookup_exit:
    (ht->ht_lock)?pthread_mutex_unlock(&ht->ht_lock[hash_val]):0;
    
    return retval;
}

/*
 *******************************************************************************
 * dalloc_hash_entries() -- Deallocate malloc'ed hash table entries
 *
 * Return: 0 -> Success
 *******************************************************************************
 */
STATIC int dealloc_hash_entries(const hash_table_def_t * htd)
{
    int rv = 0;
    int n;
    hash_entry_t *he, *he_next;

    for (n = 0; n < htd->size; n++) {
	he = htd->ht[n].next;
	while (he) {
	    he_next = he->next;
	    free(he);
	    he = he_next;
	}
	htd->ht[n].next = 0;
    }
    return rv;
}

/*
 *******************************************************************************
 * nocase_compare() -- No case compare of two strings of length n.
 *
 * Return: 0 -> equal; != 0 -> not equal
 *******************************************************************************
 */
STATIC int nocase_compare(const char *p1, const char *p2, int size)
{
#define ALIGNED_ADDR(_x) (((long)(_x) & (sizeof(u_int64_t)-1)) == 0)
#define MAP_UPPER(_p) (u_int64_t)((u_int64_t)map_upper[(int)_p[0]] << 56 | \
				(u_int64_t)map_upper[(int)_p[1]] << 48 | \
				(u_int64_t)map_upper[(int)_p[2]] << 40 | \
				(u_int64_t)map_upper[(int)_p[3]] << 32 | \
				(u_int64_t)map_upper[(int)_p[4]] << 24 | \
				(u_int64_t)map_upper[(int)_p[5]] << 16 | \
				(u_int64_t)map_upper[(int)_p[6]] << 8 | \
				(u_int64_t)map_upper[(int)_p[7]])
#define MAP_UPPER_VAL(_p) (u_int64_t) \
			((u_int64_t)map_upper[(_p & 0xFF)] << 56 | \
			(u_int64_t)map_upper[(_p >> 8) & 0xFF] << 48 | \
			(u_int64_t)map_upper[(_p >> 16) & 0xFF] << 40 | \
			(u_int64_t)map_upper[(_p >> 24) & 0xFF] << 32 | \
			(u_int64_t)map_upper[(_p >> 32) & 0xFF] << 24 | \
			(u_int64_t)map_upper[(_p >> 40) & 0xFF] << 16 | \
			(u_int64_t)map_upper[(_p >> 48) & 0xFF] << 8 | \
			(u_int64_t)map_upper[(_p >> 56) & 0xFF])
    int n;
    register u_int64_t d1r;
    register u_int64_t d2r;
    u_int64_t d1;
    u_int64_t d2;

    if (!p1 || !p2 || (size == 0)) {
	return 1;
    }

    if (!ALIGNED_ADDR(p1) || !ALIGNED_ADDR(p2)) {
	for (n = 0; n < size; n++) {
	    if (map_upper[(int) *p1] != map_upper[(int) *p2]) {
		return 2;	// not equal
	    }
	    p1++;
	    p2++;
	}
	return 0;		// equal
    }

    while (size > 0) {
	if (size >= (int) sizeof(u_int64_t)) {
	    d1r = *((u_int64_t *) p1);
	    d2r = *((u_int64_t *) p2);
	    if (MAP_UPPER_VAL(d1r) != MAP_UPPER_VAL(d2r)) {
		return 3;	// not equal
	    }
	    p1 = p1 + sizeof(u_int64_t);
	    p2 = p2 + sizeof(u_int64_t);
	    size -= sizeof(u_int64_t);
	} else {
	    d1 = 0;
	    memcpy(&d1, p1, size);
	    d2 = 0;
	    memcpy(&d2, p2, size);
	    if (MAP_UPPER(((char *) &d1)) != MAP_UPPER(((char *) &d2))) {
		return 4;	// Not equal
	    }
	    return 0;		// equal
	}
    }
    return 0;			// equal
}

/*
 *******************************************************************************
 * case_compare() -- Case compare of two strings of length n.
 *
 * Return: 0 -> equal; != 0 -> not equal
 *******************************************************************************
 */
STATIC int case_compare(const char *p1, const char *p2, int size)
{
    if (!p1 || !p2 || (size == 0)) {
	return 1;
    }
    return strncmp(p1, p2, size) ? 1 : 0;
}


/*
 *******************************************************************************
 * cache_control_data_init() -- Init HTTP cache control hash data
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
STATIC int cache_control_data_init(void)
{
    int n;
    name_value_descriptor_t *ccd;
    int rv;

    for (n = 0; n < CT_MAX_DEFS; n++) {
	ccd = &cache_control_data[n];
	rv = (*ht_cache_control_vals.add_func) (&ht_cache_control_vals,
						ccd->name,
						strlen(ccd->name),
						ccd->id);
	NKN_ASSERT(rv == 0);
    }
    return 0;
}

/*
 *******************************************************************************
 * known_headers_data_init() -- Init HTTP known headers hash data
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
STATIC int known_headers_data_init(void)
{
    int n;
    int prot;
    int max_hdrs;
    mime_header_descriptor_t *hd;
    int rv;

    /*
     * Iterate through protocols
     * Fill up hash tables for respective protocols
     */
    for (prot = MIME_PROT_HTTP; prot < MIME_PROT_MAX; prot++) {
	switch (prot) {
	case MIME_PROT_HTTP:
	    max_hdrs = MIME_HDR_MAX_DEFS;
	    hd = &http_known_headers[0];
	    break;
#ifndef PROTO_HTTP_LIB
	case MIME_PROT_RTSP:
	    max_hdrs = RTSP_HDR_MAX_DEFS;
	    hd = &rtsp_known_headers[0];
	    break;
#endif /* !PROTO_HTTP_LIB */
	default:
	    continue;
	}

	for (n = 0; n < max_hdrs; n++) {

	    rv = (*ht_known_headers[prot].
		  add_func) (&ht_known_headers[prot], hd->name,
			     strlen(hd->name), hd->id);
	    hd++;
	}
	NKN_ASSERT(rv == 0);
    }
    return 0;
}

/*
 *******************************************************************************
 * hash_table_init() -- Initialize defined hash tables
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
STATIC int hash_table_init(void)
{
	known_headers_data_init();
	cache_control_data_init();
	return 0;
}

/*
 *******************************************************************************
 * mapstr2upper() -- Convert string to upper case
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mapstr2upper(const char *in, char *out, int len)
{
    int n;

    if (!in || !out) {
	return 1;
    }
    for (n = 0; n < len; n++) {
	out[n] = map_upper[(int) in[n]];
    }
    return 0;
}

/*
 *******************************************************************************
 * mapstr2lower() -- Convert string to lower case
 *
 *   Return 0 => Success
 *******************************************************************************
 */
int mapstr2lower(const char *in, char *out, int len)
{
    int n;

    if (!in || !out) {
    	return 1;
    }
    for (n = 0; n < len; n++) {
        out[n] = map_lower[(int)in[n]];
    }
    return 0;
}

/*
 *******************************************************************************
 * Fundamental definitions
 *******************************************************************************
 */
void suppress_unused_func_http_header(void)
{
    CHAR_PTR(0, 0, 0);
}

STATIC dataddr_t bad_heap_ptr(mime_header_t * hd, const char *ptr)
{
    DBG_LOG(SEVERE, MOD_HTTPHDRS,
	    "http_header=%p ptr=%p heap=%p heap_size=%ld "
	    "heap_ext=%p heap_ext_size=%d",
	    hd, ptr, hd->heap, sizeof(hd->heap), hd->heap_ext,
	    hd->heap_ext_size);
    NKN_ASSERT(!"bad_heap_ptr()");
    return -1;
}

STATIC dataddr_t bad_ext_ptr(mime_header_t * hd, const char *ptr)
{
    DBG_LOG(SEVERE, MOD_HTTPHDRS,
	    "http_header=%p ptr=%p ext_data=%p ext_data_size=%d",
	    hd, ptr, hd->ext_data, hd->ext_data_size);
    NKN_ASSERT(!"bad_ext_ptr()");
    return -1;
}

/* All dataddr_t values are non-zero, created via MK_POFFSET (pseudo offset) */
#define MK_POFFSET(_off) ((1 << ((sizeof(dataddr_t) * NBBY)-1)) | (_off))
#define MK_OFFSET(_off)  (~(1 << ((sizeof(dataddr_t) * NBBY)-1)) & (_off))

#define IN_LOCAL_HEAP(_hd, _ptr) \
    (((_ptr) >= (_hd)->heap) && ((_ptr) < ((_hd)->heap + sizeof((_hd)->heap))))

#define IN_EXT_HEAP(_hd, _ptr) \
    (((_ptr) >= (_hd)->heap_ext) && \
		((_ptr) < ((_hd)->heap_ext + (_hd)->heap_ext_size)))

#define IN_EXT_DATA(_hd, _ptr) \
    (((_ptr) >= (_hd)->ext_data) && \
		((_ptr) < ((_hd)->ext_data + (_hd)->ext_data_size)))

#define HEAP_OFFSET(_hd, _ptr) \
    (IN_LOCAL_HEAP((_hd), (_ptr)) ? \
		(MK_POFFSET((_ptr) - (_hd)->heap)) :  \
		(IN_EXT_HEAP((_hd), (_ptr)) ? \
		(MK_POFFSET(sizeof((_hd)->heap) + (_ptr) - (_hd)->heap_ext)) : \
			bad_heap_ptr((_hd), (_ptr)) /* Invalid heap pointer */))

#define EXT_DATA_OFFSET(_hd, _ptr) \
    (IN_EXT_DATA((_hd), (_ptr)) ? (MK_POFFSET((_ptr) - (_hd)->ext_data)) : \
		bad_ext_ptr((_hd), (_ptr))  /* Invalid external pointer */)

#define EXT_OFF2CHAR_PTR(_hd, _off) \
    (char *) \
		(((_off) == 0) ? 0 : \
			((_hd)->ext_data + MK_OFFSET((_off))))

#define OFF2PTR(_hd, _off) \
    ((heap_data_t *) \
		(((_off) == 0) ? 0 : \
			(MK_OFFSET((_off)) < sizeof((_hd)->heap) ? \
				(_hd)->heap + MK_OFFSET((_off)) :  \
			(_hd)->heap_ext + (MK_OFFSET((_off))-sizeof((_hd)->heap)))))

#define OFF2CHAR_PTR(_hd, _off) \
    ((char *) \
		(((_off) == 0) ? 0 : \
			(MK_OFFSET((_off)) < sizeof((_hd)->heap) ? \
			(_hd)->heap + MK_OFFSET((_off)) : \
		(_hd)->heap_ext + (MK_OFFSET((_off))-sizeof((_hd)->heap)))))

STATIC char *CHAR_PTR(const mime_header_t * hd, dataddr_t off, int flag)
{
    heap_data_t *hdat = OFF2PTR(hd, off);
    if (hdat->flags & F_VALUE_DATA) {
	NKN_ASSERT(flag == F_VALUE_HEAPDATA_REF);

	return hdat->flags & F_VALUE_HEAPDATA_REF ?
	    OFF2CHAR_PTR(hd, hdat->u.v.value) :
	    EXT_OFF2CHAR_PTR(hd, hdat->u.v.value);

    } else if (hdat->flags & F_NAME_VALUE_DATA) {
	if (flag == F_NAME_HEAPDATA_REF) {
	    return hdat->flags & F_NAME_HEAPDATA_REF ?
		OFF2CHAR_PTR(hd, hdat->u.nv.name) :
		EXT_OFF2CHAR_PTR(hd, hdat->u.nv.name);

	} else if (flag == F_VALUE_HEAPDATA_REF) {
	    return hdat->flags & F_VALUE_HEAPDATA_REF ?
		OFF2CHAR_PTR(hd, hdat->u.nv.value) :
		EXT_OFF2CHAR_PTR(hd, hdat->u.nv.value);

	} else {
	    DBG_LOG(SEVERE, MOD_HTTPHDRS,
		    "Invalid flag=0x%x arg hd=%p hdat=%p", flag, hd, hdat);
	    NKN_ASSERT(!"Invalid flag arg");
	}
    } else {
	DBG_LOG(SEVERE, MOD_HTTPHDRS,
		"Invalid flag=0x%x arg hd=%p hdat=%p", flag, hd, hdat);
	NKN_ASSERT(!"Invalid flag arg");
    }
    return NULL;
}

#define DATA_HEAP_ELEMENTS(_hd, _sz) \
    (_sz) <= sizeof(((heap_data_t *)0)->u.d.data) ? 1 : \
        ((((_sz) - sizeof(((heap_data_t *)0)->u.d.data) + \
	sizeof(heap_data_t) - 1) / sizeof(heap_data_t))+1)

#define DATAOFF_TO_HEAP_ELEMENT(_hd, _off) \
    OFF2PTR((_hd), ((_off) & ~(sizeof(heap_data_t)-1)))

#define IS_WS(_c) (((_c) == ' ') || ((_c) == '\t'))

#define IS_DIGIT(_c) (((_c) >= '0') && ((_c) <= '9'))

#define IS_ALPHA(_c) ((((_c) >= 'A') && ((_c) <= 'Z')) || \
		(((_c) >= 'a') && ((_c) <= 'z')))

#define TSTR(_p, _l, _r)  char *(_r); ( 1 ? ( (_r) = alloca((_l)+1), \
	memcpy((_r), (_p), (_l)), (_r)[(_l)] = '\0' ) : 0 )

/*
 *******************************************************************************
 *
 *		P R I V A T E  F U N C T I O N S
 *
 *******************************************************************************
 */


/*
 *******************************************************************************
 * disable_http_headers_log() -- Disable the logging in the functions in this file
 *
 *******************************************************************************
 */
void disable_http_headers_log(void)
{
    log_enabled = 0;
}

/*
 *******************************************************************************
 * dump_http_header() -- Dump http_header data into given buffer. Buffer is
 *	NULL terminated.
 *
 *  Returns input buf pointer NULL terminated
 *******************************************************************************
 */
const char *dump_http_header(const mime_header_t * hd, char *outbuf,
			     int outbufsz, int pretty)
{
#define OUTBUF(_data, _datalen) { \
    if (((_datalen)+bytesused) >= outbufsz) { \
	DBG("datalen(%d)+bytesused(%d) >= outbufsz(%d)",  \
		(_datalen), bytesused, outbufsz); \
	outbuf[outbufsz-1] = '\0'; \
	return outbuf; \
    } \
    memcpy((void *)&outbuf[bytesused], (_data), (_datalen)); \
    bytesused += (_datalen); \
}
    int rv;
    int token;
    int nth;
    const char *name;
    int namelen;
    const char *data;
    int datalen;
    u_int32_t attrs;
    int hdrcnt;
    int bytesused = 0;

    rv = mime_hdr_get_known(hd, MIME_HDR_X_NKN_METHOD, &data, &datalen, &attrs,
			    &hdrcnt);
    if (rv) {
	datalen = 0;
    }
    // {Method}
    if (pretty)
	OUTBUF("\t", 1);
    OUTBUF(data, datalen);
    OUTBUF(" ", 1);

    rv = mime_hdr_get_known(hd, MIME_HDR_X_NKN_ABS_URL_HOST, &data, &datalen, &attrs,
			    &hdrcnt);
    if (rv) {
    	rv = mime_hdr_get_known(hd, MIME_HDR_HOST, &data, &datalen, &attrs, &hdrcnt);
    }
    if (rv) {
	datalen = 0;
    }
    // http://{host}
    OUTBUF("http://", 7);
    OUTBUF(data, datalen);

    rv = mime_hdr_get_known(hd, MIME_HDR_X_NKN_URI, &data, &datalen, &attrs,
			    &hdrcnt);
    if (rv) {
	datalen = 0;
    }
    OUTBUF(data, datalen);
    if (pretty)
	OUTBUF("\nKnown headers:\n", 16);

    // Dump known headers
    for (token = 0; token < MIME_HDR_MAX_DEFS; token++) {
    	if (!hd->known_header_map[token]) {
	    continue;
	}
	rv = mime_hdr_get_known(hd, token, &data, &datalen, &attrs,
				&hdrcnt);
	if (!rv) {
	    if (pretty)
		OUTBUF("\t", 1);
	    OUTBUF(http_known_headers[token].name,
		   http_known_headers[token].namelen);
	    if (pretty) {
		OUTBUF(":", 1);
	    } else {
		OUTBUF("\t", 1);
	    }
	    OUTBUF(data, datalen);
	} else {
	    continue;
	}
	for (nth = 1; nth < hdrcnt; nth++) {
	    rv = mime_hdr_get_nth_known(hd, token, nth, &data, &datalen,
					&attrs);
	    if (!rv) {
		OUTBUF(",", 1);
		OUTBUF(data, datalen);
	    }
	}
	OUTBUF("\n", 1);
    }
    if (pretty)
	OUTBUF("Unknown headers:\n", 17);

    // Dump unknown headers
    nth = 0;
    while ((rv = mime_hdr_get_nth_unknown(hd, nth, &name, &namelen,
					  &data, &datalen, &attrs)) == 0) {
	if (pretty)
	    OUTBUF("\t", 1);
	OUTBUF(name, namelen);
	if (pretty) {
	    OUTBUF(":", 1);
	} else {
	    OUTBUF("\t", 1);
	}
	OUTBUF(data, datalen);
	OUTBUF("\n", 1);
	nth++;
    }
    if (pretty)
	OUTBUF("Namevalue headers:\n", 19);

    // Dump name/value headers
    nth = 0;
    while ((rv = mime_hdr_get_nth_namevalue(hd, nth, &name, &namelen,
					    &data, &datalen,
					    &attrs)) == 0) {
	if (pretty)
	    OUTBUF("\t", 1);
	OUTBUF(name, namelen);
	if (pretty) {
	    OUTBUF(":", 1);
	} else {
	    OUTBUF("\t", 1);
	}
	OUTBUF(data, datalen);
	OUTBUF("\n", 1);
	nth++;
    }
    OUTBUF("\0", 1);

    return outbuf;
#undef OUTBUF
}


/*
 *******************************************************************************
 * alloc_heap_free_pool() -- allocate data from free elements in the 
 *				 local or external heap
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
STATIC
    int alloc_heap_free_pool(mime_header_t * hd, unsigned int elements,
			     dataddr_t * ret_offset, int *local_heap)
{
    heap_data_t *phd;
    heap_data_t *phd_free;
    unsigned int contiguous_free_elements;
    char *heap_start;
    char *heap_end;
    int heaps_to_scan;

    if (!hd->heap_free_index) {
	return 1;		// No allocated elements to scan
    }

    for (heaps_to_scan = 0; heaps_to_scan < 2; heaps_to_scan++) {
	phd_free = 0;
	contiguous_free_elements = 0;

	if (!heaps_to_scan) {
	    /* Scan internal heap */
	    heap_start = hd->heap;
	    phd = (heap_data_t *) heap_start;

	    if (hd->heap_free_index >=
		(int) (sizeof(hd->heap) / sizeof(heap_data_t))) {
		heap_end = hd->heap + sizeof(hd->heap);
	    } else {
		heap_end = hd->heap +
		    (hd->heap_free_index * sizeof(heap_data_t));
	    }
	} else {
	    /* Scan external heap */
	    heap_start = hd->heap_ext;
	    phd = (heap_data_t *) heap_start;

	    if (hd->heap_free_index >
		(int) (sizeof(hd->heap) / sizeof(heap_data_t))) {
		heap_end = hd->heap_ext +
		    (hd->heap_free_index -
		     (sizeof(hd->heap) / sizeof(heap_data_t))) *
		    sizeof(heap_data_t);
	    } else {
		break;
	    }
	}

	while ((char *) phd < heap_end) {
	    if (phd->flags == 0) {	// Free element
		if (phd_free) {
		    contiguous_free_elements++;
		} else {
		    contiguous_free_elements = 1;
		    phd_free = phd;
		}

		if (elements == contiguous_free_elements) {
		    hd->heap_frees -= elements;
		    *ret_offset = (char *) phd_free - heap_start;
		    *local_heap = heaps_to_scan ? 0 : 1;
		    return 0;	// Requested free elements found
		}
		phd++;
	    } else {
		phd_free = 0;
		contiguous_free_elements = 0;

		if (phd->flags & (F_VALUE_DATA | F_NAME_VALUE_DATA)) {
		    phd++;
		} else if (phd->flags & F_DATA) {
		    phd +=
			DATA_HEAP_ELEMENTS(hd,
					   (unsigned int) (phd->u.d.size));
		} else {
		    DBG("Unknown data heap=%d type=0x%lx, "
			"terminating free list scan",
			heaps_to_scan, phd->flags);
		    return 2;
		}
	    }
	}
    }
    return 3;			// No free elements found
}

/*
 *******************************************************************************
 * alloc_heap() -- allocate data from local or external heap
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
STATIC
    int alloc_heap(mime_header_t * hd, int size, dataddr_t * ret_offset,
		   int *local_heap)
{
    int rv = 0;
    unsigned int elements;
    char *p;
    int heap_ext_index;

    if (size) {
	/*
	 * Allocate a F_DATA element
	 */
	if (size > MAX_HEAP_ELEMENTSZ) {
	    DBG("size (%d) > MAX_HEAP_ELEMENTSZ (%d)",
		size, MAX_HEAP_ELEMENTSZ);
	    rv = 1;
	    goto exit;
	}
	elements = DATA_HEAP_ELEMENTS(hd, (unsigned int) size);
    } else {
	elements = 1;
    }

    /*
     *  Use free elements if available
     */
    if ((hd->heap_frees >= (int) elements) &&
	(alloc_heap_free_pool(hd, elements, ret_offset, local_heap) ==
	 0)) {
	return 0;
    }

    if ((hd->heap_free_index + elements) <=
	(sizeof(hd->heap) / sizeof(heap_data_t))) {
	/*
	 * Local heap allocation
	 */
	*ret_offset = ((hd->heap_free_index) * sizeof(heap_data_t));
	*local_heap = 1;
    } else {
	/*
	 * External heap allocation
	 */
	if ((unsigned int) hd->heap_free_index <
	    (sizeof(hd->heap) / sizeof(heap_data_t))) {
	    /*
	     * Allocation cannot be satisfied in local heap even
	     * though free elements exist, waste free elements and
	     * force use of external heap.
	     */
	    hd->heap_free_index = sizeof(hd->heap) / sizeof(heap_data_t);
	}
	heap_ext_index = hd->heap_free_index -
	    (sizeof(hd->heap) / sizeof(heap_data_t));

	while (1) {
	    if ((heap_ext_index + elements) <=
		(hd->heap_ext_size / sizeof(heap_data_t))) {
		*ret_offset = ((heap_ext_index) * sizeof(heap_data_t));
		*local_heap = 0;
		break;
	    } else {
		if (!hd->heap_ext) {
		    hd->heap_ext =
			nkn_malloc_type(MIME_HEADER_HEAPSIZE,
					mod_http_heap_ext);
		    if (hd->heap_ext) {
			hd->heap_ext_size = MIME_HEADER_HEAPSIZE;

			/** Note nkn_malloc_type, should be infrequent **/
			DBG("Note: heap_ext nkn_malloc_type'ed, "
			    "hd=%p heap_ext=%p heap_ext_size=%d",
			    hd, hd->heap_ext, hd->heap_ext_size);
		    } else {
			DBG("nkn_malloc_type(%ld) failed",
			    MIME_HEADER_HEAPSIZE);
			rv = 2;
			break;
		    }
		} else {
		    if ((hd->heap_ext_size * 2) > MAX_EXT_HEAPSZ) {
			DBG("realloc() > MAX_EXT_HEAPSZ (%d)",
			    MAX_EXT_HEAPSZ);
			rv = 3;
			break;
		    }
		    p = nkn_realloc_type(hd->heap_ext, hd->heap_ext_size * 2,
					 mod_http_heap_ext);
		    if (p) {
			hd->heap_ext = p;
			hd->heap_ext_size *= 2;

			/** Note realloc, should be infrequent **/
			DBG("Note: heap_ext realloc'ed, "
			    "hd=%p heap_ext=%p heap_ext_size=%d",
			    hd, hd->heap_ext, hd->heap_ext_size);
		    } else {
			DBG("realloc(%d) failed", hd->heap_ext_size * 2);
			rv = 4;
			break;
		    }
		}
	    }
	}
    }
    hd->heap_free_index += elements;

  exit:
    return rv;
}

/*
 *******************************************************************************
 * get_heap_element() -- allocate and initialize heap element
 *
 *  Returns !=0 => Success
 *******************************************************************************
 */
STATIC dataddr_t get_heap_element(mime_header_t * hd, int type, int size)
{
    dataddr_t ret_offset = 0;
    int local_heap = 0;
    int rv;
    dataddr_t hptr;
    heap_data_t *pheap_data;

    switch (type) {
    case F_VALUE_DATA:
    case F_NAME_VALUE_DATA:
    case F_DATA:
	rv = alloc_heap(hd, (type == F_DATA) *  size,
			&ret_offset, &local_heap);
	if (rv) {
	    DBG("alloc_heap() failed rv=%d", rv);
	    return 0;
	}
	hptr =
	    MK_POFFSET((local_heap ? 0 : sizeof(hd->heap)) + ret_offset);
	pheap_data = OFF2PTR(hd, hptr);
	memset((void *) pheap_data, 0, sizeof(heap_data_t));
	pheap_data->flags = type;
	if (type == F_DATA) {
	    pheap_data->u.d.size = size;
	}
	break;

    default:
	DBG("Undefined type=%d", type);
	hptr = 0;
	break;
    }
    return hptr;
}

/*
 *******************************************************************************
 * delete_heap_element() -- Mark heap element as deleted
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
STATIC int delete_heap_element(mime_header_t * hd, heap_data_t * heap_data)
{
    int elements;

    if (heap_data->flags & (F_VALUE_DATA | F_NAME_VALUE_DATA)) {
	heap_data->flags = 0;
	hd->heap_frees++;
    } else {
	NKN_ASSERT(heap_data->flags & F_DATA);
	elements =
	    DATA_HEAP_ELEMENTS(hd, (unsigned int) (heap_data->u.d.size));
	hd->heap_frees += elements;
	while (elements) {
	    (heap_data + (elements - 1))->flags = 0;
	    elements--;
	}
    }
    return 0;
}

/*
 *******************************************************************************
 * compute_data_offset() -- Given a data ptr, compute the corresponding offset.
 *
 *  If the given data ptr does not reside in ext_data, allocate a raw_data
 *  element in the heap and copy the data into it and return the corresponding
 *  offset.
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
STATIC
    int compute_data_offset(mime_header_t * hd, const char *data, int len,
			    dataddr_t * off, dataddr_t * heap_data,
			    int force_heap_alloc)
{
    int rv = 0;
    dataddr_t data_hdptr = 0;
    heap_data_t *pheap_data;

    if (!force_heap_alloc && IN_EXT_DATA(hd, data)) {
	*off = EXT_DATA_OFFSET(hd, data);
	*heap_data = 0;
    } else {
	/* Copy to heap */
	data_hdptr = get_heap_element(hd, F_DATA, len);
	if (!data_hdptr) {
	    DBG("get_heap_element(%p, F_DATA, %d) failed", hd, len);
	    rv = 1;
	    goto err_exit;
	}
	pheap_data = OFF2PTR(hd, data_hdptr);
	memcpy((void *) pheap_data->u.d.data,
	       (void *) data, len);
	*off = HEAP_OFFSET(hd, pheap_data->u.d.data);
	*heap_data = data_hdptr;
    }
    return rv;

  err_exit:

    return rv;
}

/*
 *******************************************************************************
 * add_name_value() -- Add given name/value element to given list
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
STATIC
    int add_name_value(mime_header_t * hd, dataddr_t * list_head,
		       uint16_t * hdrcnt, const char *name, int namelen,
		       const char *val, int vallen, int force_heap_alloc,
		       u_int8_t user_attr)
{
    int rv = 0;
    dataddr_t namedata_hdptr = 0;
    dataddr_t data_hdptr = 0;
    dataddr_t nameval_hdptr = 0;
    dataddr_t hdptr = 0;
    heap_data_t *pheap_data;
    heap_data_t *pheap_data_nameval;

    dataddr_t name_offset;
    dataddr_t val_offset;
    const char *p;

    int create_name_value = 0;

    /*
     * Compute value offset
     */
    rv = compute_data_offset(hd, val, vallen, &val_offset, &data_hdptr,
			     force_heap_alloc);
    if (rv) {
	DBG("compute_data_offset() failed rv=%d", rv);
	rv = 1;
	goto err_exit;
    }

    if (*list_head) {
	hdptr = *list_head;

	while (hdptr) {
            pheap_data = OFF2PTR(hd, hdptr);
	    p = pheap_data->flags & F_NAME_HEAPDATA_REF ?
		OFF2CHAR_PTR(hd, pheap_data->u.nv.name) :
		EXT_OFF2CHAR_PTR(hd, pheap_data->u.nv.name);
	    NKN_ASSERT(p == CHAR_PTR(hd, hdptr, F_NAME_HEAPDATA_REF));

	    if ((pheap_data->u.nv.name_len == namelen) &&
		!strncasecmp(name, p, namelen)) {
		/* 
		 * Replace 
		 */
		if (pheap_data->flags & F_VALUE_HEAPDATA_REF) {
		    delete_heap_element(hd,
					DATAOFF_TO_HEAP_ELEMENT(hd,
								pheap_data->
								u.nv.
								value));
		    pheap_data->flags &= ~F_VALUE_HEAPDATA_REF;
		}
		pheap_data->flags |=
		    data_hdptr ? F_VALUE_HEAPDATA_REF : 0;
		pheap_data->flags &= ~F_USER_ATTR_MASK;
		pheap_data->flags |= USERATTR2ATTR(user_attr);

		pheap_data->u.nv.value = val_offset;
		pheap_data->u.nv.value_len = vallen;
		break;
	    }

	    if (pheap_data->u.nv.next) {
		hdptr = pheap_data->u.nv.next;
	    } else {
		create_name_value = 1;
		break;
	    }
	}
    } else {
	create_name_value = 1;
    }

    if (create_name_value) {
	/* Allocate name_value data */
	nameval_hdptr = get_heap_element(hd, F_NAME_VALUE_DATA, 0);
	if (!nameval_hdptr) {
	    rv = 2;
	    goto err_exit;
	}

	rv = compute_data_offset(hd, name, namelen,
				 &name_offset, &namedata_hdptr,
				 force_heap_alloc);
	if (rv) {
	    DBG("compute_data_offset() failed rv=%d", rv);
	    rv = 3;
	    goto err_exit;
	}
	pheap_data_nameval = OFF2PTR(hd, nameval_hdptr);
	pheap_data_nameval->u.nv.name = name_offset;
	pheap_data_nameval->u.nv.name_len = namelen;

	pheap_data_nameval->u.nv.value = val_offset;
	pheap_data_nameval->u.nv.value_len = vallen;

	pheap_data_nameval->flags |=
	    namedata_hdptr ? F_NAME_HEAPDATA_REF : 0;
	pheap_data_nameval->flags |=
	    data_hdptr ? F_VALUE_HEAPDATA_REF : 0;
	pheap_data_nameval->flags &= ~F_USER_ATTR_MASK;
	pheap_data_nameval->flags |= USERATTR2ATTR(user_attr);

	if (*list_head) {
	    OFF2PTR(hd, hdptr)->u.nv.next = nameval_hdptr;
	} else {
	    *list_head = nameval_hdptr;
	}
	(*hdrcnt)++;
    }

    return rv;

  err_exit:

    if (namedata_hdptr) {
	delete_heap_element(hd, OFF2PTR(hd, namedata_hdptr));
    }
    if (data_hdptr) {
	delete_heap_element(hd, OFF2PTR(hd, data_hdptr));
    }
    if (nameval_hdptr) {
	delete_heap_element(hd, OFF2PTR(hd, nameval_hdptr));
    }
    return rv;
}

/*
 *******************************************************************************
 * delete_name_value_by_name() -- Delete name/value element from given list
 *   using the given name.
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
STATIC
    int delete_name_value_by_name(mime_header_t * hd,
				  dataddr_t * list_head, uint16_t * hdrcnt,
				  const char *name, int namelen)
{
    int rv = 0;
    dataddr_t hdptr = 0;
    dataddr_t last_hdptr = 0;
    const char *p;
    heap_data_t *pheap_data;

    if (*list_head) {
	hdptr = *list_head;
	last_hdptr = hdptr;

	while (hdptr) {
	    pheap_data = OFF2PTR(hd, hdptr);
	    p = pheap_data->flags & F_NAME_HEAPDATA_REF ?
		OFF2CHAR_PTR(hd, pheap_data->u.nv.name) :
		EXT_OFF2CHAR_PTR(hd, pheap_data->u.nv.name);
	    NKN_ASSERT(p == CHAR_PTR(hd, hdptr, F_NAME_HEAPDATA_REF));

	    if ((pheap_data->u.nv.name_len == namelen) &&
		!strncasecmp(name, p, namelen)) {
		/* 
		 * Delete
		 */
		if (pheap_data->flags & F_NAME_HEAPDATA_REF) {
		    delete_heap_element(hd,
					DATAOFF_TO_HEAP_ELEMENT(hd,
								pheap_data->
								u.nv.
								name));
		    pheap_data->flags &= ~F_NAME_HEAPDATA_REF;
		}

		if (pheap_data->flags & F_VALUE_HEAPDATA_REF) {
		    delete_heap_element(hd,
					DATAOFF_TO_HEAP_ELEMENT(hd,
								pheap_data->
								u.nv.
								value));
		    pheap_data->flags &= ~F_VALUE_HEAPDATA_REF;
		}

		if (last_hdptr == hdptr) {
		    *list_head = pheap_data->u.nv.next;
		} else {
		    OFF2PTR(hd, last_hdptr)->u.nv.next =
			pheap_data->u.nv.next;
		}
		delete_heap_element(hd, pheap_data);
		(*hdrcnt)--;
		break;
	    }

	    if (pheap_data->u.nv.next) {
		last_hdptr = hdptr;
		hdptr = pheap_data->u.nv.next;
	    } else {
		rv = 1;		/* No match */
		break;
	    }
	}
    } else {
	rv = 2;			/* No elements */
    }
    return rv;
}

/*
 *******************************************************************************
 * find_name_value_by_name() -- Lookup name/value element from given list
 *   using the given name.
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
STATIC
    int find_name_value_by_name(const mime_header_t * hd,
				const dataddr_t * list_head,
				const char *name, int namelen,
				dataddr_t * heap_data)
{
    int rv = 0;
    dataddr_t hdptr = 0;
    const char *p;
    heap_data_t *pheap_data;

    if (*list_head) {
	hdptr = *list_head;

	while (hdptr) {
	    pheap_data = OFF2PTR(hd, hdptr);
	    p = pheap_data->flags & F_NAME_HEAPDATA_REF ?
		OFF2CHAR_PTR(hd, pheap_data->u.nv.name) :
		EXT_OFF2CHAR_PTR(hd, pheap_data->u.nv.name);
	    NKN_ASSERT(p == CHAR_PTR(hd, hdptr, F_NAME_HEAPDATA_REF));

	    if ((pheap_data->u.nv.name_len == namelen) &&
		!strncasecmp(name, p, namelen)) {
		/* 
		 * Found
		 */
		*heap_data = hdptr;
		break;
	    }

	    if (pheap_data->u.nv.next) {
		hdptr = pheap_data->u.nv.next;
	    } else {
		rv = 1;		/* No match */
		break;
	    }
	}
    } else {
	rv = 2;			/* No elements */
    }
    return rv;
}

/*
 *******************************************************************************
 * find_nth_name_value() -- Return the nth name/value element from 
 *   the given list
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
STATIC
    int find_nth_name_value(const mime_header_t * hd,
			    const dataddr_t * list_head, int nth_element,
			    dataddr_t * heap_data)
{
    int rv = 0;
    int elements = 0;
    dataddr_t hdptr = 0;
    heap_data_t *pheap_data;

    if (*list_head) {
	hdptr = *list_head;
	while (hdptr) {
	    elements++;
	    if (nth_element == (elements - 1)) {
		*heap_data = hdptr;
		break;
	    }
   	    pheap_data = OFF2PTR(hd, hdptr);
	    if (pheap_data->u.nv.next) {
		hdptr = pheap_data->u.nv.next;
	    } else {
		rv = 1;		// Not found
		break;
	    }
	}
    } else {
	rv = 1;			/* No elements */
    }
    return rv;
}

/*
 *******************************************************************************
 * get_nth_list_element() -- Return the nth name/value data
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
STATIC
    int get_nth_list_element(const mime_header_t * hd,
			     const dataddr_t * list_head, int arg_num,
			     const char **name, int *name_len,
			     const char **val, int *val_len,
			     u_int32_t * attr)
{
    int rv;
    dataddr_t namevalptr = 0;
    heap_data_t *pheap_data_nameval;

    rv = find_nth_name_value(hd, list_head, arg_num, &namevalptr);
    if (!rv) {
	pheap_data_nameval = OFF2PTR(hd, namevalptr);
	*name = pheap_data_nameval->flags & F_NAME_HEAPDATA_REF ?
	    OFF2CHAR_PTR(hd, pheap_data_nameval->u.nv.name) :
	    EXT_OFF2CHAR_PTR(hd, pheap_data_nameval->u.nv.name);
	NKN_ASSERT(*name == CHAR_PTR(hd, namevalptr, F_NAME_HEAPDATA_REF));

	*name_len = pheap_data_nameval->u.nv.name_len;
	*val = pheap_data_nameval->flags & F_VALUE_HEAPDATA_REF ?
	    OFF2CHAR_PTR(hd, pheap_data_nameval->u.nv.value) :
	    EXT_OFF2CHAR_PTR(hd, pheap_data_nameval->u.nv.value);
	NKN_ASSERT(*val == CHAR_PTR(hd, namevalptr, F_VALUE_HEAPDATA_REF));

	*val_len = pheap_data_nameval->u.nv.value_len;
	*attr = pheap_data_nameval->flags;
    } else {
	DBG("find_nth_name_value() failed rv=%d", rv);
	rv = 2;
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_get_known_nth_value() -- Return the nth value data associated with
 *   the given known header.
 *
 *  Returns 0 => Success
 *******************************************************************************
 */
STATIC
int mime_hdr_get_known_nth_value(const mime_header_t * hd, int token,
				     int nth_value, const char **data,
				     int *len, u_int32_t * attributes)
{
    int rv = 0;
    int n = 0;
    dataddr_t valptr;
    heap_data_t *pheap_data_val;

    if ((token < MIME_HDR_MAX_DEFS) && hd->known_header_map[token]) {
	valptr = hd->known_header_map[token];

	while (valptr) {
	    pheap_data_val = OFF2PTR(hd, valptr);
	    if (n == nth_value) {
		*data = pheap_data_val->flags & F_VALUE_HEAPDATA_REF ?
		    OFF2CHAR_PTR(hd, pheap_data_val->u.v.value) :
		    EXT_OFF2CHAR_PTR(hd, pheap_data_val->u.v.value);
		NKN_ASSERT(*data ==
			   CHAR_PTR(hd, valptr, F_VALUE_HEAPDATA_REF));

		*len = pheap_data_val->u.v.value_len;
		*attributes = pheap_data_val->flags;
		break;
	    }

	    if (pheap_data_val->u.v.value_next) {
		valptr = pheap_data_val->u.v.value_next;
	    } else {
#if 0
		DBG_LOG(MSG2, MOD_HTTPHDRS, "Nth=%d not found, token=%d",
			nth_value, token);
#endif
		rv = 1;		/* Not found */
		break;
	    }
	    n++;
	}

    } else {
#if 0
	DBG_LOG(MSG2, MOD_HTTPHDRS, "No elements, token=%d", token);
#endif
	rv = 2;			/* No elements */
    }
    return rv;
}

/*
 *******************************************************************************
 * atol_len() -- [ptr, len] atoi for long
 *******************************************************************************
 */
long atol_len(const char *str, int len)
{
    long sign = 1;
    long val = 0;

    while (len && (*str == ' '))
	--len, str++;

    if (*str == '-') {
	sign = -1;
	len--;
	str++;
    }

    while (len) {
	if (('0' <= *str) && (*str <= '9')) {
	    val = val * 10 + (*str - '0');
	    len--;
	    str++;
	} else {
	    break;
	}
    }
    return sign * val;
}

/*
 *******************************************************************************
 * hextol_len() -- [ptr, len] hex ascii to long
 *******************************************************************************
 */
long hextol_len(const char *str, int len)
{
    long sign = 1;
    long val = 0;

    while (len && (*str == ' '))
	--len, str++;

    if (*str == '-') {
	sign = -1;
	len--;
	str++;
    }

    while (len) {
	if (('0' <= *str) && (*str <= '9')) {
	    val = val * 16 + (*str - '0');
	    len--;
	    str++;
	} else if (('a' <= *str) && (*str <= 'f')) {
	    val = val * 16 + (10 + (*str - 'a'));
	    len--;
	    str++;
	} else if (('A' <= *str) && (*str <= 'F')) {
	    val = val * 16 + (10 + (*str - 'A'));
	    len--;
	    str++;
	} else {
	    break;
	}
    }
    return sign * val;
}

/*
 *******************************************************************************
 * extract_suffix() -- [ptr, len] extract suffix component.
 *******************************************************************************
 */
int extract_suffix(const char *str, int slen, char *outbuf, int outbuflen,
                   char suffix_delimiter, int lowercase_on_output)
{
    int retval = 0;
    char *p;
    int suffix_len;
    char *map_table;
    int cnt;

    if (!str || !slen || !outbuf || !outbuflen) {
    	return 1; // Invalid input
    }

    p = (char *)memrchr(str, suffix_delimiter, slen);
    if (p) {
    	suffix_len = slen - ((p - str) + 1);
    	if (suffix_len) {
	    if ((suffix_len + 1) <= outbuflen) {
	    	p++;
	    	map_table = lowercase_on_output ? map_lower : map_upper;
		for (cnt = 0; cnt < suffix_len; cnt++) {
		    outbuf[cnt] = map_table[(int)*p];
		    p++;
		}
		outbuf[cnt] = '\0';
	    }
	} else {
	    retval = 2; // No suffix data
	}
    } else {
    	retval = 3; // No suffix delimiter
    }
    return retval;
}

/*
 *******************************************************************************
 * atoll_len() -- [ptr, len] atoi for long long
 *******************************************************************************
 */
long long atoll_len(const char *str, int len)
{
    long long sign = 1;
    long long val = 0;

    while (len && (*str == ' '))
	--len, str++;

    if (*str == '-') {
	sign = -1;
	len--;
	str++;
    }

    while (len) {
	if (('0' <= *str) && (*str <= '9')) {
	    val = val * 10 + (*str - '0');
	    len--;
	    str++;
	} else {
	    break;
	}
    }
    return sign * val;
}

/*
 *******************************************************************************
 * get_hex_value() -- Convert char to hex equivalent else return char
 *
 *   Return converted char
 *******************************************************************************
 */
STATIC unsigned char get_hex_value(char c)
{
    if (c >= '0' && c <= '9') {
	return c - '0';
    } else if (c >= 'a' && c <= 'f') {
	return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
	return c - 'A' + 10;
    } else {
	return c;
    }
}

/* RFC 2396 - 2. URI Characters and Escape Sequences
 * uric          = reserved | unreserved | escaped
 */

/* RFC 2396 - 2.2 Reserved Characters 
 * reserved    = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" | "$" | ","
 */

/* RFC 2396 - 2.3. Unreserved Characters
 * unreserved  = alphanum | mark
 * mark        = "-" | "_" | "." | "!" | "~" | "*" | "'" | "(" | ")"
 * alphanum      = alpha | digit
 * alpha         = lowalpha | upalpha
 * lowalpha = "a" | "b" | "c" | "d" | "e" | "f" | "g" | "h" | "i" |
 *	      "j" | "k" | "l" | "m" | "n" | "o" | "p" | "q" | "r" |
 *            "s" | "t" | "u" | "v" | "w" | "x" | "y" | "z"
 * upalpha  = "A" | "B" | "C" | "D" | "E" | "F" | "G" | "H" | "I" |
 *            "J" | "K" | "L" | "M" | "N" | "O" | "P" | "Q" | "R" |
 *	      "S" | "T" | "U" | "V" | "W" | "X" | "Y" | "Z"
 * digit    = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" |
 *	      "8" | "9"
 */

STATIC int is_escaped_base(char c)
{
    switch (c) {
	/* reserved */
    case ';': case '/': case '?': case ':': case '@':
    case '&': case '=': case '+': case '$': case ',':
	/* mark */
    case '-': case '_': case '.': case '!': case '~':
    case '*': case '\'': case '(': case ')':
	/* lowalpha */
    case 'a': case 'b': case 'c': case 'd': case 'e':
    case 'f': case 'g': case 'h': case 'i': case 'j':
    case 'k': case 'l': case 'm': case 'n': case 'o':
    case 'p': case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x': case 'y': 
    case 'z':
	/* upalpha */
    case 'A': case 'B': case 'C': case 'D': case 'E':
    case 'F': case 'G': case 'H': case 'I': case 'J':
    case 'K': case 'L': case 'M': case 'N': case 'O':
    case 'P': case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X': case 'Y':
    case 'Z':
	/* digit */
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
	return 0;
    default:
	break;
    }
    return 1;
}

/** 
 * @brief Variable identifies that escape init is done
 */
static int init_escape_done = 0;

/** 
 * @brief Escape table
 */
static int escape_table[0xFF+1];

/** 
 * @brief initialize the lookup table for escaped chars
 * 
 * @return always 1 
 */
STATIC int init_is_escaped(void ){
    int i;
    for (i=0; i<0xFF; i++){
	escape_table[i] = is_escaped_base(i);
    }
    escape_table[i] = 0;
    init_escape_done = 1;
    return 1;
}

/** 
 * @brief Lookuptable implementation of is_escaped
 *	First Call to this function will initialize the table
 *	if it is not initialized.
 * 
 * @param c - Character To be tested
 * 
 * @return  - 1 - c is to be escaped
 *	      0 - c is not to be escaped
 */
STATIC inline int is_escaped(unsigned char c){
    return (init_escape_done ? escape_table[c] : 
	    (init_is_escaped()? escape_table[c]:0));
}

/*
 *******************************************************************************
 * unescape_str() -- Unescape given string
 *
 *   if (outbuf == 0) malloc buffer
 *
 *   Return pointer to memory containing unescaped string (NULL terminated)
 *******************************************************************************
 */
STATIC
    char *unescape_str(const char *p, int len, int query_str, char *outbuf,
		       int outbufsz,
		       int *outbytesused /* Not including NULL */ )
{
    int n;
    int outbuf_ix;
    unsigned char cval1 = 0;
    unsigned char cval2;

    enum { GETCHAR, HEXCHAR_1, HEXCHAR_2 };
    int state;

    state = GETCHAR;

    if (!outbuf) {
	outbufsz = len + 1;
	outbuf = nkn_malloc_type(outbufsz, mod_http_charbuf);
    }
    outbuf_ix = 0;

    for (n = 0; n < len; n++) {
	if (outbuf_ix >= outbufsz) {
	    DBG("outbuf size exceeded, len=%d outbufsz=%d", len, outbufsz);
	    outbuf_ix--;	/* Add premature NULL and exit */
	    goto exit;
	}
	switch (state) {
	case GETCHAR:
	    if (query_str && (p[n] == '+')) {
		outbuf[outbuf_ix++] = p[n];
	    } else if (p[n] == '%') {
		state = HEXCHAR_1;
	    } else {
		outbuf[outbuf_ix++] = p[n];
	    }
	    break;

	case HEXCHAR_1:
	    cval1 = get_hex_value(p[n]);
	    if (p[n] == cval1) {	/* Not hex */
		outbuf[outbuf_ix++] = '%';
		n--;
		state = GETCHAR;
	    } else {
		state = HEXCHAR_2;
	    }
	    break;

	case HEXCHAR_2:
	    cval2 = get_hex_value(p[n]);
	    if (p[n] == cval2) {	/* Not hex */
		outbuf[outbuf_ix++] = '%';
		n -= 2;
	    } else {
		/* bug 1188 Removing %00 */
		if(cval1 | cval2){
		    outbuf[outbuf_ix++] = (cval1 << 4) | cval2;
		}
	    }
	    state = GETCHAR;
	    break;

	default:
	    NKN_ASSERT(!"Should never happen.");
	    break;
	}
    }
  exit:
    outbuf[outbuf_ix++] = '\0';
    if (outbytesused) {
	*outbytesused = outbuf_ix;
    }
    return outbuf;
}

/*
 *******************************************************************************
 * add_query_namevalue() -- Add query name/value header, if it exists the 
 *   current value is overwritten
 *
 *   Return 0 => Success
 *******************************************************************************
 */
STATIC
    int add_query_namevalue(mime_header_t * hd, const char *name,
			    int namelen, const char *val, int vallen)
{
    int rv;
    rv = add_name_value(hd, &hd->query_string_head,
			&hd->cnt_querystring_headers, name, namelen,
			val, vallen, 0, 0);
    if (rv) {
	DBG("add_name_value() failed rv=%d", rv);
	rv = 1;
    }
    return rv;
}

#ifdef UNUSED_FOR_NOW
/*
 *******************************************************************************
 * replace_heap_value_data() -- Replace heap_value_data 
 *
 *   Return 0 => Success
 *******************************************************************************
 */
STATIC
    int replace_heap_value_data(mime_header_t * hd, dataddr_t hpd,
				const char *data, int len)
{
    dataddr_t hpd_d;
    dataddr_t t_hpd;
    dataddr_t t_hpd_next;
    heap_data_t *pheap_data_hpd;
    heap_data_t *pheap_data_t_hpd;
    heap_data_t *pheap_data_hpd_d;

    pheap_data_hpd = OFF2PTR(hd, hpd);
    if (pheap_data_hpd->flags & F_VALUE_HEAPDATA_REF) {
	delete_heap_element(hd,
			    DATAOFF_TO_HEAP_ELEMENT(hd,
						    pheap_data_hpd->u.v.
						    value));
	pheap_data_hpd->flags &= ~F_VALUE_HEAPDATA_REF;
    }

    /* Delete all elements in the value_next chain */
    if (pheap_data_hpd->u.v.value_next) {
	t_hpd = pheap_data_hpd->u.v.value_next;
	while (t_hpd) {
	    pheap_data_t_hpd = OFF2PTR(hd, t_hpd);
	    if (pheap_data_t_hpd->u.v.value_next) {
		t_hpd_next = pheap_data_t_hpd->u.v.value_next;
	    } else {
		t_hpd_next = 0;
	    }
	    if (pheap_data_t_hpd->flags & F_VALUE_HEAPDATA_REF) {
		delete_heap_element(hd,
				    DATAOFF_TO_HEAP_ELEMENT(hd,
							    pheap_data_t_hpd->
							    u.v.value));
		pheap_data_t_hpd->flags &= ~F_VALUE_HEAPDATA_REF;
	    }
	    delete_heap_element(hd, pheap_data_t_hpd);

	    t_hpd = t_hpd_next;
	}
    }

    hpd_d = get_heap_element(hd, F_DATA, len);
    if (!hpd_d) {
	DBG("get_heap_element(F_DATA, %d) failed", len);
	return 1;
    }
    pheap_data_hpd_d = OFF2PTR(hd, hpd_d);
    memcpy((void *) &(pheap_data_hpd_d->u.d.data), (void *) data, len);
    pheap_data_hpd = OFF2PTR(hd, hpd);	// Recalculate the offset, as buffer might have been realloced
    pheap_data_hpd->u.v.value =
	HEAP_OFFSET(hd, pheap_data_hpd_d->u.d.data);
    pheap_data_hpd->flags |= F_VALUE_HEAPDATA_REF;
    pheap_data_hpd->u.v.value_next = 0;

    return 0;
}
#endif				/* UNUSED_FOR_NOW */

/*
 *******************************************************************************
 * add_cooked_heap_value_data() -- Add cooked data to the given 
 *   value heap element.
 *
 *   Return 0 => Success
 *******************************************************************************
 */
STATIC
    int add_cooked_heap_value_data(mime_header_t * hd, dataddr_t hpd,
				   const char *data, int len)
{
    dataddr_t hpd_d;
    heap_data_t *pheap_data;
    heap_data_t *pheap_data_hpd_d;

    pheap_data = OFF2PTR(hd, hpd);
    NKN_ASSERT(pheap_data->flags & F_VALUE_DATA);
    hpd_d = get_heap_element(hd, F_DATA, len);
    if (!hpd_d) {
	DBG("get_heap_element(F_DATA, %d) failed", len);
	return 1;
    }
    pheap_data_hpd_d = OFF2PTR(hd, hpd_d);
    memcpy((void *) &(pheap_data_hpd_d->u.d.data), (void *) data, len);
    pheap_data = OFF2PTR(hd, hpd);	// Recalculate the offset, as buffer might have been realloced
    pheap_data->u.v.cooked_data =
	HEAP_OFFSET(hd, pheap_data_hpd_d->u.d.data);
    return 0;
}

/*
 *******************************************************************************
 * parse_query_string() -- Parse and insert name/value pairs
 *   into query_string_head list.
 *
 *   Return 0 => Success
 *******************************************************************************
 */
STATIC int parse_query_string(mime_header_t * hd)
{
    int rv = 0;
    dataddr_t hpval;
    char *p, *p_end;
    char *qs;
    char printbuf[32];
    char *pname = 0;
    char *pvalue = 0;
    int local_buflen;
    int pname_len;
    int pvalue_len;
    heap_data_t *pheap_data;

    while (1) {
	if (hd->query_string_head) {
	    break;		/* Already parsed */
	}

	if (!hd->known_header_map[MIME_HDR_X_NKN_QUERY]) {
	    DBG("No query string, hd=%p", hd);
	    rv = 1;		/* No query string defined */
	    break;
	}

	hpval = hd->known_header_map[MIME_HDR_X_NKN_QUERY];
	pheap_data = OFF2PTR(hd, hpval);
	p = (pheap_data->flags & F_VALUE_HEAPDATA_REF) ?
	    OFF2CHAR_PTR(hd, pheap_data->u.v.value) :
	    EXT_OFF2CHAR_PTR(hd, pheap_data->u.v.value);
	NKN_ASSERT(p == CHAR_PTR(hd, hpval, F_VALUE_HEAPDATA_REF));

	if ((*p != '?') || (pheap_data->u.v.value_len <= 1)) {
	    printbuf[0] = *p;
	    printbuf[1] = 0;
	    DBG("Invalid query string, qs=%s len=%d ",
		printbuf, pheap_data->u.v.value_len);
	    rv = 2;		/* Invalid query string */
	    break;
	}

	local_buflen = pheap_data->u.v.value_len + 1;
	qs = (char *) alloca(local_buflen + 1);
	memcpy((void *) qs, p, local_buflen - 1);
	qs[local_buflen - 1] = ('&' == qs[local_buflen - 2])? '\0' : '&';
	qs[local_buflen] = '\0';
	p = qs + 1;

	pname = (char *) alloca(local_buflen);
	pvalue = (char *) alloca(local_buflen);

	DBG("Parsing QS [%s]", qs);

	while ((p_end = strpbrk(p, "=&"))) {
	    if ((p_end - p) == 0) {
		DBG("Invalid QS name[%s]", p);
		rv = 3;
		break;
	    }
	    pname = unescape_str(p, p_end - p, 1, pname, local_buflen,
				 &pname_len);

	    if (*p_end == '=') {
		p = ++p_end;
		p_end = strpbrk(p, "&");
		if (!p_end) {
		    DBG("Invalid QS value[%s]", p);
		    rv = 4;
		    break;

		} else if ((p_end - p) == 0) {
		    /* Null value */
		    rv = add_query_namevalue(hd, pname, pname_len - 1, "",
					     1);
		    if (rv) {
			DBG("add_query_namevalue() failed rv=%d", rv);
			rv = 5;
			break;
		    }

		} else {
		    pvalue = unescape_str(p, p_end - p, 1, pvalue,
					  local_buflen, &pvalue_len);
		    rv = add_query_namevalue(hd, pname, pname_len - 1,
					     pvalue, pvalue_len - 1);
		    if (rv) {
			DBG("add_query_namevalue() failed rv=%d", rv);
			rv = 6;
		    }
		}
	    } else {
		/* Null value */
		rv = add_query_namevalue(hd, pname, pname_len - 1, "", 1);
		if (rv) {
		    DBG("add_query_namevalue() failed rv=%d", rv);
		    rv = 7;
		    break;
		}
	    }
	    p = ++p_end;
	}
	break;
    }

    return rv;
}

/*
 *******************************************************************************
 * parse_rfc1122_date() -- Fast RFC 1122 date parser
 *******************************************************************************
 */
STATIC int fast_parse_rfc1122_date(const char *buf, int len, struct tm *tp)
{
    UNUSED_ARGUMENT(len);

    /*
     * Optimization for RFC 1122 date
     *
     * Assumptions:
     *  1) No leading whitespace
     *  2) Length >= 29 with comma at buf[3]
     */


/* Day of week, ascending ascii value order */
#define Fri_HEX 0x467269
#define Mon_HEX 0x4D6F6E
#define Sat_HEX 0x536174
#define Sun_HEX 0x53756E
#define Thu_HEX 0x546875
#define Tue_HEX 0x547565
#define Wed_HEX 0x576564


/* Month of year, ascending ascii value order */
#define Apr_HEX 0x417072
#define Aug_HEX 0x417567
#define Dec_HEX 0x446563
#define Feb_HEX 0x466562
#define Jan_HEX 0x4A616E
#define Jul_HEX 0x4A756C
#define Jun_HEX 0x4A756E
#define Mar_HEX 0x4D6172
#define May_HEX 0x4D6179
#define Nov_HEX 0x4E6F76
#define Oct_HEX 0x4F6374
#define Sep_HEX 0x536570

    unsigned int week;
    unsigned int month;

    tp->tm_wday = -1;
    week = (buf[0] << 16) | (buf[1] << 8) | buf[2];

    if (week <= Sun_HEX) {
	switch (week) {
	case Fri_HEX:
	    tp->tm_wday = 5;
	    break;
	case Mon_HEX:
	    tp->tm_wday = 1;
	    break;
	case Sat_HEX:
	    tp->tm_wday = 6;
	    break;
	case Sun_HEX:
	    tp->tm_wday = 0;
	    break;
	}
    } else {
	switch (week) {
	case Thu_HEX:
	    tp->tm_wday = 4;
	    break;
	case Tue_HEX:
	    tp->tm_wday = 2;
	    break;
	case Wed_HEX:
	    tp->tm_wday = 3;
	    break;
	}
    }
    if (tp->tm_wday < 0) {
	return 1;
    }

    tp->tm_mday = (buf[5] - '0') * 10 + (buf[6] - '0');

    tp->tm_mon = -1;
    month = (buf[8] << 16) | (buf[9] << 8) | buf[10];

    if (month <= Jul_HEX) {
	if (month <= Dec_HEX) {
	    switch (month) {
	    case Apr_HEX:
		tp->tm_mon = 3;
		break;
	    case Aug_HEX:
		tp->tm_mon = 7;
		break;
	    case Dec_HEX:
		tp->tm_mon = 11;
		break;
	    }
	} else {
	    switch (month) {
	    case Feb_HEX:
		tp->tm_mon = 1;
		break;
	    case Jan_HEX:
		tp->tm_mon = 0;
		break;
	    case Jul_HEX:
		tp->tm_mon = 6;
		break;
	    }
	}
    } else {
	if (month <= May_HEX) {
	    switch (month) {
	    case Jun_HEX:
		tp->tm_mon = 5;
		break;
	    case Mar_HEX:
		tp->tm_mon = 2;
		break;
	    case May_HEX:
		tp->tm_mon = 4;
		break;
	    }
	} else {
	    switch (month) {
	    case Nov_HEX:
		tp->tm_mon = 10;
		break;
	    case Oct_HEX:
		tp->tm_mon = 9;
		break;
	    case Sep_HEX:
		tp->tm_mon = 8;
		break;
	    }
	}
    }

    if (tp->tm_mday < 0) {
	return 2;
    }

    tp->tm_year = ((buf[12] - '0') * 1000 + (buf[13] - '0') * 100 +
		   (buf[14] - '0') * 10 + (buf[15] - '0')) - 1900;
    tp->tm_hour = (buf[17] - '0') * 10 + (buf[18] - '0');
    tp->tm_min = (buf[20] - '0') * 10 + (buf[21] - '0');
    tp->tm_sec = (buf[23] - '0') * 10 + (buf[24] - '0');

    if ((buf[19] != ':') || (buf[22] != ':')) {
	return 3;
    }
    return 0;
}

/*
 *******************************************************************************
 * parse_day() -- Parse day string
 *******************************************************************************
 */
STATIC int parse_day(const char **pbuf, const char *buf_end, int *day)
{
    const char *buf;
    const char *day_end;
    const char **day_names;
    int n;

    buf = *pbuf;
    /* Find day start */
    while ((buf != buf_end) && !IS_ALPHA(*buf)) {
	buf++;
    }

    /* Find day end */
    day_end = buf;
    while ((day_end != buf_end) && IS_ALPHA(*day_end)) {
	day_end++;
    }

    if ((day_end - buf) == 3) {
	day_names = day_names_short;
    } else {
	day_names = day_names_long;
    }

    for (n = 0; n < 7; n++) {
	if (strncasecmp(day_names[n], buf, 3) == 0) {
	    *day = n;
	    *pbuf = day_end;
	    return 0;
	}
    }
    return 1;
}

/*
 *******************************************************************************
 * parse_month() -- Parse month string
 *******************************************************************************
 */
STATIC int parse_month(const char **pbuf, const char *buf_end, int *month)
{
    const char *buf;
    const char *month_end;
    int n;

    buf = *pbuf;
    /* Find month start */
    while ((buf != buf_end) && !IS_ALPHA(*buf)) {
	buf++;
    }

    /* Find month end */
    month_end = buf;
    while ((month_end != buf_end) && IS_ALPHA(*month_end)) {
	month_end++;
    }
    *pbuf = month_end;

    for (n = 0; n < 12; n++) {
	if (strncasecmp(month_names[n], buf, 3) == 0) {
	    *month = n;
	    return 0;
	}
    }
    return 1;
}

/*
 *******************************************************************************
 * parse_year() -- Parse year string
 *******************************************************************************
 */
STATIC int parse_year(const char **pbuf, const char *buf_end, int *year)
{
    const char *buf;
    int i = 0;

    buf = *pbuf;
    /* Find year start */
    while ((buf != buf_end) && !IS_DIGIT(*buf)) {
	buf++;
    }

    /* Compute year */
    while ((buf != buf_end) && IS_DIGIT(*buf)) {
	i = (i * 10) + (*buf - '0');
	buf++;
    }
    *pbuf = buf;

    if (i >= 1900) {
	i -= 1900;
    } else if (i < 70) {
	i += 100;
    }

    *year = i;

    return 0;
}

/*
 *******************************************************************************
 * parse_int() -- Parse integer string
 *******************************************************************************
 */
STATIC int parse_int(const char **pbuf, const char *buf_end, int *intval)
{
    const char *buf;
    int negative_val = 0;
    int val = 0;

    buf = *pbuf;
    /* Find digit start */
    while ((buf != buf_end) && (*buf != '-') && !IS_DIGIT(*buf)) {
	buf++;
    }

    if ((buf == buf_end) || (*buf == '\0')) {
	*pbuf = buf;
	return 1;
    }

    if (*buf == '-') {
	negative_val = 1;
    }

    while ((buf != buf_end) && IS_DIGIT(*buf)) {
	val = (val * 10) + (*buf - '0');
	buf++;
    }
    *pbuf = buf;
    *intval = negative_val ? -val : val;
    return 0;
}

/*
 *******************************************************************************
 * parse_time() -- Parse time string
 *******************************************************************************
 */
STATIC
    int parse_time(const char **pbuf, const char *buf_end, int *hour,
		   int *min, int *sec)
{
    int rv;
    rv = parse_int(pbuf, buf_end, hour);
    if (rv) {
	return 1;
    }
    rv = parse_int(pbuf, buf_end, min);
    if (rv) {
	return 2;
    }
    rv = parse_int(pbuf, buf_end, sec);
    if (rv) {
	return 3;
    }
    return 0;
}

/*
 *******************************************************************************
 * parse_mday() -- Parse day of month string
 *******************************************************************************
 */
STATIC int parse_mday(const char **pbuf, const char *buf_end, int *mday)
{
    return parse_int(pbuf, buf_end, mday);
}

/*
 *******************************************************************************
 * parse_name_value() -- Parse name value data in header data 
 *   (e.g. cache control, accept) and call back with the given name/value.
 *
 *   Return 0 => Success
 *******************************************************************************
 */
STATIC
    int parse_name_value(const hash_table_def_t * htd,
			 const name_value_descriptor_t * nvd,
			 int ignore_unknown_tokens,
			 const char *buf, const char *buf_end,
			 void *token,
			 int (*have_name_val) (const void *token, int data,
					       const char *val,
					       const char *val_end))
{
    typedef enum { SCAN_TOKEN_BEGIN, SCAN_TOKEN_END,
	SCAN_UNKNOWN_TOKEN_END,
	SCAN_VAL_BEGIN, SCAN_VAL_END,
	SCAN_QUOT_END,
    } state_t;
    state_t st;
    state_t st_l;
    int last_char;
    int rv = 0;
    int rdata;
    const char *p;
    const char *p_beg = 0;
    const char *p_end;

    if (!htd || !nvd || (buf >= buf_end) || !have_name_val) {
	DBG("Invalid data, htd=%p nvd=%p buf=%p "
	    "buf_end=%p have_name_val=%p",
	    htd, nvd, buf, buf_end, have_name_val);
	rv = 1;
	goto exit;
    } else {
	TSTR(buf, buf_end - buf + 1, tptr);
	DBG("Parsing [%s]", tptr);
    }

    p = buf;
    st = SCAN_TOKEN_BEGIN;

    while (p <= buf_end) {
	last_char = (p >= buf_end);

	switch (st) {
	case SCAN_TOKEN_BEGIN:
	    if (last_char || IS_ALPHA(*p)) {
		p_beg = p;
		st = SCAN_TOKEN_END;
		/* Fall through */
	    } else {
		if (*p == '\"') {
			st_l = st;
			st = SCAN_QUOT_END;
		}

		break;
	    }

	case SCAN_TOKEN_END:
	    rdata = -1;
	    if (last_char) {
		rv = (*htd->lookup_func) (htd, p_beg, (p - p_beg) + 1,
					  &rdata);
		if (rv) {
		    TSTR(p_beg, (p - p_beg) + 1, tptr);
		    if (ignore_unknown_tokens) {
			DBG("Ignoring unknown token [%s]", tptr);
			st = SCAN_UNKNOWN_TOKEN_END;
			rv = 0;
			break;
		    } else {
			DBG("Invalid token [%s]", tptr);
			rv = 2;
			goto exit;
		    }

		}
	    } else {
		switch (*p) {
		case '=':
		case ',':
		case ' ':
			/** case '\r': **/
			/** case '\n': **/
		    rv = (*htd->lookup_func) (htd, p_beg, p - p_beg,
					      &rdata);
		    if (rv) {
			TSTR(p_beg, (p - p_beg), tptr);
			if (ignore_unknown_tokens) {
			    DBG("Ignoring unknown token [%s]", tptr);
			    st = SCAN_UNKNOWN_TOKEN_END;
			    rv = 0;
			} else {
			    DBG("Invalid token [%s]", tptr);
			    rv = 3;
			    goto exit;
			}
		    }
		    break;
		default:
		    break;
		}
	    }

	    if (rdata >= 0) {
		if (nvd[rdata].has_value && (*p == '=')) {
		    st = SCAN_VAL_BEGIN;
		} else {
		    if (nvd[rdata].has_value > 0) {
			/* Required data not present */
			TSTR(buf, (buf_end - buf) + 1, tptr);
			DBG("Invalid token, value required [%s]", tptr);
			rv = 4;
			goto exit;
		    }

		    rv = (*have_name_val) (token, rdata, 0, 0);
		    if (rv) {
			TSTR(buf, (buf_end - buf) + 1, tptr);
			DBG("(*have_name_val)(%p, %d, 0, 0) failed "
			    "rv=%d", token, rdata, rv);
			rv = 5;
			goto exit;
		    }
		    st = SCAN_TOKEN_BEGIN;
		}
	    }
	    break;

	case SCAN_UNKNOWN_TOKEN_END:
	    if ((*p == ',') || (*p == ' ')) {
		st = SCAN_TOKEN_BEGIN;
	    }
	    if (*p == '\"') {
		    st_l = st;
		    st = SCAN_QUOT_END;
	    }
	    break;

	case SCAN_VAL_BEGIN:
	    /* BZ 3362, Read -ve values also, so that they can be handled
	     * properly.
	     */
	    if (last_char || IS_ALPHA(*p) || IS_DIGIT(*p) || *p == '-') {
		p_beg = p;
		st = SCAN_VAL_END;
		/* Fall through */
	    } else {
		break;
	    }

	case SCAN_VAL_END:
	    p_end = 0;

	    if (last_char) {
		p_end = p;
	    } else {
		switch (*p) {
		case ',':
		case ' ':
			/** case '\r': **/
			/** case '\n': **/
		    p_end = p - 1;
		    break;
		default:
		    break;
		}
	    }

	    if (p_end) {
		rv = (*have_name_val) (token, rdata, p_beg, p_end);
		if (rv) {
		    TSTR(buf, (buf_end - buf) + 1, tptr);
		    DBG("(*have_name_val)(%p, %d, %p, %p) failed rv=%d",
			token, rdata, p_beg, p_end, rv);
		    rv = 6;
		    goto exit;
		}
		st = SCAN_TOKEN_BEGIN;
	    }
	    break;
	case SCAN_QUOT_END: 
	    if (*p == '\"') {
		    st = st_l;
	    }
	    break;

	default:
	    break;
	}
	p++;
    }

  exit:

    return rv;
}

/*
 *******************************************************************************
 * parse_cache_control_enum() -- Parse cache control attributes
 *
 *   Return 0 => Success
 *******************************************************************************
 */
STATIC
    int set_cc_name_val(const void *token, int data, const char *val,
			const char *val_end)
{
    int rv = 0;
    cooked_data_types_t *cdp;
    cdp = (cooked_data_types_t *) token;

    if (val) {
	switch (data) {
	case CT_MAX_AGE:
	    /* BZ 3362, if value is negative make it 0
	     */
	    /* BZ 4528 , if value is negative on request , 
	     * should not revalidate
	     */

         // BZ-9789: This check is to use the first of multiple max-age occurrences.
         if (cdp->u.dt_cachecontrol_enum.mask & CT_MASK_MAX_AGE)
             break;

	    cdp->u.dt_cachecontrol_enum.max_age =
		    atol_len(val, val_end - val + 1);
	    break;
	case CT_MAX_STALE:
	    cdp->u.dt_cachecontrol_enum.max_stale =
		atol_len(val, val_end - val + 1);
	    break;
	case CT_MIN_FRESH:
	    cdp->u.dt_cachecontrol_enum.min_fresh =
		atol_len(val, val_end - val + 1);
	    break;
	case CT_S_MAXAGE:
	    cdp->u.dt_cachecontrol_enum.s_maxage =
		atol_len(val, val_end - val + 1);
	    break;
	default:
	    {
		TSTR(val, val_end - val + 1, tptr);
		DBG("Ignoring value=[%s] for type=%d", tptr, data);
		break;
	    }
	}			/* End of switch */
    }
    cdp->u.dt_cachecontrol_enum.mask |= 1 << data;

    return rv;
}

STATIC
    int parse_cache_control_enum(const char *buf, const char *buf_end,
				 cooked_data_types_t * out)
{
    int rv = 0;
    if (!out) {
	DBG("Invalid input, out=%p", out);
	rv = 1;
	goto exit;
    }
    memset((void *) out, 0, sizeof(*out));

    rv = parse_name_value(&ht_cache_control_vals, cache_control_data,
			  1 /* Ignore unknown tokens */ ,
			  buf, buf_end, out, set_cc_name_val);
  exit:

    return rv;
}

/*
 *******************************************************************************
 * parse_contentrange() -- Parse HTTP "Content-Range" header
 *
 *   Return 0 => Success
 *******************************************************************************
 */
STATIC
    int parse_contentrange(const char *buf, const char *buf_end,
			   cooked_data_types_t * out)
{
    int rv = 0;
    const char *p;
    int64_t off1 = 0;
    int64_t off2 = 0;
    int64_t entity_len = 0;

    typedef enum { INIT, LOOK_OFF1, LOOK_OFF2, LOOK_ENTITY_LEN } state_t;
    state_t st = INIT;

    /* e.g. bytes 100-200/300 */

    p = buf;
    while (p <= buf_end) {
	if (!IS_DIGIT(*p)) {
	    p++;
	} else {
	    st = LOOK_OFF1;
	    break;
	}
    }

    if (st == LOOK_OFF1) {
	while (p <= buf_end) {
	    if (IS_DIGIT(*p)) {
		off1 = off1 * 10 + (*p - '0');
		p++;
	    } else {
		if (*p == '-') {
		    st = LOOK_OFF2;
		    p++;
		}
		break;
	    }
	}
    }

    if (st == LOOK_OFF2) {
	while (p <= buf_end) {
	    if (IS_DIGIT(*p)) {
		off2 = off2 * 10 + (*p - '0');
		p++;
	    } else {
		if (*p == '/') {
		    st = LOOK_ENTITY_LEN;
		    p++;
		}
		break;
	    }
	}
    }

    if (st == LOOK_ENTITY_LEN) {
	while (p <= buf_end) {
	    if (IS_DIGIT(*p)) {
		entity_len = entity_len * 10 + (*p - '0');
		p++;
	    } else {
		if (*p == '/') {
		    st = LOOK_ENTITY_LEN;
		    p++;
		}
		break;
	    }
	}
    }

    if ((st == LOOK_ENTITY_LEN) && (off1 <= off2) && entity_len) {
	out->u.dt_contentrange.offset = off1;
	out->u.dt_contentrange.length = (off2 - off1) + 1;
	out->u.dt_contentrange.entity_length = entity_len;
    } else {
	TSTR(buf, buf_end - buf + 1, tp);
	DBG("Bad value=[%s] off1=%ld off2=%ld entity_len=%ld",
	    tp, off1, off2, entity_len);
	rv = 1;
    }
    return rv;
}

/*
 *******************************************************************************
 * parse_range() -- Parse HTTP "range" header
 *
 *   Return 0 => Success; type denotes the data type for [data, len] 
 *******************************************************************************
 */
STATIC
    int parse_range(const char *buf, const char *buf_end,
		    cooked_data_types_t * out)
{
    int rv = 0;
    const char *p;
    int64_t off1 = 0;
    int64_t off2 = 0;

    typedef enum { INIT, LOOK_OFF1, LOOK_OFF2, LOOK_OTHERS } state_t;
    state_t st = INIT;

    /* Only basic range supported (e.g. bytes 100-200) */

    p = buf;
    while (p < buf_end) {
	if (!IS_DIGIT(*p)) {
	    if (*p == '-') {
		/* Unsuppported form (e.g. bytes: -500) */
		break;
	    }
	    p++;
	} else {
	    st = LOOK_OFF1;
	    break;
	}
    }

    if (st == LOOK_OFF1) {
	while (p < buf_end) {
	    if (IS_DIGIT(*p)) {
		off1 = off1 * 10 + (*p - '0');
		p++;
	    } else {
		if (*p == '-') {
		    st = LOOK_OFF2;
		    p++;
		}
		break;
	    }
	}
    }

    if (st == LOOK_OFF2) {
	while (p <= buf_end) {
	    if (IS_DIGIT(*p)) {
		off2 = off2 * 10 + (*p - '0');
		p++;
	    } else {
		if (*p == ',') {
		    st = LOOK_OTHERS;
		    p++;
		}
		break;
	    }
	}
    }

    if (st == LOOK_OTHERS) {
	/* Multiple range(s) not supported (e.g. bytes=10-20,20-30) */
    }

    if ((st == LOOK_OFF2) && (off1 < off2)) {
	out->u.dt_range.offset = off1;
	out->u.dt_range.length = off2 - off1;
    } else {
	TSTR(buf, buf_end - buf + 1, tp);
	DBG("Bad value=[%s] off1=%ld off2=%ld", tp, off1, off2);
	rv = 1;
    }
    return rv;
}

/*
 ****************************************************************************** 
 * count_known_header_values() -- Count value headers along with total byes
 *   used for all value headers.
 *
 * Return: 0 => Success
 *
 ****************************************************************************** 
 */
int count_known_header_values(const mime_header_t * hd, int token,
				  int *num_values,
				  int *total_val_bytecount)
{
    int rv;
    const char *data;
    int len;
    u_int32_t attrs;
    int header_num = 0;
    int total_bytes = 0;

    while ((rv = mime_hdr_get_nth_known(hd, token, header_num,
					&data, &len, &attrs)) == 0) {
	total_bytes += len;
	header_num++;
    }

    *num_values = header_num;
    *total_val_bytecount = total_bytes;
    return 0;
}

/*
 ****************************************************************************** 
 * concatenate_known_header_values() -- Concatenate values for the given known
 *   header into the allocated buffer which is NULL terminated.  Each value is
 *   comma separated.
 *
 ****************************************************************************** 
 */
char *concatenate_known_header_values(const mime_header_t * hd,
                                      int token, char *buf, int bufsz)
{
    int rv = 0;
    int bytes_used = 0;
    int header_num = 0;
    const char *data;
    int len;
    u_int32_t attrs;

    if (!buf) {
	DBG("!buf passed, buf=%p", buf);
	rv = 1;
	goto err_exit;
    }

    while ((rv = mime_hdr_get_nth_known(hd, token, header_num,
					&data, &len, &attrs)) == 0) {
	if (data && len) {
	    if ((bytes_used + len + 1) > bufsz) {
		DBG("bufsz exceeded reqbufsz=%d bufsz=%d", len + 1, bufsz);
		rv = 2;
		goto err_exit;
	    }
	    memcpy((void *) &buf[bytes_used], (void *) data, len);
	    bytes_used += len;
	    if (data[len - 1] != ',') {
		buf[bytes_used] = ',';
		bytes_used++;
	    }
	}
	header_num++;
    }

    buf[bytes_used ? (bytes_used - 1) : 0] = 0;
    return buf;

  err_exit:

    return NULL;
}

/*
 ****************************************************************************** 
 * update_nv_list -- Update or add given new name/value list into the existing 
 *                   name/value list.
 *
 *   The given name_value_t lists reside in malloc'ed memory.
 *   New elements are appended to nv_list_arg.
 *
 *   Return 0 on success
 *
 ****************************************************************************** 
 */
STATIC
    int update_nv_list(name_value_t ** nv_list_arg, int *nv_list_entries,
		       int *nv_list_maxentries,
		       name_value_t * new_nv_list, int new_nv_list_entries)
{
    name_value_t *nv_list = *nv_list_arg;
    name_value_t *t_nv;
    int new_nv_ix;
    int nv_ix;
    int added_entries = 0;
    int processed;
    int rv = 0;

    for (new_nv_ix = 0; new_nv_ix < new_nv_list_entries; new_nv_ix++) {
	processed = 0;
	for (nv_ix = 0; nv_ix < *nv_list_entries; nv_ix++) {
	    if (strcmp(nv_list[nv_ix].name, new_nv_list[new_nv_ix].name) ==
		0) {
		// Name match
		if (nv_list[nv_ix].value && new_nv_list[new_nv_ix].value) {
		    if (strcasecmp(nv_list[nv_ix].value,
			       new_nv_list[new_nv_ix].value)) {
			// Values differ, update
			nv_list[nv_ix].value =
			    new_nv_list[new_nv_ix].value;
		    }
		}
		processed = 1;
		break;
	    }
	}
	if (!processed && (nv_ix >= *nv_list_entries)) {
	    // Addition
	    if ((*nv_list_entries + added_entries + 1) > *nv_list_maxentries) {
		*nv_list_maxentries *= 2;
		t_nv = nkn_realloc_type(nv_list,
				*nv_list_maxentries * sizeof(name_value_t),
				mod_http_name_value_t);
		if (t_nv) {
		    nv_list = t_nv;
		    *nv_list_arg = t_nv;
		} else {
		    DBG("realloc() failed, nv_list_maxentries=%d",
			*nv_list_maxentries);
		    rv = 1;
		    break;
		}
	    }
	    nv_list[*nv_list_entries + added_entries].name =
		new_nv_list[new_nv_ix].name;
	    nv_list[*nv_list_entries + added_entries].value =
		new_nv_list[new_nv_ix].value;
	    added_entries++;
	}
    }
    if (!rv) {
	*nv_list_entries += added_entries;
    }
    return rv;
}

/*
 *******************************************************************************
 * mime_hdr_pack_ext_data() -- Pack all external data references into the 
 *				provided buffer
 *
 * Note:(packed_ext_data = NULL) just computes the required bytes where
 *	it is assumed that all heap data references will be translated into
 *	external data references so these will be included in the required
 *	bytes count (packed_ext_data_bytes_used).
 *
 * Returns 0 on success
 *******************************************************************************
 */
STATIC 
int mime_hdr_pack_ext_data(mime_header_t *hdr, 
			const char *ext_range_2, int ext_range_2_length,
			const char *ext_range_3, int ext_range_3_length,
			char *packed_ext_data, 
			int packed_ext_data_size, 
			int *packed_ext_data_bytes_used)
{

#define EXT_DATA_ADDR_REAL_ADDR(_hdr, _in_data, _out_data) { \
    if (((_in_data) - (_hdr)->ext_data) < (_hdr)->ext_data_size) { \
    	(_out_data) = (_in_data); \
    } else if (((_in_data) - (_hdr)->ext_data) < \
    		((_hdr)->ext_data_size + ext_range_2_length)) { \
    	(_out_data) =  ext_range_2 + \
		(((_in_data) - (_hdr)->ext_data) - (_hdr)->ext_data_size); \
    } else if (((_in_data) - (_hdr)->ext_data) < \
    		((_hdr)->ext_data_size + ext_range_2_length + \
		 ext_range_3_length)) { \
    	(_out_data) =  ext_range_3 +  \
		    (((_in_data) - (_hdr)->ext_data) - \
		    (_hdr)->ext_data_size - ext_range_2_length); \
    } else { \
    	(_out_data) =  0; \
    } \
}

    heap_data_t *phd;
    char *heap_start;
    char *heap_end;
    int heaps_to_scan;

    char *outp = packed_ext_data;
    dataddr_t outp_bytes_used = 0;
    const char *psrc;
    const char *rsrc;

    if (!hdr || !packed_ext_data_bytes_used) {
    	return 1;
    }

    if (packed_ext_data && (hdr->ext_data_size != packed_ext_data_size)) {
    	return 2;
    }

    for (heaps_to_scan = 0; heaps_to_scan < 2; heaps_to_scan++) {
	if (!heaps_to_scan) {
	    /* Scan internal heap */
	    heap_start = hdr->heap;
	    phd = (heap_data_t *) heap_start;

	    if (hdr->heap_free_index >=
		(int) (sizeof(hdr->heap) / sizeof(heap_data_t))) {
		heap_end = hdr->heap + sizeof(hdr->heap);
	    } else {
		heap_end = hdr->heap +
		    (hdr->heap_free_index * sizeof(heap_data_t));
	    }
	} else {
	    /* Scan external heap */
	    heap_start = hdr->heap_ext;
	    phd = (heap_data_t *) heap_start;

	    if (hdr->heap_free_index >
		(int) (sizeof(hdr->heap) / sizeof(heap_data_t))) {
		heap_end = hdr->heap_ext +
		    (hdr->heap_free_index -
		     (sizeof(hdr->heap) / sizeof(heap_data_t))) *
		    sizeof(heap_data_t);
	    } else {
		break;
	    }
	}

	while ((char *) phd < heap_end) {
	    if (phd->flags & (F_VALUE_DATA|F_NAME_VALUE_DATA)) {
	    	// External data cases
		if ((phd->flags & 
			(F_VALUE_DATA|F_VALUE_HEAPDATA_REF)) == 
				F_VALUE_DATA) {
		    if (outp) {
			psrc = EXT_OFF2CHAR_PTR(hdr, phd->u.v.value);
			EXT_DATA_ADDR_REAL_ADDR(hdr, psrc, rsrc);
			if (!rsrc) {
			   return 3;
			}
		    	memcpy(outp, rsrc, phd->u.v.value_len);
			phd->u.v.value = MK_POFFSET(outp_bytes_used);
			outp += phd->u.v.value_len;
		    }
		    outp_bytes_used += phd->u.v.value_len;
		} 

		if ((phd->flags &
			(F_NAME_VALUE_DATA|F_NAME_HEAPDATA_REF)) == 
				F_NAME_VALUE_DATA) {
		    if (outp) {
			psrc = EXT_OFF2CHAR_PTR(hdr, phd->u.nv.name);
			EXT_DATA_ADDR_REAL_ADDR(hdr, psrc, rsrc);
			if (!rsrc) {
			   return 4;
			}
		    	memcpy(outp, rsrc, phd->u.nv.name_len);
			phd->u.nv.name = MK_POFFSET(outp_bytes_used);
			outp += phd->u.nv.name_len;
		    }
		    outp_bytes_used += phd->u.nv.name_len;

		    if (outp) {
			psrc = EXT_OFF2CHAR_PTR(hdr, phd->u.nv.value);
			EXT_DATA_ADDR_REAL_ADDR(hdr, psrc, rsrc);
			if (!rsrc) {
			   return 5;
			}
		    	memcpy(outp, rsrc, phd->u.nv.value_len);
			phd->u.nv.value = MK_POFFSET(outp_bytes_used);
			outp += phd->u.nv.value_len;
		    }
		    outp_bytes_used += phd->u.nv.value_len;
		}

	    	// Local or external heap cases
		//
		// Consider these in the buffer bytes required case (!outp)
		// since these will be converted to external data

		if ((phd->flags & 
			(F_VALUE_DATA|F_VALUE_HEAPDATA_REF)) == 
				(F_VALUE_DATA|F_VALUE_HEAPDATA_REF)) {
		    if (!outp) {
		    	outp_bytes_used += phd->u.v.value_len;
		    }
		}

		if ((phd->flags &
			(F_NAME_VALUE_DATA|F_VALUE_HEAPDATA_REF)) == 
				(F_NAME_VALUE_DATA|F_VALUE_HEAPDATA_REF)) {
		    if (!outp) {
		    	outp_bytes_used += phd->u.nv.name_len;
		    	outp_bytes_used += phd->u.nv.value_len;
		    }
		}
		phd++;
	    } else {
		if (phd->flags & F_DATA) {
		    phd +=
			DATA_HEAP_ELEMENTS(hd,
					   (unsigned int) (phd->u.d.size));
		} else {
		    // Free element
		    phd++;
		}
	    }
	}
    }
    *packed_ext_data_bytes_used = outp_bytes_used;
    return 0;
}
#undef EXT_DATA_ADDR_REAL_ADDR

/*
 * mime_hdr_compress()
 *
 * Compress the given mime_header_t by removing local and external heap
 * fragmentation in addition to making all local and external heap 
 * data references external data references.
 * To avoid data copies, we define two ranges beyond the defined external
 * data to denote local heap (ext_range_2) and external heap (ext_range_3)
 * references which are passed as input to mime_hdr_pack_ext_data().
 *
 *    external data start = hdr->ext_data
 *    pseudo local heap start  = hdr->ext_data + hdr->ext_data_size
 *    pseudo external heap start = hdr->ext_data + hdr->ext_data_size +
 *				   sizeof(hdr->heap)
 *
 * Note: All data references in 'out_hdr' refer to data in 'in_hdr' so
 *       you cannot free 'in_hdr' until all action is complete on 'out_hdr'
 *
 * Return: 0 => Success with valid 'out_hdr' data,
 */
STATIC
int mime_hdr_compress(const mime_header_t *in_hdr, mime_header_t *out_hdr,
		      const char **ext_range_2, int *ext_range_2_length,
		      const char **ext_range_3, int *ext_range_3_length)
{

#define EXT_DATA_ADDR(_hdr, _in_data, _out_data) { \
    if (IN_LOCAL_HEAP((_hdr), (_in_data))) { \
	(_out_data) = (_hdr)->ext_data + (_hdr)->ext_data_size +  \
		((_in_data) - (_hdr)->heap); \
    } else if (IN_EXT_HEAP((_hdr), (_in_data))) { \
	(_out_data) = (_hdr)->ext_data + (_hdr)->ext_data_size +  \
		sizeof((_hdr)->heap) + \
		((_in_data) - (_hdr)->heap_ext); \
    } else if (IN_EXT_DATA((_hdr), (_in_data))) { \
	(_out_data) = (_in_data); \
    } else { \
	(_out_data) = 0; \
    } \
}

    int retval = 0;
    int rv;
    int n;
    int nth;
    const char *name;
    int namelen;
    const char *data;
    int datalen;
    u_int32_t attr;
    int hdrcnt;
    const char *pname;
    const char *pdata;

    out_hdr->ext_data = in_hdr->ext_data;
    out_hdr->ext_data_size = in_hdr->ext_data_size + sizeof(in_hdr->heap) + 
  	in_hdr->heap_ext_size;

    // Copy known headers

    for (n = 0; n < MIME_HDR_MAX_DEFS; n++) {
	rv = mime_hdr_get_known(in_hdr, n, &data, &datalen, &attr, &hdrcnt);
	if (!rv) {
	    EXT_DATA_ADDR(in_hdr, data, pdata);
	    if (!pdata) {
		DBG("EXT_DATA_ADDR() failed data=%p pdata=%p", data, pdata);
	    	retval = 1;
		goto exit;
	    }
	    rv = mime_hdr_add_known(out_hdr, n, pdata, datalen);
	    if (rv) {
		DBG("mime_hdr_add_known() failed rv=%d", rv);
	    	retval = 2;
		goto exit;
	    }
	} else {
	    continue;
	}
	for (nth = 1; nth < hdrcnt; nth++) {
	    rv = mime_hdr_get_nth_known(in_hdr, n, nth, &data, &datalen,
					&attr);
	    if (!rv) {
	    	EXT_DATA_ADDR(in_hdr, data, pdata);
	    	if (!pdata) {
		    DBG("EXT_DATA_ADDR() failed data=%p pdata=%p", data, pdata);
		    retval = 3;
		    goto exit;
	    	}
		rv = mime_hdr_add_known(out_hdr, n, pdata, datalen);
		if (rv) {
		    DBG("mime_hdr_add_known() failed rv=%d", rv);
		    retval = 4;
		    goto exit;
		}
	    } else {
		DBG("mime_hdr_get_nth_known() failed rv=%d nth=%d", rv, nth);
	    }
	}
    }

    // Copy unknown headers

    n = 0;
    while ((rv = mime_hdr_get_nth_unknown(in_hdr, n, &name, &namelen,
					  &data, &datalen, &attr)) == 0) {
	EXT_DATA_ADDR(in_hdr, name, pname);
	if (!pname) {
	    DBG("EXT_DATA_ADDR() failed name=%p pname=%p", name, pname);
	    retval = 5;
	    goto exit;
	}
	EXT_DATA_ADDR(in_hdr, data, pdata);
	if (!pdata) {
	    DBG("EXT_DATA_ADDR() failed data=%p pdata=%p", data, pdata);
	    retval = 6;
	    goto exit;
	}
	rv = mime_hdr_add_unknown(out_hdr, pname, namelen, pdata, datalen);
	if (rv) {
	    DBG("mime_hdr_add_unknown() failed rv=%d", rv);
	    retval = 7;
	    goto exit;
	}
	n++;
    }
exit:
    if (!retval) {
    	// Reset to reflect actual external data size
    	out_hdr->ext_data_size = in_hdr->ext_data_size;
	*ext_range_2 = in_hdr->heap;
	*ext_range_2_length = sizeof(in_hdr->heap);
	*ext_range_3 = in_hdr->heap_ext;
	*ext_range_3_length = in_hdr->heap_ext_size;
    }
    return retval;
}
#undef EXT_DATA_ADDR

/*
 * End of mime_hdr_private.c
 */
