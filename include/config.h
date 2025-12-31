#ifndef CONFIG_H
#define CONFIG_H

/* The interval that vBME680PollTask delays for in between readings, in
   milliseconds. */
#define configBME680_POLL_INTERVAL 5000

/* The file name that vSDCardWriteTask opens/creates and writes the output
   data to. It is best practice to give this file a .csv extension. */
#define configSD_FILE_NAME "data.csv"

#endif
