#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>

typedef struct {
    const char *text;
    const char *pat;
    int64_t m;
    int64_t from;
    int64_t to;
    int64_t count;
} Task;

void *search(void *arg)
{
    Task *t = arg;
    printf("PID=%d TID=%d  начал поиск, позиции [%ld, %ld)\n", getpid(), gettid(), t->from, t->to);
    t->count = 0;
    for (int64_t i = t->from; i < t->to; ++i) {
        int64_t j = 0;
        while (j < t->m && t->text[i + j] == t->pat[j])
            j++;
        if (j == t->m)
            t->count++;
    }
    printf("PID=%d TID=%d  закончил, найдено %ld\n", getpid(), gettid(), t->count);
    return NULL;
}

int main(int argc, char *argv[])
{

    int res = 0;
    Task *tasks = NULL;
    char *text = NULL;
    pthread_t *th = NULL;

    if (argc != 3) {
        fprintf(stderr, "использование: %s <потоки> <образец>\n", argv[0]);
        fprintf(stderr, "строка для поиска читается со стандартного ввода\n");
        res = 1;
        goto end;
    }
    int threads = atoi(argv[1]);
    const char *pat = argv[2];
    int64_t m = strlen(pat);
    if (threads < 1 || m < 1) {
        fprintf(stderr, "число потоков и длина образца должны быть больше 0\n");
        res = 1;
        goto end;
    }
    

    size_t size = 0;
    printf("Введите строку:\n");
    int64_t n = getline(&text, &size, stdin);
    if (n == -1) {
        printf("строка не введена\n");
        res = 1;
        goto end;
    }
    if (n > 0 && text[n - 1] == '\n') {
        text[n - 1] = '\0';
        n--;
    }

    int64_t positions = n - m + 1;
    if (positions <= 0){
        printf("ОТВЕТ: 0\nМгновенный ответ без создания потоков, так как длина текста меньше длины образца\n");
        goto end;
    }
    if (positions < threads)
        threads = positions;
    int64_t chunk = positions / threads;

    th = malloc(threads * sizeof(pthread_t));
    tasks = malloc(threads * sizeof(Task));
    if (th == NULL || tasks == NULL) {
        perror("malloc");
        res = 1;
        goto end;
    }
    for (int k = 0; k < threads; ++k) {
        tasks[k].text = text;
        tasks[k].pat = pat;
        tasks[k].m = m;
        tasks[k].from = k * chunk;
        if (k == threads - 1)
            tasks[k].to = positions;
        else
            tasks[k].to = (k + 1) * chunk;
    }

    printf("PID=%d TID=%d  main: потоков %d\n", getpid(), gettid(), threads);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    if (threads == 1) {
        search(&tasks[0]);
    } else {
        for (int k = 0; k < threads; ++k) {
            int rc = pthread_create(&th[k], NULL, search, &tasks[k]);
            if (rc != 0) {
                fprintf(stderr, "pthread_create: %s\n", strerror(rc));
                res = 1;
                for (int l = 0; l < k; ++l){
                    int rc = pthread_join(th[l], NULL);
                    if (rc != 0)
                        fprintf(stderr, "pthread_join: %s\n", strerror(rc));
                }
                goto end;
            }
        }
        for (int k = 0; k < threads; ++k) {
            int rc = pthread_join(th[k], NULL);
            if (rc != 0)
                fprintf(stderr, "pthread_join: %s\n", strerror(rc));
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double sec = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;

    int64_t total = 0;
    for (int k = 0; k < threads; ++k)
        total += tasks[k].count;

    printf("потоков: %d, длина строки: %ld, образец: \"%s\"\n", threads, n, pat);
    printf("найдено вхождений: %ld\n", total);
    printf("время поиска: %.4f с\n", sec);

end:
    free(th);
    free(tasks);
    free(text);
    return res;
}
