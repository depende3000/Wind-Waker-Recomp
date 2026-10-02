// Gives every thread a stack large enough for Dawn's shader compiler. libnx
// gives std::thread (and any pthread created without a size) 128 KiB, and
// Tint's recursive WGSL parser and resolver overflow that (Atmosphère crash
// report 2168-0002 in tint::resolver on the Dawn probe). Aurora creates its
// workers with std::thread, so the host links with
// -Wl,--wrap=pthread_create and every thread asking for less gets 4 MiB.
#include <pthread.h>

#define MINIMUM_STACK_SIZE (4u * 1024u * 1024u)

int __real_pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                          void* (*start)(void*), void* arg);

int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                          void* (*start)(void*), void* arg) {
    pthread_attr_t sized;
    if (attr != NULL) {
        size_t requested = 0;
        if (pthread_attr_getstacksize(attr, &requested) == 0 && requested >= MINIMUM_STACK_SIZE)
            return __real_pthread_create(thread, attr, start, arg);
        sized = *attr;
    } else {
        pthread_attr_init(&sized);
    }
    pthread_attr_setstacksize(&sized, MINIMUM_STACK_SIZE);
    const int result = __real_pthread_create(thread, &sized, start, arg);
    if (attr == NULL)
        pthread_attr_destroy(&sized);
    return result;
}
