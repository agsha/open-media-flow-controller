/*******************************************************************************
 * The information contained in this file is confidential and proprietary to
 * Broadcom Corporation.  No part of this file may be reproduced or
 * distributed, in any form or by any means for any purpose, without the
 * express written permission of Broadcom Corporation.
 *
 * (c) COPYRIGHT 2011 Broadcom Corporation, ALL RIGHTS RESERVED.
 *
 *
 * Module Description:
 *
 *
 * History:
 *    10/25/04 John Chen        Modified for Linux
 *    12/28/01 Hav Khauv        Initial version.
 ******************************************************************************/

#if DBG
#include "debug.h"
#include "ediag_compat.h"
#include "ediag_drv.h"

/*******************************************************************************
 * Macros for processing variable number of arguments.
 ******************************************************************************/

#ifndef _VA_LIST_DEFINED
#define _VA_LIST_DEFINED

typedef char *va_list;

#define _INTSIZEOF(n)   ( (sizeof(n) + sizeof(int) - 1) & ~(sizeof(int) - 1) )

#define va_start(ap,v)  ( ap = (va_list)&v + _INTSIZEOF(v) )
#define va_arg(ap,t)    ( *(t *)((ap += _INTSIZEOF(t)) - _INTSIZEOF(t)) )
#define va_end(ap)      ( ap = (va_list)0 )
#endif


/* Set to non-zero value, when DbgBreakX() is called */
static int dbg_break_flag;

static int ctl_c_flag;

/* Counter of attached async handlers */
static int async_hdlr_cnt;

/*******************************************************************************
 * Description:
 *
 * Return:
 ******************************************************************************/
STATIC long
debug_ltoh(
    unsigned long val,
    long width,
    char fill_char,
    char *dst_buf,
    unsigned long buf_len)
{
    unsigned long digit;
    unsigned long len;
    long ret_len;
    char c;

    if(buf_len == 0)
    {
        ret_len = 0;
        return ret_len;
    }

    if(width == 0)
    {
        width++;
    }

    len = 0;

    for(; ;)
    {
        if(len >= buf_len)
        {
            len = 0;
            break;
        }

        digit = val % 16;
        val = val/16;

        if(digit < 10)
        {
            *dst_buf = '0' + (char) digit;
        }
        else
        {
            *dst_buf = 'a' + (char) (digit-10);
        }

        dst_buf++;

        len++;
        width--;

        if(val == 0)
        {
            break;
        }
    }

    while(width > 0)
    {
        if(len >= buf_len)
        {
            len = 0;
            break;
        }

        *dst_buf = fill_char;
        dst_buf++;
        len++;
        width--;
    }

    ret_len = len;

    if(len)
    {
        /* Reverse the digits. */
        digit = 1;
        while(digit < len)
        {
            c = *(dst_buf-digit);
            *(dst_buf-digit) = *(dst_buf-len);
            *(dst_buf-len) = c;

            len--;
            digit++;
        }
    }

    return ret_len;
} /* debug_ltoh */



/*******************************************************************************
 * Description:
 *
 * Return:
 ******************************************************************************/
STATIC long
debug_ltod(
    long val,
    long width,
    char fill_char,
    char *dst_buf,
    unsigned long buf_len)
{
    unsigned long digit;
    unsigned long len;
    long ret_len;
    char c;
    int negative = 0;

    if(buf_len == 0)
    {
        ret_len = 0;

        return ret_len;
    }

    if(width == 0)
    {
        width++;
    }

    len = 0;

    /* Push the "minus" in */
    if (val < 0) {
	    *dst_buf = '-';
	    dst_buf++;
	    len++;
	    width--;
	    val = -val;
	    negative = 1;
    }

    for(; ;)
    {
        if(len >= buf_len)
        {
            len = 0;
            break;
        }

        digit = val % 10;
        val = val/10;

        *dst_buf = '0' + (char) digit;
        dst_buf++;

        len++;
        width--;

        if(val == 0)
        {
            break;
        }
    }

    while(width > 0)
    {
        if(len >= buf_len)
        {
            len = 0;
            break;
        }

        *dst_buf = fill_char;
        dst_buf++;
        len++;
        width--;
    }

    ret_len = len;

    if(len)
    {
        /* Reverse the digits. */
        digit = 1;
	len -= negative;
        while(digit < len)
        {
            c = *(dst_buf-digit);
            *(dst_buf-digit) = *(dst_buf-len);
            *(dst_buf-len) = c;

            len--;
            digit++;
        }
    }

    return ret_len;
} /* debug_ltod */



