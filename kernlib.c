#include <kernel.h>
#include <filesystem.h>
#include <console.h>
#include <sys/errno.h>
#include <linux/types.h>
#include <time.h>
#include "blocks.h"

#define O_CREAT	0x0200

extern struct Task *currentTask;
extern struct MessagePort *FSPort;
extern struct MessagePort *KbdPort;
extern struct MessagePort *ConsolePort;

extern long sec, min, hour, day, month, year, unixtime;

void PrintClock()
{
	kprintf(0, 63, "                 ");
//	kprintf(0, 63, "%2d/%02d/%02d %2d:%02d:%02d", day, month, year, hour, min,
//			sec);
	kprintf(0, 63, "%d", unixtime);
}

void setclock()
{
	struct tm tm;
	tm.tm_sec = sec;
	tm.tm_min = min;
	tm.tm_hour = hour;
	tm.tm_mday = day;
	tm.tm_mon = month - 1;
	tm.tm_year = year + 100;
	tm.tm_isdst = -1;
	tm.tm_gmtoff = 0;
	Debug();
	unixtime = mktime(&tm);
}

long FindFirstFreeFD()
{
	long n;
	long bitmap = currentTask->FDbitmap;

	for (n = 0; n < sizeof(long); n++)
		if (!(bitmap & (1 << n)))
			return n;
	return -EMFILE;
}

//===============================================================
// Convert a file descriptor for the current process to an FCB
//===============================================================
struct FCB *fdToFCB(FD fileDescriptor)
{
	struct FCB *temp = currentTask->fcbList;
	while (temp)
	{
		if (temp->fileDescriptor == fileDescriptor)
			break;
		temp = temp->nextFCB;
	}
	return temp;
}
//================================================
// Read noBytes into buffer from the file fHandle
//================================================
long ReadFromFile(struct FCB *fHandle, char *buffer, long noBytes)
{
	if (noBytes == 0)
		return 0;

	long retval = 0;
	long bytesRead = 0;
	char done = 0;
	char *buff = AllocKMem(PageSize);
	struct Message *FSMsg = ALLOCMSG;

	while (noBytes > PageSize)
	{
		FSMsg->nextMessage = 0;
		FSMsg->byte = READFILE;
		FSMsg->quad = (long) fHandle;
		FSMsg->quad2 = (long) buff;
		FSMsg->quad3 = PageSize;
		SendReceiveMessage(FSPort, FSMsg);
		copyMem(buff, buffer + bytesRead, FSMsg->quad);
		bytesRead += FSMsg->quad;
		noBytes -= PageSize;
		if (FSMsg->quad < PageSize)
			done = 1;
	}
	if (!done)
	{
		FSMsg->nextMessage = 0;
		FSMsg->byte = READFILE;
		FSMsg->quad = (long) fHandle;
		FSMsg->quad2 = (long) buff;
		FSMsg->quad3 = noBytes;
		SendReceiveMessage(FSPort, FSMsg);
		copyMem(buff, buffer + bytesRead, FSMsg->quad);
		bytesRead += FSMsg->quad;
	}

	retval = bytesRead;
	DeallocMem(buff);
	DeallocMem(FSMsg);
	return (retval);
}

//===========================================
// A utility function to copy a memory range
//===========================================
void copyMem(unsigned char *source, unsigned char *dest, long size)
{
	int i;

	if (dest < (unsigned char *) 0x10000)
	{
		KWriteString("OOps!!!", 20, 40);
		asm("cli;" "hlt;");
	}

	for (i = 0; i < size; i++)
	{
		dest[i] = source[i];
	}
}

//===========================================================================
// A kernel library function to write a null-terminated string to the screen.
//===========================================================================
void KWriteString(char *str, int row, int col)
{
	char *VideoBuffer = (char *) 0x80000B8000;

	int temp = 160 * row + 2 * col;
	int i = 0;
	while (str[i] != 0)
	{
		VideoBuffer[temp + 2 * i] = str[i];
		i++;
	}
}

//==========================================================================
// A kernel library function to write a quad character to the screen.
//==========================================================================
void KWriteHex(long c, int row) //, int col)
{
	char *VideoBuffer = (char *) 0x80000B8000;

	int i;
	for (i = 0; i < 8; i++)
	{
		char lo = c & 0xF;
		lo += 0x30;
		if (lo > 0x39)
		{
			lo += 7;
		}
		char hi = (c & 0xF0) >> 4;
		hi += 0x30;
		if (hi > 0x39)
		{
			hi += 7;
		}
		VideoBuffer[160 * row + 28 - 4 * i] = hi;
		VideoBuffer[160 * row + 30 - 4 * i] = lo;
		c = c >> 8;
	}
}

