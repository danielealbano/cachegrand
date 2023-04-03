/**
 * Copyright (c) 2012, Armon Dadgar
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 * The code contains modifications done by Albano Daniele Salvatore, the original source can be found at
 * https://github.com/armon/libart/blob/master/src/art.c
 **/

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifdef __amd64__
#include <emmintrin.h>
#endif

#include "misc.h"
#include "xalloc.h"

#include "art_spsc.h"

/**
 * Macros to manipulate pointer tags
 */
#define IS_LEAF(x) (((uintptr_t)(x) & 1))
#define SET_LEAF(x) ((void*)((uintptr_t)(x) | 1))
#define LEAF_RAW(x) ((art_leaf*)((void*)((uintptr_t)(x) & ~1)))

/**
 * Allocates a node of the given type,
 * initializes to zero and sets the type.
 */
static art_node* art_alloc_node(uint8_t type) {
    art_node* n;
    switch (type) {
        case NODE4:
            n = (art_node*)xalloc_alloc_zero(sizeof(art_node4));
            break;
        case NODE16:
            n = (art_node*)xalloc_alloc_zero(sizeof(art_node16));
            break;
        case NODE48:
            n = (art_node*)xalloc_alloc_zero(sizeof(art_node48));
            break;
        case NODE256:
            n = (art_node*)xalloc_alloc_zero(sizeof(art_node256));
            break;
        default:
            // Can't really happen
            abort();
    }

    n->type = type;
    return n;
}

/**
 * Allocate a leaf
 */
static art_leaf* art_make_leaf(const unsigned char *key, size_t key_len, void *value) {
    art_leaf *l = (art_leaf*)xalloc_alloc_zero(sizeof(art_leaf) + key_len);
    l->value = value;
    l->key_len = key_len;
    memcpy(l->key, key, key_len);

    return l;
}

// Recursively destroys the tree
static void art_destroy_node(art_node *n) {
    // Break if null
    if (!n) {
        return;
    }

    // Special case leafs
    if (IS_LEAF(n)) {
        xalloc_free(LEAF_RAW(n));
        return;
    }

    // Handle each node type
    int i, idx;
    union {
        art_node4 *p1;
        art_node16 *p2;
        art_node48 *p3;
        art_node256 *p4;
    } p;

    switch (n->type) {
        case NODE4:
            p.p1 = (art_node4*)n;

            for (i = 0; i < n->num_children; i++) {
                art_destroy_node(p.p1->children[i]);
            }

            break;

        case NODE16:
            p.p2 = (art_node16*)n;

            for (i = 0; i < n->num_children; i++) {
                art_destroy_node(p.p2->children[i]);
            }

            break;

        case NODE48:
            p.p3 = (art_node48*)n;

            for (i = 0; i < 256; i++) {
                idx = ((art_node48*)n)->keys[i];
                if (!idx) {
                    continue;
                }

                art_destroy_node(p.p3->children[idx - 1]);
            }

            break;

        case NODE256:
            p.p4 = (art_node256*)n;

            for (i = 0; i < 256; i++) {
                if (p.p4->children[i]) {
                    art_destroy_node(p.p4->children[i]);
                }
            }

            break;

        default:
            abort();
    }

    // Free ourselves on the way up
    xalloc_free(n);
}

/**
 * Initializes an ART tree
 * @return 0 on success.
 */
int art_tree_init(art_tree *t) {
    t->root = NULL;
    t->size = 0;
    return 0;
}

/**
 * Destroys an ART tree
 * @return 0 on success.
 */
int art_tree_destroy(art_tree *t) {
    art_destroy_node(t->root);
    return 0;
}

