#ifndef ITM_H
#define ITM_H

#define SWO_SPEED 2000000

void ITM_Init(void);

/* Overrides weak _write in printf */
int _write(int file, char *ptr, int len);


#endif /* ITM_H */
