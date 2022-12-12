#ifndef __LAPIC_include__
#define __LAPIC_include__

#include <cstdint>
#include "ApicRegisterInterface.h"
#include "device/interrupt/ModelSpecificRegister.h"
#include "device/interrupt/Pic.h"
#include "device/power/acpi/Acpi.h"
#include "kernel/log/Logger.h"
#include "kernel/interrupt/InterruptDispatcher.h"

#define HHUOS_LAPIC_ENABLE_DEBUG 1

// NOTE: There are now 4 types of "Interrupt":
// NOTE: - Pic::Interrupt (on a PIC), referred to as irq
// NOTE: - LApic::Interrupt (on a local APIC), referred to as lint
// NOTE: - IoApic interrupts (on an IO APIC), referred to as gsi (Global System Interrupt)
// NOTE: - InterruptDispatcher::Interrupt (IDT vector number), referred to as vector

// NOTE: When the system is in PIC mode, it receives IRQs (there are 16 of them),
// NOTE: when it's in APIC mode it receives GSIs (regularly up to
// NOTE: The GSIs range from 0 to 15 (for legacy PIC compatibility), the rest is adjacent and distributed to IO APICs

// TODO: Error handling: An error handler class that implements the handler for the ERROR vector

// TODO: 64 Bit needs ACPI APIC Address Override structure (but current code is not 64 bit compatible anyways)

namespace Device {

class LApic {
    friend class ApicTimer; // ApicTimer is configured by using LApic registers
    friend class IoApic; // IoApic disables EOI suppression if it has no EOI register

public:
    // TODO: Implement allow/forbid for these in the interruptservice?
    // NOTE: The values have nothing to do with physical pins, they are the register offsets for the LVT
    // NOTE: Register::LVT_<...> and Interrupt::<...> is interchangable
    enum Interrupt : uint16_t {
        CMCI = 0x2F0, // TODO: Might not exist (check xv6)
        TIMER = 0x320,
        THERMAL = 0x330,
        PERFORMANCE = 0x340,
        LINT0 = 0x350,
        LINT1 = 0x360,
        ERROR = 0x370
    };

public:
    LApic() = delete; // Static class

    LApic(const LApic &copy) = delete;

    LApic &operator=(const LApic &copy) = delete;

    LApic(LApic &&move) = delete;

    LApic &operator=(LApic &&move) = delete;

    ~LApic() = delete; // Static class


    static bool isSupported();

    // TODO: Differentiate between different cores (put initialized in the LApicConfiguration struct?)
    // NOTE: Currently if this is true this also means that all other APs were initialized
    /**
     * Check if local APIC is initialized.
     *
     * @return True, if the local APIC is initialized
     */
    static bool isInitialized();

    // TODO: Currently also initializes all APs, should probably remove that
    /**
     * Initialize the local APIC with all local APIC interrupts masked and
     * EOI broadcasting disabled.
     * Only call explicitly for the bootstrap processor.
     *
     * Must not be called with enabled interrupts.
     */
    static void initialize();

    /**
     * Unmask a local interrupt in the local APIC of the current CPU.
     *
     * @param lint The local interrupt to activate
     */
    static void allow(Interrupt lint);

    /**
     * Forbid a local interrupt in the local APIC of the current CPU.
     *
     * @param lint The local interrupt to deactivate
     */
    static void forbid(Interrupt lint);

    /**
     * Get the state of this interrupt - whether it is masked out or not.
     *
     * @param lint The register offset of the interrupt
     * @return True, if the interrupt is disabled in the local APIC of the current CPU
     */
    static bool status(Interrupt lint);

    // TODO: For compatibility with older IO APICs (< version 0x20) this has to be used in
    //       combination with temporarily setting all IO APIC redirection entries to level-triggered
    //       (https://github.com/torvalds/linux/blob/master/arch/x86/kernel/apic/io_apic.c#L470)
    /**
     * Send an end of interrupt signal to the local APIC of the current CPU.
     * The signal will be broadcasted to IO APICs if the interrupt was
     * level-triggered and broadcasting is enabled in the SVR.
     * (IA-32 Architecture Manual Chapter 10.8.5)
     *
     * For IO APICs with version >= 0x20 prefer sending the EOI to the IO APIC.
     */
    static void sendEndOfInterrupt();

