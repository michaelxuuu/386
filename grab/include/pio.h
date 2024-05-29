void outb(uint8_t byte, uint16_t port);
uint8_t inb(uint16_t port);
uint32_t inl(uint16_t port);
void outl(uint32_t dword, uint16_t port);
void insl(uint16_t port, void *dest, int cnt);
void outsl(void *src, uint16_t port, int cnt);
