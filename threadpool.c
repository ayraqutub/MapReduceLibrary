#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "threadpool.h"
// typedef void (*thread_func_t)(void *arg);

// typedef struct ThreadPool_job_t {
//     thread_func_t func;              // function pointer
//     void *arg;                       // arguments for that function
//     struct ThreadPool_job_t *next;   // pointer to the next job in the queue
//     // add other members if needed
//     unsigned int time;               // length of job
// } ThreadPool_job_t;

// typedef struct {
//     unsigned int size;               // no. jobs in the queue
//     ThreadPool_job_t *head;          // pointer to the first (shortest) job
//     // add other members if needed
// } ThreadPool_job_queue_t;

// typedef struct {
//     pthread_t *threads;              // pointer to the array of thread handles
//     ThreadPool_job_queue_t jobs;     // queue of jobs waiting for a thread to run
//     // add other members if needed
//     pthread_cond_t jobAvailable;    // condition variable for when the queue is not empty
//     pthread_cond_t allIdle;         // condition variable for when all threads are idle
//     pthread_mutex_t lock;           // mutex for accessing the queue
//     bool stop;                      // flag for if the threadpool should stop
//     unsigned int active_threads;
// } ThreadPool_t;

/**
* C style constructor for creating a new ThreadPool object
* Parameters:
*     num - Number of threads to create
* Return:
*     ThreadPool_t* - Pointer to the newly created ThreadPool object
*/
ThreadPool_t *ThreadPool_create(unsigned int num){
    // allocate memory for threads
    ThreadPool_t *tp = (ThreadPool_t *)malloc(sizeof(ThreadPool_t));
    if (!tp) return NULL;

    // allocate memory for pointer to thread handles
    tp->threads = (pthread_t *)malloc(num * sizeof(pthread_t));
    if (!tp->threads) {
        free(tp);
        return NULL;
    }

    // initialize empty job queue
    tp->jobs.size = 0;
    tp->jobs.head = NULL;
    tp->stop = false;
    tp->active_threads = 0;

    // initialize mutex, condition variable
    pthread_mutex_init(&tp->lock, NULL);
    pthread_cond_init(&tp->jobAvailable, NULL);

    // create and initialize threads, threads execute Thread_run
    for (int i = 0; i < num; i++) {
        pthread_create(&tp->threads[i], NULL, (void *(*)(void *))Thread_run, (void *)tp); 
    }
    return tp;
}

/**
* C style destructor to destroy a ThreadPool object
* Parameters:
*     tp - Pointer to the ThreadPool object
*     num - Number of threads created (ADDED)
*/
void ThreadPool_destroy(ThreadPool_t *tp, unsigned int num){
    pthread_mutex_lock(&tp->lock);
    tp->stop = true;

    pthread_cond_broadcast(&tp->jobAvailable); // wake up threads waiting for a job
    pthread_cond_broadcast(&tp->allIdle); // wake up threads waiting for idle condition
    pthread_mutex_unlock(&tp->lock);

    // let all threads finish
    for (int i = 0; i < num; i++) {
        pthread_join(tp->threads[i], NULL);
    }

    pthread_mutex_destroy(&tp->lock);
    pthread_cond_destroy(&tp->jobAvailable);
    pthread_cond_destroy(&tp->allIdle);

    // free memory allocated for the threads
    free(tp -> threads);
    free(tp);
}

/**
* Add a job to the ThreadPool's job queue using Shortest Job First (SJF) scheduling
* Parameters:
*     tp   - Pointer to the ThreadPool object
*     func - Pointer to the function that will be called by the serving thread
*     arg  - Arguments for that function
*     time - Length of time the job will take (ADDED)
* Return:
*     true  - On success
*     false - Otherwise
*/
bool ThreadPool_add_job(ThreadPool_t *tp, thread_func_t func, void *arg, unsigned int time){
    ThreadPool_job_t *job = (ThreadPool_job_t *)malloc(sizeof(ThreadPool_job_t));
    if (!job) return false;

    job->func = func;
    job->arg = arg;
    job->next = NULL;
    job->time = time;

    pthread_mutex_lock(&tp->lock);

    if (tp->jobs.head == NULL || tp->jobs.head->time > job->time){
        job->next = tp->jobs.head;
        tp->jobs.head = job;
    } else {
        ThreadPool_job_t *current = tp->jobs.head;
        while (current->next != NULL && current->next->time < job->time){
            current = current->next;
        }
        job->next = current->next;
        current->next = job;
    }
    tp->jobs.size++;

    pthread_cond_signal(&tp->jobAvailable); 
    pthread_mutex_unlock(&tp->lock);

    return true;
}

/**
* Get a job from the job queue of the ThreadPool object
* Parameters:
*     tp - Pointer to the ThreadPool object
* Return:
*     ThreadPool_job_t* - Next job to run
*/
ThreadPool_job_t *ThreadPool_get_job(ThreadPool_t *tp){
    //pthread_mutex_lock(&tp->lock);
    // while (tp->jobs.size == 0 && !tp->stop){
    //     pthread_cond_wait(&tp->jobAvailable, &tp->lock);
    // }
    // if (tp->stop){
    //     //pthread_mutex_unlock(&tp->lock);
    //     return NULL;
    // }
    ThreadPool_job_t *job = tp->jobs.head;
    if (job){
        tp->jobs.head = job->next;
        tp->jobs.size--;
    }
    //pthread_mutex_unlock(&tp->lock);
    return job;
}

/**
* Start routine of each thread in the ThreadPool Object
* In a loop, check the job queue, get a job (if any) and run it
* Parameters:
*     tp - Pointer to the ThreadPool object containing this thread
*/
void *Thread_run(ThreadPool_t *tp){
    while (1){
        pthread_mutex_lock(&tp->lock);
        while (tp->jobs.size == 0 && !tp->stop){
            pthread_cond_wait(&tp->jobAvailable, &tp->lock);
        }
        if (tp->stop && tp->jobs.size == 0){
            pthread_mutex_unlock(&tp->lock);
            break;
        }

        ThreadPool_job_t *job = ThreadPool_get_job(tp);
        if(job == NULL){
            if(tp->stop){
                pthread_mutex_unlock(&tp->lock);
                break;
            }
            pthread_mutex_unlock(&tp->lock);
            continue;
        }
        tp->active_threads++;
        pthread_mutex_unlock(&tp->lock);

        job->func(job->arg);

        pthread_mutex_lock(&tp->lock);
        free(job);
        tp->active_threads--;
        if (tp->jobs.size == 0 && tp->active_threads == 0){
            pthread_cond_signal(&tp->allIdle);
        }
        pthread_mutex_unlock(&tp->lock);

    }
    return NULL;
}

/**
* Ensure that all threads are idle and the job queue is empty before returning
* Parameters:
*     tp - Pointer to the ThreadPool object 
*/
void ThreadPool_check(ThreadPool_t *tp){
    pthread_mutex_lock(&tp->lock);
    while (tp->jobs.size > 0 || tp->active_threads > 0){
        pthread_cond_wait(&tp->allIdle, &tp->lock);
    }
    pthread_mutex_unlock(&tp->lock);
}