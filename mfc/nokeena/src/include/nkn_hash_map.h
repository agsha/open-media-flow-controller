#ifdef __cplusplus
extern "C" {
#endif
	void c_diskmgr_tbl_init(void);
	int c_diskmgr_tbl_insert(char *key, void *value);
	int c_diskmgr_tbl_delete (char *key);
	void* c_diskmgr_tbl_get(char *key);
	void c_diskmgr_tbl_print(void);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
        void c_memmgr_tbl_init(void);
        int c_memmgr_tbl_insert(char *key, void* value);
        int c_memmgr_tbl_delete (char *key);
        void * c_memmgr_tbl_get(char *key);
        void c_memmgr_tbl_print(void);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
        void c_sleeper_q_tbl_init(void);
        int c_sleeper_q_tbl_insert(char *key, void* value);
        int c_sleeper_q_tbl_delete (char *key);
        void * c_sleeper_q_tbl_get(char *key);
        void c_sleeper_q_tbl_print(void);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
        void c_rtsleeper_q_tbl_init(void);
        int c_rtsleeper_q_tbl_insert(char *key, void* value);
        int c_rtsleeper_q_tbl_delete (char *key);
        void * c_rtsleeper_q_tbl_get(char *key);
        void c_rtsleeper_q_tbl_print(void);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif
        void c_LOCmgr_tbl_init(void);
        int c_LOCmgr_tbl_insert(char *key, void* value);
        int c_LOCmgr_tbl_delete (char *key);
        void * c_LOCmgr_tbl_get(char *key);
        void c_LOCmgr_tbl_print(void);
#ifdef __cplusplus
}
#endif

void c_diskio_tbl_init(void);
int c_diskio_tbl_insert(char *key, void* value);
int c_diskio_tbl_delete (char *key);
void * c_diskio_tbl_get(char *key);
void c_diskio_tbl_print(void);
