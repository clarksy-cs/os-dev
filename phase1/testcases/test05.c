#include <stdio.h>
#include <usloss.h>
#include <phase1.h>
#include <string.h>

int XXp1(char *);
char buf[256];


int start1(char *arg)
{
   int status, kidpid, i, j;
   char buf[20];

   printf("start1(): started\n");

   for (j = 0; j < 2; j++) {
      for (i = 2; i < 5; i++) {
         sprintf(buf, "XXp%d", i);
         printf("start1(): buf = `%s'\n", buf);
         kidpid = fork1(buf, XXp1, buf, USLOSS_MIN_STACK, 3);
         printf("start1(): after fork of child %d\n", kidpid);
      }

      for (i = 2; i < 5; i++) {
         kidpid = join (&status);
         printf("start1(): after join of child %d, status = %d\n",
                kidpid, status);
      }

   }
   return 0;
}

int XXp1(char *arg)
{
   long j;
   long i;
   long loopCount = 0x008fffffff;
   long printAt = 0x000fffffff;

   printf("XXp1(): %s, started, pid = %d\n", arg, getpid());
   if ( strcmp(arg, "XXp3") == 0 ) {
      for (i = 0; i < loopCount; i++)
         if ( i == printAt)
         {
            dump_processes();
         }
   }
   else {
      for (i = 0; i < loopCount; i++)
         j = i;
   }

   printf("XXp1(): exitting, pid = %d\n", getpid());
   quit(-getpid());
   return 0;
}

