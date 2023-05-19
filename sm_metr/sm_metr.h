//﻿electric meter DTS-238-7 WISEN integration homeassistant

#include "esphome.h"
using namespace esphome;

static const char *const TAG = "SM";
static const int RECEIVE_TIMEOUT = 300;
static const int FRAME_INTERVAL = 200;                     // Active device reporting rate (i.e. frame interval) > 200mc
static const int IND_LED_INTERVAL = 100;
static const int TIME_BLOC_LOCK  = 30000;

struct SmFrame {
  uint8_t type_frame;
  std::vector<uint8_t> payload;
 };

struct ApiCommand {
  uint8_t length_frame;
  uint8_t valueapi;
 };
 
ApiCommand sending {}; 
optional<ApiCommand> sending_switch {};
optional<uint8_t> reception {};
optional<bool> smtime {};
bool smtimetiming = false;
bool smtimetimingsave = false;
uint32_t last_ind_led_timestamp = 0;
uint32_t last_lock = 0;


class MyCustomSwitch : public Component, public Switch {
  public:
  
    int relayId;
    MyCustomSwitch(int relai) {relayId = relai;}
    
    void setup() override {
    
    }
    
    void write_state(bool state) override {
     
      if (!id(sm_metr_lock).state) {  
        last_lock =  millis();
      }
      
      //Switch - metr detail sm metr
      if (relayId == 0) {
        if (state) {
          if (id(sm_metr_setting).state || id(sm_metr_timing).state || id(sm_metr_lock).state) {
            state = !state;
          }    
        }
      }
     
      //Switch - total sm metr reset (GENERAL RESET)
      if (relayId == 1) {
        if (state) {
          if (id(sm_metr_setting).state || id(sm_metr_timing).state || id(sm_metr_lock).state || id(sm_metr_detail).state) {
            state = !state;
          } else {
            sending.length_frame = 0xFF;  
            sending.valueapi = state;
            sending_switch = sending;          
            id(status_gpio14).turn_off();
          }
        } else {
          state = !state;  
        }     
      } 
     
      // Switch - power sm metr (ON/OFF).
      if (relayId == 2) {
        if (id(sm_metr_lock).state) {
          state = !state;
        } else {
          sending.length_frame = 0x07;  
          sending.valueapi = state;
          sending_switch = sending;
        }  
      }      

      // Switch - reset sm metr (RESET).
      if (relayId == 3) {
        if (state) {
          if (id(sm_metr_setting).state || id(sm_metr_timing).state || id(sm_metr_lock).state || id(sm_metr_detail).state) {
            state = !state;
          } else {        
            sending.length_frame = 0x12;  
            sending.valueapi = state;
            sending_switch = sending;
          }    
        }  
      }
        
      // Switch - timing (TIMING)
      if (relayId == 4) {
        if (state && (id(sm_metr_setting).state || id(sm_metr_detail).state || id(sm_metr_lock).state)) {
          state = !state;
        } else {
          sending.length_frame = 0x15;  
          sending.valueapi = state;
          sending_switch = sending;          
        }
      }
      
      // Switch - save timing (SAVE)
      if (relayId == 5) {
        if (state) {  
          state = !state; 
        } else {        
          sending.length_frame = 0x16;  
          sending.valueapi = state;
          sending_switch = sending; 
        }  
      }   
     
      // Switch - reset timing. (RESET) 
      if (relayId == 6) {
        if (state) {  
          state = !state; 
        } else {        
          sending.length_frame = 0x09;  
          sending.valueapi = state;
          sending_switch = sending; 
        } 
      }

      // Switch - setting.(SETTING)
      if (relayId == 7) {
        if ((id(sm_metr_timing).state || id(sm_metr_detail).state || id(sm_metr_lock).state)) {
          state = false;
        } else {
          sending.length_frame = 0x06;  
          sending.valueapi = state;
          sending_switch = sending;         
        }         
      }
      
      // Switch - preparation and storage of settings 
      // ( protect Vmax, Vmin, Imax; price kW; the starting kWh value of metr).(SAVE)
      if (relayId == 8) {
          sending.length_frame = 0x0C;  
          sending.valueapi = state;
          sending_switch = sending;
      }    
      
      // Switch - preparation purchases: on/off
      if (relayId == 9) {
        if(!id(sm_metr_setting).state || id(sm_metr_lock).state) {
          state = !state;
        } else {
          sending.length_frame = 10;  
          sending.valueapi = state;
          sending_switch = sending;
        }
      }       
      
      // Switch - save preparation purchases.(SAVE)
      if (relayId == 10) {
         sending.length_frame = 0x0F;  
         sending.valueapi = state;
         sending_switch = sending;
      } 
      
      // Switch - lock
      if (relayId == 11) {
        if (state) {
         sending.length_frame = 0xF0;  
         sending.valueapi = state;
         sending_switch = sending;        
        } else {
          last_lock =  millis();
        }
      } 
      publish_state(state);
    }
};


