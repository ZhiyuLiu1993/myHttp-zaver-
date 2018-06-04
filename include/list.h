//
// Created by root on 5/23/18.
//

#ifndef MYHTTP_LIST_H
#define MYHTTP_LIST_H

#ifndef NULL
#define NULL 0
#endif

//这部分代码参考的是linux内核实现
//对外提供的都是宏，内部使用静态内联函数，防止多重定义

//链表是一个双向链表
//只有头部信息，具体使用时还需要封装一个value
struct list_head{
    struct list_head *prev;
    struct list_head *next;
};

typedef struct list_head list_head;

#define INIT_LIST_HEAD(ptr) do{\
    struct list_head *_ptr = (struct list_head *)ptr; \
    (_ptr)->next = (_ptr); \
    (_ptr->prev) = (_ptr); \
} while(0)


static inline void __list_add(list_head *_new, list_head *prev, list_head *next){
    _new->next = next;
    next->prev = _new;
    prev->next = _new;
    _new->prev = prev;
}

static inline void list_add(list_head *_new, list_head *head){
    __list_add(_new, head, head->next);
}

static inline void list_add_tail(list_head *_new, list_head *head){
    __list_add(_new, head->prev, head);
}

static inline void __list_del(list_head *prev, list_head *next){
    prev->next = next;
    next->prev = prev;
}

static inline void list_del(list_head *entry){
    __list_del(entry->prev, entry->next);
}

static inline int list_empty(list_head *head){
    return (head->next == head) && (head->prev == head);
}

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

//下面这个宏
//通过结构体中某个成员的首地址获得整个结构体的首地址
//typeof 根据变量获取变量类型
#define container_of(ptr, type, member) ({ \
    const typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type, member)); \
})

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

//从前往后
#define list_for_each(pos, head) \
    for(pos = (head)->next; pos != (head); pos = pos->next)

//从后往前
#define list_for_each_prev(pos, head) \
    for(pos = (head)->prev; pos != (head); pos = pos->prev)

#define list_for_each_entry(pos, head, member) \
    for(pos = list_entry((head)->next, typeof(*pos), member); \
        &(pos->member) != (head); \
        pos = list_entry(pos->member.next, typeof(*pos), member))

#endif //MYHTTP_LIST_H
