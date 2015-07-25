#include <stdlib.h>
#include <string.h>
#include "int_htab.h"

static inline unsigned int FNV_1a(unsigned int key)
{
	unsigned char *bytes = (unsigned char *)&key;
	unsigned int hash = 2166136261U;
	hash = (16777619U * hash) ^ bytes[0];
	hash = (16777619U * hash) ^ bytes[1];
	hash = (16777619U * hash) ^ bytes[2];
	hash = (16777619U * hash) ^ bytes[3];
	return hash;
}

int_htab *int_htab_create()
{
	int_htab *htab = malloc(sizeof(*htab));
	if (!htab)
		return NULL;


	htab->size = INT_HTAB_INITIAL_SIZE;
	htab->used = 0;

	htab->entries = malloc(INT_HTAB_INITIAL_SIZE * sizeof(*htab->entries));
	memset(htab->entries, 0, INT_HTAB_INITIAL_SIZE * sizeof(*htab->entries));

	return htab;
}

void int_htab_resize(int_htab *htab, unsigned int new_size)
{
	int i;
	int_htab_entry *old_entries;
	unsigned int old_size;

	old_entries = htab->entries;
	old_size = htab->size;

	htab->size = new_size;
	htab->used = 0;
	htab->entries = malloc(new_size * sizeof(*htab->entries));
	memset(htab->entries, 0, new_size * sizeof(*htab->entries));

	for (i = 0; i < old_size; i++) {
		if (old_entries[i].value != NULL) {
			int_htab_insert(htab, old_entries[i].key, old_entries[i].value);
		}
	}

	free(old_entries);
}

int int_htab_insert(int_htab *htab, unsigned int key, void *value)
{
	if (value == NULL)
		return 0;

	unsigned int mask = htab->size - 1;
	unsigned int idx = FNV_1a(key) & mask;

	/* Open addressing, linear probing */
	while (htab->entries[idx].value != NULL) {
		idx = (idx + 1) & mask;
	}

	/* Found an empty slot */
	if (htab->entries[idx].value == NULL) {
		htab->entries[idx].key = key;
		htab->entries[idx].value = value;
		htab->used++;

		/* Calculate current load factor */
		if ((htab->used*100)/htab->size > INT_HTAB_MAX_LOAD) {
			int_htab_resize(htab, 2*htab->size);
		}

		return 1;
	}

	return 0;
}

void *int_htab_get(const int_htab *htab, unsigned int key)
{
	unsigned int mask = htab->size - 1;
	unsigned int idx = FNV_1a(key) & mask;

	/* Open addressing, linear probing */
	while (htab->entries[idx].key != key && htab->entries[idx].value != NULL) {
		idx = (idx + 1) & mask;
	}

	/* Found the key */
	if (htab->entries[idx].key == key) {
		return htab->entries[idx].value;
	}

	return NULL;
}

void int_htab_free(int_htab *htab)
{
	int i;
	for (i = 0; i < htab->size; i++) {
		if (htab->entries[i].value != NULL)
			free(htab->entries[i].value);
	}
	free(htab);
}
