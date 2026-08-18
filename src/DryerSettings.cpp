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

  eco_enabled           = false;
  eco_start_hour        = ECO_START_HOUR;
  eco_end_hour          = ECO_END_HOUR;
  eco_target_percentage = ECO_NIGHT_TARGET_PERCENTAGE;

  init_phase_duration             = INIT_PHASE_DURATION;
  brassage_phase_duration         = BRASSAGE_PHASE_DURATION;
  extraction_phase_duration       = EXTRACTION_PHASE_DURATION;
  extraction_damper_open_duration = EXTRACTION_DAMPER_OPEN_DURATION;

  band_hydraulic      = CTRL_BANDE_HYDRO;
  band_electric       = CTRL_BANDE_ELEC;
  horizon_hydraulic   = CTRL_HYDRO_HORIZON;
  horizon_electric    = CTRL_HORIZON;
  hydraulic_t_on_min  = CTRL_HYDRO_T_ON_MIN;
  hydraulic_t_off_min = CTRL_HYDRO_T_OFF_MIN;
  electric_t_on_min   = CTRL_T_ON_MIN;
  electric_t_off_min  = CTRL_T_OFF_MIN;
  safety_max          = TEMPERATURE_SAFETY_MAX;

  // Both registers start from the divider's theoretical end stops. They are
  // asymmetric, so these only hold until each one is calibrated from the menu.
  extraction_raw_closed = DAMPER_RAW_CLOSED_DEFAULT;
  extraction_raw_open   = DAMPER_RAW_OPEN_DEFAULT;
  recycling_raw_closed  = DAMPER_RAW_CLOSED_DEFAULT;
  recycling_raw_open    = DAMPER_RAW_OPEN_DEFAULT;

  lora_telemetry_interval_ms = LORA_TELEMETRY_INTERVAL_MS;

  SealRecord(*this);
}

void SessionSnapshot::Reset()
{
  memset(this, 0, sizeof(*this));
  version = SESSION_VERSION;
  SealRecord(*this);
}
