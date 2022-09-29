#include "esphome.h"
using namespace esphome;

static const char *const TAG = "SM";
static const int RECEIVE_TIMEOUT = 300;
static const int FRAME_INTERVAL = 200;                     // Active device reporting rate (i.e. frame interval) > 200mc
static const int SENDING_INTERVAL = 30000;                 // Settings reading cycles 
static const int IND_LED_INTERVAL = 100;

struct SmFrame {
  uint8_t type_frame;
  std::vector<uint8_t> payload;
 };

uint32_t sending_interval_reg = 30000;       
uint32_t last_command_timestamp = 0; 
uint32_t last_sending_timestamp = 0; 
uint32_t last_rx_char_timestamp = 0; 
uint32_t last_ind_led_timestamp = 0;

float _import_active_energy = 0;
float _export_active_energy = 0;
float _contract_active_energy = 0;
float _frequency = 0;
    
float _rms_voltage_phase_a = 0;
float _rms_current_phase_a = 0;
float _rms_active_power_phase_a = 0;
float _rms_reactive_power_phase_a = 0;
float _rms_power_factor_phase_a = 0;
    
float _rms_voltage_phase_b = 0;
float _rms_current_phase_b = 0;
float _rms_active_power_phase_b = 0;
float _rms_reactive_power_phase_b = 0;
float _rms_power_factor_phase_b = 0;

float _rms_voltage_phase_c = 0;
float _rms_current_phase_c = 0;
float _rms_active_power_phase_c = 0;
float _rms_reactive_power_phase_c = 0;
float _rms_power_factor_phase_c = 0;; 
    
float _rms_active_power_g = 0;
float _rms_reactive_power_g = 0;
float _rms_power_factor_g = 0;

float _voltage_high_limit = 0;
float _voltage_low_limit = 0;
float _current_high_limit = 0;
float _staring_kWh_value_of_metr = 0;
float _metr_price = 0; 

bool _prepayment_fuction_onoff = false;

uint8_t read_cycle_number = 13;
uint8_t frame_number_inp = 0;
bool timing = false;

class MyCustomSwitch : public Component, public Switch {
  public:
  
    int relayId;
  
    MyCustomSwitch(int relai) {relayId = relai;}
    
    void setup() override {
    
    }
    
    void write_state(bool state) override {

      //Switch - total sm metr reset (GENERAL RESET)
      if (relayId == 1) {
        read_cycle_number = 9;
        last_sending_timestamp = 0;
        frame_number_inp = 0;
        id(status_gpio14).turn_off();
      } 
     
      // Switch - power sm metr (ON/OFF).
      if (relayId == 2) {
        if (state) {
          read_cycle_number = 5;
        } else {
          read_cycle_number = 4;  
        }
        frame_number_inp = 0;
        last_sending_timestamp = 0;
        this->state = !state;
      }      

      // Switch - reset sm metr (RESET).
      if (relayId == 3) {
        read_cycle_number = 6;
        last_sending_timestamp = 0;
        frame_number_inp = 0;
        }
        
      // Switch - timing (TIMING)
      if (relayId == 4) {
        if (state) {
          read_cycle_number = 12;
          last_sending_timestamp = 0;
          timing = true;
        }
      }
      
      // Switch - save timing (SAVE)
      if (relayId == 5) {
        read_cycle_number = 12;
        last_sending_timestamp = 0;
      }
      
      // Switch - reset timing. (RESET) 
      if (relayId == 6) {
        read_cycle_number = 11;
        last_sending_timestamp = 0;
      }
      
      // Switch - setting.(SETTING)
      if (relayId == 7 && state) {
        read_cycle_number = 0;
        last_sending_timestamp = 0;
      }
      
      // Switch - preparation and storage of settings 
      // ( protect Vmax, Vmin, Imax; price kW; the starting kWh value of metr).(SAVE)
      if (relayId == 8) {
        read_cycle_number = 7;
        last_sending_timestamp = 0;
        frame_number_inp = 0;
      }       
      
      // Switch - preparation purchases: on/off
      if (relayId == 9) {
        read_cycle_number = 8; 
        last_sending_timestamp = 0;
        frame_number_inp = 0;
        id(save_prepayment).publish_state(true);
        _prepayment_fuction_onoff = state;
        state = !state;    
      } 
      
      // Switch - save preparation purchases.(SAVE)
      if (relayId == 10) {
        read_cycle_number = 8;
        last_sending_timestamp = 0;
        frame_number_inp = 0;
        _prepayment_fuction_onoff = id(prepayment_fuction_on_off).state;
      }      
      
      publish_state(state);
      
    }
};

