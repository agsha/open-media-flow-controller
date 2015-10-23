#ifndef NKN_VPE_HA_STATE_H
#define NKN_VPE_HA_STATE_H


typedef enum stream_state_ha{
    STRM_REGISTRATION_FAILURE = 0,
    STRM_ACTIVE = 1,
    STRM_DEAD,
    STRM_GLITCH,
    STRM_ON_RECOVERY
} stream_state_e;








#endif
