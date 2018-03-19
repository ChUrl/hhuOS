/*****************************************************************************
 *                                                                           *
 *                                  I D E                                    *
 *                                                                           *
 *---------------------------------------------------------------------------*
 * Beschreibung:    PCI IDE Controller                                       *
 *                                                                           *
 * Credits:         http://wiki.osdev.org/IDE                                *
 *                                                                           *
 * Autor:           Filip Krakowski, 03.11.2017                              *
 *****************************************************************************/

#include "devices/block/IDE.h"



extern "C" {
#include "lib/libc/string.h"
}

void outb(uint16_t ioPort, uint8_t value) {
  asm volatile("outb %0, %1" : : "a"(value), "Nd"(ioPort));
}

uint8_t inb(uint16_t ioPort) {
  unsigned char ret;
  asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(ioPort));
  return ret;
}

void inw(uint16_t ioPort, uint16_t buffer, uint32_t words) {}

uint16_t inw(uint16_t ioPort) {
  uint16_t ret;
  asm volatile("inw %1, %0" : "=a"(ret) : "Nd"(ioPort));
  return ret;
}

IDE::IDE() {}

void IDE::setup(uint32_t BAR0, uint32_t BAR1, uint32_t BAR2, uint32_t BAR3,
                uint32_t BAR4) {

  IDE_TRACE("Setting up IDE driver\n");
  IDE_TRACE("BAR0=%x   BAR1=%x   BAR2=%x   BAR4=%x   BAR5=%x\n", BAR0, BAR1,
            BAR2, BAR3, BAR4);

//   channels[ATA_PRIMARY].base = (BAR0 & 0xFFFFFFFC) + 0x1F0 * (!BAR0);
//   channels[ATA_PRIMARY].ctrl = (BAR1 & 0xFFFFFFFC) + 0x3F6 * (!BAR1);
//   channels[ATA_SECONDARY].base = (BAR2 & 0xFFFFFFFC) + 0x170 * (!BAR2);
//   channels[ATA_SECONDARY].ctrl = (BAR3 & 0xFFFFFFFC) + 0x376 * (!BAR3);
//   channels[ATA_PRIMARY].bmide = (BAR4 & 0xFFFFFFFC) + 0;
//   channels[ATA_SECONDARY].bmide = (BAR4 & 0xFFFFFFFC) + 8;

    channels[ATA_PRIMARY].base = 0x1F0;
    channels[ATA_PRIMARY].ctrl = 0x3F6;
    channels[ATA_SECONDARY].base = 0x170;
    channels[ATA_SECONDARY].ctrl = 0x376;
    channels[ATA_PRIMARY].bmide = BAR4 & ~0x1;
    channels[ATA_SECONDARY].bmide = (BAR4 & ~0x1) + 8;

    IDE_TRACE("P_BASE=%x   P_CTRL=%x   S_BASE=%x   S_CTRL=%x\n",
        channels[ATA_PRIMARY].base, channels[ATA_PRIMARY].ctrl,
        channels[ATA_SECONDARY].base, channels[ATA_SECONDARY].ctrl);

  detect();
}

uint8_t IDE::readByte(uint8_t channel, uint8_t reg) {
  unsigned char result;
  if (reg > 0x07 && reg < 0x0C)
    writeByte(channel, ATA_REG_CONTROL, 0x80 | channels[channel].ni);
  if (reg < 0x08)
    result = inb(channels[channel].base + reg - 0x00);
  else if (reg < 0x0C)
    result = inb(channels[channel].base + reg - 0x06);
  else if (reg < 0x0E)
    result = inb(channels[channel].ctrl + reg - 0x0A);
  else if (reg < 0x16)
    result = inb(channels[channel].bmide + reg - 0x0E);
  if (reg > 0x07 && reg < 0x0C)
    writeByte(channel, ATA_REG_CONTROL, channels[channel].ni);
  return result;
}