//=========================================================
//  Opens the file s.
//  Returns a FD for the file in RAX
//=========================================================
FD DoOpen(unsigned char *s, int flags)
{
	struct FCB *fcb = 0;

	unsigned char *S = NameToFullPath(s);
	struct Message *msg = ALLOCMSG;
	msg->nextMessage = 0;
	msg->byte = OPENFILE;
	msg->quad = (long) S;
	SendReceiveMessage(FSPort, msg);
	fcb = (struct FCB *) msg->quad;
	if (((long)fcb < 0) && (flags & O_CREAT))
	{
		msg->nextMessage = 0;
		msg->byte = CREATEFILE;
		msg->quad = (long) S;
		SendReceiveMessage(FSPort, msg);
		fcb = (struct FCB *) msg->quad;
	}
	DeallocMem(S);
	DeallocMem(msg);
	if (fcb > 0)
	{
		FD fileDescriptor = FindFirstFreeFD();
		if (fileDescriptor == -EMFILE)
			return -EMFILE;
		currentTask->FDbitmap |= 1 << fileDescriptor;
		fcb->fileDescriptor = fileDescriptor;
		struct FCB *temp = currentTask->fcbList;
		while (temp->nextFCB)
			temp = temp->nextFCB;
		temp->nextFCB = fcb;
		return fileDescriptor;
	}
	else
		return (long) fcb;
}

//=========================================================
// 
//=========================================================
int DoClose(FD fileDescriptor)
{
	struct FCB *temp = fdToFCB(fileDescriptor);
	if (temp)
	{
		currentTask->FDbitmap &= ~(1 << temp->fileDescriptor);
		struct FCB *temp2 = currentTask->fcbList;
		if (temp2 == temp)
			currentTask->fcbList = currentTask->fcbList->nextFCB;
		else
		{
			while (temp2->nextFCB != temp)
				temp2 = temp2->nextFCB;
			temp2->nextFCB = temp->nextFCB;
		}
		if (temp->deviceType == KBD || temp->deviceType == CONS)
			return 0;
		struct Message *msg = ALLOCMSG;

		msg->nextMessage = 0;
		msg->byte = CLOSEFILE;
		msg->quad = (long) temp;
		SendReceiveMessage(FSPort, msg);
		DeallocMem(msg);
		return 0;
	}
	return -EBADF;
}

int DoStat(char *path, struct FileInfo *info) // *** No - this should take a filename!!!
{
	unsigned char *S = NameToFullPath(path);
	struct Message *msg = ALLOCMSG;
	char *buff = (char *) AllocKMem(sizeof(struct FileInfo));

	msg->nextMessage = 0;
	msg->byte = GETFILEINFO;
	msg->quad = (long) S;
	msg->quad2 = (long) buff;
	SendReceiveMessage(FSPort, msg);
	int retval = -ENOENT;
	if (msg->quad)
	{
		retval = 0;
		copyMem(buff, (char *) info, sizeof(struct FileInfo));
	}
	DeallocMem(buff);
	DeallocMem(S);
	DeallocMem(msg);
	return retval;
}

int DoFStat(FD fileDescriptor, struct FileInfo *info)
{
	struct FCB *temp = fdToFCB(fileDescriptor);
	if (temp)
	{
		if (temp->deviceType == KBD || temp->deviceType == CONS)
		{
			info->Length = 0;
		}
		else
		{
			struct Message *msg = ALLOCMSG;
			char *buff = (char *) AllocKMem(sizeof(struct FileInfo));

			msg->nextMessage = 0;
			msg->byte = GETFFILEINFO;
			msg->quad = (long) temp;
			msg->quad2 = (long) buff;
			SendReceiveMessage(FSPort, msg);
			copyMem(buff, (char *) info, sizeof(struct FileInfo));
			DeallocMem(buff);
			DeallocMem(msg);
			return 0;
		}
	}
	return -EBADF;
}
long DoRead(FD fileDescriptor, char *buffer, long noBytes)
{
	if (!noBytes)
		return 0;

	long retval = 0;

	struct FCB *temp = fdToFCB(fileDescriptor);
	if (temp)
	{
		if (temp->deviceType == KBD)
		{
			struct Message *kbdMsg;

			kbdMsg = ALLOCMSG;
			kbdMsg->nextMessage = 0;
			kbdMsg->byte = 1; // GETCHAR message
			kbdMsg->quad = currentTask->console;
			SendReceiveMessage(KbdPort, kbdMsg);
			char c = kbdMsg->byte;
			buffer[0] = c;
			buffer[1] = 0;
			DeallocMem(kbdMsg);
			return (1);
		}
		else
			retval = ReadFromFile(temp, buffer, noBytes);
	}
	return (retval);
}

