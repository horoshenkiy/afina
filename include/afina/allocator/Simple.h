#ifndef AFINA_ALLOCATOR_SIMPLE_H
#define AFINA_ALLOCATOR_SIMPLE_H

#include <string>
#include <cstddef>

namespace Afina {
namespace Allocator {

// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
class Pointer;

/**
 * Wraps given memory area and provides defagmentation allocator interface on
 * the top of it.
 *
 * Allocator instance doesn't take ownership of wrapped memmory and do not delete it
 * on destruction. So caller must take care of resource cleaup after allocator stop
 * being needs
 */
// TODO: Implements interface to allow usage as C++ allocators
class Simple {
public:
    Simple(void *base, const size_t size);

    /**
     * TODO: semantics
     * @param N size_t
     */
    Pointer alloc(size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     * @param N size_t
     */
    void realloc(Pointer &p, size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     */
    void free(Pointer &p);

    /**
     * TODO: semantics
     */
    void defrag();

    /**
     * TODO: semantics
     */
    std::string dump() const;

private:

    struct Desc {
        void* p;
    };

    struct Chunk {
        Desc *desc;
        size_t size;
    };

    struct FreeChunk {
        Desc *desc;
        size_t size;
        FreeChunk* next_chunk;
        FreeChunk* prev_chunk;
    };

    struct Tail {
        FreeChunk* b_free_chunk;
        Desc *b_desc;
        size_t count_desc;
    };

    void *_base;
    const size_t _base_len;

    Tail* _tail;

    //// chunk
    /////////////////////////////////////////////

    Chunk *cut_chunk(FreeChunk* free_chunk, size_t N);

    Chunk *create_chunk(void* begin, size_t N);

    FreeChunk *create_f_chunk(void* begin, size_t N);

    FreeChunk *join_chunks(FreeChunk *free_chunk);

    //// descriptor
    /////////////////////////////////////////////

    Desc* create_desc();

    void delete_desc(Desc *desc);

};

} // namespace Allocator
} // namespace Afina
#endif // AFINA_ALLOCATOR_SIMPLE_H
