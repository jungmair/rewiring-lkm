#pragma once
#include<cstdint>
#include <sys/mman.h>
#include <cerrno>
#include <system_error>
//definitions for simplifications
typedef uint32_t PageId;
typedef struct {
    char data[4096];
} Page;
//abstract base class for rewiring
class rewiring{
protected:
    //start of mapping
    void* mapping = nullptr;
    //number of pages of the mapping
    size_t num_pages = 0;
    //a page id array that stores the mapping in a suitable form
    PageId* pageIds = nullptr;
    //check if mmap call worked, if not -> throw exception
    void check_mmap_result(void* res){
        if(res==MAP_FAILED){
            throw std::system_error(errno, std::generic_category(), "mmap failed");
        }
    }
    //check if munmap call worked, ifn not -> throw exception
    void check_munmap_result( int res){
        if(res!=0){
            throw std::system_error(errno, std::generic_category(), "munmap failed");
        }
    }
public:

    static constexpr size_t page_size=4096;

    //virtual functions to be implemented by the concrete rewiring class
    virtual void resize(size_t pages)=0;
    virtual void syncFromPT(size_t start,size_t len)=0;
    virtual void syncToPT(size_t start,size_t len)=0;
    virtual void createNewPageIds(size_t num,size_t* positions,PageId* array)=0;
    virtual ~rewiring() = default;

    size_t getNumPages() const {
        return num_pages;
    }

    void *getMapping() const {
        return mapping;
    }

    PageId *getPageIds() const {
        return pageIds;
    }
    //static method for creating a rewiring object, if wished (and module inserted)-> lkm-based, otherwise mmap-based
    static rewiring* create(bool use_lkm=true);
};


#include <fstream>
#include <iostream>
#include "lkm_rewiring.tcc"
#include "mmap-rewiring.tcc"

rewiring* rewiring::create(bool use_lkm){
    //check if /dev/rewiring exists
    std::ifstream f("/dev/rewiring");
    bool lkm_present=f.good();
    f.close();
    if(lkm_present&&use_lkm){
        //create new lkm-based rewiring
        return new lkm_rewiring();
    }else{
        //fall back to mmap-based rewiring

        //print warning message
        std::cerr<<"WARNING: Falling back to mmap-based rewiring!!!"<<std::endl;
        //retrieve and print vm.max_map_count
        std::ifstream f2("/proc/sys/vm/max_map_count");
        size_t vm_max_map_count;
        f2>>vm_max_map_count;
        std::cerr<<"WARNING: rewiring capabilities are limited by vm.max_map_count="<<vm_max_map_count<<std::endl;
        //create new mmap-based rewiring
        return new mmap_rewiring();
    }
}