class MyCustomSensor : public PollingComponent {
  public:

    MyCustomSensor() : PollingComponent(45000) { }

    Sensor *import_active_energy = new Sensor();
    Sensor *export_active_energy = new Sensor();
    Sensor *contract_active_energy = new Sensor();
    Sensor *frequency = new Sensor();
    
    Sensor *rms_voltage_phase_a = new Sensor();
    Sensor *rms_current_phase_a = new Sensor();
    Sensor *rms_active_power_phase_a = new Sensor();
    Sensor *rms_reactive_power_phase_a = new Sensor();
    Sensor *rms_power_factor_phase_a = new Sensor();
    
    Sensor *rms_voltage_phase_b = new Sensor();
    Sensor *rms_current_phase_b = new Sensor();
    Sensor *rms_active_power_phase_b = new Sensor();
    Sensor *rms_reactive_power_phase_b = new Sensor();
    Sensor *rms_power_factor_phase_b = new Sensor();

    Sensor *rms_voltage_phase_c = new Sensor();
    Sensor *rms_current_phase_c = new Sensor();
    Sensor *rms_active_power_phase_c = new Sensor();
    Sensor *rms_reactive_power_phase_c = new Sensor();
    Sensor *rms_power_factor_phase_c = new Sensor();    
    
    Sensor *rms_active_power_g = new Sensor();
    Sensor *rms_reactive_power_g = new Sensor();
    Sensor *rms_power_factor_g = new Sensor();

  void setup() override {

  }

  void update() override {
  
    import_active_energy->publish_state(_import_active_energy);
    export_active_energy->publish_state(_export_active_energy);
    contract_active_energy->publish_state(_contract_active_energy);
    frequency->publish_state(_frequency);
    
    rms_voltage_phase_a->publish_state(_rms_voltage_phase_a);
    rms_current_phase_a->publish_state(_rms_current_phase_a);
    rms_active_power_phase_a->publish_state(_rms_active_power_phase_a);
    rms_reactive_power_phase_a->publish_state(_rms_reactive_power_phase_a);
    rms_power_factor_phase_a->publish_state(_rms_power_factor_phase_a);
    
    rms_voltage_phase_b->publish_state(_rms_voltage_phase_b);
    rms_current_phase_b->publish_state(_rms_current_phase_b);
    rms_active_power_phase_b->publish_state(_rms_active_power_phase_b);
    rms_reactive_power_phase_b->publish_state(_rms_reactive_power_phase_b);
    rms_power_factor_phase_b->publish_state(_rms_power_factor_phase_b);
    
    rms_voltage_phase_c->publish_state(_rms_voltage_phase_c);
    rms_current_phase_c->publish_state(_rms_current_phase_c);
    rms_active_power_phase_c->publish_state(_rms_active_power_phase_c);
    rms_reactive_power_phase_c->publish_state(_rms_reactive_power_phase_c);
    rms_power_factor_phase_c->publish_state(_rms_power_factor_phase_c);
    
    rms_active_power_g->publish_state(_rms_active_power_g);
    rms_reactive_power_g->publish_state(_rms_reactive_power_g);
    rms_power_factor_g->publish_state(_rms_power_factor_g);
    
  }
};

