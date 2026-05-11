extern void __libc_init_array(void);

extern void __libc_fini_array(void);

extern int main();


void _start(void) {
    __libc_init_array();
    main();
    __libc_fini_array();
}


// eof