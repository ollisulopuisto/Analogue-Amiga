/*
 * Copyright 2022 Murray Aickin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

#include "hardware.h"
#include "apf.h"
#include "riscprintf.h"
#include "timer.h"

#define SSPI_ACK

bool dataslot_ready(){
  int apf_codes = READ_TARGET_DATASLOT_CONTROL(0);
  if ((apf_codes & APF_ERROR) > 0) mainprintf("\033[38;1;5mCheck codes %0.4X\033[0m\r\n", (apf_codes & APF_ERROR));
  // if (!(apf_codes & APF_DONE)) mainprintf("\033[38;1;5mwe are waiting\033[0m\r\n");

  return(apf_codes & APF_DONE);
}

uint8_t dataslot_status(){

  int apf_codes = READ_TARGET_DATASLOT_CONTROL(0);
  return(apf_codes & 0x7);
}

// This is to search though the dataslot ram looking for the location of the data slot in it
uint32_t dataslot_search_id(uint16_t value)
{
    int i = 0;
    uint32_t temp;
    // while loop from 1 to 5
    do  {
      temp = DATASLOT_RAM_ACCESS(i<<2) & 0xffff;
      if (temp == value){
        return (i & 0xFFFF);
      };
      i=i+2;
    } while (i <= 0x100);
    return (0);
}

// This is to search though the dataslot ram on what the sizes of the dataslow wanted
uint32_t dataslot_size(uint16_t value)
{
  int i;
  uint32_t size;
  mainprintf("number value: %d \r\n", value);
  i = dataslot_search_id(value);
  mainprintf("number slot: %d \r\n", i);
  size = DATASLOT_RAM_ACCESS((i+1)<<2);
  mainprintf("number size: %d \r\n", size);
  return (size);
}


uint32_t dataslot_size_lba64(uint16_t value)
{
  int i;
  uint32_t size;
  riscprintf("lba64 number value: %d \r\n", value);
  i = dataslot_search_id(value);
  riscprintf("lba64 number slot: %d \r\n", i);
  size = (DATASLOT_RAM_ACCESS((i)<<2) >> 16) & 0x1ff;
  riscprintf("lba64 1st number size: %d \r\n", size);
  size = (size << 24) | (DATASLOT_RAM_ACCESS((i+1)<<2) >> 9);
  riscprintf("lba64 2nd number size: %d \r\n", size);
  return (size);
}

// This checks the reg in the system on if there was an apf dataslot update. Once this reg is read it is cleared by hardware
bool dataslot_updated()
{
    return(DATASLOT_UPDATE_REG(0));
}

// This will send the read command to the APF
// The WRITE_* are the regs in the core for the location of the dataslow wanted to be read
// This is used for block data with no pointer changes

uint32_t dataslot_read(uint16_t dataslot, uint32_t address, uint32_t offset, uint32_t length, bool lba48)
{
  
  WRITE_TARGET_DATASLOT_ID(0) = dataslot;
  WRITE_TARGET_DATASLOT_BRIDGE_ADD(0) = address;
  if (lba48) {
    WRITE_TARGET_DATASLOT_OFFSET_LBA48HIG(0) = ((offset >> 23) & 0x1FF);
    WRITE_TARGET_DATASLOT_OFFSET(0) = ((offset & 0x7FFFFF) << 9);
    if (((offset >> 23) & 0x1FF) > 0) mainprintf("APF APF48 read overboard\r\n");
  } else{
    WRITE_TARGET_DATASLOT_OFFSET(0) = offset;
  }
  WRITE_TARGET_DATASLOT_LENGTH(0) = length;
  if (lba48) {
    WRITE_TARGET_DATASLOT_CONTROL(0) = TARGET_DATASLOT_LBA_48_READ_REG;
  } else{
    WRITE_TARGET_DATASLOT_CONTROL(0) = TARGET_DATASLOT_READ_REG;
  }
  if (lba48) {
    mainprintf("APF APF48 read: %d, %0.4x, %0.4x, %d\r\n", dataslot, address, offset, length);
  } else{
    mainprintf("APF read: %d, %0.4x, %0.4x, %d\r\n", dataslot, address, offset, length);
  }
  int apf_codes;
  int i = 0;

  do
  {
    apf_codes = READ_TARGET_DATASLOT_CONTROL(0);
    if ((apf_codes & APF_ERROR) > 0)
    {
      mainprintf("\033[36;5;4mAPF FAILD dataslot_read check? dataslot %d, address %0.4X, offset%0.4X, length%0.4X\033[0m\r\n", dataslot, address, offset, length);
      return (apf_codes & APF_ERROR);
    }
    i++;
    // mainprintf("apf_codes %0.8x\r\n", apf_codes);
  } while (!(apf_codes & APF_ACK) && !(apf_codes & APF_DONE) && !(i >= 10000000));

	do
	{
		apf_codes = READ_TARGET_DATASLOT_CONTROL(0);
    if ((apf_codes & APF_ERROR) > 0)
		{
			mainprintf("\033[36;5;4mAPF FAILD dataslot_read done? dataslot %d, address %0.4X, offset %0.4X, length %0.4X error %0.4X\033[0m\r\n", dataslot, address, offset, length, apf_codes & APF_ERROR);
			return (apf_codes & APF_ERROR);
		}
	} while (!(apf_codes & APF_DONE));
  mainprintf("\033[38;1;5mdone\033[0m\r\n");
  return(0);
}

// This will send the write command to the APF and send back a error if true
// This is used for block data with no pointer changes

uint32_t dataslot_write(uint16_t dataslot, uint32_t address, uint32_t offset, uint32_t length, bool lba48)
{
  WRITE_TARGET_DATASLOT_ID(0) = dataslot;
  WRITE_TARGET_DATASLOT_BRIDGE_ADD(0) = address;
  if (lba48) {
    WRITE_TARGET_DATASLOT_OFFSET_LBA48HIG(0) = ((offset >> 23) & 0x1FF);
    WRITE_TARGET_DATASLOT_OFFSET(0) = ((offset & 0x7FFFFF) << 9);
  } else{
    WRITE_TARGET_DATASLOT_OFFSET(0) = offset;
  }
  WRITE_TARGET_DATASLOT_LENGTH(0) = length;
  if (lba48) {
    WRITE_TARGET_DATASLOT_CONTROL(0) = TARGET_DATASLOT_LBA_48_WRITE_REG;
  } else{
    WRITE_TARGET_DATASLOT_CONTROL(0) = TARGET_DATASLOT_WRITE_REG;
  }
  if (lba48) {
  mainprintf("APF APF48 Write: %d, %0.4x, %0.4x, %d\r\n", dataslot, address, offset, length);
  } else{
  mainprintf("APF Write: %d, %0.4x, %0.4x, %d\r\n", dataslot, address, offset, length);
  }
  int apf_codes;
	do
	{
		apf_codes = READ_TARGET_DATASLOT_CONTROL(0);
    if ((apf_codes & APF_ERROR) > 0)
		{
			riscprintf("\033[36;5;4mAPF FAILD dataslot_write? dataslot %d, address %0.4X, offset%0.4X, length%0.4X\033[0m\r\n", dataslot, address, offset, length);
			return (apf_codes & APF_ERROR);
		}
	} while (!(apf_codes & APF_DONE));
  return(0);
}

// THis will set the pointers read to go on the APF regs
uint32_t dataslot_read_lba_set_fast(uint16_t dataslot, uint32_t address, uint32_t offset, uint32_t length)
{
  int i = 0;
  int apf_codes;
  do
	{
    apf_codes = READ_TARGET_DATASLOT_CONTROL(0);
    i++;
  } while (!(apf_codes & APF_DONE) | (i <=10000));

  READ_TARGET_DATASLOT_ID(0) = dataslot;
  READ_TARGET_DATASLOT_BRIDGE_ADD(0) = APF_ADDRESS_OFFSET | address;
  READ_TARGET_DATASLOT_OFFSET(0) = offset << 9;
  READ_TARGET_DATASLOT_LENGTH(0) = length;
  READ_TARGET_DATASLOT_CONTROL(0) = TARGET_DATASLOT_READ_REG;


  do
	{
    apf_codes = READ_TARGET_DATASLOT_CONTROL(0);
  } while (!(apf_codes & APF_ACK) );

  int tmp = READ_TARGET_DATASLOT_OFFSET(0);
  READ_TARGET_DATASLOT_OFFSET(0) = tmp + length;
  return(0);
}



// THis will set the pointers read to go on the APF regs
uint32_t dataslot_read_lba_set(uint16_t dataslot, uint32_t address, uint32_t offset)
{
  READ_TARGET_DATASLOT_ID(0) = dataslot;
  READ_TARGET_DATASLOT_BRIDGE_ADD(0) = APF_ADDRESS_OFFSET | address;
  READ_TARGET_DATASLOT_OFFSET(0) = offset;

  mainprintf("\033[38;1;6mREAD_TARGET_DATASLOT_SETUP Offset %.4x, address %.4x, dataslot %d, \033[0m\r\n", offset, address, dataslot);
  return(0);
}

// This will read the data from where the file pointer is (This is set by dataslot_read_lba_set)
uint32_t dataslot_read_lba(uint32_t length)
{
  READ_TARGET_DATASLOT_LENGTH(0) = length;
  READ_TARGET_DATASLOT_CONTROL(0) = TARGET_DATASLOT_READ_REG;
  mainprintf("READ_TARGET_DATASLOT_LENGTH Sent %.4x\r\n", WRITE_TARGET_DATASLOT_LENGTH(0));
  int i = 0;
  int apf_codes;
  do
	{
    apf_codes = READ_TARGET_DATASLOT_CONTROL(0);

    i++;
  } while (!(apf_codes & APF_ACK) | (i <=100));
	do
	{
		apf_codes = READ_TARGET_DATASLOT_CONTROL(0);
    if ((apf_codes & APF_ERROR) > 0)
		{
			mainprintf("\033[36;5;4mAPF FAILD read_lba? length %0.4X\033[0m\r\n", length);
			return (apf_codes & APF_ERROR);
		}
    // minimig_input_update();
	} while (!(apf_codes & APF_DONE));
  int tmp = WRITE_TARGET_DATASLOT_OFFSET(0);
  WRITE_TARGET_DATASLOT_OFFSET(0) = tmp + length;
  return(0);
}

// This will read the data from where the file pointer is (This is set by dataslot_read_lba_set) and does the next batch of data
uint32_t dataslot_read_lba_fast( uint32_t length, bool wait_update)
{
  int apf_codes;
  int i = 0;
  if (length > 0)READ_TARGET_DATASLOT_LENGTH(0) = length;
  else return(0);
  READ_TARGET_DATASLOT_CONTROL(0) = TARGET_DATASLOT_READ_REG;

  // mainprintf("READ_TARGET_DATASLOT_LENGTH fast Sent %.4x\r\n", length);
  if (wait_update) {
    do
  	{
      apf_codes = READ_TARGET_DATASLOT_CONTROL(0);
      i++;
    } while (!(apf_codes & APF_ACK) | (i <=10000));
    int tmp = WRITE_TARGET_DATASLOT_OFFSET(0);
    WRITE_TARGET_DATASLOT_OFFSET(0) = tmp + length;
    // uint32_t address = READ_TARGET_DATASLOT_BRIDGE_ADD(0);
    // WRITE_TARGET_DATASLOT_BRIDGE_ADD(0) = APF_ADDRESS_OFFSET | address + length;
  }
  return(0);
}

// THis will set the pointers write to go on the APF regs
uint32_t dataslot_write_lba_set(uint16_t dataslot, uint32_t address, uint32_t offset)
{
  WRITE_TARGET_DATASLOT_ID(0) = dataslot;
  WRITE_TARGET_DATASLOT_BRIDGE_ADD(0) = APF_ADDRESS_OFFSET | address;
  WRITE_TARGET_DATASLOT_OFFSET(0) = offset << 9;
  WRITE_TARGET_DATASLOT_LENGTH(0) = 1;
  WRITE_TARGET_DATASLOT_CONTROL(0) = TARGET_DATASLOT_WRITE_REG;

    // mainprintf("dataslot_write_lba_set address %.4x offset %.4x\r\n", address, offset);
  int apf_codes;
	do
	{
		apf_codes = READ_TARGET_DATASLOT_CONTROL(0);
    if ((apf_codes & APF_ERROR) > 0)
		{
			// riscprintf("\033[36;5;4mAPF FAILD dataslot_write_lba_set? dataslot %d, address %0.4X, offset%0.4X\033[0m\r\n", dataslot, address, offset);
			return (apf_codes & APF_ERROR);
		}
	} while (!(apf_codes & APF_DONE));
  return(0);
}

// This will write the data from where the file pointer is (This is set by dataslot_read_lba_set)
uint32_t dataslot_write_lba(uint32_t length, bool update_address)
{
  WRITE_TARGET_DATASLOT_LENGTH(0) = length;
  WRITE_TARGET_DATASLOT_CONTROL(0) = TARGET_DATASLOT_WRITE_REG;
  int apf_codes;
	do
	{
		apf_codes = WRITE_TARGET_DATASLOT_CONTROL(0);
    if ((apf_codes & APF_ERROR) > 0)
		{
			// riscprintf("\033[36;5;4mAPF FAILD dataslot_write_lba? length%0.4X\033[0m\r\n", length);
			return (apf_codes & APF_ERROR);
		}
    // minimig_input_update();
	} while (!(apf_codes & APF_DONE));
  int tmp = WRITE_TARGET_DATASLOT_OFFSET(0);
  WRITE_TARGET_DATASLOT_OFFSET(0) = tmp + length;
  uint32_t address = READ_TARGET_DATASLOT_BRIDGE_ADD(0);
  if (update_address) WRITE_TARGET_DATASLOT_BRIDGE_ADD(0) = APF_ADDRESS_OFFSET | address + length;
  return(0);
}

// we need a check commands for the APF

// Path buffers the host reads or writes over the bridge (MPU RAM is mapped 1:1)
static uint32_t dataslot_path_buffer[DATASLOT_PATH_LEN / 4];
#if !DATASLOT_OPENFILE_IN_DATATABLE
static uint32_t dataslot_open_struct[(DATASLOT_PATH_LEN + 8) / 4];
#endif

// Starts a 0x0190 / 0x0192 command and waits for the host. Returns the result code.
static uint32_t dataslot_file_command(uint16_t dataslot, uint32_t address, uint32_t control)
{
  WRITE_TARGET_DATASLOT_ID(0) = dataslot;
  WRITE_TARGET_DATASLOT_BRIDGE_ADD(0) = address;
  WRITE_TARGET_DATASLOT_CONTROL(0) = control;

  // The bridge drops done (and the old result) as the command starts; make sure that
  // has happened so the previous command's done is not taken as this answer
  int apf_codes;
  int i = 0;
  do
  {
    apf_codes = READ_TARGET_DATASLOT_CONTROL(0);
    i++;
  } while ((apf_codes & APF_DONE) && !(apf_codes & APF_ACK) && (i < 1000));

  unsigned int start = GetMilliseconds();
  do
  {
    apf_codes = READ_TARGET_DATASLOT_CONTROL(0);
    if ((GetMilliseconds() - start) > 3000) {
      mainprintf("APF file command %0.2x timed out\r\n", control);
      return (DATASLOT_FILE_TIMEOUT);
    }
  } while (!(apf_codes & APF_DONE));
  return (apf_codes & APF_ERROR);
}

// 0x0190: full path of the file in a data slot. path must hold DATASLOT_PATH_LEN bytes.
uint32_t dataslot_get_filename(uint16_t dataslot, char *path)
{
  uint32_t result = dataslot_file_command(dataslot, APF_ADDRESS_OFFSET | (uint32_t)dataslot_path_buffer, TARGET_DATASLOT_GETFILE_REG);
  // The bridge stores bytes in memory order, so the words read back as a plain string
  const char *src = (const char *)dataslot_path_buffer;
  int i = 0;
  if (result == 0) {
    while (i < DATASLOT_PATH_LEN - 1 && src[i]) {
      path[i] = src[i];
      i++;
    }
  }
  path[i] = 0;
  mainprintf("APF get file name %d: %d %s\r\n", dataslot, result, path);
  return (result);
}

// 0x0192: open another file into a data slot (no create, no resize)
uint32_t dataslot_open_file(uint16_t dataslot, const char *path)
{
  int len = 0;
  while (len < DATASLOT_PATH_LEN - 1 && path[len]) len++;
#if DATASLOT_OPENFILE_IN_DATATABLE
  // Bridge words are big endian: the first character goes in bits 31:24
  for (int w = 0; w < DATASLOT_PATH_LEN / 4; w++) {
    uint32_t word = 0;
    for (int b = 0; b < 4; b++) {
      int n = (w << 2) + b;
      uint32_t c = (n < len) ? (uint8_t)path[n] : 0;
      word |= c << (24 - (b << 3));
    }
    DATASLOT_RAM_ACCESS(DATASLOT_OPENFILE_TABLE_OFFSET + (w << 2)) = word;
  }
  DATASLOT_RAM_ACCESS(DATASLOT_OPENFILE_TABLE_OFFSET + DATASLOT_PATH_LEN) = 0;     // flags: open only
  DATASLOT_RAM_ACCESS(DATASLOT_OPENFILE_TABLE_OFFSET + DATASLOT_PATH_LEN + 4) = 0; // size: unused
  uint32_t address = DATASLOT_TABLE_BRIDGE_BASE + DATASLOT_OPENFILE_TABLE_OFFSET;
#else
  char *dst = (char *)dataslot_open_struct;
  for (int n = 0; n < DATASLOT_PATH_LEN; n++) dst[n] = (n < len) ? path[n] : 0;
  dataslot_open_struct[DATASLOT_PATH_LEN / 4] = 0;
  dataslot_open_struct[DATASLOT_PATH_LEN / 4 + 1] = 0;
  uint32_t address = APF_ADDRESS_OFFSET | (uint32_t)dataslot_open_struct;
#endif
  uint32_t result = dataslot_file_command(dataslot, address, TARGET_DATASLOT_OPENFILE_REG);
  mainprintf("APF open file %d: %d %s\r\n", dataslot, result, path);
  return (result);
}
