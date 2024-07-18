// PCI API
typedef int pcidev_t;

// *Shared* configuration space, taking up first 16 bytes of
// of the 256 byte configuratin space as opposed to the
// device-*specific* configuration space that takes up the
// remaining 256-16=240 bytes
struct pcihdr {
        // 1st bar
        uint16_t vendorid;
        uint16_t deviceid;
        // 2nd bar
        uint16_t command;
        uint16_t status;
        // 3nd bar
        uint8_t revisonid;
        uint8_t progif;
        uint8_t subclass;
        uint8_t class;
        // 4th bar
        uint8_t cachelinesz;
        uint8_t latencytimer;
        uint8_t headertype;
        uint8_t bist;
};

void pci_prob_dev();

uint32_t pci_read_dword(pcidev_t dev, int dwoff);

void pci_write_dword(pcidev_t dev, int dwoff, uint32_t data);

uint16_t pci_read_word(pcidev_t dev, int woff);

void pci_write_word(pcidev_t dev, int woff, uint16_t data);

uint8_t pci_read_byte(pcidev_t dev, int boff);

void pci_write_byte(pcidev_t dev, int boff, uint8_t data);

void pci_read_hdr(pcidev_t dev, struct pcihdr *hdr);

pcidev_t pci_get_dev(uint16_t vendorid, uint16_t deviceid);

void pci_list();
