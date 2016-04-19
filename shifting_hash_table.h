#ifndef SHT_H
#define SHT_H

#include <stdint.h>
#include "lib/hash_function.h"

#define KEY_LEN 30
#define VAL_TYPE int // the value type stored in the hash table

struct HashEntryP
{
    char key[KEY_LEN];
    size_t key_len;
    VAL_TYPE val;
    struct HashEntryP * next;
};

#define MAX_SUBTABLE_NUM 16


struct shifting_hash_table {
    int subtable_num; // how many subtables used in the second stage
    int subtable_len; // how many buckets used in each subtable
    char * bloom_filter; // the Bloom filter, it should be placed in some other device, such as FPGA, for the first stage
    int * counting_bloom_filter;
    int bf_length;
    int bf_func_num;
    struct HashEntryP * subtables[MAX_SUBTABLE_NUM];

    /* for stat */
    int query_cnt;
    int query_mem_access_tot;
    int query_mem_access_detail[256];
    int query_mem_access_max;
    int abroad_cnt;
    int query_bf_mem_access_times;
};


int sht_init(struct shifting_hash_table *p_hash, int subtable_num, int subtable_len, int bloom_filter_bits);
int sht_insert(struct shifting_hash_table *p_hash, const char *key, size_t key_len, VAL_TYPE val);
VAL_TYPE sht_search(struct shifting_hash_table *p_hash, const char *key, size_t key_len);
int sht_destroy(struct shifting_hash_table *p_hash);

/* for stat */
void sht_report(struct shifting_hash_table *p_hash, FILE *pf, int is_inline);
void sht_clear_query_stats(struct shifting_hash_table *p_hash);

#endif //SHT_H