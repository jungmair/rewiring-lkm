#include <vector>
#include <sys/mman.h>
#include <iostream>
#include "../lib/rewiring.tcc"
#include<chrono>
#include "emmintrin.h"
std::pair<size_t,size_t> bench(bool use_lkm,size_t num_pages){
    //1. create rewiring instance
    rewiring* r=rewiring::create(use_lkm);
    //2. measure time for'all-to-one' setup
    auto start=std::chrono::system_clock::now();
    r->resize(num_pages);
    auto* m= static_cast<uint8_t *>(r->getMapping());
    PageId firstPageId;
    size_t positions[2]{0,1};
    r->createNewPageIds(1,positions,&firstPageId);
    for(size_t i=0;i<num_pages;i++){
        r->getPageIds()[i]=firstPageId;
    }
    r->syncToPT(0,1);
    m[0]=1;
    _mm_mfence();
    r->syncToPT(0,num_pages);
    auto end=std::chrono::system_clock::now();
    //store 'all-to-one' setup time
    size_t setup=std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
    //3. measure time for 'all-to-one' iteration
    start=std::chrono::system_clock::now();
    //access the first byte of every page (and check result)
    for(size_t i=0;i<num_pages;i++){
        if(m[i*4096]!=1)std::cout<<"wrong:"<<i<<std::endl;
    }
    end=std::chrono::system_clock::now();
    //store time for iteration
    size_t iter=std::chrono::duration_cast<std::chrono::nanoseconds>(end-start).count();
    //cleanup
    delete r;
    //return both times
    return {setup,iter};
}
void perform_bench(size_t num_pages,std::ofstream& out){
    //perform 'all-to-one' benchmark for num_pages pages
    //to avoid errors in measurements, execute every benchmark 10 times and take the minimum

    //numbers for lkm/mmap
    size_t lkm_setup=std::numeric_limits<size_t>::max();
    size_t mmap_setup=std::numeric_limits<size_t>::max();
    size_t lkm_iter=std::numeric_limits<size_t>::max();
    size_t mmap_iter=std::numeric_limits<size_t>::max();

    //perform 10x benchmark for lkm
    for(int i=0;i<10;i++) {
        auto p=bench(true, num_pages);
        lkm_setup = std::min(lkm_setup,p.first);
        lkm_iter = std::min(lkm_iter,p.second);

    }
    //perform 10x benchmark for mmap
    for(int i=0;i<10;i++) {
        auto p=bench(false, num_pages);
        mmap_setup = std::min(mmap_setup,p.first);
        mmap_iter = std::min(mmap_iter,p.second);
    }
    //write to CSV
    out<<num_pages<<";"<<lkm_setup<<";"<<lkm_iter<<";"<<mmap_iter<<";"<<mmap_setup<<std::endl;
}
int main(){
    std::ofstream out("result.csv");
    out<<"#pages;lkm_setup;lkm_iter;mmap_setup;mmap_iter"<<std::endl;
    //perform benchmarks for 100,1000,10000,100000,1000000,10000000 pages:
    perform_bench(100,out);
    perform_bench(1000,out);
    perform_bench(10000,out);
    perform_bench(100000,out);
    perform_bench(1000000,out);
    perform_bench(10000000,out);

    return 0;
}