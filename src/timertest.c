#include <exec/types.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/timer.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <clib/dos_protos.h>

#include <stdio.h>

void delete_timer(struct timerequest *);
LONG time_delay(struct timeval *, LONG);
struct timerequest *create_timer(ULONG);
void wait_for_timer(struct timerequest *, struct timeval *);

struct Library *TimerBase; /* to get at the time comparison functions */

void main(int argc, char **argv)
{
//    struct timerequest *tr; /* IO block for timer commands */
    struct timeval currentval;

    printf("Before 1/2 second delay\n");
    /* sleep for 500,000 micro-seconds = 1/2 second */
    currentval.tv_secs = 0;
    currentval.tv_micro = 500000;
    time_delay(&currentval, UNIT_MICROHZ);
    printf("After 1/2 second delay\n");
}

struct timerequest *create_timer(ULONG unit)
{
    /* return a pointer to a timer request.  If any problem, return NULL */
    LONG error;
    struct MsgPort *timerport;
    struct timerequest *TimerIO;

    timerport = CreatePort(0, 0);
    if (timerport == NULL)
        return (NULL);

    TimerIO = (struct timerequest *)
        CreateExtIO(timerport, sizeof(struct timerequest));
    if (TimerIO == NULL)
    {
        DeletePort(timerport); /* Delete message port */
        return (NULL);
    }

    error = OpenDevice(TIMERNAME, unit, (struct IORequest *)TimerIO, 0L);
    if (error != 0)
    {
        delete_timer(TimerIO);
        return (NULL);
    }
    return (TimerIO);
}

LONG time_delay(struct timeval *tv, LONG unit)
{
    struct timerequest *tr;
    /* get a pointer to an initialized timer request block */
    tr = create_timer(unit);

    /* any nonzero return says timedelay routine didn't work. */
    if (tr == NULL)
        return (-1L);

    wait_for_timer(tr, tv);

    /* deallocate temporary structures */
    delete_timer(tr);
    return (0L);
}

void wait_for_timer(struct timerequest *tr, struct timeval *tv)
{

    tr->tr_node.io_Command = TR_ADDREQUEST; /* add a new timer request */

    /* structure assignment */
    tr->tr_time = *tv;

    /* post request to the timer -- will go to sleep till done */
    DoIO((struct IORequest *)tr);
}

void delete_timer(struct timerequest *tr)
{
    struct MsgPort *tp;

    if (tr != 0)
    {
        tp = tr->tr_node.io_Message.mn_ReplyPort;

        if (tp != 0)
            DeletePort(tp);

        CloseDevice((struct IORequest *)tr);
        DeleteExtIO((struct IORequest *)tr);
    }
}