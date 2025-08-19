#include "nexussynth/midi_phoneme_integrator.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <numeric>

namespace nexussynth {
namespace midi {

    // =================== TempoMap Implementation ===================
    
    TempoMap::TempoMap() : ppqn_(480) {
        // Add default tempo (120 BPM) at tick 0
        tempo_events_.emplace_back(0, 120.0);
    }
    
    TempoMap::TempoMap(uint16_t ppqn) : ppqn_(ppqn) {
        tempo_events_.emplace_back(0, 120.0);
    }
    
    void TempoMap::addTempoEvent(const TempoEvent& event) {
        tempo_events_.push_back(event);
        sortTempoEvents();
    }
    
    double TempoMap::ticksToSeconds(uint32_t ticks) const {
        if (tempo_events_.empty()) {
            return 0.0;
        }
        
        double accumulated_time = 0.0;
        uint32_t last_tick = 0;
        
        for (const auto& event : tempo_events_) {
            if (event.tick > ticks) {
                // Calculate remaining time with previous tempo
                uint32_t remaining_ticks = ticks - last_tick;
                double seconds_per_tick = event.microseconds_per_quarter / (1000000.0 * ppqn_);
                accumulated_time += remaining_ticks * seconds_per_tick;
                break;
            }
            
            if (event.tick > last_tick) {
                // Calculate time elapsed at previous tempo
                uint32_t elapsed_ticks = event.tick - last_tick;
                double seconds_per_tick = (last_tick == 0 && tempo_events_.size() > 0) ? 
                    tempo_events_[0].microseconds_per_quarter / (1000000.0 * ppqn_) :
                    event.microseconds_per_quarter / (1000000.0 * ppqn_);
                accumulated_time += elapsed_ticks * seconds_per_tick;
            }
            
            last_tick = event.tick;
        }
        
        // Handle case where ticks is beyond last tempo event
        if (ticks > last_tick) {
            uint32_t remaining_ticks = ticks - last_tick;
            double seconds_per_tick = tempo_events_.back().microseconds_per_quarter / (1000000.0 * ppqn_);
            accumulated_time += remaining_ticks * seconds_per_tick;
        }
        
        return accumulated_time;
    }
    
    double TempoMap::ticksToMilliseconds(uint32_t ticks) const {
        return ticksToSeconds(ticks) * 1000.0;
    }
    
    uint32_t TempoMap::secondsToTicks(double seconds) const {
        if (tempo_events_.empty() || seconds <= 0.0) {
            return 0;
        }
        
        double accumulated_time = 0.0;
        uint32_t accumulated_ticks = 0;
        
        for (size_t i = 0; i < tempo_events_.size(); ++i) {
            const auto& event = tempo_events_[i];
            double seconds_per_tick = event.microseconds_per_quarter / (1000000.0 * ppqn_);
            
            // Calculate time to next tempo event (or end)
            uint32_t next_tick = (i + 1 < tempo_events_.size()) ? 
                tempo_events_[i + 1].tick : UINT32_MAX;
            
            uint32_t ticks_available = (next_tick == UINT32_MAX) ? 
                static_cast<uint32_t>((seconds - accumulated_time) / seconds_per_tick) :
                next_tick - event.tick;
            
            double time_available = ticks_available * seconds_per_tick;
            
            if (accumulated_time + time_available >= seconds) {
                // Target time is within this tempo segment
                double remaining_time = seconds - accumulated_time;
                uint32_t remaining_ticks = static_cast<uint32_t>(remaining_time / seconds_per_tick);
                return accumulated_ticks + remaining_ticks;
            }
            
            accumulated_time += time_available;
            accumulated_ticks += ticks_available;
        }
        
        return accumulated_ticks;
    }
    
    uint32_t TempoMap::millisecondsToTicks(double milliseconds) const {
        return secondsToTicks(milliseconds / 1000.0);
    }
    
    double TempoMap::getTempoAtTick(uint32_t tick) const {
        if (tempo_events_.empty()) {
            return 120.0; // Default tempo
        }
        
        for (auto it = tempo_events_.rbegin(); it != tempo_events_.rend(); ++it) {
            if (it->tick <= tick) {
                return it->bpm;
            }
        }
        
        return tempo_events_[0].bpm;
    }
    
    double TempoMap::getTempoAtTime(double seconds) const {
        uint32_t tick = secondsToTicks(seconds);
        return getTempoAtTick(tick);
    }
    
    void TempoMap::clear() {
        tempo_events_.clear();
        tempo_events_.emplace_back(0, 120.0); // Reset to default
    }
    
    void TempoMap::sortTempoEvents() {
        std::sort(tempo_events_.begin(), tempo_events_.end(),
                 [](const TempoEvent& a, const TempoEvent& b) {
                     return a.tick < b.tick;
                 });
    }
    
