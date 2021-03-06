/**************************************************************************/
/*! 
    @file     Adafruit_INA219.cpp
    @author   K.Townsend (Adafruit Industries)
	@license  BSD (see license.txt)
	
	Driver for the INA219 current sensor

	This is a library for the Adafruit INA219 breakout
	----> https://www.adafruit.com/products/???
		
	Adafruit invests time and resources providing this open source code, 
	please support Adafruit and open-source hardware by purchasing 
	products from Adafruit!

	@section  HISTORY

    v1.0 - First release
*/
/**************************************************************************/
#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

#include <Wire.h>

#include "Adafruit_INA219.h"

/**************************************************************************/
/*! 
    @brief  Sends a single command byte over I2C
*/
/**************************************************************************/
void Adafruit_INA219::wireWriteRegister (uint8_t reg, uint16_t value)
{
  Wire.beginTransmission(ina219_i2caddr);
  #if ARDUINO >= 100
    Wire.write(reg);                       // Register
    Wire.write((value >> 8) & 0xFF);       // Upper 8-bits
    Wire.write(value & 0xFF);              // Lower 8-bits
  #else
    Wire.send(reg);                        // Register
    Wire.send(value >> 8);                 // Upper 8-bits
    Wire.send(value & 0xFF);               // Lower 8-bits
  #endif
  Wire.endTransmission();
}

/**************************************************************************/
/*! 
    @brief  Reads a 16 bit values over I2C
*/
/**************************************************************************/
void Adafruit_INA219::wireReadRegister(uint8_t reg, uint16_t *value)
{

  Wire.beginTransmission(ina219_i2caddr);
  #if ARDUINO >= 100
    Wire.write(reg);                       // Register
  #else
    Wire.send(reg);                        // Register
  #endif
  Wire.endTransmission();
  
  delay(1); // Max 12-bit conversion time is 586us per sample

  Wire.requestFrom(ina219_i2caddr, (uint8_t)2);  
  #if ARDUINO >= 100
    // Shift values to create properly formed integer
    *value = ((Wire.read() << 8) | Wire.read());
  #else
    // Shift values to create properly formed integer
    *value = ((Wire.receive() << 8) | Wire.receive());
  #endif
}

/**************************************************************************/
/*! 
    @brief  Configures to INA219 to be able to measure up to 32V and 2A
            of current.  Each unit of current corresponds to 100uA, and
            each unit of power corresponds to 2mW. Counter overflow
            occurs at 3.2A.
			
    @note   These calculations assume a 0.1 ohm resistor is present
*/
/**************************************************************************/
void Adafruit_INA219::setCalibration_32V_2A(void)
{
  // By default we use a pretty huge range for the input voltage,
  // which probably isn't the most appropriate choice for system
  // that don't use a lot of power.  But all of the calculations
  // are shown below if you want to change the settings.  You will
  // also need to change any relevant register settings, such as
  // setting the VBUS_MAX to 16V instead of 32V, etc.

  // VBUS_MAX = 32V             (Assumes 32V, can also be set to 16V)
  // VSHUNT_MAX = 0.32          (Assumes Gain 8, 320mV, can also be 0.16, 0.08, 0.04)
  // RSHUNT = 0.1               (Resistor value in ohms)
  
  // 1. Determine max possible current
  // MaxPossible_I = VSHUNT_MAX / RSHUNT
  // MaxPossible_I = 3.2A
  
  // 2. Determine max expected current
  // MaxExpected_I = 2.0A
  
  // 3. Calculate possible range of LSBs (Min = 15-bit, Max = 12-bit)
  // MinimumLSB = MaxExpected_I/32767
  // MinimumLSB = 0.000061              (61uA per bit)
  // MaximumLSB = MaxExpected_I/4096
  // MaximumLSB = 0,000488              (488uA per bit)
  
  // 4. Choose an LSB between the min and max values
  //    (Preferrably a roundish number close to MinLSB)
  // CurrentLSB = 0.0001 (100uA per bit)
  ina219_currentLsb_mA = 0.1;
  
  // 5. Compute the calibration register
  // Cal = trunc (0.04096 / (Current_LSB * RSHUNT))
  // Cal = 4096 (0x1000)
  
  ina219_calValue = 4096;
  
  // 6. Calculate the power LSB
  // PowerLSB = 20 * CurrentLSB
  // PowerLSB = 0.002 (2mW per bit)
  ina219_powerLsb_mW = 2; // in mW
  
  // 7. Compute the maximum current and shunt voltage values before overflow
  //
  // Max_Current = Current_LSB * 32767
  // Max_Current = 3.2767A before overflow
  //
  // If Max_Current > Max_Possible_I then
  //    Max_Current_Before_Overflow = MaxPossible_I
  // Else
  //    Max_Current_Before_Overflow = Max_Current
  // End If
  //
  // Max_ShuntVoltage = Max_Current_Before_Overflow * RSHUNT
  // Max_ShuntVoltage = 0.32V
  //
  // If Max_ShuntVoltage >= VSHUNT_MAX
  //    Max_ShuntVoltage_Before_Overflow = VSHUNT_MAX
  // Else
  //    Max_ShuntVoltage_Before_Overflow = Max_ShuntVoltage
  // End If
  
  // 8. Compute the Maximum Power
  // MaximumPower = Max_Current_Before_Overflow * VBUS_MAX
  // MaximumPower = 3.2 * 32V
  // MaximumPower = 102.4W
  
  // Set Calibration register to 'Cal' calculated above	
  wireWriteRegister(INA219_REG_CALIBRATION, ina219_calValue);
  
  // Set Config register to take into account the settings above
  uint16_t config = INA219_CONFIG_BVOLTAGERANGE_32V |
                    INA219_CONFIG_GAIN_8_320MV |
                    INA219_CONFIG_BADCRES_12BIT |
                    INA219_CONFIG_SADCRES_12BIT_1S_532US |
                    INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
  wireWriteRegister(INA219_REG_CONFIG, config);
}

