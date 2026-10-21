#include "boot_journal.h"
bool boot_journal_next(const boot_storage_t *s,uint32_t *counter){
    bool have=false;uint32_t best=0;unsigned best_page=0;
    for(unsigned p=0;p<2;p++)for(unsigned w=0;w<BOOT_PAGE_WORDS;w+=2){
        uint32_t n=s->pages[p][w],inv=s->pages[p][w+1];
        if(n && n!=UINT32_MAX && inv==~n && (!have || (int32_t)(n-best)>0)){best=n;best_page=p;have=true;}
    }
    unsigned page=have?best_page:0,slot=BOOT_PAGE_WORDS;
    for(unsigned w=0;w<BOOT_PAGE_WORDS;w+=2)if(s->pages[page][w]==UINT32_MAX && s->pages[page][w+1]==UINT32_MAX){slot=w;break;}
    if(slot==BOOT_PAGE_WORDS){page=1u-page;if(!s->erase(s->pages[page]))return false;slot=0;}
    uint32_t next=best+1u;if(next==0 || next==UINT32_MAX)next=1;
    if(!s->program(s->pages[page]+slot,next) || !s->program(s->pages[page]+slot+1,~next))return false;
    if(s->pages[page][slot]!=next || s->pages[page][slot+1]!=~next)return false;
    *counter=next;return true;
}
