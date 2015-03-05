
Select lines of a log file with given time range using binary search.

example:

    logcut -f '2 hours ago' -a /var/log/messages
    logcut -f '5:55' -t '8:30' -i /usr/local/tomcat5/logs/localhost_log.txt
    logcut -f '2/1' -t '2/8' -w /var/log/httpd/access_log

Log records of syslog has no 'year' information.  'logcut' fills up the
year using current time's year.  If month of the log is less than or
equals to the current month, it fills the current your, otherwise it fills
(current year - 1) as the year of the log record.
 
As 'logcut' uses binary search, logs must be sorted.  Log records without
year, years will be filled up but they must be sorted.
