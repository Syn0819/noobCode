#ifndef COROUTINE_H
#define COROUTINE_H

#include "ucontext.h"
#include "stddef.h"

enum coroutine_state {
    coroutine_dead,
    coroutine_ready,
    coroutine_Isrunning,
    coroutine_suspend
};

const int STACK_SIZE = 1024*1024;
const int DEFAULT_COROUTINE = 16;

struct coroutine;

class schedule {
public:
    // 开启一个协程调度器
    void coroutine_open();
    // 关闭当前协程调度器
    void coroutine_close();
    // 开启一个新协程，从coroutine_func开始运行
    int coroutine_new(void (*coroutine_func)(schedule*, void *ud), void *ud);
    // 切换到编号为id的协程 继续执行
    void coroutine_resume(int id);
    // 释放CPU，请求切换协程
    void coroutine_yield();
    // 查询编号为id的协程的运行状态
    int coroutine_status(int id);
    // 查询当前协程是否在正常运行
    int coroutine_running();

    ~schedule();
    schedule();
public:
    char stack[STACK_SIZE];
    ucontext_t main;
    int nco;
    int cap;
    int running;
    coroutine **co;
};
#endif