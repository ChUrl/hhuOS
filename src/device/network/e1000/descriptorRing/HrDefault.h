/*
 * Copyright (C) 2018/19 Thiemo Urselmann
 * Heinrich-Heine University
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 * Note:
 * All references marked with [...] refer to the following developers manual.
 * Intel Corporation. PCI/PCI-X Family of Gigabit Ethernet Controllers Software Developer’s Manual.
 * 317453006EN.PDF Revision 4.0. 2009.
 */

#ifndef HHUOS_HRDEFAULT_H
#define HHUOS_HRDEFAULT_H


#include "HrDefault.h"
#include "HardwareDescriptorRing.h"

/**
 * This abstract class extends the HardwareDescriptorRing
 * interface.
 *
 * All transmit/receive independent methods are
 * implemented here. Implementing classes have to
 * specify the initial tail values according to
 * transmit/receive.
 */
class HrDefault : public HardwareDescriptorRing {
public:
    ~HrDefault() override = default;

    HrDefault(HrDefault const &) = delete;
    HrDefault &operator=(HrDefault const &) = delete;

    /**
     * Inherited methods from HardwareDescriptorRing.
     * This methods are meant to be overridden and
     * implemented by this class.
     */

    void initialize() final;
    void updateTail() final;
protected:
    /**
     * Constructor. The tail attribute is set to 0.
     * @param virtualBase MMIO-space base address.
     * @param physicalAddress Physical address of the corresponding descriptor block.
     * @param descriptors Amount of descriptors contained in the corresponding descriptor block.
     */
    HrDefault(uint8_t *virtualBase, uint64_t physicalAddress, uint16_t descriptors);

    /**
     * The virtual base address of the ring-registers within the MMIO-Space.
     */
    const uint8_t *virtualBase;

    /**
     * Physical address according to the virtual base address of
     * the ring-registers within the MMIO-Space.
     */
    const uint64_t physicalAddress;

    /**
     * Amount of descriptors inside the ring.
     */
    const uint16_t descriptors;

    /**
     * The current tail value. Default value is 0.
     */
    uint32_t tail;

    /**
     * Inherited method from HardwareDescriptorRing.
     * This methods is meant to be overridden and
     * implemented by extending this class.
     */

    void initTail() override = 0;

    /**
     * Inherited methods from HardwareDescriptorRing.
     * This methods are meant to be overridden and
     * implemented by this class.
     */

    void initBase() final;
    void initHead() final;
    void initLength() final;
    uint32_t *chooseRegister(uint8_t number) final;
};


#endif //HHUOS_HRDEFAULT_H
