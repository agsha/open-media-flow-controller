/*
 * (C) Copyright 2015 Juniper Networks
 * All rights reserved.
 */

/* ------------------------------------------------------------------------- */
/* md_iptables_customer.inc.c: graft points for the Iptables mgmtd module.
 *
 * Note that the main Iptables mgmtd module is split up into five
 * different files: md_iptables.c and four others named
 * 'md_iptables_*.inc.c'.  These all compile into a single file, and
 * graft points from any of these five files will go into here.
 *
 * The graft point number space is split up, so each file can have its
 * own set of numbers without worrying about what the other files are doing:
 *   - md_iptables.c            graft points [1..99]
 *   - md_iptables_apply.c      graft points [101..199]
 *   - md_iptables_cmd.c        graft points [201..299]
 *   - md_iptables_decls.c      graft points [301..399]
 *   - md_iptables_state.c      graft points [401..499]
 */

/* ------------------------------------------------------------------------ */
/* Graft point 1 (from md_iptables.c): initialization
 */
#if MD_IPTABLES_INC_GRAFT_POINT == 1

#endif /* GRAFT_POINT 1 */


/* ------------------------------------------------------------------------ */
/* Graft point 2 (from md_iptables.c): deinitialization
 */
#if MD_IPTABLES_INC_GRAFT_POINT == 2

#endif /* GRAFT_POINT 2 */


/* ------------------------------------------------------------------------ */
/* Graft point 3 (from md_iptables.c): function implementations
 */
#if MD_IPTABLES_INC_GRAFT_POINT == 3

#endif /* GRAFT_POINT 3 */


/* ------------------------------------------------------------------------ */
/* Graft point 301 (from md_iptables_decls.c): additional tables to manage.
 */
#if MD_IPTABLES_INC_GRAFT_POINT == 301
    mitn_mangle,
#endif /* GRAFT_POINT 301 */
