

#ifdef INC_MD_CLIENT_INC_GRAFT_POINT
#if (MD_CLIENT_INC_GRAFT_POINT == 1)
    mgt_ftpuser,
    mgt_logtransfer,
#endif
#endif


#ifdef INC_MD_CLIENT_INC_GRAFT_POINT
#if (MD_CLIENT_INC_GRAFT_POINT == 2)
    {mgt_ftpuser, "ftpuser"},
    {mgt_logtransfer, "LogTransferGroup"},
#endif
#endif


