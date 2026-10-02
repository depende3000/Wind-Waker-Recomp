#include "composite_provider.h"

#include <stddef.h>
#include <string.h>

#ifndef BLUEWAKE_STATIC_COMPOSITE

#include <dlfcn.h>

void* bluewake_composite_open(const char* path) {
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void* bluewake_composite_symbol(void* composite, const char* name) {
    return dlsym(composite, name);
}

const char* bluewake_composite_error(void) {
    const char* error = dlerror();
    return error != NULL ? error : "unknown dynamic loader error";
}

#else

// Every export the host looks up. Each is weak so a composite without, say,
// the options mod links and reports it missing, as dlsym would. Keep this list
// in step with the names passed to bluewake_composite_symbol.
#define BLUEWAKE_COMPOSITE_EXPORTS(X) \
    X(staticrecomp_get_module)        \
    X(staticrecomp_get_rel_data)      \
    X(ppc_guest_alias_clear)          \
    X(ppc_guest_alias_add_shared)     \
    X(bluewake_set_mem_write_journal) \
    X(bluewake_set_edge_service)      \
    X(bluewake_composite_mod_count)   \
    X(bluewake_composite_mod_name)    \
    X(bluewake_composite_apply_mods)  \
    X(bluewake_composite_mod_writes)  \
    X(bluewake_composite_option_flags) \
    X(bluewake_composite_set_native_hook) \
    X(bluewake_composite_option_count) \
    X(bluewake_composite_option)      \
    X(bluewake_composite_option_writes)

#define DECLARE_EXPORT(name) extern char bwc_##name[] __attribute__((weak));
BLUEWAKE_COMPOSITE_EXPORTS(DECLARE_EXPORT)
#undef DECLARE_EXPORT

static const struct {
    const char* name;
    void* address;
} kExports[] = {
#define EXPORT_ENTRY(name) {#name, bwc_##name},
    BLUEWAKE_COMPOSITE_EXPORTS(EXPORT_ENTRY)
#undef EXPORT_ENTRY
};

static const char* g_error = "";

void* bluewake_composite_open(const char* path) {
    (void)path;
    if (bwc_staticrecomp_get_module == NULL) {
        g_error = "this build has no linked composite";
        return NULL;
    }
    return (void*)kExports;
}

void* bluewake_composite_symbol(void* composite, const char* name) {
    (void)composite;
    for (size_t i = 0; i < sizeof(kExports) / sizeof(kExports[0]); ++i) {
        if (strcmp(kExports[i].name, name) == 0) {
            if (kExports[i].address == NULL)
                g_error = "symbol not present in the linked composite";
            return kExports[i].address;
        }
    }
    g_error = "symbol is not in the static export table";
    return NULL;
}

const char* bluewake_composite_error(void) {
    return g_error;
}

#endif