/**************************************************************************/
/*! 
    @brief  Configures to INA219 to be able to measure up to 32V and 1A
            of current.  Each unit of current corresponds to 40uA, and each
            unit of power corresponds to 800�W. Counter overflow occurs at
            1.3A.
			
    @note   These calculations assume a 0.1 ohm resistor is present
*/
/**************************************************************************/
void Adafruit_INA219::setCalibration_32V_1A(void)
{
  // By default we use a pretty huge range for the input voltage,
  // which probably isn't the most appropriate choice for system
  // that don't use a lot of power.  But all of the calculations
  // are shown below if you want to change the settings.  You will
  // also need to change any relevant register settings, such as
  // setting the VBUS_MAX to 16V instead of 32V, etc.

  // VBUS_MAX = 32V		(Assumes 32V, can also be set to 16V)
  // VSHUNT_MAX = 0.32	(Assumes Gain 8, 320mV, can also be 0.16, 0.08, 0.04)
  // RSHUNT = 0.1			(Resistor value in ohms)

  // 1. Determine max possible current
  // MaxPossible_I = VSHUNT_MAX / RSHUNT
  // MaxPossible_I = 3.2A

  // 2. Determine max expected current
  // MaxExpected_I = 1.0A

  // 3. Calculate possible range of LSBs (Min = 15-bit, Max = 12-bit)
  // MinimumLSB = MaxExpected_I/32767
  // MinimumLSB = 0.0000305             (30.5�A per bit)
  // MaximumLSB = MaxExpected_I/4096
  // MaximumLSB = 0.000244              (244�A per bit)

  // 4. Choose an LSB between the min and max values
  //    (Preferrably a roundish number close to MinLSB)
  // CurrentLSB = 0.0000400 (40�A per bit)
  ina219_currentLsb_mA = 0.04; // in mA

  // 5. Compute the calibration register
  // Cal = trunc (0.04096 / (Current_LSB * RSHUNT))
  // Cal = 10240 (0x2800)

  ina219_calValue = 10240;
  
  // 6. Calculate the power LSB
  // PowerLSB = 20 * CurrentLSB
  // PowerLSB = 0.0008 (800�W per bit)
  ina219_powerLsb_mW = 0.8; // in mW

  // 7. Compute the maximum current and shunt voltage values before overflow
  //
  // Max_Current = Current_LSB * 32767
  // Max_Current = 1.31068A before overflow
  //
  // If Max_Current > Max_Possible_I then
  //    Max_Current_Before_Overflow = MaxPossible_I
  // Else
  //    Max_Current_Before_Overflow = Max_Current
  // End If
  //
  // ... In this case, we're good though since Max_Current is less than MaxPossible_I
  //
  // Max_ShuntVoltage = Max_Current_Before_Overflow * RSHUNT
  // Max_ShuntVoltage = 0.131068V
  //
  // If Max_ShuntVoltage >= VSHUNT_MAX
  //    Max_ShuntVoltage_Before_Overflow = VSHUNT_MAX
  // Else
  //    Max_ShuntVoltage_Before_Overflow = Max_ShuntVoltage
  // End If

  // 8. Compute the Maximum Power
  // MaximumPower = Max_Current_Before_Overflow * VBUS_MAX
  // MaximumPower = 1.31068 * 32V
  // MaximumPower = 41.94176W

  // Set Calibration register to 'Cal' calculated above	
  wireWriteRegister(INA219_REG_CALIBRATION, ina219_calValue);

  // Set Config register to take into account the settings above
  uint16_t config = INA219_CONFIG_BVOLTAGERANGE_32V |
                    INA219_CONFIG_GAIN_8_320MV |
                    INA219_CONFIG_BADCRES_12BIT |
                    INA219_CONFIG_SADCRES_12BIT_1S_532US |
                    INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
  wireWriteRegister(INA219_REG_CONFIG, config);
}

