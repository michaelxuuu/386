#include <types.h>
#include <pio.h>
#include <printf.h>
#include <pci.h>
#include <assert.h>
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
#define PORT_ADDR 0xcf8
#define PORT_DATA 0xcfc
//
// The address to be put into the address port has the fields shown below:
// 
// Bit 31	  |Bits 30-24|Bits 23-16  |Bits 15-11	 |Bits 10-8	     |Bits 7-0
// -----------+----------+------------+--------------+---------------+------------------
// Enable Bit |Reserved	 |Bus Number  |Device Number |Function Number|Register Offset
// where the register offset has to be dword aligned and thus its bits 1:0 are always 0
//
#define mkaddr(enable, bus, dev, func, off) \
    ((((enable) & 1) << 31) | (((bus) & 0xff) << 16) | \
    (((dev) & 0x1f) << 11) | (((func) & 7) << 8) | (off))
#define readbar(enable, bus, dev, func, bar_num) \
    (outl(mkaddr(enable, bus, dev, func, (bar_num << 2)), PORT_ADDR), inl(PORT_DATA))
#define writebar(enable, bus, dev, func, bar_num, value) \
    (outl(mkaddr(enable, bus, dev, func, (bar_num << 2)), PORT_ADDR), outl(value, PORT_DATA))
#define NDEV_PER_BUS 32
#define NFUN_PER_DEV 8
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
// PCI class codes
#define PCI_CLASS_STORAGE 0x1
#define PCI_CLASS_BRIDGE 0x4
// PCI subclass codes
#define PCI_SUBCLASS_IDECONTROLLER 0x1
#define PCI_SUBCLASS_PCI2PCI 0x6
// Vendor ID 0xffff indicates device doesn't exist
#define NULLVENDOR 0xffff
// Read configuration registers from the *shared* configuration space
#define readvendorid(bus, dev, func) ((uint16_t)readbar(1, bus, dev, func, 0))
#define readdeviceid(bus, dev, func) ((uint16_t)(readbar(1, bus, dev, func, 0) >> 16))
#define readclass(bus, dev, func) ((uint8_t)(readbar(1, bus, dev, func, 2) >> 24))
#define readsubclass(bus, dev, func) ((uint8_t)(readbar(1, bus, dev, func, 2) >> 16))
#define readprogif(bus, dev, func) ((uint8_t)(readbar(1, bus, dev, func, 2) >> 8))
#define readhdrtype(bus, dev, func) ((uint8_t)(readbar(1, bus, dev, func, 3) >> 16))
// Read configuration registers from the pci2pci-*specific* configuration space
// https://wiki.osdev.org/PCI#Configuration_Space
#define readsecondarybus(bus, dev, func) ((uint8_t)(readbar(1, bus, dev, func, 6) >> 8))

// Support up to 10 pci devices
#define NPCIDEV 10
struct pcidev {
    // Where to find cc registers of this device
    int bus;
    int dev;
    int fun;
    // Buffered cc registers
    uint16_t vendorid;
    uint16_t deviceid;
    uint8_t class;
    uint8_t subclass;
    uint8_t progif;
    uint8_t _;
};

static int npcidev = 0;
static struct pcidev pcidevs[NPCIDEV];

// 
// Scan pci buses and list add PCI devices detected
// This scan is recursive cus PCI devices can from a tree structure and have secondary buses
// as in the case of having a PCI to PCI bridge.
// 
void pci_scan(uint8_t bus)
{
    printf("%8s%8s%8s%16s\n", "bus", "dev", "fun", "vendor:device");
    for (int dev = 0; dev < NDEV_PER_BUS; dev++) {
        // Device doesn't exist if vendor id read from
        // the configuration space of its function 0 is 0xffff
        uint16_t vendor = readvendorid(bus, dev, 0);
        if (vendor == NULLVENDOR)
             continue;
        uint16_t deviceid = readdeviceid(bus, dev, 0);
        // Test the multi-function bit in function 0's header,
        // and we don't bother checking other functions than function 0
        // if not set.
        int nfun = (readhdrtype(bus, dev, 0) & (1 << 7)) ? NFUN_PER_DEV : 1;
        for (int fun = 0; fun < nfun; fun++) {
            if (readvendorid(bus, dev, fun) == NULLVENDOR)
                continue;
            // Check if this function is PCI to PCI bridge,
            // and if it is we need to retrieve the secondary PCI bus ID
            // from it. This is where the optimization comes in - you scan as many
            // bus as there are rather than "brutal force" scan over all 256 possible buses!
            uint8_t class = readclass(bus, dev, fun);
            uint8_t subclass = readsubclass(bus, dev, fun);
            uint8_t progif = readprogif(bus, dev, fun);
            pcidevs[npcidev].bus = bus;
            pcidevs[npcidev].dev = dev;
            pcidevs[npcidev].fun = fun;
            pcidevs[npcidev].vendorid = vendor;
            pcidevs[npcidev].deviceid = deviceid;
            pcidevs[npcidev].class = class;
            pcidevs[npcidev].subclass = subclass;
            pcidevs[npcidev].progif = progif;
            npcidev++;
            if (fun) deviceid = readdeviceid(bus, dev, fun);
            printf("%8d%8d%8d%x:%x\n", bus, dev, fun, vendor, deviceid);
            if (class == PCI_CLASS_BRIDGE &&
                subclass == PCI_SUBCLASS_PCI2PCI) {
                // PCI to PCI bridge function detected. Start recursion!
                uint8_t secondarybus = readsecondarybus(bus, dev, fun);
                pci_scan(secondarybus);
            }
        }
    }
}

int pci_get_dev(uint8_t class, uint8_t subclass, uint8_t progif) {
    for (int i = 0; i < npcidev; i++) {
        struct pcidev* p = &pcidevs[i];
        if (class == p->class && subclass == p->subclass && 
            progif == p->progif)
            return i;
    }
    return -1;
}

void pci_write(int dev, int bar, uint32_t data) {
    assert(dev < npcidev);
    struct pcidev* p = &pcidevs[dev];
    writebar(1, p->bus, p->dev, p->fun, bar, data);
}

uint32_t pci_read(int dev, int bar) {
    assert(dev < npcidev);
    struct pcidev* p = &pcidevs[dev];
    return readbar(1, p->bus, p->dev, p->fun, bar);
}