class SMComponent : public Component, public UARTDevice {
  public:
    SMComponent(UARTComponent *parent) : UARTDevice(parent) {}
    
    void setup() override {
      id(smtime).publish_state(false);
      id(timing_info).publish_state(false);
      id(data_time_onoff).publish_state("OFF");
    }
    
    void loop() override {
      while (this->available()) {
        uint8_t c;
        this->read_byte(&c);
        if(read_cycle_number != 13) 
          this->handle_char_(c);
      }
      process_command_queue_();
    }
    
  protected:
    std::vector<uint8_t> rx_message_;
    std::vector<SmFrame> command_queue_;
    optional<uint8_t> expected_response_;
    uint8_t frame_number_ = 0;
    int i = 0;
    
    void handle_char_(uint8_t c) { 
      this->rx_message_.push_back(c);             
      if (!this->validate_message_()) {
        this->rx_message_.clear();
      } else {
          last_rx_char_timestamp = millis();
      }
    }

    bool validate_message_() {
      uint8_t at = this->rx_message_.size() - 1;
      auto *data = &this->rx_message_[0];
      uint8_t new_byte = data[at];

      // Byte 0: HEADER (always 0x48)
      if (at == 0) {
        if (new_byte == 0x48) {
          return true;
        } else {
            return false;
        }
      } 
        
      // Byte 1: Frame length (0x06 <= length <= 0xFE)
      uint8_t length = data[1];
      if (at == 1) {
        if (0x06 <= length && length <= 0xFE ) {
          return true;
        } else {
            return false;
        }
      } 
        
      // 0x01 device report frame Device --> Module --> APP Module, return original frame
      // 0x02 Module sends frame APP-->Module-->Device Device, return original frame
      // 0xFE Module working frame Device --> Module module, depending on frame command
      // 0xFF error frame module <——> device nobody
      uint8_t type = data[2];
      if (at == 2) {
        if (type == 0x01 || type == 0x02 || type == 0xFE || type == 0XFF) {
          return true;
        } else {
            return false;
        }  
      } 
       
      // frame number    
      uint8_t number = data[3];
      if (at == 3)
        return true;
     
      // wait until all data is read
      if (at < length - 1) {
        return true;            // no validation for these fields
      }
      
      // CHECKSUM - sum of all bytes (including header) modulo 256 
      uint8_t rx_checksum = new_byte;
      uint8_t calc_checksum = 0;
      for (uint8_t i = 0; i < length - 1; i++)
        calc_checksum += data[i];
 
      if (rx_checksum != calc_checksum){
        ESP_LOGE(TAG, "Invalid message checksum %02X!=%02X FRAME=[%s]", rx_checksum, calc_checksum, format_hex_pretty(rx_message_).c_str());
        this->rx_message_={0x48, 0x07, 0xFF, this->frame_number_, 0x02, 0x00, (uint8_t)(0x48+0x07+0xFF+this->frame_number_+0x02)};
        this->frame_number_ +=1;
        ESP_LOGE(TAG, "Sending an error frame to the device: TYPE=0xFF FRAME=[%s]", format_hex_pretty(rx_message_).c_str());
        this->send_source_frame_(SmFrame {.type_frame = 0xFF, .payload = {rx_message_}}); // code validation error
        return false;  // return false to reset rx buffer                                                                                
      }                           

      ESP_LOGI(TAG, "Received: TYPE=0x%02X FRAME=[%s]", type, format_hex_pretty(rx_message_).c_str());
      
      if (type == 0x02) {
        this->frame_number_ +=1;
        sending_interval_reg = 1000;
        return false;   // return false to reset rx buffer
      }
      
      this->send_source_frame_(SmFrame {.type_frame = 0x01, .payload = rx_message_});  
      // valid message
      const uint8_t *message_data = data; 
      
      // send input frame     
      if (type == 0x01) { 
   
        this->handle_command_( type, message_data);
        frame_number_inp +=1;
        if (frame_number_inp > 2 ) {
          sending_interval_reg = SENDING_INTERVAL;
        } else {
          sending_interval_reg = 1000; 
        }
      }    
      return false;    // return false to reset rx buffer
      
    }
    