static art_node** art_find_child(art_node *n, unsigned char c) {
    int i, mask, bitfield;

    union {
        art_node4 *p1;
        art_node16 *p2;
        art_node48 *p3;
        art_node256 *p4;
    } p;

    switch (n->type) {
        case NODE4:
            p.p1 = (art_node4*)n;

            for (i = 0; i < n->num_children; i++) {
                /* this cast works around a bug in gcc 5.1 when unrolling loops
                 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59124
                 */
                if (((unsigned char*)p.p1->keys)[i] == c)
                    return &p.p1->children[i];
            }
            break;

            {
                case NODE16:
                    p.p2 = (art_node16*)n;

                    // support non-86 architectures
#ifdef __amd64__
                    // Compare the key to all 16 stored keys
                    __m128i cmp;
                    cmp = _mm_cmpeq_epi8(_mm_set1_epi8((char)c),
                                         _mm_loadu_si128((__m128i*)p.p2->keys));

                    // Use a mask to ignore children that don't exist
                    mask = (1 << n->num_children) - 1;
                    bitfield = _mm_movemask_epi8(cmp) & mask;
#else
#warning No optimized art_find_child implementation for the current architecture
                    // Compare the key to all 16 stored keys
                    bitfield = 0;
                    for (i = 0; i < 16; ++i) {
                        if (p.p2->keys[i] == c)
                            bitfield |= (1 << i);
                    }

                    // Use a mask to ignore children that don't exist
                    mask = (1 << n->num_children) - 1;
                    bitfield &= mask;
#endif

                    /*
                     * If we have a match (any bit set) then we can
                     * return the pointer match using ctz to get
                     * the index.
                     */
                    if (bitfield)
                        return &p.p2->children[__builtin_ctz(bitfield)];
                    break;
            }

        case NODE48:
            p.p3 = (art_node48*)n;
            i = p.p3->keys[c];

            if (i) {
                return &p.p3->children[i-1];
            }

            break;

        case NODE256:
            p.p4 = (art_node256*)n;

            if (p.p4->children[c]) {
                return &p.p4->children[c];
            }

            break;

        default:
            abort();
    }
    return NULL;
}

/**
 * Returns the number of prefix characters shared between
 * the key and node.
 */
static int art_check_prefix(const art_node *n, const unsigned char *key, size_t key_len, size_t depth) {
    int idx;
    int max_cmp_tmp = MIN(n->partial_len, MAX_PREFIX_LEN);
    int max_cmp = MIN(max_cmp_tmp, key_len - depth);

    for (idx=0; idx < max_cmp; idx++) {
        if (n->partial[idx] != key[depth+idx]) {
            return idx;
        }
    }

    return idx;
}

/**
 * Checks if a leaf matches
 * @return 0 on success.
 */
static int art_leaf_matches(const art_leaf *n, const unsigned char *key, size_t key_len) {
    // Fail if the key lengths are different
    if (n->key_len != (uint32_t)key_len) {
        return 1;
    }

    // Compare the keys starting at the depth
    return memcmp(n->key, key, key_len);
}

/**
 * Searches for a value in the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @return NULL if the item was not found, otherwise
 * the value pointer is returned.
 */
void* art_search(const art_tree *t, const unsigned char *key, size_t key_len) {
    art_node **child;
    art_node *n = t->root;
    size_t prefix_len, depth = 0;

    while (n) {
        // Might be a leaf
        if (IS_LEAF(n)) {
            n = (art_node*)LEAF_RAW(n);

            // Check if the expanded path matches
            if (!art_leaf_matches((art_leaf *) n, key, key_len)) {
                return ((art_leaf*)n)->value;
            }

            return NULL;
        }

        // Bail if the prefix does not match
        if (n->partial_len) {
            prefix_len = art_check_prefix(n, key, key_len, depth);

            if (prefix_len != MIN(MAX_PREFIX_LEN, n->partial_len)) {
                return NULL;
            }

            depth = depth + n->partial_len;
        }

        // Recursively search
        child = art_find_child(n,depth < key_len ? key[depth] : 0);
        n = (child) ? *child : NULL;
        depth++;
    }

    return NULL;
}

