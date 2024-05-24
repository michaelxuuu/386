//
// Concepts:
// PCI device, PCI controller, port mapped i/o
//
// CPU <-in/out-> 
// system bus <-> 
// host bridge (i/o controller) <-> 
// PCI controller <-> PCI lanes
//                          | PCI device 0        
//                          | PCI device 1
//                          | PCI device 2
//                          | ...
//                          | PCI device 31 [why 31? look at the device number of the address format below]
//
// PCI controller is assigned 2 i/o ports - 0xcf8 for its address register and 0xcfc for its data register.
// CPU communicates with it through accessing these two i/o ports using in/out instructions.
// Using only two ports *is* sufficient for the CPU to program the PCI devices  - CPU first writes to the address
// register to specify which device it wants to talk to and depending on whether it's a read or write operation, 
// it goes on read or write to the data register.
//
//
// The address to be put into the address port has the fields shown below:
// 
// Bit 31	  |Bits 30-24|Bits 23-16  |Bits 15-11	 |Bits 10-8	     |Bits 7-0
// -----------+----------+------------+--------------+---------------+------------------
// Enable Bit |Reserved	 |Bus Number  |Device Number |Function Number|Register Offset
// where the register offset has to be dword aligned and thus its bits 1:0 are always 0
//
// 
// Any PCI compliant device must provide a 256 byte configuration space
// that *shares* the below header and the above address can be used to access
// any dword of the configuration space
//
// Register	| Offset    |	Bits 31-24	| Bits 23-16    | Bits 15-8	    | Bits 7-0
// ---------+-----------+---------------+---------------+---------------+-------------
// 0x0	    | 0x0	    |   Device  ID	                | Vendor ID     
// 0x1	    | 0x4	    |   Status                      | Command       
// 0x2	    | 0x8	    |   Class code	| Subclass	    | Prog IF	    | Revision ID
// 0x3	    | 0xC	    |   BIST        | Header type	| Latency Timer | Cache Line Size
//
// Header Type identifies the layout of the rest of the header beginning at byte 0x10 of the header. 
// If bit 7 of this register is set, the device has multiple functions; otherwise, 
// it is a single function device.
//

#include <types.h>
#include <pio.h>
#include <printf.h>
#include <pci.h>
#include <assert.h>

#define PORT_ADDR 0xcf8
#define PORT_DATA 0xcfc

#define NDEV_PER_BUS 32
#define NFUN_PER_DEV 8

// PCI class codes
#define PCI_CLASS_BRIDGE 0x4
#define PCI_CLASS_STORAGE 0x1
// PCI subclass codes
#define PCI_SUBCLASS_PCI2PCI 0x6
#define PCI_SUBCLASS_IDECONTROLLER 0x1
// Vendor ID 0xffff indicates device doesn't exist
#define NULLVENDOR 0xffff

// Support up to 10 pci devices
#define NPCIDEV 10

struct pcidev {
    // Device location
    int bus;
    int dev;
    int fun;
    // Buffered configruation space header
    struct pcihdr hdr;
};

static int npcidev = 0;
static struct pcidev pcidevs[NPCIDEV];

static inline uint32_t mkaddr(int enable, int bus, int dev, int func, int off) {
    return (((enable & 1) << 31) | ((bus & 0xff) << 16) | \
        ((dev & 0x1f) << 11) | ((func & 7) << 8) | off);
}

static uint32_t _pci_read_dword(int bus, int dev, int func, int dwoff) {
    outl(mkaddr(1, bus, dev, func, (dwoff << 2)), PORT_ADDR);
    return inl(PORT_DATA);
}

static void _pci_write_dword(int bus, int dev, int func, int dwoff, uint32_t data) {
    outl(mkaddr(1, bus, dev, func, (dwoff << 2)), PORT_ADDR);
    outl(data, PORT_DATA);
}

static uint16_t _pci_read_word(int bus, int dev, int func, int woff) {
    return (_pci_read_dword(bus, dev, func, woff/2) >> (16 * (woff%2)));
}

static void _pci_write_word(int bus, int dev, int func, int woff, uint16_t data) {
    uint32_t dw = _pci_read_dword(bus, dev, func, woff/2);
    dw = (dw & (0xffff << (16 * (woff%2)))) | (data << (16 * (woff%2)));
    _pci_write_dword(bus, dev, func, woff/2, dw);
}

static uint8_t _pci_read_byte(int bus, int dev, int func, int boff) {
    return (_pci_read_dword(bus, dev, func, boff/4) >> (8 * (boff%4)));
}

static void _pci_write_byte(int bus, int dev, int func, int boff, uint8_t data) {
    uint32_t dw = _pci_read_dword(bus, dev, func, boff/4);
    dw = (dw & (0xff << (8 * (boff%4)))) | (data << (16 * (boff%4)));
    _pci_write_dword(bus, dev, func, boff/4, dw);
}