class SMComponent : public Component, public UARTDevice {
  public:
  
    SMComponent(UARTComponent *parent) : UARTDevice(parent) {}
    
 #ifdef USE_TIME
    void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
 #endif    

    void setup() override {
      set_time_id(homeassistant_time);
      last_lock =  millis();
      id(sm_metr_save_timing).publish_state(false); 
      this->set_interval("heartbeat", 15000, [this] { this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x06, 0x02, 0x00, 0x00, 0x50}}); });
    }
    
    void loop() override {
      while (this->available()) {
        uint8_t c;
        this->read_byte(&c);
        this->handle_char_(c);
      }
      process_command_queue_();
    }
    
  protected:
    uint32_t last_command_timestamp_ = 0;
    uint32_t last_rx_char_timestamp_ = 0;   
    std::vector<uint8_t> rx_message_;
    std::vector<SmFrame> command_queue_;
    optional<uint8_t> expected_response_;
    float voltage_high_limit_ = 0;
    float voltage_low_limit_ = 0;
    float current_high_limit_ = 0; 
    float prepayment_purchase_kWh_ = 0;
    float prepayment_purchases_setting_balance_alarm_kWh_ = 0;
    bool prepayment_fuction_ = false;
    int initialization_ = 0;
  #ifdef USE_TIME
      optional<time::RealTimeClock *> time_id_{};
  #endif  
    int i = 0;
    int data_ = 0;
    int total_reset_ = 0;
    
    void handle_char_(uint8_t c) { 
      this->rx_message_.push_back(c);             
      if (!this->validate_message_()) {
        this->rx_message_.clear();
      } else {
          this->last_rx_char_timestamp_ = millis();
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
        this->rx_message_={0x48, 0x07, 0xFF, 0x00, 0x02, 0x00, (uint8_t)(0x48+0x07+0xFF+0x02)};
        ESP_LOGE(TAG, "Sending an error frame to the device: TYPE=0xFF FRAME=[%s]", format_hex_pretty(rx_message_).c_str());
        this->send_source_frame_(SmFrame {.type_frame = 0xFF, .payload = {rx_message_}}); // code validation error
        return false;  // return false to reset rx buffer                                                                                
      }                           

      ESP_LOGD(TAG, "Received: TYPE=0x%02X FRAME=[%s]", type, format_hex_pretty(rx_message_).c_str());
      
      if (type == 0x02) {
        if (id(sm_metr_reset).state) {
          id(sm_metr_reset).turn_off();    
        }  
        return false;   // return false to reset rx buffer
      }
      
      this->send_source_frame_(SmFrame {.type_frame = 0x01, .payload = rx_message_}); 
      
      // valid message
      const uint8_t *message_data = data; 
      this->handle_command_( type, message_data);
    
      return false;    // return false to reset rx buffer
      
    }
    
    // process command queue
    void process_command_queue_() {
      uint32_t now = millis();
      uint32_t delay = now - this->last_command_timestamp_;
      
      if (now - last_lock > TIME_BLOC_LOCK) {
        if (!id(sm_metr_lock).state) {
          id(sm_metr_lock).turn_on();
        }
      }    
      
      if (!id(sm_metr_satatus).state && !id(sm_metr_total_reset).state) {
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
      
      if (now - this->last_rx_char_timestamp_ > RECEIVE_TIMEOUT) 
        this->rx_message_.clear();
      
      if (this->expected_response_.has_value() && delay > FRAME_INTERVAL) 
        this->expected_response_.reset(); 
     
      if (!this->command_queue_.empty() &&  !this->expected_response_.has_value()){ 
        this->send_frame_(command_queue_.front());
        this->command_queue_.erase(command_queue_.begin());
      }
 
      if (smtime.has_value()) {
        smtime.reset(); 
        #ifdef USE_TIME
          if (this->time_id_.has_value()) {
            this->send_local_time_();
            auto *time_id = *this->time_id_;
          } else {
            ESP_LOGW(TAG, "LOCAL_TIME_QUERY is not handled because time is not configured");
          }
        #else
          ESP_LOGE(TAG, "LOCAL_TIME_QUERY is not handled");
        #endif  
      }
      
      if (sending_switch.has_value()) {
        sending_switch.reset();
        
        // Switch - lock 
        if (sending_switch->length_frame == 0xF0) { 
          if (sending_switch->valueapi) {
            if(id(sm_metr_detail).state) {id(sm_metr_detail).turn_off();}
            if(id(sm_metr_timing).state) {id(sm_metr_timing).turn_off();}
            if(id(sm_metr_setting).state) {id(sm_metr_setting).turn_off();}
          }    
        }
        
        //total sm metr reset (GENERAL RESET)
        if (sending_switch->length_frame == 0xFF) { 
          this->total_reset_ = 0; 
          this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x0C, 0x02, 0x00, 0x03, 0x27, 0x10, 0x01, 0x13, 0x00, 0xAF, 0x53}});
        }
        
        // sending a frame - power
        if (sending_switch->length_frame == 0x07) { 
          this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x07, 0x02, 0x00, 0x09, sending_switch->valueapi, (uint8_t)(0x5A +sending_switch->valueapi)}});
        }
        
        // sending a frame - reset sm metr
        if (sending_switch->length_frame == 0x12) { 
          this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x12, 0x02, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, \
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x61}});
        }
        
        // Switch - timing (TIMING)
        if (sending_switch->length_frame == 0x15) { 
          if (!sending_switch->valueapi) {
            if (id(sm_metr_save_timing).state) {
              id(sm_metr_save_timing).publish_state(false); 
            }  
          } 
            smtime = true;
            smtimetiming = true;            
        } 

        // Switch - save timing (SAVE)        
        if (sending_switch->length_frame == 0x16) { 
          if (!sending_switch->valueapi) {
            smtime = true;
            smtimetiming = true;  
            smtimetimingsave = true;
          } else {
            if (id(sm_metr_timing).state && !id(sm_metr_reset_timing).state) {
              this->data_ = (id(day_time_h).state * 60 + id(day_time_m).state) - (id(data_time_hour).state * 60 + id(data_time_minute).state);
              auto index = id(day_week).active_index();      
              int week = index.value();
              index = id(day_week).index_of(id(data_time_week).state);
              week = week - index.value();
              if (week == 0) {
                if (this->data_ == 0) {week = 7;} 
                if (this->data_ > 0) {week = 0;}
                if (this->data_ < 0) {week = 6;} 
              }    
              if (week < 0) {week = 7 + week;}
              if (this->data_ < 0) {this->data_ = this->data_ + 1440;}
              this->data_ = week * 1440 + this->data_;
            } else {
              id(sm_metr_save_timing).publish_state(false); 
              smtime = true;
              smtimetiming = true;            
            }
          }
        }      
        
        // sending a frame  - reset timing
        if (sending_switch->length_frame == 0x09) { 
          this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x09, 0x02, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x5F}});
          smtime = true;
          smtimetiming = true;
        }

        // sending a frame - setting ( protect Vmax, Vmin, Imax; price kW; the starting kWh value of metr  and  purchases)
        if (sending_switch->length_frame == 0x06) { 
          if (sending_switch->valueapi) {
            this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x06, 0x02, 0x00, 0x02, 0x52}});
          } else {
            if (id(sm_metr_save_protec_price_energy_initial).state) {
              id(sm_metr_save_protec_price_energy_initial).publish_state(false);
            }
            if (id(sm_metr_save_prepayment).state) {
              id(sm_metr_save_prepayment).publish_state(false);
            }  
            this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x06, 0x02, 0x00, 0x02, 0x52}});    
          }    
        }
        // sending a frame - preparation and storage of settings ( protect Vmax, Vmin, Imax; price kW; the starting kWh value of metr). 
        if (sending_switch->length_frame == 0x0C) {
          if (sending_switch->valueapi) {
            if ((this->voltage_high_limit_ == id(voltage_high_limit).state) && (this->voltage_low_limit_ == id(voltage_low_limit).state) && \
                (this->current_high_limit_ == id(current_high_limit).state) && (id(starting_kWh_value_of_metr).state == id(my_global_float_startingkWh)) && \
                (id(metr_price).state == id(my_global_float_pricekWh))) {   
              id(sm_metr_save_protec_price_energy_initial).publish_state(false);
            }    
          } else {
            if ((this->voltage_high_limit_ != id(voltage_high_limit).state) || (this->voltage_low_limit_ != id(voltage_low_limit).state) || \
                (this->current_high_limit_ != id(current_high_limit).state)) { 
              uint8_t current_high_limit_input_ = (uint16_t)(id(current_high_limit).state * 100) >> 8;
              uint8_t current_high_limit_input__ = (uint16_t)(id(current_high_limit).state * 100);
              uint8_t voltage_high_limit_input_ = (uint16_t)id(voltage_high_limit).state >> 8;
              uint8_t voltage_high_limit_input__ = (uint8_t)id(voltage_high_limit).state;
              uint8_t voltage_low_limit_input__ = (uint8_t)id(voltage_low_limit).state;
           
              this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x0C, 0x02, 0x00, 0x03, 
              current_high_limit_input_, current_high_limit_input__, voltage_high_limit_input_, voltage_high_limit_input__, 0x00, voltage_low_limit_input__, 
              (uint8_t)(0x48+0x0C+0x02+0x03+current_high_limit_input_+current_high_limit_input__+voltage_high_limit_input_+voltage_high_limit_input__ + 
              voltage_low_limit_input__)}});
            }  
            if (id(starting_kWh_value_of_metr).state != id(my_global_float_startingkWh)) {
              id(my_global_float_startingkWh) = id(starting_kWh_value_of_metr).state;
            }       
            if (id(metr_price).state != id(my_global_float_pricekWh)) {
              id(my_global_float_pricekWh) = id(metr_price).state;
            }
          }
        }
        // sending a frame - save preparation purchases and  on/off.
        if (sending_switch->length_frame == 0x0F) {
          if (sending_switch->valueapi) {
            if ((this->prepayment_purchase_kWh_ == id(prepayment_purchase_kWh).state) && \
                (this->prepayment_purchases_setting_balance_alarm_kWh_ == id(prepayment_purchases_setting_balance_alarm_kWh).state) && \
                (this->prepayment_fuction_ == id(sm_metr_prepayment_fuction).state)) {
              id(sm_metr_save_prepayment).publish_state(false);
            }    
          } else {
            if ((this->prepayment_purchase_kWh_ != id(prepayment_purchase_kWh).state) || \
                (this->prepayment_purchases_setting_balance_alarm_kWh_ != id(prepayment_purchases_setting_balance_alarm_kWh).state) || \
                (this->prepayment_fuction_ != id(sm_metr_prepayment_fuction).state)) {
              uint8_t purchase_kWh_ = (uint32_t)(id(prepayment_purchase_kWh).state * 100) >> 24;
              uint8_t purchase_kWh__ = (uint32_t)(id(prepayment_purchase_kWh).state * 100)>> 16;
              uint8_t purchase_kWh___ = (uint32_t)(id(prepayment_purchase_kWh).state * 100) >> 8;
              uint8_t purchase_kWh____ = (uint32_t)(id(prepayment_purchase_kWh).state * 100);          

              uint8_t setting_balance_alarm_kWh_ = (uint32_t)(id(prepayment_purchases_setting_balance_alarm_kWh).state * 100) >> 24;
              uint8_t setting_balance_alarm_kWh__ = (uint32_t)(id(prepayment_purchases_setting_balance_alarm_kWh).state * 100) >> 16;
              uint8_t setting_balance_alarm_kWh___ = (uint32_t)(id(prepayment_purchases_setting_balance_alarm_kWh).state * 100) >> 8;
              uint8_t setting_balance_alarm_kWh____ = (uint32_t)(id(prepayment_purchases_setting_balance_alarm_kWh).state * 100);  
        
              this->send_source_frame_ (SmFrame {.type_frame = 0x02, .payload = {0x48, 0x0F, 0x02, 0x00, 0x0D, 
              purchase_kWh_, purchase_kWh__, purchase_kWh___, purchase_kWh____, 
              setting_balance_alarm_kWh_, setting_balance_alarm_kWh__, setting_balance_alarm_kWh___, setting_balance_alarm_kWh____, (uint8_t)id(sm_metr_prepayment_fuction).state, 
              (uint8_t)(0x48+0x0F+0x02+0x0D+purchase_kWh_+purchase_kWh__+purchase_kWh___+purchase_kWh____+ 
              setting_balance_alarm_kWh_+setting_balance_alarm_kWh__+setting_balance_alarm_kWh___+setting_balance_alarm_kWh____+(uint8_t)id(sm_metr_prepayment_fuction).state)}});  
            }
          }    
        } 
        // Switch - preparation purchases: on/off
        if (sending_switch->length_frame == 10) {
          id(sm_metr_save_prepayment).turn_on();
        }  
      }
      
    }

    // Detailed layout received frame 
    void handle_command_(uint8_t type, const uint8_t *buffer) {

      //device report frames
      switch (buffer[4]) {
      
        // meter detail, frame length 0x43
        case 0x0B: {
          float examination {};
          examination = ((buffer[58] << 24) | (buffer[59] << 16) | (buffer[60] << 8) | buffer[61]) * 0.01;
          if (examination != id(import_active_energy).state) {id(import_active_energy).publish_state(examination);}
          examination = ((buffer[62] << 24) | (buffer[63] << 16) | (buffer[64] << 8) | buffer[65]) * -0.01;
          if (examination != id(export_active_energy).state) {id(export_active_energy).publish_state(examination);}
          examination = (((buffer[54] << 24) | (buffer[55] << 16) | (buffer[56] << 8) | buffer[57]) * 0.01) + id(my_global_float_startingkWh);
          if (examination != id(contract_active_energy).state) {id(contract_active_energy).publish_state(examination);}
          examination = ((buffer[52] << 8) | buffer[53]) * 0.01;
          if (examination != id(frequency).state) {id(frequency).publish_state(examination);}
          
          examination = ((buffer[14] << 8) | buffer[15]) * 0.1;
          if (examination != id(rms_voltage_phase_a).state) {id(rms_voltage_phase_a).publish_state(examination);}
          examination = ((buffer[5] << 16) | (buffer[6] << 8) | buffer[7]) * 0.001; 
          if (examination >= 1000 ) {examination = (1000 - examination);} 
          if (examination != id(rms_current_phase_a).state) {id(rms_current_phase_a).publish_state(examination);}
          examination = ((buffer[35] << 16) | (buffer[36] << 8) | buffer[37]) * 0.0001; 
          if (examination >= 100 ) {examination = (100 - examination);} 
          if (examination != id(rms_active_power_phase_a).state) {id(rms_active_power_phase_a).publish_state(examination);}
          examination = ((buffer[23]  << 16) | (buffer[24] << 8) | buffer[25]) * 0.0001; 
          if (examination >= 100 ) {examination = (100 - examination);} 
          if (examination != id(rms_reactive_power_phase_a).state) {id(rms_reactive_power_phase_a).publish_state(examination);}  
          examination = ((buffer[46] << 8) | buffer[47]) * 0.001; 
          if (examination != id(rms_power_factor_phase_a).state) {id(rms_power_factor_phase_a).publish_state(examination);}
          
          examination = ((buffer[16] << 8) | buffer[17]) * 0.1;
          if (examination != id(rms_voltage_phase_b).state) {id(rms_voltage_phase_b).publish_state(examination);}
          examination = ((buffer[8] << 16) | (buffer[9] << 8) | buffer[10]) * 0.001; 
          if (examination >= 1000 ) {examination = (1000 - examination);} 
          if (examination != id(rms_current_phase_b).state) {id(rms_current_phase_b).publish_state(examination);}
          examination = ((buffer[38] << 16) | (buffer[39] << 8) | buffer[40]) * 0.0001; 
          if (examination >= 100 ) {examination = (100 - examination);} 
          if (examination != id(rms_active_power_phase_b).state) {id(rms_active_power_phase_b).publish_state(examination);}
          examination = ((buffer[26] << 16) | (buffer[27] << 8) | buffer[28]) * 0.0001; 
          if (examination >= 100 ) {examination = (100 - examination);} 
          if (examination != id(rms_reactive_power_phase_b).state) {id(rms_reactive_power_phase_b).publish_state(examination);}  
          examination = ((buffer[48] << 8) | buffer[49]) * 0.001; 
          if (examination != id(rms_power_factor_phase_b).state) {id(rms_power_factor_phase_b).publish_state(examination);}
          
          examination = ((buffer[18] << 8) | buffer[19]) * 0.1;
          if (examination != id(rms_voltage_phase_c).state) {id(rms_voltage_phase_c).publish_state(examination);}
          examination = ((buffer[11] << 16) | (buffer[12] << 8) | buffer[13]) * 0.001; 
          if (examination >= 1000 ) {examination = (1000 - examination);} 
          if (examination != id(rms_current_phase_c).state) {id(rms_current_phase_c).publish_state(examination);}
          examination = ((buffer[41] << 16) | (buffer[42] << 8) | buffer[43]) * 0.0001; 
          if (examination >= 100 ) {examination = (100 - examination);} 
          if (examination != id(rms_active_power_phase_c).state) {id(rms_active_power_phase_c).publish_state(examination);}
          examination = ((buffer[29] << 16) | (buffer[30] << 8) | buffer[31]) * 0.0001; 
          if (examination >= 100 ) {examination = (100 - examination);} 
          if (examination != id(rms_reactive_power_phase_c).state) {id(rms_reactive_power_phase_c).publish_state(examination);}  
          examination = ((buffer[50] << 8) | buffer[51]) * 0.001; 
          if (examination != id(rms_power_factor_phase_c).state) {id(rms_power_factor_phase_c).publish_state(examination);}           
         
          examination = ((buffer[32] << 16) | (buffer[33] << 8) | buffer[34]) * 0.0001; 
          if (examination >= 100 ) {examination = (100 - examination);} 
          if (examination != id(rms_active_power_g).state) {id(rms_active_power_g).publish_state(examination);}
          examination = ((buffer[20] << 16) | (buffer[21] << 8) | buffer[22]) * 0.0001; 
          if (examination >= 100 ) {examination = (100 - examination);} 
          if (examination != id(rms_reactive_power_g).state) {id(rms_reactive_power_g).publish_state(examination);}  
          examination = ((buffer[44] << 8) | buffer[45]) * 0.001; 
          if (examination != id(rms_power_factor_g).state) {id(rms_power_factor_g).publish_state(examination);} 
          
          if (!id(sm_metr_total_reset).state) {
            if (this->initialization_ == 1) {
              this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x06, 0x02, 0x00, 0x02, 0x52}});  
              this->initialization_ += 1;
            }
          }    
        }  
        break;
        // frame length 0x15
        case 0x01: {
          float examination {};
          if (id(sm_metr_power).state != buffer[6]) {id(sm_metr_power).publish_state(buffer[6]);}
          examination = ((buffer[7] << 24) | (buffer[8] << 16) | (buffer[9] << 8) | buffer[10]) * 0.01;
          if (examination != id(total_active_energy).state) {id(total_active_energy).publish_state(examination);} 
          examination = ((buffer[11] << 24) | (buffer[12] << 16) | (buffer[13] << 8) | buffer[14]) * 0.0001;
          if (examination != id(total_active_power).state) {id(total_active_power).publish_state(examination);}
 
          if ((!(buffer[16] != 0) && (buffer[17] != 0)) || (buffer[16] != 0)) {
            if (!id(sm_metr_reset_timing).state){
              id(sm_metr_reset_timing).publish_state(true);
            }

            float week = (buffer[16] << 8) | buffer[17];
            id(data_time_hour).publish_state((int)(week / 60));
            id(data_time_minute).publish_state((week / 60 - id(data_time_hour).state) * 60); 
            week = (int)((week + 1440 - id(day_time_h).state * 60 + id(day_time_m).state) / 1440);
            auto index = id(day_week).active_index();
            
            if (week > index.value()) {
              week = (int)(week + index.value() - 7);
            } else {
              week = (int)(week + index.value());
            }

            
            reception = 0x19;
            auto option = id(day_week).at((int)week);
            auto call = id(day_week).make_call();
            call.set_option(option.value());
            call.perform();
            reception.reset();
            id(data_time_week).publish_state(id(day_week).state.c_str());

            if (buffer[18] == 0) {
              id(data_time_onoff).publish_state("OFF");
            } else {
              id(data_time_onoff).publish_state("ON");
            }
            
            smtime = true;
            smtimetiming = true;
            
          } else {
            if (id(sm_metr_reset_timing).state) {
              id(sm_metr_reset_timing).publish_state(false);
            }          
          }
          
          if (id(sm_metr_reset).state) {
            id(sm_metr_reset).turn_off();
          }
          
          if (!id(sm_metr_total_reset).state) {
            this->set_timeout("metr_detail", 5000, [this] { this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x06, 0x02, 0x00, 0x0A, 0x5A}});});
            if (this->initialization_ != 2) {
              this->initialization_ += 1;
            }
          } else {
            if (this->total_reset_ == 1) {
              this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x0F, 0x02, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x66}});
              this->total_reset_ += 1;
            }  
            if (this->total_reset_ == 3) {  
              if (!id(sm_metr_power).state) {
                this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x07, 0x02, 0x00, 0x09, 0x01, 0x5B}});
                this->total_reset_ += 1;
              }
            }  
            if (this->total_reset_ == 4) { 
               this->total_reset_ += 1;
            }
            if (((this->total_reset_ == 3) && id(sm_metr_power).state) || this->total_reset_ == 5) {
                id(sm_metr_total_reset).publish_state(false);
            }
          }    
        }
      
        break;   
        // setting parameters, frame length 0x19
        case 0x08: {
          reception = 0x19; 
          this->voltage_high_limit_ = (buffer[5] << 8) | buffer[6];
          id(voltage_high_limit).publish_state(voltage_high_limit_);
          this->voltage_low_limit_ = (buffer[7] << 8) | buffer[8];
          id(voltage_low_limit).publish_state(voltage_low_limit_);
          this->current_high_limit_ = ((buffer[9] << 8) | buffer[10]) * 0.01;
          id(current_high_limit).publish_state(current_high_limit_);
          id(starting_kWh_value_of_metr).publish_state(id(my_global_float_startingkWh));
          id(metr_price).publish_state(id(my_global_float_pricekWh));        

          id(prepayment_purchase_kWh).publish_state(((buffer[11] << 24) | (buffer[12] << 16) | (buffer[13] << 8) | buffer[14]) * 0.01);
          id(prepayment_purchases_setting_balance_kWh).publish_state(((buffer[15] << 24) | (buffer[16] << 16) | (buffer[17] << 8) | buffer[18]) * 0.01);
          id(prepayment_purchases_setting_balance_alarm_kWh).publish_state(((buffer[19] << 24) | (buffer[20] << 16) | (buffer[21] << 8) | buffer[22]) * 0.01);
          id(prepayment_purchase_value).publish_state(id(prepayment_purchases_setting_balance_kWh).state * id(my_global_float_pricekWh));
          if (buffer[23] == 0x01) {
            id(sm_metr_prepayment_fuction).publish_state(true);
          } else {
            id(sm_metr_prepayment_fuction).publish_state(false);  
          }    
          reception.reset(); 
          
          if (id(sm_metr_total_reset).state) {
            if (this->total_reset_ == 0) {
              this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x09, 0x02, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x5F}}); 
              this->total_reset_ += 1;
            } 
            if (this->total_reset_ == 2) {
              this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x06, 0x02, 0x00, 0x00, 0x50}});
              this->total_reset_ += 1;
            }             
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
      ESP_LOGD(TAG, "Sending: TYPE=0x%02X FRAME=[%s]", frame.type_frame, format_hex_pretty(frame.payload).c_str());
      this->write_array(frame.payload.data(), frame.payload.size());
      this->expected_response_ =  frame.type_frame;
      this->last_command_timestamp_ = millis();
    }
    
 
    #ifdef USE_TIME
    void send_local_time_() {
      std::vector<uint8_t> payload;
      auto *time_id = *this->time_id_;
      time::ESPTime now = time_id->now();
      if (now.is_valid()) {
        uint8_t year = now.year - 2000;
        uint8_t month = now.month;
        uint8_t day_of_month = now.day_of_month;
        uint8_t hour = now.hour;
        uint8_t minute = now.minute;
        uint8_t second = now.second;
        // days starts from Monday, esphome uses Sunday as day 1
        uint8_t day_of_week = now.day_of_week - 1;
        if (day_of_week == 0) {
          day_of_week = 7;
        }
        
        if (!smtimetiming && !smtimetimingsave) {
          this->send_source_frame_(SmFrame {.type_frame = 0xFE, .payload = {0x48, 0x0D, 0xFE, 0x01, 0x40, year, month, day_of_month, hour, minute, second, day_of_week, 
          (uint8_t)(0x94+year+month+day_of_month+hour+minute+second+day_of_week)}});   
        }
        
        // Timing
        if (smtimetiming) {
          smtimetiming = false;
          reception = 0x09;
          id(day_time_h).publish_state(hour);
          id(day_time_m).publish_state(minute);
          auto option = id(day_week).at(day_of_week - 1);
          auto call = id(day_week).make_call();
          call.set_option(option.value());
          call.perform();
          reception.reset();
          if (!id(sm_metr_save_timing).state && !id(sm_metr_reset_timing).state && !smtimetimingsave) {
            id(data_time_hour).publish_state(hour);
            id(data_time_minute).publish_state(minute); 
            id(data_time_week).publish_state(id(day_week).state.c_str());
          }
          // Save timing 
          if (smtimetimingsave) { 
            smtimetimingsave = false; 

            this->send_source_frame_(SmFrame {.type_frame = 0x02, .payload = {0x48, 0x09, 0x02, 0x00, 0x0c, \
            (uint8_t)((uint16_t)this->data_ >> 8), (uint8_t)this->data_, \
            (uint8_t)id(sm_metr_timing_on_off).state, (uint8_t)(0x5F+(uint8_t)((uint16_t)this->data_ >> 8)+(uint8_t)this->data_+ \
            (uint8_t)id(sm_metr_timing_on_off).state)}});     
          }   
        }
      } else {
        // By spec we need to notify MCU that the time was not obtained if this is a rESPonse to a query
        ESP_LOGW(TAG, "Sending missing local time");
      }
    }
    #endif    
     
};