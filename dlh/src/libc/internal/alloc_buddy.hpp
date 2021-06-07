/*! \file
 *  \brief An extensible \ref Allocator::Buddy "buddy allocator"
 *
 * The buddy allocator was written by Evan Wallace (published under MIT license)
 * and slightly modified by Bernhard Heinloth.
 * \see https://github.com/evanw/buddy-malloc
 *
 * Copyright 2018 Evan Wallace
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <cstdint>
#include <dlh/unistd.hpp>

namespace Allocator {

/*! \brief Buddy Allocator Template
 *
 * This class implements a buddy memory allocator, which is an allocator that
 * allocates memory within a fixed linear address range. It spans the address
 * range with a binary tree that tracks free space. Both `malloc` and `free`
 * are O(log N) time where N is the maximum possible number of allocations.
 *
 * The "buddy" term comes from how the tree is used. When memory is allocated,
 * nodes in the tree are split recursively until a node of the appropriate size
 * is reached. Every split results in two child nodes, each of which is the
 * buddy of the other. When a node is freed, the node and its buddy can be
 * merged again if the buddy is also free. This makes the memory available
 * for larger allocations again.
 *
 * \tparam MIN_ALLOC_LOG2  binary logarithm value of the minimal allocation size,
 *                         has to be at least 4 (= 16 bytes)
 * \tparam MAX_ALLOC_LOG2  binary logarithm value of the maximum total memory
 *                         which is allocatable
 */
template<size_t MIN_ALLOC_LOG2, size_t MAX_ALLOC_LOG2>
class Buddy {
	/*! \brief Header
	 * Every allocation needs an header to store the allocation size while
	 * staying aligned. The address returned by "malloc" is the address
	 * right after this header (i.e. the size occupies the `size_t` before the
	 * returned address).
	 */
	static const size_t HEADER_SIZE = sizeof(size_t);

	/*! \brief Minimum allocation size
	 * The minimum allocation size is at least twice the header size
	 */
	static const size_t MIN_ALLOC = static_cast<size_t>(1) << MIN_ALLOC_LOG2;

	/*! \brief Maximum total allocation size
	 * This is the total size of the heap.
	 */
	static const size_t MAX_ALLOC = static_cast<size_t>(1) << MAX_ALLOC_LOG2;

	/*! \brief Free List structure
	 * Free lists are stored as circular doubly-linked lists. Every possible
	 * allocation size has an associated free list that is threaded through all
	 * currently free blocks of that size. That means MIN_ALLOC must be at least
	 * "sizeof(List)".
	 */
	struct List {
		/*! \brief Pointer to previous item (or this if first)
		 */
		List * prev;

		/*! \brief Pointer to next item (or this if last)
		 */
		List * next;

		/*! \brief Clear the list
		 * Because these are circular lists, an "empty" list is an entry where
		 * both links point to itself. This makes insertion and removal simpler
		 * because they don't need any branches.
		 */
		void clear() {
			prev = next = this;
		}

		/*! \brief Append the provided entry to the end of the list.
		 * This assumes the entry isn't in a list already because it
		 * overwrites the linked list pointers.
		 * \param entry new list entry
		 */
		void push(List * entry) {
			entry->prev = prev;
			entry->next = this;
			prev->next = entry;
			prev = entry;
		}

		/*! \brief Remove the provided entry from whichever list it's currently in.
		 * This assumes that the entry is in a list. You don't need to provide
		 * the list because the lists are circular, so the list's pointers will
		 * automatically be updated if the first or last entries are removed.
		 * \return pointer to the element
		 */
		List * remove() {
			prev->next = next;
			next->prev = prev;
			return this;
		}

		/*! \brief Remove and return the first entry in the list
		 * \return the first entry or nullptr if the list is empty.
		 */
		List * pop() {
			return prev == this ? nullptr : prev->remove();
		}
	};


	/*! \brief Number of allocation buckets
	 * Allocations are done in powers of two starting from MIN_ALLOC and ending at
	 * MAX_ALLOC inclusive. Each allocation size has a bucket that stores the free
	 * list for that allocation size.
	 *
	 * Given a bucket index, the size of the allocations in that bucket can be
	 * found with "(size_t)1 << (MAX_ALLOC_LOG2 - bucket)".
	 */
	static const size_t BUCKET_COUNT = MAX_ALLOC_LOG2 - MIN_ALLOC_LOG2 + 1;

	/*! \brief Allocation buckets
	 * Each bucket corresponds to a certain allocation size and stores a free list
	 * for that size. The bucket at index 0 corresponds to an allocation size of
	 * MAX_ALLOC (i.e. the whole address space).
	 */
	List buckets[BUCKET_COUNT];


