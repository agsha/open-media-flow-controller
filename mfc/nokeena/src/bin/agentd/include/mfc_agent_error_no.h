#ifndef  MFC_AGENTD_ERROR_NO_H

#define  MFC_AGENTD_ERROR_NO_H

typedef struct error_code {
    char *err_str;
    int  err_num;
} error_code_t;


enum error_map_num {
    XMLGW_CONN_FAILED = 0,
    XMLGW_AUTH_FAILED ,

    SSH_CONN_FAILED ,
    SSH_AUTH_FAILED,
    SSH_PROMPT_NOT_FOUND,
    SSH_PUSH_CMD_FAILED,

    TRANSL_ND_ENTRY_NOT_FOUND,
    TRANSL_ND_FORM_ERR,

    LIC_NOT_FOUND,
    MFC_COMMIT_FAILED,



} error_map_num_t;

extern error_code_t error_type_map[];

#endif