    static void handleErrors();

private:
    // Offsets, IA-32 Architecture Manual Chapter 10.4.1
    // NOTE: Omitted entries already in LApic::Interrupt and ApicTimer
    enum Register : uint16_t {
        ID = 0x20, // ID
        VER = 0x30, // Version
        TPR = 0x80, // Task Priority Register
        APR = 0x90, // Arbitration Priority Register
        PPR = 0xA0, // Processor Priority Register
        EOI = 0xB0, // End of Interrupt Register
        RRD = 0xC0, // Remote Read Register
        LDR = 0xD0, // Logical Destination Register
        DFR = 0xE0, // Destination Format Register
        SVR = 0xF0, // Spurious Interrupt Vector Register
        ISR = 0x100, // In-Service Register (255 bit)
        TMR = 0x180, // Trigger Mode Register (255 bit)
        IRR = 0x200, // Interrupt Request Register (255 bit)
        ESR = 0x280, // Error Status Register
        ICR_LOW = 0x300, // Interrupt Command Register (64 bit)
        ICR_HIGH = 0x310,
    };

    // NOTE: I chose to store parsed information like this to be able to initialize it from different sources
    // NOTE: (like ACPI 1.0b, ACPI >= 2.0, MP tables)
    // TODO: Add contents of MSR if MSR exists for every core individually?
    typedef struct LApicConfiguration {
        uint8_t uid; // ACPI also stores this // TODO: Do I need this
        uint8_t id; // TODO: Check if ACPI sets this by itself of if the ID reg values have to be signalled to ACPI
        // TODO: Does this mean the processor can't be used currently but could be started? Or can't be started at all?
        bool enabled; // If false the operating system can't use this processor
    } LApicConfiguration;

    typedef struct LNMIConfiguration {
        uint8_t uid; // 0xFF means all CPUs
        uint8_t id; // Added for convenience, 0xFF means all CPUs
        LVTPinPolarity polarity;
        LVTTriggerMode triggerMode;
        Interrupt lint;
    } LNMIConfiguration;

    /**
     * This describes the hardware configuration of the system for all local APICs.
     */
    typedef struct LPlatformConfiguration {
        bool xApicSupported;
        bool x2ApicSupported;
        bool isX2Apic; // Indicates mode currently running in
        uint8_t version; // Needs to be set after MMIO is available
        uint32_t address;
        uint32_t virtAddress;
        Util::Data::ArrayList<LApicConfiguration *> lapics;
        Util::Data::ArrayList<LNMIConfiguration *> lnmis;
    } LPlatformConfiguration;

private:
    /**
     * Get the ID of the local APIC belonging to the current CPU.
     * Used to determine what CPU is currently running.
     *
     * @return The local APIC's ID
     */
    [[nodiscard]] static uint8_t getId();

    /**
     * Reads information influencing local APIC initialization from ACPI tables.
     */
    static void initializePlatformConfiguration();

    static uint8_t uidToId(uint8_t uid);

    static LApicConfiguration *getLApicConfiguration(uint8_t id);

    static LNMIConfiguration *getNMIConfiguration(LApicConfiguration *lapic);

    // TODO: If initializeApplicationProcessor needs to call this move it down
    static void initializeMMIORegion();

    /**
     * Initialize the local APIC of an additional AP (Application Processor).
     * (MultiProcessor Specification Appendix B.4)
     *
     * Must not be called with enabled interrupts.
     */
    static void initializeApplicationProcessor(LApicConfiguration *lapic);

    static void initializeController(LApicConfiguration *lapic);

    /**
     * Marks every local interrupt in the local vector table as edge-triggered,
     * active high, masked and fixed delivery mode.
     * Vector numbers are set to InterruptDispatcher equivalent vectors.
     */
    static void initializeLVT();

    static void dumpLPlatformConfiguration();

     // NOTE: Reading and writing local APIC's registers
     // NOTE: Parses the read/written value to/from types from ApicRegisterInterface.h
     // NOTE: Only registers of currently running CPU will be affected

    [[nodiscard]] static MSREntry readBaseMSR(); // TODO: Affects all local APICs?

    static void writeBaseMSR(MSREntry entry); // TODO: Affects all local APICs?

    [[nodiscard]] static uint32_t readDoubleWord(uint16_t reg);

    static void writeDoubleWord(uint16_t reg, uint32_t val);

    [[nodiscard]] static SVREntry readSVR();

    static void writeSVR(SVREntry svr);

    [[nodiscard]] static LVTEntry readLVT(Interrupt lint);

    static void writeLVT(Interrupt lint, LVTEntry entry);

    [[nodiscard]] static ICREntry readICR(); // Obtain delivery status of IPI

    // TODO: IPIs can only be sent by the BSP? Or does this only apply when no other cores are started yet?
    static void writeICR(ICREntry icr); // Issue IPIs

private:
    static bool initialized;
    static Device::ModelSpecificRegister ia32ApicBaseMsr;

    static LPlatformConfiguration platformConfiguration;

    static Util::Async::Spinlock mmioLock; // TODO
    static Util::Async::Spinlock ipiLock; // TODO

    static Kernel::Logger log;
};

}

#endif