// Find the minimum leaf under a node
static art_leaf* art_find_minimum(const art_node *n) {
    // Handle base cases
    if (!n) {
        return NULL;
    }

    if (IS_LEAF(n)) {
        return LEAF_RAW(n);
    }

    int idx;
    switch (n->type) {
        case NODE4:
            return art_find_minimum(((const art_node4 *) n)->children[0]);

        case NODE16:
            return art_find_minimum(((const art_node16 *) n)->children[0]);

        case NODE48:
            idx=0;

            while (!((const art_node48*)n)->keys[idx]) {
                idx++;
            }

            idx = ((const art_node48*)n)->keys[idx] - 1;
            return art_find_minimum(((const art_node48 *) n)->children[idx]);

        case NODE256:
            idx=0;

            while (!((const art_node256*)n)->children[idx]) {
                idx++;
            }

            return art_find_minimum(((const art_node256 *) n)->children[idx]);

        default:
            abort();
    }
}

// Find the maximum leaf under a node
static art_leaf* art_find_maximum(const art_node *n) {
    // Handle base cases
    if (!n) {
        return NULL;
    }

    if (IS_LEAF(n)) {
        return LEAF_RAW(n);
    }

    int idx;
    switch (n->type) {
        case NODE4:
            return art_find_maximum(((const art_node4 *) n)->children[n->num_children - 1]);

        case NODE16:
            return art_find_maximum(((const art_node16 *) n)->children[n->num_children - 1]);

        case NODE48:
            idx=255;

            while (!((const art_node48*)n)->keys[idx]) {
                idx--;
            }

            idx = ((const art_node48*)n)->keys[idx] - 1;
            return art_find_maximum(((const art_node48 *) n)->children[idx]);

        case NODE256:
            idx=255;

            while (!((const art_node256*)n)->children[idx]) {
                idx--;
            }

            return art_find_maximum(((const art_node256 *) n)->children[idx]);

        default:
            abort();
    }
}

/**
 * Returns the art_find_minimum valued leaf
 */
art_leaf* art_minimum(art_tree *t) {
    return art_find_minimum((art_node *) t->root);
}

/**
 * Returns the maximum valued leaf
 */
art_leaf* art_maximum(art_tree *t) {
    return art_find_maximum((art_node *) t->root);
}

static int art_longest_common_prefix(art_leaf *l1, art_leaf *l2, size_t depth) {
    int max_cmp = MIN(l1->key_len, l2->key_len) - depth;
    int idx;
    for (idx=0; idx < max_cmp; idx++) {
        if (l1->key[depth+idx] != l2->key[depth+idx]) {
            return idx;
        }
    }
    return idx;
}

static void art_copy_header(art_node *dest, art_node *src) {
    dest->num_children = src->num_children;
    dest->partial_len = src->partial_len;
    memcpy(dest->partial, src->partial, MIN(MAX_PREFIX_LEN, src->partial_len));
}

static void art_add_child256(art_node256 *n, art_node **ref, unsigned char c, void *child) {
    (void)ref;
    n->n.num_children++;
    n->children[c] = (art_node*)child;
}

static void art_add_child48(art_node48 *n, art_node **ref, unsigned char c, void *child) {
    if (likely(n->n.num_children < 48)) {
        int pos = 0;
        while (n->children[pos]) pos++;
        n->children[pos] = (art_node*)child;
        n->keys[c] = pos + 1;
        n->n.num_children++;
    } else {
        art_node256 *new_node = (art_node256*) art_alloc_node(NODE256);
        for (int i=0;i<256;i++) {
            if (n->keys[i]) {
                new_node->children[i] = n->children[n->keys[i] - 1];
            }
        }
        art_copy_header((art_node *) new_node, (art_node *) n);
        *ref = (art_node*)new_node;
        xalloc_free(n);
        art_add_child256(new_node, ref, c, child);
    }
}

