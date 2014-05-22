#ifndef _TRACE_H
#define _TRACE_H

#include <stdio.h>
#include <ctype.h>

#if defined(_DEBUG) && !defined(_SUPRESS_TRACE)
#   define PTRACE(fmt)                printf(fmt)
#   define PTRACE1(fmt,s0)            printf(fmt,s0)
#   define PTRACE2(fmt,s0,s1)         printf(fmt,s0,s1)
#   define PTRACE3(fmt,s0,s1,s2)      printf(fmt,s0,s1,s2)
#   define PTRACE4(fmt,s0,s1,s2,s3)   printf(fmt,s0,s1,s2,s3)
#else
#   define PTRACE(fmt)
#   define PTRACE1(fmt,s0)
#   define PTRACE2(fmt,s0,s1)
#   define PTRACE3(fmt,s0,s1,s2)
#   define PTRACE4(fmt,s0,s1,s2,s3)
#endif

//useful definition
#ifndef MILLION
# define MILLION 1000000L
#endif

#ifndef BILLION
# define BILLION (1000*MILLION)
#endif
//power of two constants
#define KILO (1024)
#define MEG  (KILO*KILO)
#define GIG  (MEG*KILO)

#define PERROR(fmt)                printf("%s:%d "fmt,__FILE__,__LINE__)
#define PERROR1(fmt,s0)            printf("%s:%d "fmt,__FILE__,__LINE__,s0)
#define PERROR2(fmt,s0,s1)         printf("%s:%d "fmt,__FILE__,__LINE__,s0,s1)
#define PERROR3(fmt,s0,s1,s2)      printf("%s:%d "fmt,__FILE__,__LINE__,s0,s1,s2)
#define PERROR4(fmt,s0,s1,s2,s3)   printf("%s:%d "fmt,__FILE__,__LINE__,s0,s1,s2,s3)

/** @brief dumps a buffer to the trace output */
#ifdef __cplusplus
 extern "C" {
#endif

 //quiet unused function warnings
static void HEXDUMP(void *pAddressIn, long  lSize) __attribute__((unused));
//unused variables warning
#define UNUSED(x) (void)(x)
/**
 * This is a debug function for dumping a buffer
 * @param[in] pAddressIn Address of the buffer
 * @param[in] lSize The length of the buffer
 */
static void HEXDUMP(void *pAddressIn, long  lSize)
{
    char szBuf[100];
    long lIndent = 1;
    long lOutLen, lIndex, lIndex2, lOutLen2;
    long lRelPos;
    struct { char *pData; unsigned long lSize; } buf;
    unsigned char *pTmp,ucTmp;
    unsigned char *pAddress = (unsigned char *)pAddressIn;

    buf.pData   = (char *)pAddress;
    buf.lSize   = lSize;

    while (buf.lSize > 0)
    {
        pTmp     = (unsigned char *)buf.pData;
        lOutLen  = (int)buf.lSize;
        if (lOutLen > 16)
            lOutLen = 16;

        // create a 64-character formatted output line:
        sprintf(szBuf, " >                            "
            "                      "
            "    %08lX", pTmp-pAddress);
        lOutLen2 = lOutLen;

        for(lIndex = 1+lIndent, lIndex2 = 53-15+lIndent, lRelPos = 0;
            lOutLen2;
            lOutLen2--, lIndex += 2, lIndex2++
            )
        {
            ucTmp = *pTmp++;

            sprintf(szBuf + lIndex, "%02X ", (unsigned short)ucTmp);
            if(!isprint(ucTmp))  ucTmp = '.'; // nonprintable char
            szBuf[lIndex2] = ucTmp;

            if (!(++lRelPos & 3))     // extra blank after 4 bytes
            {  lIndex++; szBuf[lIndex+2] = ' '; }
        }

        if (!(lRelPos & 3)) lIndex--;

        szBuf[lIndex  ]   = '<';
        szBuf[lIndex+1]   = ' ';

        PTRACE1("%s\n", szBuf);

        buf.pData   += lOutLen;
        buf.lSize   -= lOutLen;
    }
}
#ifdef __cplusplus
 }
#endif

#endif //_TRACE_H

