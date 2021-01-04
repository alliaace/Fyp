#ifndef _GC_MANAGER_H_
#define _GC_MANAGER_H_

extern unsigned int garbage_collection_count;

void GARBAGE_COLLECTION_CHECK(void);

int G_C(void);
int BM_G_C(int32_t victim_pbn);
int32_t VICTIM_BLOCK_SELECTION(void);

int VALID_PAGES_COPY(int32_t old_pbn, int32_t new_pbn);
int REPLACED_BLOCK_MERGE(int32_t old_pbn, int32_t new_pbn);

#endif
