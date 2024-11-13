extern void __libc_init_array(void);

extern int main();


void entry_main(void) {
    __libc_init_array();
    main();
}


// eof