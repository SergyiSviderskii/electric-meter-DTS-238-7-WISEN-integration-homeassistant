# electric_meter_DTS-238-7_WISEN---integration-homeassistant


protocol: http://docs.hekr.me/protocol/


integration of a three-phase electric meter DTS-238-7 (WISEN application, hekr protocol) into a homeassistant via the ESPhome add-on.

1.We disassemble the counter and unsolder the ESP module.

2. Connecting the ESP to the programmer

3. Copying a folder sm_metr.h в homeassistant /config/esphome/

4. We launch the ESPhome add-on and create a configuration file using the wizard and transfer it to the created file, smmetr_api.yml or smmetr_mqtt.yml file.

5. Compiling and writing to ESP.

6. We solder the ESP in place and turn on the counter.

The lovles_smmetr.yml file for the visualization panel.

The top button on the counter is GENERAL REZET (hold 5s.)