void IDE::writeByte(uint8_t channel, uint8_t reg, uint8_t value) {

  if (reg > 0x07 && reg < 0x0C)
    writeByte(channel, ATA_REG_CONTROL, 0x80 | channels[channel].ni);
  if (reg < 0x08) {
    outb(channels[channel].base + reg - 0x00, value);
  }

  else if (reg < 0x0C)
    outb(channels[channel].base + reg - 0x06, value);
  else if (reg < 0x0E)
    outb(channels[channel].ctrl + reg - 0x0A, value);
  else if (reg < 0x16)
    outb(channels[channel].bmide + reg - 0x0E, value);
  if (reg > 0x07 && reg < 0x0C)
    writeByte(channel, ATA_REG_CONTROL, channels[channel].ni);
}

uint8_t IDE::poll(uint8_t channel, bool advancedCheck) {

  for (int i = 0; i < IDE_MAX_DEVICES; i++) {
    readByte(channel, ATA_REG_ALTSTATUS);
  }

  while (readByte(channel, ATA_REG_STATUS) & ATA_STS_BSY);

  if (advancedCheck) {
    uint8_t state = readByte(channel, ATA_REG_STATUS); // Read Status Register.

    if (state & ATA_STS_ERR)
      return 2; // Error.

    if (state & ATA_STS_DF)
      return 1; // Device Fault.

    if ((state & ATA_STS_DRQ) == 0)
      return 3; // DRQ should be set
  }

  return 0;
}

void IDE::selectDrive(uint8_t channel, uint8_t drive) {
  if (channel != ATA_PRIMARY && channel != ATA_SECONDARY) {
    IDE_TRACE("Error: invalid channel!\n");
    return;
  }

  switch (drive) {
  case ATA_MASTER:
    writeByte(channel, ATA_REG_HDDEVSEL, ATA_DRV_MASTER);
    break;
  case ATA_SLAVE:
    writeByte(channel, ATA_REG_HDDEVSEL, ATA_DRV_SLAVE);
    break;
  default:
    IDE_TRACE("Error: invalid drive!\n");
    return;
  }

  IDE_TRACE("Selected drive %d on channel %d\n", drive, channel);
}

void IDE::delay(uint32_t steps) {
  for (uint32_t i = 0; i < steps; i++) {
    readByte(0, ATA_REG_STATUS);
  }
}

