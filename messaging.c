#include <kernel.h>

extern struct Task *currentTask;

void *AllocKMem(long);

//=================================
// Send a message to a message port
//=================================
void SendMessage(struct MessagePort *MP, struct Message *Msg)
{
	struct Message *temp = (struct Message *)ALLOCMSG;

	copyMem((unsigned char *)Msg, (unsigned char *)temp,
		(long)sizeof(struct Message));
	temp->pid = (long)currentTask->pid;
	asm("cli");
	if (MP->msgQueue == 0) {
		MP->msgQueue = temp;
	} else {
		struct Message *pMsg = MP->msgQueue;
		while (pMsg->nextMessage != 0) {
			pMsg = pMsg->nextMessage;
		}
		pMsg->nextMessage = temp;
	}
	asm("sti");
	if (MP->waitingProc != (struct Task *)-1L) {
		struct Task *task = MP->waitingProc;
		MP->waitingProc = (struct Task *)-1L;
		task->waiting = 0;
		UnBlockTask(task);
		SWTASKS;
	}
}

//======================================
// Receive a message from a message port
//======================================
void ReceiveMessage(struct MessagePort *MP, struct Message *Msg)
{
	struct Message *temp;

	while (MP->msgQueue == 0) {
		MP->waitingProc = currentTask;
		currentTask->waiting = 0x80;
		BlockTask(currentTask);
		SWTASKS;
	}
	asm("cli");
	temp = MP->msgQueue;
	MP->msgQueue = temp->nextMessage;
	temp->nextMessage = 0;
	asm("sti");
	copyMem((unsigned char *)temp, (unsigned char *)Msg,
		(long)sizeof(struct Message));
	DeallocMem(temp);
}

//========================
// Allocate a message port
//========================
struct MessagePort *AllocMessagePort()
{
	struct MessagePort *temp = (struct MessagePort *)AllocKMem((long)sizeof(struct MessagePort));

	temp->waitingProc = (struct Task *)-1L;
	temp->msgQueue = 0;
	return (temp);
}

//======================================================
// Send a message to a message port and wait for a reply
//======================================================
void SendReceiveMessage(struct MessagePort *MP, struct Message *Msg)
{
	struct MessagePort *tempMP = AllocMessagePort();

	Msg->tempPort = tempMP;
	SendMessage(MP, Msg);
	ReceiveMessage(tempMP, Msg);
	DeallocMem(tempMP);
}