    std::pair<size_t, size_t> TempoMap::findTempoRange(uint32_t tick) const {
        auto it = std::upper_bound(tempo_events_.begin(), tempo_events_.end(), tick,
                                  [](uint32_t t, const TempoEvent& event) {
                                      return t < event.tick;
                                  });
        
        size_t end_idx = it - tempo_events_.begin();
        size_t start_idx = (end_idx > 0) ? end_idx - 1 : 0;
        
        return {start_idx, end_idx};
    }

    // =================== MidiParser Implementation ===================
    
    MidiParser::ParseResult MidiParser::parseFile(const std::string& filename) {
        ParseResult result;
        
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            result.errors.push_back("Failed to open MIDI file: " + filename);
            return result;
        }
        
        // Read entire file into buffer
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> buffer(file_size);
        file.read(reinterpret_cast<char*>(buffer.data()), file_size);
        
        if (file.gcount() != static_cast<std::streamsize>(file_size)) {
            result.errors.push_back("Failed to read complete MIDI file");
            return result;
        }
        
        return parseFromBuffer(buffer);
    }
    
    MidiParser::ParseResult MidiParser::parseFromBuffer(const std::vector<uint8_t>& data) {
        ParseResult result;
        current_tick_ = 0;
        running_status_ = 0;
        
        if (data.size() < 14) {
            result.errors.push_back("MIDI file too small");
            return result;
        }
        
        size_t offset = 0;
        
        // Parse header
        if (!parseHeader(data.data(), offset, result)) {
            return result;
        }
        
        // Parse tracks
        uint16_t track_count = 0;
        while (offset < data.size() && track_count < 65536) {
            // Check for track header "MTrk"
            if (offset + 8 > data.size()) break;
            
            if (data[offset] == 'M' && data[offset + 1] == 'T' && 
                data[offset + 2] == 'r' && data[offset + 3] == 'k') {
                
                offset += 4;
                uint32_t track_length = readUint32BE(data.data(), offset);
                offset += 4;
                
                if (offset + track_length > data.size()) {
                    result.errors.push_back("Invalid track length");
                    break;
                }
                
                current_tick_ = 0; // Reset for each track
                if (!parseTrack(data.data(), offset, track_length, result)) {
                    result.errors.push_back("Failed to parse track " + std::to_string(track_count));
                }
                
                track_count++;
            } else {
                break;
            }
        }
        
        // Build tempo map
        for (const auto& tempo_event : result.tempo_events) {
            result.tempo_map.addTempoEvent(tempo_event);
        }
        result.tempo_map.setPPQN(result.ppqn);
        
        // Calculate note timing in milliseconds
        for (auto& note : result.notes) {
            note.start_time_ms = result.tempo_map.ticksToMilliseconds(note.start_tick);
            note.duration_ms = result.tempo_map.ticksToMilliseconds(note.duration_ticks);
            note.frequency_hz = midiNoteToFrequency(note.note_number);
        }
        
        // Calculate CC and pitch bend timing
        for (auto& cc : result.cc_events) {
            cc.time_ms = result.tempo_map.ticksToMilliseconds(cc.tick);
        }
        
        for (auto& bend : result.pitch_bend_events) {
            bend.time_ms = result.tempo_map.ticksToMilliseconds(bend.tick);
            bend.semitones = (bend.value / 8192.0) * 2.0; // ±2 semitones default range
        }
        
        // Calculate total duration
        if (!result.notes.empty()) {
            auto last_note = std::max_element(result.notes.begin(), result.notes.end(),
                [](const MidiNote& a, const MidiNote& b) {
                    return (a.start_time_ms + a.duration_ms) < (b.start_time_ms + b.duration_ms);
                });
            result.total_duration_ms = last_note->start_time_ms + last_note->duration_ms;
        }
        
        result.success = result.errors.empty();
        return result;
    }
    
    double MidiParser::midiNoteToFrequency(uint8_t note_number) {
        // A4 (MIDI note 69) = 440 Hz
        return 440.0 * std::pow(2.0, (note_number - 69) / 12.0);
    }
    
    uint8_t MidiParser::frequencyToMidiNote(double frequency) {
        if (frequency <= 0.0) return 0;
        
        double note_number = 69.0 + 12.0 * std::log2(frequency / 440.0);
        return static_cast<uint8_t>(std::round(std::clamp(note_number, 0.0, 127.0)));
    }
    
    std::string MidiParser::midiNoteToName(uint8_t note_number) {
        static const char* note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        int octave = (note_number / 12) - 1;
        int note = note_number % 12;
        return std::string(note_names[note]) + std::to_string(octave);
    }
    
