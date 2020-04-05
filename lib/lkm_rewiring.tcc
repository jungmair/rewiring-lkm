#pragma once

#include <cstring>
#include <linux/memfd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "../module/inc/communication.h"
class lkm_rewiring: public rewiring{
    int fd;

public:
    lkm_rewiring():rewiring(){
        //tries to open rewiring file
        fd= open("/dev/rewiring", O_RDWR);
        // error handling
        if (fd < 0) {
            throw std::system_error(errno, std::generic_category(), "opening of file failed");
        }
    }
    virtual void resize(size_t pages){
        size_t oldNumPages=num_pages;
        //before resizing: fetch current state from module
        syncFromPT(0,num_pages);
        //then: unmap (this will also clear the state in the module
        if(mapping!=NULL) {
            check_munmap_result(munmap(mapping,num_pages*page_size));
        }
        //resize page id array
        PageId* newPageIds=new PageId[pages];
        if(pageIds) {
            std::memcpy(newPageIds, pageIds, sizeof(PageId) * std::min(num_pages, pages));
            delete[] pageIds;
        }
        //update information
        pageIds=newPageIds;
        num_pages=pages;
        //create new mapping
        mapping = mmap(NULL, pages * page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        check_mmap_result(mapping);
        //sync additional page ids from kernel module
        if(oldNumPages<pages) {
            syncFromPT(oldNumPages,num_pages-oldNumPages);
        }
        //sync old page ids to the kernel module
        if(oldNumPages>0) {
            syncToPT(0, oldNumPages);
        }
    }
    virtual void syncFromPT(size_t start,size_t len){
        //send "GET_PAGE_IDS" to kernel module
        if(mapping) {
            struct cmd getPagesCMD = {
                    .type=GET_PAGE_IDS,
                    .start=start,
                    .len=len,
                    .mapping_start=mapping,
                    .payload=&pageIds[start],
            };

            if(ioctl(fd, REW_CMD, &getPagesCMD)!=0){
                throw std::system_error(errno, std::generic_category(), "ioctl failed");
            }
        }

    };
    virtual void syncToPT(size_t start,size_t len){
        //send "SET_PAGE_IDS" command to kernel module, with correct parameters
        struct cmd setPagesCMD = {
                .type=SET_PAGE_IDS,
                .start=start,
                .len=len,
                .mapping_start=mapping,
                .payload=&pageIds[start],
        };
        if(ioctl(fd,REW_CMD,&setPagesCMD)!=0){
            throw std::system_error(errno, std::generic_category(), "ioctl failed");
        }

    }
    virtual void createNewPageIds(size_t num,size_t* /*positions*/,PageId* array){
        //send "CREATE_PAGE_IDS" command to kernel module with right parameters
        struct cmd createPageIds = {
                .type=CREATE_PAGE_IDS,
                .start=0,
                .len=num,
                .mapping_start=mapping,
                .payload=array,
        };
        if(ioctl(fd,REW_CMD,&createPageIds)!=0){
            throw std::system_error(errno, std::generic_category(), "ioctl failed");
        }
    }

    ~lkm_rewiring(){
        //cleanup -> unmap mapping, close fd, free pageid array
        if(mapping){
            check_munmap_result(munmap(mapping,num_pages*page_size));
        }
        close(fd);
        if(pageIds) {
            delete[] pageIds;
        }
    }
};