/*******************************************************************************
 * Description:
 *
 * Return:
 ******************************************************************************/
STATIC long
debug_atol(
    char *buf,
    unsigned long *ret_val)
{
    unsigned long result;
    long len;

    result = 0;
    len = 0;

    while(*buf >= '0' && *buf <= '9')
    {
        result = result * 10 + *buf-'0';
        buf++;
        len++;
    }

    *ret_val = result;

    return len;
} /* debug_atol */



/*******************************************************************************
 * Description:
 *
 * Return:
 ******************************************************************************/
STATIC long
debug_strcpy(
    char *src_buf,
    char *dst_buf,
    unsigned long buf_len)
{
    long len;

    len = 0;
    while(*src_buf && (len < (long) buf_len))
    {
        *dst_buf = *src_buf;
        dst_buf++; src_buf++;
        len++;
    }

    /* Return an error if buf_len is too short. */
    if(*src_buf)
    {
        len = -1;
    }

    return len;
} /* debug_strcpy */



/*******************************************************************************
 * Description:
 *
 * Return:
 ******************************************************************************/
STATIC unsigned long
debug_sprintf(
    char *buf,
    unsigned long buf_len,
    char *format,
    va_list args)
{
    unsigned long len;
    long ret_len;

    len = 0;
    while(*format && len < buf_len)
    {
        if(*format == '%')
        {
            format++;

            switch(*format)
            {
                case '%':
                    *buf = '%'; buf++; len++;
                    break;

                case 's':
                case 'S':
                {
                    char *str_arg;
		    char *null_string="<NULL>";

                    str_arg = va_arg(args, char *);
                    if ( ! str_arg ) {
                        str_arg = null_string;
                    }

                    ret_len = debug_strcpy(str_arg, buf, buf_len - len);
                    if(ret_len == -1)
                    {
                        len = 0;
                    }
                    else
                    {
                        buf += ret_len;
                        len += ret_len;
                    }

                    break;
                }

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                case 'd': case 'D':
                case 'x': case 'X':
                {
                    unsigned long width;
                    char fill_char;

                    if(*format == '0')
                    {
                        fill_char = '0';
                    }
                    else
                    {
                        fill_char = ' ';
                    }
                    width = 0;

                    if(*format >= '0' && *format <= '9')
                    {
                        ret_len = debug_atol(format, &width);
                        format += ret_len;
                    }

                    if(*format == 'd' || *format == 'D')
                    {
			long int_arg;
                        int_arg = va_arg(args, int);
                        ret_len = debug_ltod(int_arg, width, fill_char, buf,
                            buf_len - len);
                    }
                    else if(*format == 'x' || *format == 'X')
                    {
		        unsigned long int_arg;
                        int_arg = va_arg(args, unsigned long);
                        ret_len = debug_ltoh(int_arg, width, fill_char, buf,
                            buf_len - len);
                    }
                    else
                    {
                        ret_len = -1;
                    }

                    if(ret_len == -1)
                    {
                        len = 0;
                    }
                    else
                    {
                        buf += ret_len;
                        len += ret_len;
                    }

                    break;
                }

                /* Add 'p' for pointer; self sizing for 32 & 64 bit. */
                case 'P': case 'p':
                {
                    unsigned long int_arg;
                    unsigned long width;
                    char fill_char;

                    /* Always use "0x" prefix... */
                    *buf = '0'; buf++; len++;
                    if(len < buf_len)
                    {
                        *buf = 'x'; buf++; len++;
                    }
                    fill_char = '0';
                    width = 2*sizeof(void *);
                    int_arg = va_arg(args, unsigned long);

                    ret_len = debug_ltoh(
                        int_arg,
                        width,
                        fill_char,
                        buf,
                        buf_len - len);

                    if(ret_len == -1)
                    {
                        len = 0;
                    }
                    else
                    {
                        buf += ret_len;
                        len += ret_len;
                    }

                    break;
                }

                default:
                    break;
            }
        }
        else
        {
            *buf = *format;
            buf++;
            len++;
        }

        format++;

        /* Error encountered. */
        if(len == 0)
        {
            break;
        }
    }

    *buf = 0;

    return len;
} /* debug_printf */

void os_if_set_dbg_break(void)
{
	dbg_break_flag = 1;
	smp_wmb();
}