static void art_add_child16(art_node16 *n, art_node **ref, unsigned char c, void *child) {
    if (likely(n->n.num_children < 16)) {
        unsigned mask = (1 << n->n.num_children) - 1;

        // support non-x86 architectures
#ifdef __amd64__
        __m128i cmp;

        // Compare the key to all 16 stored keys
        cmp = _mm_cmplt_epi8(_mm_set1_epi8((char)c),
                             _mm_loadu_si128((__m128i*)n->keys));

        // Use a mask to ignore children that don't exist
        unsigned bitfield = _mm_movemask_epi8(cmp) & mask;
#else
#warning No optimized art_add_child16 implementation for the current architecture
        // Compare the key to all 16 stored keys
            unsigned bitfield = 0;
            for (short i = 0; i < 16; ++i) {
                if (c < n->keys[i])
                    bitfield |= (1 << i);
            }

            // Use a mask to ignore children that don't exist
            bitfield &= mask;
#endif

        // Check if less than any
        unsigned idx;
        if (bitfield) {
            idx = __builtin_ctz(bitfield);
            memmove(n->keys+idx+1,n->keys+idx,n->n.num_children-idx);
            memmove(n->children+idx+1,n->children+idx,
                    (n->n.num_children-idx)*sizeof(void*));
        } else {
            idx = n->n.num_children;
        }

        // Set the child
        n->keys[idx] = c;
        n->children[idx] = (art_node*)child;
        n->n.num_children++;
    } else {
        art_node48 *new_node = (art_node48*) art_alloc_node(NODE48);

        // Copy the child pointers and populate the key map
        memcpy(new_node->children, n->children, sizeof(void*)*n->n.num_children);

        for (int i=0;i<n->n.num_children;i++) {
            new_node->keys[n->keys[i]] = i + 1;
        }

        art_copy_header((art_node *) new_node, (art_node *) n);
        *ref = (art_node*)new_node;
        xalloc_free(n);
        art_add_child48(new_node, ref, c, child);
    }
}

static void art_add_child4(art_node4 *n, art_node **ref, unsigned char c, void *child) {
    if (likely(n->n.num_children < 4)) {
        int idx;
        for (idx=0; idx < n->n.num_children; idx++) {
            if (c < n->keys[idx]) break;
        }

        // Shift to make room
        memmove(n->keys+idx+1, n->keys+idx, n->n.num_children - idx);
        memmove(n->children+idx+1, n->children+idx,
                (n->n.num_children - idx)*sizeof(void*));

        // Insert element
        n->keys[idx] = c;
        n->children[idx] = (art_node*)child;
        n->n.num_children++;
    } else {
        art_node16 *new_node = (art_node16*) art_alloc_node(NODE16);

        // Copy the child pointers and the key map
        memcpy(new_node->children, n->children, sizeof(void*) * n->n.num_children);
        memcpy(new_node->keys, n->keys, sizeof(unsigned char) * n->n.num_children);
        art_copy_header((art_node *) new_node, (art_node *) n);
        *ref = (art_node*)new_node;
        xalloc_free(n);
        art_add_child16(new_node, ref, c, child);
    }
}

static void art_add_child(art_node *n, art_node **ref, unsigned char c, void *child) {
    switch (n->type) {
        case NODE4:
            return art_add_child4((art_node4 *) n, ref, c, child);

        case NODE16:
            return art_add_child16((art_node16 *) n, ref, c, child);

        case NODE48:
            return art_add_child48((art_node48 *) n, ref, c, child);

        case NODE256:
            return art_add_child256((art_node256 *) n, ref, c, child);

        default:
            abort();
    }
}

/**
 * Calculates the index at which the prefixes mismatch
 */
