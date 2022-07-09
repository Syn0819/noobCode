#include "coroutine.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <iostream>
typedef void (*coroutine_func)(schedule*, void *ud);

struct coroutine
{   
    // 协程运行函数
    coroutine_func func;
    // 协程参数
    void *ud;
    // 协程上下文
    ucontext_t ctx;
    // 所属调度器
    schedule* sch;
    // 已经分配的内存大小
    ptrdiff_t cap;
    // 当前协程运行时栈，保存起来后的大小
    ptrdiff_t size;
    // 协程当前的状态
    int status;
    // 当前协程的保存起来的允许时栈
    char *stack;
};


void _co_delete(coroutine* dco) {
    free(dco->stack);
    free(dco);
}

/*
* 协程的创建，与初始化
* 可见，协程实际是在堆上的
*/
coroutine* _co_new(schedule* s, coroutine_func func, void *ud) {
    coroutine* newco = (coroutine*)malloc(sizeof(*newco));
    newco->stack = nullptr;
    newco->status = coroutine_ready;
    newco->ud = ud;
    newco->cap = 0;
    newco->sch = s;
    newco->size = 0;
    newco->func = func;
    return newco;
}

/*
* 删除该调度器
*/
void schedule::coroutine_close() {
    for(int i = 0; i < cap; i++) {
        coroutine* dco = co[i];
        if(dco) _co_delete(dco);
    }

    free(co);
    co = nullptr;
    // 调度器的释放交给析构函数
}

/*
* 创建一个协程对象
* @param func 该协程函数执行体
* @param ud func的参数
* @return 新建协程的id
*/
int schedule::coroutine_new(coroutine_func func, void *ud) {
    coroutine* newco = _co_new(this, func, ud);
	std::cout << "_co_new, nco: " << nco << ", cap: " << cap << std::endl;
    if(nco >= cap) {
		std::cout << "do not space" << std::endl;
        // 如果目前协程数量已经大于等于调度器的容量，那么需要进行扩容
        int id = cap;
        co = (coroutine**)realloc(co, cap * 2 * sizeof(coroutine*));
        memset(co+cap, 0, sizeof(coroutine*) * cap);
		std::cout << "realloc" << std::endl;
        co[cap] = newco;
        cap *= 2;
        ++nco;
        return id;
    } else {
		std::cout << "have space" << std::endl;
        // 找出第一个协程数组中为空的位置放入
        int i;
        for(i = 0; i < cap; ++i) {
            int id = (i+nco) % cap;
            if(co[id] == nullptr) {
                co[id] = newco;
                ++nco;
                return id;
            }
        }
		std::cout << "finish" << std::endl;
    }
    return -1;
}

// 构造函数，初始化调度器，并给协程数组分配空间
schedule::schedule() {
    nco = 0;
    cap = DEFAULT_COROUTINE;
    running = -1;
    co = (coroutine**)malloc(sizeof(coroutine*) * cap);
    memset(co, 0, sizeof(coroutine*) * cap);
}

// 创建一个调度器，放入构造函数处理
void schedule::coroutine_open() {
}

static void mainfunc(uint32_t low32, uint32_t hi32) {
    uintptr_t ptr = (uintptr_t) low32 | ((uintptr_t) hi32 << 32);
    schedule* ss = (schedule*) ptr;
    
    int id = ss->running;
    coroutine* C = ss->co[id];
    C->func(ss, C->ud);
    _co_delete(C);
    ss->co[id] = nullptr;
    --ss->nco;
    ss->running = -1;
}

void schedule::coroutine_resume(int id) {
    assert(running == -1);
    assert(id >= 0 && id < cap);

    coroutine* C = co[id];
    if(C == nullptr) return;

    int status = C->status;
    uintptr_t ptr;
    switch(status) {
    case coroutine_ready:
        // 初始化ucontext_t结构体，将当前的上下文放入C->ctx中
        getcontext(&C->ctx);
        // 将当前协程的运行时栈顶设置为this->stack;
        // 每个协程都这么设置，这就是所谓的共享栈
        // 这里是栈顶
        C->ctx.uc_stack.ss_sp = this->stack;
        C->ctx.uc_stack.ss_size = STACK_SIZE;
        C->ctx.uc_link = &main;
        running = id;
        C->status = coroutine_Isrunning;

        // 设置执行C->ctx函数，将调度器作为参数传进去
        ptr = (uintptr_t) this;
        // 注意传入参数，调度器的指针被分为两半传入
        makecontext(&C->ctx, (void (*)(void)) mainfunc, 2, (uint32_t)ptr, (uint32_t)(ptr>>32));
        // 将当前上下文放入main中，并将C->ctx的上下文替换到当前上下文
        swapcontext(&main, &C->ctx);
        break;
    case coroutine_suspend:
        memcpy(this->stack + STACK_SIZE - C->size, C->stack, C->size);
        this->running = id;
        C->status = coroutine_Isrunning;
        swapcontext(&main, &C->ctx);
        break;
    default:
        assert(0);
    }
}

int schedule::coroutine_running() {
    return this->running;
}

int schedule::coroutine_status(int id) {
    assert(id >= 0 && id < cap);
    if(co[id] == nullptr) {
        return coroutine_dead;
    } 
    return co[id]->status;
}

static void _save_stack(coroutine* C, char* top) {
    char dummy = 0;
    assert(top-&dummy <= STACK_SIZE);
    if(C->cap < top-&dummy) {
        free(C->stack);
        C->cap = top-&dummy;
        C->stack = (char *)malloc(C->cap);
    }
    C->size = top-&dummy;
    memcpy(C->stack, &dummy, C->size);
}

void schedule::coroutine_yield() {
    // 取出当前正在运行的协程
    int id = running;
    assert(id >= 0);

    coroutine* C = co[id];
    // 保存当前运行协程的栈内容
    _save_stack(C, stack + STACK_SIZE);

    // 处理状态
    C->status = coroutine_suspend;
    running = -1;

    swapcontext(&C->ctx, &main);
}