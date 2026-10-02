// Access to the translated game composite. Desktop and iOS load it as a shared
// library (dlopen/dlsym). BLUEWAKE_STATIC_COMPOSITE builds (the Switch NRO)
// link it in as one relocatable object whose defined symbols carry a bwc_
// prefix, keeping the composite's own copy of the CPU runtime separate from
// the host's exactly as the shared library does; lookups then go through a
// fixed table.
#pragma once

// Returns an opaque handle, or NULL with bluewake_composite_error() set.
void* bluewake_composite_open(const char* path);

// Returns the named export, or NULL if this composite does not provide it.
void* bluewake_composite_symbol(void* composite, const char* name);

// The reason the last open or lookup failed.
const char* bluewake_composite_error(void);
