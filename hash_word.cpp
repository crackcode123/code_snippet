
#include<stdlib.h>
#include<string>
#include<cstring>
#include<stdint.h>
#include<stddef.h>

//
// count hash table
//

typedef struct _word_node
{
	char* word;
	uint16_t count;
	struct _word_node* next;
} word_node;

//
// max 11 words
//
#define max_bucket 11
#define hash_multiplier 31
word_node* bin[max_bucket];

static uint32_t hash(char* const str)
{
	uint32_t h = 0;

	for (char* p = str; *p; p++)
	{
		h = (h* hash_multiplier) + *p;
	}

	return h % max_bucket;
}

static void make_hash(char* const str)
{
	uint32_t hash_this_str = hash(str);
	uint16_t found = false;

	for (word_node* h1 = bin[hash_this_str]; h1 != NULL; h1 = h1->next)
	{
		if (!strcmp(h1->word, str))
		{
			++(h1->count);
			found = true;
			break;
		}
	}

	if (!found)
	{

		size_t node_size = sizeof(word_node);
		struct _word_node* make_new_object = (struct _word_node*)malloc(node_size);

		make_new_object->word = (char*)malloc((strlen(str) + 1));
		make_new_object->count = 0;

		strcpy(make_new_object->word, str);
		++(make_new_object->count);

		make_new_object->next = bin[hash_this_str];
		bin[hash_this_str] = make_new_object;
	}

}

void word_count_main()
{

	for (int i = 0; i < max_bucket; ++i)
	{
		bin[i] = NULL;

	}

	const char* array[] = { "CAT", "CAT", "Hello", "Dadu", "BAT" };

	for (int k = 0; k < 5; ++k)
	{
		make_hash((char*)array[k]);
	}
}

int main()
{
	word_count_main();
	return 0;
}

