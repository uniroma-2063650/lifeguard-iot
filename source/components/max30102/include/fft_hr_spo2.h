#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

std::pair<std::optional<uint32_t>, std::optional<uint32_t>>
calc_hr_spo2_fft(const uint32_t *red_buffer, const uint32_t *ir_buffer,
                 size_t len, float sample_rate, float cutoff_bpm_low,
                 float decay_bps_high);
