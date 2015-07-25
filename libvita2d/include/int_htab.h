#ifndef INT_HTAB_H
#define INT_HTAB_H

#ifdef __cplusplus
extern "C" {
#endif

#define INT_HTAB_INITIAL_SIZE (256)
#define INT_HTAB_MAX_LOAD     (70) // over 100

typedef struct int_htab_entry {
	unsigned int key;
	void *value;
} int_htab_entry;

typedef struct int_htab {
	unsigned int size;
	unsigned int used;
	int_htab_entry *entries;
} int_htab;

int_htab *int_htab_create();
void int_htab_free(int_htab *htab);
int int_htab_insert(int_htab *htab, unsigned int key, void *value);
void *int_htab_get(const int_htab *htab, unsigned int key);


#ifdef __cplusplus
}
#endif

#endif
