#include "DryerSettings.h"

void DryerSettings::Reset()
{
  memset(this, 0, sizeof(*this));

  version = SETTINGS_VERSION;

  target_temperature = TEMPERATURE_TARGET;
  target_humidity    = 50.0f;
  water_target       = WATER_TARGET_DEFAULT;

  hydraulic_enabled = HYDRAULIC_ENABLED_DEFAULT;
  electric_enabled  = ELECTRIC_ENABLED_DEFAULT;

  heat_source                = HEAT_SOURCE_DEFAULT;
  dehum_extraction_threshold = DEHUM_EXTRACTION_THRESHOLD_DEFAULT;

  program = DRYER_PROGRAM_DEFAULT;

  eco_enabled           = false;
  eco_start_hour        = ECO_START_HOUR;
  eco_end_hour          = ECO_END_HOUR;
  eco_target_percentage = ECO_NIGHT_TARGET_PERCENTAGE;

  init_phase_duration             = INIT_PHASE_DURATION;
  brassage_phase_duration         = BRASSAGE_PHASE_DURATION;
  extraction_phase_duration       = EXTRACTION_PHASE_DURATION;
  extraction_damper_open_duration = EXTRACTION_DAMPER_OPEN_DURATION;

  band_electric      = CTRL_BANDE_ELEC;
  horizon_electric   = CTRL_HORIZON;
  electric_t_on_min  = CTRL_T_ON_MIN;
  electric_t_off_min = CTRL_T_OFF_MIN;
  air_renewal_window = CTRL_AIR_RENEWAL_S;
  safety_max         = TEMPERATURE_SAFETY_MAX;

  // One register out of the box: a dryer that has never been configured behaves
  // exactly as the firmware did before the count was a setting, and declaring
  // the second one is a deliberate act by whoever landed its wire.
  damper_count                = DAMPER_COUNT_DEFAULT;
  damper_feedback_low_is_open = DAMPER_FEEDBACK_LOW_IS_OPEN_DEFAULT;
  damper_extraction_inverted  = DAMPER_EXTRACTION_INVERTED_DEFAULT;
  damper_recycling_inverted   = DAMPER_RECYCLING_INVERTED_DEFAULT;

  // Both registers start from the divider's theoretical ends. They are
  // asymmetric, so these only hold until each one is calibrated from the menu.
  extraction_raw_min = DAMPER_RAW_MIN_DEFAULT;
  extraction_raw_max = DAMPER_RAW_MAX_DEFAULT;
  recycling_raw_min  = DAMPER_RAW_MIN_DEFAULT;
  recycling_raw_max  = DAMPER_RAW_MAX_DEFAULT;

  // The extension port is the dryer's own connector and costs nothing when
  // nothing is plugged into it; the radio is off until someone asks for it.
  telemetry_rs485 = true;
  telemetry_wifi  = false;

  SealRecord(*this);
}

void SessionSnapshot::Reset()
{
  memset(this, 0, sizeof(*this));
  version = SESSION_VERSION;
  SealRecord(*this);
}
