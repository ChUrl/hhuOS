#include "kernel/service/InterruptService.h"
#include "kernel/system/System.h"
#include "LocalApic.h"
#include "LocalApicError.h"

namespace Device {

Kernel::Logger LocalApicError::log = Kernel::Logger::get("Apic Error Handler");

void LocalApicError::plugin() {
    auto &interruptService = Kernel::System::getService<Kernel::InterruptService>();
    interruptService.assignInterrupt(Kernel::InterruptVector::ERROR, *this);
}

void LocalApicError::trigger(const Kernel::InterruptFrame &frame) {
    // This works for multiple cores because the core that handles the interrupt calls this function
    // and thus reaches the correct local APIC

    // Single write before read (read/write register, IA-32 Architecture Manual Chapter 10.5.3)
    LocalApic::writeDoubleWord(LocalApic::ESR, 0);
    const uint32_t errors = LocalApic::readDoubleWord(LocalApic::ESR);

    // If any of these is present is architecture dependent, so instead a unified error message is logged.
    /*
    // Errors for all CPUs
    bool illegalVectorReceived = errors & (1 << 6);
    bool illegalVectorSent = errors & (1 << 5);

    // Errors reserved on original Pentium CPUs
    bool illegalRegisterAccess = errors & (1 << 7);

    // Errors reserved on Core, P4, Xeon CPUs
    bool receiveAcceptError = errors & (1 << 3);
    bool sendAcceptError = errors & (1 << 2);
    bool receiveChecksumError = errors & (1 << 1);
    bool sendChecksumError = errors & 1;

    if (illegalVectorReceived) { log.error("ERROR: Illegal vector received!"); }
    if (illegalVectorSent) { log.error("ERROR: Illegal vector sent!"); }
    if (illegalRegisterAccess) { log.error("ERROR: Illegal register access!"); }
    if (receiveAcceptError) { log.error("ERROR: Receive accept error!"); }
    if (sendAcceptError) { log.error("ERROR: Send accept error!"); }
    if (receiveChecksumError) { log.error("ERROR: Receive checksum error!"); }
    if (sendChecksumError) { log.error("ERROR: Send checksum error!"); }
    */

    log.error("Local APIC [%d] encountered error: [0b%b]!", LocalApic::getId(), errors);
}

} // namespace Device