void Adafruit_INA219::setCalibration_16V_400mA(void) {
  
  // Calibration which uses the highest precision for 
  // current measurement (0.1mA), at the expense of 
  // only supporting 16V at 400mA max.

  // VBUS_MAX = 16V
  // VSHUNT_MAX = 0.04          (Assumes Gain 1, 40mV)
  // RSHUNT = 0.1               (Resistor value in ohms)
  
  // 1. Determine max possible current
  // MaxPossible_I = VSHUNT_MAX / RSHUNT
  // MaxPossible_I = 0.4A

  // 2. Determine max expected current
  // MaxExpected_I = 0.4A
  
  // 3. Calculate possible range of LSBs (Min = 15-bit, Max = 12-bit)
  // MinimumLSB = MaxExpected_I/32767
  // MinimumLSB = 0.0000122              (12uA per bit)
  // MaximumLSB = MaxExpected_I/4096
  // MaximumLSB = 0.0000977              (98uA per bit)
  
  // 4. Choose an LSB between the min and max values
  //    (Preferrably a roundish number close to MinLSB)
  // CurrentLSB = 0.00005 (50uA per bit)
  ina219_currentLsb_mA = 0.05; // in mA
  
  // 5. Compute the calibration register
  // Cal = trunc (0.04096 / (Current_LSB * RSHUNT))
  // Cal = 8192 (0x2000)

  ina219_calValue = 8192;

  // 6. Calculate the power LSB
  // PowerLSB = 20 * CurrentLSB
  // PowerLSB = 0.001 (1mW per bit)
  ina219_powerLsb_mW = 1; // in mW

  // 7. Compute the maximum current and shunt voltage values before overflow
  //
  // Max_Current = Current_LSB * 32767
  // Max_Current = 1.63835A before overflow
  //
  // If Max_Current > Max_Possible_I then
  //    Max_Current_Before_Overflow = MaxPossible_I
  // Else
  //    Max_Current_Before_Overflow = Max_Current
  // End If
  //
  // Max_Current_Before_Overflow = MaxPossible_I
  // Max_Current_Before_Overflow = 0.4
  //
  // Max_ShuntVoltage = Max_Current_Before_Overflow * RSHUNT
  // Max_ShuntVoltage = 0.04V
  //
  // If Max_ShuntVoltage >= VSHUNT_MAX
  //    Max_ShuntVoltage_Before_Overflow = VSHUNT_MAX
  // Else
  //    Max_ShuntVoltage_Before_Overflow = Max_ShuntVoltage
  // End If
  //
  // Max_ShuntVoltage_Before_Overflow = VSHUNT_MAX
  // Max_ShuntVoltage_Before_Overflow = 0.04V
  
  // 8. Compute the Maximum Power
  // MaximumPower = Max_Current_Before_Overflow * VBUS_MAX
  // MaximumPower = 0.4 * 16V
  // MaximumPower = 6.4W
  
  // Set Calibration register to 'Cal' calculated above 
  wireWriteRegister(INA219_REG_CALIBRATION, ina219_calValue);
  
  // Set Config register to take into account the settings above
  uint16_t config = INA219_CONFIG_BVOLTAGERANGE_16V |
                    INA219_CONFIG_GAIN_1_40MV |
                    INA219_CONFIG_BADCRES_12BIT |
                    INA219_CONFIG_SADCRES_12BIT_1S_532US |
                    INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
  wireWriteRegister(INA219_REG_CONFIG, config);
}