void IDE::identifyDrive(uint8_t channel, uint8_t drive) {
  writeByte(channel, ATA_REG_SECCOUNT0, 0);
  writeByte(channel, ATA_REG_LBA0, 0);
  writeByte(channel, ATA_REG_LBA1, 0);
  writeByte(channel, ATA_REG_LBA2, 0);
  writeByte(channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

  IDE_TRACE("Requested Identify data\n");
}

void IDE::waitBsy(uint8_t channel) {
  IDE_TRACE("Waiting for BSY on channel %d\n", channel);
  uint8_t timeout = 0;
  uint8_t err = 0;
  while (readByte(channel, ATA_REG_ALTSTATUS) & ATA_STS_BSY && timeout < ATA_TIMEOUT) {
    IDE_TRACE("Still busy...\n");

    err = readByte(channel, ATA_REG_ERROR);
    if (err & 0x1 || err & 0x20) {
        IDE_TRACE("ERROR: %x\n", err);
        return;
    }

    delay(1);
    timeout++;
  }

  if (timeout == 5) {
      IDE_TRACE("Error: Timeout on channel %d\n", channel);
  } else {
      IDE_TRACE("Device on channel %d is ready\n", channel);
  }
}

void IDE::resetDrive(uint8_t channel) {

  IDE_TRACE("Resetting channel %d\n", channel);

  writeByte(channel, ATA_REG_CONTROL, 4);

  delay(5);

  writeByte(channel, ATA_REG_CONTROL, 2);

  delay(5);

  waitBsy(channel);

  IDE_TRACE("Channel %d reset | ERR = %x\n", channel, readByte(channel, ATA_STS_ERR));
}

void IDE::detect() {

  uint8_t count = 0;
  uint8_t ide_buf[512];

  memset(ide_buf, 0, 512);

  resetDrive(ATA_PRIMARY);
  resetDrive(ATA_SECONDARY);

  for (uint8_t channel = 0; channel < 2; channel++) {
    for (uint8_t drive = 0; drive < 2; drive++) {

      uint8_t err = 0, type = IDE_ATA;
      ideDevices[count].reserved = 0;

      selectDrive(channel, drive);

      delay(5);

      identifyDrive(channel, drive);

      delay(5);

      if (readByte(channel, ATA_REG_STATUS) == 0) {
        IDE_TRACE("No Device present\n");
        continue;
      }

      IDE_TRACE("Device is present\n");

      waitBsy(channel);

      if (readByte(channel, ATA_REG_STATUS) & ATA_STS_ERR) {
        err = 1;
      }

      if (err != 0) {
        unsigned char cl = readByte(channel, ATA_REG_LBA1);
        unsigned char ch = readByte(channel, ATA_REG_LBA2);

        if (cl == 0x14 && ch == 0xEB)
          type = IDE_ATAPI;
        else if (cl == 0x69 && ch == 0x96)
          type = IDE_ATAPI;
        else
          continue;

        writeByte(channel, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);

        IDE_TRACE("ATAPI detected\n");

        delay(5);
      }

      IDE_TRACE("Reading drive information\n");

      readBuffer(channel, ATA_REG_DATA, (uint16_t *)&ide_buf, 256);

      ideDevices[count].reserved = 1;
      ideDevices[count].type = type;
      ideDevices[count].channel = channel;
      ideDevices[count].drive = drive;
      ideDevices[count].signature =
          *((unsigned short *)(ide_buf + ATA_IDENT_DEVICETYPE));
      ideDevices[count].capabilities =
          *((unsigned short *)(ide_buf + ATA_IDENT_CAPABILITIES));
      ideDevices[count].commandSets =
          *((unsigned int *)(ide_buf + ATA_IDENT_COMMANDSETS));

      if (ideDevices[count].commandSets & (1 << 26)) {
        ideDevices[count].size =
            *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA_EXT)) * 512;
      } else {
        ideDevices[count].size =
            *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA)) * 512;
      }

      for (uint8_t k = 0; k < 40; k += 2) {
        ideDevices[count].model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
        ideDevices[count].model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
      }

      ideDevices[count].model[40] = 0;

      for (uint8_t k = 0; k < 18; k += 2) {
        ideDevices[count].serial[k] = ide_buf[ATA_IDENT_SERIAL + k + 1];
        ideDevices[count].serial[k + 1] = ide_buf[ATA_IDENT_SERIAL + k];
      }

      ideDevices[count].model[18] = 0;

      IDE_TRACE(" -> Found %s Drive %dMB - %s\n         # %s\n",
                (const char *[]){"ATA", "ATAPI"}[ideDevices[count].type],
                ideDevices[count].size / 1024 / 2, ideDevices[count].model,
                ideDevices[count].serial);

      count++;
    }
  }
}

void IDE::readBuffer(uint8_t channel, uint8_t reg, uint16_t *buf,
                     uint32_t words) {

  if (reg > 0x07 && reg < 0x0C) {
    writeByte(channel, ATA_REG_CONTROL, 0x80 | channels[channel].ni);
  }

  uint32_t address = 0x0;

  if (reg < 0x08) {
    address = channels[channel].base + reg - 0x00;
  }

  else if (reg < 0x0C) {
    address = channels[channel].base + reg - 0x06;
  }

  else if (reg < 0x0E) {
    address = channels[channel].ctrl + reg - 0x0A;
  }

  else if (reg < 0x16) {
    address = channels[channel].bmide + reg - 0x0E;
  }

  for (uint32_t i = 0; i < words; i++) {
    buf[i] = inw(address);
  }

  if (reg > 0x07 && reg < 0x0C) {
    writeByte(channel, ATA_REG_CONTROL, channels[channel].ni);
  }
}
