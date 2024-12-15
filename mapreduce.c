#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include "mapreduce.h"
#include "threadpool.h"

// function pointer typedefs
typedef void (*Mapper)(char *file_name);
typedef void (*Reducer)(char *key, unsigned int partition_idx);

typedef struct KeyValueNode {
    char *key;
    char *value;
    struct KeyValueNode *next;
} KeyValueNode;

typedef struct Partition {
    KeyValueNode *head;
    unsigned int *index;
    pthread_mutex_t lock;
} Partition;

typedef struct Args {
    unsigned int pid;
    Reducer callback;
}Args;

Partition *partitions;
unsigned int num_partitions;

ThreadPool_t *thread_pool;

/**
* Run the MapReduce framework
* create a thread pool of size T threads 
* and use these threads to perform the map and reduce tasks
* all map tasks must be run before any reduce tasks
* Parameters:
*     file_count   - Number of files (i.e. input splits)
*     file_names   - Array of filenames
*     mapper       - Function pointer to the map function
*     reducer      - Function pointer to the reduce function
*     num_workers  - Number of threads in the thread pool
*     num_parts    - Number of partitions to be created
*/
void MR_Run(unsigned int file_count, char *file_names[], Mapper mapper, Reducer reducer, unsigned int num_workers, unsigned int num_parts){
    // Initialize partitions
    num_partitions = num_parts;
    partitions = (Partition *)malloc(num_parts * sizeof(Partition));
    partitions->index = (unsigned int *)calloc(num_parts, sizeof(unsigned int));
    for (unsigned int i = 0; i < num_partitions; i++) {
        partitions[i].head = NULL;
        pthread_mutex_init(&partitions[i].lock, NULL);
    }
    ThreadPool_t *tp = ThreadPool_create(num_workers);
    bool success = false;
    struct stat *fileStat = (struct stat *)malloc(sizeof(struct stat));
    for (unsigned int i = 0; i < file_count; i++) {
        stat(file_names[i], fileStat);
        size_t file_size = fileStat->st_size;

        success = ThreadPool_add_job(tp, (void *)mapper, file_names[i], file_size);
        if (!success) {
            fprintf(stderr, "Something went wrong adding the job!\n");
            break;
        }
    }
    free(fileStat);
    ThreadPool_check(tp);

    for (unsigned int i = 0; i < num_partitions; i++) {
        pthread_mutex_lock(&partitions[i].lock);
        unsigned int partition_size = 0;
        KeyValueNode *current = partitions[i].head;
        while (current) {
            partition_size++;
            current = current->next;
        }

        Args *args = (Args *)malloc(sizeof(Args));

        args->callback = reducer;
        args->pid = i;

        pthread_mutex_unlock(&partitions[i].lock);
        ThreadPool_add_job(tp, (void *)MR_Reduce, args, partition_size);
    }
    ThreadPool_check(tp);
    ThreadPool_destroy(tp, num_workers);
    for (unsigned int i = 0; i < num_partitions; i++) {
        KeyValueNode *current = partitions[i].head;
        while (current) {
            KeyValueNode *next = current->next;
            free(current->key);
            // free(current->value);
            free(current);
            current = next;
        }
        pthread_mutex_destroy(&partitions[i].lock);
    }
    free(partitions);
    free(partitions->index);
}

/**
* Write a specifc map output, a <key, value> pair, to a partition
* takes a key-value pair produced by a mapper and writes it to a specific partition.
* This partition is determined by passing the key of this pair to the MR Partitioner library function
* Parameters:
*     key           - Key of the output
*     value         - Value of the output
*/
void MR_Emit(char *key, char *value){
    unsigned int partition = MR_Partitioner(key, num_partitions);
    KeyValueNode *node = (KeyValueNode *)malloc(sizeof(KeyValueNode));
    node->key = strdup(key);
    node->value = strdup(value);
    node->next = NULL;

    pthread_mutex_lock(&partitions[partition].lock);
    if(partitions[partition].head == NULL || strcmp(key, partitions[partition].head->key) < 0){
        node->next = partitions[partition].head;
        partitions[partition].head = node;
    }
    else{
        KeyValueNode *current = partitions[partition].head;
        while(current->next && strcmp(key, current->next->key) >= 0){
            current = current->next;
        }
        node->next = current->next;
        current->next = node;
    }

    pthread_mutex_unlock(&partitions[partition].lock);
}

/**
* Hash a mapper's output to determine the partition that will hold it
* Parameters:
*     key           - Key of a specifc map output
*     num_partitions- Total number of partitions
* Return:
*     unsigned int  - Index of the partition
*/
unsigned int MR_Partitioner(char *key, unsigned int num_partitions){
    unsigned long hash = 5381;
    int c;
    while ((c = *key++) != '\0') {
        hash = hash * 33 + c;
    }
    return hash % num_partitions;
}

/**
* Run the reducer callback function for each <key, (list of values)> 
* retrieved from a partition
* Parameters:
*     threadarg     - Pointer to a hidden args object
*/
void MR_Reduce(void *threadarg){
    Args *args = (Args *)threadarg;
    unsigned int partition_idx = args->pid;

    KeyValueNode *current = partitions[partition_idx].head;
    while (current != NULL){
        char *key = strdup(current->key);

        args->callback(key, partition_idx);
        while (current && strcmp(current -> key, key) == 0){
            current = current -> next;
        }
        //free(key);
    }
    free(args);
}

/**
* Get the next value of the given key in the partition
* Parameters:
*     key           - Key of the values being reduced
*     partition_idx - Index of the partition containing this key
* Return:
*     char *        - Value of the next <key, value> pair if its key is the current key
*     NULL          - Otherwise
*/
char *MR_GetNext(char *key, unsigned int partition_idx){
    if(partition_idx >= num_partitions){
        return NULL;
    }

    KeyValueNode *current = partitions[partition_idx].head;
    for(unsigned int i = 0; i < partitions->index[partition_idx]; i++){
        current = current->next;
    }

    if(current == NULL){
        return NULL;
    }

    if(strcmp(key, current->key) != 0){
        return NULL;
    }

    partitions->index[partition_idx]++;

    return current->value;
}