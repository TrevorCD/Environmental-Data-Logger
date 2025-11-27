#include <stdint.h>
#include "stm32f4xx.h"
#include "itm.h"

/* Check if ITM is enabled and ready */
static inline int ITM_Ready(void)
{
    return (ITM->TCR & ITM_TCR_ITMENA_Msk) &&
		(ITM->TER & (1UL << 0));
}

/* Overrides weak _write() */
int _write(int file, char *ptr, int len)
{
    (void)file; // unused

	if (!ITM_Ready())
		return 0;   // ITM not ready -> output nothing
	
    for (int i = 0; i < len; i++)
		{
			// sends char on ITM->Port[0]
			ITM_SendChar(ptr[i]);
		}
    return len;
}

/* Configures ITM registers */
void ITM_Init(void)
{
	/* Enable Trace */
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	/* Enable ITM */
	ITM->TCR = ITM_TCR_ITMENA_Msk | ITM_TCR_SWOENA_Msk;
	/* Enable port 0 */
	ITM->TER = 1;
	/* Configure TPIU Prescalar */
	TPI->ACPR = (SystemCoreClock / SWO_SPEED) - 1; // e.g. 168MHz/2MHz - 1 = 83
	/* Set protocol to UART */
	TPI->SPPR = 0x2U;	
}
