#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "shifting_hash_table.h"

#define TABLE_SEL_HASH Simple

unsigned int (* hash_func[MAX_SUBTABLE_NUM])(const unsigned char * str, unsigned int len) = {
        BOB1, BOB2, BOB3, BOB4, BOB5, BOB6, BOB7, BOB8, BOB9, BOB10,
        BOB11, BOB12, BOB13, BOB14, BOB15, BOB16
};

#define HASH(k, key, len) (hash_func[k])((const unsigned char *)key, (unsigned int)len)

void copy_item_to_bucket(struct HashEntryP * p_slot, const char * key, size_t key_len, VAL_TYPE val);
struct HashEntryP * make_new_node(const char * key, size_t key_len, VAL_TYPE val);
int query_bf(struct shifting_hash_table * p, const char * key, size_t key_len);
void insert_into_bf(struct shifting_hash_table * p, const char * key, size_t key_len, int offset);
void delete_from_bf(struct shifting_hash_table * p, const char * key, size_t key_len, int offset);
int check_match(struct HashEntryP * p, const char * key, size_t key_len);
void query_mem_access_log(struct shifting_hash_table * p, int mem_access_cnt);


int sht_init(struct shifting_hash_table *p_hash, int subtable_num, int subtable_len, int bloom_filter_bits)
{
    p_hash->subtable_len = subtable_len;
    p_hash->subtable_num = subtable_num;
    p_hash->bloom_filter = (char *)malloc((size_t)(bloom_filter_bits + MAX_SUBTABLE_NUM + 7) / 8);
    p_hash->counting_bloom_filter = (int *)malloc((bloom_filter_bits + MAX_SUBTABLE_NUM) * sizeof(int));
    memset(p_hash->bloom_filter, 0, (size_t)(bloom_filter_bits + MAX_SUBTABLE_NUM + 7) / 8);
    memset(p_hash->counting_bloom_filter, 0, (bloom_filter_bits + MAX_SUBTABLE_NUM) * sizeof(int));
    p_hash->bf_length = bloom_filter_bits;
    p_hash->bf_func_num = 7; // default hash function number of BF is 7

    int i;

    for (i = 0; i < subtable_num; ++i) {
        p_hash->subtables[i] = (struct HashEntryP *)malloc(sizeof(struct HashEntryP) * subtable_len);
        memset(p_hash->subtables[i], 0, sizeof(struct HashEntryP) * subtable_len);
    }

    srand((unsigned int)time(NULL));

    /**
     * for stat
     */
    p_hash->query_cnt = 0;
    p_hash->query_mem_access_tot = 0;
    memset(p_hash->query_mem_access_detail, 0, sizeof(p_hash->query_mem_access_detail));
    p_hash->abroad_cnt = 0;
    p_hash->query_mem_access_max = 0;
    p_hash->query_bf_mem_access_times = 0;

    return 0;
}

#define MAX(x, y) x > y ? x : y

inline void query_mem_access_log(struct shifting_hash_table * p_mhash, int mem_acc)
{
    p_mhash->query_mem_access_tot += mem_acc;
    p_mhash->query_mem_access_detail[mem_acc]++;
    p_mhash->query_mem_access_max = MAX(p_mhash->query_mem_access_max, mem_acc);
}

inline void insert_into_bf(struct shifting_hash_table * p, const char * key, size_t key_len, int offset)
{
    int i;
    for (i = 0; i < p->bf_func_num; ++i) {
        int pos = hash_func[i]((const unsigned char *)key, (unsigned int)key_len) % p->bf_length;
        pos += offset;
        p->bloom_filter[pos / 8] |= 1 << (pos % 8);
        p->counting_bloom_filter[pos]++;
    }
}

inline void delete_from_bf(struct shifting_hash_table * p, const char * key, size_t key_len, int offset)
{
    int i;
    for (i = 0; i < p->bf_func_num; ++i) {
        int pos = hash_func[i]((const unsigned char *)key, (unsigned int)key_len) % p->bf_length;
        pos += offset;
        p->counting_bloom_filter[pos]--;
        if (p->counting_bloom_filter[pos] == 0) {
            p->bloom_filter[pos / 8] &= ~(1 << (pos % 8));
        }
    }
}

inline int query_bf(struct shifting_hash_table * p, const char * key, size_t key_len)
{
    int ret = (1 << MAX_SUBTABLE_NUM) - 1;
    int i;
    for (i = 0; (i < p->bf_func_num) && ret; ++i) {
        int pos = hash_func[i]((const unsigned char *)key, (unsigned int)key_len) % p->bf_length;
        int temp = 0, j;
        for (j = 0; j < MAX_SUBTABLE_NUM / 8 + 2; ++j) {
            temp += (((unsigned int)((unsigned char)p->bloom_filter[pos / 8 + j])) << (j * 8));
        }
        temp >>= pos % 8;
        ret &= temp;
        ++p->query_bf_mem_access_times;
    }

    return ret;
}