/**************************************************************************/
/*! 
    @brief  Configures to INA219 to be able to measure up to user
            defined values.
            Give Shut Value in Ohms
            Max V (V) value across shunt
            Max Bus V (V) value
            Max I (A) expected
            This function is doing the calculations
            Fonction based on https://github.com/flav1972/ArduinoINA219 & co
			
    @note   Use this fonction if you replace the default shunt (0.1 ohm)
*/
/**************************************************************************/
void Adafruit_INA219::setCalibration_Def(float r_shunt, float v_shunt_max, float v_bus_max, float i_max_expected)
{
  uint16_t cal, digits, bvoltage, gain;
  float i_max_possible, min_lsb, max_lsb, swap;
  float current_lsb, power_lsb;
#if (INA219_DEBUG == 1)
  float max_current,max_before_overflow,max_shunt_v,max_shunt_v_before_overflow,max_power;
#endif

  // By default we use a pretty huge range for the input voltage,
  // which probably isn't the most appropriate choice for system
  // that don't use a lot of power.  But all of the calculations
  // are shown below if you want to change the settings.  You will
  // also need to change any relevant register settings, such as
  // setting the VBUS_MAX to 16V instead of 32V, etc.

  // VBUS_MAX = v_bus_max          (user defined arg)
  // VSHUNT_MAX = v_shunt_max      (user defined arg)
  // RSHUNT = r_shunt              (user defined arg)
  
  // 1. Determine max possible current
  // MaxPossible_I = VSHUNT_MAX / RSHUNT
  i_max_possible = v_shunt_max / r_shunt;
  
  // 2. Determine max expected current
  // MaxExpected_I = i_max_expected (user defined arg)
  
  // 3. Calculate possible range of LSBs (Min = 15-bit, Max = 12-bit)
  // MinimumLSB = MaxExpected_I/32767
  // MaximumLSB = MaxExpected_I/4096
  min_lsb = i_max_expected / 32767;
  max_lsb = i_max_expected / 4096;
    
  // 4. Choose an LSB between the min and max values
  //    (Preferrably a roundish number close to MinLSB)
  current_lsb = min_lsb;
  digits=0;

  /* From datasheet: This value was selected to be a round number near the Minimum_LSB.
   * This selection allows for good resolution with a rounded LSB.
   * eg. 0.000610 -> 0.000700
   */
  while( current_lsb > 0.0 ){//If zero there is something weird...
      if( (uint16_t)current_lsb / 1){
      	current_lsb = (uint16_t) current_lsb + 1;
      	current_lsb /= pow(10,digits);
      	break;
      }
      else{
       	digits++;
        current_lsb *= 10.0;
      }
  };
  ina219_currentLsb_mA = current_lsb*1000;
  
  // 5. Compute the calibration register
  // Cal = trunc (0.04096 / (Current_LSB * RSHUNT))
  swap = (0.04096)/(current_lsb*r_shunt);
  cal = (uint16_t)swap;

  ina219_calValue = cal;
  
  // 6. Calculate the power LSB
  // PowerLSB = 20 * CurrentLSB
  power_lsb = current_lsb * 20;
  ina219_powerLsb_mW = power_lsb*1000;
  
  // 7. Compute the maximum current and shunt voltage values before overflow
  //
  // Max_Current = Current_LSB * 32767
  //
  // If Max_Current > Max_Possible_I then
  //    Max_Current_Before_Overflow = MaxPossible_I
  // Else
  //    Max_Current_Before_Overflow = Max_Current
  // End If
  //
  // Max_ShuntVoltage = Max_Current_Before_Overflow * RSHUNT
  //
  // If Max_ShuntVoltage >= VSHUNT_MAX
  //    Max_ShuntVoltage_Before_Overflow = VSHUNT_MAX
  // Else
  //    Max_ShuntVoltage_Before_Overflow = Max_ShuntVoltage
  // End If
  //
  // 8. Compute the Maximum Power
  // MaximumPower = Max_Current_Before_Overflow * VBUS_MAX
#if (INA219_DEBUG == 1)
  max_current = current_lsb*32767;
  max_before_overflow =  max_current > i_max_possible?i_max_possible:max_current;

  max_shunt_v = max_before_overflow*r_shunt;
  max_shunt_v_before_overflow = max_shunt_v > v_shunt_max?v_shunt_max:max_shunt_v;

  max_power = v_bus_max * max_before_overflow;
  Serial.print("v_bus_max:      "); Serial.println(v_bus_max, 8);
  Serial.print("v_shunt_max:    "); Serial.println(v_shunt_max, 8);
  Serial.print("i_max_possible: "); Serial.println(i_max_possible, 8);
  Serial.print("i_max_expected: "); Serial.println(i_max_expected, 8);
  Serial.print("min_lsb:        "); Serial.println(min_lsb, 12);
  Serial.print("max_lsb:        "); Serial.println(max_lsb, 12);
  Serial.print("current_lsb:    "); Serial.println(current_lsb, 12);
  Serial.print("power_lsb:      "); Serial.println(power_lsb, 8);
  Serial.println("  ");
  Serial.print("cal:            "); Serial.println(cal);
  Serial.print("r_shunt:        "); Serial.println(r_shunt);
  Serial.print("max_before_overflow:         "); Serial.println(max_before_overflow,8);
  Serial.print("max_shunt_v_before_overflow: "); Serial.println(max_shunt_v_before_overflow,8);
  Serial.print("max_power:      "); Serial.println(max_power,8);
  Serial.println("  ");
#endif

  // Set Calibration register to 'Cal' calculated above	
  wireWriteRegister(INA219_REG_CALIBRATION, ina219_calValue);

  // sets the voltage rage more accurate with v_bus_max
  bvoltage = (v_bus_max > 16) ? INA219_CONFIG_BVOLTAGERANGE_32V : INA219_CONFIG_BVOLTAGERANGE_16V;
  
  // sets the gain to be the lowest possible depending on v_shunt_max expected
  if(v_shunt_max <= 0.04)
    gain = INA219_CONFIG_GAIN_1_40MV; // Gain 1, 40mV Range
  else if(v_shunt_max <= 0.08)
    gain = INA219_CONFIG_GAIN_2_80MV; // Gain 2, 80mV Range
  else if(v_shunt_max <= 0.160)
    gain = INA219_CONFIG_GAIN_4_160MV; // Gain 4, 160mV Range
  else
    gain = INA219_CONFIG_GAIN_8_320MV; // Gain 8, 320mV Range

#if (INA219_DEBUG == 1)
  Serial.print("BVOLTAGERANGE: 0x"); Serial.println(bvoltage, HEX);
  Serial.print("PGA GAIN:      0x"); Serial.println(gain, HEX);
  Serial.println("  ");
#endif

  // Set Config register to take into account the settings above
  uint16_t config = bvoltage |
                    gain |
                    INA219_CONFIG_BADCRES_12BIT |
                    INA219_CONFIG_SADCRES_12BIT_1S_532US |
                    INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS;
  wireWriteRegister(INA219_REG_CONFIG, config);
}

