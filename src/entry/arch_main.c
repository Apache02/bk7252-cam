extern int main();

unsigned int ulCriticalNesting = 0;
unsigned int pxCurrentTCB = 0;

void entry_main(void) {
    main();
}

void bk_trap_udef() {
    while (1);
}

void bk_trap_pabt() {
    while (1);
}

void bk_trap_dabt() {
    while (1);
}

void bk_trap_resv() {
    while (1);
}

void intc_irq(void) {}

void intc_fiq(void) {}

extern void vTaskSwitchContext(void) {}


// eof