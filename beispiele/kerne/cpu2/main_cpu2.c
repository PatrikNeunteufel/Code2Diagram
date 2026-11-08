/* CPU2: wartet auf CPU1, filtert jede neue Messung (FLAG 2) und meldet das Ergebnis (FLAG 1). */
#include "gemeinsam.h"

__interrupt void cpu2_ipc_isr(void)
{
    IPC_ackFlagRtoL(IPC_CPU2_L_CPU1_R, IPC_FLAG2);
    filtere();
    IPC_setFlagLtoR(IPC_CPU2_L_CPU1_R, IPC_FLAG1);
}

void main(void)
{
    Device_init();
    init_gemeinsam();
    IPC_sync(IPC_CPU2_L_CPU1_R, IPC_FLAG31);
    loesche_ram();
    IPC_registerInterrupt(IPC_CPU2_L_CPU1_R, IPC_INT2, cpu2_ipc_isr);
    IPC_sync(IPC_CPU2_L_CPU1_R, IPC_FLAG30);
    for (;;) {
        hintergrund();
    }
}
