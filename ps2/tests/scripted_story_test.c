/* CI-only host check for synthetic PS2 gameplay navigation.
 * This is NOT hardware controller validation. */
#include <stdio.h>
#include <stdint.h>
#include "scripted_story.h"

int main(void)
{
    uint32_t t;
    unsigned pulses=0;
    for(t=0;t<600u;t++){
        uint32_t b=ps2_scripted_story_buttons(t);
        uint32_t want=(t==120u)?BTN_START:
                      ((t==150u||t==180u)?BTN_A:0u);
        if(b!=want){
            fprintf(stderr,"scripted PS2 input at %u: got %08lx expected %08lx\n",
                    (unsigned)t,(unsigned long)b,(unsigned long)want);
            return 1;
        }
        if(b)pulses++;
        if((t==121u||t==151u||t==181u) && b!=0u)return 2;
    }
    if(pulses!=3u)return 3;
    puts("PS2 scripted title-to-story navigation: 3 isolated pulses PASS (not real pad input)");
    return 0;
}
