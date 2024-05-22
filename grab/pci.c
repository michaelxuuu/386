#include <types.h>
#include <pio.h>
#include <printf.h>
//
// Concepts:
// PCI device, PCI controller, port mapped i/o
//
// CPU <-in/out-> PCI controller  V PCI lanes
//                                |
//                                | PCI device 0        
//                                | PCI device 1
//                                | PCI device 2
//                                | ...
//                                | PCI device 31 [why 31? look at the device number of the address format below]
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
    (((dev) & 0x1f) << 11) | (((func) << 8) & 3) | (off))
#define r_dword(enable, bus, dev, func, dwoff) \
    (outl(mkaddr(enable, bus, dev, func, (dwoff << 2)), PORT_ADDR), inl(PORT_DATA))
#define r_word(enable, bus, dev, func, woff) \
    ((r_dword(enable, bus, dev, func, (woff >> 1)) >> ((woff & 1) * 16)) & 0xffff)
#define r_byte(enable, bus, dev, func, boff) \
    ((r_dword(enable, bus, dev, func, (boff >> 2)) >> ((boff & 3) * 8)) & 0xff)
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
// 0x3	    | 0xC	    |   BIST Header | type	        | Latency Timer | Cache Line Size
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
#define r_hdr_deviceid(bus, dev, func) r_word(1, bus, dev, func, 1)
#define r_hdr_vendorid(bus, dev, func) r_word(1, bus, dev, func, 0)
#define r_hdr_type(bus, dev, func) r_byte(1, bus, dev, func, 14)
#define r_hdr_class(bus, dev, func) r_byte(1, bus, dev, func, 11)
#define r_hdr_subclass(bus, dev, func) r_byte(1, bus, dev, func, 10)
#define r_hdr_progif(bus, dev, func) r_byte(1, bus, dev, func, 9)
// Read configuration registers from the pci2pci-*specific* configuration space
// https://wiki.osdev.org/PCI#Configuration_Space
#define r_hdr_pci2pci_secondarybus(bus, dev, func) r_byte(1, bus, dev, func, 25)

void printdev(uint8_t class, uint8_t subclass, uint8_t progif);

// Scan pci buses and list add PCI devices detected
// This scan is recursive cus PCI devices can from a tree structure and have secondary buses
// as in the case of having a PCI to PCI bridge.
//
void scan(uint8_t bus)
{
// Check devices on this bus
    for (int dev = 0; dev < NDEV_PER_BUS; dev++) {
    // Check functions of this device
        // Test the multi-function bit in function 0's header,
        // and we don't bother checking other functions than function 0
        // if not set.
        int nfun = (r_hdr_type(bus, dev, 0) & (1 << 7)) ? NFUN_PER_DEV : 1;
        for (int fun = 0; fun < nfun; fun++) {
            uint16_t vendor = r_hdr_vendorid(bus, dev, fun);
            uint16_t deviceid = r_hdr_deviceid(bus, dev, fun);
            // Device doesn't exist if vendor id read from
            // the configuration space of its function 0 is 0xffff
            if (vendor == NULLVENDOR && !fun)
                break;
            if (vendor == NULLVENDOR)
                continue;
            // Check if this function is PCI to PCI bridge,
            // and if it is we need to retrieve the secondary PCI bus ID
            // from it. This is where the optimization comes in - you scan as many
            // bus as there are rather than "brutal force" scan over all 256 possible buses!
            uint8_t class = r_hdr_class(bus, dev, fun);
            uint8_t subclass = r_hdr_subclass(bus, dev, fun);
            uint8_t progif = r_hdr_progif(bus, dev, fun);
            printf("device id:%x vendor:%x class:%x subclass:%x\n", deviceid, vendor, class, subclass);
            if (class == PCI_CLASS_STORAGE &&
                subclass == PCI_SUBCLASS_PCI2PCI) {
                // PCI to PCI bridge function detected. Start recursion!
                uint8_t secondarybus = r_hdr_pci2pci_secondarybus(bus, dev, fun);
                scan(secondarybus);
            }
        }
    }
}

void printdev(uint8_t class, uint8_t subclass, uint8_t progif) {
    if (class == PCI_CLASS_STORAGE) {
        
    }
}
