/*
 * compressed_trie.c
 *
 * Path-compressed binary trie (Patricia-style) for IPv4 prefix routing.
 *
 *   - insert()  adds an IP prefix (network / prefix_len -> nexthop)
 *   - search()  does a longest-prefix-match lookup for a 32-bit address
 *
 * Each node stores the *full* prefix from the root (prefix, prefix_len).
 * Interior "glue" nodes created while splitting carry nexthop == 0, which
 * is treated as "not a real route" (so nexthop 0 is reserved).
 *
 * Build:  gcc -Wall -Wextra -O2 -o compressed_trie compressed_trie.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Data structure                                                     */
/* ------------------------------------------------------------------ */

struct node {
    uint32_t     prefix;      /* prefix bits (host byte order), low bits 0 */
    int          prefix_len;  /* number of significant bits [0..32]        */
    uint32_t     nexthop;     /* route nexthop; 0 => interior/glue node    */
    struct node *node[2];     /* children, indexed by the next bit         */
};

static struct node *root; /* the root node (prefix_len 0, i.e. 0.0.0.0/0) */

/* ------------------------------------------------------------------ */
/* Bit helpers                                                        */
/* ------------------------------------------------------------------ */

/* prefix_mask(len): 32-bit mask with the top `len` bits set.
 *   prefix_mask(0)  == 0x00000000
 *   prefix_mask(24) == 0xFFFFFF00
 *   prefix_mask(32) == 0xFFFFFFFF
 */
static uint32_t prefix_mask(int len)
{
    return len == 0 ? 0u : (0xFFFFFFFFu << (32 - len));
}

/* bit_at(key, pos): the bit of `key` at position `pos`, MSB first.
 * pos 0 is the most-significant bit, pos 31 the least-significant.
 */
static uint32_t bit_at(uint32_t key, int pos)
{
    return (key >> (31 - pos)) & 1u;
}

/* common_prefix_len(a, b, maxlen): number of leading bits shared by
 * `a` and `b`, MSB first, capped at `maxlen`.
 */
static int common_prefix_len(uint32_t a, uint32_t b, int maxlen)
{
    uint32_t diff = a ^ b;
    int i = 0;

    while (i < maxlen && ((diff >> (31 - i)) & 1u) == 0)
        i++;

    return i;
}

/* ------------------------------------------------------------------ */
/* Node allocation                                                    */
/* ------------------------------------------------------------------ */

static struct node *new_node(uint32_t prefix, int prefix_len, uint32_t nexthop)
{
    struct node *n = calloc(1, sizeof(*n));
    if (!n) {
        perror("calloc");
        exit(EXIT_FAILURE);
    }
    n->prefix     = prefix & prefix_mask(prefix_len);
    n->prefix_len = prefix_len;
    n->nexthop    = nexthop;
    n->node[0]    = NULL;
    n->node[1]    = NULL;
    return n;
}

/* ------------------------------------------------------------------ */
/* Insert                                                             */
/* ------------------------------------------------------------------ */

void insert(uint32_t key, int prefix_len, uint32_t nexthop)
{
    key &= prefix_mask(prefix_len);

    if (root == NULL)
        root = new_node(0, 0, 0);   /* default root, not a real route */

    /* Default route (0.0.0.0/0) lives on the root itself. */
    if (prefix_len == 0) {
        root->nexthop = nexthop;
        return;
    }

    struct node *parent  = root;    /* matched up to parent->prefix_len bits */

    for (;;) {
        uint32_t     b     = bit_at(key, parent->prefix_len);
        struct node *child = parent->node[b];

        if (child == NULL) {
            /* No branch here yet: hang a fresh leaf. */
            parent->node[b] = new_node(key, prefix_len, nexthop);
            return;
        }

        int lim = prefix_len < child->prefix_len ? prefix_len : child->prefix_len;
        int cpl = common_prefix_len(key, child->prefix, lim);

        if (cpl == child->prefix_len) {
            /* child->prefix is a prefix of key: descend into it. */
            if (child->prefix_len == prefix_len) {
                child->nexthop = nexthop;  /* exact match: update route */
                return;
            }
            parent = child;
            continue;
        }

        /* Need to split at bit position cpl. */
        struct node *inter =
            new_node(key & prefix_mask(cpl), cpl, 0);

        inter->node[bit_at(child->prefix, cpl)] = child;
        parent->node[b] = inter;

        if (cpl == prefix_len) {
            /* new prefix is exactly the split point -> real route on inter */
            inter->nexthop = nexthop;
        } else {
            inter->node[bit_at(key, cpl)] =
                new_node(key, prefix_len, nexthop);
        }
        return;
    }
}

/* ------------------------------------------------------------------ */
/* Search (longest prefix match)                                      */
/* ------------------------------------------------------------------ */

/* Returns 1 and sets *nexthop_out on a hit, 0 if no route matches. */
int search(uint32_t addr, uint32_t *nexthop_out)
{
    struct node *n     = root;
    int          found = 0;
    uint32_t     best  = 0;

    while (n != NULL) {
        /* Does addr agree with this node's full prefix? */
        if (common_prefix_len(addr, n->prefix, n->prefix_len) < n->prefix_len)
            break;                     /* diverges: no deeper match possible */

        if (n->nexthop != 0) {         /* real route: remember as best so far */
            best  = n->nexthop;
            found = 1;
        }

        if (n->prefix_len == 32)
            break;                     /* full-length match, cannot go deeper */

        n = n->node[bit_at(addr, n->prefix_len)];
    }

    if (found && nexthop_out)
        *nexthop_out = best;
    return found;
}

