#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include "dfrobot_i2c_rgbbutton.h"

namespace esphome {
namespace dfrobot_i2c_rgbbutton {

static const char *TAG = "dfrobot_i2c_rgbbutton";

static const uint16_t RGBBUTTON_PART_ID = 0x43DF;

/* RGBButton register address */
static const uint8_t RGBBUTTON_I2C_ADDR_REG = 0x00;

static const uint8_t RGBBUTTON_RED_REG = 0x01;
static const uint8_t RGBBUTTON_GREEN_REG = 0x02;
static const uint8_t RGBBUTTON_BLUE_REG = 0x03;

static const uint8_t RGBBUTTON_BUTTON_SIGNAL_REG = 0x04;

static const uint8_t RGBBUTTON_PID_MSB_REG = 0x09;

float DFRobot_i2c_RGBButton::get_setup_priority() const { return setup_priority::IO; } // for ESPHome

void DFRobot_i2c_RGBButton::setup() { // triggers at startup
  ESP_LOGI(TAG, "Setting up i2c communication...");

  if(!begin()){ // initialize connection
    ESP_LOGE(TAG, "NO device found with address %d!", this->address_);
    ESP_LOGE(TAG, "Setup failed!");
    this->mark_failed(); // mark component as failed if device is not reacting
    return;
  }

  uint16_t productID = get_pid();
  ESP_LOGI(TAG, "Device with address %d connected successfully!", this->address_);
  ESP_LOGI(TAG, "  Product ID: %d", productID);

  set_button_color(this->default_color_, true); // set default button color
}

void DFRobot_i2c_RGBButton::loop() { } // triggers every clock cycle

void DFRobot_i2c_RGBButton::update() { // triggers on update_interval
  get_button_state(); // get button state
}

void DFRobot_i2c_RGBButton::set_button_color(uint8_t r, uint8_t g, uint8_t b, bool force) { // set button color via r, g, b values
  uint8_t rgbBuf[3] = {r, g, b};

  if (!force && std::memcmp(rgbBuf, this->button_color_, 3) == 0) { // checks if the color is already applied
    ESP_LOGD(TAG, "Color hasn't changed, no changes applied.");
    return;
  }

  // set color and store the rgb values
  this->write_register(RGBBUTTON_RED_REG, rgbBuf, 3);
  std::copy(rgbBuf, rgbBuf + 3, this->button_color_);

  // ESP_LOGD(TAG, "New color set:");
  // ESP_LOGD(TAG, "  R: %d, G: %d, B: %d", this->button_color_[0], this->button_color_[1], this->button_color_[2]);
}

void DFRobot_i2c_RGBButton::set_button_color(unsigned long color, bool force) {
  // convert hex to RGB
  uint8_t rgbBuf[3] = {
      static_cast<uint8_t>((color >> 16) & 0xFF),
      static_cast<uint8_t>((color >> 8) & 0xFF),
      static_cast<uint8_t>(color & 0xFF)

  };

  set_button_color(rgbBuf[0], rgbBuf[1], rgbBuf[2], force);
}

void DFRobot_i2c_RGBButton::set_button_color(DFRobot_i2c_RGBButton::eGeneralRGBValue_t color, bool force) {
  // convert hex to RGB
  uint8_t rgbBuf[3] = {
      static_cast<uint8_t>((color >> 16) & 0xFF),
      static_cast<uint8_t>((color >> 8) & 0xFF),
      static_cast<uint8_t>(color & 0xFF)

  };

  set_button_color(rgbBuf[0], rgbBuf[1], rgbBuf[2], force);
}

uint8_t* DFRobot_i2c_RGBButton::get_button_color_rgb() { // get button color as array (R, G, B)
  return this->button_color_;
}

unsigned long DFRobot_i2c_RGBButton::get_button_color_hex() { // get button color as hex value (unsigned long)
  return ((this->button_color_[0] & 0xff) << 16) + ((this->button_color_[1] & 0xff) << 8) + (this->button_color_[2] & 0xff);
}

bool DFRobot_i2c_RGBButton::get_button_state() { // get button state
  uint8_t buttonStatus;
  this->read_register(RGBBUTTON_BUTTON_SIGNAL_REG, &buttonStatus, 1);
  this->button_state_ = buttonStatus ? true : false;

  // publish binary sensor state
  if (this->button_sensor_ != nullptr) { // checks if binary_sensor is setup in configuration
    if (!this->button_sensor_->has_state() || (this->button_sensor_->state != this->button_state_)) { // checks if button's last state has no or another state
      if (this->button_state_) { // button pressed
        ESP_LOGD(TAG, "Button pressed!");
        this->button_sensor_->publish_state(this->button_state_);
      } else { // button released
        ESP_LOGD(TAG, "Button released!");
        this->button_sensor_->publish_state(this->button_state_);
      }
    }
  }

  return this->button_state_;
}

bool DFRobot_i2c_RGBButton::begin() {
  uint8_t idBuf[2];
  if(i2c::ERROR_OK != this->read_register(RGBBUTTON_PID_MSB_REG, idBuf, sizeof(idBuf))) {   // judge whether the data bus is successful
    return false;
  }

  if(RGBBUTTON_PART_ID != concat_bytes(idBuf[0], idBuf[1])) {   // judge whether the chip version matches
    ESP_LOGD(TAG, "Chip Version does not match!");
    return false;
  }

  return true;
}

uint8_t DFRobot_i2c_RGBButton::get_i2c_address() {
  uint8_t temp, i2cAddr;
  temp = this->deviceAddr_;
  deviceAddr_ = 0;   // common access address of I2C protocol
  this->read_register(RGBBUTTON_I2C_ADDR_REG, &i2cAddr, 1);
  this->deviceAddr_ = temp;
  return i2cAddr;
}

uint16_t DFRobot_i2c_RGBButton::get_pid() {
  uint8_t pidBuf[2];
  this->read_register(RGBBUTTON_PID_MSB_REG, pidBuf, sizeof(pidBuf));
  return concat_bytes(pidBuf[0], pidBuf[1]);
}

uint16_t DFRobot_i2c_RGBButton::concat_bytes(uint8_t msb, uint8_t lsb) {
  return (((uint16_t)msb << 8) | (uint16_t)lsb);
}

}  // namespace dfrobot_i2c_rgbbutton
}  // namespace esphome
