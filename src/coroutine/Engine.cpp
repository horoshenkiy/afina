#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

void Engine::Store(context &ctx) {
    char end_stack;

    if (std::get<0>(ctx.Stack) != nullptr)
        delete std::get<0>(ctx.Stack);

    ctx.Low = &end_stack;
    ctx.Hight = StackBottom;

    ptrdiff_t len = StackBottom - &end_stack;
    ctx.Stack = std::make_tuple(new char[len], len);
    memcpy(std::get<0>(ctx.Stack), &end_stack, len);
}

void Engine::Restore(context &ctx) {
    char end_stack;

    if (ctx.Low < &end_stack)
        Restore(ctx);

    memcpy(StackBottom - std::get<1>(ctx.Stack), std::get<0>(ctx.Stack), std::get<1>(ctx.Stack));
    longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    if (alive == nullptr)
        return;

    context *ctx_for_schedule = alive;
    while (ctx_for_schedule == cur_routine && ctx_for_schedule != nullptr)
        ctx_for_schedule = ctx_for_schedule->next;

    if (ctx_for_schedule == nullptr)
        return;

    if (cur_routine != nullptr) {
        Store(*cur_routine);

        if (setjmp(cur_routine->Environment) != 0)
            return;
    }

    cur_routine = ctx_for_schedule;
    Restore(*cur_routine);
}

void Engine::sched(void *routine_) {
    context *ctx = (context*)routine_, *ctx_for_schedule;

    // find routine in list for execute
    ctx_for_schedule = alive;
    while (ctx_for_schedule != ctx && ctx_for_schedule != nullptr) {
        ctx_for_schedule = ctx_for_schedule->next;
    }

    // if not found routine return
    if (ctx_for_schedule == nullptr)
        return;

    if (cur_routine != nullptr) {
        Store(*cur_routine);

        if (setjmp(cur_routine->Environment) != 0)
            return;
    }

    cur_routine = ctx;
    Restore(*cur_routine);
}

} // namespace Coroutine
} // namespace Afina