	/*! \brief Allocation size
	 * We could initialize the allocator by giving it one free block the size of
	 * the entire address space. However, this would cause us to instantly reserve
	 * half of the entire address space on the first allocation, since the first
	 * split would store a free list entry at the start of the right child of the
	 * root. Instead, we have the tree start out small and grow the size of the
	 * tree as we use more memory. The size of the tree is tracked by this value.
	 */
	size_t bucket_limit;

	/*! \brief binary tree
	 * This array represents a linearized binary tree of bits. Every possible
	 * allocation larger than MIN_ALLOC has a node in this tree (and therefore a
	 * bit in this array).
	 *
	 * Given the index for a node, linearized binary trees allow you to traverse to
	 * the parent node or the child nodes just by doing simple arithmetic on the
	 * index:
	 *
	 * - Move to parent:         index = (index - 1) / 2;
	 * - Move to left child:     index = index * 2 + 1;
	 * - Move to right child:    index = index * 2 + 2;
	 * - Move to sibling:        index = ((index - 1) ^ 1) + 1;
	 *
	 * Each node in this tree can be in one of several states:
	 *
	 * - UNUSED (both children are UNUSED)
	 * - SPLIT (one child is UNUSED and the other child isn't)
	 * - USED (neither children are UNUSED)
	 *
	 * These states take two bits to store. However, it turns out we have enough
	 * information to distinguish between UNUSED and USED from context, so we only
	 * need to store SPLIT or not, which only takes a single bit.
	 *
	 * Note that we don't need to store any nodes for allocations of size MIN_ALLOC
	 * since we only ever care about parent nodes.
	 */
	uint8_t node_is_split[(1 << (BUCKET_COUNT - 1)) / 8];

	/*! \brief Start of reserved memory
	 * This is the starting address of the address range for this allocator. Every
	 * returned allocation will be an offset of this pointer from 0 to MAX_ALLOC.
	 */
	uintptr_t base_ptr;

	/*! \brief Current end of reserved memory
	 * This is the maximum address that has ever been used by the allocator. It's
	 * used to know when to call RESERVED function to request more memory.
	 */
	uintptr_t max_ptr;

	/*! \brief Prepare request for more reserved memory
	 * \param new_value the requested new end for the reserved memory -- all
	 *                  addresses from base_ptr to new_value have to be valid
	 *                  reserved addresses for the allocator.
	 * \return `true` if the memory could be reserved, `false` otherwise
	 */
	bool update_max_ptr(uintptr_t new_value) {
		if (new_value > max_ptr) {
			if (brk(reinterpret_cast<void*>(new_value))) {
				return false;
			}
			max_ptr = new_value;
		}
		return true;
	}

	/*! \brief Map from the index of a node to the address of memory that node represents.
	 * The bucket can be derived from the index using a loop but is required to be
	 * provided here since having them means we can avoid the loop and have this
	 * function return in constant time.
	 * \param index index of the node
	 * \param bucket bucket index
	 * \return List element (memory address) of the node
	 */
	List *ptr_for_node(size_t index, size_t bucket) const {
		return reinterpret_cast<List *>(base_ptr + ((index - (1 << bucket) + 1) << (MAX_ALLOC_LOG2 - bucket)));
	}

	/*! \brief Map from an address of memory to the node that represents that address.
	 * There are often many nodes that all map to the same address, so
	 * the bucket is needed to uniquely identify a node.
	 * \param ptr memory address
	 * \param bucket bucket index
	 * \return index of the node
	 */
	size_t node_for_ptr(uintptr_t ptr, size_t bucket) const {
		return ((ptr - base_ptr) >> (MAX_ALLOC_LOG2 - bucket)) + (1 << bucket) - 1;
	}

	/*! \brief Get the "is split" flag.
	 * \param index the index of a node
	 * \return the "is split" flag of the parent
	 */
	int parent_is_split(size_t index) const {
		index = (index - 1) / 2;
		return (node_is_split[index / 8] >> (index % 8)) & 1;
	}

	/*! \brief Flip the "is split" flag of the parent.
	 * \param index the index of a node
	 */
	void flip_parent_is_split(size_t index) {
		index = (index - 1) / 2;
		node_is_split[index / 8] ^= 1 << (index % 8);
	}

	/*! \brief Get the smallest bucket for the requested size
	 * \param the requested size passed to malloc
	 * \param index of the smallest bucket that can fit that size
	 */
	size_t bucket_for_request(size_t request) const {
		size_t bucket = BUCKET_COUNT - 1;
		for (size_t size = MIN_ALLOC; size < request; size *= 2) {
			bucket--;
		}
		return bucket;
	}

