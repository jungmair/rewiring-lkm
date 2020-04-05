#pragma once

#include <vector>
#include "rewiring.tcc"

//wrapper class, to perform rewiring in multiple steps similar to the staging process of git
// 1. stage the rewirings
// 2. commit rewirings
//this is especially useful, if e.g. two page ranges should be swapped
class staged_rewiring{
public:
    //responsible rewiring "manager"
    rewiring* r;
    //helper object for storing a staged rewiring
    struct staging{
        size_t start;
        size_t len;
        PageId* pageIds; //which pages should be mapped to the range [start,start+len)
    };
    //store all stagings
    std::vector<staging> staged;
public:
    staged_rewiring(bool use_lkm):staged(){
        r=rewiring::create(use_lkm);
    }
    void resize(size_t pages){
        //forward resize to rewiring
        r->resize(pages);
    }

    void *getMapping() const {
        //forward to rewiring
        return r->getMapping();
    }
    size_t getNumPages(){
        //forward to rewiring
        return r->getNumPages();
    }

    void stage_rewiring(Page *addr, Page *source, size_t n_pages) {
        staging st{
            .start=static_cast<size_t>(addr-(Page*)getMapping()),
                .len=n_pages,
                .pageIds=new PageId[st.len]
        };
        //sync page ids that should be mapped later
        r->syncFromPT(source-(Page*)getMapping(),st.len);
        //store page ids into staging object
        std::memcpy(st.pageIds,&r->getPageIds()[source-(Page*)getMapping()],st.len*sizeof(PageId));
        staged.push_back(st);
    }
    /**
     * commit all staged rewirings
     */
    void commit_rewirings(){
        //apply every staged rewiring
        for(staging st:staged){
            //do rewiring
            std::memcpy(&r->getPageIds()[st.start],st.pageIds,st.len*sizeof(PageId));
            r->syncToPT(st.start,st.len);
            delete[] st.pageIds;
        }
        staged.clear();
    }
    ~staged_rewiring(){
        //free rewiring
       delete r;
    }

};