#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "nvram-common.h"

static char *nvram;
static size_t nvram_offset = 0;





static int nvram_load_ini(const char *path)
{
    FILE *fp;
    size_t file_size;
    char *k, *v;
    char *buf;

    fp = fopen(path, "r");
    if(fp == (FILE *) NULL) {
        DEBUG_PRINTF("Cannot open %s\n", path);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = malloc(file_size);
    if (buf == NULL) {
        DEBUG_PRINTF("Cannot allocate memory\n");
        return -1;
    }

    if (fread(buf, 1, file_size, fp) != file_size) {
        DEBUG_PRINTF("Cannot read ini file\n");
        free(buf);
        return -1;
    }
    fclose(fp);

    k = strtok(buf, "=");
    v = strtok(NULL, "\n");
    while(k != NULL && v != NULL) {
        if(kv_ptr_count >= max_kv_ptr_count) {
            max_kv_ptr_count *= 2;
            kv_pairs = realloc(kv_pairs, max_kv_ptr_count * sizeof(char *));
            if (kv_pairs == NULL) {
                printf("Cannot allocate memory\n");
                return -1;
            }
        }
        kv_pairs[kv_ptr_count++] = strdup(k);
        if (kv_pairs[kv_ptr_count] == NULL) {
            printf("Cannot allocate memory\n");
            kv_ptr_count--;
            return -1;
        }
        kv_pairs[kv_ptr_count++] = strdup(v);
        if (kv_pairs[kv_ptr_count] == NULL) {
            kv_ptr_count -= 2;
            printf("Cannot allocate memory\n");
            return -1;
        }
        k = strtok(NULL, "=");
        v = strtok(NULL, "\n");
    }

    free(buf);
    return 0;
}

int nvram_init(void)
{
    if (kv_pairs)
    {
        return 0;
    }
    kv_pairs = malloc(max_kv_ptr_count * sizeof(char *));
    if (NULL == kv_pairs)
    {
        DEBUG_PRINTF("Failed to allocate memory for key value array. Terminating.\n");
        return -1;
    }
    nvram_load_ini(NVRAM_INI_FILE_PATH);
    return 0;
}


char *nvram_get(const char *key)
{
    int i;
    int found=0;
    char *value;
    char *ret;
    for(i=0;i<kv_ptr_count;i+=2)
    {
        if(strcmp(key,kv_pairs[i]) == 0)
        {
            LOG_PRINTF("%s=%s\n",key,kv_pairs[i+1]);
            found = 1;
            value=kv_pairs[i+1];
            break;
        }
    }

    ret = NULL;
    if(!found)
    {
            LOG_PRINTF( RED_ON"%s=Unknown\n"RED_OFF,key);
    }else
    {
            ret=strdup(value);
    }
    return ret;
}

int nvram_set(const char *key, const char *value)
{
    int i;
    int found=0;
    for(i=0;i<kv_ptr_count;i+=2)
    {
        if(strcmp(key,kv_pairs[i]) == 0)
        {
            free(kv_pairs[i+1]);
            kv_pairs[i+1]=strdup(value);
            found = 1;
            break;
        }
    }
    if(!found)
    {
        kv_pairs[kv_ptr_count++]=strdup(key);
        kv_pairs[kv_ptr_count++]=strdup(value);
    }
    return 0;
}

int nvram_getall(char *buf, size_t len)
{
    int i, ks, vs, offset = 0;
    for(i = 0; i < kv_ptr_count; i += 2)
    {
        ks = strlen(kv_pairs[i]);
        vs = strlen(kv_pairs[i+1]);
        if (len < offset + ks + vs + 2) {
            return 0;
        }
        memcpy(buf + offset, kv_pairs[i], ks);
        offset += ks;
        buf[offset++] = '=';
        memcpy(buf + offset, kv_pairs[i+1], vs);
        offset += vs;
        buf[offset] = 0;
    }
    return 1;
}