long DoWrite(FD fileDescriptor, char *buffer, long noBytes)
{
	if (!noBytes)
		return 0;

	long retval = 0;

	struct FCB *temp = fdToFCB(fileDescriptor);
	if (temp)
	{
		if (temp->deviceType == CONS)
		{
			char *buff = AllocKMem(noBytes + 1);
			copyMem(buffer, buff, noBytes);
			buff[noBytes] = 0;

			struct Message *msg = ALLOCMSG;
			msg->nextMessage = 0;
			msg->byte = WRITESTR;
			msg->quad = (long) buff;
			msg->quad2 = currentTask->console;
			SendMessage(ConsolePort, msg);
			DeallocMem(msg);
			retval = noBytes;
		}
		else
		{
			struct Message *FSMsg;

			FSMsg = ALLOCMSG;
			char *buff = AllocKMem(noBytes);
			copyMem(buffer, buff, noBytes);

			FSMsg->nextMessage = 0;
			FSMsg->byte = WRITEFILE;
			FSMsg->quad = (long) temp;
			FSMsg->quad2 = (long) buff;
			FSMsg->quad3 = noBytes;
			SendReceiveMessage(FSPort, FSMsg);
			DeallocMem(buff);
			retval = FSMsg->quad;
			DeallocMem(FSMsg);
		}
	}
	return (retval);
}

FD DoCreate(unsigned char *s)
{
	unsigned char *S = NameToFullPath(s);
	struct Message *msg = ALLOCMSG;
	msg->nextMessage = 0;
	msg->byte = CREATEFILE;
	msg->quad = (long) S; //str;
	SendReceiveMessage(FSPort, msg);
	DeallocMem(S);
	long retval = msg->quad;
	DeallocMem(msg);
	struct FCB *fcb = (struct FCB *) retval;
	if (fcb)
	{
		FD fileDescriptor = FindFirstFreeFD();
		if (fileDescriptor == -EMFILE)
			return -EMFILE;
		currentTask->FDbitmap |= 1 << fileDescriptor;
		fcb->fileDescriptor = fileDescriptor;
		struct FCB *temp = currentTask->fcbList;
		while (temp->nextFCB)
			temp = temp->nextFCB;
		temp->nextFCB = fcb;
		return fileDescriptor;
	}
	return -ENFILE;
}

long DoMkDir(unsigned char *s)
{
	unsigned char *S = NameToFullPath(s);
	struct Message *msg = ALLOCMSG;
	msg->nextMessage = 0;
	msg->byte = CREATEDIR;
	msg->quad = (long) S;
	SendReceiveMessage(FSPort, msg);
	DeallocMem(S);
	long retval = msg->quad;
	DeallocMem(msg);
}

long DoDelete(unsigned char *name)
{
	int retval = 0;
	unsigned char *S = NameToFullPath(name);
	struct Message *msg = ALLOCMSG;

	msg->nextMessage = 0;
	msg->byte = DELETEFILE;
	msg->quad = (long) S;
	SendReceiveMessage(FSPort, msg);
	DeallocMem(S);
	DeallocMem(msg);
	return retval;
}

long DoChDir(unsigned char *dirName)
{
	int retval = -ENOENT;

	unsigned char *S = NameToFullPath(dirName);
	struct Message *msg = ALLOCMSG;

	msg->nextMessage = 0;
	msg->byte = TESTFILE;
	msg->quad = (long) S;
	SendReceiveMessage(FSPort, msg);
	if (msg->quad)
	{
		DeallocMem(currentTask->currentDirName);
		currentTask->currentDirName = AllocKMem(strlen(S) + 1);
		strcpy(currentTask->currentDirName, S);
		retval = 0;
	}
	DeallocMem(S);
	DeallocMem(msg);
	return retval;
}

unsigned char *DoGetcwd(char *name, long length)
{
	if (length < strlen(currentTask->currentDirName))
		return (unsigned char *) -ERANGE;
	strcpy(name, currentTask->currentDirName);
	return name;
}

