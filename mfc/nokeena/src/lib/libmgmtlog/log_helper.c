#include <stdio.h>
#include "nkn_mgmt_log.h"

unsigned int jnpr_log_level = 7;
void 
set_log_level(unsigned int level)
{
    jnpr_log_level = level;
    return;
}
unsigned int
get_log_level(void)
{
    return jnpr_log_level;
}


