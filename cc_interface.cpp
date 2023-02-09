// modified from https://github.com/atc1441/ESP_CC_Flasher
#include "cc_interface.h"
#include "pico/stdlib.h"


void CC_interface::begin(uint8_t CC, uint8_t DD, uint8_t RESET)
{
  _CC_PIN = CC;
  _DD_PIN = DD;
  _RESET_PIN = RESET;
  gpio_init(_CC_PIN);
  gpio_init(_DD_PIN);
  gpio_init(_RESET_PIN);

  gpio_set_dir(_CC_PIN, GPIO_OUT);
  gpio_set_dir(_DD_PIN, GPIO_OUT);
  gpio_set_dir(_RESET_PIN, GPIO_OUT);
  
  dd_direction = 0;
  gpio_put(_DD_PIN, 1);

  gpio_put(_CC_PIN, 0);
  gpio_put(_RESET_PIN, 1);  
}

uint16_t CC_interface::get_serial_number() {
  uint16_t device_id_answer = send_cc_cmd(0x68);
  return device_id_answer;
}

void CC_interface::set_ignore_mode() {
  gpio_set_dir(_DD_PIN, GPIO_IN);
  gpio_pull_up(_DD_PIN);
  dd_direction = 1;
}

void CC_interface::set_write_mode() {
  gpio_disable_pulls(_DD_PIN);
  gpio_set_dir(_DD_PIN, GPIO_OUT);
  dd_direction = 0;
}

void CC_interface::set_read_mode() {
  gpio_set_dir(_DD_PIN, GPIO_IN);
  gpio_disable_pulls(_DD_PIN);
  dd_direction = 1;
}

uint8_t CC_interface::opcode(uint8_t opCode)
{
  cc_send_byte(0x55);
  cc_send_byte(opCode);
  return cc_receive_byte();
}

uint8_t CC_interface::opcode(uint8_t opCode, uint8_t opCode1)
{
  cc_send_byte(0x56);
  cc_send_byte(opCode);
  cc_send_byte(opCode1);
  return cc_receive_byte();
}

uint8_t CC_interface::opcode(uint8_t opCode, uint8_t opCode1, uint8_t opCode2)
{
  cc_send_byte(0x57);
  cc_send_byte(opCode);
  cc_send_byte(opCode1);
  cc_send_byte(opCode2);
  return cc_receive_byte();
}

uint8_t CC_interface::send_cc_cmdS(uint8_t cmd)
{
  cc_send_byte(cmd);
  return cc_receive_byte();
}

uint16_t CC_interface::send_cc_cmd(uint8_t cmd)
{
  cc_send_byte(cmd);
  return (cc_receive_byte() << 8) + cc_receive_byte();
}

void CC_interface::write_xdata_memory(uint16_t address, uint16_t len, uint8_t buffer[])
{
  opcode(0x90, address >> 8, address);
  for (int i = 0; i < len; i++)
  {
    opcode(0x74, buffer[i]);
    opcode(0xf0);
    opcode(0xa3);
  }
}

uint8_t CC_interface::set_lock_byte(uint8_t lock_byte)
{
  opcode(0x75, 0xc6, 0x00); // write all 0 to clock register
  uint64_t timeout = (time_us_64()/1000) + 100;
  while (!(opcode(0xe5, 0xbe) & 0x40)) // mov	a,0beh
  {
    if ((time_us_64()/1000) > timeout)
    {
      return 1;
    }
  }
  lock_byte = lock_byte & 0x1f; // Max lock byte value
  WR_CONFIG(0x01);              // Select flash info Page
  opcode(0x00);                 // NOP

  opcode(0xE5, 0x92); // direct byte 0x92 to acc
  opcode(0x75, 0x92, 0x00); // move 0 to 0x92, use dptr 0
  opcode(0xE5, 0x83); // direct  0x83 to acc, dptr high byte to acc
  opcode(0xE5, 0x82); // direct 0x82 to acc, dptr low byte to acc

  opcode(0x90, 0xF0, 0x00); // #0xF000 to dptr
  opcode(0x74, 0xFF); // set acc to 0xFF
  opcode(0xF0); // MOVX @DPTR,A: move a to external ram
  opcode(0xA3);            // Increase data pointer
  opcode(0x74, lock_byte); // Transmit the set lock byte
  opcode(0xF0); // MOVX @DPTR,A: move a to external ram
  opcode(0xA3); // Increase Pointer
  opcode(0x90, 0x00, 0x00); // 0000 to data pointer
  opcode(0x75, 0x92, 0x00); // move 0 to 0x92, use dptr 0
  opcode(0x74, 0x00); // set acc to 0

  opcode(0x00); // NOP

  opcode(0xE5, 0x92); // direct byte 0x92 to acc (nop)
  opcode(0x75, 0x92, 0x00); // move 0 to 0x92 (use dptr 0)
  opcode(0xE5, 0x83); // direct 0x83 to acc
  opcode(0xE5, 0x82); // direct 0x82 to acc
 
  opcode(0x90, 0xF8, 0x00); // 0xF800 to dptr
  opcode(0x74, 0xF0); // set acc to f0 (b register)
  opcode(0xF0);  // MOVX @DPTR,A: move a to external ram
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0x00); // set acc to 0
  opcode(0xF0); // MOVX @DPTR,A: move a to external ram
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0xDF); // set acc to 0xDF
  opcode(0xF0); // MOVX @DPTR,A: move a to external ram
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0xAF);
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0x00);
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0x02);
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0x12);
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  opcode(0x74, 0x4A);
  opcode(0xF0);
  opcode(0xA3); // Increase Pointer
  
  opcode(0x90, 0x00, 0x00); // 0000 to data pointer
  opcode(0x75, 0x92, 0x00); // 0 to 0x92
  opcode(0x74, 0x00); // set acc to 0

  opcode(0x00); // NOP

  opcode(0xE5, 0xC6); // direct c6 to acc
  opcode(0x74, 0x00); // set acc to 0
  opcode(0x75, 0xAB, 0x23); // 23 to flash write timing
  opcode(0x75, 0xD5, 0xF8); // DMA Channel 0 Configuration Address High
  opcode(0x75, 0xD4, 0x00); // DMA Channel 0 Configuration Address Low
  opcode(0x75, 0xD6, 0x01); // DMA Channel Arm
  opcode(0x75, 0xAD, 0x00); // Flash Address High
  opcode(0x75, 0xAC, 0x00); // Flash Address Low
  opcode(0x75, 0xAE, 0x02); // write to flash control

  opcode(0x00); // NOP

  opcode(0xE5, 0xAE); // direct AE to acc
  opcode(0x74, 0x00); // set acc to 0

  return WR_CONFIG(0x00); // Select normal flash page
}

