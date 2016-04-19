#include <stdio.h>
#include <string.h>
#include "shifting_hash_table.h"

#define MAX_READ 1000
// input file name, which contains the key value pairs
#define INPUT_FILE "../sample.txt"

char read_key[MAX_READ][KEY_LEN];
int read_val[MAX_READ];

int main()
{
    FILE * fin = fopen(INPUT_FILE, "r");
    int i;

    for (i = 0; i < MAX_READ; ++i) {
        fscanf(fin, "%s %d", read_key[i], read_val + i);
    }
    fclose(fin);
    printf("read finished.\n");

    struct shifting_hash_table ht;

    // init the hash table first, parameters are
    // subtables number, buckets in each subtable, the size of the Bloom filter
    sht_init(&ht, 8, (int)(MAX_READ * 1.05 / 8), (int)(MAX_READ * 1.05 * 10));

    // insert some elements
    for (i = 0; i < MAX_READ; ++i) {
        sht_insert(&ht, read_key[i], strlen(read_key[i]), read_val[i]);
    }
    printf("insert finished.\n");

    // and query them
    for (i = 0; i < MAX_READ; ++i) {
        int ret = sht_search(&ht, read_key[i], strlen(read_key[i]));
        if (ret != read_val[i]) {
            printf("Mismatch! %s, expected %d, return %d.\n", read_key[i], read_val[i], ret);
        }
    }

    // report some statistic property, such as load factor
    sht_report(&ht, stdout, 0);

    // destroy it finally
    sht_destroy(&ht);

    return 0;
}
