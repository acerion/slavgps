#include <stdio.h>
#include <bzlib.h>

int main(void)
{
	int bzerror;
	FILE * file = NULL;
	BZFILE * bf = BZ2_bzReadOpen(&bzerror, file, 0, 0, NULL, 0);
	BZ2_bzReadClose(&bzerror, bf);

	return 0;
}
