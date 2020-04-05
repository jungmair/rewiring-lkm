
#include "util/deque.h"
#include <iostream>
#include <deque>
#include <cassert>
#include <chrono>
#include "emmintrin.h"
//the three deque implementations for benchmarking:
enum deque_impl{
    REWIRED_LKM,REWIRED_MMAP,STD
};
size_t bench(deque_impl impl,size_t num_entries,size_t shifts) {
    if(impl==STD){
        std::deque<size_t> q;
        auto start = std::chrono::system_clock::now();
        //first insert num_entries elements into deque
        for (size_t i = 0; i < num_entries; i++) {
            q.push_back(i);
        }
        //second: perform shifts
        for (size_t i = 0; i < shifts; i++) {
            q.push_back(i);
            q.pop_front();
        }
        auto end = std::chrono::system_clock::now();
        //return measured time in nanoseconds
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }else {
        rewired_deque<Page, size_t> q(impl==REWIRED_LKM);
        auto start = std::chrono::system_clock::now();
        //first insert num_entries elements into deque
        for (size_t i = 0; i < num_entries; i++) {
            q.push_back(i);
        }
        //second: perform shifts
        for (size_t i = 0; i < shifts; i++) {
            q.push_back(i);
            q.pop_front();
        }
        auto end = std::chrono::system_clock::now();
        //return measured time in nanoseconds
        return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    }
}


int main(){
    //benchmark config: #entries=100000000, #shifts=1000000000
    size_t numEntries=100000000;
    size_t shifts=1000000000;
    //execute benchmark for the three deque implementations
    size_t lkm=bench(REWIRED_LKM, numEntries,shifts);
    size_t mmap=bench(REWIRED_MMAP, numEntries,shifts);
    size_t std=bench(STD, numEntries,shifts);
    //write result as csv
    std::ofstream out("result.csv");
    out<<"type;time"<<std::endl;
    out << "lkm" << ";"  << lkm << std::endl;
    out << "mmap" << ";"  << mmap << std::endl;
    out << "std" << ";"  << std << std::endl;
    return 0;
}