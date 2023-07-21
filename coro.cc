#include <memory>
#include <stdexcept>
#include <list>
#include <cassert>

struct Coro {
    void * sp;
    void * ip;

    char stack[10240];
    char end[0];
};

struct CoroManager {
    std::list<Coro *> coro_queue;
    void * sp;
    void * ip;
};

CoroManager manager;
Coro *current = nullptr;


void CoroInit() {
    if (!manager.coro_queue.empty()) {

        current = manager.coro_queue.front();
        manager.coro_queue.pop_front();

        // save manager ctx
        asm ("movq $manager_resume_point, %0" : "=m"(manager.ip) : :);
        asm ("movq %%rsp, %0" : "=m"(manager.sp) : :);

        // switch to coro ctx
        asm ("movq %0, %%rsp" : : "m"(current->sp) :);
        asm ("jmpq *%0" : : "m"(current->ip) :);

        asm ("manager_resume_point:");
    }
}

void CoroSchedule() {
    // save old coro ctx
    asm ("movq $coro_resume_point, %0" : "=m"(current->ip) : :);
    asm ("movq %%rsp, %0" : "=m"(current->sp) : :);
    manager.coro_queue.push_back(current);
    current = manager.coro_queue.front();
    manager.coro_queue.pop_front();
    // switch to new coro ctx
    asm ("movq %0, %%rsp" : : "m"(current->sp) :);
    asm ("jmpq *%0" : : "m"(current->ip) :);
    asm ("coro_resume_point:");
}

void CoroOnExit() {
    free(current);
    current = nullptr;
    if (manager.coro_queue.empty()) {
        // switch to manager ctx
        asm ("movq %0, %%rsp" : : "m"(manager.sp) :);
        asm ("jmpq *%0" : : "m"(manager.ip) :);
    } else {
        current = manager.coro_queue.front();
        manager.coro_queue.pop_front();
        // switch to coro ctx
        asm ("movq %0, %%rsp" : : "m"(current->sp) :);
        asm ("jmpq *%0" : : "m"(current->ip) :);
    }
}


void CoroCreate(void (*work)()) {
    auto coro = (Coro *)malloc(sizeof(Coro));
    coro->sp = (void *)(((uint64_t)coro->end >> 3) << 3); // align to 8 bytes
    (uint64_t &)coro->sp -= 8;
    *((uint64_t *)coro->sp) = (uint64_t)&CoroOnExit;
    coro->ip = (void * )work;

    manager.coro_queue.push_back(coro);
}


void f() {
    printf("%d\n", __LINE__);
    CoroSchedule();
    printf("%d\n", __LINE__);
}

void g() {
    printf("%d\n", __LINE__);
    CoroSchedule();
    printf("%d\n", __LINE__);
}


int main() {
    CoroCreate(f);
    CoroCreate(g);
    CoroInit();
}
