#include "Appliance/AirConditioner/StatusData.h"

namespace dudanov {
namespace midea {
namespace ac {

float StatusData::getTargetTemp() const {
  uint8_t tmp = this->m_getValue(2, 15) + 16;
  uint8_t tmpNew = this->m_getValue(13, 31);
  if (tmpNew)
    tmp = tmpNew + 12;
  float temp = static_cast<float>(tmp);
  if (this->m_getValue(2, 16))
    temp += 0.5F;
  return temp;
}

void StatusData::setTargetTemp(float temp) {
  uint8_t tmp = static_cast<uint8_t>(temp * 4.0F) + 1;
  uint8_t integer = tmp / 4;
  this->m_setValue(18, integer - 12, 31);
  integer -= 16;
  if (integer < 1 || integer > 14)
    integer = 1;
  this->m_setValue(2, ((tmp & 2) << 3) | integer, 31);
}

static float getTemp(int integer, int decimal, bool fahrenheits) {
  integer -= 50;
  if (!fahrenheits && decimal > 0)
    return static_cast<float>(integer / 2) + static_cast<float>(decimal) * ((integer >= 0) ? 0.1F : -0.1F);
  if (decimal >= 5)
    return static_cast<float>(integer / 2) + ((integer >= 0) ? 0.5F : -0.5F);
  return static_cast<float>(integer) * 0.5F;
}
float StatusData::getIndoorTemp() const { return getTemp(this->m_getValue(11), this->m_getValue(15, 15), this->isFahrenheits()); }
float StatusData::getOutdoorTemp() const { return getTemp(this->m_getValue(12), this->m_getValue(15, 15, 4), this->isFahrenheits()); }
float StatusData::getHumiditySetpoint() const { return static_cast<float>(this->m_getValue(19, 127)); }

Mode StatusData::getMode() const { return this->m_getPower() ? this->getRawMode() : Mode::MODE_OFF; }

void StatusData::setMode(Mode mode) {
  if (mode != Mode::MODE_OFF) {
    this->m_setPower(true);
    this->m_setValue(2, mode, 7, 5);
  } else {
    this->m_setPower(false);
  }
}

FanMode StatusData::getFanMode() const {
  //some ACs return 30 for LOW and 50 for MEDIUM. Note though, in appMode, this device still uses 40/60
  uint8_t fanMode = this->m_getValue(3);
  if (fanMode == 30) {
    fanMode = FAN_LOW;
  } else if (fanMode == 50) {
    fanMode = FAN_MEDIUM;
  }
  return static_cast<FanMode>(fanMode); 
}

Preset StatusData::getPreset() const {
  if (this->m_getEco())
    return Preset::PRESET_ECO;
  if (this->m_getTurbo())
    return Preset::PRESET_TURBO;
  if (this->m_getSleep())
    return Preset::PRESET_SLEEP;
  if (this->m_getFreezeProtection())
    return Preset::PRESET_FREEZE_PROTECTION;
  return Preset::PRESET_NONE;
}

void StatusData::setPreset(Preset preset) {
  this->m_setEco(false);
  this->m_setSleep(false);
  this->m_setTurbo(false);
  this->m_setFreezeProtection(false);
  switch (preset) {
    case Preset::PRESET_NONE:
      break;
    case Preset::PRESET_ECO:
      this->m_setEco(true);
      break;
    case Preset::PRESET_TURBO:
      this->m_setTurbo(true);
      break;
    case Preset::PRESET_SLEEP:
      this->m_setSleep(true);
      break;
    case Preset::PRESET_FREEZE_PROTECTION:
      this->m_setFreezeProtection(true);
      break;
    default:
      break;
  }
}

static uint8_t bcd2u8(uint8_t bcd) { return 10 * (bcd >> 4) + (bcd & 15); }

float StatusData::getPowerUsage() const {
  uint32_t power = 0;
  const uint8_t *ptr = this->m_data.data() + 18;
  for (uint32_t weight = 1;; weight *= 100, --ptr) {
    power += weight * bcd2u8(*ptr);
    if (weight == 10000)
      return static_cast<float>(power) * 0.1F;
  }
}

// Some models report the real-time power field (bytes 16..18) as a plain
// big-endian binary integer in units of 0.1 W instead of BCD. Decoding such a
// frame as BCD reads far too low (e.g. 0x00,0x35,0x08 -> 350.8 W as BCD vs
// 1357.6 W as binary). See powerBytesLookBinary() for format detection.
float StatusData::getPowerUsageBinary() const {
  const uint8_t *d = this->m_data.data() + 16;  // bytes 16,17,18 (MSB..LSB)
  uint32_t raw = (uint32_t(d[0]) << 16) | (uint32_t(d[1]) << 8) | d[2];
  return static_cast<float>(raw) * 0.1F;
}

// A genuine BCD power field can only contain nibbles 0-9. If any nibble in the
// three power bytes is > 9, the frame cannot be BCD and must be binary.
bool StatusData::powerBytesLookBinary() const {
  const uint8_t *d = this->m_data.data() + 16;
  for (int i = 0; i < 3; i++)
    if ((d[i] >> 4) > 9 || (d[i] & 0x0F) > 9)
      return true;
  return false;
}

}  // namespace ac
}  // namespace midea
}  // namespace dudanov
