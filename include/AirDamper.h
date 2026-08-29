#ifndef AIR_DAMPER_H
#define AIR_DAMPER_H

#include <Arduino.h>
#include "config.h"
#include "DamperFeedback.h"

// How the dryer's registers are wired, pushed in from the settings record.
//
// A single struct rather than a setter per field: every one of these is read
// together whenever a target or a position is computed, and applying half of a
// new configuration would leave the two registers describing different machines.
struct DamperConfig
{
  uint8_t  count;                 // 1 or DAMPER_COUNT_MAX
  bool     feedback_low_is_open;  // actuator model, read on a Normal register
  bool     extraction_inverted;   // each actuator's own direction switch
  bool     recycling_inverted;
  uint16_t extraction_raw_min;
  uint16_t extraction_raw_max;
  uint16_t recycling_raw_min;
  uint16_t recycling_raw_max;
};

// The air path — one or two Belimo LM24A-SR registers driven by one command.
//
// The command is strictly binary: recirculation or extraction. With two
// registers they are **complementary**, since air is either extracted or
// recycled and never both, so a single relay drives both actuators with one of
// them wired to travel the other way. Which way each one travels is not assumed
// here any more: each Belimo carries a mechanical direction switch, and where it
// is set is a setting. The translation from command to per-register target
// happens in PushTargets() and nowhere else — v3's mistake was inverting the
// damper in two separate places, which made the real direction impossible to
// follow.
//
// The registers are **asymmetric** in geometry, so each carries its own
// calibration and reports its own opening: see DamperFeedback. Control of the
// air path depends on none of it — but the airflow interlock does, and that is
// the one place a reading has ever been allowed to stop the dryer.
//
// The direction switch also turns the feedback round. That is the Belimo's
// behaviour and it is easy to get wrong: the U output does not report a
// mechanical angle in some absolute frame, it reports the position *in the
// actuator's own frame*, which the switch mirrors along with the travel. A
// complementary pair therefore puts out the **same** voltage on both channels —
// one register wide open, the other shut, both reading 10.10V. So the open end
// of the signal is not one setting for the whole dryer: it is the actuator
// model's sense, turned round again on each register whose switch is inverted.
// That combination is applied in ApplyConfig(), next to PushTargets(), so both
// halves of what the switch does are decided in the same place.
class AirDamper
{
public:
  AirDamper();

  void Open();   // extraction
  void Close();  // recirculation
  bool IsOpen() const { return is_open_; }

  // Apply a wiring configuration. Targets are re-pushed immediately, so a
  // direction changed from the menu takes effect without waiting for the next
  // command.
  void    ApplyConfig(const DamperConfig &config);
  uint8_t GetCount() const { return count_; }
  bool    GetFeedbackLowIsOpen() const { return feedback_low_is_open_; }
  bool    GetExtractionInverted() const { return extraction_inverted_; }
  bool    GetRecyclingInverted() const { return recycling_inverted_; }

  // The registers, each with its own calibration, sample and opening.
  DamperFeedback       &Extraction()       { return extraction_; }
  const DamperFeedback &Extraction() const { return extraction_; }
  DamperFeedback       &Recycling()        { return recycling_; }
  const DamperFeedback &Recycling() const  { return recycling_; }

  // True while a register is still travelling. Every register is driven by the
  // same relay, so they move together and stop within a few seconds of each
  // other. A register the dryer does not have is not consulted.
  bool IsMoving() const;

  // --- Airflow interlock ---
  //
  // Call once per feedback sampling round, after the raw values have been pushed
  // in: it is what times the confirmation window.
  void UpdateInterlock();

  // True once both registers have read shut for DAMPER_BLOCKED_CONFIRM_MS. Only
  // possible with two registers — one register cannot close the air path on its
  // own. Nothing latches: a good reading clears it immediately.
  bool IsAirflowBlocked() const { return airflow_blocked_; }

  // True when every register the dryer claims to have is reporting a usable
  // opening. False means the interlock cannot be evaluated at all, which is why
  // it blocks a start rather than being ignored.
  bool IsFeedbackUsable() const;

private:
  bool           is_open_;
  uint8_t        count_;
  bool           feedback_low_is_open_;
  bool           extraction_inverted_;
  bool           recycling_inverted_;
  DamperFeedback extraction_;
  DamperFeedback recycling_;

  // Confirmation window for the interlock: when the two registers first read
  // shut together, and whether that has lasted long enough to be believed.
  uint32_t both_closed_since_ms_;
  bool     both_closed_;
  bool     airflow_blocked_;

  // Translate the command into each register's target, applying that register's
  // direction switch. The single place where a direction is interpreted.
  void PushTargets();
};

#endif // AIR_DAMPER_H
