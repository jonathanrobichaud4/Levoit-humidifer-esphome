#include "levoit.h"
#include "esphome/components/network/util.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

namespace esphome {
namespace levoit {

static const char *const TAG = "levoit";
static const int COMMAND_DELAY = 100;  // 50ms seems too low at least on the 400s; lots of NACK errors; 100ms is better
static const int RECEIVE_TIMEOUT = 100;
static const int MAX_RETRIES = 5;

void Levoit::setup() {
  ESP_LOGI(TAG, "Setting up Levoit %s", device_model_ == LevoitDeviceModel::CORE_300S ? "Core 300S" : "Core 400S");

  this->set_interval("heartbeat", 15000, [this] {
    ESP_LOGV(TAG, "Sending heartbeat");
    LevoitCommand statusRequest = {.payloadType = LevoitPayloadType::STATUS_REQUEST,
                                   .packetType = LevoitPacketType::SEND_MESSAGE,
                                   .payload = {0x00}};
    this->send_command(statusRequest);
  });

  // this seems to introduce significant message unreliability on the 400s;
  // we must be approaching mcu capacity
  // TODO: ideally there is some way in the protocol to determine wifi light status,
  // which would allow us to only send commands if needed
  if (this->device_model_ != LevoitDeviceModel::CORE_400S) {
    this->set_interval("status", 5000, [this] {
      if (network::is_connected()) {
        if (remote_is_connected()) {
          // solid
          LevoitCommand statusRequest = {.payloadType = LevoitPayloadType::SET_WIFI_STATUS_LED,
                                         .packetType = LevoitPacketType::SEND_MESSAGE,
                                         .payload = {0x00, 0x01, 0x7D, 0x00, 0x7D, 0x00, 0x00}};
          this->send_command(statusRequest);
        } else {
          // blink
          LevoitCommand statusRequest = {.payloadType = LevoitPayloadType::SET_WIFI_STATUS_LED,
                                         .packetType = LevoitPacketType::SEND_MESSAGE,
                                         .payload = {0x00, 0x02, 0xF4, 0x01, 0xF4, 0x01, 0x00}};
          this->send_command(statusRequest);
        }
      } else {
        // off
        LevoitCommand statusRequest = {.payloadType = LevoitPayloadType::SET_WIFI_STATUS_LED,
                                       .packetType = LevoitPacketType::SEND_MESSAGE,
                                       .payload = {0x00, 0x00, 0xF4, 0x01, 0xF4, 0x01, 0x00}};
        this->send_command(statusRequest);
      }
    });
  }
}

void Levoit::loop() {
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    this->handle_char_(c);
  }
  process_command_queue_();
}

void Levoit::dump_config() { ESP_LOGCONFIG(TAG, "Levoit!"); }

bool Levoit::validate_message_() {
  uint32_t at = this->rx_message_.size() - 1;
  auto *data = &this->rx_message_[0];
  uint8_t new_byte = data[at];

  if (at == 0)
    return new_byte == 0xA5;

  if (at == 1) {
    if (new_byte == 0x52) {
      ESP_LOGE(TAG, "Received error response, ignoring packet");
      return false;
    }
    return (new_byte == 0x12) || (new_byte == 0x22);
  }

  uint8_t sequenceNumber = data[2];
  if (at == 2)
    return true;

  uint8_t payloadLength = data[3];
  if (at == 3) {
    return true;
  }

  if (at == 4)
    return (new_byte == 0x00);

  uint8_t payloadChecksum = data[5];
  if (at == 5) {
    return true;
  }

  if (at - 5 < payloadLength) {
    return true;
  }

  uint8_t calc_checksum = 255;
  for (uint8_t i = 0; i < 6 + payloadLength; i++) {
    if (i != 5) {
      calc_checksum -= data[i];
    }
  }

  if (payloadChecksum != calc_checksum) {
    ESP_LOGE(TAG, "Received invalid message checksum, ignoring packet");
    return false;
  }

  // valid message
  const uint8_t *message_data = data + 6;

  LevoitPayloadType payloadType =
      (LevoitPayloadType) (message_data[2] | (message_data[1] << 8) | (message_data[0] << 16) | (0x00 << 24));

  uint8_t *payload_data = data + 9;

  // If it's not a 1-byte ACK response, handle the payload.
  if (data[1] != 0x12 || payloadLength - 3 != 1) {
    this->handle_payload_(payloadType, sequenceNumber, payload_data, payloadLength - 3);
  } else if (data[1] == 0x12) {
    ESP_LOGV(TAG, "Received ACK (%06x)", (uint32_t) payloadType);
  }

  // acknowledge packet if required
  if (data[1] == 0x22) {
    LevoitCommand acknowledgeResponse = {
        .payloadType = payloadType, .packetType = LevoitPacketType::ACK_MESSAGE, .payload = {0x00}};
    this->send_raw_command(acknowledgeResponse);
  } else if (data[1] == 0x12) {
    lastCommandAcked = true;
  }

  this->sequenceNumber = sequenceNumber + 1;

  // return false to reset rx buffer
  return false;
}

void Levoit::handle_char_(uint8_t c) {
  this->rx_message_.push_back(c);
  if (!this->validate_message_()) {
    this->rx_message_.clear();
  } else {
    this->last_rx_char_timestamp_ = millis();
  }
}

void Levoit::handle_payload_(LevoitPayloadType type, uint8_t sequenceNumber, uint8_t *payload, size_t len) {
  ESP_LOGV(TAG, "Received command (%06x): %s", (uint32_t) type, format_hex_pretty(payload, len).c_str());
  // Run through listeners
  for (auto &listener : this->listeners_) {
    if (listener.type == static_cast<LevoitPayloadType>(get_model_specific_payload_type(type)))
      listener.func(payload, len);
  }
}

void Levoit::register_listener(LevoitPayloadType payloadType,
                               const std::function<void(uint8_t *buf, size_t len)> &func) {
  auto listener = LevoitListener{
      .type = static_cast<LevoitPayloadType>(get_model_specific_payload_type(payloadType)),
      .func = func,
  };
  this->listeners_.push_back(listener);
}

void Levoit::send_raw_command(LevoitCommand command) {
  this->last_command_timestamp_ = millis();

  sequenceNumber++;

  uint8_t payloadTypeByte1 = ((uint32_t) command.payloadType >> 16) & 0xff;
  uint8_t payloadTypeByte2 = ((uint32_t) command.payloadType >> 8) & 0xff;
  uint8_t payloadTypeByte3 = (uint32_t) command.payloadType & 0xff;

  // Initialize the outgoing packet
  std::vector<uint8_t> rawPacket = {0xA5,
                                    (uint8_t) command.packetType,
                                    sequenceNumber,
                                    (uint8_t) (command.payload.size() + 3),
                                    0x00,
                                    0x00,
                                    payloadTypeByte1,
                                    payloadTypeByte2,
                                    payloadTypeByte3};

  if (!command.payload.empty())
    rawPacket.insert(rawPacket.end(), command.payload.begin(), command.payload.end());

  // Calculate checksum & insert into packet
  uint8_t checksum = 255;
  for (uint8_t i = 0; i < rawPacket.size(); i++) {
    if (i != 5) {
      checksum -= rawPacket[i];
    }
  }
  rawPacket[5] = checksum;

  const char *packetTypeStr = (command.packetType == LevoitPacketType::ACK_MESSAGE)
                                  ? "ACK"
                                  : ((command.packetType == LevoitPacketType::SEND_MESSAGE) ? "CMD" : "UNKNOWN");
  ESP_LOGV(TAG, "Sending %s (%06x): %s", packetTypeStr, (uint32_t) command.payloadType,
           format_hex_pretty(rawPacket.data(), rawPacket.size()).c_str());
  this->write_array(rawPacket);
}

void Levoit::process_command_queue_() {
  uint32_t now = millis();
  uint32_t delay = now - this->last_command_timestamp_;

  if (now - this->last_rx_char_timestamp_ > RECEIVE_TIMEOUT && !this->rx_message_.empty()) {
    ESP_LOGW(TAG, "No response from U2, clearing buffer");
    this->rx_message_.clear();
  }

  if (lastCommandAcked) {
    this->command_queue_.erase(command_queue_.begin());
    lastCommandAcked = false;
    lastCommandRetries = 0;
  }

  // Left check of delay since last command in case there's ever a command sent by calling send_raw_command_ directly
  if (delay > COMMAND_DELAY && !this->command_queue_.empty() && this->rx_message_.empty()) {
    if (lastCommandRetries < MAX_RETRIES) {
      if (lastCommandRetries > 0) {
        ESP_LOGW(TAG, "ACK response timeout, retry #%d", lastCommandRetries);
      }
      this->send_raw_command(command_queue_.front());
      lastCommandRetries++;
    } else {
      ESP_LOGE(TAG, "Maximum NACKs received, skipping packet");
      this->command_queue_.erase(command_queue_.begin());
      lastCommandAcked = false;
      lastCommandRetries = 0;
    }
  }
}

void Levoit::send_command(const LevoitCommand &command) {
  auto modified_command = command;
  modified_command.payloadType = static_cast<LevoitPayloadType>(get_model_specific_payload_type(command.payloadType));
  command_queue_.push_back(modified_command);
  process_command_queue_();
}

void Levoit::set_device_model(std::string model) {
  if (model == "core300s") {
    device_model_ = LevoitDeviceModel::CORE_300S;
  } else if (model == "core400s") {
    device_model_ = LevoitDeviceModel::CORE_400S;
  } else {
    ESP_LOGW(TAG, "Unknown device model: %s", model.c_str());
  }
}

uint32_t Levoit::get_model_specific_payload_type(LevoitPayloadType type) {
  auto model_itr = MODEL_SPECIFIC_PAYLOAD_TYPES.find(device_model_);
  if (model_itr != MODEL_SPECIFIC_PAYLOAD_TYPES.end()) {
    auto payload_itr = model_itr->second.find(type);
    if (payload_itr != model_itr->second.end()) {
      return payload_itr->second;
    }
  }
  // If no override is found, return the default payload
  return static_cast<uint32_t>(type);
}

}  // namespace levoit
}  // namespace esphome