    bool MidiParser::parseHeader(const uint8_t* data, size_t& offset, ParseResult& result) {
        // Check "MThd" signature
        if (data[offset] != 'M' || data[offset + 1] != 'T' || 
            data[offset + 2] != 'h' || data[offset + 3] != 'd') {
            result.errors.push_back("Invalid MIDI header signature");
            return false;
        }
        offset += 4;
        
        // Header length (should be 6)
        uint32_t header_length = readUint32BE(data, offset);
        offset += 4;
        
        if (header_length != 6) {
            result.errors.push_back("Invalid MIDI header length");
            return false;
        }
        
        // Format type (0, 1, or 2)
        uint16_t format = readUint16BE(data, offset);
        offset += 2;
        
        if (format > 2) {
            result.errors.push_back("Unsupported MIDI format: " + std::to_string(format));
            return false;
        }
        
        // Number of tracks
        uint16_t track_count = readUint16BE(data, offset);
        offset += 2;
        
        // Time division (PPQN)
        uint16_t time_division = readUint16BE(data, offset);
        offset += 2;
        
        if (time_division & 0x8000) {
            result.errors.push_back("SMPTE time division not supported");
            return false;
        }
        
        result.ppqn = time_division;
        return true;
    }
    
    bool MidiParser::parseTrack(const uint8_t* data, size_t& offset, size_t track_length, ParseResult& result) {
        size_t track_end = offset + track_length;
        
        while (offset < track_end) {
            // Read delta time
            uint32_t delta_time = readVariableLength(data, offset);
            current_tick_ += delta_time;
            
            if (offset >= track_end) break;
            
            uint8_t event_type = data[offset];
            
            if (event_type == 0xFF) {
                // Meta event
                if (!parseMetaEvent(data, offset, result)) {
                    return false;
                }
            } else if (event_type == 0xF0 || event_type == 0xF7) {
                // SysEx event (skip)
                offset++;
                uint32_t length = readVariableLength(data, offset);
                offset += length;
            } else {
                // MIDI event
                if (!parseMidiEvent(data, offset, result)) {
                    return false;
                }
            }
        }
        
        return true;
    }
    