int DoSeek(FD fileDescriptor, int offset, int whence)
{
	struct FCB *temp = fdToFCB(fileDescriptor);
	if (temp)
	{
		if (temp->deviceType == CONS)
		{
			return -EINVAL;
		}
		else
		{
			struct Message *FSMsg;

			FSMsg = ALLOCMSG;

			FSMsg->nextMessage = 0;
			FSMsg->byte = SEEK;
			FSMsg->quad = (long) temp;
			FSMsg->quad2 = (long) offset;
			FSMsg->quad3 = (long) whence;
			SendReceiveMessage(FSPort, FSMsg);
			long retval = FSMsg->quad;
			DeallocMem(FSMsg);
			return retval;
		}
	}
	return -EBADF;
}

int DoTruncate(FD fileDescriptor, long length)
{
	struct FCB *temp = fdToFCB(fileDescriptor);
	if (temp)
	{
		if (temp->deviceType == CONS)
		{
			return -EINVAL;
		}
		else
		{
			struct Message *FSMsg;

			FSMsg = ALLOCMSG;

			FSMsg->nextMessage = 0;
			FSMsg->byte = TRUNCATE;
			FSMsg->quad = (long) temp;
			FSMsg->quad2 = (long) length;
			SendReceiveMessage(FSPort, FSMsg);
			long retval = FSMsg->quad;
			DeallocMem(FSMsg);
			return retval;
		}
	}
	return -EBADF;
}

unsigned char *NameToFullPath(unsigned char *name)
{
	char *S = AllocKMem(strlen(name) + strlen(currentTask->currentDirName) + 3);

	// "." is a very special case. It changes nothing.
	if (!strcmp(name, "."))
	{
		strcpy(S, currentTask->currentDirName);
		return S;
	}
	// The special case ".."
	if (!strcmp(name, ".."))
	{
		if (!strcmp(currentTask->currentDirName, "/"))
		{
			strcpy(S, "/");
			return S;
		}
		else
		{
			strcpy(S, currentTask->currentDirName);
			while (S[strlen(S) - 1] != '/')
				S[strlen(S) - 1] = 0;
			S[strlen(S) - 1] = 0;
			if (strlen(S) == 0)
				S[0] = '/';
		}
	}
	else
	{
		// The special case "./xxx"
		if (name[0] == '.' && name[1] == '/')
			name += 2;
		// The special case "../xxx"
		if (name[0] == '.' && name[1] == '.' && name[2] == '/')
		{
			name += 3;
			strcpy(S, currentTask->currentDirName);
			while (S[strlen(S) - 1] != '/')
				S[strlen(S) - 1] = 0;
			strcat(S, name);
		}
		else
		{
			strcpy(S, "");
			if (!strcmp(name, "/"))
				strcpy(S, "/");
			else
			{
				if (name[0] == '/')
					strcpy(S, name);
				else
				{
					if (strcmp(currentTask->currentDirName, "/"))
						strcpy(S, currentTask->currentDirName);
					strcat(S, "/");
					strcat(S, name);
				}
			}
		}
	}
	return S;
}

#include <stdarg.h>

int kprintf(int row, int column, unsigned char *s, ...)
{
	va_list ap;
	va_start(ap, s);

	unsigned char sprocessed[256];
	int i = 0;
	int j = 0;
	int k = 0;
	short minwidth = 0;
	short zeropadding = 0;
	short indicator = 0;

	while (s[i])
	{
		minwidth = 0;
		zeropadding = 0;
		indicator = 0;
		k = 0;

		if (s[i] != '%')
		{
			sprocessed[j] = s[i];
			i++;
			j++;
		}
		else
		{
			i++;
			if (s[i] == '#')
			{
				indicator = 1;
				i++;
			}
			if (s[i] == '0')
			{
				zeropadding = 1;
				i++;
			}
			if ('0' < s[i] && s[i] <= '9') // A width specifier
			{
				minwidth = s[i] - '0';
				i++;
			}
			switch (s[i])
			{
			case 'c':
				while (minwidth-- > 1)
					sprocessed[j++] = ' ';
				int c = va_arg(ap, int);
				sprocessed[j++] = c;
				break;
			case 's':
				;
				unsigned char *s1 = va_arg(ap, unsigned char *);
				while (minwidth-- > strlen(s1))
					sprocessed[j++] = ' ';
				while (s1[k])
					sprocessed[j++] = s1[k++];
				break;
			case 'd':
				;
				char buffer[20];
				int number = va_arg(ap, int);
				intToAsc(number, buffer, 20);
				for (k = 0; k < 20; k++)
					if (20 - k > minwidth)
					{
						if (buffer[k] != ' ')
							sprocessed[j++] = buffer[k];
					}
					else
					{
						if (zeropadding && buffer[k] == ' ')
							buffer[k] = '0';
						sprocessed[j++] = buffer[k];
					}
				break;
			case 'x':
				;
				number = va_arg(ap, int);
				intToHAsc(number, buffer, 20);
				if (indicator)
				{
					sprocessed[j++] = '0';
					sprocessed[j++] = 'x';
				}
				for (k = 0; k < 20; k++)
					if (20 - k > minwidth)
					{
						if (buffer[k] != ' ')
							sprocessed[j++] = buffer[k];
					}
					else
					{
						if (zeropadding && buffer[k] == ' ')
							buffer[k] = '0';
						sprocessed[j++] = buffer[k];
					}
				break;
			default:
				break;
			}
			i++;
		}
	}
	va_end(ap);
	KWriteString(sprocessed, row, column);
	return 0;
}

