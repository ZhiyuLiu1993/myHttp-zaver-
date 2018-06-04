//
// Created by root on 5/22/18.
//

#include <priority_queue.h>
#include "priority_queue.h"

int my_pq_init(my_pq_t *my_pq, my_pq_comparator_pt comp, size_t size){
    my_pq->pq = (void **)malloc(sizeof(void *) * (size + 1));
    if(!my_pq->pq){
        log_err("my_pq_init: malloc failed");
        return -1;
    }

    my_pq->nalloc = 0;
    my_pq->size = size + 1;
    my_pq->comp = comp;

    return MY_OK;
}

//FIXME: 改为bool类型是否更好
int my_pq_is_empty(my_pq_t *my_pq){
    return (my_pq->nalloc == 0) ? 1 : 0;
}

size_t my_pq_size(my_pq_t *my_pq){
    return my_pq->nalloc;
}

void *my_pq_min(my_pq_t *my_pq){
    if(my_pq_is_empty(my_pq)){
        return NULL;
    }

    return my_pq->pq[1];
}

static int resize(my_pq_t *my_pq, size_t new_size){
    if(new_size <= my_pq->nalloc){
        log_err("resize: new_size is too small");
        return -1;
    }

    void **new_ptr = (void **)malloc(sizeof(void *) * new_size);
    if(!new_ptr) {
        log_err("resize: malloc falied");
        return -1;
    }

    memcpy(new_ptr, my_pq->pq, sizeof(void *) * (my_pq->nalloc + 1));
    free(my_pq->pq);
    my_pq->pq = new_ptr;
    my_pq->size = new_size;
    return MY_OK;
}

static void exch(my_pq_t *my_pq, size_t i, size_t j){
    void *tmp = my_pq->pq[i];
    my_pq->pq[i] = my_pq->pq[j];
    my_pq->pq[j] = tmp;
}

//自下而上进行调整，将新插入的值up
static void swim(my_pq_t *my_pq, size_t k){
    while(k > 1 && my_pq->comp(my_pq->pq[k], my_pq->pq[k/2])){
        exch(my_pq, k, k/2);
        k /= 2;
    }
}

//将k位置的数调整为当前及之后最小的
//自上而下的调整过程
static size_t sink(my_pq_t *my_pq, size_t k){
   size_t j;
   size_t nalloc = my_pq->nalloc;

   while(2*k <= nalloc){
       j = 2*k;
       if(j < nalloc && my_pq->comp(my_pq->pq[j+1], my_pq->pq[j]))
           ++j;
       if(!my_pq->comp(my_pq->pq[j], my_pq->pq[k]))
           break;
       exch(my_pq, j, k);
       k = j;
   }

   return k;
}

int my_pq_delmin(my_pq_t *my_pq){
    //队列为空 应该提示错误信息
    if(my_pq_is_empty(my_pq)){
        return MY_OK;
    }

    exch(my_pq, 1, my_pq->nalloc);
    --my_pq->nalloc;
    sink(my_pq, 1);
    //这里的resize可能会降低效率，是否应该由用户主动选择
    if(my_pq->nalloc > 0 && my_pq->nalloc <= (my_pq->size - 1) / 4){
        if(resize(my_pq, my_pq->size / 2) < 0){
            return -1;
        }
    }

    return MY_OK;
}

//nalloc+1 = size 表示队列已满 下标从1开始
int my_pq_insert(my_pq_t *my_pq, void *item){
    if(my_pq->nalloc + 1 == my_pq->size){
        //扩充时选择1.5倍在多次扩容后可以重复利用之前的内存, 但是最好先考虑好使用空间以提升效率
        if(resize(my_pq, my_pq->size * 2) < 0){
            return -1;
        }
    }

    my_pq->pq[++my_pq->nalloc] = item;
    swim(my_pq, my_pq->nalloc);

    return MY_OK;
}

size_t my_pq_sink(my_pq_t *my_pq, size_t i){
    return sink(my_pq, i);
}