inline void copy_item_to_bucket(struct HashEntryP * p_slot, const char * key, size_t key_len, VAL_TYPE val)
{
    p_slot->val = val;
    memcpy(p_slot->key, key, (size_t)key_len);
    p_slot->key_len = key_len;
}

inline struct HashEntryP * make_new_node(const char * key, size_t key_len, VAL_TYPE val)
{
    struct HashEntryP * p;
    p = (struct HashEntryP *)malloc(sizeof(struct HashEntryP));
    p->val = val;
    memcpy(p->key, key, key_len);
    p->key_len = key_len;
    p->next = NULL;
    return p;
}

int sht_insert(struct shifting_hash_table *p_hash, const char *origin_key, size_t key_len, VAL_TYPE val)
{
    char key[KEY_LEN];
    memcpy(key, origin_key, key_len);

    // check if origin position is empty
    int k = TABLE_SEL_HASH((const unsigned char *)key, (unsigned int)key_len) % p_hash->subtable_num;
    int default_pos = (hash_func[k])((const unsigned char *)key, (unsigned int)key_len) % p_hash->subtable_len;

    if (p_hash->subtables[k][default_pos].val == 0) {
        // insert the item directly
        copy_item_to_bucket(&p_hash->subtables[k][default_pos], key, key_len, val);
        return 0;
    }

    // if origin position is not empty, check the item in this place whether at home or not
    int temp_k = TABLE_SEL_HASH(
            (const unsigned char *) p_hash->subtables[k][default_pos].key,
            (unsigned int) p_hash->subtables[k][default_pos].key_len
    ) % p_hash->subtable_num;
    if (temp_k != k) {
        char temp_key[KEY_LEN];
        size_t temp_key_len;
        VAL_TYPE temp_val;

        temp_key_len = p_hash->subtables[k][default_pos].key_len;
        memcpy(temp_key, p_hash->subtables[k][default_pos].key, temp_key_len);
        temp_val = p_hash->subtables[k][default_pos].val;

        copy_item_to_bucket(&p_hash->subtables[k][default_pos], key, key_len, val);

        key_len = temp_key_len;
        memcpy((void *)key, temp_key, key_len);
        val = temp_val;

        delete_from_bf(p_hash, key, key_len, k);
        k = temp_k;
        default_pos = HASH(k, key, key_len) % p_hash->subtable_len;

        p_hash->abroad_cnt--;
    }

    // check if there is an empty place
    int i = 0, pos;
    int empty_slot[MAX_SUBTABLE_NUM], empty_cnt = 0, empty_pos[MAX_SUBTABLE_NUM];
    for (i = 0; i < p_hash->subtable_num; ++i) {
        pos = HASH(i, key, key_len) % p_hash->subtable_len;
        if (p_hash->subtables[i][pos].val == 0) {
            empty_pos[empty_cnt] = pos;
            empty_slot[empty_cnt++] = i;
        }
    }

    if (empty_cnt == 0) {
        // insert into default position
        struct HashEntryP * p = make_new_node(key, key_len, val),
                * prev = &p_hash->subtables[k][default_pos];
        p->next = prev->next;
        prev->next = p;

        return 0;
    }

    // random
    int rand_num = rand() % empty_cnt;
    int new_k = empty_slot[rand_num];
    int new_pos = empty_pos[rand_num];
    copy_item_to_bucket(&p_hash->subtables[new_k][new_pos], key, key_len, val);

    // insert into bloom filter
    insert_into_bf(p_hash, key, key_len, new_k);

    // stat
    p_hash->abroad_cnt++;

    return 0;
}

inline int check_match(struct HashEntryP * p, const char * key, size_t key_len)
{
    if (p->key_len != key_len) {
        return 0;
    }
    return memcmp(p->key, key, key_len) == 0;
}

VAL_TYPE sht_search(struct shifting_hash_table *p_hash, const char *key, size_t key_len)
{
    int pos_bitmap = query_bf(p_hash, key, key_len);
    int k, pos;

    /**
     * stat
     */
    p_hash->query_cnt++;
    int mem_acc = 0;

    for (k = 0; k < p_hash->subtable_num; ++k) {
        if ((pos_bitmap & (1 << k)) == 0) {
            continue;
        }
        pos = HASH(k, key, key_len) % p_hash->subtable_len;
        mem_acc++;
        if (check_match(&p_hash->subtables[k][pos], key, key_len)) {
            /**
             * stat
             */
            query_mem_access_log(p_hash, mem_acc);
            return p_hash->subtables[k][pos].val;
        }
    }

    // else, check the default position
    k = TABLE_SEL_HASH((const unsigned char *)key, (unsigned int)key_len) % p_hash->subtable_num;
    pos = HASH(k, key, key_len) % p_hash->subtable_len;
    struct HashEntryP * p_hash_entry = &p_hash->subtables[k][pos];
    for (; p_hash_entry; p_hash_entry = p_hash_entry->next) {
        // stat
        mem_acc++;
        if (check_match(p_hash_entry, key, key_len)) {
            query_mem_access_log(p_hash, mem_acc);
            return p_hash_entry->val;
        }
    }

    // else, return not found
    // stat
    query_mem_access_log(p_hash, mem_acc);
    return 0;
}