    bool MidiParser::parseMetaEvent(const uint8_t* data, size_t& offset, ParseResult& result) {
        offset++; // Skip 0xFF
        
        if (offset >= result.notes.size()) return false;
        
        uint8_t meta_type = data[offset++];
        uint32_t length = readVariableLength(data, offset);
        
        switch (meta_type) {
            case 0x51: // Set Tempo
                if (length == 3) {
                    uint32_t microseconds_per_quarter = 
                        (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
                    double bpm = 60000000.0 / microseconds_per_quarter;
                    
                    TempoEvent tempo_event(current_tick_, bpm);
                    result.tempo_events.push_back(tempo_event);
                }
                break;
                
            case 0x05: // Lyric
            case 0x01: // Text
                {
                    std::string text(reinterpret_cast<const char*>(data + offset), length);
                    result.lyrics.push_back(text);
                }
                break;
                
            default:
                // Skip unknown meta events
                break;
        }
        
        offset += length;
        return true;
    }
    
    bool MidiParser::parseMidiEvent(const uint8_t* data, size_t& offset, ParseResult& result) {
        uint8_t status = data[offset];
        
        // Handle running status
        if (status < 0x80) {
            status = running_status_;
        } else {
            running_status_ = status;
            offset++;
        }
        
        uint8_t channel = status & 0x0F;
        uint8_t command = status & 0xF0;
        
        switch (command) {
            case 0x90: // Note On
            case 0x80: // Note Off
                {
                    uint8_t note = data[offset++];
                    uint8_t velocity = data[offset++];
                    
                    if (command == 0x90 && velocity > 0) {
                        // Note On
                        MidiNote midi_note;
                        midi_note.note_number = note;
                        midi_note.velocity = velocity;
                        midi_note.start_tick = current_tick_;
                        midi_note.duration_ticks = 0; // Will be set when Note Off is found
                        
                        result.notes.push_back(midi_note);
                    } else {
                        // Note Off - find corresponding Note On
                        for (auto it = result.notes.rbegin(); it != result.notes.rend(); ++it) {
                            if (it->note_number == note && it->duration_ticks == 0) {
                                it->duration_ticks = current_tick_ - it->start_tick;
                                break;
                            }
                        }
                    }
                }
                break;
                
            case 0xB0: // Control Change
                {
                    uint8_t controller = data[offset++];
                    uint8_t value = data[offset++];
                    
                    CCEvent cc_event;
                    cc_event.tick = current_tick_;
                    cc_event.controller = controller;
                    cc_event.value = value;
                    
                    result.cc_events.push_back(cc_event);
                }
                break;
                
            case 0xE0: // Pitch Bend
                {
                    uint8_t lsb = data[offset++];
                    uint8_t msb = data[offset++];
                    
                    int16_t pitch_value = ((msb << 7) | lsb) - 8192;
                    
                    PitchBendEvent bend_event;
                    bend_event.tick = current_tick_;
                    bend_event.value = pitch_value;
                    
                    result.pitch_bend_events.push_back(bend_event);
                }
                break;
                
            case 0xC0: // Program Change
            case 0xD0: // Channel Pressure
                offset++; // Skip single data byte
                break;
                
            case 0xA0: // Polyphonic Pressure
                offset += 2; // Skip two data bytes
                break;
                
            default:
                // Unknown command
                offset++;
                break;
        }
        
        return true;
    }
    
    uint32_t MidiParser::readVariableLength(const uint8_t* data, size_t& offset) {
        uint32_t value = 0;
        uint8_t byte;
        
        do {
            byte = data[offset++];
            value = (value << 7) | (byte & 0x7F);
        } while (byte & 0x80);
        
        return value;
    }
    
    uint32_t MidiParser::readUint32BE(const uint8_t* data, size_t offset) {
        return (data[offset] << 24) | (data[offset + 1] << 16) | 
               (data[offset + 2] << 8) | data[offset + 3];
    }
    
    uint16_t MidiParser::readUint16BE(const uint8_t* data, size_t offset) {
        return (data[offset] << 8) | data[offset + 1];
    }

    // =================== MidiPhonemeIntegrator Implementation ===================
    
    MidiPhonemeIntegrator::MidiPhonemeIntegrator() = default;
    
    MidiPhonemeIntegrator::MidiPhonemeIntegrator(const IntegrationOptions& options) 
        : options_(options) {
    }
    
    MidiPhonemeIntegrator::IntegrationResult MidiPhonemeIntegrator::integrateFromMidi(
        const std::string& midi_file, const std::vector<utau::OtoEntry>& oto_entries) {
        
        MidiParser parser;
        auto midi_result = parser.parseFile(midi_file);
        
        if (!midi_result.success) {
            IntegrationResult result;
            result.errors = midi_result.errors;
            return result;
        }
        
        return integrateFromData(midi_result, oto_entries);
    }
    
    MidiPhonemeIntegrator::IntegrationResult MidiPhonemeIntegrator::integrateFromData(
        const MidiParser::ParseResult& midi_data, const std::vector<utau::OtoEntry>& oto_entries) {
        
        IntegrationResult result;
        
        try {
            // Calculate phoneme timings from MIDI and oto data
            auto phoneme_timings = calculatePhonemeTimings(
                midi_data.notes, oto_entries, midi_data.tempo_map);
            
            // Align MIDI notes with phoneme timings
            auto musical_phonemes = alignMidiWithPhonemes(midi_data.notes, phoneme_timings);
            
            // Generate pitch curves and apply musical parameters
            for (auto& phoneme : musical_phonemes) {
                phoneme.pitch_curve = generatePitchCurve(
                    phoneme.midi_note, midi_data.pitch_bend_events, midi_data.cc_events);
                
                mapMusicalParameters(phoneme, midi_data.cc_events, phoneme.timing.start_time_ms);
                
                // Generate VCV information if enabled
                if (options_.generate_vcv_patterns && vcv_recognizer_) {
                    // Find corresponding oto entry for VCV analysis
                    for (const auto& oto : oto_entries) {
                        if (std::abs(oto.offset - phoneme.timing.start_time_ms) < options_.timing_tolerance_ms) {
                            auto vcv_result = vcv_recognizer_->recognizeFromAlias(oto.alias);
                            if (!vcv_result.vcv_segments.empty()) {
                                phoneme.vcv_info = vcv_result.vcv_segments[0];
                            }
                            break;
                        }
                    }
                }
            }
            
            result.musical_phonemes = musical_phonemes;
            result.total_duration_ms = midi_data.total_duration_ms;
            result.timing_accuracy = calculateTimingAccuracy(musical_phonemes);
            
            // Generate corresponding oto entries
            result.generated_oto_entries = midi_utils::musicalPhonemesToOtoEntries(musical_phonemes);
            
            validateIntegrationResult(result);
            result.success = result.errors.empty();
            
        } catch (const std::exception& e) {
            addError(result, "Integration failed: " + std::string(e.what()));
        }
        
        return result;
    }
    
    std::vector<PhonemeTimingInfo> MidiPhonemeIntegrator::calculatePhonemeTimings(
        const std::vector<MidiNote>& midi_notes,
        const std::vector<utau::OtoEntry>& oto_entries,
        const TempoMap& tempo_map) {
        
        std::vector<PhonemeTimingInfo> timings;
        
        // Strategy: Match MIDI notes to oto entries by timing proximity
        for (const auto& note : midi_notes) {
            // Find closest oto entry by timing
            const utau::OtoEntry* best_oto = nullptr;
            double best_distance = std::numeric_limits<double>::max();
            
            for (const auto& oto : oto_entries) {
                double oto_time = oto.offset;
                double distance = std::abs(note.start_time_ms - oto_time);
                
                if (distance < best_distance && distance < options_.timing_tolerance_ms) {
                    best_distance = distance;
                    best_oto = &oto;
                }
            }
            
            if (best_oto) {
                PhonemeTimingInfo timing = calculateSinglePhoneTiming(note, *best_oto, tempo_map);
                
                // Extract phoneme from lyric or alias
                timing.phoneme = extractPhonemeFromLyric(note.lyric);
                if (timing.phoneme.empty()) {
                    timing.phoneme = best_oto->alias;
                }
                
                timing.pitch_hz = note.frequency_hz;
                timing.timing_confidence = 1.0 - (best_distance / options_.timing_tolerance_ms);
                
                if (isTimingValid(timing)) {
                    timings.push_back(timing);
                }
            }
        }
        
        return timings;
    }
    
    std::vector<double> MidiPhonemeIntegrator::generatePitchCurve(
        const MidiNote& note, const std::vector<PitchBendEvent>& pitch_bends,
        const std::vector<CCEvent>& cc_events, double sample_rate) {
        
        std::vector<double> pitch_curve;
        
        double duration_seconds = note.duration_ms / 1000.0;
        int num_samples = static_cast<int>(duration_seconds * sample_rate);
        
        if (num_samples <= 0) {
            return pitch_curve;
        }
        
        pitch_curve.reserve(num_samples);
        
        double base_frequency = note.frequency_hz;
        
        for (int i = 0; i < num_samples; ++i) {
            double time_ms = note.start_time_ms + (i / sample_rate) * 1000.0;
            
            // Start with base frequency
            double current_frequency = base_frequency;
            
            // Apply pitch bend
            double pitch_bend = midi_utils::interpolatePitchBend(pitch_bends, time_ms);
            current_frequency *= std::pow(2.0, pitch_bend / 12.0);
            
            // Apply modulation (CC 1)
            double modulation = midi_utils::interpolateCC(cc_events, time_ms, 1) / 127.0;
            double vibrato_depth = modulation * 0.05; // 5% vibrato at max modulation
            double vibrato_rate = 5.0; // 5 Hz vibrato
            double vibrato = std::sin(2.0 * M_PI * vibrato_rate * time_ms / 1000.0) * vibrato_depth;
            current_frequency *= (1.0 + vibrato);
            
            pitch_curve.push_back(current_frequency);
        }
        
        return pitch_curve;
    }
    
    void MidiPhonemeIntegrator::mapMusicalParameters(
        MusicalPhoneme& phoneme, const std::vector<CCEvent>& cc_events, double time_ms) {
        
        // Map CC events to musical parameters
        
        // CC 1: Modulation -> Vibrato depth
        phoneme.vibrato_depth = midi_utils::interpolateCC(cc_events, time_ms, 1) / 127.0;
        
        // CC 7: Volume -> Dynamics
        phoneme.dynamics = midi_utils::interpolateCC(cc_events, time_ms, 7) / 127.0;
        
        // CC 10: Pan -> Brightness (repurpose)
        phoneme.brightness = midi_utils::interpolateCC(cc_events, time_ms, 10) / 127.0;
        
        // CC 74: Filter Cutoff -> Vibrato rate modifier
        double cutoff = midi_utils::interpolateCC(cc_events, time_ms, 74) / 127.0;
        phoneme.vibrato_rate = 3.0 + cutoff * 4.0; // 3-7 Hz range
        
        // Ensure reasonable defaults
        if (phoneme.dynamics == 0.0) phoneme.dynamics = 1.0;
        if (phoneme.brightness == 0.0) phoneme.brightness = 0.5;
        if (phoneme.vibrato_rate == 0.0) phoneme.vibrato_rate = 5.0;
    }
    
    std::vector<MusicalPhoneme> MidiPhonemeIntegrator::alignMidiWithPhonemes(
        const std::vector<MidiNote>& midi_notes,
        const std::vector<PhonemeTimingInfo>& phoneme_timings) {
        
        std::vector<MusicalPhoneme> aligned_phonemes;
        
        for (size_t i = 0; i < std::min(midi_notes.size(), phoneme_timings.size()); ++i) {
            MusicalPhoneme musical_phoneme;
            musical_phoneme.midi_note = midi_notes[i];
            musical_phoneme.timing = phoneme_timings[i];
            
            // Align timing if strict alignment is disabled
            if (!options_.strict_timing_alignment) {
                // Adjust phoneme timing to match MIDI note timing
                double timing_offset = midi_notes[i].start_time_ms - phoneme_timings[i].start_time_ms;
                
                if (std::abs(timing_offset) < options_.timing_tolerance_ms) {
                    musical_phoneme.timing.start_time_ms = midi_notes[i].start_time_ms;
                    musical_phoneme.timing.duration_ms = midi_notes[i].duration_ms;
                }
            }
            
            aligned_phonemes.push_back(musical_phoneme);
        }
        
        return aligned_phonemes;
    }
    
    PhonemeTimingInfo MidiPhonemeIntegrator::calculateSinglePhoneTiming(
        const MidiNote& midi_note, const utau::OtoEntry& oto_entry, const TempoMap& tempo_map) {
        
        PhonemeTimingInfo timing;
        
        // Use MIDI timing as primary reference
        timing.start_time_ms = midi_note.start_time_ms;
        timing.duration_ms = midi_note.duration_ms;
        timing.pitch_hz = midi_note.frequency_hz;
        
        // Extract UTAU-specific timing from oto entry
        timing.preutterance_ms = oto_entry.preutterance;
        timing.overlap_ms = oto_entry.overlap;
        timing.consonant_ms = oto_entry.consonant;
        timing.blank_ms = oto_entry.blank;
        
        // Adjust timing if necessary
        if (timing.preutterance_ms > 0) {
            timing.start_time_ms -= timing.preutterance_ms;
            timing.duration_ms += timing.preutterance_ms;
        }
        
        if (timing.blank_ms > 0) {
            timing.duration_ms += timing.blank_ms;
        }
        
        return timing;
    }
    
    double MidiPhonemeIntegrator::calculateTimingAccuracy(const std::vector<MusicalPhoneme>& phonemes) {
        if (phonemes.empty()) return 0.0;
        
        double total_confidence = 0.0;
        
        for (const auto& phoneme : phonemes) {
            total_confidence += phoneme.timing.timing_confidence;
        }
        
        return total_confidence / phonemes.size();
    }
    
    std::vector<double> MidiPhonemeIntegrator::optimizePhonemeTimings(
        const std::vector<PhonemeTimingInfo>& timings) {
        
        // Simple optimization: smooth timing discontinuities
        std::vector<double> optimized_timings;
        optimized_timings.reserve(timings.size());
        
        for (size_t i = 0; i < timings.size(); ++i) {
            double timing = timings[i].start_time_ms;
            
            // Apply smoothing with neighbors
            if (i > 0 && i < timings.size() - 1) {
                double prev_timing = timings[i - 1].start_time_ms + timings[i - 1].duration_ms;
                double next_timing = timings[i + 1].start_time_ms;
                
                // Weighted average for smoothing
                timing = 0.5 * timing + 0.25 * prev_timing + 0.25 * next_timing;
            }
            
            optimized_timings.push_back(timing);
        }
        
        return optimized_timings;
    }
    
    std::string MidiPhonemeIntegrator::extractPhonemeFromLyric(const std::string& lyric, size_t index) {
        if (lyric.empty()) return "";
        
        // Simple extraction: split by spaces and take the indexed word
        std::istringstream iss(lyric);
        std::string word;
        size_t current_index = 0;
        
        while (iss >> word && current_index <= index) {
            if (current_index == index) {
                return word;
            }
            current_index++;
        }
        
        // If index is out of range, return the last word
        return word;
    }
    
    bool MidiPhonemeIntegrator::isTimingValid(const PhonemeTimingInfo& timing) {
        return timing.duration_ms > 0.0 && 
               timing.start_time_ms >= 0.0 && 
               timing.pitch_hz > 0.0 &&
               timing.timing_confidence > 0.0;
    }
    
    void MidiPhonemeIntegrator::validateIntegrationResult(IntegrationResult& result) {
        // Check for timing overlaps
        for (size_t i = 1; i < result.musical_phonemes.size(); ++i) {
            const auto& prev = result.musical_phonemes[i - 1];
            const auto& curr = result.musical_phonemes[i];
            
            double prev_end = prev.timing.start_time_ms + prev.timing.duration_ms;
            if (prev_end > curr.timing.start_time_ms) {
                addWarning(result, "Timing overlap detected between phonemes " + 
                          std::to_string(i - 1) + " and " + std::to_string(i));
            }
        }
        
        // Check timing accuracy
        if (result.timing_accuracy < 0.5) {
            addWarning(result, "Low timing accuracy: " + std::to_string(result.timing_accuracy));
        }
    }
    
    void MidiPhonemeIntegrator::addError(IntegrationResult& result, const std::string& error) {
        result.errors.push_back(error);
        result.success = false;
    }
    
    void MidiPhonemeIntegrator::addWarning(IntegrationResult& result, const std::string& warning) {
        result.warnings.push_back(warning);
    }

    // =================== RealtimeMidiConverter Implementation ===================
    
    RealtimeMidiConverter::RealtimeMidiConverter() = default;
    
    RealtimeMidiConverter::RealtimeMidiConverter(const RealtimeOptions& options) 
        : options_(options), current_time_(0.0) {
    }
    
    void RealtimeMidiConverter::processMidiEvent(const MidiNote& note) {
        MusicalPhoneme phoneme;
        phoneme.midi_note = note;
        
        // Simple real-time conversion (would need more sophisticated logic)
        phoneme.timing.start_time_ms = note.start_time_ms;
        phoneme.timing.duration_ms = note.duration_ms;
        phoneme.timing.pitch_hz = note.frequency_hz;
        phoneme.timing.is_valid = true;
        phoneme.timing.timing_confidence = 1.0;
        
        schedulePhoneme(phoneme);
    }
    
    void RealtimeMidiConverter::processCCEvent(const CCEvent& /* cc */) {
        // Update current CC state for future phonemes
        // Implementation would maintain CC state
    }
    
    void RealtimeMidiConverter::processPitchBend(const PitchBendEvent& /* bend */) {
        // Update current pitch bend state
        // Implementation would maintain pitch bend state
    }
    
    std::vector<MusicalPhoneme> RealtimeMidiConverter::getReadyPhonemes(double current_time_ms) {
        current_time_ = current_time_ms;
        updateBuffer(current_time_ms);
        
        std::vector<MusicalPhoneme> ready_phonemes;
        
        auto it = phoneme_buffer_.begin();
        while (it != phoneme_buffer_.end()) {
            if (isPhonemeReady(*it, current_time_ms)) {
                ready_phonemes.push_back(*it);
                it = phoneme_buffer_.erase(it);
            } else {
                ++it;
            }
        }
        
        return ready_phonemes;
    }
    
    void RealtimeMidiConverter::clearBuffer() {
        phoneme_buffer_.clear();
    }
    
    size_t RealtimeMidiConverter::getBufferSize() const {
        return phoneme_buffer_.size();
    }
    
    void RealtimeMidiConverter::schedulePhoneme(const MusicalPhoneme& phoneme) {
        phoneme_buffer_.push_back(phoneme);
        
        // Maintain buffer size limit
        if (phoneme_buffer_.size() > static_cast<size_t>(options_.max_polyphony * 2)) {
            // Remove oldest phonemes if buffer is full
            phoneme_buffer_.erase(phoneme_buffer_.begin());
        }
    }
    
    void RealtimeMidiConverter::updateBuffer(double current_time_ms) {
        // Remove expired phonemes
        auto it = std::remove_if(phoneme_buffer_.begin(), phoneme_buffer_.end(),
            [current_time_ms](const MusicalPhoneme& p) {
                double end_time = p.timing.start_time_ms + p.timing.duration_ms;
                return end_time < current_time_ms - 1000.0; // 1 second grace period
            });
        
        phoneme_buffer_.erase(it, phoneme_buffer_.end());
    }
    
    bool RealtimeMidiConverter::isPhonemeReady(const MusicalPhoneme& phoneme, double current_time_ms) {
        double start_time = phoneme.timing.start_time_ms - options_.lookahead_ms;
        return current_time_ms >= start_time;
    }

    // =================== Utility Functions Implementation ===================
    
    namespace midi_utils {
        
        double calculateNoteOnsetAccuracy(const std::vector<MusicalPhoneme>& phonemes) {
            if (phonemes.empty()) return 0.0;
            
            double total_accuracy = 0.0;
            
            for (const auto& phoneme : phonemes) {
                // Compare MIDI timing vs phoneme timing
                double midi_start = phoneme.midi_note.start_time_ms;
                double phoneme_start = phoneme.timing.start_time_ms;
                double timing_error = std::abs(midi_start - phoneme_start);
                
                // Convert to accuracy score (0-1)
                double accuracy = std::exp(-timing_error / 50.0); // 50ms tolerance
                total_accuracy += accuracy;
            }
            
            return total_accuracy / phonemes.size();
        }
        
        double calculatePitchAccuracy(const std::vector<MusicalPhoneme>& phonemes) {
            if (phonemes.empty()) return 0.0;
            
            double total_accuracy = 0.0;
            
            for (const auto& phoneme : phonemes) {
                double midi_freq = phoneme.midi_note.frequency_hz;
                double phoneme_freq = phoneme.timing.pitch_hz;
                
                if (midi_freq > 0.0 && phoneme_freq > 0.0) {
                    double cents_error = 1200.0 * std::log2(phoneme_freq / midi_freq);
                    double accuracy = std::exp(-std::abs(cents_error) / 100.0); // 100 cents tolerance
                    total_accuracy += accuracy;
                } else {
                    total_accuracy += 0.5; // Neutral score for missing data
                }
            }
            
            return total_accuracy / phonemes.size();
        }
        
        double interpolateCC(const std::vector<CCEvent>& events, double time_ms, uint8_t cc_number) {
            if (events.empty()) return 0.0;
            
            // Find CC events for the specified controller
            std::vector<const CCEvent*> relevant_events;
            for (const auto& event : events) {
                if (event.controller == cc_number) {
                    relevant_events.push_back(&event);
                }
            }
            
            if (relevant_events.empty()) return 0.0;
            
            // Find surrounding events
            const CCEvent* before = nullptr;
            const CCEvent* after = nullptr;
            
            for (const auto* event : relevant_events) {
                if (event->time_ms <= time_ms) {
                    if (!before || event->time_ms > before->time_ms) {
                        before = event;
                    }
                }
                if (event->time_ms >= time_ms) {
                    if (!after || event->time_ms < after->time_ms) {
                        after = event;
                    }
                }
            }
            
            if (before && after && before != after) {
                // Linear interpolation
                double t = (time_ms - before->time_ms) / (after->time_ms - before->time_ms);
                return before->value + t * (after->value - before->value);
            } else if (before) {
                return before->value;
            } else if (after) {
                return after->value;
            }
            
            return 0.0;
        }
        
        double interpolatePitchBend(const std::vector<PitchBendEvent>& events, double time_ms) {
            if (events.empty()) return 0.0;
            
            // Find surrounding events
            const PitchBendEvent* before = nullptr;
            const PitchBendEvent* after = nullptr;
            
            for (const auto& event : events) {
                if (event.time_ms <= time_ms) {
                    if (!before || event.time_ms > before->time_ms) {
                        before = &event;
                    }
                }
                if (event.time_ms >= time_ms) {
                    if (!after || event.time_ms < after->time_ms) {
                        after = &event;
                    }
                }
            }
            
            if (before && after && before != after) {
                // Linear interpolation
                double t = (time_ms - before->time_ms) / (after->time_ms - before->time_ms);
                return before->semitones + t * (after->semitones - before->semitones);
            } else if (before) {
                return before->semitones;
            } else if (after) {
                return after->semitones;
            }
            
            return 0.0;
        }
        
        std::vector<utau::OtoEntry> musicalPhonemesToOtoEntries(
            const std::vector<MusicalPhoneme>& phonemes) {
            
            std::vector<utau::OtoEntry> oto_entries;
            
            for (size_t i = 0; i < phonemes.size(); ++i) {
                const auto& phoneme = phonemes[i];
                
                utau::OtoEntry entry;
                entry.filename = "generated_" + std::to_string(i) + ".wav";
                entry.alias = phoneme.timing.phoneme;
                entry.offset = phoneme.timing.start_time_ms;
                entry.consonant = phoneme.timing.consonant_ms;
                entry.blank = phoneme.timing.blank_ms;
                entry.preutterance = phoneme.timing.preutterance_ms;
                entry.overlap = phoneme.timing.overlap_ms;
                
                oto_entries.push_back(entry);
            }
            
            return oto_entries;
        }
        
        std::vector<PhonemeTimingInfo> extractPhonemeTimings(
            const std::vector<MusicalPhoneme>& phonemes) {
            
            std::vector<PhonemeTimingInfo> timings;
            timings.reserve(phonemes.size());
            
            for (const auto& phoneme : phonemes) {
                timings.push_back(phoneme.timing);
            }
            
            return timings;
        }
        
        QualityMetrics assessIntegrationQuality(const std::vector<MusicalPhoneme>& phonemes) {
            QualityMetrics metrics;
            
            metrics.timing_precision = calculateNoteOnsetAccuracy(phonemes);
            metrics.pitch_stability = calculatePitchAccuracy(phonemes);
            
            // Calculate musical coherence (smoothness of transitions)
            double coherence_sum = 0.0;
            for (size_t i = 1; i < phonemes.size(); ++i) {
                const auto& prev = phonemes[i - 1];
                const auto& curr = phonemes[i];
                
                // Check timing continuity
                double gap = curr.timing.start_time_ms - 
                           (prev.timing.start_time_ms + prev.timing.duration_ms);
                double timing_coherence = std::exp(-std::abs(gap) / 100.0); // 100ms tolerance
                
                // Check pitch continuity  
                double pitch_ratio = curr.timing.pitch_hz / prev.timing.pitch_hz;
                double pitch_coherence = std::exp(-std::abs(std::log2(pitch_ratio)) / 2.0); // 2 octave tolerance
                
                coherence_sum += (timing_coherence + pitch_coherence) / 2.0;
            }
            
            metrics.musical_coherence = phonemes.size() > 1 ? 
                coherence_sum / (phonemes.size() - 1) : 1.0;
            
            // Overall score is weighted average
            metrics.overall_score = 0.4 * metrics.timing_precision +
                                  0.3 * metrics.pitch_stability +
                                  0.3 * metrics.musical_coherence;
            
            return metrics;
        }
        
        void exportTimingAnalysis(const std::vector<MusicalPhoneme>& phonemes,
                                const std::string& output_path) {
            
            std::ofstream file(output_path);
            if (!file.is_open()) return;
            
            file << "Index,Phoneme,Start_ms,Duration_ms,MIDI_Start_ms,MIDI_Duration_ms,Timing_Error_ms,Confidence\n";
            
            for (size_t i = 0; i < phonemes.size(); ++i) {
                const auto& p = phonemes[i];
                double timing_error = p.midi_note.start_time_ms - p.timing.start_time_ms;
                
                file << i << ","
                     << p.timing.phoneme << ","
                     << p.timing.start_time_ms << ","
                     << p.timing.duration_ms << ","
                     << p.midi_note.start_time_ms << ","
                     << p.midi_note.duration_ms << ","
                     << timing_error << ","
                     << p.timing.timing_confidence << "\n";
            }
        }
        
        void exportPitchTrajectory(const std::vector<MusicalPhoneme>& phonemes,
                                 const std::string& output_path) {
            
            std::ofstream file(output_path);
            if (!file.is_open()) return;
            
            file << "Time_ms,Frequency_Hz,Phoneme\n";
            
            for (const auto& phoneme : phonemes) {
                if (phoneme.pitch_curve.empty() || phoneme.time_points.empty()) continue;
                
                for (size_t i = 0; i < phoneme.pitch_curve.size(); ++i) {
                    double time_ms = phoneme.timing.start_time_ms + 
                                   (i < phoneme.time_points.size() ? phoneme.time_points[i] : 0.0);
                    
                    file << time_ms << ","
                         << phoneme.pitch_curve[i] << ","
                         << phoneme.timing.phoneme << "\n";
                }
            }
        }
        
    } // namespace midi_utils

} // namespace midi
} // namespace nexussynth