static int art_prefix_mismatch(const art_node *n, const unsigned char *key, size_t key_len, size_t depth) {
    int max_cmp_tmp = MIN(MAX_PREFIX_LEN, n->partial_len);
    int max_cmp = MIN(max_cmp_tmp, key_len - depth);
    int idx;

    for (idx=0; idx < max_cmp; idx++) {
        if (n->partial[idx] != key[depth+idx]) {
            return idx;
        }
    }

    // If the prefix is short we can avoid finding a leaf
    if (n->partial_len > MAX_PREFIX_LEN) {
        // Prefix is longer than what we've checked, find a leaf
        art_leaf *l = art_find_minimum(n);
        max_cmp = MIN(l->key_len, key_len)- depth;
        for (; idx < max_cmp; idx++) {
            if (l->key[idx+depth] != key[depth+idx]) {
                return idx;
            }
        }
    }
    return idx;
}

static void* art_recursive_insert(art_node *n, art_node **ref, const unsigned char *key, size_t key_len, void *value, size_t depth, int *old, int replace) {
    // If we are at a NULL node, inject a leaf
    if (!n) {
        *ref = (art_node*)SET_LEAF(art_make_leaf(key, key_len, value));
        return NULL;
    }

    // If we are at a leaf, we need to replace it with a node
    if (IS_LEAF(n)) {
        art_leaf *l = LEAF_RAW(n);

        // Check if we are updating an existing value
        if (!art_leaf_matches(l, key, key_len)) {
            *old = 1;
            void *old_val = l->value;
            if(replace) l->value = value;
            return old_val;
        }

        // New value, we must split the leaf into a node4
        art_node4 *new_node = (art_node4*) art_alloc_node(NODE4);

        // Create a new leaf
        art_leaf *l2 = art_make_leaf(key, key_len, value);

        // Determine longest prefix
        int longest_prefix = art_longest_common_prefix(l, l2, depth);
        new_node->n.partial_len = longest_prefix;
        memcpy(new_node->n.partial, key+depth, MIN(MAX_PREFIX_LEN, longest_prefix));

        // Add the leafs to the new node4
        *ref = (art_node*)new_node;
        art_add_child4(new_node, ref, l->key[depth + longest_prefix], SET_LEAF(l));
        art_add_child4(new_node, ref, l2->key[depth + longest_prefix], SET_LEAF(l2));
        return NULL;
    }

    // Check if given node has a prefix
    if (n->partial_len) {
        // Determine if the prefixes differ, since we need to split
        int prefix_diff = art_prefix_mismatch(n, key, key_len, depth);
        if ((uint32_t)prefix_diff >= n->partial_len) {
            depth += n->partial_len;
            goto RECURSE_SEARCH;
        }

        // Create a new node
        art_node4 *new_node = (art_node4*) art_alloc_node(NODE4);
        *ref = (art_node*)new_node;
        new_node->n.partial_len = prefix_diff;
        memcpy(new_node->n.partial, n->partial, MIN(MAX_PREFIX_LEN, prefix_diff));

        // Adjust the prefix of the old node
        if (n->partial_len <= MAX_PREFIX_LEN) {
            art_add_child4(new_node, ref, n->partial[prefix_diff], n);
            n->partial_len -= (prefix_diff+1);
            memmove(n->partial, n->partial+prefix_diff+1, MIN(MAX_PREFIX_LEN, n->partial_len));
        } else {
            n->partial_len -= (prefix_diff+1);
            art_leaf *l = art_find_minimum(n);
            art_add_child4(new_node, ref, l->key[depth + prefix_diff], n);
            memcpy(n->partial, l->key+depth+prefix_diff+1, MIN(MAX_PREFIX_LEN, n->partial_len));
        }

        // Insert the new leaf
        art_leaf *l = art_make_leaf(key, key_len, value);
        art_add_child4(new_node, ref, key[depth + prefix_diff], SET_LEAF(l));
        return NULL;
    }

RECURSE_SEARCH:;

    // Find a child to recurse to
    art_node **child = art_find_child(n, key[depth]);
    if (child) {
        return art_recursive_insert(*child, child, key, key_len, value, depth + 1, old, replace);
    }

    // No child, node goes within us
    art_leaf *l = art_make_leaf(key, key_len, value);
    art_add_child(n, ref, key[depth], SET_LEAF(l));
    return NULL;
}

