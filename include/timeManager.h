// NTP Parameters
const char *NTP_SERVER = "ch.pool.ntp.org";
const char *TZ_INFO = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"; // enter your time zone (https://remotemonitoringsystems.ca/time-zone-abbreviations.php)
tm timeinfo;
time_t now;
long unsigned lastNTPtime;
unsigned long lastEntryTime;
int hours = 0;           //To manage hours
int displayedHours=0;     // To manage hours on pixels 
int minutes = 0;         //To manage minute
int previousMinutes = 0; //Store previous minutes
int unit_minutes = 0;    //Store unit

void getTime()
{
    time(&now);
    localtime_r(&now, &timeinfo);

    hours = timeinfo.tm_hour;
    displayedHours=timeinfo.tm_hour;
    minutes = timeinfo.tm_min;
    /*-- If hour is after half past --*/
    if (minutes >= 35 && minutes <= 59)
    {
        displayedHours = displayedHours + 1;
    }
}