    // process command queue
    void process_command_queue_() {
      uint32_t now = millis();
      uint32_t delay = now - last_command_timestamp;
      
      if (!id(sm_metr_satatus).state && !id(total_reset).state) {
        if (now - last_ind_led_timestamp > IND_LED_INTERVAL) {
          last_ind_led_timestamp = millis();
          if (i == 0) {
            id(status_gpio14).turn_off();
            i = 1;
          } else {
            id(status_gpio14).turn_on();
            i = 0;
          }
        }        
      }
      

      if (now - last_rx_char_timestamp > RECEIVE_TIMEOUT) 
        this->rx_message_.clear();
      
      if (this->expected_response_.has_value() && delay > FRAME_INTERVAL) 
        this->expected_response_.reset(); 
      
         
      if (!this->command_queue_.empty() &&  !this->expected_response_.has_value()){ 
        this->send_frame_(command_queue_.front());
        this->command_queue_.erase(command_queue_.begin());
      }
      
      //Time
      if (id(smtime).state && sending_interval_reg != 13) {
        read_cycle_number = 12;
        last_sending_timestamp = 0;
        id(smtime).publish_state(false);
      }      
      
      if (now - last_sending_timestamp > sending_interval_reg) {
        last_sending_timestamp = millis();
        this->settings_reading_cycles_ (); 
      } 
      
    }
    
