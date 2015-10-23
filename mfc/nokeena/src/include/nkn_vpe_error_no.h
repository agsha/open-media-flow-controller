#ifndef _NKN_VPE_ERRNO_
#define _NKN_VPE_ERRNO_

//INPUT ARGUMENTS ERROR
#define NKN_VPE_WRONG_NO_OF_PROFILES       -100 //'-n' argument is a MUST and the no of input profiles cannot be zero
#define NKN_VPE_E_ARG                      -101 //error/missing input arguments

//FILE OPERATION ERROR
#define NKN_VPE_INPUT_E_ACCESS             -200 //unable to open I/O descriptor (wrong path or not enough permissions)
#define NKN_VPE_E_READ                     -201 //error reading from I/O descriptor
#define NKN_VPE_E_PERMISSION               -202 
#define NKN_VPE_OUTPUT_E_ACCESS            -203 
#define NKN_VPE_E_UNKNOWN_IO_HANDLER       -204
#define NKN_VPE_E_FREE_SPACE               -205

//OTHER ERRORS
#define NKN_VPE_E_MEM                      -400 //malloc fails (not enough memory)
#define NKN_VPE_E_OTHER                    -401
#define NKN_VPE_E_INVALID_HANDLE           -402


//VPE CORE PRE PROCE ERRORS
#define NKN_VPE_E_NO_HANLDER               -500 //no registered codec handler for the codec
#define NKN_VPE_E_METADATA_OUT             -501 
#define NKN_VPE_E_PRE_PROC                 -502
#define NKN_VPE_E_CONTAINER_DATA           -503
#define NKN_VPE_E_TPLAY_LIST               -504

#endif //_NKN_VPE_ERRNO_
