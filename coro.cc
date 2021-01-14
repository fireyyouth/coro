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

bool on_manager_stack = true;


void CoroManagerSchedule() {
    while (!manager.coro_queue.empty()) {
        assert(on_manager_stack);

        current = manager.coro_queue.front();
        manager.coro_queue.pop_front();

        // save manager ctx
        asm ("movq $manager_resume_point, %0" : "=m"(manager.ip) : :);
        asm ("movq %%rsp, %0" : "=m"(manager.sp) : :);

        // switch to coro ctx
        on_manager_stack = false;
        asm ("movq %0, %%rsp" : : "m"(current->sp) :);
        asm ("jmpq *%0" : : "m"(current->ip) :);

        asm ("manager_resume_point:");
        on_manager_stack = true;
    }
}

void CoroYield() {
    assert(!on_manager_stack);

    // save coro ctx
    asm ("movq $coro_resume_point, %0" : "=m"(current->ip) : :);
    asm ("movq %%rsp, %0" : "=m"(current->sp) : :);

    manager.coro_queue.push_back(current);
    current = nullptr;

    // switch to manager ctx
    asm ("movq %0, %%rsp" : : "m"(manager.sp) :);
    asm ("jmpq *%0" : : "m"(manager.ip) :);

    asm ("coro_resume_point:");
}

void FreeWrapper(void *p) {
    printf("%s: %p\n", __func__, p);
    free(p);
}

void *MallocWrapper(size_t size) {
    auto r = malloc(size);
    printf("%s: %p\n", __func__, r);
    return r;
}

void CoroOnExit() {
    assert(!on_manager_stack);

    FreeWrapper(current);
    current = nullptr;

    // switch to manager ctx
    asm ("movq %0, %%rsp" : : "m"(manager.sp) :);
    asm ("jmpq *%0" : : "m"(manager.ip) :);
}


void CoroCreate(void (*work)()) {
    auto coro = (Coro *)MallocWrapper(sizeof(Coro));
    coro->sp = (void *)(((uint64_t)coro->end >> 3) << 3); // align to 8 bytes
    (uint64_t &)coro->sp -= 8;
    *((uint64_t *)coro->sp) = (uint64_t)&CoroOnExit;
    coro->ip = (void * )work;

    manager.coro_queue.push_back(coro);
}



void f() {
    printf("%d\n", __LINE__);
    CoroYield();
    printf("%d\n", __LINE__);
}

void g() {
    printf("%d\n", __LINE__);
    CoroYield();
    printf("%d\n", __LINE__);
}


int main() {
    CoroCreate(f);
    CoroCreate(g);
    CoroManagerSchedule();
}