uint8_t CC_interface::WR_CONFIG(uint8_t config)
{
  cc_send_byte(0x1d);
  cc_send_byte(config);
  return cc_receive_byte();
}

uint8_t CC_interface::WD_CONFIG()
{
  cc_send_byte(0x24);
  return cc_receive_byte();
}

void CC_interface::set_pc(uint16_t address)
{
  opcode(0x02, address >> 8, address);
}



uint8_t CC_interface::write_code_memory(uint16_t address, uint8_t buffer[], int len)
{

  opcode(0x75, 0xc6, 0x00); // write all 0 to clock register
  uint64_t timeout = (time_us_64()/1000) + 100;
  while (!(opcode(0xe5, 0xbe) & 0x40)) // mov	a,0beh
  {
    if ((time_us_64()/1000) > timeout)
    {
      return 1;
    }
  }

  int entry_len = len;
  if (len % 2 != 0)
    len++;
  int position = 0;
  int len_per_transfer = 64;
  address = address / 2;
  while (len)
  {
    flash_opcode[2] = (address >> 8) & 0xff;
    flash_opcode[5] = address & 0xff;
    flash_opcode[13] = (len > len_per_transfer) ? (len_per_transfer / 2) : (len / 2);
    write_xdata_memory(0xf000, len_per_transfer, &buffer[position]);
    write_xdata_memory(0xf100, sizeof(flash_opcode), flash_opcode);
    opcode(0x75, 0xC7, 0x51); // move #0x51 to 0xC7
    set_pc(0xf100);
    send_cc_cmdS(0x4c);
    uint64_t timeout = (time_us_64()/1000) + 500;
    while (!(send_cc_cmdS(0x34) & 0x08))
    {
      if (((time_us_64()/1000)) > timeout)
      {
        return 1;
      }
    }
    len -= flash_opcode[13] * 2;
    position += flash_opcode[13] * 2;
    address += flash_opcode[13];
  }
  return 0;
}


// data is sent to chip on positive edge of the clock
void CC_interface::cc_send_byte(uint8_t in_byte)
{
  set_write_mode();
  for (int i = 8; i; i--)
  {
    gpio_put(_DD_PIN, (in_byte >> 7));
    gpio_put(_CC_PIN, 1);
    in_byte <<= 1;
    busy_wait_us(5); // 1 has worked
    gpio_put(_CC_PIN, 0);
    busy_wait_us(5); // 1 has worked
  }
}

void CC_interface::read_xdata_memory(uint16_t address, uint16_t len, uint8_t buffer[])
{
  opcode(0x90, address >> 8, address); // mov DPTR, #address
  for (int i = 0; i < len; i++)
  {
    buffer[i] = opcode(0xe0); // MOVX A,@DPTR
    opcode(0xa3); // inc DPTR
  }
}


void CC_interface::read_code_memory(uint16_t address, uint16_t len, uint8_t buffer[])
{
  opcode(0x75, 0xc7, 0x01); // mov	0c7h,#1 (disable flash prefetch)
  opcode(0x90, address >> 8, address); // mov DPTR, #address
  for (int i = 0; i < len; i++)
  {
    opcode(0xe4); // clr A
    buffer[i] = opcode(0x93); // movc A,@A+DPTR
    opcode(0xa3); // inc DPTR
  }
}


// data is read from chip on negative edge of the clock
uint8_t CC_interface::cc_receive_byte()
{
  uint8_t out_byte = 0x00;
  set_read_mode();
  for (int i = 8; i; i--)
  {
    gpio_put(_CC_PIN, 1);
    busy_wait_us(5); // 1 has worked
    out_byte <<= 1;
    out_byte |= ((sio_hw->gpio_in >> _DD_PIN) & 1);
    gpio_put(_CC_PIN, 0);
    busy_wait_us(5); // 1 has worked
  }
  return out_byte;
}

void CC_interface::enable_cc_debug()
{
  set_ignore_mode();
  gpio_put(_RESET_PIN, 0);
  busy_wait_us(10); //10 has worked
  gpio_put(_CC_PIN, 1);
  busy_wait_us(5);
  gpio_put(_CC_PIN, 0);
  busy_wait_us(5);
  gpio_put(_CC_PIN, 1);
  busy_wait_us(5);
  gpio_put(_CC_PIN, 0);
  busy_wait_us(10); // 10 has worked
  gpio_put(_RESET_PIN, 1);
  busy_wait_us(10); // 10 has worked
}

void CC_interface::reset_cc()
{
  set_ignore_mode();
  gpio_put(_RESET_PIN, 0);
  busy_wait_us(5); // was 5
  gpio_put(_RESET_PIN, 1);
  busy_wait_us(5); // added
}

CC_interface cc;