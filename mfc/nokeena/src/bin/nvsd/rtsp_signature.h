#ifndef _RTSP_SIGNATURE_H
#define _RTSP_SIGNATURE_H

/*
 * Signatures will not work in platforms other than x86
 * Need to change signatures for other platform
 */

/* [Just for reference]
 * Hex dump of first 4 chars of METHODS determines the signature
 * METHODS are case sensitive
 * 0000000: 4445 5343 0a41 4e4e 4f0a 4745 545f 0a4f  DESC.ANNO.GET_.O
 * 0000010: 5054 490a 5041 5553 0a50 4c41 590a 5245  PTI.PAUS.PLAY.RE
 * 0000020: 434f 0a52 4544 490a 5345 5455 0a53 4554  CO.REDI.SETU.SET
 * 0000030: 5f0a 5445 4152 0a0a                      _.TEAR..
 */

/*
 * All methods 4 char signature are unique
 */
#define RTSP_SIG_MTHD_DESCRIBE  (0x43534544)
#define RTSP_SIG_MTHD_ANNOUNCE  (0x4f4e4e41)
#define RTSP_SIG_MTHD_GET_PARAM	(0x5f544547)
#define RTSP_SIG_MTHD_OPTIONS	(0x4954504f)
#define RTSP_SIG_MTHD_PAUSE	(0x53554150)
#define RTSP_SIG_MTHD_PLAY	(0x59414c50)
#define RTSP_SIG_MTHD_RECORD	(0x4f434552)
#define RTSP_SIG_MTHD_REDIRECT	(0x49444552)
#define RTSP_SIG_MTHD_SETUP	(0x55544553)
#define RTSP_SIG_MTHD_SET_PARAM	(0x5f544553)
#define RTSP_SIG_MTHD_TEARDOWN	(0x52414554)
#define RTSP_SIG_MTHD_GET       (0x20544547)

/*
 * [Just for reference]
 * Hex dump of first 4 chars of headers determines the signature
 * Headrs are case insensitive
 * 00000000  61 63 63 65 0a 61 6c 6c  6f 0a 61 75 74 68 0a 62  |acce.allo.auth.b|
 * 00000010  61 6e 64 0a 62 6c 6f 63  0a 63 61 63 68 0a 63 6f  |and.bloc.cach.co|
 * 00000020  6e 66 0a 63 6f 6e 6e 0a  63 6f 6e 74 0a 63 73 65  |nf.conn.cont.cse|
 * 00000030  71 0a 64 61 74 65 0a 65  78 70 69 0a 66 72 6f 6d  |q.date.expi.from|
 * 00000040  0a 69 66 2d 6d 0a 6c 61  73 74 0a 70 72 6f 78 0a  |.if-m.last.prox.|
 * 00000050  70 75 62 6c 0a 72 61 6e  67 0a 72 65 66 65 0a 72  |publ.rang.refe.r|
 * 00000060  65 71 75 0a 72 65 74 72  0a 72 74 70 2d 0a 73 63  |equ.retr.rtp-.sc|
 * 00000070  61 6c 0a 73 65 73 73 0a  73 65 72 76 0a 73 70 65  |al.sess.serv.spe|
 * 00000080  65 0a 74 72 61 6e 0a 75  6e 73 75 0a 75 73 65 72  |e.tran.unsu.user|
 * 00000090  0a 76 69 61 3a 0a 77 77  77 2d 0a 78 2d 70 6c 0a  |.via:.www-.x-pl.|
 * 000000a0  0a                                                |.|
 * 000000a1
 *
 */

/* 
 * Header signatures
 * They name has only the 4 characters which are 
 * represented. Here one key can map to many headers
 * which should be resolved in code.
 *
 * NOTE '-' replaced by '_' in naming only and not in value
 */
#define RTSP_SIG_HDR_acce (0x65636361)
#define RTSP_SIG_HDR_allo (0x6f6c6c61)
#define RTSP_SIG_HDR_auth (0x68747561)
#define RTSP_SIG_HDR_band (0x646e6162)
#define RTSP_SIG_HDR_bloc (0x636f6c62)
#define RTSP_SIG_HDR_cach (0x68636163)
#define RTSP_SIG_HDR_conf (0x666e6f63)
#define RTSP_SIG_HDR_conn (0x6e6e6f63)
#define RTSP_SIG_HDR_cont (0x746e6f63)
#define RTSP_SIG_HDR_cseq (0x71657363)
#define RTSP_SIG_HDR_date (0x65746164)
#define RTSP_SIG_HDR_expi (0x69707865)
#define RTSP_SIG_HDR_from (0x6d6f7266)
#define RTSP_SIG_HDR_if_m (0x6d2d6669)
#define RTSP_SIG_HDR_last (0x7473616c)
#define RTSP_SIG_HDR_prox (0x786f7270)
#define RTSP_SIG_HDR_publ (0x6c627570)
#define RTSP_SIG_HDR_rang (0x676e6172)
#define RTSP_SIG_HDR_refe (0x65666572)
#define RTSP_SIG_HDR_requ (0x75716572)
#define RTSP_SIG_HDR_retr (0x72746572)
#define RTSP_SIG_HDR_rtp_ (0x2d707472)
#define RTSP_SIG_HDR_scal (0x6c616373)
#define RTSP_SIG_HDR_sess (0x73736573)
#define RTSP_SIG_HDR_serv (0x76726573)
#define RTSP_SIG_HDR_spee (0x65657073)
#define RTSP_SIG_HDR_tran (0x6e617274)
#define RTSP_SIG_HDR_unsu (0x75736e75)
#define RTSP_SIG_HDR_user (0x72657375)
#define RTSP_SIG_HDR_late_tolerance (0x72742d78)

/* Special case for via : 
 * Colon added to have 4 bytes length 
 * 'via:'  maps to 0x3a616976 */
#define RTSP_SIG_HDR_via_ (0x3a616976)
#define RTSP_SIG_HDR_www_ (0x2d777777)

/* Extention headers. 
 * x-playNow:
 */
#define RTSP_SIG_HDR_x_pl (0x6c702d78)

#endif
