#include <stdio.h>
#include "heap.h"

#define N 12345

struct S
{
	int x, y, z;
};

int cmp_s(const void *a, const void *b)
{
	const struct S *s = a, *t = b;
	if (s->x != t->x) return s->x - t->x;
	if (s->y != t->y) return s->y - t->y;
	if (s->z != t->z) return s->z - t->z;
	return 0;
}

int main()
{
	static struct S s[N];
	int i;

	for (i = 0; i < N; ++i)
	{
		s[i].x = rand()%26;
		s[i].y = rand()%26;
		s[i].z = rand()%26;
	}

	qsort(s, N, sizeof(struct S), cmp_s);

	for (i = 0; i < N; ++i)
	{
		printf("%c%c%c\n", (char)('a' + s[i].x), (char)('a' + s[i].y), (char)('a' + s[i].z));
	}
	
	return 0;
}