    // Preparation of settings reading cycles SMMetr
    void settings_reading_cycles_ () {
      unsigned long data = 0;
      
      switch (read_cycle_number) {
        // frame call  ( protect Vmax, Vmin, Imax; price kW; the starting kWh value of metr)  and  purchases
        case 0: {
          this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x06, 0x02, this->frame_number_, 0x02, (uint8_t)(0x52 + this->frame_number_)}});
        }  
        break;
        // frame call - metr detail
        case 1: {
          this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x06, 0x02, this->frame_number_, 0x0A, (uint8_t)(0x5A + this->frame_number_)}});
        }  
        break;
        // frame call - total active (power on/off, power, energy) and timing
        case 2: {
          this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x06, 0x02, this->frame_number_, 0x00, (uint8_t)(0x50 + this->frame_number_)}});
        }  
        break;
        // frame call - total active (power on/off, power, energy) and timing
        case 3: {
          this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x06, 0x02, this->frame_number_, 0x00, (uint8_t)(0x50 + this->frame_number_)}});
        }  
        break;
        // sending a frame - power OFF.
        case 4: {          
          this->send_source_frame_(SmFrame {.type_frame = 0x07, .payload = {0x48, 0x07, 0x02, this->frame_number_, 0x09, 0x00, (uint8_t)(0x5A + this->frame_number_)}});
        }
        break;
        // sending a frame - power ON.
        case 5: {          
          this->send_source_frame_(SmFrame {.type_frame = 0x07, .payload = {0x48, 0x07, 0x02, this->frame_number_, 0x09, 0x01, (uint8_t)(0x5B + this->frame_number_)}});
        }
        break; 
        // sending a frame - reset.
        case 6: {  
          this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x12, 0x02, this->frame_number_, 0x05, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, (uint8_t)(0x61 + this->frame_number_)}});
        }  
        break;
        // sending a frame - preparation and storage of settings ( protect Vmax, Vmin, Imax; price kW; the starting kWh value of metr). 
        case 7: {
          data = id(current_high_limit).state * 100; 
          uint8_t current_high_limit_input_ = (uint16_t)data >> 8;
          uint8_t current_high_limit_input__ = (uint16_t)data;
          uint8_t voltage_high_limit_input_ = (uint16_t)id(voltage_high_limit).state >> 8;
          uint8_t voltage_high_limit_input__ = (uint8_t)id(voltage_high_limit).state;
          uint8_t voltage_low_limit_input__ = (uint8_t)id(voltage_low_limit).state;
          id(my_global_float_staringkWh) = id(staring_kWh_value_of_metr).state;
          id(my_global_float_pricekWh) = id(metr_price).state;

          this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x0C, 0x02, this->frame_number_, 0x03, 
          current_high_limit_input_, current_high_limit_input__, voltage_high_limit_input_, voltage_high_limit_input__, 0x00, voltage_low_limit_input__, 
          (uint8_t)(0x48+0x0C+0x02+this->frame_number_ +0x03+current_high_limit_input_+current_high_limit_input__+voltage_high_limit_input_+voltage_high_limit_input__ +
          voltage_low_limit_input__)}});
        }  
        break;
        // sending a frame - save preparation purchases and  on/off.
        case 8: {

          data = id(prepayment_purchase_kWh).state * 100;  
          uint8_t purchase_kWh_ = (uint32_t)data >> 24;
          uint8_t purchase_kWh__ = (uint32_t)data >> 16;
          uint8_t purchase_kWh___ = (uint32_t)data >> 8;
          uint8_t purchase_kWh____ = (uint32_t)data;          
          
          data =id(prepayment_purchases_seiting_balance_alarm_kWh).state * 100;
          uint8_t seiting_balance_alarm_kWh_ = (uint32_t)data >> 24;
          uint8_t seiting_balance_alarm_kWh__ = (uint32_t)data >> 16;
          uint8_t seiting_balance_alarm_kWh___ = (uint32_t)data >> 8;
          uint8_t seiting_balance_alarm_kWh____ = (uint32_t)data;        
                   
          
          this->send_source_frame_ (SmFrame {.type_frame = 0x02, .payload = {0x48, 0x0F, 0x02, this->frame_number_, 0x0D, 
          purchase_kWh_, purchase_kWh__, purchase_kWh___, purchase_kWh____,
          seiting_balance_alarm_kWh_, seiting_balance_alarm_kWh__, seiting_balance_alarm_kWh___, seiting_balance_alarm_kWh____, _prepayment_fuction_onoff,  
          (uint8_t)(0x48+0x0F+0x02+this->frame_number_+0x0D+purchase_kWh_+purchase_kWh__+purchase_kWh___+purchase_kWh____+
          seiting_balance_alarm_kWh_+seiting_balance_alarm_kWh__+seiting_balance_alarm_kWh___+seiting_balance_alarm_kWh____+_prepayment_fuction_onoff)}});

          sending_interval_reg = 5000;
        }
        break; 
        // sending a frame  - reset settings ( protect Vmax, Vmin, Imax).
        case 9: {  
          this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x0C, 0x02, this->frame_number_, 0x03, 0x27, 0x10, 0x01, 0x13, 0x00, 0xAF, (uint8_t)(0x53+this->frame_number_)}});
        }
        break;
        // sending a frame  - reset purchases.
        case 10: {               
          this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x0F, 0x02, this->frame_number_, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, (uint8_t)(0x66+this->frame_number_)}});
        }
        break;
        // sending a frame  - reset timing
        case 11: {
          //id(timing_info).publish_state(false);
          this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x09, 0x02, this->frame_number_, 0x0C, 0x00, 0x00, 0x00, (uint8_t)(0x5F + this->frame_number_)}});
        }
        break;
        case 12: {
          auto time = id(smmetr_time).now();
          time::ESPTime now = time;
          if (now.is_valid()) {
            uint8_t year = now.year - 2000;
            uint8_t month = now.month;
            uint8_t day_of_month = now.day_of_month;
            uint8_t hour = now.hour;
            uint8_t minute = now.minute;
            uint8_t second = now.second;
            uint8_t day_of_week = now.day_of_week - 1;
            if (day_of_week == 0) 
              day_of_week = 7;
            // Timing, save timing  
            if (timing) {
              id(day_time_h).publish_state(hour);
              id(day_time_m).publish_state(minute);
              id(day_week).publish_state(day_of_week);
              timing = false;
              read_cycle_number = 2;
              sending_interval_reg = 1000;
            } else {             
              if (id(save_timing).state) {
                float data_w = id(day_week).state - day_of_week;
                float data = (id(day_time_h).state * 60 + id(day_time_m).state) - (hour * 60 + minute);
                if (id(day_week).state != 0) {                 
                  if (data_w <= 0 && data <= 0) 
                    data_w = 7 + data_w;      
                  if (data_w <= 0 && data > 0) {
                    if (data_w != 0)
                      data_w = 7 + data_w;            
                  } 
                } else {
                  data_w = 0;
                  if (data <= 0)
                    data_w = 1;
                }
                 
                data = data_w * 1440 + data;
                 
                second = (uint8_t)id(data_time_on_off).state;          //  on/off timing
                hour = (uint16_t)data >> 8;                            // minu.
                minute = (uint16_t)data;                               // max 10080              

                this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x09, 0x02, this->frame_number_, 0x0c, hour, minute, second, uint8_t(0x5F+this->frame_number_+
                hour+minute+second)}});  
              // Time    
              } else {
                this->send_source_frame_(SmFrame {.type_frame = 0xFE, .payload = {0x48, 0x0D, 0xFE, 0x01, 0x40, year, month, day_of_month, hour, minute, second, day_of_week, 
                (uint8_t)(0x94+year+month+day_of_month+hour+minute+second+day_of_week)}});
                read_cycle_number = 0;
                sending_interval_reg = 1000;
              }
            }  
          } else {
            timing = false;
            id(save_timing).publish_state(false);
            read_cycle_number = 0;
            sending_interval_reg = 1000;
          }
        } 
        break;
        case 13: {
          read_cycle_number = 0;
          sending_interval_reg = 0;
        }
        break;
        default:{
          read_cycle_number = 0;
          frame_number_inp = 0;
          sending_interval_reg = 1000;
        }
        break;        
      }  
    }

    // Detailed layout received frame 
    void handle_command_(uint8_t type, const uint8_t *buffer) {

      //device report frames
      switch (buffer[4]) {
        // meter detail, frame length 0x43
        case 0x0B: {
          _import_active_energy = ((buffer[58] << 24) | (buffer[59] << 16) | (buffer[60] << 8) | buffer[61]) * 0.01;
          _export_active_energy = ((buffer[62] << 24) | (buffer[63] << 16) | (buffer[64] << 8) | buffer[65]) * -0.01;
          _contract_active_energy = (((buffer[54] << 24) | (buffer[55] << 16) | (buffer[56] << 8) | buffer[57]) * 0.01) + id(my_global_float_staringkWh);
          _frequency = ((buffer[52] << 8) | buffer[53]) * 0.01;
            
          _rms_voltage_phase_a = ((buffer[14] << 8) | buffer[15]) * 0.1;
          _rms_current_phase_a = ((buffer[5] << 16) | (buffer[6] << 8) | buffer[7]) * 0.001;
          if (_rms_current_phase_a >= 1000 ) 
            _rms_current_phase_a = 1000 - _rms_current_phase_a;            
          _rms_active_power_phase_a = ((buffer[35] << 16) | (buffer[36] << 8) | buffer[37]) * 0.0001;
          if (_rms_active_power_phase_a >= 100 )
            _rms_active_power_phase_a = 100 - _rms_active_power_phase_a;
          _rms_reactive_power_phase_a = ((buffer[23]  << 16) | (buffer[24] << 8) | buffer[25]) * 0.0001;
          if (_rms_reactive_power_phase_a >= 100 ) 
            _rms_reactive_power_phase_a = 100 - _rms_reactive_power_phase_a;
          _rms_power_factor_phase_a = ((buffer[46] << 8) | buffer[47]) * 0.001;
            
          _rms_voltage_phase_b = ((buffer[16] << 8) | buffer[17]) * 0.1;
          _rms_current_phase_b = ((buffer[8] << 16) | (buffer[9] << 8) | buffer[10]) * 0.001;
          if (_rms_current_phase_b >= 1000 )
            _rms_current_phase_b = 1000 - _rms_current_phase_b;
          _rms_active_power_phase_b = ((buffer[38] << 16) | (buffer[39] << 8) | buffer[40]) * 0.0001;
          if (_rms_active_power_phase_b  >= 100 ) 
            _rms_active_power_phase_b = 100 - _rms_active_power_phase_b;
          _rms_reactive_power_phase_b = (( buffer[26] << 16) | (buffer[27] << 8) | buffer[28]) * 0.0001;
          if (_rms_reactive_power_phase_b >= 100 ) 
            _rms_reactive_power_phase_b = 100 - _rms_reactive_power_phase_b;           
          _rms_power_factor_phase_b = ((buffer[48] << 8) | buffer[49]) * 0.001;
            
          _rms_voltage_phase_c = ((buffer[18] << 8) | buffer[19]) * 0.1;
          _rms_current_phase_c = ((buffer[11] << 16) | (buffer[12] << 8) | buffer[13]) * 0.001;
          if (_rms_current_phase_c >= 1000 ) 
            _rms_current_phase_c = 1000 - _rms_current_phase_c;            
          _rms_active_power_phase_c = ((buffer[41] << 16) | (buffer[42] << 8) | buffer[43]) * 0.0001;
          if (_rms_active_power_phase_c >= 100 ) 
            _rms_active_power_phase_c = 100 - _rms_active_power_phase_c;            
          _rms_reactive_power_phase_c = ((buffer[29] << 16) | (buffer[30] << 8) | buffer[31]) * 0.0001;
          if (_rms_reactive_power_phase_c >= 100 ) 
            _rms_reactive_power_phase_c = 100 - _rms_reactive_power_phase_c;            
          _rms_power_factor_phase_c = ((buffer[50] << 8) | buffer[51]) * 0.001;
            
          _rms_active_power_g = ((buffer[32] << 16) | (buffer[33] << 8) | buffer[34]) * 0.0001;
          if (_rms_active_power_g >= 100 )
            _rms_active_power_g = 100 - _rms_active_power_g;            
          _rms_reactive_power_g = ((buffer[20] << 16) | (buffer[21] << 8) | buffer[22]) * 0.0001;
          if (_rms_reactive_power_g >= 100 ) 
            _rms_reactive_power_g = 100 - _rms_reactive_power_g;            
          _rms_power_factor_g = ((buffer[44] << 8) | buffer[45]) * 0.001; 
          
          read_cycle_number = 2;
        }  
        break;
        // frame length 0x15
        case 0x01: {
        
          id(power).publish_state(buffer[6]);
          id(total_active_energy).publish_state(((buffer[7] << 24) | (buffer[8] << 16) | (buffer[9] << 8) | buffer[10]) * 0.01);
          float data = ((buffer[11] << 24) | (buffer[12] << 16) | (buffer[13] << 8) | buffer[14]) * 0.0001;
          if (data >= 100 ) 
             data = 100 - data;
          id(total_active_power).publish_state(data);
            
          uint16_t data_data = (buffer[16] << 8) | buffer[17]; 
          id(data_time_hour).publish_state((int)data_data/60);
          id(data_time_minute).publish_state(((float)data_data/60 - (int)data_data/60) * 60); 
          id(data_time_on_off).publish_state(buffer[18]);
          
          if(id(day_week).state == 0)
            id(data_time_week).publish_state("No");
          if(id(day_week).state == 1)
            id(data_time_week).publish_state("Mo");          
          if(id(day_week).state == 2)
            id(data_time_week).publish_state("Tu");
          if(id(day_week).state == 3)
            id(data_time_week).publish_state("We");
          if(id(day_week).state == 4)
            id(data_time_week).publish_state("Th");
          if(id(day_week).state == 5)
            id(data_time_week).publish_state("Fr");
          if(id(day_week).state == 6)
            id(data_time_week).publish_state("Sa");
          if(id(day_week).state == 7)
            id(data_time_week).publish_state("Su");
  
          if (id(reset_timing).state)   
            id(reset_timing).publish_state(false);    
          
          if (id(reset).state)
            id(reset).publish_state(false);
      
          if (data_data == 0) {
            id(timing_info).publish_state(false);
          } else {   
            id(timing_info).publish_state(true);
            }
            
          if (id(timing_sm).state) {
            if (id(data_time_on_off).state)
              id(data_time_onoff).publish_state("ON");
          }
                    
          if (id(save_timing).state) { 
            id(save_timing).publish_state(false);
            id(timing_info).publish_state(true);
            if (id(data_time_on_off).state) {
              id(data_time_onoff).publish_state("ON");
            } else {
              id(data_time_onoff).publish_state("OFF");
              }
          }
          
          if (id(save_protect_price_energy_initial).state) 
            id(save_protect_price_energy_initial).publish_state(false); 
            
          if (id(save_prepayment).state) 
            id(save_prepayment).publish_state(false);  

          if (read_cycle_number == 11) {
            read_cycle_number = 5;
          } else { 
            if (id(total_reset).state) {  
              id(total_reset).publish_state(false);
              id(status_gpio14).turn_on();
            }  
            read_cycle_number += 1;
            if (read_cycle_number > 3)  
              read_cycle_number = 1; 
          }    
        }
      
        break;   
        // setting parameters, frame length 0x19
        case 0x08: {
        
          id(voltage_high_limit).publish_state(((buffer[5] << 8) | buffer[6]));
          id(voltage_low_limit).publish_state((buffer[7] << 8) | buffer[8]);
          id(current_high_limit).publish_state(((buffer[9] << 8) | buffer[10]) * 0.01); 
          id(staring_kWh_value_of_metr).publish_state(id(my_global_float_staringkWh));
          id(metr_price).publish_state(id(my_global_float_pricekWh));        

          id(prepayment_purchase_kWh).publish_state(((buffer[11] << 24) | (buffer[12] << 16) | (buffer[13] << 8) | buffer[14]) * 0.01);
          id(prepayment_purchases_seiting_balance_kWh).publish_state(((buffer[15] << 24) | (buffer[16] << 16) | (buffer[17] << 8) | buffer[18]) * 0.01);
          id(prepayment_purchases_seiting_balance_alarm_kWh).publish_state(((buffer[19] << 24) | (buffer[20] << 16) | (buffer[21] << 8) | buffer[22]) * 0.01);
          id(prepayment_purchase_value).publish_state(id(prepayment_purchases_seiting_balance_kWh).state * id(my_global_float_pricekWh));
          id(prepayment_fuction_on_off).publish_state(buffer[23]);
          
          if (read_cycle_number == 9 || read_cycle_number == 10) {
            frame_number_inp = 0;
            read_cycle_number += 1;
          } else {    
            read_cycle_number = 1;
          }
        }
        break;
        default:
        break; 
      }
    }
    

    // Write frame for transmission
    void send_source_frame_(SmFrame frame) {
      command_queue_.push_back(frame);
      process_command_queue_();
    }
    
    // Frame transmission
    void send_frame_(SmFrame frame) { 
      ESP_LOGI(TAG, "Sending: TYPE=0x%02X FRAME=[%s]", frame.type_frame, format_hex_pretty(frame.payload).c_str());
      this->write_array(frame.payload.data(), frame.payload.size());
      this->expected_response_ =  frame.type_frame;
      last_command_timestamp = millis();
    }
    
};

 // ESP_LOGE(TAG, "%02X", data_data );