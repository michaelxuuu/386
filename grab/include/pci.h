void pci_scan(uint8_t bus);
uint32_t pci_read_dev(int dev, int bar, uint32_t data);
void pci_write_dev(int dev, int bar, uint32_t data);
int pci_get_dev(uint8_t class, uint8_t subclass, uint8_t progif);
