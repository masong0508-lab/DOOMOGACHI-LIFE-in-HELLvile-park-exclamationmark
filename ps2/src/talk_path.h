/* ISO9660 Talk filename constructor, used by native tests and PS2 CD.
 * Only simple ASCII language codes become a disc path. Never allow
 * traversal or a caller-controlled file outside TALK_*.BIN.
 */
#ifndef DOOMO_PS2_TALK_PATH_H
#define DOOMO_PS2_TALK_PATH_H
#include <stddef.h>
static inline int ps2_talk_path(char out[24],const char *code)
{
    size_t i,n=0;
    if(!out||!code)return 0;
    while(code[n] && n<7u)n++;
    if(n<2u || n>6u || code[n])return 0;
    out[0]='\\';
    out[1]='T';out[2]='A';out[3]='L';out[4]='K';out[5]='_';
    for(i=0;i<n;i++){
        char ch=code[i];
        if(ch>='a'&&ch<='z')ch=(char)(ch-'a'+'A');
        if(!((ch>='A'&&ch<='Z')||(ch>='0'&&ch<='9')))return 0;
        out[6+i]=ch;
    }
    out[6+n]='.';out[7+n]='B';out[8+n]='I';
    out[9+n]='N';out[10+n]=';';out[11+n]='1';
    out[12+n]=0;
    return 1;
}
#endif