/**************************************************************************/
/*! 
    @brief  Instantiates a new INA219 class
*/
/**************************************************************************/
Adafruit_INA219::Adafruit_INA219(uint8_t addr) {
  ina219_i2caddr = addr;
  ina219_currentLsb_mA = 0;
  ina219_powerLsb_mW = 0;
}

/**************************************************************************/
/*! 
    @brief  Setups the HW (defaults to 32V and 2A for calibration values)
*/
/**************************************************************************/
void Adafruit_INA219::begin(uint8_t addr) {
  ina219_i2caddr = addr;
  begin();
}

void Adafruit_INA219::begin(void) {
  Wire.begin();    
  // Set chip to large range config values to start
  setCalibration_32V_2A();
}

/**************************************************************************/
/*! 
    @brief  Gets the raw bus voltage (16-bit signed integer, so +-32767)
*/
/**************************************************************************/
int16_t Adafruit_INA219::getBusVoltage_raw() {
  uint16_t value;
  wireReadRegister(INA219_REG_BUSVOLTAGE, &value);

  // Shift to the right 3 to drop CNVR and OVF and multiply by LSB
  return (int16_t)((value >> 3) * 4);
}

/**************************************************************************/
/*! 
    @brief  Gets the raw shunt voltage (16-bit signed integer, so +-32767)
*/
/**************************************************************************/
int16_t Adafruit_INA219::getShuntVoltage_raw() {
  uint16_t value;
  wireReadRegister(INA219_REG_SHUNTVOLTAGE, &value);
  return (int16_t)value;
}

/**************************************************************************/
/*! 
    @brief  Gets the raw current value (16-bit signed integer, so +-32767)
*/
/**************************************************************************/
int16_t Adafruit_INA219::getCurrent_raw() {
  uint16_t value;

  // Sometimes a sharp load will reset the INA219, which will
  // reset the cal register, meaning CURRENT and POWER will
  // not be available ... avoid this by always setting a cal
  // value even if it's an unfortunate extra step
  wireWriteRegister(INA219_REG_CALIBRATION, ina219_calValue);

  // Now we can safely read the CURRENT register!
  wireReadRegister(INA219_REG_CURRENT, &value);
  
  return (int16_t)value;
}
 