int os_if_is_dbg_break(void)
{
	return dbg_break_flag;
}

void os_if_set_ctl_c_pressed(void)
{
	ctl_c_flag = 1;
	smp_wmb();
}

int os_if_get_ctl_c_pressed(void)
{
	return ctl_c_flag;
}

int os_if_get_async_hdlr_cnt(void)
{
	return async_hdlr_cnt;
}

void os_if_set_async_hdlr_cnt(int cnt)
{
	async_hdlr_cnt = cnt;
	smp_wmb();
}

/*******************************************************************************
 * Description:
 *
 * Return:
 ******************************************************************************/
void
debug_break(void)
{
    if (!os_if_is_dbg_break()) {
	    printk(KERN_ERR"debug_break is called...\n");
	    dump_stack();
	    os_if_set_dbg_break();
	    evst_send_sigio_all();
    }
} /* debug_break */


/*******************************************************************************
 * Description:
 *
 * Return:
 ******************************************************************************/
void __cdecl
debug_msg(
    void *ctx,
    unsigned long level,
    char *file,
    unsigned long line,
    char *msg,
    ...)
{
    unsigned long len;
    char buf[256];
    va_list args;
    char *fname;

    /* Get the filename and ignore the path.  Go to the end of the string. */
    fname = file;
    while(*fname)
    {
        fname++;
    }

    len = 0;

    /* Find the first '\' or '/' starting from the end of the string. */
    while(fname > file)
    {
        if(*fname == '\\' || *fname == '/')
        {
            fname++;
            break;
        }

        len++;
        fname--;
    }

    if((level & CP_MASK) && (level & LV_MASK) <= DBG_MSG_LV)
    {
        va_start(args, msg);
        len = debug_sprintf(buf, sizeof(buf), msg, args);
        if(len)
        {
            if((level & LV_MASK) == LV_FATAL)
            {
#ifdef LINUX
                printk(KERN_ERR "(%11s, %4d): PANIC!!! %s", fname, (int)line, buf);
                //pal_print("(%11s, %4d): PANIC!!! %s", fname, (int)line, buf);
#else
                DbgPrint("(%11s, %4d): PANIC!!! %s", fname, line, buf);
#endif
            }
            else if((level & LV_MASK) == LV_WARN)
            {
#ifdef LINUX
                printk(KERN_WARNING "(%11s, %4d): WARNING!!! %s", fname, (int)line, buf);
                //pal_print("(%11s, %4d): WARNING!!! %s", fname, (int)line, buf);
#else
                DbgPrint("(%11s, %4d): WARNING!!! %s", fname, line, buf);
#endif
            }
            else
            {
                if(*buf == '#')
                {
#ifdef LINUX
                    printk("(%11s, %4d): %s", fname, (int)line, buf);
                    //pal_print("(%11s, %4d): %s", fname, (int)line, buf);
#else
                    DbgPrint("(%11s, %4d): %s", fname, (int)line, buf);
#endif
                }
                else
                {
#ifdef LINUX
                    printk("(%11s, %4d):    %s", fname, (int)line, buf);
                    //pal_print("(%11s, %4d):    %s", fname, (int)line, buf);
#else
                    DbgPrint("(%11s, %4d):    %s", fname, (int)line, buf);
#endif
                }
            }
        }
        else
        {
#ifdef LINUX
            printk(KERN_CRIT "(%11s, %4d): DEBUG_MSG_FAILED!!! %s", fname, (int)line, msg);
            //pal_print("(%11s, %4d): DEBUG_MSG_FAILED!!! %s", fname, (int)line, msg);
#else
            DbgPrint("(%11s, %4d): DEBUG_MSG_FAILED!!! %s", fname, (int)line, msg);
#endif
        }
        va_end(args);
    }
} /* debug_msg */


/*******************************************************************************
 * Description:
 *
 * Return:
 ******************************************************************************/
void __cdecl
debug_msgx(
	unsigned long level,
        char *msg,
        ...)
{
    unsigned long len;
    char buf[256];
    va_list args;

    if(LOG_MSG(level))
    {
        va_start(args, msg);
        len = debug_sprintf(buf, sizeof(buf), msg, args);
        if(len && MSG_LEVEL(level))
        {
#ifdef LINUX
            printk("%s", buf);
            //pal_print("%s", buf);
#else
            DbgPrint("%s", buf);
#endif
        }
        va_end(args);
    }
} /* debug_msgx */



#endif
