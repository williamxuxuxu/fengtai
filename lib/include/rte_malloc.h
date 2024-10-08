/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2019 Intel Corporation
 */

#ifndef _RTE_MALLOC_H_
#define _RTE_MALLOC_H_

/**
 * @file
 * RTE Malloc. This library provides methods for dynamically allocating memory
 * from hugepages.
 */

#include <stdio.h>
#include <stddef.h>
#include <rte_compat.h>
//#include <rte_memory.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  Structure to hold heap statistics obtained from rte_malloc_get_socket_stats function.
 */
struct rte_malloc_socket_stats {
	size_t heap_totalsz_bytes; /**< Total bytes on heap */
	size_t heap_freesz_bytes;  /**< Total free bytes on heap */
	size_t greatest_free_size; /**< Size in bytes of largest free block */
	unsigned free_count;       /**< Number of free elements on heap */
	unsigned alloc_count;      /**< Number of allocated elements on heap */
	size_t heap_allocsz_bytes; /**< Total allocated bytes on heap */
};

/**
 * This function allocates memory from the huge-page area of memory. The memory
 * is not cleared. In NUMA systems, the memory allocated resides on the same
 * NUMA socket as the core that calls this function.
 *
 * @param type
 *   A string identifying the type of allocated objects (useful for debug
 *   purposes, such as identifying the cause of a memory leak). Can be NULL.
 * @param size
 *   Size (in bytes) to be allocated.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the allocated object.
 */
__rte_always_inline void * 
rte_malloc(const char *type, size_t size, unsigned align)
{
	(void)type;
	(void)align;
	
	if(size == 0)
		return NULL;
	
	return malloc(size);
}

/**
 * Replacement function for calloc(), using huge-page memory. Memory area is
 * initialised with zeros. In NUMA systems, the memory allocated resides on the
 * same NUMA socket as the core that calls this function.
 *
 * @param type
 *   A string identifying the type of allocated objects (useful for debug
 *   purposes, such as identifying the cause of a memory leak). Can be NULL.
 * @param num
 *   Number of elements to be allocated.
 * @param size
 *   Size (in bytes) of a single element.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must obviously be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the allocated object.
 */
__rte_always_inline void *
rte_calloc(const char *type, size_t num, size_t size, unsigned align)
{
	(void)type;
	(void)align;
	
	if(num == 0 || size == 0)
		return NULL;
	
	return calloc(num, size);
}

/**
 * Allocate zero'ed memory from the heap.
 *
 * Equivalent to rte_malloc() except that the memory zone is
 * initialised with zeros. In NUMA systems, the memory allocated resides on the
 * same NUMA socket as the core that calls this function.
 *
 * @param type
 *   A string identifying the type of allocated objects (useful for debug
 *   purposes, such as identifying the cause of a memory leak). Can be NULL.
 * @param size
 *   Size (in bytes) to be allocated.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must obviously be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the allocated object.
 */
__rte_always_inline void *
rte_zmalloc(const char *type, size_t size, unsigned align)
{
	return rte_calloc(type, 1, size, align);
}

/**
 * Replacement function for realloc(), using huge-page memory. Reserved area
 * memory is resized, preserving contents. In NUMA systems, the new area
 * may not reside on the same NUMA node as the old one.
 *
 * @param ptr
 *   Pointer to already allocated memory
 * @param size
 *   Size (in bytes) of new area. If this is 0, memory is freed.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must obviously be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the reallocated memory.
 */
__rte_always_inline void *
rte_realloc(void *ptr, size_t size, unsigned int align)
{
	(void)align;
	
	return realloc(ptr, size);
}

/**
 * Replacement function for realloc(), using huge-page memory. Reserved area
 * memory is resized, preserving contents. In NUMA systems, the new area
 * resides on requested NUMA socket.
 *
 * @param ptr
 *   Pointer to already allocated memory
 * @param size
 *   Size (in bytes) of new area. If this is 0, memory is freed.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must obviously be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @param socket
 *   NUMA socket to allocate memory on.
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the reallocated memory.
 */
__rte_always_inline __rte_experimental
void *
rte_realloc_socket(void *ptr, size_t size, unsigned int align, int socket)
{
	(void)socket;
	return rte_realloc(ptr, size, align);
}

/**
 * This function allocates memory from the huge-page area of memory. The memory
 * is not cleared.
 *
 * @param type
 *   A string identifying the type of allocated objects (useful for debug
 *   purposes, such as identifying the cause of a memory leak). Can be NULL.
 * @param size
 *   Size (in bytes) to be allocated.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @param socket
 *   NUMA socket to allocate memory on. If SOCKET_ID_ANY is used, this function
 *   will behave the same as rte_malloc().
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the allocated object.
 */
__rte_always_inline void *
rte_malloc_socket(const char *type, size_t size, unsigned align, int socket)
{
	(void)socket;
	return rte_malloc(type, size, align);
}

/**
 * Allocate zero'ed memory from the heap.
 *
 * Equivalent to rte_malloc() except that the memory zone is
 * initialised with zeros.
 *
 * @param type
 *   A string identifying the type of allocated objects (useful for debug
 *   purposes, such as identifying the cause of a memory leak). Can be NULL.
 * @param size
 *   Size (in bytes) to be allocated.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must obviously be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @param socket
 *   NUMA socket to allocate memory on. If SOCKET_ID_ANY is used, this function
 *   will behave the same as rte_zmalloc().
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the allocated object.
 */
__rte_always_inline void *
rte_zmalloc_socket(const char *type, size_t size, unsigned align, int socket)
{
	(void)socket;
	return rte_zmalloc(type, size, align);
}

/**
 * Replacement function for calloc(), using huge-page memory. Memory area is
 * initialised with zeros.
 *
 * @param type
 *   A string identifying the type of allocated objects (useful for debug
 *   purposes, such as identifying the cause of a memory leak). Can be NULL.
 * @param num
 *   Number of elements to be allocated.
 * @param size
 *   Size (in bytes) of a single element.
 * @param align
 *   If 0, the return is a pointer that is suitably aligned for any kind of
 *   variable (in the same manner as malloc()).
 *   Otherwise, the return is a pointer that is a multiple of *align*. In
 *   this case, it must obviously be a power of two. (Minimum alignment is the
 *   cacheline size, i.e. 64-bytes)
 * @param socket
 *   NUMA socket to allocate memory on. If SOCKET_ID_ANY is used, this function
 *   will behave the same as rte_calloc().
 * @return
 *   - NULL on error. Not enough memory, or invalid arguments (size is 0,
 *     align is not a power of two).
 *   - Otherwise, the pointer to the allocated object.
 */
__rte_always_inline void *
rte_calloc_socket(const char *type, size_t num, size_t size, unsigned align, int socket)
{
	(void)socket;
	return rte_calloc(type, num, size, align);
}

/**
 * Frees the memory space pointed to by the provided pointer.
 *
 * This pointer must have been returned by a previous call to
 * rte_malloc(), rte_zmalloc(), rte_calloc() or rte_realloc(). The behaviour of
 * rte_free() is undefined if the pointer does not match this requirement.
 *
 * If the pointer is NULL, the function does nothing.
 *
 * @param ptr
 *   The pointer to memory to be freed.
 */
__rte_always_inline void
rte_free(void *ptr)
{
	if(ptr != NULL)
		free(ptr);
}


#ifdef __cplusplus
}
#endif

#endif /* _RTE_MALLOC_H_ */
