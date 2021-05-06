#include "wt.h"

int ebpf_lex_compare(uint8_t *key_1, uint64_t key_len_1,
                     uint8_t *key_2, uint64_t key_len_2) {
    /* extracted from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/btree_cmp.i#L90 
     * ( might consider replace with vector operation :) although not sure whether ebpf supports it )
     */
    uint64_t len = (key_len_1 > key_len_2) ? key_len_2 : key_len_1, max_len = EBPF_KEY_MAX_LEN;
    for (; len > 0 && max_len > 0; --len, --max_len, ++key_1, ++key_2)
        if (*key_1 != *key_2)
            return (*key_1 < *key_2 ? -1 : 1);
    return ((key_len_1 == key_len_2) ? 0 : (key_len_1 < key_len_2) ? -1 : 1);
}

int ebpf_unpack_posint(uint8_t **pp, uint64_t *retp) {
    uint64_t x;
    uint8_t len, max_len = 16;  /* max_len is set to pass the ebpf verifier */
    uint8_t *p;

    /* There are four length bits in the first byte. */
    p = *pp;
    len = (*p++ & 0xf);

    for (x = 0; len != 0 && max_len != 0; --len, --max_len)
        x = (x << 8) | *p++;

    *retp = x;
    *pp = p;
    return 0;
}

int ebpf_vunpack_uint(uint8_t **pp, uint64_t *xp) {
    uint8_t *p;
    int ret;

    /* encoding scheme: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/intpack.i#L10 */
    p = *pp;
    switch (*p & 0xf0) {
    case EBPF_POS_1BYTE_MARKER:
    case EBPF_POS_1BYTE_MARKER | 0x10:
    case EBPF_POS_1BYTE_MARKER | 0x20:
    case EBPF_POS_1BYTE_MARKER | 0x30:
        /* higher 2 bits of the first byte is 10 */
        *xp = GET_BITS(*p, 6, 0);  /* extract integer from the remaining (8 - 2) = 6 bites */
        p += 1;
        break;
    case EBPF_POS_2BYTE_MARKER:
    case EBPF_POS_2BYTE_MARKER | 0x10:
        /* higher 3 bits of the first byte is 110 */
        *xp = GET_BITS(*p++, 5, 0) << 8;
        *xp |= *p++;
        *xp += EBPF_POS_1BYTE_MAX + 1;
        break;
    case EBPF_POS_MULTI_MARKER:
        /* higher 4 bits of the first byte is 1110 */
        ret = ebpf_unpack_posint(pp, xp);
        if (ret != 0) {
            return ret;
        }
        *xp += EBPF_POS_2BYTE_MAX + 1;
        return 0;
    default:
        return -EBPF_EINVAL;
    }

    *pp = p;
    return 0;
}

int ebpf_addr_to_offset(uint8_t *addr, uint64_t *offset, uint64_t *size) {
    int ret;
    uint64_t raw_offset, raw_size, raw_checksum;

    ret = ebpf_vunpack_uint(&addr, &raw_offset);
    if (ret < 0)
        return ret;
    ret = ebpf_vunpack_uint(&addr, &raw_size);
    if (ret < 0)
        return ret;
    ret = ebpf_vunpack_uint(&addr, &raw_checksum);  /* checksum is not used */
    if (ret < 0)
        return ret;
    if (raw_size == 0) {
        *offset = 0;
        *size = 0;
    } else {
        /* assumption: allocation size is EBPF_BLOCK_SIZE */
        *offset = EBPF_BLOCK_SIZE * (raw_offset + 1);
        *size = EBPF_BLOCK_SIZE * raw_size;
    }
    return 0;
}

int ebpf_get_cell_type(uint8_t *cell) {
    return EBPF_CELL_SHORT_TYPE(cell[0]) ? EBPF_CELL_SHORT_TYPE(cell[0]) : EBPF_CELL_TYPE(cell[0]);
}

int ebpf_parse_cell_addr(uint8_t **cellp, uint64_t *offset, uint64_t *size, 
                         bool update_pointer) {
    uint8_t *cell = *cellp, *p = *cellp, *addr;
    uint8_t flags;
    uint64_t addr_len;
    int ret;

    /* read the first cell descriptor byte (cell type, RLE count) */
    if ((ebpf_get_cell_type(cell) != EBPF_CELL_ADDR_INT
         && ebpf_get_cell_type(cell) != EBPF_CELL_ADDR_LEAF
         && ebpf_get_cell_type(cell) != EBPF_CELL_ADDR_LEAF_NO)
        || ((cell[0] & EBPF_CELL_64V) != 0)) {
        return -EBPF_EINVAL;
    }
    p += 1;

    /* read the second cell descriptor byte (if present) */
    if ((cell[0] & EBPF_CELL_SECOND_DESC) != 0) {
        flags = *p;
        p += 1;
        if (flags != 0) {
            return -EBPF_EINVAL;
        }
    }

    /* the cell is followed by data length and a chunk of data */
    ret = ebpf_vunpack_uint(&p, &addr_len);
    if (ret != 0) {
        return ret;
    }
    addr = p;

    /* convert addr to file offset */
    ret = ebpf_addr_to_offset(addr, offset, size);
    if (ret != 0) {
        return ret;
    }

    if (update_pointer)
        *cellp = p + addr_len;
    return 0;
}