int sht_destroy(struct shifting_hash_table *p_hash)
{
    int i;
    for (i = 0; i < p_hash->subtable_num; ++i) {
        free(p_hash->subtables[i]);
    }
    free(p_hash->bloom_filter);
    free(p_hash->counting_bloom_filter);

    return 0;
}

#define FPRINTF_BR(pf) fprintf(pf, "%s", is_inline ? "" :  "\n-----\n" )

void sht_report(struct shifting_hash_table *p_hash, FILE *pf, int is_inline)
{
    // report subtable number
    if (is_inline)
        fprintf(pf, "%d\t", p_hash->subtable_num);

    // report load factor
    int full_buckets_cnt = 0;
    {
        double load_factor;
        for (int i = 0; i < p_hash->subtable_num; ++i) {
            for (int j = 0; j < p_hash->subtable_len; ++j) {
                full_buckets_cnt += p_hash->subtables[i][j].val ? 1 : 0;
            }
        }
        load_factor = ((double)full_buckets_cnt) / (p_hash->subtable_num * p_hash->subtable_len);
        if (is_inline)
            fprintf(pf, "%.5lf\t", load_factor);
        else
            fprintf(pf, "Load factor: %.5lf\n", load_factor);
    }

    // report linked length
    int tot_list_length = 0;
    {
        FPRINTF_BR(pf);
        int length_cnt[255];
        int max_cnt = 0;
        memset(length_cnt, 0, sizeof(length_cnt));
        for (int i = 0; i < p_hash->subtable_num; ++i) {
            for (int j = 0; j < p_hash->subtable_len; ++j) {
                struct HashEntryP * p;
                p = &p_hash->subtables[i][j];
                p = p->next;
                int temp_length = 0;
                while (p) {
                    temp_length++;
                    p = p->next;
                }
                tot_list_length += temp_length;
                length_cnt[temp_length]++;
                max_cnt = max_cnt > temp_length ? max_cnt : temp_length;
            }
        }
        double avg_link_length = ((double) tot_list_length) / (p_hash->subtable_num * p_hash->subtable_len);
        if (!is_inline) {
            fprintf(pf, "Linked List length Avg: %.5lf\n", avg_link_length);
            fprintf(pf, "\tLength\t       #\n");
            for (int i = 0; i <= max_cnt; ++i) {
                fprintf(pf, "\t%6d\t%8d\n", i, length_cnt[i]);
            }
        }
        else {
            fprintf(pf, "%d\t", tot_list_length);
        }
    }

    // report total insert number
    fprintf(pf, "%d\t", tot_list_length + full_buckets_cnt);

    // report abroad item count
    {
        FPRINTF_BR(pf);
        if (is_inline)
            fprintf(pf, "%d\t", p_hash->abroad_cnt);
        else
            fprintf(pf, "Abroad count: %d\n", p_hash->abroad_cnt);
    }

    // report query memory access count
    {
        FPRINTF_BR(pf);
        if (is_inline) {
            fprintf(pf, "%d\t", p_hash->query_cnt);
            fprintf(pf, "%.5lf\t", ((double) p_hash->query_mem_access_tot) / p_hash->query_cnt);
            for (int i =  0; i < 6; ++i) {
                fprintf(pf, "%d\t", p_hash->query_mem_access_detail[i]);
            }
        }
        else {
            fprintf(pf, "Total query number: %d\n", p_hash->query_cnt);
            fprintf(pf, "Avg mem access: %.5lf\n", ((double) p_hash->query_mem_access_tot) / p_hash->query_cnt);
            fprintf(pf, "\tAccNum\t#\tpercent\n");
            for (int i =  0; i <= p_hash->query_mem_access_max; ++i) {
                fprintf(pf, "\t%d\t%d\t%6.4lf\n", i, p_hash->query_mem_access_detail[i],
                        (double)100.0 * p_hash->query_mem_access_detail[i] / p_hash->query_cnt);
            }
        }
    }

    // report bloom filter query times
    {
        FPRINTF_BR(pf);
        if (is_inline) {
            fprintf(pf, "%d\t", p_hash->query_bf_mem_access_times);
        }
    }

    fprintf(pf, "\n");

}

void sht_clear_query_stats(struct shifting_hash_table *p_hash)
{
    p_hash->query_cnt = 0;
    p_hash->query_mem_access_tot = 0;
    p_hash->query_mem_access_max = 0;
    p_hash->query_bf_mem_access_times = 0;
    memset(p_hash->query_mem_access_detail, 0, sizeof(p_hash->query_mem_access_detail));
}
