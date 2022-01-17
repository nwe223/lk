//
// Copyright (c) 2022 Travis Geiselbrecht
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT
#include "ahci.h"

#include <arch/atomic.h>
#include <dev/bus/pci.h>
#include <kernel/event.h>
#include <kernel/thread.h>
#include <kernel/vm.h>
#include <lk/bits.h>
#include <lk/cpp.h>
#include <lk/err.h>
#include <lk/init.h>
#include <lk/list.h>
#include <lk/trace.h>
#include <platform/interrupts.h>
#include <string.h>
#include <type_traits>

#include "port.h"
#include "ahci_hw.h"

#define LOCAL_TRACE 1

volatile int ahci::global_count_= 0;

ahci::ahci() = default;
ahci::~ahci() = default;

status_t ahci::init_device(pci_location_t loc) {
    char str[32];
    loc_ = loc;

    LTRACEF("pci location %s\n", pci_loc_string(loc_, str));

    pci_bar_t bars[6];
    status_t err = pci_bus_mgr_read_bars(loc_, bars);
    if (err != NO_ERROR) return err;

    LTRACEF("ahci BARS:\n");
    if (LOCAL_TRACE) pci_dump_bars(bars, 6);

    if (!bars[5].valid) {
        return ERR_NOT_FOUND;
    }

    // allocate a unit number
    unit_ = atomic_add(&global_count_, 1);

    // map bar 5, main memory mapped register interface, 4K
    snprintf(str, sizeof(str), "ahci%d abar", unit_);
    err = vmm_alloc_physical(vmm_get_kernel_aspace(), str, bars[5].size, &abar_regs_, 0,
                             bars[5].addr, /* vmm_flags */ 0, ARCH_MMU_FLAG_UNCACHED_DEVICE);
    if (err != NO_ERROR) {
        return ERR_NOT_FOUND;
    }

    LTRACEF("ABAR mapped to %p\n", abar_regs_);

    pci_bus_mgr_enable_device(loc_);

    LTRACEF("CAP %#x\n", read_reg(ahci_reg::CAP));
    LTRACEF("PI %#x\n", read_reg(ahci_reg::PI));

    uint32_t port_bitmap = read_reg(ahci_reg::PI);
    size_t port_count = 0;
    for (size_t port = 0; port < 32; port++) {
        if ((port_bitmap & (1U << port)) == 0) {
            // skip port not implemented
            break;
        }
        port_count++;

        ports_[port] = new ahci_port(*this, port);
        auto *p = ports_[port];

        err = p->probe();
        if (err != NO_ERROR) {
            continue;
        }

        err = p->identify();
    }


    return NO_ERROR;
}

// hook called at init time to iterate through pci bus and find all of the ahci devices
static void ahci_init(uint level) {
    LTRACE_ENTRY;

    // probe pci to find a device
    for (size_t i = 0; ; i++) {
        pci_location_t loc;
        status_t err = pci_bus_mgr_find_device_by_class(&loc, 0x1, 0x6, 0x1, i);
        if (err != NO_ERROR) {
            break;
        }

        // we maybe found one, create a new device and initialize it
        auto e = new ahci;
        err = e->init_device(loc);
        if (err != NO_ERROR) {
            char str[14];
            printf("ahci: device at %s failed to initialize\n", pci_loc_string(loc, str));
            delete e;
            continue;
        }
    }
}
LK_INIT_HOOK(ahci, &ahci_init, LK_INIT_LEVEL_PLATFORM + 1);
