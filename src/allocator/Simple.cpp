#include <afina/allocator/Simple.h>

#include <afina/allocator/Pointer.h>
#include <afina/allocator/Error.h>
#include <cstring>

namespace Afina {
namespace Allocator {

Simple::Simple(void *base, size_t size) : _base(base), _base_len(size) {
    _tail = (Tail*)((char*)_base + size - sizeof(Tail));

    _tail->b_free_chunk = (FreeChunk*)_base;
    _tail->b_desc = nullptr;
    _tail->count_desc = 1;

    FreeChunk *free_chunk = _tail->b_free_chunk;
    free_chunk->desc = (Desc*)((char*)_tail - sizeof(Desc));
    free_chunk->size = size - sizeof(Tail) - sizeof(Desc) - sizeof(Chunk);
    free_chunk->next_chunk = nullptr;
    free_chunk->prev_chunk = nullptr;

    free_chunk->desc->p = nullptr;
}


Pointer Simple::alloc(size_t N) {
    Chunk* result = nullptr;

    FreeChunk* curr_f_chunk = _tail->b_free_chunk->next_chunk;
    while(curr_f_chunk != nullptr) {
        if (curr_f_chunk->size >= N) {
            result = cut_chunk(curr_f_chunk, N);
            break;
        }

        if (join_chunks(curr_f_chunk) != nullptr)
            continue;

        curr_f_chunk = curr_f_chunk->next_chunk;
    }

    if (result == nullptr && _tail->b_free_chunk->size >= N)
        result = cut_chunk(_tail->b_free_chunk, N);

    if (result == nullptr)
        throw AllocError(AllocErrorType::NoMemory, "No memory for allocate!");

    return Pointer((void**)result->desc);
}

void Simple::realloc(Pointer &p, size_t N) {
    if (p.get() == nullptr) {
        p = alloc(N);
        return;
    }

    Chunk *chunk = (Chunk*)((char*)p.get() - sizeof(Chunk));

    if (chunk->size >= N) {
        if (chunk->size - N <= sizeof(FreeChunk))
            return;

        size_t size_f_chunk = chunk->size - sizeof(Chunk) - N;
        chunk->size = N;

        create_f_chunk((FreeChunk*)((char*)chunk + sizeof(Chunk) + N), size_f_chunk);
        return;
    }

    FreeChunk *free_chunk = (FreeChunk*)((char*)chunk + sizeof(Chunk) + chunk->size);
    if (free_chunk->desc->p == nullptr) {
        while (join_chunks(free_chunk) != nullptr);

        size_t new_size_chunk = chunk->size + sizeof(Chunk) + free_chunk->size;
        if (new_size_chunk >= N) {
            Chunk *add_chunk = cut_chunk(free_chunk, N - chunk->size);

            if (add_chunk != nullptr) {
                chunk->size += sizeof(Chunk) + add_chunk->size;
                return;
            }
        }
    }

    //////////////////////////////////////
    Chunk *new_chunk = (Chunk*)((char*)(alloc(N).get()) - sizeof(Chunk));

    memcpy(new_chunk, chunk, chunk->size);
    create_f_chunk(chunk, chunk->size);

    new_chunk->desc->p = (char*)new_chunk + sizeof(Chunk);

    p = Pointer((void**)new_chunk->desc);
}

void Simple::free(Pointer &p) {
    FreeChunk *free_chunk = (FreeChunk*)((char*)p.get() - sizeof(Chunk));

    free_chunk->next_chunk = _tail->b_free_chunk->next_chunk;
    free_chunk->prev_chunk = _tail->b_free_chunk;
    free_chunk->desc->p = nullptr;

    if (free_chunk->next_chunk != nullptr)
        free_chunk->next_chunk->prev_chunk = free_chunk;
    _tail->b_free_chunk->next_chunk = free_chunk;
}

void Simple::defrag() {
    Chunk *curr_chunk = (Chunk*)_base;
    size_t size_move = 0;

    while(curr_chunk != (Chunk*)_tail->b_free_chunk) {
        Chunk *next = (Chunk*)((char*)curr_chunk + sizeof(Chunk) + curr_chunk->size);

        if(curr_chunk->desc->p == nullptr) {
            size_move += sizeof(Chunk) + curr_chunk->size;
            delete_desc(curr_chunk->desc);
            curr_chunk = next;
            continue;
        }

        curr_chunk->desc->p = (char*)curr_chunk->desc->p - size_move;
        memcpy((char*)curr_chunk - size_move, curr_chunk, sizeof(Chunk) + curr_chunk->size);

        curr_chunk = next;
    }

    // TODO: add delete descriptor


    Desc *desc_b_f_chunk = _tail->b_free_chunk->desc;
    _tail->b_free_chunk = (FreeChunk*)((char*)_tail->b_free_chunk - size_move);

    _tail->b_free_chunk->desc = desc_b_f_chunk;
    _tail->b_free_chunk->size += size_move;
    _tail->b_free_chunk->next_chunk = nullptr;
    _tail->b_free_chunk->prev_chunk = nullptr;
}

/**
 * TODO: semantics
 */
std::string Simple::dump() const { return ""; }

/////////////////////////////////////////////
//// chunk
/////////////////////////////////////////////

Simple::Chunk* Simple::cut_chunk(FreeChunk *free_chunk, size_t N) {
    Chunk *result = nullptr;

    if (free_chunk->size - N <= sizeof(FreeChunk)) {
        if (free_chunk->prev_chunk == nullptr)
            return nullptr;

        //// delete free chunk from list
        if (free_chunk->next_chunk == nullptr) {
            free_chunk->prev_chunk->next_chunk = nullptr;
        } else {
            free_chunk->next_chunk->prev_chunk = free_chunk->prev_chunk;
            free_chunk->prev_chunk->next_chunk = free_chunk->next_chunk;
        }

        result = (Chunk*)free_chunk;
        result->desc->p = (char*)result + sizeof(Chunk);
    } else {

        FreeChunk *new_free_chunk = (FreeChunk*)((char*)free_chunk + sizeof(Chunk) + N);

        new_free_chunk->desc = free_chunk->desc;
        new_free_chunk->size = free_chunk->size - N - sizeof(Chunk);
        new_free_chunk->prev_chunk = free_chunk->prev_chunk;
        new_free_chunk->next_chunk = free_chunk->next_chunk;

        //// edit free chunk in list
        if (new_free_chunk->prev_chunk == nullptr)
            _tail->b_free_chunk = new_free_chunk;
        else {
            new_free_chunk->prev_chunk->next_chunk = new_free_chunk;
            new_free_chunk->next_chunk->prev_chunk = new_free_chunk;
        }

        result = create_chunk((void*)free_chunk, N);
    }

    return result;
}

Simple::Chunk* Simple::create_chunk(void *begin, size_t N) {
    Chunk *result = (Chunk*)begin;
    result->desc = create_desc();
    result->size = N;

    result->desc->p = (char*)result + sizeof(Chunk);

    return result;
}

Simple::FreeChunk* Simple::create_f_chunk(void *begin, size_t N) {
    FreeChunk *result = (FreeChunk*)begin;

    result->desc = create_desc();
    result->desc->p = nullptr;
    result->size = N;

    result->next_chunk = _tail->b_free_chunk->next_chunk;
    result->prev_chunk = _tail->b_free_chunk;

    if (_tail->b_free_chunk->next_chunk != nullptr)
        result->next_chunk->prev_chunk = result;

    _tail->b_free_chunk->next_chunk = result;

    return result;
}

Simple::FreeChunk* Simple::join_chunks(FreeChunk *free_chunk) {
    if(free_chunk == _tail->b_free_chunk)
        return nullptr;

    FreeChunk *next_f_chunk = (FreeChunk*)((char*)free_chunk + sizeof(Chunk) + free_chunk->size);
    if (next_f_chunk->desc->p != nullptr)
        return nullptr;

    if (next_f_chunk->next_chunk == nullptr) {
        next_f_chunk->prev_chunk->next_chunk = nullptr;

        free_chunk->size += sizeof(Chunk) + next_f_chunk->size;
        delete_desc(next_f_chunk->desc);
        return free_chunk;
    }

    next_f_chunk->next_chunk->prev_chunk = next_f_chunk->prev_chunk;
    if (next_f_chunk->prev_chunk != nullptr)
        next_f_chunk->prev_chunk->next_chunk = next_f_chunk->next_chunk;

    free_chunk->size += sizeof(Chunk) + next_f_chunk->size;
    delete_desc(next_f_chunk->desc);
    return free_chunk;
}

/////////////////////////////////////////////
//// descriptor
/////////////////////////////////////////////

Simple::Desc* Simple::create_desc() {
    Desc* result;

    if (_tail->b_desc != nullptr) {
        result = _tail->b_desc;
        _tail->b_desc = (Desc*)result->p;

    } else {
        if (_tail->b_free_chunk->size < sizeof(void*))
            throw AllocError(AllocErrorType::InvalidFree, "Need defrag!");

        result = (Desc*)((char*)_tail - (_tail->count_desc + 1) * sizeof(Desc));
        _tail->b_free_chunk->size -= sizeof(Desc);
        _tail->count_desc++;
    }

    return result;
}

void Simple::delete_desc(Desc *desc) {
    desc->p = _tail->b_desc;
    _tail->b_desc = desc;
}


} // namespace Allocator
} // namespace Afina