	/*
	 * The tree is always rooted at the current bucket limit. This call grows the
	 * tree by repeatedly doubling it in size until the root lies at the provided
	 * bucket index. Each doubling lowers the bucket limit by 1.
	 */
	int lower_bucket_limit(size_t bucket) {
		while (bucket < bucket_limit) {
			size_t root = node_for_ptr(base_ptr, bucket_limit);

			// If the parent isn't SPLIT, that means the node at the current bucket
			// limit is UNUSED and our address space is entirely free. In that case,
			// clear the root free list, increase the bucket limit, and add a single
			// block with the newly-expanded address space to the new root free list.
			if (!parent_is_split(root)) {
				reinterpret_cast<List*>(base_ptr)->remove();
				buckets[--bucket_limit].clear();
				buckets[bucket_limit].push(reinterpret_cast<List*>(base_ptr));
				continue;
			}

			// Otherwise, the tree is currently in use. Create a parent node for the
			// current root node in the SPLIT state with a right child on the free
			// list. Make sure to reserve the memory for the free list entry before
			// writing to it. Note that we do not need to flip the "is split" flag for
			// our current parent because it's already on (we know because we just
			// checked it above).
			List * right_child = ptr_for_node(root + 1, bucket_limit);
			if (!update_max_ptr(reinterpret_cast<uintptr_t>(right_child) + sizeof(List))) {
				return 0;
			}
			buckets[bucket_limit].push(right_child);
			buckets[--bucket_limit].clear();

			// Set the grandparent's SPLIT flag so if we need to lower the bucket limit
			// again, we'll know that the new root node we just added is in use.
			root = (root - 1) / 2;
			if (root != 0) {
				flip_parent_is_split(root);
			}
		}

		return 1;
	}

 public:
	/*! \brief Allocate uninitialized memory
	 * \param request the size for the memory
	 * \return address of the allocated memory or 0
	 */
	uintptr_t malloc(size_t request) {
		// Make sure it's possible for an allocation of this size to succeed. There's
		// a hard-coded limit on the maximum allocation size because of the way this
		// allocator works.
		if (request + HEADER_SIZE > MAX_ALLOC) {
			return 0;
		}

		// Initialize our global state if this is the first call to "malloc". At the
		// beginning, the tree has a single node that represents the smallest
		// possible allocation size. More memory will be reserved later as needed.
		if (base_ptr == 0) {
			base_ptr = max_ptr = reinterpret_cast<uintptr_t>(sbrk(0));
			bucket_limit = BUCKET_COUNT - 1;
			if (!update_max_ptr(reinterpret_cast<uintptr_t>(base_ptr) + sizeof(List))) {
				return 0;
			}
			buckets[BUCKET_COUNT - 1].clear();
			buckets[BUCKET_COUNT - 1].push(reinterpret_cast<List *>(base_ptr));
		}

		// Find the smallest bucket that will fit this request. This doesn't check
		// that there's space for the request yet.
		size_t bucket = bucket_for_request(request + HEADER_SIZE);
		size_t original_bucket = bucket;

		// Search for a bucket with a non-empty free list that's as large or larger
		// than what we need. If there isn't an exact match, we'll need to split a
		// larger one to get a match.
		while (bucket + 1 != 0) {
			// We may need to grow the tree to be able to fit an allocation of this
			// size. Try to grow the tree and stop here if we can't.
			if (!lower_bucket_limit(bucket)) {
				return 0;
			}

			// Try to pop a block off the free list for this bucket. If the free list
			// is empty, we're going to have to split a larger block instead.
			uintptr_t ptr = reinterpret_cast<uintptr_t>(buckets[bucket].pop());
			if (ptr == 0) {
				// If we're not at the root of the tree or it's impossible to grow the
				// tree any more, continue on to the next bucket.
				if (bucket != bucket_limit || bucket == 0) {
					bucket--;
					continue;
				}

				// Otherwise, grow the tree one more level and then pop a block off the
				// free list again. Since we know the root of the tree is used (because
				// the free list was empty), this will add a parent above this node in
				// the SPLIT state and then add the new right child node to the free list
				// for this bucket. Popping the free list will give us this right child.
				if (!lower_bucket_limit(bucket - 1)) {
					return 0;
				}
				ptr = reinterpret_cast<uintptr_t>(buckets[bucket].pop());
			}

			// Try to expand the address space first before going any further. If we
			// have run out of space, put this block back on the free list and fail.
			size_t size = static_cast<size_t>(1) << (MAX_ALLOC_LOG2 - bucket);
			size_t bytes_needed = bucket < original_bucket ? size / 2 + sizeof(List) : size;
			if (!update_max_ptr(ptr + bytes_needed)) {
				buckets[bucket].push(reinterpret_cast<List *>(ptr));
				return 0;
			}

			// If we got a node off the free list, change the node from UNUSED to USED.
			// This involves flipping our parent's "is split" bit because that bit is
			// the exclusive-or of the UNUSED flags of both children, and our UNUSED
			// flag (which isn't ever stored explicitly) has just changed.

			// Note that we shouldn't ever need to flip the "is split" bit of our
			// grandparent because we know our buddy is USED so it's impossible for our
			// grandparent to be UNUSED (if our buddy chunk was UNUSED, our parent
			// wouldn't ever have been split in the first place).
			size_t i = node_for_ptr(ptr, bucket);
			if (i != 0) {
				flip_parent_is_split(i);
			}

			// If the node we got is larger than we need, split it down to the correct
			// size and put the new unused child nodes on the free list in the
			// corresponding bucket. This is done by repeatedly moving to the left
			// child, splitting the parent, and then adding the right child to the free
			// list.
			while (bucket < original_bucket) {
				i = i * 2 + 1;
				bucket++;
				flip_parent_is_split(i);
				buckets[bucket].push(ptr_for_node(i + 1, bucket));
			}

			// Now that we have a memory address, write the block header (just the size
			// of the allocation) and return the address immediately after the header.
			*reinterpret_cast<size_t *>(ptr) = request;
			return ptr + HEADER_SIZE;
		}

		return 0;
	}

