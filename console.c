#include "memory.h"
#include "kstructs.h"
#include "console.h"

//====================================================
// This is the task that listens for console requests.
//====================================================

short int      column;
short int      row;
char           *VideoBuffer;
struct Message ConsoleMsg;

void scrollscreen()
{
	short int row;
	short int column;
	
	for (row = 1; row < 25; row++)
		for (column = 0; column < 80; column++)
   		VideoBuffer[160 * (row - 1) + 2 * column] = VideoBuffer[160 * row + 2 * column];
	for (column = 0; column < 80; column++)
		      VideoBuffer[160 * 24 + 2 * column] = ' ';
}

void printchar(unsigned char c)
{
   switch (c)
   {
   case 0:
      break;

   case BACKSPACE:
      if (column > 0)
      {
         column--;
      }
      break;

   case CR:
      column = 0;
      row++;
		if (row == 25)
		{
			scrollscreen();
			row--;
		}
      break;

   default:
      VideoBuffer[160 * row + 2 * column] = c;
      column++;
      if (column == 80)
      {
         column = 0;
         row++;
		if (row == 25)
		{
			scrollscreen();
			row--;
		}
      }
   }
}


void consoleTaskCode()
{
   column      = 0;
   row         = 0;
   VideoBuffer = (char *)0xB8000;
   ((struct MessagePort *)ConsolePort)->waitingProc = (struct Task *)-1L;
   ((struct MessagePort *)ConsolePort)->msgQueue    = 0;

   unsigned char *s;

   while (1)
   {
      ReceiveMessage(ConsolePort, &ConsoleMsg);
      switch (ConsoleMsg.byte)
      {
      case WRITECHAR:
         printchar((unsigned char)ConsoleMsg.quad);
         break;

      case WRITESTR:
         s = (unsigned char *)ConsoleMsg.quad;
         while (*s != 0)
         {
            printchar(*s);
            s++;
         }
         DeallocMem((void *)ConsoleMsg.quad);
         break;

      case CLRSCR:
         for (row = 0; row < 25; row++)
         {
            for (column = 0; column < 80; column++)
            {
               VideoBuffer[160 * row + 2 * column]     = ' ';
               VideoBuffer[160 * row + 2 * column + 1] = 7;
            }
         }
         column = 0;
         row    = 0;
         break;

      default:
         break;
      }
   }
}
