#ifndef __LAPIC_include__
#define __LAPIC_include__

#include <cstdint>
#include "ApicRegisterInterface.h"
#include "device/interrupt/ModelSpecificRegister.h"
#include "device/interrupt/Pic.h"
#include "device/power/acpi/Acpi.h"
#include "kernel/log/Logger.h"
#include "kernel/interrupt/InterruptDispatcher.h"

#define HHUOS_LAPIC_ENABLE 1
#define HHUOS_LAPIC_ENABLE_DEBUG 1

// NOTE: There are now 4 enums in total that are called "Interrupt":
// NOTE: - Pic::Interrupt (Physical IRQ lines on the PIC), referred to as irq
// NOTE: - LApic::Interrupt (Local interrupt on one of the 7 physical pins on the local APIC), referred to as lint
// NOTE: - IoApic::Interrupt (Physical IRQ lines on the IO APIC), referred to as gsi (Global System Interrupt)
// NOTE: - InterruptDispatcher::Interrupt (IDT vector number), referred to as vector

// NOTE: When the system is in PIC mode, it receives IRQs, when it's in APIC mode it receives GSIs
// NOTE: The GSIs range from 0 to 15 (for legacy PIC compatibility), the rest is adjacent and distributed to IO APICs

// TODO: Check ACPI Specification Chapter 5.2.13 for GSI naming scheme (IO APIC pins are calld INTIs)
//       and rename accordingly

// TODO: Add "unstatic" members to handle multiple local APICS through instances?
//       I don't want this because I want the handling of variable core amounts to be
//       contained in this class (otherwise runtime dependent amounts of LApic objects would
//       have to be managed in the InterruptService.

// TODO: Error handling: An error handler class that implements the handler for the ERROR vector

namespace Device {

class LApic {
    friend class ApicTimer; // ApicTimer is configured by using LApic registers
    friend class IoApic; // IoApic disables EOI suppression if it has no EOI register

public:
    // TODO: Implement allow/forbid for these in the interruptservice?
    // NOTE: The values have nothing to do with physical pins, they are the register offsets for the LVT
    // NOTE: Register::LVT_<...> and Interrupt::<...> is interchangable
    enum Interrupt : uint16_t {
        CMCI = 0x2F0,
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


    // TODO: Does not differentiate between multiple CPUs
    /**
     * Check if local APIC is initialized.
     *
     * @return True, if the local APIC is initialized
     */
    static bool isInitialized();

    /**
     * Check if local APIC support is present on the system (using CPUID).
     *
     * @return True if local APIC is supported
     */
    static bool hasApicSupport();

    /**
     * Check if x2APIC support is present on the system (using CPUID).
     *
     * @return True if x2APIC is supported
     */
    static bool hasX2ApicSupport();

    /**
     * Check if local APIC is running in x2Apic mode.
     *
     * @return True if APIC is running in x2Apic mode
     */
    [[nodiscard]] static bool isX2Apic();

    /**
     * Get the ID of the local APIC belonging to the current CPU.
     *
     * @return The local APIC's ID
     */
    [[nodiscard]] static uint8_t getId();

    /**
     * Get the local APIC version.
     *
     * @return The local APIC's version.
     */
    [[nodiscard]] static uint8_t getVersion();

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

    typedef struct LApicConfiguration {
        uint8_t uid;
        uint8_t id;
        bool enabled;
        bool canEnable; // TODO: Not available in ACPI 1.0?

        // Required because I stuff them into an ArrayList
        bool operator!=(const LApicConfiguration &other) const {
            return uid != other.uid || id != other.id || enabled != other.enabled || canEnable != other.canEnable;
        }
    } LApicConfiguration;

    typedef struct NMIConfiguration {
        uint8_t uid;
        LVTPinPolarity polarity;
        LVTTriggerMode triggerMode;
        Interrupt lint;

        // Required because I stuff them into an ArrayList
        bool operator!=(const NMIConfiguration &other) const {
            return uid != other.uid || polarity != other.polarity || triggerMode != other.triggerMode
            || lint != other.lint;
        };
    } NMIConfiguration;

    // TODO: Rename LocalPlatformConfiguration
    /**
     * This describes the hardware configuration of the system for all local APICs.
     */
    typedef struct PlatformConfiguration {
        bool xApicSupported;
        bool x2ApicSupported;
        bool isX2Apic;
        uint8_t version; // Needs to be set after MMIO is available
        uint32_t address;
        Util::Data::ArrayList<LApicConfiguration> lapics;
        Util::Data::ArrayList<NMIConfiguration> lnmis;
    } PlatformConfiguration;

private:
    /**
     * Reads information influencing local APIC initialization from ACPI tables.
     */
    static void initializePlatformConfiguration();

    static uint8_t idToUid(uint8_t uid);

    static LApicConfiguration getLApicConfiguration(uint8_t id);

    static NMIConfiguration getNMIConfiguration(uint8_t id); // TODO: return pointer

    /**
     * Marks every local interrupt in the local vector table as edge-triggered,
     * active high, masked and fixed delivery mode.
     * Vector numbers are set to InterruptDispatcher equivalent vectors.
     */
    static void initializeLVT();

    /**
     * Initialize the local APIC of an additional AP (Application Processor).
     * (MultiProcessor Specification Appendix B.4)
     *
     * Must not be called with enabled interrupts.
     *
     * @param destination The local APIC ID of the target CPU
     * @param startup_routine The physical address of the entry point for the target CPU
     */
    static void initializeApplicationProcessor(uint8_t destination, uint32_t startup_routine) = delete;

     // NOTE: Reading and writing local APIC's registers.
     // NOTE: Parses the read/written value to/from types from ApicRegisterInterface.h

    [[nodiscard]] static MSREntry readBaseMSR(); // Affects all local APICs

    static void writeBaseMSR(MSREntry entry); // Affects all local APICs

    [[nodiscard]] static uint32_t readDoubleWord(uint16_t reg); // Affects current CPU's local APIC

    static void writeDoubleWord(uint16_t reg, uint32_t val); // Affects current CPU's local APIC

    [[nodiscard]] static SVREntry readSVR();

    static void writeSVR(SVREntry svr);

    [[nodiscard]] static LVTEntry readLVT(Interrupt lint);

    static void writeLVT(Interrupt lint, LVTEntry entry);

    [[nodiscard]] static ICREntry readICR(); // Obtain delivery status of IPI

    // TODO: IPIs can only be sent by the BSP? Or does this only apply when no other cores are started yet?
    static void writeICR(ICREntry icr); // Issue IPIs

#if HHUOS_LAPIC_ENABLE_DEBUG == 1

    static void logDebugDump();

#endif

private:
    static bool initialized;
    static uint32_t baseVirtAddress; // The same for every local APIC, read/written registers differ
    static Device::ModelSpecificRegister ia32ApicBaseMsr;

    // NOTE: I chose to keep this and the contained structs stack allocated
    // NOTE: because I didn't want the hassle of having heap-allocated static objects.
    // NOTE: The ArrayLists are only allocated once elements are added.
    static PlatformConfiguration platformConfiguration; // TODO: Heap allocate

    static Util::Async::Spinlock mmioLock; // TODO
    static Util::Async::Spinlock ipiLock; // TODO

    static Kernel::Logger log;
};

}

#endif
