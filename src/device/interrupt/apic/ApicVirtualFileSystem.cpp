
#include "Apic.h"
#include "device/interrupt/Pic.h"
#include "filesystem/memory/MemoryDriver.h"
#include "filesystem/memory/MemoryFileNode.h"
#include "kernel/service/FilesystemService.h"
#include "kernel/system/System.h"

namespace Device {

void Apic::mountVirtualFilesystemNodes() {
    ensureApic();
    // TODO: Create some UpdateOnReadFileNode or sth. that executes a updateCallback function before read
    //       to update these files lazily (wouldn't want to update the interrupts file on every interrupt...)

    auto &filesystemService = Kernel::System::getService<Kernel::FilesystemService>();
    auto &driver = filesystemService.getFilesystem().getVirtualDriver("/device");

    auto *localApicNode = new Filesystem::Memory::MemoryFileNode("lapic");
    auto *ioApicNode = new Filesystem::Memory::MemoryFileNode("ioapic");
    auto *lvtNode = new Filesystem::Memory::MemoryFileNode("lvt");
    auto *redtblNode = new Filesystem::Memory::MemoryFileNode("redtbl");
    auto *picNode = new Filesystem::Memory::MemoryFileNode("pic");
    auto *irqsNode = new Filesystem::Memory::MemoryFileNode("irqs");

    Util::String lapic, ioapic, lvt, redtbl, pic, irqs;
    printLocalApics(lapic);
    printIoApic(ioapic);
    LocalApic::printLvt(lvt);
    ioApic->printRedtbl(redtbl);
    Pic::printStatus(pic);
    printInterrupts(irqs);

    localApicNode->writeData(static_cast<const uint8_t *>(lapic), 0, lapic.length());
    ioApicNode->writeData(static_cast<const uint8_t *>(ioapic), 0, ioapic.length());
    lvtNode->writeData(static_cast<const uint8_t *>(lvt), 0, lvt.length());
    redtblNode->writeData(static_cast<const uint8_t *>(redtbl), 0, redtbl.length());
    picNode->writeData(static_cast<const uint8_t *>(pic), 0, pic.length());
    irqsNode->writeData(static_cast<const uint8_t *>(irqs), 0, irqs.length());

    filesystemService.createDirectory("/device/apic");
    driver.addNode("/apic/", localApicNode);
    driver.addNode("/apic/", ioApicNode);
    driver.addNode("/apic/", lvtNode);
    driver.addNode("/apic/", redtblNode);
    driver.addNode("/apic/", picNode);
    driver.addNode("/apic/", irqsNode);
}

void Apic::printLocalApics(Util::String &string) {
    string += Util::String::format("Local APIC supported modes: [%s%s] (Current mode: [xApic])\n",
                                   LocalApic::supportsXApic() ? "xApic" : "None",
                                   LocalApic::supportsX2Apic() ? ", x2Apic" : "");
    string += Util::String::format("Local APIC version: [0x%x]\n", LocalApic::getVersion());
    string += Util::String::format("Local APIC xApic MMIO: [0x%x] (phys) -> [0x%x] (virt)\n",
                                   LocalApic::baseAddress,
                                   LocalApic::mmioAddress);

    string += "\nLocal APICs:\n";
    for (uint32_t i = 0; i < localApics.size(); ++i) {
        const LocalApic &localApic = *localApics.get(i);
        string += Util::String::format("Id: [0x%x], Running: [%d], NMI: (LINT: [%d], Polarity: [%s], Trigger: [%s])\n",
                                       localApic.cpuId,
                                       localApic.initialized,
                                       localApic.nmiLint - LocalApic::LINT0,
                                       localApic.nmiPolarity == LVTEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                                       localApic.nmiTrigger == LVTEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }
}

void Apic::printIoApic(Util::String &string) {
    string += Util::String::format("I/O APIC version: [0x%x]\n",
                                   ioApic->getVersion());
    string += Util::String::format("Supports directed EOI: [%d]\n",
                                   static_cast<int>(ioApic->getVersion() >= 0x20));

    string += "\nI/O APIC:\n";
    string += Util::String::format("Id: [%d], GSI: [%d] - [%d], MMIO: [0x%x] (phys) -> [0x%x] (virt)\n",
                                   ioApic->ioId,
                                   static_cast<uint32_t>(ioApic->gsiBase),
                                   static_cast<uint32_t>(ioApic->gsiMax),
                                   ioApic->baseAddress,
                                   ioApic->mmioAddress);
    for (uint32_t j = 0; j < ioApic->nmiSources.size(); ++j) {
        const IoApic::NmiSource &nmi = *ioApic->nmiSources.get(j);
        string += Util::String::format("NMI: (GSI: [%d], Polarity: [%s], Trigger: [%s])\n",
                                       static_cast<uint32_t>(nmi.source),
                                       nmi.polarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                                       nmi.trigger == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }

    string += ("\nI/O APIC IRQ overrides:\n");
    for (uint32_t i = 0; i < IoApic::irqOverrides.size(); ++i) {
        const IoApic::IrqOverride &override = *IoApic::irqOverrides.get(i);
        string += Util::String::format("Source: [%d], Target: [%d], Polarity: [%s], Trigger: [%s]\n",
                                       static_cast<uint32_t>(override.source),
                                       static_cast<uint32_t>(override.target),
                                       override.polarity == REDTBLEntry::PinPolarity::HIGH ? "HIGH" : "LOW",
                                       override.trigger == REDTBLEntry::TriggerMode::EDGE ? "EDGE" : "LEVEL");
    }
}

void Apic::printInterrupts(Util::String &string) {
    // Print the header
    string += "vector";
    for (uint32_t i = 0; i < getCpuCount(); ++i) {
        string += Util::String::format(",cpu%d", i);
    }
    string += "\n";

    // Print a line for each interrupt vector, listing the amounts per core
    Util::String line;
    bool occured = false;
    for (uint32_t i = 0; i < counters->length(); ++i) {
        if (i % getCpuCount() == 0) {
            // We are on a new line
            if (occured) {
                // Append the last line unless its all 0s
                string += line;
                string += "\n";
            }

            line = Util::String::format("%d", i / getCpuCount());
            occured = false;
        }

        line += Util::String::format(",%d", (*counters)[i]);
        occured = occured | ((*counters)[i] != 0);
    }
}

} // namespace Device