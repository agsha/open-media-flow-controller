#ifndef _HTTP_SIGNATURE_H
#define _HTTP_SIGNATURE_H
/* File : http_signature.h
 * Copyright (C) 2008 - 2009 Nokeena Networks, Inc. 
 * All rights reserved.
 */

/*
 * [Just for reference]
 * Hex dump of first 4 chars of METHODS determines the signature
 * METHODS are case sensitive
 *
 * 00000000  4f 50 54 49 0a 47 45 54  20 0a 48 45 41 44 0a 50  |OPTI.GET .HEAD.P|
 * 00000010  4f 53 54 0a 50 55 54 20  0a 44 45 4c 45 0a 54 52  |OST.PUT .DELE.TR|
 * 00000020  41 43 0a 43 4f 4e 4e 0a                           |AC.CONN.|
 * 00000028
 */

#define HTTP_SIG_MTHD_OPTIONS	(0x4954504f)
#define HTTP_SIG_MTHD_GET	(0x20544547)
#define HTTP_SIG_MTHD_HEAD	(0x44414548)
#define HTTP_SIG_MTHD_POST	(0x54534f50)
#define HTTP_SIG_MTHD_PUT	(0x20545550)
#define HTTP_SIG_MTHD_DELETE	(0x454c4544)
#define HTTP_SIG_MTHD_TRACE	(0x43415254)
#define HTTP_SIG_MTHD_CONNECT	(0x4e4e4f43)

/*
 * [Just for reference]
 * Hex dump of first 4 chars of METHODS determines the signature
 *
 * 00000000  61 63 63 65 0a 61 67 65  3a 0a 61 6c 6c 6f 0a 61  |acce.age:.allo.a|
 * 00000010  75 74 68 0a 63 61 63 68  0a 63 6f 6e 6e 0a 63 6f  |uth.cach.conn.co|
 * 00000020  6e 74 0a 63 6f 6f 6b 0a  64 61 74 65 0a 65 74 61  |nt.cook.date.eta|
 * 00000030  67 0a 65 78 70 65 0a 65  78 70 69 0a 66 72 6f 6d  |g.expe.expi.from|
 * 00000040  0a 68 6f 73 74 0a 69 66  2d 4d 0a 69 66 2d 4e 0a  |.host.if-M.if-N.|
 * 00000050  69 66 2d 52 0a 69 66 2d  55 0a 6b 65 65 70 0a 6c  |if-R.if-U.keep.l|
 * 00000060  61 73 74 0a 6c 6f 63 61  0a 6d 61 78 2d 0a 6d 69  |ast.loca.max-.mi|
 * 00000070  6d 65 0a 70 72 61 67 0a  70 72 6f 78 0a 70 72 6f  |me.prag.prox.pro|
 * 00000080  78 0a 70 75 62 6c 0a 72  61 6e 67 0a 72 65 66 65  |x.publ.rang.refe|
 * 00000090  0a 72 65 71 75 0a 72 65  74 72 0a 73 65 72 76 0a  |.requ.retr.serv.|
 * 000000a0  73 65 74 2d 0a 74 65 3a  20 0a 74 72 61 69 0a 74  |set-.te: .trai.t|
 * 000000b0  72 61 6e 0a 75 70 67 72  0a 75 73 65 72 0a 76 61  |ran.upgr.user.va|
 * 000000c0  72 79 0a 76 69 61 3a 0a  77 61 72 6e 0a 77 77 77  |ry.via:.warn.www|
 * 000000d0  2d 0a                                             |-.|
 * 000000d2
 */
#define HTTP_SIG_HDR_acce	(0x65636361)
/* Special case for age:
 * Colon added to have 4 bytes length
 * 'age:'  maps to 0x3a656761 */
#define HTTP_SIG_HDR_age_	(0x3a656761)
#define HTTP_SIG_HDR_allo	(0x6f6c6c61)
#define HTTP_SIG_HDR_auth	(0x68747561)
#define HTTP_SIG_HDR_cach	(0x68636163)
#define HTTP_SIG_HDR_conn	(0x6e6e6f63)
#define HTTP_SIG_HDR_cont	(0x746e6f63)
#define HTTP_SIG_HDR_cook	(0x6b6f6f63)
#define HTTP_SIG_HDR_date	(0x65746164)
#define HTTP_SIG_HDR_etag	(0x67617465)
#define HTTP_SIG_HDR_expe	(0x65707865)
#define HTTP_SIG_HDR_expi	(0x69707865)
#define HTTP_SIG_HDR_from	(0x6d6f7266)
#define HTTP_SIG_HDR_host	(0x74736f68)
#define HTTP_SIG_HDR_if_M	(0x4d2d6669)
#define HTTP_SIG_HDR_if_N	(0x4e2d6669)
#define HTTP_SIG_HDR_if_R	(0x522d6669)
#define HTTP_SIG_HDR_if_U	(0x552d6669)
#define HTTP_SIG_HDR_keep	(0x7065656b)
#define HTTP_SIG_HDR_last	(0x7473616c)
#define HTTP_SIG_HDR_loca	(0x61636f6c)
#define HTTP_SIG_HDR_max_	(0x2d78616d)
#define HTTP_SIG_HDR_mime	(0x656d696d)
#define HTTP_SIG_HDR_prag	(0x67617270)
#define HTTP_SIG_HDR_prox	(0x786f7270)
#define HTTP_SIG_HDR_publ	(0x6c627570)
#define HTTP_SIG_HDR_rang	(0x676e6172)
#define HTTP_SIG_HDR_refe	(0x65666572)
#define HTTP_SIG_HDR_requ	(0x75716572)
#define HTTP_SIG_HDR_retr	(0x72746572)
#define HTTP_SIG_HDR_serv	(0x76726573)
#define HTTP_SIG_HDR_set_	(0x2d746573)

/* Special handling reqd for "TE:"
 * Should be manually compared on default case 
 * */
#define HTTP_SIG_HDR_te_	(0x203a6574)  
#define HTTP_SIG_HDR_trai	(0x69617274)
#define HTTP_SIG_HDR_tran	(0x6e617274)
#define HTTP_SIG_HDR_upgr	(0x72677075)
#define HTTP_SIG_HDR_user	(0x72657375)
#define HTTP_SIG_HDR_vary	(0x79726176)
/* Special case for via :
 * Colon added to have 4 bytes length
 * 'via:'  maps to 0x3a616976 */
#define HTTP_SIG_HDR_via_	(0x3a616976)
#define HTTP_SIG_HDR_warn	(0x6e726177)
#define HTTP_SIG_HDR_www_	(0x2d777777)

#endif
