#include "cmemory.h"
#include "ckstructs.h"
#include "library/syscalls.h"

int main(void)
{
	WriteString("Task 4 is now running.", 14, 50);
	//writeconsolechar('A');
	while (1)
	{
		WriteDouble(GetTicks(), 23, 60);
	}
	return 0;
}