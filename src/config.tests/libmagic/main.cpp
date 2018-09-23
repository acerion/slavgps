/*
  http://doc.qt.io/qt-5/qmake-test-function-reference.html#qtcompiletest-test
*/
#include <magic.h>
int main()
{
	magic_t myt = magic_open(MAGIC_CONTINUE|MAGIC_ERROR|MAGIC_MIME);
	if (myt) {
		magic_close(myt);
	}
	return 0;
}