/**************************************************************************/
/*! 
    @brief  Gets the raw power value (16-bit signed integer, so +-32767)
*/
/**************************************************************************/
int16_t Adafruit_INA219::getPower_raw() {
  uint16_t value;
  wireReadRegister(INA219_REG_POWER, &value);
  return (int16_t)value;
}
 
/**************************************************************************/
/*! 
    @brief  Gets the shunt voltage in mV (so +-327mV)
*/
/**************************************************************************/
float Adafruit_INA219::getShuntVoltage_mV() {
  int16_t value;
  value = getShuntVoltage_raw();
  return value * 0.01;
}

/**************************************************************************/
/*! 
    @brief  Gets the shunt voltage in volts
*/
/**************************************************************************/
float Adafruit_INA219::getBusVoltage_V() {
  int16_t value = getBusVoltage_raw();
  return value * 0.001;
}

/**************************************************************************/
/*! 
    @brief  Gets the current value in mA, taking into account the
            config settings and current LSB
*/
/**************************************************************************/
float Adafruit_INA219::getCurrent_mA() {
  float valueDec = getCurrent_raw();
  valueDec *= ina219_currentLsb_mA;
  return valueDec;
}

/**************************************************************************/
/*! 
    @brief  Gets the power value in mW, taking into account the
            config settings and current LSB
*/
/**************************************************************************/
float Adafruit_INA219::getPower_mW() {
  float valueDec = getPower_raw(); //
  valueDec *= ina219_powerLsb_mW;
  return valueDec;
}

/**************************************************************************/
/*! 
    @brief  Change the config register so that the INA219 returns current
            measurement values from single samples.
*/
/**************************************************************************/
void Adafruit_INA219::setAmpInstant() {
  uint16_t value;

  // get the current configuration data
  wireReadRegister(INA219_REG_CONFIG, &value);

  // change out the bits to average 1 samples at 12 bits per sample
  value = value & ~INA219_CONFIG_SADCRES_MASK | INA219_CONFIG_SADCRES_12BIT_1S_532US ;

  // write the changed config value back again
  wireWriteRegister(INA219_REG_CONFIG, value);
}

/**************************************************************************/
/*! 
    @brief  Change the config register so that the INA219 returns a current
            measurement value that is an average over 128 samples.
*/
/**************************************************************************/
void Adafruit_INA219::setAmpAverage() {
  uint16_t value;

  // get the current configuration data
  wireReadRegister(INA219_REG_CONFIG, &value);

  // change out the bits to average 128 samples at 12 bits per sample
  value = value & ~INA219_CONFIG_SADCRES_MASK | INA219_CONFIG_SADCRES_12BIT_128S_69MS;

  // write the changed config value back again
  wireWriteRegister(INA219_REG_CONFIG, value);

  delay(69); // Max 12-bit 128S conversion time is 69mS per sample, but
  // read can happen more frequently
}

/**************************************************************************/
/*! 
    @brief  Change the config register so that the INA219 returns voltage
            measurement values from single samples.
*/
/**************************************************************************/
void Adafruit_INA219::setVoltInstant() {
  uint16_t value;

  // get the current configuration data
  wireReadRegister(INA219_REG_CONFIG, &value);

  // change out the bits to average 1 samples at 12 bits per sample
  value = value & ~INA219_CONFIG_BADCRES_MASK | INA219_CONFIG_BADCRES_12BIT ;

  // write the changed config value back again
  wireWriteRegister(INA219_REG_CONFIG, value);
}

/**************************************************************************/
/*! 
    @brief  Change the config register so that the INA219 returns a current
            measurement value that is an average over 128 samples.
*/
/**************************************************************************/
void Adafruit_INA219::setVoltAverage() {
  uint16_t value;

  // get the current configuration data
  wireReadRegister(INA219_REG_CONFIG, &value);

  // change out the bits to average 128 samples at 12 bits per sample
  value = value & ~INA219_CONFIG_BADCRES_MASK | INA219_CONFIG_BADCRES_12BIT_128S_69MS ;

  // write the changed config value back again
  wireWriteRegister(INA219_REG_CONFIG, value);

  delay(69); // Max 12-bit 128S conversion time is 69mS per sample, but
  // read can happen more frequently
}

