#pragma once

#include <cstdio>
#include "../../lib/staged_rewiring.tcc"

template<typename P, typename T>
class rewired_deque {
public:
    staged_rewiring sr;
    //points to the first element in the dequeue
    T *head;
    //points after the last element in the queue
    T *tail;

    T *mapping_start;
    T *mapping_end;

    void reorganize() {
        T *oldStartMapping = mapping_start;
        size_t freePagesBefore = free_before();
        size_t freePagesAfter = free_after();
        if (freePagesAfter + freePagesBefore < 2 || freePagesAfter + freePagesBefore < sr.getNumPages() / 4) {
            freePagesAfter += sr.getNumPages() / 3;
            sr.resize(sr.getNumPages() + sr.getNumPages() / 3);
            mapping_start = (T *) sr.getMapping();
            mapping_end = (T *) ((P*)sr.getMapping() + sr.getNumPages());
        }
        size_t freeTotal = freePagesAfter + freePagesBefore;
        size_t usedTotal = sr.getNumPages() - freeTotal;
        P *oldHeadPage = (P*)sr.getMapping() + freePagesBefore;
        P *newHeadPage = (P*)sr.getMapping() + freeTotal / 2;
        movePages(oldHeadPage, newHeadPage,usedTotal);
        long toMove = oldHeadPage - newHeadPage;
        head = head + (mapping_start - oldStartMapping);
        tail = tail + (mapping_start - oldStartMapping);
        head = mov(head, static_cast<size_t>(-toMove));
        tail = mov(tail, static_cast<size_t>(-toMove));
    }
    uint8_t prefault(){
        uint8_t res=0;
        for(size_t i=0;i<sr.getNumPages();i++){
            res^=((uint8_t*)mapping_start)[i*4096];
        }
        return res;
    }

    void movePages(P *destPage, P *srcPage, size_t nPages) {
        long toMove = destPage - srcPage;
        //move the pages by rewiring
        sr.stage_rewiring(srcPage, destPage, nPages);
        P *newFollowingPage = srcPage + nPages;
        P *oldFollowingPage = destPage + nPages;
        //rewire now hidden pages to fill the gap
        if (toMove < 0) {
            sr.stage_rewiring(destPage, oldFollowingPage, static_cast<size_t>(-toMove));
        } else if (toMove > 0) {
            sr.stage_rewiring(newFollowingPage, destPage, static_cast<size_t >(toMove));
        }
        sr.commit_rewirings();
    }

    size_t free_after() const { return ((mapping_end - tail) * sizeof(T)) / sizeof(P); }

    size_t free_before() const { return ((head - mapping_start) * sizeof(T)) / sizeof(P); }

    T *mov(T *before, size_t movedPages) {
        return (T *) (((uint8_t *) before) + movedPages * sizeof(P));
    }

public:
    explicit rewired_deque(bool use_lkm=true) : sr(use_lkm) {
        sr.resize(3);
        mapping_start = (T *) sr.getMapping();
        mapping_end = (T *) ((P*)sr.getMapping() + sr.getNumPages());
        size_t elements = mapping_end - mapping_start;
        size_t half = elements / 2;
        head = mapping_start + half;
        tail = head;
    }

    T &operator[](const size_t index) const {
        return head[index];
    }

    size_t size() const {
        return tail - head;
    }

    void push_back(const T &value) {
        if (tail + 1 > mapping_end) {
            reorganize();
        }
        *(tail++) = value;
    }

    void push_front(const T &value) {
        if (head - 1 <= mapping_start) {
            reorganize();
        }
        *(--head) = value;
    }

    void pop_back() {
        if (head != tail) {
            tail--;
        }
    }

    void pop_front() {
        if (head != tail) {
            head++;
        }
    }

    T &front() const {
        return *head;
    }

    T &back() const {
        return *(tail - 1);
    }


};