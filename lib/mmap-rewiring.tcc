#pragma once

#include <cstring>
#include <climits>
#include <linux/memfd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
class mmap_rewiring: public rewiring{
    int fd;
public:
    mmap_rewiring() {
        //create main memory file
        fd = memfd_create("ramfile", 0);
        if (fd == -1) {
            throw std::system_error(errno, std::generic_category(), "creation of ramfile failed");
        }
        //set length to max long -> we do not have to care about file length anymore
        if (ftruncate(fd, LONG_MAX) == -1) {
            throw std::system_error(errno, std::generic_category(), "ftruncate failed");
        }
    }
    virtual void resize(size_t pages){
        size_t oldNumPages=num_pages;
        if(mapping!=NULL) {
            //unmap old mapping
            check_munmap_result(munmap(mapping,num_pages*page_size));
        }
        //resize page ids
        PageId* newPageIds=new PageId[pages];
        if(pageIds) {
            std::memcpy(newPageIds, pageIds, sizeof(PageId) * std::min(num_pages, pages));
            delete[] pageIds;
        }
        for(size_t i=num_pages;i<pages;i++){
            newPageIds[i]=i;
        }
        //update information
        pageIds=newPageIds;
        num_pages=pages;
        //create new larger mapping
        mapping = mmap(NULL, pages * page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        check_mmap_result(mapping);
        //redo "rewire" using the stored page ids
        syncToPT(0,oldNumPages);
    }
    virtual void syncFromPT(size_t /*start*/,size_t /*len*/){
        //for mmap-based mapping, there is no external state
    };
    virtual void syncToPT(size_t start,size_t len){
        //update mapping by creating as much mmaps as required
        Page* mapping_= static_cast<Page *>(mapping);
        //try to "squeeze" as much pages into one mmap call as possible
        //->increment delayed and proceed with next page
        size_t delayed=0;

        for(size_t i=0;i<len;i++) {
            //can we delay the mmap call?
            if((i+1<len)&&pageIds[start+i]+1==pageIds[start+i+1]){
                //yes -> only increment delayed
                delayed++;
                continue;
            }
            //no: call mmap for delayed+1 pages
            check_mmap_result(mmap(&mapping_[start+i-delayed], page_size * (1+delayed), PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, fd,
                             pageIds[start+i-delayed] * page_size));
            //reset delayed
            delayed=0;
        }
    }
    virtual void createNewPageIds(size_t num,size_t* positions,PageId* array){
        //for mmap-based mapping, there is no "explicit" way for "creating" page ids
        //to create a mostly linear mapping: use provided positions as page ids
        for(size_t i=0;i<num;i++){
            array[i]=positions[i];
        }
    }

    ~mmap_rewiring(){
        //cleanup: unmap mapping,close file decriptor, free page id array
        if(mapping){
            check_munmap_result(munmap(mapping,num_pages*page_size));
        }
        close(fd);
        if(pageIds) {
            delete[] pageIds;
        }
    }
};