/* ------------------------------------------------------------------ */
/* Delete                                                             */
/* ------------------------------------------------------------------ */

/* Remove the route for exactly (key/prefix_len).
 * Returns 1 if a matching route was removed, 0 if none existed.
 *
 * After clearing the route we walk back up the path collapsing any node
 * that is no longer needed: a glue node (nexthop 0) with a single child is
 * merged into that child, and an empty leaf is unlinked from its parent.
 * The collapse cascades upward so the trie stays path-compressed.
 */
int delete(uint32_t key, int prefix_len)
{
    key &= prefix_mask(prefix_len);

    if (root == NULL)
        return 0;

    /* Default route lives on the root, which is never freed. */
    if (prefix_len == 0) {
        if (root->nexthop == 0)
            return 0;
        root->nexthop = 0;
        return 1;
    }

    struct node *path[34];   /* path[0..depth] = root .. target        */
    int          slot[34];   /* slot[i] = child index from path[i]     */
    int          depth = 0;

    path[0] = root;

    for (;;) {
        struct node *cur   = path[depth];
        int          b     = bit_at(key, cur->prefix_len);
        struct node *child = cur->node[b];

        if (child == NULL)
            return 0;                  /* not found */

        int lim = prefix_len < child->prefix_len ? prefix_len : child->prefix_len;
        if (common_prefix_len(key, child->prefix, lim) < child->prefix_len)
            return 0;                  /* diverges: not found */

        slot[depth]  = b;
        path[++depth] = child;

        if (child->prefix_len == prefix_len)
            break;                     /* found the exact node */

        /* child->prefix_len < prefix_len here, so keep descending */
    }

    struct node *target = path[depth];
    if (target->nexthop == 0)
        return 0;                      /* node exists but carries no route */

    target->nexthop = 0;               /* remove the route */

    /* Collapse redundant nodes from the target back up toward the root. */
    for (int i = depth; i >= 1; i--) {
        struct node *n      = path[i];
        struct node *parent = path[i - 1];
        int          pslot  = slot[i - 1];

        if (n->nexthop != 0)
            break;                     /* real route: node must stay */

        int cnt = (n->node[0] != NULL) + (n->node[1] != NULL);

        if (cnt == 2)
            break;                     /* branch point: node must stay */

        if (cnt == 1) {
            /* merge glue node into its single child */
            parent->node[pslot] = n->node[0] ? n->node[0] : n->node[1];
            free(n);
            break;                     /* parent's child count is unchanged */
        }

        /* cnt == 0: unlink empty leaf; parent may now be collapsible too */
        parent->node[pslot] = NULL;
        free(n);
    }

    return 1;
}

/* ------------------------------------------------------------------ */
/* Demo                                                               */
/* ------------------------------------------------------------------ */

static uint32_t ipv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) |
           ((uint32_t)c << 8)  |  (uint32_t)d;
}

static void print_ip(uint32_t ip)
{
    printf("%u.%u.%u.%u",
           (ip >> 24) & 0xff, (ip >> 16) & 0xff,
           (ip >> 8) & 0xff,  ip & 0xff);
}

static void lookup(uint32_t addr)
{
    uint32_t nh;
    print_ip(addr);
    if (search(addr, &nh)) {
        printf(" -> nexthop ");
        print_ip(nh);
        printf("\n");
    } else {
        printf(" -> no route\n");
    }
}

int main(void)
{
    /* prefix            len  nexthop */
    insert(ipv4(10, 0, 0, 0),     8,  ipv4(192, 168, 1, 1));
    insert(ipv4(10, 1, 0, 0),    16,  ipv4(192, 168, 1, 2));
    insert(ipv4(10, 1, 2, 0),    24,  ipv4(192, 168, 1, 3));
    insert(ipv4(172, 16, 0, 0),  12,  ipv4(192, 168, 1, 4));
    insert(ipv4(0, 0, 0, 0),      0,  ipv4(192, 168, 1, 254)); /* default */

    lookup(ipv4(10, 1, 2, 55));   /* -> .3 (most specific /24) */
    lookup(ipv4(10, 1, 9, 9));    /* -> .2 (/16)               */
    lookup(ipv4(10, 9, 9, 9));    /* -> .1 (/8)                */
    lookup(ipv4(172, 16, 5, 5));  /* -> .4 (/12)               */
    lookup(ipv4(8, 8, 8, 8));     /* -> .254 (default route)   */

    printf("\n-- delete 10.1.2.0/24 --\n");
    delete(ipv4(10, 1, 2, 0), 24);
    lookup(ipv4(10, 1, 2, 55));   /* now falls back to .2 (/16) */

    printf("\n-- delete 10.1.0.0/16 --\n");
    delete(ipv4(10, 1, 0, 0), 16);
    lookup(ipv4(10, 1, 2, 55));   /* now falls back to .1 (/8)  */
    lookup(ipv4(10, 1, 9, 9));    /* now falls back to .1 (/8)  */

    printf("\n-- delete default route --\n");
    delete(ipv4(0, 0, 0, 0), 0);
    lookup(ipv4(8, 8, 8, 8));     /* now no route               */

    printf("\n-- delete non-existent 5.5.5.0/24 --\n");
    printf("removed = %d\n", delete(ipv4(5, 5, 5, 0), 24)); /* -> 0 */

    return 0;
}
