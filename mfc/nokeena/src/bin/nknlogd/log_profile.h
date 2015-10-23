
#ifndef __LOG_PROFILE_H__
#define __LOG_PROFILE_H__



struct format_field;

struct logfile {
    int		slot;
    char	*filename;
    FILE	*fp;
    uint64_t	cur_filesize;
};

struct log_profile {

    /* word 1 */
    int32_t	channelid;
    int32_t	type;
    /* word 2 */
    uint32_t	active;
    uint32_t	flags;
    /* word 3 */
    struct logfile *lf;
    /* word 4 */
    char	*iobuf;
    /* word 5 */
    uint64_t	max_filesize;
    /* word 6 */
    int8_t	log_rotate;
    int8_t	cur_fileid;
    int8_t	max_fileid;
    int8_t	__pad;
    /* Binary representation of the format given from the CLI */
    int			ff_used;
    /* word 7 */
    struct format_field	*ff_ptr;

    /* word 8 */
    /* Raw format as given from the CLI */
    char *format;

    /* Upload config details */
    char *rmt_scheme;
    char *rmt_url;
    char *rmt_pass;

};


#endif /* __LOG_PROFILE_H__ */
