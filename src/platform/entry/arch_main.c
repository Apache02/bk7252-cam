extern void __libc_init_array(void);
extern int main();


void entry_main(void) {
    __libc_init_array();
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

void bk_trap_swi() {
    while (1);
}


// eof