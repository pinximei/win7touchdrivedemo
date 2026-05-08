// touchcheck.cpp — print Win7 touch capability flags as seen by win32k.
#include <windows.h>
#include <stdio.h>

#ifndef SM_DIGITIZER
#define SM_DIGITIZER         94
#define SM_MAXIMUMTOUCHES    95
#define NID_INTEGRATED_TOUCH 0x01
#define NID_EXTERNAL_TOUCH   0x02
#define NID_INTEGRATED_PEN   0x04
#define NID_EXTERNAL_PEN     0x08
#define NID_MULTI_INPUT      0x40
#define NID_READY            0x80
#endif

int main()
{
    int dig  = GetSystemMetrics(SM_DIGITIZER);
    int maxt = GetSystemMetrics(SM_MAXIMUMTOUCHES);
    printf("SM_DIGITIZER         = 0x%X\n", dig);
    printf("  NID_READY           = %d\n", (dig & NID_READY)         != 0);
    printf("  NID_MULTI_INPUT     = %d\n", (dig & NID_MULTI_INPUT)   != 0);
    printf("  NID_INTEGRATED_TOUCH= %d\n", (dig & NID_INTEGRATED_TOUCH) != 0);
    printf("  NID_EXTERNAL_TOUCH  = %d\n", (dig & NID_EXTERNAL_TOUCH)   != 0);
    printf("  NID_INTEGRATED_PEN  = %d\n", (dig & NID_INTEGRATED_PEN)   != 0);
    printf("  NID_EXTERNAL_PEN    = %d\n", (dig & NID_EXTERNAL_PEN)     != 0);
    printf("SM_MAXIMUMTOUCHES    = %d\n", maxt);
    return 0;
}