int ebpf_parse_cell_key(uint8_t **cellp, uint8_t **key, uint64_t *key_size, 
                        bool update_pointer) {
    uint8_t *cell = *cellp, *p = *cellp;
    uint64_t data_len;
    int ret;

    /* read the first cell descriptor byte (cell type, RLE count) */
    if ((ebpf_get_cell_type(cell) != EBPF_CELL_KEY)
        || ((cell[0] & EBPF_CELL_64V) != 0)) {
        return -EBPF_EINVAL;
    }
    p += 1;

    /* key cell does not have the second descriptor byte */

    /* the cell is followed by data length and a chunk of data */
    ret = ebpf_vunpack_uint(&p, &data_len);
    if (ret != 0) {
        return ret;
    }
    data_len += EBPF_CELL_SIZE_ADJUST;

    *key = p;
    *key_size = data_len;

    if (update_pointer)
        *cellp = p + data_len;
    return 0;
}

int ebpf_parse_cell_short_key(uint8_t **cellp, uint8_t **key, uint64_t *key_size, 
                              bool update_pointer) {
    uint8_t *cell = *cellp, *p = *cellp;
    uint64_t data_len;

    /* read the first cell descriptor byte */
    if (ebpf_get_cell_type(cell) != EBPF_CELL_KEY_SHORT) {
        return -EBPF_EINVAL;
    }
    data_len = cell[0] >> EBPF_CELL_SHORT_SHIFT;
    *key_size = data_len;

    p += 1;
    *key = p;

    if (update_pointer)
        *cellp = p + data_len;
    return 0;
}

int ebpf_get_page_type(uint8_t *page_image) {
    struct ebpf_page_header *header = (struct ebpf_page_header *)page_image;  /* page disk image starts with page header */
    return header->type;
}

/*
__wt_page_inmem: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/bt_page.c#L128
__inmem_row_int: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/bt_page.c#L375
WT_CELL_FOREACH_ADDR: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/cell.i#L1155
__wt_cell_unpack_safe: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/include/cell.i#L663
__wt_row_search: https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/row_srch.c#L331
*/
int ebpf_search_int_page(uint8_t *page_image, 
                         uint8_t *user_key_buf, uint64_t user_key_size,
                         uint64_t *descent_offset, uint64_t *descent_size, uint64_t *descent_index) {
    uint8_t *p = page_image;
    struct ebpf_page_header *header = (struct ebpf_page_header *)page_image;
    uint32_t nr_kv = header->u.entries / 2, i, ii;
    uint64_t prev_cell_descent_offset = 0, prev_cell_descent_size = 0;
    int ret;

    if (page_image == NULL
        || user_key_buf == NULL
        || user_key_size == 0
        || ebpf_get_page_type(page_image) != EBPF_PAGE_ROW_INT
        || descent_offset == NULL
        || descent_size == NULL) {
        printk("ebpf_search_int_page: invalid arguments\n");
        return -EBPF_EINVAL;
    }

    /* skip page header + block header */
    p += (EBPF_PAGE_HEADER_SIZE + EBPF_BLOCK_HEADER_SIZE);

    /* traverse all key value pairs */
    for (i = 0, ii = EBPF_BLOCK_SIZE; i < nr_kv && ii > 0; ++i, --ii) {
        uint8_t *cell_key_buf;
        uint64_t cell_key_size;
        uint64_t cell_descent_offset, cell_descent_size;
        int cmp;

        /*
         * searching for the corresponding descent.
         * each cell (key, addr) corresponds to key range [key, next_key)
         * extracted from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/row_srch.c#L331
         */

        /* parse key cell */
        switch (ebpf_get_cell_type(p)) {
        case EBPF_CELL_KEY:
            ret = ebpf_parse_cell_key(&p, &cell_key_buf, &cell_key_size, true);
            if (ret < 0) {
                printk("ebpf_search_int_page: ebpf_parse_cell_key failed, kv %d, offset %ld, ret %d\n", i, (uint64_t)(p - page_image), ret);
                return ret;
            }
            break;
        case EBPF_CELL_KEY_SHORT:
            ret = ebpf_parse_cell_short_key(&p, &cell_key_buf, &cell_key_size, true);
            if (ret < 0) {
                printk("ebpf_search_int_page: ebpf_parse_cell_short_key failed, kv %d, offset %ld, ret %d\n", i, (uint64_t)(p - page_image), ret);
                return ret;
            }
            break;
        default:
            printk("ebpf_search_int_page: invalid cell type %d, kv %d, offset %ld\n", ebpf_get_cell_type(p), i, (uint64_t)(p - page_image));
            return -EBPF_EINVAL;
        }
        /* parse addr cell */
        ret = ebpf_parse_cell_addr(&p, &cell_descent_offset, &cell_descent_size, true);
        if (ret < 0) {
            printk("ebpf_search_int_page: ebpf_parse_cell_addr failed, kv %d, offset %ld, ret %d\n", i, (uint64_t)(p - page_image), ret);
            return ret;
        }

        /*
         * compare with user key
         * extracted from https://github.com/wiredtiger/wiredtiger/blob/mongodb-4.4.0/src/btree/row_srch.c#L331
         */
        if (i == 0)
            cmp = 1;  /* 0-th key is MIN */
        else
            cmp = ebpf_lex_compare(user_key_buf, user_key_size, cell_key_buf, cell_key_size);
        if (cmp == 0) {
            /* user key = cell key */
            *descent_offset = cell_descent_offset;
            *descent_size = cell_descent_size;
            *descent_index = i;
            return 0;
        } else if (cmp < 0) {
            /* user key < cell key */
            *descent_offset = prev_cell_descent_offset;
            *descent_size = prev_cell_descent_size;
            *descent_index = i - 1;
            return 0;
        }
        prev_cell_descent_offset = cell_descent_offset;
        prev_cell_descent_size = cell_descent_size;
    }
    *descent_offset = prev_cell_descent_offset;
    *descent_size = prev_cell_descent_size;
    *descent_index = i - 1;
    return 0;
}

