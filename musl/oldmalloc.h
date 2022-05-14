#pragma once

void *musl_malloc(size_t n);
void musl_free(void *p);
void musl_dump_bins(void);
