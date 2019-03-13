#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Function pointers to hw3 functions */
void* (*mm_malloc)(size_t);
void* (*mm_realloc)(void*, size_t);
void (*mm_free)(void*);

void load_alloc_functions() {
    void *handle = dlopen("hw3lib.so", RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    char* error;
    mm_malloc = dlsym(handle, "mm_malloc");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    mm_realloc = dlsym(handle, "mm_realloc");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }

    mm_free = dlsym(handle, "mm_free");
    if ((error = dlerror()) != NULL)  {
        fprintf(stderr, "%s\n", dlerror());
        exit(1);
    }
}

void test_large_malloc() {
    printf("TEST: test_large_malloc\n");
    printf("=======================\n");
    printf("\tmalloc-ing some memory...\n");

    char *data = (char *) mm_malloc(sizeof(char) * 6);

    const char *name = "Arjun";

    memcpy(data, name, 6);

    printf("\tTrying to malloc a chunk of memory that is way too large... NULL should be returned.\n");

    char *large = (char *) mm_malloc(9223372036854775807);

    assert(large == NULL);

    printf("\tFinally, I free the memory...\n");

    mm_free(data);
}

void test_reuse_freed_memory() {
    printf("TEST: test_reuse_freed_memory\n");
    printf("=======================\n");
    printf("\tmalloc-ing some memory...\n");

    char *data = (char *) mm_malloc(sizeof(char) * 6);

    const char *name = "Arjun";

    memcpy(data, name, 6);

    printf("\tFreeing the memory I just malloc-ed.\n");

    mm_free(data);

    printf("\tmalloc-ing some memory of the same size...\n");

    char *data2 = (char *) mm_malloc(sizeof(char) * 6);

    printf("\tFinally, I free the memory again...\n");

    mm_free(data2);
}

void test_realloc_bigger() {
    printf("TEST: test_reuse_freed_memory\n");
    printf("=======================\n");
    printf("\tmalloc-ing some memory...\n");

    char *data = (char *) mm_malloc(sizeof(char) * 6);

    const char *name = "Arjun";

    memcpy(data, name, 6);

    printf("\tReallocing the data to a bigger array...\n");

    mm_realloc(data, 20);

    printf("\tThe initial values should still be there, but the newly allocated space should be 0\n");

    for (int i = 0; i <= strlen(name); i++) {
        if (name[i] != data[i]) {
            printf("\tFAIL: I expected data[%d] to be %c, but it was %c\n", i, name[i], data[i]);
        }
    }
    
    for (int i = strlen(name) + 1; i < 20; i++) {
        if (data[i] != 0) {
            printf("\tFAIL: I expected data[%d] to be 0, but it was %d\n", i, data[i]);
        }
    }

    printf("\tFinally, I free the memory...\n");

    mm_free(data);
}

int main() {
    load_alloc_functions();

    /*int *data = (int*) mm_malloc(sizeof(int));
    assert(data != NULL);
    data[0] = 0x162;
    mm_free(data);
    printf("malloc test successful!\n");*/

    test_realloc_bigger();

    return 0;
}
