#include "coroutine.h"
#include <iostream>

struct args
{
    int n;
};

static void foo(schedule* s, void *ud){
    args* arg = (args*)ud;
    int start = arg->n;
    int i;
    for(int i = 0; i < 5; i++) {
        std::cout << "coroutine: " << s->coroutine_running() << ": " << start + i << std::endl;
        s->coroutine_yield();
    }
}

int main() {
    schedule* S = new schedule();

    struct args arg1 = {0};
    struct args arg2 = {100};

    int co1 = S->coroutine_new(foo, &arg1);
    int co2 = S->coroutine_new(foo, &arg2);

    std::cout << "main start" << std::endl;

    while(S->coroutine_status(co1) && S->coroutine_status(co2)) {
        S->coroutine_resume(co1);
        S->coroutine_resume(co2);
    }
    std::cout << "main end" << std::endl;

    S->coroutine_close();
    return 0;
}