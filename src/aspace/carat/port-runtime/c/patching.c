#include "patching.h"


int carat_patch_escapes(allocEntry *entry, void* allocationTarget) {

    nk_slist_node_uintptr_t *iter;
    uintptr_t val;
    nk_slist_foreach((entry->allocToEscapeMap), val, iter) {
        void** escape = (void**) val;
        int64_t offset = doesItAlias(entry->pointer, entry->length, (uint64_t) *escape);
        if(offset >= 0) {
            *escape = (void*) ((int64_t) allocationTarget + offset);
        }
    }

    return 0;
}

int carat_update_entry(allocEntry *entry, void* allocationTarget) {

    // Create a new entry
    allocEntry *newEntry = allocEntry(allocationTarget, entry->length);
    newEntry->allocToEscapeMap = entry->allocToEscapeMap;
    nk_pair_uintptr_t_uint64_t *pair = NK_PAIR_BUILD(uintptr_t, uintptr_t, ((uintptr_t) allocationTarget), ((uintptr_t) newEntry));
	nk_slist_add(uintptr_t, allocationMap, ((uintptr_t) pair)); // FIX: add_or_panic

    nk_slist_remove(uintptr_t, allocationMap, (entry->pointer));
    return 0;
}

static void handle_thread(struct nk_thread *t, void *state) {
    struct move_alloc_state *move_state = (struct move_alloc_state*) state;
    
    struct nk_regs * r = (struct nk_regs*)((char*)t->rsp - 128); // FIX: t->rsp - 128 might be wrong, look at garbage collector

// moved these comments out of our HANDLE
// if the register is within our alloc
// DEBUG("It aliased %p will now become %p which is offset %ld\n", registerPtr.ptr, newAllocPtr.ptr, offset);
// DEBUG("The register %s is now %p\n", regNames[i], (void*) uc->uc_mcontext.gregs[i]);
#define HANDLE(reg) \
    if((r->reg >= (uint64_t) state->allocationToMove) && (r->reg < ((uint64_t) state->allocationToMove + state->length))){ \
            uint64_t offset = r->reg - (uint64_t) state->allocationToMove; \
            uint64_t newAddr = (uint64_t) state->allocationTarget + offset; \
            r->reg = newAddr; \
    } \

    HANDLE(r15)
    HANDLE(r14)
    HANDLE(r13)
    HANDLE(r12)
    HANDLE(r11)
    HANDLE(r10)
    HANDLE(r9)
    HANDLE(r8)
    HANDLE(rbp)
    HANDLE(rdi)
    HANDLE(rsi)
    HANDLE(rdx)
    HANDLE(rcx)
    HANDLE(rbx)
    HANDLE(rax)
    // handle rsp and rip later

}

int nk_carat_move_allocation(void* allocationToMove, void* allocationTarget) {
    if(nk_sched_stop_world()) {
        //print error, "Oaklahoma has reopened!"
        return -1;
    }

    // genreate patches
    // apply patches

    allocEntry* entry = findAllocEntry(allocationToMove);
    if(!entry) {
        ERROR("Cannot find entry\n");
        goto out_bad;
    }

    // Generate what patches need to be executed for overwriting memory, then executes patches
    if (carat_patch_escapes(entry, allocationTarget) == 1){
        ERROR("Unable to patch\n");
        goto out_bad;
    }

    // For each thread, patch registers

    struct move_alloc_state state = {allocationToMove, allocationTarget, entry->length, 0};
    nk_sched_map_threads(-1, handle_thread, &state);
    if(state.failed) {
        ERROR("Unable to patch threads\n");
        goto out_bad;
    }
    
    memmove(allocationToMove, allocationTarget, entry->length);
    
    carat_update_entry(entry, allocationTarget);


    // Do we need to handle our own stack?

out_good:
    nk_sched_start_world();
    return 0;

out_bad:
    nk_sched_start_world();
    return -1;

}