/**
 * inserts a new value into the art tree
 * @arg t the tree
 * @arg key the key
 * @arg key_len the length of the key
 * @arg value opaque value.
 * @return null if the item was newly inserted, otherwise
 * the old value pointer is returned.
 */
void* art_insert(art_tree *t, const unsigned char *key, size_t key_len, void *value) {
    int old_val = 0;
    void *old = art_recursive_insert(t->root, &t->root, key, key_len, value, 0, &old_val, 1);

    if (!old_val) {
        t->size++;
    }

    return old;
}

/**
 * inserts a new value into the art tree (no replace)
 * @arg t the tree
 * @arg key the key
 * @arg key_len the length of the key
 * @arg value opaque value.
 * @return null if the item was newly inserted, otherwise
 * the old value pointer is returned.
 */
void* art_insert_no_replace(art_tree *t, const unsigned char *key, size_t key_len, void *value) {
    int old_val = 0;
    void *old = art_recursive_insert(t->root, &t->root, key, key_len, value, 0, &old_val, 0);

    if (!old_val) {
        t->size++;
    }

    return old;
}

static void art_remove_child256(art_node256 *n, art_node **ref, unsigned char c) {
    n->children[c] = NULL;
    n->n.num_children--;

    // Resize to a node48 on underflow, not immediately to prevent
    // trashing if we sit on the 48/49 boundary
    if (unlikely(n->n.num_children == 37)) {
        art_node48 *new_node = (art_node48*) art_alloc_node(NODE48);
        *ref = (art_node*)new_node;
        art_copy_header((art_node *) new_node, (art_node *) n);

        int pos = 0;
        for (int i = 0; i < 256; i++) {
            if (n->children[i]) {
                new_node->children[pos] = n->children[i];
                new_node->keys[i] = pos + 1;
                pos++;
            }
        }

        xalloc_free(n);
    }
}

static void art_remove_child48(art_node48 *n, art_node **ref, unsigned char c) {
    int pos = n->keys[c];
    n->keys[c] = 0;
    n->children[pos - 1] = NULL;
    n->n.num_children--;

    if (unlikely(n->n.num_children == 12)) {
        art_node16 *new_node = (art_node16*) art_alloc_node(NODE16);
        *ref = (art_node*)new_node;
        art_copy_header((art_node *) new_node, (art_node *) n);

        int child = 0;
        for (int i = 0; i < 256;i++) {
            pos = n->keys[i];
            if (pos) {
                new_node->keys[child] = i;
                new_node->children[child] = n->children[pos - 1];
                child++;
            }
        }
        xalloc_free(n);
    }
}

static void art_remove_child16(art_node16 *n, art_node **ref, art_node **l) {
    uintptr_t pos = l - n->children;
    memmove(n->keys + pos, n->keys + pos + 1, n->n.num_children - 1 - pos);
    memmove(n->children + pos, n->children + pos+1, (n->n.num_children - 1 - pos)*sizeof(void*));
    n->n.num_children--;

    if (unlikely(n->n.num_children == 3)) {
        art_node4 *new_node = (art_node4*) art_alloc_node(NODE4);
        *ref = (art_node*)new_node;
        art_copy_header((art_node *) new_node, (art_node *) n);
        memcpy(new_node->keys, n->keys, 4);
        memcpy(new_node->children, n->children, 4 * sizeof(void*));
        xalloc_free(n);
    }
}

