#include <stdio.h>
#include <string.h>
#include "talk_path.h"
static int checks,failures;
#define CHECK(x) do {checks++;if(!(x)){failures++;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
int main(void)
{
    char out[24];
    memset(out,0x5a,sizeof out);
    CHECK(ps2_talk_path(out,"en"));
    CHECK(strcmp(out,"\\TALK_EN.BIN;1")==0);
    CHECK(out[23]==0x5a);
    CHECK(ps2_talk_path(out,"es"));
    CHECK(strcmp(out,"\\TALK_ES.BIN;1")==0);
    CHECK(ps2_talk_path(out,"abc123"));
    CHECK(strcmp(out,"\\TALK_ABC123.BIN;1")==0);
    CHECK(!ps2_talk_path(out,NULL));
    CHECK(!ps2_talk_path(NULL,"en"));
    CHECK(!ps2_talk_path(out,"e"));
    CHECK(!ps2_talk_path(out,"abcdefg"));
    CHECK(!ps2_talk_path(out,"../en"));
    CHECK(!ps2_talk_path(out,"en/foo"));
    CHECK(!ps2_talk_path(out,"en;1"));
    CHECK(!ps2_talk_path(out,"en.bin"));
    printf("PS2 Talk ISO path: %d checks, %d failed\n",checks,failures);
    return failures!=0;
}
