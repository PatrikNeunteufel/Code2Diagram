/* Zwei Kerne (Beispiel fuer das Sequenzdiagramm "Start der Kerne"): CPU1 startet CPU2, beide warten an zwei
 * Barrieren aufeinander, im Lauf meldet CPU1 eine neue Messung mit FLAG 2, CPU2 antwortet mit FLAG 1. */
#include "gemeinsam.h"

/* Wie IPC_sync, aber mit Obergrenze: 1 = beide synchron, 0 = Zeit abgelaufen. */
static unsigned short SYNC_BEGRENZT(unsigned long Flag)
{
    unsigned long warte;

    IPC_setFlagLtoR(IPC_CPU1_L_CPU2_R, Flag);
    for (warte = 0UL; (warte < GRENZE) && (IPC_isFlagBusyRtoL(IPC_CPU1_L_CPU2_R, Flag) == false); warte++)
    {
    }
    if (warte >= GRENZE) {
        return 0U;
    }
    IPC_ackFlagRtoL(IPC_CPU1_L_CPU2_R, Flag);
    return 1U;
}

/* CPU2 hat gefiltert (FLAG 1) */
__interrupt void cpu1_ipc_isr(void)
{
    IPC_ackFlagRtoL(IPC_CPU1_L_CPU2_R, IPC_FLAG1);
    werte_aus();
}

void starte_cpu2(void)
{
    IPC_setBootMode(IPC_CPU1_L_CPU2_R, BOOT_KEY | BOOTMODE_FLASH);
    IPC_setFlagLtoR(IPC_CPU1_L_CPU2_R, IPC_FLAG0);
}

void lade_parameter(void)
{
    lies_eeprom();
    pruefe_parameter();
}

void main(void)
{
    volatile unsigned long *vector;

    Device_init();
    init_gemeinsam();
    IPC_clearFlagLtoR(IPC_CPU1_L_CPU2_R, IPC_FLAG_ALL);
    starte_cpu2();
    lade_parameter();
    if (SYNC_BEGRENZT(IPC_FLAG31) == 0U) {
        melde_fehler();
    }
    vector = (volatile unsigned long *)(PIEVECTTABLE_BASE + ((INT_CIPC1 >> 16U) * 2U));
    *vector = (unsigned long)(&cpu1_ipc_isr);
    IPC_sync(IPC_CPU1_L_CPU2_R, IPC_FLAG30);
    for (;;) {
        if (neue_messung()) {
            IPC_setFlagLtoR(IPC_CPU1_L_CPU2_R, IPC_FLAG2);
        }
        hintergrund();
    }
}