void ebpf_dump_page(uint8_t *page_image, uint64_t size) {
    int row, column, addr;
	uint64_t page_offset = 0;
    printk("=============================EBPF PAGE DUMP START=============================\n");
    for (row = 0; row < size / 16; ++row) {
        printk(KERN_CONT "%08llx  ", page_offset + 16 * row);
        for (column = 0; column < 16; ++column) {
            addr = 16 * row + column;
            printk(KERN_CONT "%02x ", page_image[addr]);
            if (column == 7 || column == 15) {
                printk(KERN_CONT " ");
            }
        }
        printk(KERN_CONT "|");
        for (column = 0; column < 16; ++column) {
            addr = 16 * row + column;
            if (page_image[addr] >= '!' && page_image[addr] <= '~') {
                printk(KERN_CONT "%c", page_image[addr]);
            } else {
                printk(KERN_CONT ".");
            }
        }
        printk(KERN_CONT "|\n");
    }
    printk("==============================EBPF PAGE DUMP END==============================\n");
}

u32 ebpf_lookup_kern(struct bpf_imposter_kern *context) {
    struct wt_ebpf_scratch *scratch = (struct wt_ebpf_scratch *)context->scratch;
    struct ebpf_page_header *header = (struct ebpf_page_header *)context->data;
    uint64_t descent_offset = 0, descent_size = 0, descent_index = 0;
    int ret;

    memcpy(((char *)context) + 1024 + EBPF_BLOCK_SIZE * scratch->level, (char *)context, EBPF_BLOCK_SIZE);
    ++scratch->nr_page;

    switch (header->type) {
    case EBPF_PAGE_ROW_INT:
        ret = ebpf_search_int_page((uint8_t *)context->data, scratch->key, scratch->key_size, &descent_offset, &descent_size, &descent_index);
        if (ret == 0) {
            scratch->descent_index_arr[scratch->level] = descent_index;
            /* fill control fields in the context */
            if (scratch->level == EBPF_MAX_DEPTH - 1) {
                /* buffer is full, return to the application immediately */
                context->done = true;
            } else {
                context->done = false;
                context->next_addr[0] = descent_offset;
                context->size[0] = EBPF_BLOCK_SIZE;
            }
            /* update scratch */
            ++scratch->level;
        } else {
            printk("ebpf_lookup_kern: ebpf_search_int_page failed, ret %d\n", ret);
        }
        break;
    case EBPF_PAGE_ROW_LEAF:
        /* reach leaf page, return to the application immediately */
        context->done = true;
        ret = 0;
        break;
    default:
        printk("ebpf_lookup_kern: unknown page type %d\n", header->type);
        ret = -EBPF_EINVAL;
    }
    return -1 * ret;
}