static void art_remove_child4(art_node4 *n, art_node **ref, art_node **l) {
    uintptr_t pos = l - n->children;
    memmove(n->keys + pos, n->keys + pos + 1, n->n.num_children - 1 - pos);
    memmove(n->children + pos, n->children + pos + 1, (n->n.num_children - 1 - pos) * sizeof(void*));
    n->n.num_children--;

    // Remove nodes with only a single child
    if (unlikely(n->n.num_children == 1)) {
        art_node *child = n->children[0];
        if (!IS_LEAF(child)) {
            // Concatenate the prefixes
            size_t prefix = n->n.partial_len;
            if (prefix < MAX_PREFIX_LEN) {
                n->n.partial[prefix] = n->keys[0];
                prefix++;
            }
            if (prefix < MAX_PREFIX_LEN) {
                size_t sub_prefix = MIN(child->partial_len, MAX_PREFIX_LEN - prefix);
                memcpy(n->n.partial+prefix, child->partial, sub_prefix);
                prefix += sub_prefix;
            }

            // Store the prefix in the child
            memcpy(child->partial, n->n.partial, MIN(prefix, MAX_PREFIX_LEN));
            child->partial_len += n->n.partial_len + 1;
        }
        *ref = child;
        xalloc_free(n);
    }
}

static void art_remove_child(art_node *n, art_node **ref, unsigned char c, art_node **l) {
    switch (n->type) {
        case NODE4:
            return art_remove_child4((art_node4 *) n, ref, l);

        case NODE16:
            return art_remove_child16((art_node16 *) n, ref, l);

        case NODE48:
            return art_remove_child48((art_node48 *) n, ref, c);

        case NODE256:
            return art_remove_child256((art_node256 *) n, ref, c);

        default:
            abort();
    }
}

static art_leaf* art_recursive_delete(art_node *n, art_node **ref, const unsigned char *key, size_t key_len, size_t depth) {
    // Search terminated
    if (!n) {
        return NULL;
    }

    // Handle hitting a leaf node
    if (IS_LEAF(n)) {
        art_leaf *l = LEAF_RAW(n);
        if (!art_leaf_matches(l, key, key_len)) {
            *ref = NULL;
            return l;
        }

        return NULL;
    }

    // Bail if the prefix does not match
    if (n->partial_len) {
        int prefix_len = art_check_prefix(n, key, key_len, depth);
        if (prefix_len != MIN(MAX_PREFIX_LEN, n->partial_len)) {
            return NULL;
        }
        depth = depth + n->partial_len;
    }

    // Find child node
    art_node **child = art_find_child(n, key[depth]);
    if (!child) {
        return NULL;
    }

    // If the child is leaf, delete from this node
    if (IS_LEAF(*child)) {
        art_leaf *l = LEAF_RAW(*child);
        if (!art_leaf_matches(l, key, key_len)) {
            art_remove_child(n, ref, key[depth], child);
            return l;
        }
        return NULL;

        // Recurse
    } else {
        return art_recursive_delete(*child, child, key, key_len, depth + 1);
    }
}

/**
 * Deletes a value from the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @return NULL if the item was not found, otherwise
 * the value pointer is returned.
 */
void* art_delete(art_tree *t, const unsigned char *key, size_t key_len) {
    art_leaf *l = art_recursive_delete(t->root, &t->root, key, key_len, 0);
    if (l) {
        t->size--;
        void *old = l->value;
        xalloc_free(l);
        return old;
    }
    return NULL;
}