char *strrchr(char *string, char delimiter)
{
	int n = strlen(string);
	int i;
	for (i = n - 1; i >= 0; i--)
		if (string[i] == delimiter)
			break;
	return string + i;
}

memset(char *address, char value, int len)
{
	int i;
	for (i = 0; i < len; i++)
		address[i] = value;
}

memcpy(char *dest, char *source, int len)
{
	int i;
	for (i = 0; i < len; i++)
		dest[i] = source[i];
}

long strspn(char *s1, char *s2)
{
	long ret = 0;
	while (*s1 && strchr(s2, *s1++))
		ret++;
	return ret;
}

long strcspn(char *s1, char *s2)
{
	long ret = 0;
	while (*s1)
		if (strchr(s2, *s1))
			return ret;
		else
			s1++, ret++;
	return ret;
}
char *strtok(char * str, char * delim)
{
	static char* p = 0;
	if (str)
		p = str;
	else if (!p)
		return 0;
	str = p + strspn(p, delim);
	p = str + strcspn(str, delim);
	if (p == str)
		return p = 0;
	p = *p ? *p = 0, p + 1 : 0;
	return str;
}

//==================================================================
// Finds first occurrence of c in string.
// Returns pointer to that occurrence or 0 if not found
//==================================================================
unsigned char *strchr(unsigned char *string, unsigned char c)
{
	long i = 0;
	while (string[i])
	{
		if (string[i] == c)
			return string + i;
		i++;
	}
	return 0;
}

//================================================================
// Returns the length of string
//================================================================
long strlen(unsigned char *string)
{
	int i = 0;
	while (string[i++])
		;
	return i - 1;
}

//===============================================================
// Compares s1 and s2 up to length characters.
// Returns 0 if they are equal, non-zero otherwise.
//===============================================================
long strncmp(unsigned char *s1, unsigned char *s2, long length)
{
	long count;
	short done = 0;

	for (count = 0; count < length; count++)
	{
		if (s1[count] != s2[count])
		{
			done = 1;
			break;
		}
	}
	if (done)
		return (1);
	else
		return (0);
}

//=======================================================
// Copy null-terminates string s1 to s2
//=======================================================
unsigned char *strcpy(unsigned char *destination, unsigned char *source)
{
	unsigned char *retval = destination;
	while (*source)
	{
		*destination++ = *source++;
	}
	*destination = 0;
	return retval;
}

//===============================================================
// Compares s1 and s2.
// Returns 0 if they are equal, non-zero otherwise.
//===============================================================
long strcmp(unsigned char *s1, unsigned char *s2)
{
	long retval = 1;
	int count = 0;
	while (s1[count] == s2[count])
	{
		if (s1[count++] == 0)
			retval = 0;
	}
	return retval;
}

//===========================================================
// Concatenates s2 to the end of s1.
// Returns s1
//===========================================================
unsigned char *strcat(unsigned char *s1, unsigned char *s2)
{
	int n = strlen(s1);
	strcpy(s1 + n, s2);
	return s1;
}

int intToAsc(int i, char *buffer, int len)
{
	int count;

	for (count = 0; count < len; count++)
		buffer[count] = ' ';
	count = len - 1;
	do
	{
		buffer[count] = '0' + i % 10;
		i = i / 10;
		count--;
	} while (i > 0);
	return 0;
}

int intToHAsc(int i, char *buffer, int len)
{
	int count;

	for (count = 0; count < len; count++)
		buffer[count] = ' ';
	count = len - 1;
	do
	{
		int mod;
		if (i % 16 < 10)
			buffer[count] = '0' + i % 16;
		else
			buffer[count] = '0' + 7 + i % 16;
		i = i / 16;
		count--;
	} while (i > 0);
	return 0;
}