	/*! \brief Free allocated memory
	 * \param ptr pointer to the start of a memory allocated using malloc
	 */
	void free(uintptr_t ptr) {
		if (ptr != 0 && ptr >= base_ptr && ptr <= max_ptr) {
			// We were given the address returned by "malloc" so get back to the actual
			// address of the node by subtracting off the size of the block header. Then
			// look up the index of the node corresponding to this address.
			ptr -= HEADER_SIZE;
			size_t bucket = bucket_for_request(*reinterpret_cast<size_t *>(ptr) + HEADER_SIZE);
			size_t i = node_for_ptr(ptr, bucket);

			// Traverse up to the root node, flipping USED blocks to UNUSED and merging
			// UNUSED buddies together into a single UNUSED parent.
			while (i != 0) {
				/*
				 * Change this node from UNUSED to USED. This involves flipping our
				 * parent's "is split" bit because that bit is the exclusive-or of the
				 * UNUSED flags of both children, and our UNUSED flag (which isn't ever
				 * stored explicitly) has just changed.
				 */
				flip_parent_is_split(i);

				/*
				 * If the parent is now SPLIT, that means our buddy is USED, so don't merge
				 * with it. Instead, stop the iteration here and add ourselves to the free
				 * list for our bucket.
				 *
				 * Also stop here if we're at the current root node, even if that root node
				 * is now UNUSED. Root nodes don't have a buddy so we can't merge with one.
				 */
				if (parent_is_split(i) || bucket == bucket_limit) {
					break;
				}

				/*
				 * If we get here, we know our buddy is UNUSED. In this case we should
				 * merge with that buddy and continue traversing up to the root node. We
				 * need to remove the buddy from its free list here but we don't need to
				 * add the merged parent to its free list yet. That will be done once after
				 * this loop is finished.
				 */
				ptr_for_node(((i - 1) ^ 1) + 1, bucket)->remove();
				i = (i - 1) / 2;
				bucket--;
			}

			// Add ourselves to the free list for our bucket. We add to the back of the
			// list because "malloc" takes from the back of the list and we want a "free"
			// followed by a "malloc" of the same size to ideally use the same address
			// for better memory locality.
			buckets[bucket].push(ptr_for_node(i, bucket));
		}
	}

	/*! \brief Retrieve the allocated size
	 * \brief ptr pointer to the start of the allocated memory
	 * \return size of the allocated memory
	 */
	size_t size(uintptr_t ptr) {
		return ptr == 0 || ptr < base_ptr || ptr > max_ptr ? 0 : *reinterpret_cast<size_t *>(ptr - HEADER_SIZE);
	}

	// Sanity checks
	static_assert(MIN_ALLOC >= sizeof(List), "Minimum allocation size has to be at least the size of a List item!");
	static_assert(MIN_ALLOC_LOG2 < MAX_ALLOC_LOG2, "Minimum allocation has to be smaller than maximum allocation size!");
};

}  // namespace Allocator
