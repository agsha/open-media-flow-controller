

#ifdef INC_CLI_FILE_UTILS_INC_GRAFT_POINT
#if (CLI_FILE_UTILS_INC_GRAFT_POINT == 1)
    {cfi_accesslog, ACCESSLOG_DIR_PATH},
    {cfi_errorlog, ACCESSLOG_DIR_PATH},
    {cfi_tracelog, ACCESSLOG_DIR_PATH},
    {cfi_cachelog, ACCESSLOG_DIR_PATH},
    {cfi_streamlog, ACCESSLOG_DIR_PATH},
    {cfi_fuselog, ACCESSLOG_DIR_PATH},
    {cfi_objectlist, OBJECTLIST_DIR_PATH},
    {cfi_fmsfiles, FMSFILES_DIR_PATH},
    {cfi_fmsaccesslog, FMSLOG_DIR_PATH},
    {cfi_fmsedgelog, FMSLOG_DIR_PATH},
    {cfi_publishlog, ACCESSLOG_DIR_PATH},
    {cfi_maxmindfiles, GEO_MAXMIND_DIR_PATH},
    {cfi_quovafiles, GEO_QUOVA_DIR_PATH},
#endif
#endif