// Recursively iterates over the tree
static int art_recursive_iter(art_node *n, art_callback cb, void *data) {
    // Handle base cases
    if (!n) {
        return 0;
    }

    if (IS_LEAF(n)) {
        art_leaf *l = LEAF_RAW(n);
        return cb(data, (const unsigned char*)l->key, l->key_len, l->value);
    }

    int idx, res;
    switch (n->type) {
        case NODE4:
            for (int i = 0; i < n->num_children; i++) {
                res = art_recursive_iter(((art_node4 *) n)->children[i], cb, data);
                if (res) return res;
            }

            break;

        case NODE16:
            for (int i = 0; i < n->num_children; i++) {
                res = art_recursive_iter(((art_node16 *) n)->children[i], cb, data);
                if (res) return res;
            }

            break;

        case NODE48:
            for (int i = 0; i < 256; i++) {
                idx = ((art_node48*)n)->keys[i];
                if (!idx) {
                    continue;
                }

                res = art_recursive_iter(((art_node48 *) n)->children[idx - 1], cb, data);
                if (res) {
                    return res;
                }
            }

            break;

        case NODE256:
            for (int i=0; i < 256; i++) {
                if (!((art_node256*)n)->children[i]) {
                    continue;
                }

                res = art_recursive_iter(((art_node256 *) n)->children[i], cb, data);

                if (res) {
                    return res;
                }
            }

            break;

        default:
            abort();
    }
    return 0;
}

/**
 * Iterates through the entries pairs in the map,
 * invoking a callback for each. The call back gets a
 * key, value for each and returns an integer stop value.
 * If the callback returns non-zero, then the iteration stops.
 * @arg t The tree to iterate over
 * @arg cb The callback function to invoke
 * @arg data Opaque handle passed to the callback
 * @return 0 on success, or the return of the callback.
 */
int art_iter(art_tree *t, art_callback cb, void *data) {
    return art_recursive_iter(t->root, cb, data);
}

/**
 * Checks if a leaf prefix matches
 * @return 0 on success.
 */
static int art_leaf_prefix_matches(const art_leaf *n, const unsigned char *prefix, size_t prefix_len) {
    // Fail if the key length is too short
    if (n->key_len < (uint32_t)prefix_len) {
        return 1;
    }

    // Compare the keys
    return memcmp(n->key, prefix, prefix_len);
}

/**
 * Iterates through the entries pairs in the map,
 * invoking a callback for each that matches a given prefix.
 * The call back gets a key, value for each and returns an integer stop value.
 * If the callback returns non-zero, then the iteration stops.
 * @arg t The tree to iterate over
 * @arg prefix The prefix of keys to read
 * @arg prefix_len The length of the prefix
 * @arg cb The callback function to invoke
 * @arg data Opaque handle passed to the callback
 * @return 0 on success, or the return of the callback.
 */
int art_iter_prefix(art_tree *t, const unsigned char *key, size_t key_len, art_callback cb, void *data) {
    art_node **child;
    art_node *n = t->root;
    size_t prefix_len, depth = 0;
    while (n) {
        // Might be a leaf
        if (IS_LEAF(n)) {
            n = (art_node*)LEAF_RAW(n);
            // Check if the expanded path matches
            if (!art_leaf_prefix_matches((art_leaf *) n, key, key_len)) {
                art_leaf *l = (art_leaf*)n;
                return cb(data, (const unsigned char*)l->key, l->key_len, l->value);
            }

            return 0;
        }

        // If the depth matches the prefix, we need to handle this node
        if (depth == key_len) {
            art_leaf *l = art_find_minimum(n);
            if (!art_leaf_prefix_matches(l, key, key_len)) {
                return art_recursive_iter(n, cb, data);
            }

            return 0;
        }

        // Bail if the prefix does not match
        if (n->partial_len) {
            prefix_len = art_prefix_mismatch(n, key, key_len, depth);

            // Guard if the mismatch is longer than the MAX_PREFIX_LEN
            if ((uint32_t)prefix_len > n->partial_len) {
                prefix_len = n->partial_len;
            }

            // If there is no match, search is terminated
            if (!prefix_len) {
                return 0;

                // If we've matched the prefix, iterate on this node
            } else if (depth + prefix_len == key_len) {
                return art_recursive_iter(n, cb, data);
            }

            // if there is a full match, go deeper
            depth = depth + n->partial_len;
        }

        // Recursively search
        child = art_find_child(n, key[depth]);
        n = (child) ? *child : NULL;
        depth++;
    }

    return 0;
}
