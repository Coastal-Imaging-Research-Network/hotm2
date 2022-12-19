
/* defines for alarm processing */
#define ALARM_FATAL 1
#define ALARM_WARNING 0
#define DEFAULT_HANDLER 0

void clearAlarm(void);
void setAlarm( int, void *, int, char *, char * );