// 
// Scan pci buses and list add PCI devices detected
// This scan is recursive cus PCI devices can from a tree structure and have secondary buses
// as in the case of having a PCI to PCI bridge.
// 
void _pci_prob_dev(uint8_t bus)
{
    for (int dev = 0; dev < NDEV_PER_BUS; dev++) {
        // Device doesn't exist if vendor id read from
        // the configuration space of its function 0 is 0xffff
        if (_pci_read_word(bus, dev, 0, 0) == NULLVENDOR)
             continue;
        // Test the multi-function bit in function 0's header,
        // and we don't bother checking other functions than function 0
        // if not set.
        int nfun = (_pci_read_byte(bus, dev, 0, 14) & (1 << 7)) ? NFUN_PER_DEV : 1;
        for (int fun = 0; fun < nfun; fun++) {
            if (_pci_read_word(bus, dev, fun, 0) == NULLVENDOR)
                continue;
            // Check if this function is PCI to PCI bridge,
            // and if it is we need to retrieve the secondary PCI bus ID
            // from it. This is where the optimization comes in - you scan as many
            // bus as there are rather than "brutal force" scan over all 256 possible buses!
            struct pcidev *p = &pcidevs[npcidev++];
            p->bus = bus;
            p->dev = dev;
            p->fun = fun;
            p->hdr.vendorid = _pci_read_word(bus, dev, fun, 0);
            p->hdr.deviceid = _pci_read_word(bus, dev, fun, 1);
            p->hdr.command = 0;
            p->hdr.status = _pci_read_word(bus, dev, fun, 3);
            p->hdr.revisonid = _pci_read_byte(bus, dev, fun, 8);
            p->hdr.progif = _pci_read_byte(bus, dev, fun, 9);
            p->hdr.subclass = _pci_read_byte(bus, dev, fun, 10);
            p->hdr.class = _pci_read_byte(bus, dev, fun, 11);
            p->hdr.cachelinesz = _pci_read_byte(bus, dev, fun, 12);
            p->hdr.latencytimer = _pci_read_byte(bus, dev, fun, 13);
            p->hdr.headertype = _pci_read_byte(bus, dev, fun, 14);
            p->hdr.bist = _pci_read_byte(bus, dev, fun, 15);
            if (p->hdr.class == PCI_CLASS_BRIDGE &&
                p->hdr.subclass == PCI_SUBCLASS_PCI2PCI) {
                // PCI to PCI bridge function detected. Start recursion!
                uint8_t secondarybus = _pci_read_byte(bus, dev, fun, 25);
                _pci_prob_dev(secondarybus);
            }
        }
    }
}

void pci_prob_dev() {
    _pci_prob_dev(0);
}

// If progif passed in is -1, we don't take it into account.
pcidev_t pci_get_dev(uint16_t vendorid, uint16_t deviceid) {
    for (int i = 0; i < npcidev; i++) {
        struct pcidev* p = &pcidevs[i];
        if (vendorid != p->hdr.vendorid)
            continue;
        if (deviceid != p->hdr.deviceid)
            continue;
        return i;
    }
    return -1;
}

uint32_t pci_read_dword(pcidev_t dev, int dwoff) {
    assert(dev < npcidev && pcidevs[dev].hdr.vendorid);
    struct pcidev *p = &pcidevs[dev];
    return _pci_read_dword(p->bus, p->dev, p->fun, dwoff);
}

void pci_write_dword(pcidev_t dev, int dwoff, uint32_t data) {
    assert(dev < npcidev && pcidevs[dev].hdr.vendorid);
    struct pcidev *p = &pcidevs[dev];
    _pci_write_dword(p->bus, p->dev, p->fun, dwoff, data);
}

uint16_t pci_read_word(pcidev_t dev, int woff) {
    assert(dev < npcidev && pcidevs[dev].hdr.vendorid);
    struct pcidev *p = &pcidevs[dev];
    return _pci_read_word(p->bus, p->dev, p->fun, woff);
}

void pci_write_word(pcidev_t dev, int woff, uint16_t data) {
    assert(dev < npcidev && pcidevs[dev].hdr.vendorid);
    struct pcidev *p = &pcidevs[dev];
    _pci_write_word(p->bus, p->dev, p->fun, woff, data);
}

uint8_t pci_read_byte(pcidev_t dev, int boff) {
    assert(dev < npcidev && pcidevs[dev].hdr.vendorid);
    struct pcidev *p = &pcidevs[dev];
    return _pci_read_byte(p->bus, p->dev, p->fun, boff);
}

void pci_write_byte(pcidev_t dev, int boff, uint8_t data) {
    assert(dev < npcidev && pcidevs[dev].hdr.vendorid);
    struct pcidev *p = &pcidevs[dev];
    _pci_write_byte(p->bus, p->dev, p->fun, boff, data);
}

void pci_read_hdr(pcidev_t dev, struct pcihdr *hdr) {
    assert(dev < npcidev && pcidevs[dev].hdr.vendorid);
    *hdr = pcidevs[dev].hdr;
}

void pci_list() {
    printf("%8s%8s%8s%8s%8s%8s%8s\n", "bus", "dev", "fun", "vendor", "device", "class", "subclass");
    for (int i = 0; i < npcidev; i++) {
        struct pcidev *dev = &pcidevs[i];
        printf("%8d%8d%8d%8x%8x%8x%8x\n", 
            dev->bus,
            dev->dev,
            dev->fun,
            dev->hdr.vendorid,
            dev->hdr.deviceid,
            dev->hdr.class,
            dev->hdr.subclass);
    }
}

